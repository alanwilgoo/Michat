#include "lvgl.h"
#include "frontend.h"
#include "backend.h"
#include "app_events.h"

static void net_message(const char *who, const char *text, bool mine) { app_events_post_message(who, text, mine); }
static void net_status(const char *text, bool connected) { app_events_post_status(text, connected); }
static void net_online_reset(void) { app_events_post_online_reset(); }
static void net_online_item(const char *text) { app_events_post_online_item(text); }
static void net_file(const char *command, const char *from, const char *name, long size, const char *type) { app_events_post_file(command, from, name, size, type); }
static void net_auth(bool is_register, bool success, const char *message) { app_events_post_auth(is_register, success, message); }
static void net_local_port(unsigned short port) { app_events_post_local_port(port); }
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

static lv_display_t *main_display;

static void handle_app_event(const app_event_t *event)
{
    if(event != NULL && event->type == APP_EVENT_LOCAL_PORT && main_display != NULL) {
        char title[128];
        snprintf(title, sizeof(title), "Michat - Client %hu", event->port);
        lv_sdl_window_set_title(main_display, title);
        fprintf(stderr, "chat_sim: client assigned local port %hu\n", event->port);
    }
    frontend_handle_event(event);
}

static void capture_frame(lv_display_t *display)
{
    const char *path = getenv("LVGL_CAPTURE_PATH");
    SDL_Renderer *renderer;
    SDL_Surface *surface;
    int width;
    int height;

    if(path == NULL || path[0] == '\0') return;
    renderer = (SDL_Renderer *)lv_sdl_window_get_renderer(display);
    if(renderer == NULL || SDL_GetRendererOutputSize(renderer, &width, &height) != 0) return;
    surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    if(surface == NULL) return;
    if(SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                            surface->pixels, surface->pitch) == 0) {
        SDL_SaveBMP(surface, path);
        fprintf(stderr, "chat_sim: frame captured to %s (%dx%d)\n", path, width, height);
    }
    SDL_FreeSurface(surface);
}

int main(void)
{
    fprintf(stderr, "chat_sim: starting LVGL\n");
    lv_init();
    lv_group_t *input_group = lv_group_create();
    lv_group_set_default(input_group);
    lv_display_t *display = lv_sdl_window_create(960, 640);
    if(display == NULL) {
        fprintf(stderr, "chat_sim: SDL window creation failed\n");
        return 1;
    }
    fprintf(stderr, "chat_sim: SDL window created\n");
    main_display = display;
    lv_sdl_window_set_title(display, "Michat - Connecting...");
    lv_sdl_window_set_resizeable(display, true);
    lv_indev_t *mouse = lv_sdl_mouse_create();
    lv_indev_set_display(mouse, display);
    lv_indev_t *wheel = lv_sdl_mousewheel_create();
    lv_indev_set_display(wheel, display);
    lv_indev_set_group(wheel, input_group);
    lv_indev_t *keyboard = lv_sdl_keyboard_create();
    lv_indev_set_display(keyboard, display);
    lv_indev_set_group(keyboard, input_group);
    SDL_StartTextInput();
    frontend_init();
    frontend_actions_t actions = {
        .send_to = backend_send_to,
        .refresh = backend_refresh,
        .send_file_to = backend_send_file_to,
        .send_emoji_to = backend_send_emoji_to,
        .login = backend_login,
        .register_user = backend_register
    };
    frontend_set_actions(&actions);
    app_events_init(handle_app_event);
    backend_callbacks_t callbacks = {
        .on_message = net_message,
        .on_status = net_status,
        .on_online_reset = net_online_reset,
        .on_online_item = net_online_item,
        .on_file = net_file,
        .on_auth = net_auth,
        .on_local_port = net_local_port
    };
    backend_set_callbacks(&callbacks);
    backend_start();
    fprintf(stderr, "chat_sim: event loop started\n");
    for(int i = 0; i < 4; i++) {
        app_events_poll();
        lv_timer_handler();
        usleep(33000);
    }
    capture_frame(display);
    while(1) { app_events_poll(); lv_timer_handler(); usleep(5000); }
    return 0;
}
