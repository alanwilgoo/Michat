cmake_minimum_required(VERSION 3.16)
project(lvgl_chat_sim C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MICHAT_BUILD_DESKTOP "Build the existing SDL desktop client" ON)

if(NOT MICHAT_BUILD_DESKTOP)
    message(FATAL_ERROR "The desktop client target must be enabled")
endif()

file(GLOB LVGL_C CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/core/*.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/lv_init.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/display/*.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/draw/*.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/draw/sw/*.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/draw/sw/blend/*.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/drivers/sdl/*.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/font/lv_font.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/font/lv_font_fmt_txt.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/font/lv_font_montserrat_14.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/font/lv_font_montserrat_22.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/indev/*.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/libs/bin_decoder/*.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/libs/fsdrv/lv_fs_stdio.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/libs/lodepng/*.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/libs/bmp/lv_bmp.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/libs/tjpgd/*.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/layouts/*.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/layouts/flex/*.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/layouts/grid/*.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/misc/*.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/misc/cache/*.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/osal/lv_os.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/osal/lv_os_none.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/stdlib/builtin/*.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/stdlib/lv_mem.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/themes/lv_theme.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/themes/default/*.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/tick/*.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/widgets/button/*.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/widgets/buttonmatrix/*.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/widgets/image/*.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/widgets/label/*.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/widgets/keyboard/*.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/widgets/span/*.c
    ${CMAKE_SOURCE_DIR}/third_party/lvgl/src/widgets/textarea/*.c)

if(MICHAT_BUILD_DESKTOP)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    add_library(lvgl STATIC ${LVGL_C})
    target_include_directories(lvgl SYSTEM PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/third_party/lvgl)
    target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE LV_LVGL_H_INCLUDE_SIMPLE)
    add_executable(chat_sim main.c frontend.c backend.c app_events.c)
    target_include_directories(chat_sim PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/third_party/lvgl)
    target_compile_definitions(chat_sim PRIVATE MICHAT_DESKTOP_AVATARS=1 MICHAT_EMOJI_DIR_DEFAULT=\"${CMAKE_SOURCE_DIR}/assets/emojis\" MICHAT_RECEIVE_DIR_DEFAULT=\"${CMAKE_BINARY_DIR}/runtime/received\" MICHAT_PROJECT_DIR=\"${CMAKE_SOURCE_DIR}\")
    target_link_libraries(chat_sim PRIVATE lvgl PkgConfig::SDL2 pthread m)
    set_target_properties(chat_sim PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()
