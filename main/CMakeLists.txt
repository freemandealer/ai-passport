idf_component_register(
    SRCS "main.c"
         "pitch_detector.c"
         "key_analyzer.c"
         "key_control.c"
         "key_audio.c"
         "key_ui.c"
         "../assets/fonts/key_font_14.c"
         "../assets/fonts/key_font_20.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_timer
)
