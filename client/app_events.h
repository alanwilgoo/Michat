#ifndef APP_EVENTS_H
#define APP_EVENTS_H

#include <stdbool.h>

typedef enum {
    APP_EVENT_MESSAGE,
    APP_EVENT_STATUS,
    APP_EVENT_ONLINE_RESET,
    APP_EVENT_ONLINE_ITEM,
    APP_EVENT_FILE,
    APP_EVENT_AUTH,
    APP_EVENT_LOCAL_PORT
} app_event_type_t;

typedef struct {
    app_event_type_t type;
    bool mine;
    bool connected;
    char who[64];
    char text[2048];
    long size;
    char attachment_command[16];
    char attachment_type[64];
    bool auth_register;
    unsigned short port;
} app_event_t;

typedef void (*app_event_handler_t)(const app_event_t *event);

void app_events_init(app_event_handler_t handler);
void app_events_post_message(const char *who, const char *text, bool mine);
void app_events_post_status(const char *text, bool connected);
void app_events_post_online_reset(void);
void app_events_post_online_item(const char *text);
void app_events_post_file(const char *command, const char *from, const char *name, long size, const char *type);
void app_events_post_auth(bool is_register, bool success, const char *message);
void app_events_post_local_port(unsigned short port);
void app_events_poll(void);

#endif
