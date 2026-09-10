#include "myhead.h"

static int tcpsock;  /* TCP套接字 */

/* 接收线程函数：接收服务器转发的消息 */
static void *recv_fun(void *arg)
{
    char line[2048];
    (void)arg;  /* 消除未使用参数警告 */
    
    /* 循环接收服务器消息（以\n为结束符） */
    while (recv_line(tcpsock, line, sizeof(line)) == 0) {
        char *cmd = strtok(line, "#");  /* 解析命令类型 */
        if (cmd == NULL) continue;
        
        /* 处理获取在线列表响应 */
        if (strcmp(cmd, "getlist") == 0) {
            printf("目前在线的客户端信息如下:\n");
            char *item;
            while ((item = strtok(NULL, "#")) != NULL)
                printf("%s\n", item);
        }
        /* 处理聊天消息 */
        else if (strcmp(cmd, "chat") == 0) {
            char *msg = strtok(NULL, "#");
            if (msg == NULL)
                printf("聊天消息格式错误\n");
            else
                printf("\n收到聊天消息:%s\n", msg);
        }
        /* 处理文件或表情包接收 */
        else if (strcmp(cmd, "sendfile") == 0 || strcmp(cmd, "emoji") == 0) {
            char *name = strtok(NULL, "#");      /* 文件名 */
            char *size_text = strtok(NULL, "#"); /* 文件大小 */
            char *type = strtok(NULL, "#");      /* 文件类型 */
            long size = size_text == NULL ? -1 : atol(size_text);
            
            if (name == NULL || size < 0 || (strcmp(cmd, "sendfile") == 0 && type == NULL)) {
                printf("文件消息格式错误\n");
                continue;
            }
            
            /* 创建文件保存接收内容 */
            FILE *fp = fopen(name, "wb");
            if (fp == NULL) {
                perror("接收文件打开失败");
                continue;
            }
            
            /* 循环接收文件数据 */
            char buf[2048];
            long total = 0;
            while (total < size) {
                size_t want = (size - total < (long)sizeof(buf)) ? 
                              (size_t)(size - total) : sizeof(buf);
                ssize_t n = recv(tcpsock, buf, want, 0);
                if (n <= 0) {
                    fclose(fp);
                    printf("服务器断开连接\n");
                    return NULL;
                }
                fwrite(buf, 1, (size_t)n, fp);
                total += n;
            }
            fclose(fp);
            
            printf("%s接收完毕：%s (%ld字节)\n", 
                   strcmp(cmd, "emoji") == 0 ? "表情包" : "文件", name, size);
            if (type != NULL)
                printf("文件类型：%s\n", type);
        }
    }
    return NULL;
}

/* 转换Windows路径为Linux路径（用于WSL环境） */
static void normalize_path(char *path)
{
    /* 处理Windows盘符路径，如 C:\path -> /mnt/c/path */
    if (path[1] == ':' && (path[2] == '\\' || path[2] == '/')) {
        char drive = path[0];
        char converted[1024];
        snprintf(converted, sizeof(converted), "/mnt/%c/%s",
                 drive >= 'A' && drive <= 'Z' ? drive - 'A' + 'a' : drive,
                 path + 3);
        strcpy(path, converted);
    }
    /* 将反斜杠替换为正斜杠 */
    for (char *p = path; *p != '\0'; ++p) {
        if (*p == '\\') *p = '/';
    }
}

/* 发送文件到服务器（转发给目标客户端） */
static int send_file(const char *cmd, const char *ip, unsigned short port,
                     const char *path, const char *type)
{
    FILE *fp = fopen(path, "rb");
    char header[2048], buf[2048];
    
    if (fp == NULL) {
        perror("文件打开失败");
        return -1;
    }
    
    /* 获取文件大小 */
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    /* 构造协议头：cmd@ip@port@path@type@size\n */
    snprintf(header, sizeof(header), "%s@%s@%hu@%s@%s@%ld\n", 
             cmd, ip, port, path, type, size);
    
    if (send_all(tcpsock, header, strlen(header)) < 0) {
        fclose(fp);
        return -1;
    }
    
    /* 发送文件内容 */
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        if (send_all(tcpsock, buf, n) < 0) {
            fclose(fp);
            return -1;
        }
    }
    
    fclose(fp);
    printf("%s发送完毕！\n", strcmp(cmd, "emoji") == 0 ? "表情包" : "文件");
    return 0;
}

int main(void)
{
    pthread_t id;
    int ret;
    int n;
    char ip[64], msg[512], path[1024], type[64];
    unsigned short port;
    
    /* 绑定本地地址 */
    struct sockaddr_in bindaddr;
    bindaddr.sin_family = AF_INET;
    bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindaddr.sin_port = htons(10087);
    
    /* 服务器地址 */
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr("172.18.178.250");
    serveraddr.sin_port = htons(10000);
    
    /* 创建TCP套接字 */
    tcpsock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpsock < 0) {
        perror("创建失败");
        return 1;
    }
    
    /* 设置端口复用 */
    int on = 1;
    setsockopt(tcpsock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    
    /* 绑定本地端口 */
    ret = bind(tcpsock, (struct sockaddr *)&bindaddr, sizeof(struct sockaddr_in));
    if (ret == -1) {
        perror("绑定ip和端口号失败了!\n");
        return -1;
    }
    
    /* 连接服务器 */
    ret = connect(tcpsock, (struct sockaddr *)&serveraddr, sizeof(struct sockaddr_in));
    if (ret == -1) {
        perror("连接服务器失败!\n");
        return -1;
    }
    
    /* 创建接收线程 */
    pthread_create(&id, NULL, recv_fun, NULL);
    
    /* 主循环：用户交互菜单 */
    while (1) {
        printf("请选择你想要的功能!\n"
               "1.跟其他客户端聊天!\n"
               "2.发送文件给其他客户端!\n"
               "3.发送表情包给其他客户端!\n"
               "4.获取服务器上存放的在线客户端信息!\n");
        
        if (scanf("%d", &n) != 1)
            break;
        
        /* 聊天功能 */
        if (n == 1) {
            printf("请输入你想要聊天的客户端ip地址!\n");
            if (scanf("%63s", ip) != 1) {
                continue;
            }
            printf("请输入你想要聊天的客户端端口!\n");
            if (scanf("%hu", &port) != 1) {
                continue;
            }
            printf("请输入你要发送的消息!\n");
            if (scanf("%511s", msg) != 1) {
                continue;
            }
            
            /* 构造并发送聊天消息：chat@ip@port@msg\n */
            snprintf(path, sizeof(path), "chat@%s@%hu@%s\n", ip, port, msg);
            send_all(tcpsock, path, strlen(path));
        }
        /* 发送文件或表情包 */
        else if (n == 2 || n == 3) {
            printf("请输入对方的ip：\n");
            if (scanf("%63s", ip) != 1) {
                continue;
            }
            printf("请输入对方的端口号：\n");
            if (scanf("%hu", &port) != 1) {
                continue;
            }
            printf("请输入要发送的文件路径：\n");
            if (scanf("%511s", path) != 1) {
                continue;
            }
            printf("请输入文件类型(例如jpg/png/txt):\n");
            if (scanf("%63s", type) != 1) {
                continue;
            }
            
            /* 转换路径（适配WSL环境） */
            normalize_path(path);
            
            /* 发送文件 */
            send_file(n == 2 ? "file" : "emoji", ip, port, path, type);
        }
        /* 获取在线列表 */
        else if (n == 4) {
            send_all(tcpsock, "getlist\n", 8);
        } else {
            printf("输入错误!\n");
        }
    }
    
    close(tcpsock);
    return 0;
}