// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L
#include "platform/http.h"

#include <curl/curl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static pthread_once_t curl_once = PTHREAD_ONCE_INIT;
static CURLcode curl_status = CURLE_FAILED_INIT;

typedef struct {
    RunnerHttpResponse *response;
    size_t used;
    size_t capacity;
    bool oversized;
} Capture;

static void initialize_curl(void)
{
    curl_status = curl_global_init(CURL_GLOBAL_DEFAULT);
}

static void set_error(char *error, size_t size, const char *message)
{
    if (error && size) (void)snprintf(error, size, "%s", message);
}

static size_t receive_body(char *data, size_t size, size_t count, void *context)
{
    Capture *capture = context;
    if (size && count > (size_t)-1 / size) return 0U;
    const size_t amount = size * count;
    if (amount > RUNNER_HTTP_BODY_LIMIT - capture->used) {
        capture->oversized = true;
        return 0U;
    }
    const size_t required = capture->used + amount + 1U;
    if (required > capture->capacity) {
        size_t next = capture->capacity ? capture->capacity : 4096U;
        while (next < required) next *= 2U;
        char *grown = realloc(capture->response->body, next);
        if (!grown) return 0U;
        capture->response->body = grown;
        capture->capacity = next;
    }
    memcpy(capture->response->body + capture->used, data, amount);
    capture->used += amount;
    capture->response->body[capture->used] = '\0';
    return amount;
}

static size_t receive_header(char *data, size_t size, size_t count, void *context)
{
    Capture *capture = context;
    if (size && count > (size_t)-1 / size) return 0U;
    const size_t amount = size * count;
    if (amount < 5U || strncasecmp(data, "Link:", 5U) != 0) return amount;
    const char *start = data + 5U;
    const char *end = data + amount;
    while (start < end && (*start == ' ' || *start == '\t')) start++;
    while (end > start && (end[-1] == '\r' || end[-1] == '\n' || end[-1] == ' ')) end--;
    const size_t length = (size_t)(end - start);
    const size_t previous = capture->response->link ? strlen(capture->response->link) : 0U;
    if (length > 8192U || previous + length + 2U > 8192U) return 0U;
    char *link = realloc(capture->response->link, previous + length + 2U);
    if (!link) return 0U;
    capture->response->link = link;
    size_t offset = previous;
    if (previous) link[offset++] = ',';
    memcpy(link + offset, start, length);
    link[offset + length] = '\0';
    return amount;
}

static bool append_header(struct curl_slist **headers, const char *value)
{
    struct curl_slist *next = curl_slist_append(*headers, value);
    if (!next) return false;
    *headers = next;
    return true;
}

static bool http_request(const char *url, const char *token, const char *form,
    RunnerHttpResponse *response, char *error, size_t error_size, void *context)
{
    (void)context;
    if (!response) return false;
    memset(response, 0, sizeof(*response));
    if (error && error_size) error[0] = '\0';
    /* This provider never forwards a bearer token to another origin, even if
     * an API redirect or pagination header attempts to change authority. */
    const bool oauth = form != NULL;
    const char *origin = oauth ? "https://github.com/" : "https://api.github.com/";
    if (!url || strncmp(url, origin, strlen(origin)) != 0 || strlen(url) > 4096U ||
        strchr(url, '\r') || strchr(url, '\n')) {
        set_error(error, error_size, "GitHub request address is invalid.");
        return false;
    }
    if (!token) token = "";
    if (strlen(token) > 1023U) {
        set_error(error, error_size, "The saved access token is too long. Update it in Settings.");
        return false;
    }
    for (const unsigned char *p = (const unsigned char *)token; *p; ++p) {
        if (*p <= 0x20U || *p >= 0x7fU) {
            set_error(error, error_size, "The saved access token contains invalid characters. Update it in Settings.");
            return false;
        }
    }
    (void)pthread_once(&curl_once, initialize_curl);
    if (curl_status != CURLE_OK) {
        set_error(error, error_size, "Unable to initialize the internal HTTPS client.");
        return false;
    }
    CURL *client = curl_easy_init();
    if (!client) {
        set_error(error, error_size, "Unable to allocate the internal HTTPS client.");
        return false;
    }
    struct curl_slist *headers = NULL;
    char authorization[1056];
    (void)snprintf(authorization, sizeof(authorization), "Authorization: Bearer %s", token);
    bool configured = oauth ? (append_header(&headers, "Accept: application/json") &&
        append_header(&headers, "Content-Type: application/x-www-form-urlencoded")) :
        (append_header(&headers, "Accept: application/vnd.github+json") &&
        append_header(&headers, "X-GitHub-Api-Version: 2022-11-28") &&
        (!*token || append_header(&headers, authorization)));
    memset(authorization, 0, sizeof(authorization));
    Capture capture = {response, 0U, 0U, false};
    /* Every transfer owns its easy handle. Global initialization is the only
     * shared transport state; requests remain safe in the existing workers. */
#define SET_OPTION(option, value) \
    do { if (curl_easy_setopt(client, (option), (value)) != CURLE_OK) configured = false; } while (0)
    SET_OPTION(CURLOPT_URL, url);
    if (form) {
        SET_OPTION(CURLOPT_POST, 1L);
        SET_OPTION(CURLOPT_POSTFIELDS, form);
        SET_OPTION(CURLOPT_POSTFIELDSIZE, (long)strlen(form));
    } else SET_OPTION(CURLOPT_HTTPGET, 1L);
    SET_OPTION(CURLOPT_HTTPHEADER, headers);
    SET_OPTION(CURLOPT_USERAGENT, "RunnerScope");
    SET_OPTION(CURLOPT_PROTOCOLS_STR, "https");
    SET_OPTION(CURLOPT_FOLLOWLOCATION, 0L);
    SET_OPTION(CURLOPT_SSL_VERIFYPEER, 1L);
    SET_OPTION(CURLOPT_SSL_VERIFYHOST, 2L);
    SET_OPTION(CURLOPT_SSLVERSION, (long)CURL_SSLVERSION_TLSv1_2);
    SET_OPTION(CURLOPT_CONNECTTIMEOUT, 10L);
    SET_OPTION(CURLOPT_TIMEOUT, (long)RUNNER_HTTP_TIMEOUT_SECONDS);
    SET_OPTION(CURLOPT_LOW_SPEED_LIMIT, 1L);
    SET_OPTION(CURLOPT_LOW_SPEED_TIME, 10L);
    SET_OPTION(CURLOPT_NOSIGNAL, 1L);
    SET_OPTION(CURLOPT_ACCEPT_ENCODING, "");
    SET_OPTION(CURLOPT_WRITEFUNCTION, receive_body);
    SET_OPTION(CURLOPT_WRITEDATA, &capture);
    SET_OPTION(CURLOPT_HEADERFUNCTION, receive_header);
    SET_OPTION(CURLOPT_HEADERDATA, &capture);
#undef SET_OPTION
    const CURLcode result = configured ? curl_easy_perform(client) : CURLE_FAILED_INIT;
    long status = 0L;
    if (result == CURLE_OK) (void)curl_easy_getinfo(client, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(client);
    curl_slist_free_all(headers);
    if (result != CURLE_OK || (response->body && memchr(response->body, '\0', capture.used))) {
        if (capture.oversized) set_error(error, error_size, "GitHub response exceeded the safe size limit.");
        else if (result == CURLE_OK)
            set_error(error, error_size, "GitHub returned an invalid response.");
        else if (result == CURLE_OPERATION_TIMEDOUT)
            set_error(error, error_size, "GitHub request timed out. Check the network and retry.");
        else if (result == CURLE_PEER_FAILED_VERIFICATION || result == CURLE_SSL_CONNECT_ERROR)
            set_error(error, error_size, "Unable to verify a secure connection to GitHub.");
        else set_error(error, error_size, "Unable to reach GitHub. Check the network and retry.");
        runner_http_response_destroy(response);
        return false;
    }
    response->status = status >= 0L && status <= 999L ? (unsigned int)status : 0U;
    if (!response->body) response->body = calloc(1U, 1U);
    if (!response->body) {
        runner_http_response_destroy(response);
        set_error(error, error_size, "Unable to allocate the GitHub response.");
        return false;
    }
    return true;
}


bool runner_http_get(const char *url, const char *token,
    RunnerHttpResponse *response, char *error, size_t error_size, void *context)
{
    return http_request(url, token, NULL, response, error, error_size, context);
}

bool runner_http_post_oauth(const char *path, const char *form,
    RunnerHttpResponse *response, char *error, size_t error_size, void *context)
{
    if (response) memset(response, 0, sizeof(*response));
    if (!path || (strcmp(path, "/login/device/code") != 0 &&
        strcmp(path, "/login/oauth/access_token") != 0) || !form || strlen(form) > 4096U) {
        set_error(error, error_size, "Invalid GitHub sign-in request.");
        return false;
    }
    for (const unsigned char *p = (const unsigned char *)form; *p; ++p)
        if (*p < 33U || *p > 126U) {
            set_error(error, error_size, "Invalid GitHub sign-in request.");
            return false;
        }
    char url[128];
    (void)snprintf(url, sizeof(url), "https://github.com%s", path);
    return http_request(url, NULL, form, response, error, error_size, context);
}
