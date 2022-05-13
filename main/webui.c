/**
 * @file webui.c
 *
 * Copyright (C) 2021 Christian Spielberger
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>
#include "log.h"
#include "webui.h"

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

static const char *TAG = "webui";

#define HTML_HEADER \
"<!DOCTYPE html>\n" \
"<html>\n" \
"<header>" \
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">" \
"</header>" \
"<body>\n" \
"\n"

#define HTML_FOOTER \
"\n" \
"</body>\n" \
"</html>\n"

#define HTML_INPUT_TIME \
"<label for=\"stime\">Start time:</label><br>\n" \
"<input type=\"time\" id=\"stime\" name=\"stime\" min=\"06:00\" max=\"23:00\"" \
" value=\"%s\" step=\"60\"><br>\n" \
"  <br>\n" \
"<label for=\"duration\">Duration</label><br>\n" \
"<input type=\"range\" id=\"duration\" name=\"duration\" min=\"1\" max=\"8\"" \
" value=\"%d\"><br>\n"

#define HTML_FORM1 \
"<h2>Pool Saltwater System</h2>" \
"" \
"<form action=\"\" method=\"post\">\n"


#define HTML_FORM2 \
"  <input type=\"submit\" value=\"Ok\"><br>\n" \
"  <br>\n" \
"  <br>\n" \
"  <br>\n" \
"  <input type=\"radio\" id=\"nocommand\" name=\"command\" checked=\"checked\">\n" \
"  <label for=\"nocommand\">-- none --</label><br>\n" \
"  <input type=\"radio\" id=\"upgrade\" name=\"command\" value=\"upgrade\">\n" \
"  <label for=\"upgrade\">Upgrade</label><br>\n" \
"  <input type=\"radio\" id=\"reboot\" name=\"command\" value=\"reboot\">\n" \
"  <label for=\"reboot\">Reboot</label><br>\n" \
"  <input type=\"radio\" id=\"reset\" name=\"command\" value=\"reset\">\n" \
"  <label for=\"reset\">Reset</label><br>\n" \
"  <br>\n" \
"  <br>\n" \


#define HTML_FORM3 \
"  <input type=\"radio\" id=\"noforce\" name=\"force\" value=\"none\"%s>\n"\
"  <label for=\"noforce\">-- none --</label><br>\n"\
"  <input type=\"radio\" id=\"forceon\" name=\"force\" value=\"on\"%s>\n"\
"  <label for=\"forceon\">force on</label><br>\n"\
"  <input type=\"radio\" id=\"forceoff\" name=\"force\" value=\"off\"%s>\n"\
"  <label for=\"forceoff\">force off</label><br>\n"\
"</form>\n"


#define HTML_LOG \
"<p></p>" \
"<p>Log</p>" \

#define BUF_SIZE 512

enum force_run {
    FORCE_NONE,
    FORCE_ON,
    FORCE_OFF
};

struct webui {
    int hh;
    int mm;
    int duration;
    bool upgrade;
    bool reboot;
    bool reset;
    int dcnt;
    enum force_run force;
};

static struct webui d;

static time_t current_time(void);


static void init_hh_mm(void)
{
    time_t cur = current_time();
    struct tm tm;

    localtime_r(&cur, &tm);
    d.hh = tm.tm_hour;
    d.mm = tm.tm_min;
}


static esp_err_t send_html(httpd_req_t *req)
{
    char  buf[BUF_SIZE];
    char stime[10];
    const char *logl;
    const char *checked = " checked=\"checked\"";

    if (!d.hh && !d.mm)
        init_hh_mm();

    snprintf(stime, sizeof(stime), "%02d:%02d", d.hh, d.mm);
    printf("%s:%d HUUUUUU %s\n", __func__, __LINE__, stime);
    snprintf(buf, sizeof(buf), HTML_INPUT_TIME, stime, d.duration);
    httpd_resp_send_chunk(req, HTML_HEADER HTML_FORM1, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req,  buf, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, HTML_HEADER HTML_FORM2, HTTPD_RESP_USE_STRLEN);

    snprintf(buf, sizeof(buf), HTML_FORM3,
            d.force == FORCE_NONE ? checked : "",
            d.force == FORCE_ON   ? checked : "",
            d.force == FORCE_OFF  ? checked : "");

    httpd_resp_send_chunk(req,  buf, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, HTML_LOG, HTTPD_RESP_USE_STRLEN);

    logl = logr();
    while (logl) {
        snprintf(buf, sizeof(buf), "<p>%s</p>", logl);
        httpd_resp_send_chunk(req,  buf, HTTPD_RESP_USE_STRLEN);
        logl = logr();
    }

    if (snprintf(buf, sizeof(buf), "<p>%s ...</p>", d.upgrade ? "Upgrading..." :
                webui_check_time() ? "Running" : "Sleeping"))
        httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);


    if ((d.reboot || d.reset ) && snprintf(buf, sizeof(buf), "<p>%s ...</p>",
                d.reboot ? "Reboot" :
                d.reset ? "Reset" : ""))
        httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);

    httpd_resp_send_chunk(req, HTML_FOOTER, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}


/* HTTP GET handler */
static esp_err_t handle_get(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;

    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[32];
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "query1", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query1=%s", param);
            }
            if (httpd_query_key_value(buf, "query3", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query3=%s", param);
            }
            if (httpd_query_key_value(buf, "query2", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => query2=%s", param);
            }
        }
        free(buf);
    }

    send_html(req);
    return ESP_OK;
}


static const httpd_uri_t get_handler = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = handle_get,
    .user_ctx  = NULL
};


static int body_value(char *val, size_t vlen, const char *body, const char *key)
{
    size_t klen;
    const char *p;
    const char *e;

    if (!val || !vlen || !body || !key || vlen != 10) {
        return EINVAL;
    }

    klen = strlen(key);
    if (!klen || klen > 20) {
        return EINVAL;
    }

    p = strstr(body, key);
    if (!p)
        return ENODATA;

    p += klen + 1;
    e  = strchr(p, '&');
    if (!e)
        e = p + strlen(p);

    if (e <= p)
        return ENODATA;

    strncpy(val, p, MIN(vlen, e-p));
    strcat(val, "");
    return 0;
}


static time_t current_time(void)
{
    time_t now;
    time(&now);
    return now;
}


static int convert_time(const char *stime)
{
    struct tm tm;
    time_t time;

    if (!stime)
	return EINVAL;

    time = current_time();
    localtime_r(&time, &tm);
    if (sscanf(stime, "%d%%3A%d", &tm.tm_hour, &tm.tm_min) <= 0)
	return EINVAL;

    d.hh = tm.tm_hour;
    d.mm = tm.tm_min;
    return 0;
}


static int open_nvs(nvs_handle_t *nvs)
{
    int err;

    err = nvs_open("storage", NVS_READWRITE, nvs);
    if (err)
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));

    return err;
}


static void write_settings()
{
    nvs_handle_t nvs;
    int err;

    err = open_nvs(&nvs);
    if (err)
        return;

    err  = nvs_set_i32(nvs, "time_hh", d.hh);
    err |= nvs_set_i32(nvs, "time_mm", d.mm);
    err |= nvs_set_i32(nvs, "duration", d.duration);
    err |= nvs_commit(nvs);
    nvs_close(nvs);
    if (err) {
        printf("Error (%s) could not update NVS.\n", esp_err_to_name(err));
    }
}


/* An HTTP POST handler */
static esp_err_t handle_post(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;
    char stime[10] = {0};
    char dur[10] = {0};
    int err;

    while (remaining > 0) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf,
                        MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            ESP_LOGI(TAG, "%s failed ret=%d", __FUNCTION__, ret);
            return ESP_FAIL;
        }

        /* Send back the same data */
        remaining -= ret;

        /* Log data received */
        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(TAG, "%.*s", ret, buf);
        ESP_LOGI(TAG, "====================================");
        buf[sizeof(buf)-1]=0;
        if (strstr(buf, "command=upgrade")) {
            d.upgrade = true;
        }
        else if (strstr(buf, "command=reboot")) {
            ESP_LOGI(TAG, "=========== Reboot ==========");
            esp_restart();
        }
        else if (strstr(buf, "command=reset")) {
            ESP_LOGI(TAG, "=========== Reset ==========");
            init_hh_mm();
            d.duration = 3;
            ESP_ERROR_CHECK(nvs_flash_erase());
        }
        else if (strstr(buf, "force=on")) {
            ESP_LOGI(TAG, "=========== Force on ==========");
            d.force = FORCE_ON;
        }
        else if (strstr(buf, "force=off")) {
            ESP_LOGI(TAG, "=========== Force off ==========");
            d.force = FORCE_OFF;
        }
        else {
            if (body_value(stime, sizeof(stime), buf, "stime")) {
                logw("Could not parse stime");
            }
            else if (body_value(dur, sizeof(dur), buf, "duration")) {
                logw("Could not parse duration");
            } else {
                logw("stime=%s dur=%s", stime, dur);
                d.duration = atoi(dur);
                err = convert_time(stime);
                if (!err)
                    write_settings();
            }
        }
    }

    // Send response
    send_html(req);
    return ESP_OK;
}

static const httpd_uri_t post_handler = {
    .uri       = "/",
    .method    = HTTP_POST,
    .handler   = handle_post,
    .user_ctx  = NULL
};


static void read_settings()
{
    nvs_handle_t nvs;
    int err;

    err = open_nvs(&nvs);
    if (err) {
        init_hh_mm();
        d.duration = 3;
        return;
    }

    err  = nvs_get_i32(nvs, "time_hh", &d.hh);
    err |= nvs_get_i32(nvs, "time_mm", &d.mm);
    if (err) {
        printf("Error (%s) time not set.\n", esp_err_to_name(err));
        init_hh_mm();
    }

    err = nvs_get_i32(nvs, "duration", &d.duration);
    if (err) {
        printf("Error (%s) duration not set.\n", esp_err_to_name(err));
        d.duration = 3;
    }

    logw("%s read %02d:%02d duration %d", __FUNCTION__, d.hh, d.mm,
         d.duration);
    nvs_close(nvs);
}


httpd_handle_t start_webserver(void)
{
    memset(&d, 0, sizeof(d));
    printf("Opening Non-Volatile Storage (NVS) handle... \n");

    read_settings();
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &get_handler);
        httpd_register_uri_handler(server, &post_handler);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}


void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}


void webui_disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
}


void webui_connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}


bool webui_upgrade(void)
{
    bool upgrade = d.upgrade;
    d.upgrade = false;
    return upgrade;
}


bool webui_check_time()
{
    time_t timec;
    time_t times;
    struct tm tm;

    if (d.force == FORCE_OFF)
        return false;

    if (d.force == FORCE_ON)
        return true;

    timec = current_time();
    localtime_r(&timec, &tm);
    tm.tm_hour = d.hh;
    tm.tm_min  = d.mm;

    times = mktime(&tm);
    return times <= timec && timec <= times + d.duration * 3600;
}
