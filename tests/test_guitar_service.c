#include "guitar_stubs/platform.h"
#include <assert.h>
#include "../main/guitar_service.c"

const char editor_start[] = "test editor";
const char editor_end[] = "";
static char stored[4200], pending[4200];
static size_t stored_size, pending_size;
static esp_err_t init_result, write_result, commit_result;
static int apply_calls, writes, commits, closed, startup_fail, phase, stopped, destroyed, deinit, http_stopped;
static guitar_library_t active;
static unsigned activities;
static bool closing, stop_during_receive;
static int stop_error;
static bool activity(void) { ++activities; return !closing; }

const char *esp_err_to_name(esp_err_t e) { (void)e; return "test"; }
size_t heap_caps_get_largest_free_block(int cap) { (void)cap; return 50000; }
size_t esp_get_free_heap_size(void) { return 100000; }
esp_err_t nvs_flash_init(void) { return init_result; }
esp_err_t nvs_open(const char *space, int mode, nvs_handle_t *h) { (void)space; (void)mode; *h = 1; return ESP_OK; }
esp_err_t nvs_get_str(nvs_handle_t h, const char *k, char *out, size_t *n)
{ (void)h; (void)k; (void)out; (void)n; assert(!"Legacy AP password must not be loaded"); return ESP_FAIL; }
esp_err_t nvs_set_str(nvs_handle_t h, const char *k, const char *v)
{ (void)h; (void)k; (void)v; assert(!"Open AP must not persist a password"); return ESP_FAIL; }
esp_err_t nvs_get_blob(nvs_handle_t h, const char *k, void *out, size_t *n)
{
    (void)h; (void)k;
    if (!stored_size) return ESP_ERR_NVS_NOT_FOUND;
    if (*n < stored_size) { *n = stored_size; return ESP_ERR_NVS_INVALID_LENGTH; }
    memcpy(out, stored, stored_size); *n = stored_size; return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h, const char *k, const void *v, size_t n)
{
    (void)h; (void)k; ++writes;
    if (write_result) return write_result;
    assert(n <= sizeof(pending)); memcpy(pending, v, n); pending_size = n; return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h)
{
    (void)h; ++commits;
    if (commit_result) return commit_result;
    if (pending_size) { memcpy(stored, pending, pending_size); stored_size = pending_size; }
    return ESP_OK;
}
void nvs_close(nvs_handle_t h) { (void)h; ++closed; pending_size = 0; }
esp_err_t esp_read_mac(uint8_t *mac, int kind) { (void)kind; memset(mac, 0, 6); return ESP_OK; }
static esp_err_t stage(void) { return ++phase == startup_fail ? ESP_FAIL : ESP_OK; }
esp_err_t esp_netif_init(void) { return ESP_OK; }
esp_err_t esp_event_loop_create_default(void) { return ESP_OK; }
esp_netif_t *esp_netif_create_default_wifi_ap(void) { static esp_netif_t n; return &n; }
void esp_netif_destroy_default_wifi(esp_netif_t *ap) { (void)ap; ++destroyed; }
esp_err_t esp_wifi_init(const wifi_init_config_t *i) { assert(!i->nvs_enable); return stage(); }
esp_err_t esp_wifi_set_storage(int v) { (void)v; return stage(); }
esp_err_t esp_wifi_set_mode(int v) { (void)v; return stage(); }
esp_err_t esp_wifi_set_config(int v, const wifi_config_t *c)
{
    (void)v;
    assert(c->ap.max_connection == 1 && c->ap.authmode == WIFI_AUTH_OPEN);
    for (unsigned i = 0; i < sizeof(c->ap.password); ++i) assert(c->ap.password[i] == 0);
    assert(c->ap.ssid_len == strlen((const char *)c->ap.ssid));
    return stage();
}
esp_err_t esp_wifi_start(void) { return stage(); }
esp_err_t esp_wifi_stop(void) { ++stopped; return ESP_OK; }
esp_err_t esp_wifi_deinit(void) { ++deinit; return ESP_OK; }
esp_err_t httpd_start(httpd_handle_t *h, const httpd_config_t *c)
{ assert(c->stack_size >= 4096); esp_err_t r = stage(); if (!r) *h = (void *)1; return r; }
esp_err_t httpd_stop(httpd_handle_t s) { (void)s; ++http_stopped; return stop_error; }
esp_err_t httpd_register_uri_handler(httpd_handle_t s, const httpd_uri_t *h) { (void)s; (void)h; return stage(); }
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *r, const char *key, char *out, size_t n)
{
    const char *v = !strcmp(key, "Host") ? r->host : r->edit;
    if (!v || strlen(v) >= n) return ESP_FAIL;
    strcpy(out, v); return ESP_OK;
}
esp_err_t httpd_resp_set_status(httpd_req_t *r, const char *s) { snprintf(r->status, sizeof(r->status), "%s", s); return ESP_OK; }
esp_err_t httpd_resp_set_type(httpd_req_t *r, const char *s) { (void)r; (void)s; return ESP_OK; }
esp_err_t httpd_resp_set_hdr(httpd_req_t *r, const char *k, const char *s) { (void)r; (void)k; (void)s; return ESP_OK; }
esp_err_t httpd_resp_send(httpd_req_t *r, const char *s, ssize_t n)
{
    if (n < 0) n = strlen(s);
    assert((size_t)n < sizeof(r->response)); memcpy(r->response, s, n); r->response[n] = 0;
    return ESP_OK;
}
int httpd_req_recv(httpd_req_t *r, char *out, size_t n)
{
    if (stop_during_receive) atomic_store(&s_stopping, true);
    if (r->timeouts) { --r->timeouts; return HTTPD_SOCK_ERR_TIMEOUT; }
    if (r->disconnect) return 0;
    if (n > r->chunk) n = r->chunk;
    memcpy(out, r->body + r->offset, n); r->offset += n; return (int)n;
}
static esp_err_t apply(const guitar_library_t *lib, const char *body, size_t n)
{
    ++apply_calls;
    esp_err_t err = guitar_service_save(body, n);
    if (!err) active = *lib;
    return err;
}
static httpd_req_t request(const char *body)
{
    return (httpd_req_t){.content_len = strlen(body), .uri = "/api/score", .host = "192.168.4.1",
                        .edit = "1", .body = body, .chunk = 3};
}

int main(void)
{
    guitar_network_info_t info = {0};
    guitar_service_load(&active, &info);
    assert(info.storage_ok && active.song_count == 2);
    assert(writes == 0 && commits == 0); /* Loading never creates an AP key. */
    assert(guitar_service_start(apply, activity, &info) == ESP_OK);
    httpd_req_t touch = request(""); touch.uri = "/api/activity";
    assert(activity_post(&touch) == ESP_OK && activities == 1 && !strncmp(touch.status, "200", 3));
    touch = request(""); touch.edit = NULL;
    assert(activity_post(&touch) == ESP_OK && activities == 1 && !strncmp(touch.status, "403", 3));
    touch = request(""); closing = true;
    assert(activity_post(&touch) == ESP_FAIL && !strncmp(touch.status, "503", 3));
    closing = false;
    const char *good = "---\nnew song\nD\n95\n1(轻扫) 4 (分解 轻弹) 5 1\n";
    httpd_req_t req = request(good);
    req.uri = "/api/validate";
    assert(validate_post(&req) == ESP_OK && !strncmp(req.status, "200", 3));
    assert(apply_calls == 0 && writes == 0);
    req = request(good); req.uri = "/api/validate?preview=1";
    assert(validate_post(&req) == ESP_OK && apply_calls == 0 && writes == 0);
    req = request(good); req.timeouts = 2;
    assert(score_post(&req) == ESP_OK && !strncmp(req.status, "200", 3));
    assert(apply_calls == 1 && active.song_count == 1 && active.songs[0].bpm == 95);
    assert(!strcmp(s_text, good) && !strcmp(stored, good));
    guitar_service_load(&active, &info);
    assert(!strcmp(active.songs[0].title, "new song"));
    assert(active.bar_count == 4);
    assert(!strcmp(guitar_chord_comment(&active, active.bars[0]), "轻扫"));
    assert(!strcmp(guitar_chord_comment(&active, active.bars[1]), "分解 轻弹"));

    req = request("---\nwrong\nC\n80\nC(未闭合");
    assert(score_post(&req) == ESP_OK && !strncmp(req.status, "422", 3));
    assert(strstr(req.response, "注释") && apply_calls == 1 && !strcmp(s_text, good));

    req = request("---\nwrong\nC\n80\nH");
    assert(score_post(&req) == ESP_OK && !strncmp(req.status, "422", 3));
    assert(strstr(req.response, "5") && apply_calls == 1 && !strcmp(s_text, good));
    req = request(good); req.host = "hostile.example";
    assert(score_post(&req) == ESP_FAIL && !strncmp(req.status, "403", 3));
    req = request(good); req.edit = NULL;
    assert(score_post(&req) == ESP_FAIL && !strncmp(req.status, "403", 3));
    req = request(good); req.content_len = 4097;
    assert(score_post(&req) == ESP_FAIL && !strncmp(req.status, "413", 3) && req.offset == 0);
    req = request(good); req.timeouts = 3;
    assert(score_post(&req) == ESP_FAIL && !strncmp(req.status, "408", 3));
    req = request(good); req.disconnect = true;
    assert(score_post(&req) == ESP_FAIL && !strncmp(req.status, "408", 3));
    assert(apply_calls == 1 && !strcmp(s_text, good));
    stop_during_receive = true; req = request(good);
    assert(score_post(&req) == ESP_FAIL && !strncmp(req.status, "503", 3));
    assert(apply_calls == 1 && !strcmp(stored, good));
    stop_during_receive = false; atomic_store(&s_stopping, false);

    const char *newer = "---\nnewer\nC\n80\nC\n";
    int committed = commits;
    write_result = ESP_FAIL; req = request(newer);
    assert(score_post(&req) == ESP_OK && !strncmp(req.status, "503", 3));
    assert(commits == committed && !strcmp(s_text, good));
    write_result = ESP_OK; commit_result = ESP_FAIL; req = request(newer);
    assert(score_post(&req) == ESP_OK && !strncmp(req.status, "503", 3));
    assert(!strcmp(stored, good) && !strcmp(active.songs[0].title, "new song"));
    commit_result = ESP_OK;
    req = request(""); assert(score_get(&req) == ESP_OK && !strcmp(req.response, good));

    /* Full-size blob works beyond NVS's 4000-byte string limit. */
    char big[4097]; memset(big, ' ', 4096); big[4096] = 0;
    memcpy(big, newer, strlen(newer));
    req = request(big); req.chunk = 512;
    assert(score_post(&req) == ESP_OK && !strncmp(req.status, "200", 3));
    assert(stored_size == 4097);
    guitar_service_load(&active, &info);
    assert(info.storage_ok && !strcmp(active.songs[0].title, "newer"));
    stored[stored_size - 1] = 'X';
    guitar_service_load(&active, &info);
    assert(!info.storage_ok && active.song_count == 2 && stored[stored_size - 1] == 'X');
    init_result = ESP_FAIL;
    guitar_service_load(&active, &info);
    assert(!info.storage_ok && guitar_service_save(newer, strlen(newer)) == ESP_ERR_INVALID_STATE);

    stop_error = ESP_FAIL;
    assert(guitar_service_stop() == ESP_FAIL && s_server && s_ap);
    req = request(good);
    assert(score_post(&req) == ESP_FAIL && !strncmp(req.status, "503", 3));
    stop_error = ESP_OK;
    assert(guitar_service_stop() == ESP_OK && !s_server && !s_ap);
    assert(guitar_service_stop() == ESP_OK);
    for (int fail = 1; fail <= 11; ++fail) {
        startup_fail = fail; phase = stopped = destroyed = deinit = http_stopped = 0;
        assert(guitar_service_start(apply, activity, &info) != ESP_OK);
        assert(destroyed == 1 && deinit == (fail > 1) && stopped == (fail > 5) && http_stopped == (fail > 6));
    }
    assert(closed > 0);
    puts("Guitar HTTP/NVS: PASS (chunking, timeout, rejection, atomic model, reload, startup rollback)");
    return 0;
}
