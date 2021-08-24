/**
 * @file webui.c
 *
 * Copyright (C) 2021 Christian Spielberger
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <esp_log.h>
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
"<input type=\"time\" id=\"stime\" name=\"stime\" min=\"08:00\" max=\"19:00\"" \
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
"  <input type=\"radio\" id=\"no\" name=\"command\" checked=\"checked\">\n" \
"  <label for=\"upgrade\">-- none --</label><br>\n" \
"  <input type=\"radio\" id=\"upgrade\" name=\"command\" value=\"upgrade\">\n" \
"  <label for=\"upgrade\">Upgrade</label><br>\n" \
"  <input type=\"radio\" id=\"reboot\" name=\"command\" value=\"reboot\">\n" \
"  <label for=\"reboot\">Reboot</label><br>\n" \
"  <input type=\"radio\" id=\"reset\" name=\"command\" value=\"reset\">\n" \
"  <label for=\"reboot\">Reset</label><br>\n" \
"</form>\n" \

#define HTML_LOG \
"<p></p>" \
"<p>Log</p>" \


struct webui {
    time_t times;
    int duration;
    bool upgrade;
    bool reboot;
    bool reset;
    int dcnt;
};

static struct webui d;

static time_t current_time(void);


static esp_err_t send_html(httpd_req_t *req)
{
    char*  buf;
    size_t len = strlen(HTML_INPUT_TIME) + 20;
    char stime[10];
    size_t i;
    const char *logl;

    buf = malloc(len);
    if (!buf)
        return ESP_ERR_NO_MEM;

    if (!d.times)
            d.times = current_time();

    strftime(stime, sizeof stime, "%H:%M", localtime(&d.times));
    snprintf(buf, len, HTML_INPUT_TIME, stime, d.duration);
    httpd_resp_send_chunk(req, HTML_HEADER HTML_FORM1, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req,  buf, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, HTML_HEADER HTML_FORM2, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, HTML_LOG, HTTPD_RESP_USE_STRLEN);

    free(buf);
    while ((logl = logr())) {
        len = strlen(logl) + 10;
        buf = malloc(len);
        snprintf(buf, len, "<p>%s</p>", logl);
        httpd_resp_send_chunk(req,  buf, HTTPD_RESP_USE_STRLEN);
        free(buf);
    }

    len = 64;
    buf = malloc(len);
    if (snprintf(buf, len, "<p>%s ...</p>", webui_check_time() ?
                "Running" : "Sleeping"))
        httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);


    if ((d.reboot || d.reset ) && snprintf(buf, len, "<p>%s ...</p>",
                d.reboot ? "Reboot" :
                d.reset ? "Reset" : ""))
        httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);

    httpd_resp_send_chunk(req, HTML_FOOTER, HTTPD_RESP_USE_STRLEN);

    free(buf);
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
    size_t klen = strlen(key);
    const char *p;
    const char *e;
    size_t l = vlen;

    p = strstr(body, key);
    if (!p)
        return ENODATA;

    p += klen + 1;
    e  = strchr(p, '&');
    if (!e)
        e = p + strlen(p);

    if (e <= p)
        return ENODATA;

    if (e - p < l)
	l = e - p;

    strncpy(val, p, l);
    return 0;
}


static time_t convert_time(const char *stime)
{
    struct tm tm;

    if (!stime)
	return EINVAL;

    memset(&tm, 0, sizeof tm);
    if (sscanf(stime, "%d%%3A%d", &tm.tm_hour, &tm.tm_min) <= 0)
	return 0;

    return mktime(&tm);
}


static time_t current_time(void)
{
    struct tm tmc, tmcc;
    struct timespec tp;

    memset(&tmcc, 0, sizeof tmcc);

    clock_gettime(CLOCK_REALTIME, &tp);
    localtime_r(&tp.tv_sec, &tmc);
    tmcc.tm_hour = tmc.tm_hour;
    tmcc.tm_min = tmc.tm_min;
    return mktime(&tmcc);
}


/* An HTTP POST handler */
static esp_err_t handle_post(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;
    char stime[10];
    char dur[10];
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
            d.times = current_time();
            d.duration = 3;
        }
        else {
            err  = body_value(stime, sizeof stime, buf, "stime");
            err |= body_value(dur, sizeof dur, buf, "duration");
            if (!err) {
                logw("stime=%s dur=%s", stime, dur);
                d.times = convert_time(stime);
                strftime(stime, sizeof stime, "%H:%M", localtime(&d.times));
                logw("d.times=%lu stime=%s", d.times, stime);
                d.duration = atoi(dur);
                logw("duration=%d", d.duration);
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


httpd_handle_t start_webserver(void)
{
    memset(&d, 0, sizeof(d));
    d.times = current_time();
    d.duration = 3;
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

    timec = current_time();
    return d.times <= timec && timec <= d.times + d.duration * 3600;
}
