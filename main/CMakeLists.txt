idf_component_register(
    SRCS "main.c"
         "guitar_score.c"
         "guitar_shapes.c"
         "guitar_player.c"
         "guitar_metronome.c"
         "guitar_audio.c"
         "guitar_battery.c"
         "guitar_ui.c"
         "guitar_service.c"
         "../assets/fonts/guitar_font_16.c"
         "../assets/fonts/guitar_font_24.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_event esp_hw_support esp_http_server esp_netif esp_timer esp_wifi nvs_flash
    EMBED_TXTFILES "guitar_editor.html"
)
