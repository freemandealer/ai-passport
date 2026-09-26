#include "guitar_service.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "guitar_web";
static char s_text[GUITAR_TEXT_MAX + 1];
static bool s_nvs_ready;
static guitar_apply_fn s_apply;
extern const char editor_start[] asm("_binary_guitar_editor_html_start");
extern const char editor_end[] asm("_binary_guitar_editor_html_end");

void guitar_service_load(guitar_library_t *lib, guitar_network_info_t *info)
{
    snprintf(s_text, sizeof(s_text), "%s", GUITAR_DEFAULT_SCORE);
    guitar_parse(s_text, strlen(s_text), lib, NULL);
    esp_err_t err = nvs_flash_init();
    s_nvs_ready = err == ESP_OK;
    info->storage_ok = s_nvs_ready;
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(info->ssid, sizeof(info->ssid), "Guitar-%02X%02X", mac[4], mac[5]);
    if (!s_nvs_ready) {
        ESP_LOGE(TAG, "Storage unavailable (%s), existing NVS preserved", esp_err_to_name(err));
        return;
    }
    nvs_handle_t handle;
    err = nvs_open("guitar", NVS_READWRITE, &handle);
    if (err != ESP_OK) { info->storage_ok = false; return; }
    /* The open hotspot ignores legacy ap_key values without erasing NVS. */
    size_t size = sizeof(s_text);
    err = nvs_get_blob(handle, "score_v1", s_text, &size);
    bool valid = err == ESP_OK && size > 1 && size <= sizeof(s_text) &&
                 s_text[size - 1] == 0 && guitar_parse(s_text, size - 1, lib, NULL);
    if (err != ESP_ERR_NVS_NOT_FOUND && !valid) {
        info->storage_ok = false;
        ESP_LOGW(TAG, "Saved score invalid; preserving stored value, loading practice score");
    }
    if (!valid) {
        snprintf(s_text, sizeof(s_text), "%s", GUITAR_DEFAULT_SCORE);
        guitar_parse(s_text, strlen(s_text), lib, NULL);
    }
    nvs_close(handle);
}

esp_err_t guitar_service_save(const char *text, size_t length)
{
    if (!s_nvs_ready) return ESP_ERR_INVALID_STATE;
    if (!length || length > GUITAR_TEXT_MAX || text[length] != 0) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle;
    esp_err_t err = nvs_open("guitar", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(handle, "score_v1", text, length + 1);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    if (err == ESP_OK) memcpy(s_text, text, length + 1);
    return err;
}

static bool local_host(httpd_req_t *req)
{
    char host[32];
    return httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) == ESP_OK &&
        (!strcmp(host, "192.168.4.1") || !strcmp(host, "192.168.4.1:80"));
}

static esp_err_t reply(httpd_req_t *req, const char *status, const char *message)
{
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    return httpd_resp_send(req, message, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t root_get(httpd_req_t *req)
{
    if (!local_host(req)) return reply(req, "403 Forbidden", "请通过 192.168.4.1 打开编辑器");
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Content-Security-Policy", "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    return httpd_resp_send(req, editor_start, editor_end - editor_start - 1);
}

static esp_err_t score_get(httpd_req_t *req)
{
    if (!local_host(req)) return reply(req, "403 Forbidden", "无效访问地址");
    return reply(req, "200 OK", s_text);
}

static const char *parse_message(guitar_parse_code_t code)
{
    static const char *const errors[] = {
        "曲谱有效", "曲谱必须为 1 至 4096 字节", "每首歌必须以单独一行 --- 开始",
        "歌名限 24 字，支持基本汉字、英文及常见中文标点", "调号无效，例如 C、F#、Bb、Am",
        "速度需为 30 至 120 的整数", "和弦或级数无效，请查看下方语法说明",
        "歌曲标题、调号、速度和至少一个和弦都必须填写", "最多 16 首歌",
        "总计最多 512 个四拍小节", "前三行依次填写歌名、调号、速度，不能空行",
        "注释需紧随和弦，如 C(轻扫)，每小节一条、最多24字；括号需配对，不能嵌套或跨行"
    };
    return errors[code];
}

static esp_err_t receive_score(httpd_req_t *req, bool save)
{
    char header[16];
    if (!local_host(req) || httpd_req_get_hdr_value_str(req, "X-Guitar-Edit", header, sizeof(header)) != ESP_OK || strcmp(header, "1")) {
        reply(req, "403 Forbidden", "请从设备编辑器保存曲谱");
        return ESP_FAIL;
    }
    if (req->content_len == 0 || req->content_len > GUITAR_TEXT_MAX) {
        reply(req, "413 Content Too Large", "曲谱大小限 1 至 4096 字节");
        return ESP_FAIL;
    }
    char *body = malloc(req->content_len + 1);
    guitar_library_t *candidate = malloc(sizeof(*candidate));
    if (!body || !candidate) {
        free(body); free(candidate);
        reply(req, "503 Service Unavailable", "内存不足，请稍后重试");
        return ESP_FAIL;
    }
    size_t received = 0;
    unsigned timeouts = 0;
    while (received < req->content_len) {
        int n = httpd_req_recv(req, body + received, req->content_len - received);
        if (n == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts <= 2) continue;
        if (n <= 0) {
            free(body); free(candidate);
            reply(req, "408 Request Timeout", "上传中断，原曲谱未修改");
            return ESP_FAIL;
        }
        received += n;
    }
    body[received] = 0;
    guitar_error_t error;
    esp_err_t result;
    if (!guitar_parse(body, received, candidate, &error)) {
        char message[240];
        snprintf(message, sizeof(message), "第 %u 行：%s", error.line, parse_message(error.code));
        result = reply(req, "422 Unprocessable Content", message);
    } else if (!save) {
        char message[128];
        snprintf(message, sizeof(message), "校验通过：%u 首歌，%u 个四拍小节", candidate->song_count, candidate->bar_count);
        result = reply(req, "200 OK", message);
    } else {
        esp_err_t err = s_apply(candidate, body, received);
        if (err == ESP_OK) result = reply(req, "200 OK", "已保存到设备，播放已暂停。请选择歌曲后开始。");
        else result = reply(req, "503 Service Unavailable", "保存失败，原曲谱未替换，请稍后重试");
    }
    free(body); free(candidate);
    return result;
}

static esp_err_t score_post(httpd_req_t *req) { return receive_score(req, true); }
static esp_err_t validate_post(httpd_req_t *req) { return receive_score(req, false); }

esp_err_t guitar_service_start(guitar_apply_fn apply, const guitar_network_info_t *info)
{
    s_apply = apply;
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) return err;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    esp_netif_t *ap = esp_netif_create_default_wifi_ap();
    if (!ap) return ESP_ERR_NO_MEM;
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    init.nvs_enable = false;
    bool wifi_ready = false, wifi_started = false;
    httpd_handle_t server = NULL;
    err = esp_wifi_init(&init);
    if (err != ESP_OK) goto cleanup;
    wifi_ready = true;
    wifi_config_t config = {0};
    snprintf((char *)config.ap.ssid, sizeof(config.ap.ssid), "%s", info->ssid);
    config.ap.ssid_len = strlen(info->ssid);
    config.ap.channel = 1;
    config.ap.max_connection = 1;
    config.ap.authmode = WIFI_AUTH_OPEN;
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err == ESP_OK) err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err == ESP_OK) err = esp_wifi_set_config(WIFI_IF_AP, &config);
    if (err == ESP_OK) err = esp_wifi_start();
    if (err != ESP_OK) goto cleanup;
    wifi_started = true;
    httpd_config_t http = HTTPD_DEFAULT_CONFIG();
    http.stack_size = 6144;
    http.max_open_sockets = 3;
    http.lru_purge_enable = true;
    http.recv_wait_timeout = 3;
    http.send_wait_timeout = 3;
    err = httpd_start(&server, &http);
    if (err != ESP_OK) goto cleanup;
    const httpd_uri_t handlers[] = {
        {.uri = "/", .method = HTTP_GET, .handler = root_get},
        {.uri = "/api/score", .method = HTTP_GET, .handler = score_get},
        {.uri = "/api/score", .method = HTTP_POST, .handler = score_post},
        {.uri = "/api/validate", .method = HTTP_POST, .handler = validate_post}
    };
    for (unsigned i = 0; i < sizeof(handlers) / sizeof(handlers[0]); ++i) {
        err = httpd_register_uri_handler(server, &handlers[i]);
        if (err != ESP_OK) goto cleanup;
    }
    ESP_LOGI(TAG, "Editor ready; heap=%lu largest=%lu", (unsigned long)esp_get_free_heap_size(),
             (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    return ESP_OK;
cleanup:
    if (server) httpd_stop(server);
    if (wifi_started) esp_wifi_stop();
    if (wifi_ready) esp_wifi_deinit();
    esp_netif_destroy_default_wifi(ap);
    ESP_LOGE(TAG, "Editor start failed: %s; local score remains usable", esp_err_to_name(err));
    return err;
}
