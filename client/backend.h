#ifndef CHAT_NET_H
#define CHAT_NET_H
#include <stdbool.h>

typedef struct {
    void (*on_message)(const char *who, const char *text, bool mine);
    void (*on_status)(const char *text, bool connected);
    void (*on_online_reset)(void);
    void (*on_online_item)(const char *text);
    void (*on_file)(const char *command, const char *from, const char *name, long size, const char *type);
    void (*on_auth)(bool is_register, bool success, const char *message);
    void (*on_local_port)(unsigned short port);
} backend_callbacks_t;

void backend_set_callbacks(const backend_callbacks_t *callbacks);
void backend_start(void);
void backend_send(const char *text);
int backend_send_to(const char *text, const char *ip, unsigned short port);
int backend_refresh(void);
int backend_send_file(const char *command, const char *path, const char *type);
int backend_send_file_to(const char *command, const char *ip, unsigned short port,
                          const char *path, const char *type);
int backend_send_emoji_to(const char *ip, unsigned short port, const char *path);
int backend_login(const char *username, const char *password);
int backend_register(const char *username, const char *password);
#endif
