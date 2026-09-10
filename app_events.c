#include "app_events.h"
#include <pthread.h>
#include <string.h>

#define APP_EVENT_QUEUE_CAP 32

static app_event_t events[APP_EVENT_QUEUE_CAP];
static size_t head;
static size_t count;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static app_event_handler_t handler;

static void post(const app_event_t *event)
{
    pthread_mutex_lock(&lock);
    if(count < APP_EVENT_QUEUE_CAP) {
        events[(head + count) % APP_EVENT_QUEUE_CAP] = *event;
        count++;
    }
    pthread_mutex_unlock(&lock);
}

static void copy(char *destination, size_t size, const char *source)
{
    strncpy(destination, source ? source : "", size - 1);
    destination[size - 1] = '\0';
}

void app_events_init(app_event_handler_t value)
{
    handler = value;
}

void app_events_post_message(const char *who, const char *text, bool mine)
{
    app_event_t event = {.type = APP_EVENT_MESSAGE, .mine = mine};
    copy(event.who, sizeof(event.who), who ? who : "Peer");
    copy(event.text, sizeof(event.text), text);
    post(&event);
}

void app_events_post_status(const char *text, bool connected)
{
    app_event_t event = {.type = APP_EVENT_STATUS, .connected = connected};
    copy(event.text, sizeof(event.text), text);
    post(&event);
}

void app_events_post_online_reset(void)
{
    app_event_t event = {.type = APP_EVENT_ONLINE_RESET};
    post(&event);
}

void app_events_post_online_item(const char *text)
{
    app_event_t event = {.type = APP_EVENT_ONLINE_ITEM};
    copy(event.text, sizeof(event.text), text);
    post(&event);
}

void app_events_post_file(const char *command, const char *from, const char *name, long size, const char *type)
{
    app_event_t event = {.type = APP_EVENT_FILE, .size = size};
    copy(event.who, sizeof(event.who), from ? from : "Peer");
    copy(event.text, sizeof(event.text), name);
    copy(event.attachment_command, sizeof(event.attachment_command), command ? command : "sendfile");
    copy(event.attachment_type, sizeof(event.attachment_type), type);
    post(&event);
}

void app_events_post_auth(bool is_register, bool success, const char *message)
{
    app_event_t event = {.type = APP_EVENT_AUTH, .auth_register = is_register, .connected = success};
    copy(event.text, sizeof(event.text), message);
    post(&event);
}

void app_events_post_local_port(unsigned short port)
{
    app_event_t event = {.type = APP_EVENT_LOCAL_PORT, .port = port};
    post(&event);
}

void app_events_poll(void)
{
    app_event_t event;
    for(;;) {
        pthread_mutex_lock(&lock);
        if(count == 0) {
            pthread_mutex_unlock(&lock);
            return;
        }
        event = events[head];
        head = (head + 1) % APP_EVENT_QUEUE_CAP;
        count--;
        pthread_mutex_unlock(&lock);
        if(handler) handler(&event);
    }
}
