#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include "backend.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

static int sock_fd = -1;
static pthread_mutex_t sock_lock = PTHREAD_MUTEX_INITIALIZER;
static backend_callbacks_t callbacks;

void backend_set_callbacks(const backend_callbacks_t *value)
{
    callbacks = value ? *value : (backend_callbacks_t){0};
}

static void emit_status(const char *text, bool connected) { if(callbacks.on_status) callbacks.on_status(text, connected); }
static void emit_auth(bool is_register, bool success, const char *message) { if(callbacks.on_auth) callbacks.on_auth(is_register, success, message); }
static void emit_local_port(unsigned short port) { if(callbacks.on_local_port) callbacks.on_local_port(port); }

#ifndef MICHAT_DEFAULT_SERVER_IP
#define MICHAT_DEFAULT_SERVER_IP "172.18.178.250"
#endif
#define DEFAULT_SERVER_IP MICHAT_DEFAULT_SERVER_IP
#define DEFAULT_SERVER_PORT 10000
#define DEFAULT_PEER_IP "127.0.0.1"
#define DEFAULT_PEER_PORT 10086
#define RECEIVE_BUFFER_SIZE 2048
#define RECEIVE_PATH_SIZE 1024

#ifndef MICHAT_RECEIVE_DIR_DEFAULT
#define MICHAT_RECEIVE_DIR_DEFAULT "build/runtime/received"
#endif

#ifndef MICHAT_PROJECT_DIR
#define MICHAT_PROJECT_DIR "."
#endif

static int send_all(int fd, const char *data, size_t len)
{
    while(len) {
        ssize_t n = send(fd, data, len, MSG_NOSIGNAL);
        if(n <= 0) return -1;
        data += n; len -= (size_t)n;
    }
    return 0;
}

static unsigned short port_from_env(const char *name, unsigned short fallback)
{
    const char *text = getenv(name);
    char *end;
    unsigned long value;
    if(text == NULL || text[0] == '\0') return fallback;
    value = strtoul(text, &end, 10);
    if(*end != '\0' || value == 0 || value > 65535) return fallback;
    return (unsigned short)value;
}

/* An omitted CHAT_LOCAL_PORT means "let the operating system choose". Keeping
 * an explicit value available is useful for legacy/manual peer-to-peer tests. */
static unsigned short local_port_from_env(void)
{
    const char *text = getenv("CHAT_LOCAL_PORT");
    char *end;
    unsigned long value;

    if(text == NULL || text[0] == '\0') return 0;
    value = strtoul(text, &end, 10);
    if(*end != '\0' || value > 65535) return 0;
    return (unsigned short)value;
}

static int recv_line(int fd, char *out, size_t cap)
{
    size_t used = 0;
    while(used + 1 < cap) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if(n <= 0) return -1;
        if(c == '\n') { out[used] = 0; return 0; }
        out[used++] = c;
    }
    out[used] = 0;
    return -1;
}

static int recv_exact(int fd, FILE *file, long size)
{
    char buffer[RECEIVE_BUFFER_SIZE];
    long received = 0;

    while(received < size) {
        size_t wanted = (size - received < (long)sizeof(buffer))
                        ? (size_t)(size - received) : sizeof(buffer);
        ssize_t bytes = recv(fd, buffer, wanted, 0);
        if(bytes <= 0) return -1;
        if(fwrite(buffer, 1, (size_t)bytes, file) != (size_t)bytes) return -1;
        received += bytes;
    }
    return 0;
}

static int ensure_directory(const char *directory)
{
    char path[RECEIVE_PATH_SIZE];
    size_t length;

    if(directory == NULL || directory[0] == '\0') return -1;
    length = strlen(directory);
    if(length >= sizeof(path)) return -1;
    memcpy(path, directory, length + 1);

    for(size_t i = 1; i < length; i++) {
        if(path[i] != '/') continue;
        path[i] = '\0';
        if(mkdir(path, 0755) < 0 && errno != EEXIST) return -1;
        path[i] = '/';
    }
    return mkdir(path, 0755) == 0 || errno == EEXIST ? 0 : -1;
}

static const char *attachment_filename(const char *name)
{
    const char *forward;
    const char *backward;

    if(name == NULL) return NULL;
    forward = strrchr(name, '/');
    backward = strrchr(name, '\\');
    if(forward == NULL) forward = backward;
    else if(backward != NULL && backward > forward) forward = backward;
    return forward ? forward + 1 : name;
}

static int resolve_send_path(const char *path, char *resolved, size_t resolved_size)
{
    int written;

    if(path == NULL || path[0] == '\0' || resolved == NULL || resolved_size == 0) return -1;
    if(path[0] == '/') written = snprintf(resolved, resolved_size, "%s", path);
    else written = snprintf(resolved, resolved_size, "%s/%s", MICHAT_PROJECT_DIR, path);
    return written >= 0 && (size_t)written < resolved_size ? 0 : -1;
}

static void receive_attachment(int fd, const char *command)
{
    char *first = strtok(NULL, "#");
    char *second = strtok(NULL, "#");
    char *third = strtok(NULL, "#");
    char *fourth = strtok(NULL, "#");
    const char *from = "Peer";
    char *name;
    char *size_text;
    char *type;
    const char *filename;
    char save_path[RECEIVE_PATH_SIZE];
    long size;
    FILE *file;

    /* New server headers include the sender identity. Keep old four-field headers readable. */
    if(fourth != NULL) {
        from = first ? first : "Peer";
        name = second;
        size_text = third;
        type = fourth;
    } else {
        name = first;
        size_text = second;
        type = third;
    }
    size = size_text ? strtol(size_text, NULL, 10) : -1;
    filename = attachment_filename(name);
    if(filename == NULL || filename[0] == '\0' || strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0 ||
       size < 0 || (strcmp(command, "sendfile") == 0 && type == NULL)) {
        emit_status("Invalid attachment received", false);
        return;
    }
    if(ensure_directory(MICHAT_RECEIVE_DIR_DEFAULT) < 0 ||
       snprintf(save_path, sizeof(save_path), "%s/%s", MICHAT_RECEIVE_DIR_DEFAULT, filename) >= (int)sizeof(save_path)) {
        emit_status("Cannot prepare received-files folder", false);
        return;
    }
    file = fopen(save_path, "wb");
    if(file == NULL) {
        emit_status("Cannot save received attachment", false);
        return;
    }
    if(recv_exact(fd, file, size) == 0) {
        if(callbacks.on_file) callbacks.on_file(command, from, save_path, size, type ? type : "");
    } else {
        emit_status("Attachment receive interrupted", false);
    }
    fclose(file);
}

static void *receiver(void *unused)
{
    char line[2048];
    (void)unused;
    while(sock_fd >= 0 && recv_line(sock_fd, line, sizeof(line)) == 0) {
        if(strcmp(line, "LOGIN_OK") == 0 || strcmp(line, "LOGIN_FAIL") == 0 ||
           strcmp(line, "REGISTER_OK") == 0 || strcmp(line, "REGISTER_FAIL") == 0) {
            bool is_register = strncmp(line, "REGISTER", 8) == 0;
            bool success = strstr(line, "_OK") != NULL;
            emit_auth(is_register, success, success ?
                      (is_register ? "Registration successful" : "Login successful") :
                      (is_register ? "Registration failed: username may already exist" : "Login failed: invalid username or password"));
            if(success && !is_register) emit_status("Authenticated", true);
            continue;
        }
        char *cmd = strtok(line, "#");
        if(!cmd) continue;
        if(strcmp(cmd, "AUTH_REQUIRED") == 0) {
            emit_status("Please log in before chatting", false);
        } else if(strcmp(cmd, "getlist") == 0) {
            char *item;
            if(callbacks.on_online_reset) callbacks.on_online_reset();
            while((item = strtok(NULL, "#")) != NULL) if(callbacks.on_online_item) callbacks.on_online_item(item);
            emit_status("Online list refreshed", true);
        } else if(strcmp(cmd, "chat") == 0) {
            char *from = strtok(NULL, "#"); char *msg = strtok(NULL, "#"); if(msg == NULL) { msg = from; from = NULL; }
            if(msg && callbacks.on_message) callbacks.on_message(from ? from : "Peer", msg, false);
        } else if(strcmp(cmd, "file") == 0 || strcmp(cmd, "sendfile") == 0 || strcmp(cmd, "emoji") == 0) {
            receive_attachment(sock_fd, cmd);
        }
    }
    emit_status("Offline preview mode", false);
    return NULL;
}

static void *connector(void *unused)
{
    struct sockaddr_in local = {0};
    struct sockaddr_in server = {0};
    pthread_t rx;
    const char *server_ip = getenv("CHAT_SERVER_IP");
    unsigned short server_port = port_from_env("CHAT_SERVER_PORT", DEFAULT_SERVER_PORT);
    unsigned short local_port = local_port_from_env();
    if(server_ip == NULL || server_ip[0] == '\0') server_ip = DEFAULT_SERVER_IP;
    (void)unused;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) { emit_status("Offline preview mode", false); return NULL; }
    struct timeval timeout = {1, 0};
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    local.sin_family = AF_INET;
    local.sin_port = htons(local_port);
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(fd, (struct sockaddr *)&local, sizeof(local)) < 0) {
        char error[96];
        snprintf(error, sizeof(error), "Cannot bind local port %hu", local_port);
        close(fd); emit_status(error, false); return NULL;
    }
    server.sin_family = AF_INET;
    server.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server.sin_addr);
    if(connect(fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        close(fd); emit_status("Offline preview mode", false); return NULL;
    }
    {
        socklen_t local_size = sizeof(local);
        if(getsockname(fd, (struct sockaddr *)&local, &local_size) < 0) {
            close(fd); emit_status("Cannot determine local client port", false); return NULL;
        }
        local_port = ntohs(local.sin_port);
    }
    pthread_mutex_lock(&sock_lock);
    sock_fd = fd;
    {
        char reg[128];
        snprintf(reg, sizeof(reg), "register@127.0.0.1@%hu@register\n", local_port);
        if(send_all(fd, reg, strlen(reg)) < 0) {
            sock_fd = -1;
            pthread_mutex_unlock(&sock_lock);
            close(fd); emit_status("Unable to initialize chat session", false); return NULL;
        }
    }
    pthread_mutex_unlock(&sock_lock);
    emit_local_port(local_port);
    char status[128];
    snprintf(status, sizeof(status), "Connected to %s:%hu (local port %hu)", server_ip, server_port, local_port);
    emit_status(status, true);
    pthread_create(&rx, NULL, receiver, NULL);
    pthread_detach(rx);
    return NULL;
}

void backend_start(void)
{
    pthread_t tid;
    pthread_create(&tid, NULL, connector, NULL);
    pthread_detach(tid);
}

void backend_send(const char *text)
{
    const char *peer_ip = getenv("CHAT_PEER_IP");
    const char *peer_port_text = getenv("CHAT_PEER_PORT");
    unsigned long peer_port = peer_port_text ? strtoul(peer_port_text, NULL, 10) : DEFAULT_PEER_PORT;
    if(peer_ip == NULL || peer_ip[0] == '\0') peer_ip = DEFAULT_PEER_IP;
    if(peer_port == 0 || peer_port > 65535) peer_port = DEFAULT_PEER_PORT;
    backend_send_to(text, peer_ip, (unsigned short)peer_port);
}

int backend_send_to(const char *text, const char *ip, unsigned short port)
{
    char packet[2048];
    int result = -1;
    if(text == NULL || ip == NULL || ip[0] == '\0' || port == 0) return -1;
    snprintf(packet, sizeof(packet), "chat@%s@%hu@%s\n", ip, port, text);
    pthread_mutex_lock(&sock_lock);
    if(sock_fd >= 0) result = send_all(sock_fd, packet, strlen(packet));
    pthread_mutex_unlock(&sock_lock);
    return result;
}

int backend_refresh(void)
{
    int result = -1;
    pthread_mutex_lock(&sock_lock);
    if(sock_fd >= 0) result = send_all(sock_fd, "getlist\n", 8);
    pthread_mutex_unlock(&sock_lock);
    return result;
}

static int backend_send_auth(const char *command, const char *username, const char *password)
{
    char packet[160];
    int result = -1;
    if(command == NULL || username == NULL || password == NULL || username[0] == '\0' || password[0] == '\0' ||
       strchr(username, '@') || strchr(password, '@') || strchr(username, '\n') || strchr(password, '\n')) return -1;
    if(strlen(username) + strlen(password) + 16 >= sizeof(packet)) return -1;
    if(snprintf(packet, sizeof(packet), "%.*s@%.*s@%.*s\n", 8, command, 63, username, 63, password) >= (int)sizeof(packet)) return -1;
    pthread_mutex_lock(&sock_lock);
    if(sock_fd >= 0) result = send_all(sock_fd, packet, strlen(packet));
    pthread_mutex_unlock(&sock_lock);
    return result;
}

int backend_login(const char *username, const char *password)
{
    return backend_send_auth("LOGIN", username, password);
}

int backend_register(const char *username, const char *password)
{
    return backend_send_auth("REGISTER", username, password);
}

int backend_send_file(const char *command, const char *path, const char *type)
{
    const char *peer_ip = getenv("CHAT_PEER_IP");
    const char *peer_port_text = getenv("CHAT_PEER_PORT");
    unsigned long peer_port = peer_port_text ? strtoul(peer_port_text, NULL, 10) : DEFAULT_PEER_PORT;
    if(peer_ip == NULL || peer_ip[0] == '\0') peer_ip = DEFAULT_PEER_IP;
    if(peer_port == 0 || peer_port > 65535) peer_port = DEFAULT_PEER_PORT;
    return backend_send_file_to(command, peer_ip, (unsigned short)peer_port, path, type);
}

int backend_send_file_to(const char *command, const char *ip, unsigned short port,
                          const char *path, const char *type)
{
    FILE *file;
    char source_path[RECEIVE_PATH_SIZE];
    char header[2048];
    char buffer[2048];
    long size;
    int result = -1;

    if(command == NULL || ip == NULL || ip[0] == '\0' || port == 0 || type == NULL ||
       resolve_send_path(path, source_path, sizeof(source_path)) < 0) return -1;
    file = fopen(source_path, "rb");
    if(file == NULL) return -1;
    if(fseek(file, 0, SEEK_END) != 0) goto done;
    size = ftell(file);
    if(size < 0 || fseek(file, 0, SEEK_SET) != 0) goto done;
    snprintf(header, sizeof(header), "%s@%s@%hu@%s@%s@%ld\n",
             command, ip, port, source_path, type, size);

    pthread_mutex_lock(&sock_lock);
    if(sock_fd >= 0 && send_all(sock_fd, header, strlen(header)) == 0) {
        result = 0;
        while(!feof(file)) {
            size_t bytes = fread(buffer, 1, sizeof(buffer), file);
            if(bytes == 0) break;
            if(send_all(sock_fd, buffer, bytes) < 0) {
                result = -1;
                break;
            }
        }
        if(ferror(file)) result = -1;
    }
    pthread_mutex_unlock(&sock_lock);

done:
    fclose(file);
    return result;
}

int backend_send_emoji_to(const char *ip, unsigned short port, const char *path)
{
    const char *extension;

    if(path == NULL) return -1;
    extension = strrchr(path, '.');
    if(extension == NULL || extension[1] == '\0') return -1;
    extension++;
    if(strcasecmp(extension, "png") != 0 && strcasecmp(extension, "jpg") != 0 &&
       strcasecmp(extension, "jpeg") != 0 && strcasecmp(extension, "bmp") != 0) return -1;
    return backend_send_file_to("emoji", ip, port, path, extension);
}
