/**
 * @file webui.c
 *
 * Copyright (C) 2021 Christian Spielberger
 */

#include <stdio.h>
#include <esp_log.h>
#include "webui.h"

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

static const char *TAG = "webui";

#define HTML_HEADER \
"<!DOCTYPE html>\n" \
"<html>\n" \
"<body>\n" \
"\n"

#define HTML_FOOTER \
"\n" \
"</body>\n" \
"</html>\n"

#define HTML_FORM \
"<h2>Pool Saltwater System</h2>" \
"" \
"<form action=\"/set\" method=\"post\">\n" \
"  <label for=\"stime\">Start time:</label><br>\n" \
"  <input type=\"time\" id=\"stime\" name=\"stime\" min=\"08:00\" max=\"19:00\"><br>\n" \
"  <label for=\"etime\">End time:</label><br>\n" \
"  <input type=\"time\" id=\"etime\" name=\"etime\" min=\"09:00\" max=\"21:00\"><br>\n" \
"  <input type=\"submit\" value=\"Ok\"><br>\n" \
"  <br>\n" \
"  <br>\n" \
"  <br>\n" \
"<label for=\"upgrade\">Upgrade</label>\n" \
"  <input type=\"checkbox\" id=\"upgrade\" name=\"upgrade\">\n" \
"</form>\n" \


struct webui {
    const char *response;
/*    struct timeval stime;*/
/*    struct timeval etime;*/
    bool upgrade;
};

static struct webui d;

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


    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t get_handler = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = handle_get,
    .user_ctx  = HTML_HEADER HTML_FORM HTML_FOOTER
};

/* An HTTP POST handler */
static esp_err_t handle_post(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;

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
        if (strstr(buf, "upgrade=on")) {
            d.upgrade = true;
        }
    }

    // Send response
    const char* resp_str = d.response;
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t post_handler = {
    .uri       = "/set",
    .method    = HTTP_POST,
    .handler   = handle_post,
    .user_ctx  = &d
};


httpd_handle_t start_webserver(void)
{
    d.upgrade = false;
    d.response = HTML_HEADER "<p>Well done!</p>" HTML_FORM HTML_FOOTER;
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

