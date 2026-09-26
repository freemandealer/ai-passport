#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>

typedef int esp_err_t;
enum { ESP_OK, ESP_FAIL, ESP_ERR_INVALID_STATE, ESP_ERR_INVALID_ARG, ESP_ERR_NO_MEM,
       ESP_ERR_NVS_NOT_FOUND, ESP_ERR_TIMEOUT, ESP_ERR_NVS_INVALID_LENGTH };
const char *esp_err_to_name(esp_err_t err);
#define ESP_LOGE(tag, ...) ((void)(tag))
#define ESP_LOGW(tag, ...) ((void)(tag))
#define ESP_LOGI(tag, ...) ((void)(tag))
#define MALLOC_CAP_8BIT 1
size_t heap_caps_get_largest_free_block(int cap);
size_t esp_get_free_heap_size(void);
esp_err_t esp_event_loop_create_default(void);
typedef struct { int placeholder; } esp_netif_t;
esp_err_t esp_netif_init(void);
esp_netif_t *esp_netif_create_default_wifi_ap(void);
void esp_netif_destroy_default_wifi(esp_netif_t *ap);
#define ESP_MAC_WIFI_SOFTAP 1
esp_err_t esp_read_mac(uint8_t *mac, int kind);
void esp_fill_random(void *buffer, size_t length);

typedef int nvs_handle_t;
#define NVS_READWRITE 1
esp_err_t nvs_flash_init(void);
esp_err_t nvs_open(const char *space, int mode, nvs_handle_t *handle);
esp_err_t nvs_get_str(nvs_handle_t handle, const char *key, char *out, size_t *size);
esp_err_t nvs_set_str(nvs_handle_t handle, const char *key, const char *value);
esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *out, size_t *size);
esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t size);
esp_err_t nvs_commit(nvs_handle_t handle);
void nvs_close(nvs_handle_t handle);

typedef struct { bool nvs_enable; } wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() ((wifi_init_config_t){.nvs_enable=true})
typedef struct {
    struct {
        uint8_t ssid[32], password[64];
        uint8_t ssid_len, channel, max_connection;
        int authmode;
        struct { bool capable; } pmf_cfg;
    } ap;
} wifi_config_t;
#define WIFI_STORAGE_RAM 0
#define WIFI_MODE_AP 1
#define WIFI_IF_AP 1
#define WIFI_AUTH_OPEN 0
esp_err_t esp_wifi_init(const wifi_init_config_t *init);
esp_err_t esp_wifi_set_storage(int storage);
esp_err_t esp_wifi_set_mode(int mode);
esp_err_t esp_wifi_set_config(int interface, const wifi_config_t *config);
esp_err_t esp_wifi_start(void);
esp_err_t esp_wifi_stop(void);
esp_err_t esp_wifi_deinit(void);

typedef struct {
    size_t content_len;
    const char *uri;
    const char *host, *edit, *body;
    size_t offset, chunk;
    int timeouts;
    bool disconnect;
    char status[64], response[4200];
} httpd_req_t;
typedef void *httpd_handle_t;
typedef struct {
    int stack_size, max_open_sockets, recv_wait_timeout, send_wait_timeout;
    bool lru_purge_enable;
} httpd_config_t;
#define HTTPD_DEFAULT_CONFIG() ((httpd_config_t){0})
#define HTTP_GET 1
#define HTTP_POST 2
#define HTTPD_RESP_USE_STRLEN -1
#define HTTPD_SOCK_ERR_TIMEOUT -3
typedef struct { const char *uri; int method; esp_err_t (*handler)(httpd_req_t *); } httpd_uri_t;
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *req, const char *key, char *value, size_t size);
esp_err_t httpd_resp_set_status(httpd_req_t *req, const char *status);
esp_err_t httpd_resp_set_type(httpd_req_t *req, const char *type);
esp_err_t httpd_resp_set_hdr(httpd_req_t *req, const char *key, const char *value);
esp_err_t httpd_resp_send(httpd_req_t *req, const char *message, ssize_t length);
int httpd_req_recv(httpd_req_t *req, char *buffer, size_t length);
esp_err_t httpd_start(httpd_handle_t *server, const httpd_config_t *config);
esp_err_t httpd_stop(httpd_handle_t server);
esp_err_t httpd_register_uri_handler(httpd_handle_t server, const httpd_uri_t *handler);
