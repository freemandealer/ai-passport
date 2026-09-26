cmake_minimum_required(VERSION 3.16)
project(frog_ui_host C)
set(CMAKE_C_STANDARD 11)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LV_BUILD_SET_CONFIG_OPTS OFF CACHE BOOL "" FORCE)
set(REPO "${CMAKE_CURRENT_SOURCE_DIR}/../..")
add_subdirectory("${REPO}/managed_components/lvgl__lvgl" lvgl)
add_executable(frog_ui_host test_frog_ui.c "${REPO}/main/frog_ui.c"
    "${REPO}/main/frog_model.c"
    "${REPO}/assets/fonts/frog_font_14.c" "${REPO}/assets/fonts/frog_font_20.c"
    "${REPO}/components/bsp/src/bsp_display_rounding.c")
target_include_directories(frog_ui_host PRIVATE "${REPO}/main" "${REPO}/components/bsp/src")
target_compile_options(frog_ui_host PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(frog_ui_host PRIVATE lvgl m)
