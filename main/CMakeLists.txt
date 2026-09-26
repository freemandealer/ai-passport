idf_component_register(
    SRCS "main.c" "frog_model.c" "frog_ui.c"
         "../assets/fonts/frog_font_14.c"
         "../assets/fonts/frog_font_20.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_hw_support esp_timer
)
