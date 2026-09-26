cmake_minimum_required(VERSION 3.16)
project(guitar_ui_host C)
set(CMAKE_C_STANDARD 11)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LV_BUILD_SET_CONFIG_OPTS OFF CACHE BOOL "" FORCE)
set(REPO "${CMAKE_CURRENT_SOURCE_DIR}/../..")
add_subdirectory("${REPO}/managed_components/lvgl__lvgl" lvgl)
add_executable(guitar_ui_host test_guitar_ui.c "${REPO}/main/guitar_ui.c"
    "${REPO}/main/guitar_score.c" "${REPO}/main/guitar_shapes.c"
    "${REPO}/main/guitar_player.c" "${REPO}/assets/fonts/guitar_font_16.c" "${REPO}/assets/fonts/guitar_font_24.c"
    "${REPO}/components/bsp/src/bsp_display_rounding.c")
target_include_directories(guitar_ui_host PRIVATE "${REPO}/main" "${REPO}/components/bsp/src")
target_compile_definitions(guitar_ui_host PRIVATE GUITAR_UI_TEST)
target_compile_options(guitar_ui_host PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(guitar_ui_host PRIVATE lvgl m)
