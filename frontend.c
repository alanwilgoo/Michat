#include "frontend.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <dirent.h>

static lv_obj_t *messages;
static lv_obj_t *composer;
static lv_obj_t *status_line;
static lv_obj_t *chat_title;
static lv_obj_t *file_path_input;
static lv_obj_t *file_type_input;
static lv_obj_t *target_ip_input;
static lv_obj_t *target_port_input;
static lv_obj_t *online_list;
static frontend_actions_t actions;
static lv_obj_t *chat_header, *chat_body;
static lv_obj_t *auth_root, *auth_left, *auth_right, *auth_title, *auth_user, *auth_password, *auth_confirm, *auth_hint;
static lv_obj_t *auth_brand, *auth_submit_label, *auth_switch_label, *auth_forgot_label;
static lv_obj_t *auth_field_labels[2], *auth_dividers[2], *auth_confirm_label, *auth_confirm_divider;
static lv_obj_t *auth_switch, *auth_forgot, *auth_submit;
static bool auth_register_mode;
static void auth_button_event(lv_event_t *event);
static void auth_switch_event(lv_event_t *event);
static void auth_forgot_event(lv_event_t *event);
static lv_obj_t *auth_block(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t width,
                            lv_coord_t height, lv_color_t color, lv_opa_t opacity, lv_coord_t radius);
static void auth_layout(bool registration);
static void auth_set_mode(bool registration);

#define IMAGE_PREVIEW_MAX_BYTES (5L * 1024L * 1024L)
#define IMAGE_PREVIEW_MAX_WIDTH 280
#define IMAGE_PREVIEW_MAX_HEIGHT 210
#define EMOJI_LIMIT 64
#define EMOJI_PATH_MAX 1024

#ifndef MICHAT_EMOJI_DIR_DEFAULT
#define MICHAT_EMOJI_DIR_DEFAULT "assets/emojis"
#endif

static lv_obj_t *emoji_picker;
static char emoji_paths[EMOJI_LIMIT][EMOJI_PATH_MAX];
static char emoji_sources[EMOJI_LIMIT][EMOJI_PATH_MAX + 3];
static bool emoji_previewable[EMOJI_LIMIT];

void frontend_set_actions(const frontend_actions_t *value)
{
    actions = value ? *value : (frontend_actions_t){0};
}
/* Conversations are owned and updated by the LVGL main thread. */
/* Included by chat_ui.c: all session widgets belong to the UI thread. */
#define SESSION_LIMIT 32
typedef struct {
    char ip[64], username[64], key[96];
    unsigned short port;
    lv_obj_t *panel, *messages, *composer, *title, *label;
    char path[1024], type[64];
    unsigned unread;
} chat_session_t;
static chat_session_t sessions[SESSION_LIMIT];
static size_t session_count;
static chat_session_t *active_session;
static lv_obj_t *session_host, *session_list;
static lv_obj_t *inbox_messages, *inbox_composer, *inbox_title;
static char inbox_path[1024], inbox_type[64];
static void send_event(lv_event_t *event);
static void configure_input(lv_obj_t *input);
static lv_obj_t *text(lv_obj_t *parent, const char *value, lv_color_t color, bool large);
static bool target_from_inputs(const char **ip, unsigned short *port);
static long file_size(const char *path);
static bool image_source_path(const char *path, char *source, size_t source_size);
static void emoji_picker_event(lv_event_t *event);

static void session_badge(chat_session_t *s)
{
    char label[128];
    if(s->unread) snprintf(label, sizeof(label), "%s (%u)", s->key, s->unread);
    else snprintf(label, sizeof(label), "%s", s->key);
    lv_label_set_text(s->label, label);
}

static void session_set_username(chat_session_t *s, const char *username)
{
    if(s == NULL || username == NULL || username[0] == '\0') return;
    snprintf(s->username, sizeof(s->username), "%s", username);
    snprintf(s->key, sizeof(s->key), "%s", s->username);
    if(s->title) lv_label_set_text(s->title, s->key);
    if(s->label) session_badge(s);
}

static void session_select(chat_session_t *s)
{
    snprintf(active_session ? active_session->path : inbox_path, sizeof(inbox_path),
             "%s", lv_textarea_get_text(file_path_input));
    snprintf(active_session ? active_session->type : inbox_type, sizeof(inbox_type),
             "%s", lv_textarea_get_text(file_type_input));
    for(size_t i = 0; i < session_count; i++)
        lv_obj_add_flag(sessions[i].panel, LV_OBJ_FLAG_HIDDEN);
    active_session = s;
    messages = s ? s->messages : inbox_messages;
    composer = s ? s->composer : inbox_composer;
    chat_title = s ? s->title : inbox_title;
    lv_textarea_set_text(file_path_input, s ? s->path : inbox_path);
    lv_textarea_set_text(file_type_input, s ? s->type : inbox_type);
    if(s) {
        char port[8];
        snprintf(port, sizeof(port), "%hu", s->port);
        lv_textarea_set_text(target_ip_input, s->ip);
        lv_textarea_set_text(target_port_input, port);
        lv_obj_add_state(target_ip_input, LV_STATE_DISABLED);
        lv_obj_add_state(target_port_input, LV_STATE_DISABLED);
        lv_obj_remove_flag(s->panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s->panel);
        s->unread = 0;
        session_badge(s);
    } else {
        lv_obj_remove_state(target_ip_input, LV_STATE_DISABLED);
        lv_obj_remove_state(target_port_input, LV_STATE_DISABLED);
    }
    lv_group_focus_obj(composer);
}

static void session_click(lv_event_t *event)
{
    session_select(lv_event_get_user_data(event));
}

static chat_session_t *session_get(const char *ip, unsigned short port, const char *username)
{
    for(size_t i = 0; i < session_count; i++) {
        if(sessions[i].port == port && strcmp(sessions[i].ip, ip) == 0) {
            session_set_username(&sessions[i], username);
            return &sessions[i];
        }
    }
    if(session_count == SESSION_LIMIT) {
        frontend_status("Conversation limit reached (32)", false);
        return NULL;
    }
    chat_session_t *s = &sessions[session_count++];
    snprintf(s->ip, sizeof(s->ip), "%s", ip);
    snprintf(s->key, sizeof(s->key), "%s:%hu", ip, port);
    session_set_username(s, username);
    s->port = port;
    s->panel = lv_obj_create(session_host);
    lv_obj_add_flag(s->panel, LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(s->panel, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s->panel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(s->panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s->panel, lv_color_hex(0x416FF0), 0);
    lv_obj_set_style_border_width(s->panel, 2, 0);
    lv_obj_set_style_pad_all(s->panel, 10, 0);
    lv_obj_set_flex_flow(s->panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_t *header = lv_obj_create(s->panel);
    lv_obj_set_size(header, lv_pct(100), 40);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    s->title = text(header, s->key, lv_color_hex(0x162842), false);
    lv_obj_set_width(s->title, lv_pct(75));
    lv_label_set_long_mode(s->title, LV_LABEL_LONG_DOT);
    lv_obj_align(s->title, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *close = lv_button_create(header);
    lv_obj_set_size(close, 40, 30);
    lv_obj_align(close, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_center(text(close, "-", lv_color_white(), false));
    lv_obj_add_event_cb(close, session_click, LV_EVENT_CLICKED, NULL);
    s->messages = lv_obj_create(s->panel);
    lv_obj_set_width(s->messages, lv_pct(100));
    lv_obj_set_flex_grow(s->messages, 1);
    lv_obj_set_flex_flow(s->messages, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s->messages, 6, 0);
    lv_obj_set_scroll_dir(s->messages, LV_DIR_VER);
    s->composer = lv_textarea_create(s->panel);
    lv_textarea_set_one_line(s->composer, true);
    lv_textarea_set_max_length(s->composer, 600);
    lv_textarea_set_placeholder_text(s->composer, "Write a message...");
    lv_obj_set_width(s->composer, lv_pct(100));
    configure_input(s->composer);
    lv_obj_add_event_cb(s->composer, send_event, LV_EVENT_READY, NULL);
    lv_obj_t *send = lv_button_create(s->panel);
    lv_obj_set_size(send, lv_pct(100), 36);
    lv_obj_center(text(send, "Send", lv_color_white(), false));
    lv_obj_add_event_cb(send, send_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t *entry = lv_button_create(session_list);
    lv_obj_set_size(entry, lv_pct(100), 36);
    s->label = text(entry, s->key, lv_color_white(), false);
    lv_obj_set_width(s->label, lv_pct(100));
    lv_label_set_long_mode(s->label, LV_LABEL_LONG_DOT);
    lv_obj_center(s->label);
    lv_obj_add_event_cb(entry, session_click, LV_EVENT_CLICKED, s);
    return s;
}

static chat_session_t *session_from_sender(const char *who)
{
    char ip[64], extra;
    unsigned port;
    struct in_addr addr;
    if(sscanf(who, "%63[^:]:%u%c", ip, &port, &extra) != 2 ||
       port == 0 || port > 65535 || inet_pton(AF_INET, ip, &addr) != 1) return NULL;
    return session_get(ip, (unsigned short)port, NULL);
}

static void online_item_event(lv_event_t *event)
{
    lv_obj_t *button = lv_event_get_target(event);
    lv_obj_t *label = lv_obj_get_child(button, 0);
    const char *item = label ? lv_label_get_text(label) : NULL;
    const char *endpoint;
    const char *separator;
    char ip[64];
    char username[64] = {0};
    char *end;
    unsigned long port;
    size_t ip_length;

    if(item == NULL || target_ip_input == NULL || target_port_input == NULL) return;
    endpoint = strrchr(item, ' ');
    if(endpoint != NULL && endpoint[1] != '\0') {
        size_t username_length = (size_t)(endpoint - item);
        while(username_length > 0 && item[username_length - 1] == ' ') username_length--;
        if(username_length > 0 && username_length < sizeof(username)) {
            memcpy(username, item, username_length);
            username[username_length] = '\0';
            endpoint++;
        } else endpoint = item;
    } else endpoint = item;
    separator = strrchr(endpoint, ':');
    if(separator == NULL) return;
    ip_length = (size_t)(separator - endpoint);
    if(ip_length == 0 || ip_length >= sizeof(ip)) return;
    memcpy(ip, endpoint, ip_length);
    ip[ip_length] = '\0';
    port = strtoul(separator + 1, &end, 10);
    if(*end != '\0' || port == 0 || port > 65535) return;
    chat_session_t *session = session_get(ip, (unsigned short)port, username[0] ? username : NULL);
    if(session) session_select(session);
}

static lv_obj_t *text(lv_obj_t *parent, const char *value, lv_color_t color, bool large)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, value);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_font(label, large ? &lv_font_montserrat_22 : &lv_font_montserrat_14, 0);
    return label;
}

static lv_obj_t *auth_block(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t width,
                            lv_coord_t height, lv_color_t color, lv_opa_t opacity, lv_coord_t radius)
{
    lv_obj_t *block = lv_obj_create(parent);
    lv_obj_add_flag(block, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_pos(block, x, y);
    lv_obj_set_size(block, width, height);
    lv_obj_set_style_bg_color(block, color, 0);
    lv_obj_set_style_bg_opa(block, opacity, 0);
    lv_obj_set_style_border_width(block, 0, 0);
    lv_obj_set_style_radius(block, radius, 0);
    lv_obj_set_style_pad_all(block, 0, 0);
    lv_obj_remove_flag(block, LV_OBJ_FLAG_SCROLLABLE);
    return block;
}

static void auth_layout(bool registration)
{
    const lv_coord_t title_y = registration ? 82 : 132;
    const lv_coord_t first_y = registration ? 152 : 208;
    const lv_coord_t second_y = registration ? 226 : 282;

    lv_obj_align(auth_title, LV_ALIGN_TOP_MID, 0, title_y);
    lv_obj_align(auth_field_labels[0], LV_ALIGN_TOP_MID, 0, first_y);
    lv_obj_align(auth_user, LV_ALIGN_TOP_MID, 0, first_y + 16);
    lv_obj_set_pos(auth_dividers[0], 0, first_y + 54);
    lv_obj_align(auth_field_labels[1], LV_ALIGN_TOP_MID, 0, second_y);
    lv_obj_align(auth_password, LV_ALIGN_TOP_MID, 0, second_y + 16);
    lv_obj_set_pos(auth_dividers[1], 0, second_y + 54);

    if(registration) {
        const lv_coord_t confirm_y = 300;
        lv_obj_remove_flag(auth_confirm_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(auth_confirm, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(auth_confirm_divider, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(auth_forgot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(auth_confirm_label, LV_ALIGN_TOP_MID, 0, confirm_y);
        lv_obj_align(auth_confirm, LV_ALIGN_TOP_MID, 0, confirm_y + 16);
        lv_obj_set_pos(auth_confirm_divider, 0, confirm_y + 54);
        lv_obj_set_size(auth_switch, 180, 32);
        lv_obj_align(auth_switch, LV_ALIGN_TOP_MID, 0, 376);
        lv_obj_align(auth_submit, LV_ALIGN_TOP_MID, 0, 426);
        lv_obj_align(auth_hint, LV_ALIGN_TOP_MID, 0, 478);
    } else {
        lv_obj_add_flag(auth_confirm_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(auth_confirm, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(auth_confirm_divider, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(auth_forgot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(auth_switch, 164, 32);
        lv_obj_align(auth_switch, LV_ALIGN_TOP_MID, -100, 356);
        lv_obj_align(auth_forgot, LV_ALIGN_TOP_MID, 105, 356);
        lv_obj_align(auth_submit, LV_ALIGN_TOP_MID, 0, 410);
        lv_obj_align(auth_hint, LV_ALIGN_TOP_MID, 0, 462);
    }
}

static void auth_set_mode(bool registration)
{
    auth_register_mode = registration;
    lv_label_set_text(auth_title, registration ? "Create account" : "Sign In");
    lv_label_set_text(auth_submit_label, registration ? "Create account" : "Sign In");
    lv_label_set_text(auth_switch_label, registration ? "Back to sign in" : "New user? Sign up");
    if(!registration) lv_textarea_set_text(auth_confirm, "");
    auth_layout(registration);
}

static lv_obj_t *message_view_for(const char *who, bool mine)
{
    chat_session_t *destination = mine ? active_session : session_from_sender(who);
    lv_obj_t *view = destination ? destination->messages : inbox_messages;
    if(!view) view = messages;
    if(destination && destination != active_session) {
        destination->unread++;
        session_badge(destination);
    }
    return view;
}

static lv_obj_t *message_row(lv_obj_t *view, const char *who, bool mine)
{
    /* Bound widget memory during long-running conversations. */
    if(lv_obj_get_child_count(view) >= 200) lv_obj_delete(lv_obj_get_child(view, 0));
    lv_obj_t *row = lv_obj_create(view);
    lv_obj_set_width(row, lv_pct(100)); lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 4, 0);

#ifdef MICHAT_DESKTOP_AVATARS
    /* Keep the embedded UI unchanged.  The desktop client uses a WeChat-like
     * row: peer avatar + content on the left, own content + avatar on right. */
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, mine ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *avatar = lv_obj_create(row);
    lv_obj_set_size(avatar, 36, 36);
    lv_obj_set_style_radius(avatar, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(avatar, 0, 0);
    lv_obj_set_style_bg_color(avatar, mine ? lv_color_hex(0x4F8DF7) : lv_color_hex(0x8E78D6), 0);
    lv_obj_set_style_bg_opa(avatar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(avatar, 0, 0);
    lv_obj_clear_flag(avatar, LV_OBJ_FLAG_SCROLLABLE);

    char initials[3] = "P";
    if(mine) {
        strcpy(initials, "ME");
    } else if(who != NULL && who[0] != '\0' && !isdigit((unsigned char)who[0])) {
        initials[0] = (char)toupper((unsigned char)who[0]);
        initials[1] = '\0';
    }
    lv_obj_t *avatar_text = text(avatar, initials, lv_color_white(), false);
    lv_obj_center(avatar_text);

    lv_obj_t *content = lv_obj_create(row);
    lv_obj_set_width(content, lv_pct(88));
    lv_obj_set_height(content, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START,
                          mine ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);

    if(mine) {
        lv_obj_move_to_index(avatar, -1);
    }
    text(content, mine ? "You" : who, lv_color_hex(0x7888A3), false);
    return content;
#else
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, mine ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    text(row, who, lv_color_hex(0x7888A3), false);
    return row;
#endif
}

static const char *message_sender_name(const char *who, bool mine)
{
    chat_session_t *session;
    if(mine) return "You";
    session = session_from_sender(who);
    return session && session->username[0] ? session->username : who;
}

void frontend_message(const char *who, const char *value, bool mine)
{
    lv_obj_t *view = message_view_for(who, mine);
    const char *sender_name = message_sender_name(who, mine);
    if(!view) view = messages;
    lv_obj_t *row = message_row(view, sender_name, mine);
    lv_obj_t *bubble = lv_obj_create(row);
    lv_obj_set_width(bubble, lv_pct(95)); lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(bubble, 430, 0); lv_obj_set_style_pad_all(bubble, 12, 0);
    lv_obj_set_style_radius(bubble, 14, 0); lv_obj_set_style_border_width(bubble, 0, 0);
    lv_obj_set_style_bg_color(bubble, mine ? lv_color_hex(0x416FF0) : lv_color_hex(0xE7EDF7), 0); lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
    lv_obj_t *body = text(bubble, value, mine ? lv_color_white() : lv_color_hex(0x20314D), false);
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP); lv_obj_set_width(body, lv_pct(100));
    lv_obj_scroll_to_y(view, LV_COORD_MAX, LV_ANIM_ON);
}

static bool image_type(const char *value)
{
    const char *extension;
    if(value == NULL || value[0] == '\0') return false;
    extension = strrchr(value, '.');
    extension = extension ? extension + 1 : value;
    if(strncmp(extension, "image/", 6) == 0) extension += 6;
    return strcasecmp(extension, "png") == 0 || strcasecmp(extension, "jpg") == 0 ||
           strcasecmp(extension, "jpeg") == 0 || strcasecmp(extension, "bmp") == 0;
}

static bool image_attachment(const char *path, const char *type)
{
    return image_type(type) || image_type(path);
}

static bool image_source_path(const char *path, char *source, size_t source_size)
{
    int written;
    if(path == NULL || path[0] == '\0' || source == NULL || source_size == 0) return false;
    if(path[0] == 'A' && path[1] == ':') written = snprintf(source, source_size, "%s", path);
    else written = snprintf(source, source_size, "A:%s", path);
    return written >= 0 && (size_t)written < source_size;
}

static bool frontend_image_attachment(const char *who, const char *path, const char *type, long size, bool mine)
{
    char source[1152];
    lv_image_header_t header;
    lv_coord_t width;
    lv_coord_t height;

    if(size > IMAGE_PREVIEW_MAX_BYTES || !image_source_path(path, source, sizeof(source))) return false;
    if(lv_image_decoder_get_info(source, &header) != LV_RESULT_OK || header.w == 0 || header.h == 0 ||
       header.w > 8192 || header.h > 8192) return false;

    width = (lv_coord_t)header.w;
    height = (lv_coord_t)header.h;
    if(width > IMAGE_PREVIEW_MAX_WIDTH || height > IMAGE_PREVIEW_MAX_HEIGHT) {
        if((int32_t)width * IMAGE_PREVIEW_MAX_HEIGHT > (int32_t)height * IMAGE_PREVIEW_MAX_WIDTH) {
            height = (lv_coord_t)((int32_t)height * IMAGE_PREVIEW_MAX_WIDTH / width);
            width = IMAGE_PREVIEW_MAX_WIDTH;
        } else {
            width = (lv_coord_t)((int32_t)width * IMAGE_PREVIEW_MAX_HEIGHT / height);
            height = IMAGE_PREVIEW_MAX_HEIGHT;
        }
    }

    lv_obj_t *view = message_view_for(who, mine);
    if(!view) view = messages;
    lv_obj_t *row = message_row(view, message_sender_name(who, mine), mine);
    lv_obj_t *bubble = lv_obj_create(row);
    lv_obj_set_size(bubble, width + 16, height + 16);
    lv_obj_set_style_pad_all(bubble, 8, 0);
    lv_obj_set_style_radius(bubble, 14, 0);
    lv_obj_set_style_border_width(bubble, 0, 0);
    lv_obj_set_style_bg_color(bubble, mine ? lv_color_hex(0x416FF0) : lv_color_hex(0xE7EDF7), 0);
    lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
    lv_obj_t *image = lv_image_create(bubble);
    lv_image_set_src(image, source);
    /* Loading a source updates LVGL's natural size; set our bounded box afterwards. */
    lv_obj_set_size(image, width, height);
    lv_image_set_inner_align(image, LV_IMAGE_ALIGN_STRETCH);
    lv_obj_center(image);
    /* Recalculate the flex content before scrolling.  Without this, a newly
     * added image can remain partially behind the composer until the next
     * layout pass. */
    lv_obj_update_layout(view);
    lv_obj_scroll_to_view(row, LV_ANIM_OFF);
    (void)type;
    return true;
}

static long file_size(const char *path)
{
    FILE *file;
    long size;
    if(path == NULL || (file = fopen(path, "rb")) == NULL) return -1;
    if(fseek(file, 0, SEEK_END) != 0) { fclose(file); return -1; }
    size = ftell(file);
    fclose(file);
    return size;
}

static const char *emoji_directory(void)
{
    const char *override = getenv("MICHAT_EMOJI_DIR");
    return override && override[0] ? override : MICHAT_EMOJI_DIR_DEFAULT;
}

static size_t emoji_collect(void)
{
    DIR *directory;
    struct dirent *entry;
    const char *folder = emoji_directory();
    size_t count = 0;

    directory = opendir(folder);
    if(directory == NULL) return 0;
    while((entry = readdir(directory)) != NULL && count < EMOJI_LIMIT) {
        lv_image_header_t header;
        long size;
        int written;

        if(entry->d_name[0] == '.' || !image_type(entry->d_name)) continue;
        written = snprintf(emoji_paths[count], sizeof(emoji_paths[count]), "%s/%s", folder, entry->d_name);
        if(written < 0 || (size_t)written >= sizeof(emoji_paths[count])) continue;
        size = file_size(emoji_paths[count]);
        if(size <= 0 || size > IMAGE_PREVIEW_MAX_BYTES ||
           !image_source_path(emoji_paths[count], emoji_sources[count], sizeof(emoji_sources[count]))) continue;
        emoji_previewable[count] = lv_image_decoder_get_info(emoji_sources[count], &header) == LV_RESULT_OK &&
                                   header.w > 0 && header.h > 0 && header.w <= 8192 && header.h <= 8192;
        count++;
    }
    closedir(directory);
    return count;
}

static void emoji_picker_close(void)
{
    lv_obj_t *popup = emoji_picker;
    emoji_picker = NULL;
    if(popup) lv_obj_delete(popup);
}

static void emoji_send_event(lv_event_t *event)
{
    const char *path = lv_event_get_user_data(event);
    const char *ip;
    unsigned short port;
    long size;

    if(path == NULL || !target_from_inputs(&ip, &port)) return;
    size = file_size(path);
    if(size <= 0 || size > IMAGE_PREVIEW_MAX_BYTES) {
        frontend_status("Emoji image is missing or too large", false);
        return;
    }
    if(actions.send_emoji_to && actions.send_emoji_to(ip, port, path) == 0) {
        if(frontend_image_attachment("You", path, path, size, true))
            frontend_status("Emoji sent", true);
        else
            frontend_status("Emoji sent; preview unavailable", true);
        emoji_picker_close();
    } else {
        frontend_status("Emoji send failed", false);
    }
}

static void emoji_picker_event(lv_event_t *event)
{
    (void)event;
    emoji_picker_close();
}

static void emoji_picker_open(void)
{
    lv_obj_t *grid;
    size_t count;

    emoji_picker_close();
    emoji_picker = lv_obj_create(lv_layer_top());
    lv_obj_add_flag(emoji_picker, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(emoji_picker, 520, 380);
    lv_obj_center(emoji_picker);
    lv_obj_set_style_bg_color(emoji_picker, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(emoji_picker, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(emoji_picker, lv_color_hex(0x416FF0), 0);
    lv_obj_set_style_border_width(emoji_picker, 2, 0);
    lv_obj_set_style_radius(emoji_picker, 14, 0);
    lv_obj_set_style_shadow_width(emoji_picker, 22, 0);
    lv_obj_set_style_shadow_opa(emoji_picker, LV_OPA_30, 0);
    lv_obj_set_style_pad_all(emoji_picker, 12, 0);
    lv_obj_remove_flag(emoji_picker, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = text(emoji_picker, "Emoji", lv_color_hex(0x162842), true);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);
    lv_obj_t *close = lv_button_create(emoji_picker);
    lv_obj_set_size(close, 42, 30);
    lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 0, -2);
    lv_obj_set_style_bg_color(close, lv_color_hex(0xEAF1FF), 0);
    lv_obj_set_style_bg_opa(close, LV_OPA_COVER, 0);
    lv_obj_center(text(close, "Close", lv_color_hex(0x1E3763), false));
    lv_obj_add_event_cb(close, emoji_picker_event, LV_EVENT_CLICKED, NULL);

    grid = lv_obj_create(emoji_picker);
    lv_obj_set_size(grid, lv_pct(100), 310);
    lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(grid, lv_color_hex(0xF8FAFD), 0);
    lv_obj_set_style_bg_opa(grid, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 8, 0);
    lv_obj_set_style_pad_row(grid, 8, 0);
    lv_obj_set_style_pad_column(grid, 8, 0);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_scroll_dir(grid, LV_DIR_VER);

    count = emoji_collect();
    if(count == 0) {
        lv_obj_t *empty = text(grid, "No readable emoji images in assets/emojis.\nAdd PNG, JPG, JPEG, or BMP files, then reopen this panel.",
                               lv_color_hex(0x7888A3), false);
        lv_obj_set_width(empty, lv_pct(100));
        lv_label_set_long_mode(empty, LV_LABEL_LONG_WRAP);
        return;
    }
    for(size_t i = 0; i < count; i++) {
        const char *filename = strrchr(emoji_paths[i], '/');
        lv_obj_t *card = lv_button_create(grid);
        lv_obj_set_size(card, 88, 94);
        lv_obj_set_style_bg_color(card, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0xD6DEEC), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_radius(card, 8, 0);
        if(emoji_previewable[i]) {
            lv_obj_t *image = lv_image_create(card);
            lv_image_set_src(image, emoji_sources[i]);
            lv_obj_set_size(image, 62, 62);
            lv_image_set_inner_align(image, LV_IMAGE_ALIGN_STRETCH);
            lv_obj_align(image, LV_ALIGN_TOP_MID, 0, 3);
        } else {
            lv_obj_t *unavailable = text(card, "Preview\nunavailable", lv_color_hex(0x7888A3), false);
            lv_obj_set_width(unavailable, 78);
            lv_obj_set_style_text_align(unavailable, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(unavailable, LV_ALIGN_TOP_MID, 0, 16);
        }
        lv_obj_t *name = text(card, filename ? filename + 1 : emoji_paths[i], lv_color_hex(0x33445F), false);
        lv_obj_set_width(name, 78);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_align(name, LV_ALIGN_BOTTOM_MID, 0, -2);
        lv_obj_add_event_cb(card, emoji_send_event, LV_EVENT_CLICKED, emoji_paths[i]);
    }
}

static void emoji_open_event(lv_event_t *event)
{
    (void)event;
    emoji_picker_open();
}

void frontend_status(const char *value, bool connected)
{
    if(status_line) lv_label_set_text(status_line, value);
    if(status_line) lv_obj_set_style_text_color(status_line, lv_color_hex(connected ? 0x237950 : 0xB13D3D), 0);
}

void frontend_online_reset(void)
{
    if(online_list) lv_obj_clean(online_list);
}

void frontend_online_item(const char *value)
{
    char display[160];
    const char *separator;
    if(online_list == NULL || value == NULL) return;
    lv_obj_t *item = lv_button_create(online_list);
    lv_obj_set_width(item, lv_pct(100));
    lv_obj_set_height(item, 28);
    lv_obj_set_style_bg_opa(item, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(item, 0, 0);
    lv_obj_set_style_pad_all(item, 0, 0);
    lv_obj_add_event_cb(item, online_item_event, LV_EVENT_CLICKED, NULL);
    separator = strchr(value, '@');
    if(separator != NULL && separator != value)
        snprintf(display, sizeof(display), "%.*s  %s", (int)(separator - value), value, separator + 1);
    else snprintf(display, sizeof(display), "%s", value);
    lv_obj_t *label = lv_label_create(item);
    lv_label_set_text(label, display);
    lv_obj_set_width(label, lv_pct(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(label, lv_color_hex(0x33445F), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_center(label);
}

void frontend_file_received(const char *command, const char *from, const char *name, long size, const char *type)
{
    char message[256];
    const char *kind = strcmp(command, "emoji") == 0 ? "Emoji" : "File";
    snprintf(message, sizeof(message), "%s received: %.160s (%ld bytes%s%.40s)", kind,
             name, size, type && type[0] ? ", " : "", type && type[0] ? type : "");
    if(image_attachment(name, type) && frontend_image_attachment(from ? from : "Peer", name, type, size, false))
        frontend_status("Image received", true);
    else {
        frontend_message(from ? from : "System", message, false);
        frontend_status("Attachment saved", true);
    }
}

void frontend_handle_event(const app_event_t *event)
{
    if(event == NULL) return;
    if(event->type == APP_EVENT_MESSAGE) frontend_message(event->who, event->text, event->mine);
    else if(event->type == APP_EVENT_STATUS) frontend_status(event->text, event->connected);
    else if(event->type == APP_EVENT_ONLINE_RESET) frontend_online_reset();
    else if(event->type == APP_EVENT_ONLINE_ITEM) frontend_online_item(event->text);
    else if(event->type == APP_EVENT_FILE) frontend_file_received(event->attachment_command, event->who, event->text, event->size, event->attachment_type);
    else if(event->type == APP_EVENT_AUTH) {
        if(event->auth_register) {
            lv_label_set_text(auth_hint, event->text);
            if(event->connected) {
                auth_set_mode(false);
                lv_textarea_set_text(auth_password, "");
                lv_textarea_set_text(auth_confirm, "");
                lv_label_set_text(auth_hint, "Registration successful. Please log in.");
            }
        } else if(event->connected) {
            lv_obj_add_flag(auth_root, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(chat_header, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(chat_body, LV_OBJ_FLAG_HIDDEN);
            frontend_status("Authenticated", true);
        } else lv_label_set_text(auth_hint, event->text);
    }
}

static void auth_button_event(lv_event_t *event)
{
    const char *user = lv_textarea_get_text(auth_user);
    const char *password = lv_textarea_get_text(auth_password);
    const char *confirm = lv_textarea_get_text(auth_confirm);
    if(user == NULL || password == NULL || user[0] == '\0' || password[0] == '\0') {
        lv_label_set_text(auth_hint, "Username and password are required"); return;
    }
    if(strchr(user, '@') || strchr(password, '@') || strchr(user, '\n') || strchr(password, '\n')) {
        lv_label_set_text(auth_hint, "Username/password contains an invalid character"); return;
    }
    if(auth_register_mode && (confirm == NULL || strcmp(password, confirm) != 0)) {
        lv_label_set_text(auth_hint, "Passwords do not match"); return;
    }
    int result = auth_register_mode ? (actions.register_user ? actions.register_user(user, password) : -1)
                                   : (actions.login ? actions.login(user, password) : -1);
    lv_label_set_text(auth_hint, result == 0 ? "Request sent..." : "Server is offline");
}

static void auth_switch_event(lv_event_t *event)
{
    (void)event;
    auth_set_mode(!auth_register_mode);
    lv_label_set_text(auth_hint, "");
}

static void auth_forgot_event(lv_event_t *event)
{
    (void)event;
    lv_label_set_text(auth_hint, "Password reset must be handled by the server administrator");
}

static bool target_from_inputs(const char **ip, unsigned short *port)
{
    if(active_session) {
        *ip = active_session->ip;
        *port = active_session->port;
        return true;
    }
    const char *port_text;
    char *end;
    unsigned long parsed;

    *ip = lv_textarea_get_text(target_ip_input);
    port_text = lv_textarea_get_text(target_port_input);
    if(*ip == NULL || (*ip)[0] == '\0' || port_text == NULL || port_text[0] == '\0') {
        frontend_status("Enter target IP and port", false);
        return false;
    }
    parsed = strtoul(port_text, &end, 10);
    struct in_addr address;
    if(inet_pton(AF_INET, *ip, &address) != 1) {
        frontend_status("Enter a valid IPv4 address", false);
        return false;
    }
    if(*end != '\0' || parsed == 0 || parsed > 65535) {
        frontend_status("Target port must be 1-65535", false);
        return false;
    }
    *port = (unsigned short)parsed;
    return true;
}

static void send_event(lv_event_t *event)
{
    const char *ip;
    unsigned short port;
    (void)event;
    const char *value = lv_textarea_get_text(composer);
    if(!value || !value[0]) return;
    if(strchr(value, '@') || strchr(value, '\n') || strlen(value) > 1800) {
        frontend_status("Message too long or contains protocol delimiter @", false);
        return;
    }
    if(!target_from_inputs(&ip, &port)) return;
    if(actions.send_to && actions.send_to(value, ip, port) == 0) {
        frontend_message("You", value, true);
        lv_textarea_set_text(composer, "");
        lv_group_focus_obj(composer);
        frontend_status("Message submitted to server", true);
    } else {
        frontend_status("Message not sent: server is offline", false);
    }
}

static void refresh_event(lv_event_t *event)
{
    (void)event;
    if(actions.refresh && actions.refresh() == 0) frontend_status("Refreshing online list...", true);
    else frontend_status("Cannot refresh: server is offline", false);
}

static void file_event(lv_event_t *event)
{
    const char *command = lv_event_get_user_data(event);
    const char *path = lv_textarea_get_text(file_path_input);
    const char *type = lv_textarea_get_text(file_type_input);
    const char *ip;
    unsigned short port;
    if(!target_from_inputs(&ip, &port)) return;
    if(path == NULL || path[0] == '\0') {
        frontend_status("Enter a file path first", false);
        return;
    }
    if(type == NULL || type[0] == '\0') type = "bin";
    if(actions.send_file_to && actions.send_file_to(command, ip, port, path, type) == 0) {
        long size = file_size(path);
        if(image_attachment(path, type) && frontend_image_attachment("You", path, type, size, true))
            frontend_status("Image sent", true);
        else
            frontend_status(strcmp(command, "emoji") == 0 ? "Emoji sent" : "File sent", true);
    } else
        frontend_status("File send failed", false);
}

static void contact_event(lv_event_t *event)
{
    const char *name = lv_event_get_user_data(event);
    if(name) lv_label_set_text(chat_title, name);
}

static void contact(lv_obj_t *parent, const char *name, const char *preview, const char *initial)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_width(button, lv_pct(100)); lv_obj_set_height(button, 60);
    lv_obj_set_style_bg_color(button, lv_color_hex(0xEAF1FF), 0); lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(button, 10, 0);
    lv_obj_add_event_cb(button, contact_event, LV_EVENT_CLICKED, (void *)name);
    lv_obj_t *avatar = lv_obj_create(button); lv_obj_set_size(avatar, 36, 36);
    lv_obj_set_style_bg_color(avatar, lv_color_hex(0x416FF0), 0); lv_obj_set_style_bg_opa(avatar, LV_OPA_COVER, 0); lv_obj_set_style_radius(avatar, LV_RADIUS_CIRCLE, 0); lv_obj_align(avatar, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_t *initial_label = text(avatar, initial, lv_color_white(), false); lv_obj_center(initial_label);
    lv_obj_t *name_label = text(button, name, lv_color_hex(0x1C2B44), false); lv_obj_align(name_label, LV_ALIGN_TOP_LEFT, 52, 8);
    lv_obj_t *preview_label = text(button, preview, lv_color_hex(0x7888A3), false); lv_obj_set_width(preview_label, 165); lv_label_set_long_mode(preview_label, LV_LABEL_LONG_DOT); lv_obj_align(preview_label, LV_ALIGN_BOTTOM_LEFT, 52, -7);
}

static void configure_input(lv_obj_t *input)
{
    /* One-line mode resets height; apply sizing after it. */
    lv_obj_set_height(input, 42);
    lv_obj_set_style_pad_all(input, 10, 0);
    lv_obj_set_style_radius(input, 6, 0);
    lv_obj_set_style_bg_color(input, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(input, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(input, lv_color_hex(0xAAB7CC), 0);
    lv_obj_set_style_border_width(input, 2, 0);
    lv_obj_set_style_border_color(input, lv_color_hex(0x416FF0), LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(input, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(input, LV_OPA_COVER, LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(input, lv_color_hex(0x416FF0), LV_PART_CURSOR | LV_STATE_FOCUSED);
    lv_obj_set_style_width(input, 2, LV_PART_CURSOR);
    lv_group_add_obj(lv_group_get_default(), input);
}

void frontend_init(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, &lv_font_montserrat_14, 0);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xE9DDFC), 0); lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0); lv_obj_set_style_pad_all(screen, 18, 0);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(screen, 12, 0);
    auth_root = lv_obj_create(screen);
    lv_obj_set_width(auth_root, lv_pct(100)); lv_obj_set_flex_grow(auth_root, 1);
    lv_obj_set_style_bg_color(auth_root, lv_color_white(), 0); lv_obj_set_style_bg_opa(auth_root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(auth_root, 0, 0); lv_obj_set_style_radius(auth_root, 0, 0); lv_obj_set_style_pad_all(auth_root, 0, 0);
    lv_obj_set_layout(auth_root, LV_LAYOUT_NONE); lv_obj_remove_flag(auth_root, LV_OBJ_FLAG_SCROLLABLE);

    auth_left = auth_block(auth_root, 0, 0, lv_pct(44), lv_pct(100), lv_color_hex(0x9077CD), LV_OPA_COVER, 0);
    auth_right = auth_block(auth_root, lv_pct(44), 0, lv_pct(56), lv_pct(100), lv_color_white(), LV_OPA_COVER, 0);
    auth_brand = text(auth_left, "MICHAT", lv_color_white(), true); lv_obj_align(auth_brand, LV_ALIGN_TOP_LEFT, 24, 22);
    lv_obj_t *welcome = text(auth_left, "Welcome Back!", lv_color_white(), true); lv_obj_align(welcome, LV_ALIGN_TOP_MID, 0, 165);
    lv_obj_t *subtitle = text(auth_left, "Stay close to your conversations\nand the people who matter.", lv_color_hex(0xF2EEFF), false);
    lv_obj_set_width(subtitle, lv_pct(82)); lv_obj_set_style_text_align(subtitle, LV_TEXT_ALIGN_CENTER, 0); lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 207);

    auth_block(auth_left, 0, 465, 92, 160, lv_color_hex(0x735CB3), LV_OPA_80, 0);
    auth_block(auth_left, 74, 505, 75, 120, lv_color_hex(0x765EB7), LV_OPA_90, 0);
    auth_block(auth_left, 146, 432, 61, 193, lv_color_hex(0x8068C0), LV_OPA_COVER, 0);
    auth_block(auth_left, 204, 492, 58, 133, lv_color_hex(0x755DB4), LV_OPA_COVER, 0);
    auth_block(auth_left, 260, 396, 90, 229, lv_color_hex(0x785FBA), LV_OPA_COVER, 0);
    auth_block(auth_left, 339, 460, 68, 165, lv_color_hex(0x6E56AC), LV_OPA_COVER, 0);
    auth_block(auth_left, 392, 510, 36, 115, lv_color_hex(0x7C63BD), LV_OPA_COVER, 0);
    for(int row = 0; row < 5; row++) {
        auth_block(auth_left, 12, 490 + row * 27, 20, 12, lv_color_hex(0xBFAFE7), LV_OPA_80, 0);
        auth_block(auth_left, 46, 490 + row * 27, 25, 12, lv_color_hex(0xBFAFE7), LV_OPA_80, 0);
        auth_block(auth_left, 164, 515 + row * 26, 12, 14, lv_color_hex(0xC3B5E9), LV_OPA_80, 0);
        auth_block(auth_left, 184, 515 + row * 26, 12, 14, lv_color_hex(0xC3B5E9), LV_OPA_80, 0);
    }
    auth_block(auth_left, 282, 438, 42, 12, lv_color_hex(0xCFC4ED), LV_OPA_80, 0);
    auth_block(auth_left, 282, 470, 42, 12, lv_color_hex(0xCFC4ED), LV_OPA_80, 0);
    auth_block(auth_left, 282, 502, 42, 12, lv_color_hex(0xCFC4ED), LV_OPA_80, 0);
    text(auth_left, "f", lv_color_white(), true); lv_obj_align(lv_obj_get_child(auth_left, -1), LV_ALIGN_TOP_LEFT, 222, 362);
    text(auth_left, "G+", lv_color_white(), false); lv_obj_align(lv_obj_get_child(auth_left, -1), LV_ALIGN_TOP_LEFT, 46, 374);
    text(auth_left, "in", lv_color_white(), false); lv_obj_align(lv_obj_get_child(auth_left, -1), LV_ALIGN_TOP_LEFT, 174, 455);
    text(auth_left, "@", lv_color_white(), true); lv_obj_align(lv_obj_get_child(auth_left, -1), LV_ALIGN_TOP_LEFT, 366, 405);

    auth_title = text(auth_right, "Sign In", lv_color_hex(0x3F3B4B), true);
    auth_field_labels[0] = text(auth_right, "Username", lv_color_hex(0xB8B4BF), false); lv_obj_set_width(auth_field_labels[0], lv_pct(68));
    auth_user = lv_textarea_create(auth_right); lv_obj_set_width(auth_user, lv_pct(68)); lv_obj_set_height(auth_user, 40); lv_textarea_set_one_line(auth_user, true); lv_textarea_set_placeholder_text(auth_user, "Enter your username"); configure_input(auth_user);
    lv_obj_set_style_bg_opa(auth_user, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(auth_user, 0, 0); lv_obj_set_style_pad_left(auth_user, 0, 0);
    auth_dividers[0] = auth_block(auth_right, 0, 0, lv_pct(100), 1, lv_color_hex(0xDDD9E1), LV_OPA_COVER, 0);
    auth_field_labels[1] = text(auth_right, "Password", lv_color_hex(0xB8B4BF), false); lv_obj_set_width(auth_field_labels[1], lv_pct(68));
    auth_password = lv_textarea_create(auth_right); lv_obj_set_width(auth_password, lv_pct(68)); lv_obj_set_height(auth_password, 40); lv_textarea_set_one_line(auth_password, true); lv_textarea_set_password_mode(auth_password, true); lv_textarea_set_placeholder_text(auth_password, "Enter your password"); configure_input(auth_password);
    lv_obj_set_style_bg_opa(auth_password, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(auth_password, 0, 0); lv_obj_set_style_pad_left(auth_password, 0, 0);
    auth_dividers[1] = auth_block(auth_right, 0, 0, lv_pct(100), 1, lv_color_hex(0xDDD9E1), LV_OPA_COVER, 0);
    auth_confirm_label = text(auth_right, "Confirm password", lv_color_hex(0xB8B4BF), false); lv_obj_set_width(auth_confirm_label, lv_pct(68));
    auth_confirm = lv_textarea_create(auth_right); lv_obj_set_width(auth_confirm, lv_pct(68)); lv_obj_set_height(auth_confirm, 40); lv_textarea_set_one_line(auth_confirm, true); lv_textarea_set_password_mode(auth_confirm, true); lv_textarea_set_placeholder_text(auth_confirm, "Re-enter your password"); configure_input(auth_confirm);
    lv_obj_set_style_bg_opa(auth_confirm, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(auth_confirm, 0, 0); lv_obj_set_style_pad_left(auth_confirm, 0, 0);
    auth_confirm_divider = auth_block(auth_right, 0, 0, lv_pct(100), 1, lv_color_hex(0xDDD9E1), LV_OPA_COVER, 0);

    auth_switch = lv_button_create(auth_right); lv_obj_set_style_bg_opa(auth_switch, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(auth_switch, 0, 0); auth_switch_label = text(auth_switch, "New user? Sign up", lv_color_hex(0x7C61B5), false); lv_obj_center(auth_switch_label); lv_obj_add_event_cb(auth_switch, auth_switch_event, LV_EVENT_CLICKED, NULL);
    auth_forgot = lv_button_create(auth_right); lv_obj_set_size(auth_forgot, 150, 32); lv_obj_set_style_bg_opa(auth_forgot, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(auth_forgot, 0, 0); auth_forgot_label = text(auth_forgot, "Forgot Password?", lv_color_hex(0x7C61B5), false); lv_obj_center(auth_forgot_label); lv_obj_add_event_cb(auth_forgot, auth_forgot_event, LV_EVENT_CLICKED, NULL);
    auth_submit = lv_button_create(auth_right); lv_obj_set_size(auth_submit, 150, 40); lv_obj_set_style_bg_color(auth_submit, lv_color_hex(0x8F72CA), 0); lv_obj_set_style_bg_opa(auth_submit, LV_OPA_COVER, 0); lv_obj_set_style_radius(auth_submit, LV_RADIUS_CIRCLE, 0); auth_submit_label = text(auth_submit, "Sign In", lv_color_white(), false); lv_obj_center(auth_submit_label); lv_obj_add_event_cb(auth_submit, auth_button_event, LV_EVENT_CLICKED, NULL);
    auth_hint = text(auth_right, "", lv_color_hex(0xB13D3D), false); lv_obj_set_width(auth_hint, lv_pct(78)); lv_obj_set_style_text_align(auth_hint, LV_TEXT_ALIGN_CENTER, 0);
    auth_set_mode(false);
    lv_obj_t *header = lv_obj_create(screen); lv_obj_set_width(header, lv_pct(100)); lv_obj_set_height(header, 48); lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(header, 0, 0); lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_t *brand = text(header, "Michat", lv_color_hex(0x162842), true); lv_obj_align(brand, LV_ALIGN_LEFT_MID, 0, 0);
    status_line = text(header, "Connecting...", lv_color_hex(0x7888A3), false); lv_obj_set_width(status_line, lv_pct(55)); lv_label_set_long_mode(status_line, LV_LABEL_LONG_DOT); lv_obj_set_style_text_align(status_line, LV_TEXT_ALIGN_RIGHT, 0); lv_obj_align(status_line, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_t *body = lv_obj_create(screen); lv_obj_set_width(body, lv_pct(100)); lv_obj_set_flex_grow(body, 1); lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(body, 0, 0); lv_obj_set_style_pad_all(body, 0, 0); lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(body, 12, 0);
    chat_header = header; chat_body = body; lv_obj_add_flag(chat_header, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(chat_body, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *left = lv_obj_create(body); lv_obj_set_width(left, 220); lv_obj_set_height(left, lv_pct(100)); lv_obj_set_style_bg_color(left, lv_color_white(), 0); lv_obj_set_style_bg_opa(left, LV_OPA_COVER, 0); lv_obj_set_style_radius(left, 14, 0); lv_obj_set_style_pad_all(left, 12, 0); lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(left, 8, 0);
    text(left, "Online clients", lv_color_hex(0x162842), true);
    text(left, "Online now", lv_color_hex(0x162842), false);
    online_list = lv_obj_create(left); lv_obj_set_width(online_list, lv_pct(100)); lv_obj_set_height(online_list, 92); lv_obj_set_style_bg_color(online_list, lv_color_hex(0xF8FAFD), 0); lv_obj_set_style_bg_opa(online_list, LV_OPA_COVER, 0); lv_obj_set_style_border_width(online_list, 0, 0); lv_obj_set_style_pad_all(online_list, 6, 0); lv_obj_set_flex_flow(online_list, LV_FLEX_FLOW_COLUMN); lv_obj_set_scroll_dir(online_list, LV_DIR_VER);
    lv_obj_t *center = lv_obj_create(body); lv_obj_set_flex_grow(center, 1); lv_obj_set_height(center, lv_pct(100)); lv_obj_set_style_bg_color(center, lv_color_white(), 0); lv_obj_set_style_bg_opa(center, LV_OPA_COVER, 0); lv_obj_set_style_radius(center, 14, 0); lv_obj_set_style_pad_all(center, 14, 0); lv_obj_set_flex_flow(center, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(center, 8, 0);
    chat_title = text(center, "Conversation", lv_color_hex(0x162842), true); text(center, "Messages", lv_color_hex(0x7888A3), false);
    messages = lv_obj_create(center); lv_obj_set_width(messages, lv_pct(100)); lv_obj_set_flex_grow(messages, 1); lv_obj_set_style_bg_color(messages, lv_color_hex(0xF8FAFD), 0); lv_obj_set_style_bg_opa(messages, LV_OPA_COVER, 0); lv_obj_set_style_border_width(messages, 0, 0); lv_obj_set_style_pad_all(messages, 10, 0); lv_obj_set_flex_flow(messages, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(messages, 6, 0); lv_obj_set_scroll_dir(messages, LV_DIR_VER);
    lv_obj_t *composer_row = lv_obj_create(center); lv_obj_set_width(composer_row, lv_pct(100)); lv_obj_set_height(composer_row, 48); lv_obj_set_style_bg_opa(composer_row, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(composer_row, 0, 0); lv_obj_set_style_pad_all(composer_row, 0, 0); lv_obj_set_flex_flow(composer_row, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(composer_row, 8, 0);
    composer = lv_textarea_create(composer_row); lv_obj_set_flex_grow(composer, 1); lv_obj_set_height(composer, 44); lv_obj_set_style_bg_color(composer, lv_color_white(), 0); lv_obj_set_style_bg_opa(composer, LV_OPA_COVER, 0); lv_obj_set_style_border_width(composer, 1, 0); lv_obj_set_style_border_color(composer, lv_color_hex(0xD6DEEC), 0); lv_textarea_set_one_line(composer, true); lv_textarea_set_placeholder_text(composer, "Write a message...");
    lv_obj_t *send = lv_button_create(composer_row); lv_obj_set_size(send, 72, 44); lv_obj_set_style_bg_color(send, lv_color_hex(0x416FF0), 0); lv_obj_set_style_bg_opa(send, LV_OPA_COVER, 0); lv_obj_t *send_label = text(send, "Send", lv_color_white(), false); lv_obj_center(send_label); lv_obj_add_event_cb(send, send_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t *right = lv_obj_create(body); lv_obj_set_width(right, 178); lv_obj_set_height(right, lv_pct(100)); lv_obj_set_style_bg_color(right, lv_color_white(), 0); lv_obj_set_style_bg_opa(right, LV_OPA_COVER, 0); lv_obj_set_style_radius(right, 14, 0); lv_obj_set_style_pad_all(right, 14, 0); lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(right, 10, 0); lv_obj_set_scroll_dir(right, LV_DIR_VER);
    text(right, "Recipient", lv_color_hex(0x162842), true);
    target_ip_input = lv_textarea_create(right); lv_obj_set_width(target_ip_input, lv_pct(100)); lv_obj_set_height(target_ip_input, 36); lv_obj_set_style_bg_color(target_ip_input, lv_color_white(), 0); lv_obj_set_style_bg_opa(target_ip_input, LV_OPA_COVER, 0); lv_obj_set_style_border_width(target_ip_input, 1, 0); lv_obj_set_style_border_color(target_ip_input, lv_color_hex(0xD6DEEC), 0); lv_textarea_set_one_line(target_ip_input, true); lv_textarea_set_text(target_ip_input, "127.0.0.1");
    target_port_input = lv_textarea_create(right); lv_obj_set_width(target_port_input, lv_pct(100)); lv_obj_set_height(target_port_input, 36); lv_obj_set_style_bg_color(target_port_input, lv_color_white(), 0); lv_obj_set_style_bg_opa(target_port_input, LV_OPA_COVER, 0); lv_obj_set_style_border_width(target_port_input, 1, 0); lv_obj_set_style_border_color(target_port_input, lv_color_hex(0xD6DEEC), 0); lv_textarea_set_one_line(target_port_input, true); lv_textarea_set_text(target_port_input, "10086");
    lv_obj_t *refresh = lv_button_create(right); lv_obj_set_width(refresh, lv_pct(100)); lv_obj_set_height(refresh, 38); lv_obj_set_style_bg_color(refresh, lv_color_hex(0xEAF1FF), 0); lv_obj_set_style_bg_opa(refresh, LV_OPA_COVER, 0); lv_obj_t *refresh_label = text(refresh, "Refresh online", lv_color_hex(0x1E3763), false); lv_obj_center(refresh_label); lv_obj_add_event_cb(refresh, refresh_event, LV_EVENT_CLICKED, NULL);
    text(right, "Attachments", lv_color_hex(0x162842), false);
    file_path_input = lv_textarea_create(right); lv_obj_set_width(file_path_input, lv_pct(100)); lv_obj_set_height(file_path_input, 42); lv_obj_set_style_bg_color(file_path_input, lv_color_white(), 0); lv_obj_set_style_bg_opa(file_path_input, LV_OPA_COVER, 0); lv_obj_set_style_border_width(file_path_input, 1, 0); lv_obj_set_style_border_color(file_path_input, lv_color_hex(0xD6DEEC), 0); lv_textarea_set_one_line(file_path_input, true); lv_textarea_set_placeholder_text(file_path_input, "File path");
    file_type_input = lv_textarea_create(right); lv_obj_set_width(file_type_input, lv_pct(100)); lv_obj_set_height(file_type_input, 42); lv_obj_set_style_bg_color(file_type_input, lv_color_white(), 0); lv_obj_set_style_bg_opa(file_type_input, LV_OPA_COVER, 0); lv_obj_set_style_border_width(file_type_input, 1, 0); lv_obj_set_style_border_color(file_type_input, lv_color_hex(0xD6DEEC), 0); lv_textarea_set_one_line(file_type_input, true); lv_textarea_set_placeholder_text(file_type_input, "Type (jpg/png/txt)");
    lv_obj_t *send_file = lv_button_create(right); lv_obj_set_width(send_file, lv_pct(100)); lv_obj_set_height(send_file, 36); lv_obj_set_style_bg_color(send_file, lv_color_hex(0xEAF1FF), 0); lv_obj_set_style_bg_opa(send_file, LV_OPA_COVER, 0); lv_obj_t *send_file_label = text(send_file, "Send file", lv_color_hex(0x1E3763), false); lv_obj_center(send_file_label); lv_obj_add_event_cb(send_file, file_event, LV_EVENT_CLICKED, "file");
    lv_obj_t *open_emoji = lv_button_create(right); lv_obj_set_width(open_emoji, lv_pct(100)); lv_obj_set_height(open_emoji, 36); lv_obj_set_style_bg_color(open_emoji, lv_color_hex(0xEAF1FF), 0); lv_obj_set_style_bg_opa(open_emoji, LV_OPA_COVER, 0); lv_obj_t *open_emoji_label = text(open_emoji, "Emoji", lv_color_hex(0x1E3763), false); lv_obj_center(open_emoji_label); lv_obj_add_event_cb(open_emoji, emoji_open_event, LV_EVENT_CLICKED, NULL);
    configure_input(composer);
    configure_input(target_ip_input);
    configure_input(target_port_input);
    configure_input(file_path_input);
    configure_input(file_type_input);
    lv_textarea_set_accepted_chars(target_port_input, "0123456789");
    lv_textarea_set_max_length(target_port_input, 5);
    lv_textarea_set_max_length(composer, 600);
    lv_obj_add_event_cb(composer, send_event, LV_EVENT_READY, NULL);
    lv_obj_set_flex_grow(online_list, 1);
    lv_obj_set_width(right, 220);
    lv_obj_set_width(left, 200);
    session_host = center;
    inbox_messages = messages;
    inbox_composer = composer;
    inbox_title = chat_title;
    lv_label_set_text(inbox_title, "Shared inbox");
    lv_obj_t *inbox_button = lv_button_create(left);
    lv_obj_set_size(inbox_button, lv_pct(100), 36);
    lv_obj_center(text(inbox_button, "Shared inbox", lv_color_white(), false));
    lv_obj_add_event_cb(inbox_button, session_click, LV_EVENT_CLICKED, NULL);
    text(left, "Conversations", lv_color_hex(0x162842), false);
    session_list = lv_obj_create(left);
    lv_obj_set_width(session_list, lv_pct(100));
    lv_obj_set_flex_grow(session_list, 1);
    lv_obj_set_style_pad_all(session_list, 4, 0);
    lv_obj_set_flex_flow(session_list, LV_FLEX_FLOW_COLUMN);
    lv_group_focus_obj(composer);
}
