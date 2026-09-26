cmake_minimum_required(VERSION 3.16)
project(key_ui_host C)
set(CMAKE_C_STANDARD 11)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LV_BUILD_SET_CONFIG_OPTS OFF CACHE BOOL "" FORCE)
set(REPO "${CMAKE_CURRENT_SOURCE_DIR}/../..")
add_subdirectory("${REPO}/managed_components/lvgl__lvgl" lvgl)
add_executable(key_ui_host test_key_ui.c "${REPO}/main/key_ui.c"
    "${REPO}/main/pitch_detector.c" "${REPO}/main/key_analyzer.c"
    "${REPO}/assets/fonts/key_font_14.c" "${REPO}/assets/fonts/key_font_20.c"
    "${REPO}/components/bsp/src/bsp_display_rounding.c")
target_include_directories(key_ui_host PRIVATE "${REPO}/main" "${REPO}/components/bsp/src")
target_compile_options(key_ui_host PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(key_ui_host PRIVATE lvgl m)
