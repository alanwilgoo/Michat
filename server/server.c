#include "list.h"
#include <signal.h>

static list *head;                  /* 链表头指针 */
static pthread_mutex_t users_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile sig_atomic_t stop_requested;
static int listen_sock = -1;
/* Keep runtime credentials out of source control. Deployments can provide an
 * absolute path; local development uses the ignored project data directory. */
static const char *users_file_path(void)
{
    const char *path = getenv("CHAT_USERS_FILE");
    return path != NULL && path[0] != '\0' ? path : "data/users.txt";
}

/* SIGINT (Ctrl+C) and SIGTERM both stop the blocking accept loop.  close()
 * is async-signal-safe on POSIX systems, so it wakes accept promptly. */
static void request_stop(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
    if(listen_sock >= 0) {
        close(listen_sock);
        listen_sock = -1;
    }
}

static unsigned short server_port_from_env(void)
{
    const char *text = getenv("CHAT_SERVER_PORT");
    char *end;
    unsigned long value;

    if(text == NULL || text[0] == '\0') return 10000;
    value = strtoul(text, &end, 10);
    return *end == '\0' && value > 0 && value <= 65535 ? (unsigned short)value : 10000;
}

static int valid_credential(const char *value)
{
    size_t n;
    if(value == NULL || (n = strlen(value)) == 0 || n > 63) return 0;
    for(size_t i = 0; i < n; i++)
        if(value[i] == '@' || value[i] == '\n' || value[i] == '\r' || value[i] == ' ' || value[i] == '\t') return 0;
    return 1;
}

static int user_exists_locked(const char *username, const char *password, int check_password)
{
    FILE *file = fopen(users_file_path(), "r");
    char saved_user[64], saved_password[64];
    if(file == NULL) return 0;
    while(fscanf(file, "%63s %63s", saved_user, saved_password) == 2)
        if(strcmp(saved_user, username) == 0 && (!check_password || strcmp(saved_password, password) == 0)) {
            fclose(file); return 1;
        }
    fclose(file); return 0;
}

static int register_user(const char *username, const char *password)
{
    int result = -1;
    pthread_mutex_lock(&users_lock);
    if(valid_credential(username) && valid_credential(password) && !user_exists_locked(username, password, 0)) {
        FILE *file = fopen(users_file_path(), "a");
        if(file != NULL) { result = fprintf(file, "%s %s\n", username, password) > 0 ? 0 : -1; fclose(file); }
    }
    pthread_mutex_unlock(&users_lock);
    return result;
}

static int login_user(const char *username, const char *password)
{
    int result;
    pthread_mutex_lock(&users_lock);
    result = valid_credential(username) && valid_credential(password) && user_exists_locked(username, password, 1) ? 0 : -1;
    pthread_mutex_unlock(&users_lock);
    return result;
}

/* 发送所有数据（确保完整发送） */
static int send_all(int sock, const void *buf, size_t len)
{
    const char *p = buf;
    while (len > 0) {
        ssize_t n = send(sock, p, len, 0);
        if (n <= 0) return -1;
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

/* 逐行接收（以\n为结束符） */
static int recv_line(int sock, char *buf, size_t size)
{
    size_t used = 0;
    while (used + 1 < size) {
        char ch;
        ssize_t n = recv(sock, &ch, 1, 0);
        if (n <= 0) return -1;
        if (ch == '\n') {
            buf[used] = '\0';
            return 0;
        }
        buf[used++] = ch;
    }
    buf[used] = '\0';
    return -2;  /* 缓冲区不足 */
}

/* 查找在线客户端（根据IP和端口） */
static list *find_client(const char *ip, unsigned short port)
{
    list *p = head->next;
    while (p != NULL) {
        if (strcmp(p->ip, ip) == 0 && p->port == port)
            return p;
        p = p->next;
    }
    return NULL;
}

/* 发送在线列表给客户端 */
static void send_online_list(list *client)
{
    char message[2048] = "getlist#";    /* 消息头标识 */
    list *p = head->next;
    
    /* 遍历链表拼接所有在线客户端信息 */
    while (p != NULL) {
        if(!p->authenticated) { p = p->next; continue; }
        char item[160];
        /* Username is supplied by the server after successful login.  The
         * endpoint remains in this response because it is still the routing
         * key used by the existing chat/file/emoji commands. */
        snprintf(item, sizeof(item), "%s@%s:%hu#", p->username, p->ip, p->port);
        if (strlen(message) + strlen(item) + 2 >= sizeof(message))
            break;
        strcat(message, item);
        p = p->next;
    }
    strcat(message, "\n");
    send_all(client->sock, message, strlen(message));
}

/* 中转数据（从from_sock读取并发送到to_sock） */
static int relay_data(int from_sock, int to_sock, long size)
{
    char buf[2048];
    long total = 0;
    
    while (total < size) {
        size_t want = (size - total < (long)sizeof(buf)) ? 
                      (size_t)(size - total) : sizeof(buf);
        ssize_t n = recv(from_sock, buf, want, 0);
        if (n <= 0 || send_all(to_sock, buf, (size_t)n) < 0)
            return -1;
        total += n;
    }
    return 0;
}

/* 客户端接收线程函数 */
static void *recv_fun(void *arg)
{
    list *sender = (list *)arg;          /* 发送者节点 */
    char line[2048];
    
    /* 逐行接收客户端数据 */
    while (recv_line(sender->sock, line, sizeof(line)) == 0) {
        if (strncmp(line, "REGISTER@", 9) == 0 || strncmp(line, "LOGIN@", 6) == 0) {
            int is_register = strncmp(line, "REGISTER@", 9) == 0;
            char *user = line + (is_register ? 9 : 6);
            char *password = strchr(user, '@');
            int result;
            if(password == NULL) result = -1;
            else { *password++ = '\0'; result = is_register ? register_user(user, password) : login_user(user, password); }
            const char *response = is_register ? (result == 0 ? "REGISTER_OK\n" : "REGISTER_FAIL\n") : (result == 0 ? "LOGIN_OK\n" : "LOGIN_FAIL\n");
            if(send_all(sender->sock, response, strlen(response)) < 0) break;
            fprintf(stderr, "auth %s user=%s result=%s\n", is_register ? "register" : "login",
                    valid_credential(user) ? user : "<invalid>", result == 0 ? "ok" : "fail");
            if(!is_register && result == 0) {
                sender->authenticated = true;
                snprintf(sender->username, sizeof(sender->username), "%.*s",
                         (int)sizeof(sender->username) - 1, user);
            }
            continue;
        }
        if (strncmp(line, "register@", 9) == 0) { char *p = line + 9; char *a = strchr(p, "@"[0]); if (a) { char *b = strchr(a + 1, "@"[0]); if (b) { unsigned long v = strtoul(a + 1, NULL, 10); if (v > 0 && v <= 65535) sender->port = (unsigned short)v; } } continue; }
        /* 处理获取在线列表请求 */
        if (strcmp(line, "getlist") == 0) {
            if(!sender->authenticated) { send_all(sender->sock, "AUTH_REQUIRED\n", 14); continue; }
            send_online_list(sender);
            continue;
        }

        if(!sender->authenticated) {
            send_all(sender->sock, "AUTH_REQUIRED\n", 14);
            continue;
        }
        
        /* 解析协议：cmd@ip@port@field4@... */
        char *cmd = strtok(line, "@");
        char *ip = strtok(NULL, "@");
        char *port_text = strtok(NULL, "@");
        char *field4 = strtok(NULL, "@");
        
        if (cmd == NULL || ip == NULL || port_text == NULL || field4 == NULL)
            continue;
        
        /* 查找目标客户端 */
        list *target = find_client(ip, (unsigned short)atoi(port_text));
        if (target == NULL) {
            printf("没有找到目标客户端 %s:%s\n", ip, port_text);
            continue;
        }
        
        /* 处理聊天消息 */
        if (strcmp(cmd, "chat") == 0) {
            char message[2048];
            snprintf(message, sizeof(message), "chat#%s:%hu#%s\n", sender->ip, sender->port, field4);
            send_all(target->sock, message, strlen(message));
        }
        /* 处理文件或表情包转发 */
       else if(strcmp(cmd, "file") == 0 || strcmp(cmd, "emoji") == 0)
        {
            char *type = strtok(NULL, "@");
            char *size_text = strtok(NULL, "@");
            long size;
            char message[2048];
            if(type == NULL || size_text == NULL || (size = atol(size_text)) < 0)
            {
                printf("%s消息格式错误\n", cmd);
                continue;
            }
            /* 从文件路径中提取原始文件名 */
            char *filename = strrchr(field4, '/');
            if(filename == NULL) {
                filename = strrchr(field4, '\\');  /* Windows路径 */
            }
            if(filename != NULL) {
                filename++;  /* 跳过分隔符 */
            } else {
                filename = field4;  /* 没有路径，直接使用 */
            }
            snprintf(message, sizeof(message), "%s#%s:%hu#%s#%ld#%s\n",
                    strcmp(cmd, "file") == 0 ? "sendfile" : "emoji",
                    sender->ip, sender->port, filename, size, type);
            if(send_all(target->sock, message, strlen(message)) < 0 ||
            relay_data(sender->sock, target->sock, size) < 0)
            printf("转发%s失败\n", strcmp(cmd, "file") == 0 ? "文件" : "表情包");
        }
    }
    
    /* 客户端断开连接处理 */
    printf("客户端%s:%hu已断开连接\n", sender->ip, sender->port);
    close(sender->sock);
    list_delete_node(sender, head);  /* Remove this exact disconnected session. */
    return NULL;
}

int main(void)
{
    unsigned short listen_port = server_port_from_env();
    struct sockaddr_in bindaddr = {0}, clientaddr = {0};
    pthread_attr_t attr;
    struct sigaction stop_action = {0};

    stop_action.sa_handler = request_stop;
    sigemptyset(&stop_action.sa_mask);
    sigaction(SIGINT, &stop_action, NULL);
    sigaction(SIGTERM, &stop_action, NULL);
    signal(SIGPIPE, SIG_IGN);
    
    /* 初始化链表 */
    head = list_init();
    
    /* 绑定服务器地址 */
    bindaddr.sin_family = AF_INET;
    bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindaddr.sin_port = htons(listen_port);
    
    /* 创建TCP套接字 */
    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        perror("socket");
        return 1;
    }
    
    /* 设置端口复用 */
    int on = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    
    /* 绑定地址 */
    if (bind(listen_sock, (struct sockaddr *)&bindaddr, sizeof(bindaddr)) < 0) {
        perror("bind");
        return 1;
    }
    
    /* 监听 */
    if (listen(listen_sock, 10) < 0) {
        perror("listen");
        return 1;
    }
    
    /* 设置线程分离属性 */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    
    printf("服务器已启动，监听端口 %hu\n", listen_port);
    
    /* 主循环：接受客户端连接 */
    while (!stop_requested) {
        socklen_t size = sizeof(clientaddr);
        int sock = accept(listen_sock, (struct sockaddr *)&clientaddr, &size);
        if (sock < 0) {
            if(stop_requested) break;
            if(errno == EINTR) continue;
            perror("accept");
            continue;
        }
        
        /* 创建新节点 */
        list *node = calloc(1, sizeof(*node));
        if (node == NULL) {
            close(sock);
            continue;
        }
        
        /* 填充节点信息 */
        node->sock = sock;
        snprintf(node->ip, sizeof(node->ip), "%s", inet_ntoa(clientaddr.sin_addr));
        node->port = ntohs(clientaddr.sin_port);
        
        /* 插入链表 */
        list_insert_tail(node, head);
        printf("客户端%s:%hu已连接\n", node->ip, node->port);
        
        /* 创建接收线程 */
        pthread_t tid;
        pthread_create(&tid, &attr, recv_fun, node);
    }
    
    pthread_attr_destroy(&attr);
    if(listen_sock >= 0) close(listen_sock);
    listen_sock = -1;
    printf("服务器已正常退出\n");
    return 0;
}
