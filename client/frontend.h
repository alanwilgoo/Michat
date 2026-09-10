#ifndef CHAT_UI_H
#define CHAT_UI_H
#include <stdbool.h>
#include "lvgl.h"
#include "app_events.h"
typedef struct {
    int (*send_to)(const char *text, const char *ip, unsigned short port);
    int (*refresh)(void);
    int (*send_file_to)(const char *command, const char *ip, unsigned short port, const char *path, const char *type);
    int (*send_emoji_to)(const char *ip, unsigned short port, const char *path);
    int (*login)(const char *username, const char *password);
    int (*register_user)(const char *username, const char *password);
} frontend_actions_t;
void frontend_set_actions(const frontend_actions_t *actions);
void frontend_init(void);
void frontend_message(const char *who, const char *text, bool mine);
void frontend_status(const char *text, bool connected);
void frontend_online_reset(void);
void frontend_online_item(const char *text);
void frontend_file_received(const char *command, const char *from, const char *name, long size, const char *type);
void frontend_handle_event(const app_event_t *event);
#endif
