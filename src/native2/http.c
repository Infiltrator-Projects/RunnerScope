// SPDX-License-Identifier: GPL-3.0-or-later
#include "http.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool safe_token(const char *text)
{
    if (!text || !*text) return false;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p)
        if (*p < 0x21U || *p > 0x7eU || *p == '\r' || *p == '\n')
            return false;
    return true;
}

static bool safe_path(const char *text)
{
    if (!text || text[0] != '/') return false;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p)
        if (*p == '\r' || *p == '\n' || *p < 0x20U)
            return false;
    return true;
}

bool rs_http_build_get(char *output, size_t output_size,
                       const char *host, const char *path,
                       const char *bearer_token,
                       const char *user_agent,
                       size_t *written)
{
    if (written) *written = 0U;
    if (!output || output_size == 0U ||
        !safe_token(host) || !safe_path(path) ||
        !safe_token(user_agent))
        return false;
    if (bearer_token && *bearer_token && !safe_token(bearer_token))
        return false;

    int amount;
    if (bearer_token && *bearer_token) {
        amount = snprintf(
            output, output_size,
            "GET %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "User-Agent: %s\r\n"
            "Accept: application/vnd.github+json\r\n"
            "X-GitHub-Api-Version: 2022-11-28\r\n"
            "Authorization: Bearer %s\r\n"
            "Connection: close\r\n\r\n",
            path, host, user_agent, bearer_token);
    } else {
        amount = snprintf(
            output, output_size,
            "GET %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "User-Agent: %s\r\n"
            "Accept: application/vnd.github+json\r\n"
            "X-GitHub-Api-Version: 2022-11-28\r\n"
            "Connection: close\r\n\r\n",
            path, host, user_agent);
    }
    if (amount < 0 || (size_t)amount >= output_size) {
        output[0] = '\0';
        return false;
    }
    if (written) *written = (size_t)amount;
    return true;
}

static const char *find_bytes(const char *haystack, size_t haystack_length,
                              const char *needle, size_t needle_length)
{
    if (!haystack || !needle || needle_length == 0U ||
        needle_length > haystack_length)
        return NULL;
    for (size_t i = 0U; i + needle_length <= haystack_length; ++i)
        if (memcmp(haystack + i, needle, needle_length) == 0)
            return haystack + i;
    return NULL;
}

static bool header_name_equals(const char *line, size_t line_length,
                               const char *name)
{
    const size_t name_length = strlen(name);
    if (line_length <= name_length || line[name_length] != ':')
        return false;
    for (size_t i = 0U; i < name_length; ++i)
        if (tolower((unsigned char)line[i]) !=
            tolower((unsigned char)name[i]))
            return false;
    return true;
}

static const char *trim_left(const char *start, const char *end)
{
    while (start < end && (*start == ' ' || *start == '\t')) start++;
    return start;
}

bool rs_http_parse_response(const char *response, size_t response_length,
                            RsHttpResponseView *view)
{
    if (!response || !view || response_length < 12U)
        return false;
    memset(view, 0, sizeof(*view));

    const char *line_end = find_bytes(response, response_length, "\r\n", 2U);
    if (!line_end || (size_t)(line_end - response) < 12U ||
        memcmp(response, "HTTP/1.", 7U) != 0)
        return false;

    const char *status = strchr(response, ' ');
    if (!status || status + 4 > line_end) return false;
    if (!isdigit((unsigned char)status[1]) ||
        !isdigit((unsigned char)status[2]) ||
        !isdigit((unsigned char)status[3]))
        return false;
    view->status_code =
        (unsigned)(status[1] - '0') * 100U +
        (unsigned)(status[2] - '0') * 10U +
        (unsigned)(status[3] - '0');

    const char *headers_end =
        find_bytes(response, response_length, "\r\n\r\n", 4U);
    if (!headers_end) return false;

    const char *cursor = line_end + 2;
    while (cursor < headers_end) {
        const char *end = find_bytes(
            cursor, (size_t)(headers_end - cursor), "\r\n", 2U);
        if (!end) return false;
        const size_t length = (size_t)(end - cursor);
        if (header_name_equals(cursor, length, "Content-Length")) {
            const char *value = trim_left(
                cursor + strlen("Content-Length") + 1U, end);
            errno = 0;
            char *parse_end = NULL;
            unsigned long long parsed = strtoull(value, &parse_end, 10);
            if (errno != 0 || parse_end == value || parse_end > end ||
                parsed > (unsigned long long)SIZE_MAX)
                return false;
            while (parse_end < end &&
                   (*parse_end == ' ' || *parse_end == '\t'))
                parse_end++;
            if (parse_end != end) return false;
            view->content_length = (size_t)parsed;
        } else if (header_name_equals(cursor, length, "Transfer-Encoding")) {
            const char *value = trim_left(
                cursor + strlen("Transfer-Encoding") + 1U, end);
            if ((size_t)(end - value) >= 7U) {
                for (const char *p = value; p + 7U <= end; ++p) {
                    static const char chunked[] = "chunked";
                    size_t matched = 0U;
                    while (matched < 7U &&
                           tolower((unsigned char)p[matched]) ==
                           chunked[matched])
                        matched++;
                    if (matched == 7U) {
                        view->chunked = true;
                        break;
                    }
                }
            }
        }
        cursor = end + 2;
    }

    view->body = headers_end + 4;
    view->body_length =
        response_length - (size_t)(view->body - response);
    if (!view->chunked && view->content_length != 0U &&
        view->body_length < view->content_length)
        return false;
    return true;
}

static int hex_value(unsigned char c)
{
    if (c >= '0' && c <= '9') return (int)(c - '0');
    c = (unsigned char)tolower(c);
    if (c >= 'a' && c <= 'f') return (int)(c - 'a') + 10;
    return -1;
}

bool rs_http_decode_chunked(const char *encoded, size_t encoded_length,
                            char *output, size_t output_size,
                            size_t *decoded_length)
{
    if (decoded_length) *decoded_length = 0U;
    if (!encoded || !output) return false;

    size_t input = 0U, output_used = 0U;
    for (;;) {
        size_t line_start = input;
        size_t line_end = line_start;
        while (line_end + 1U < encoded_length &&
               !(encoded[line_end] == '\r' &&
                 encoded[line_end + 1U] == '\n'))
            line_end++;
        if (line_end + 1U >= encoded_length) return false;

        size_t chunk = 0U;
        bool any = false;
        for (size_t i = line_start; i < line_end && encoded[i] != ';'; ++i) {
            const int value = hex_value((unsigned char)encoded[i]);
            if (value < 0) return false;
            any = true;
            if (chunk > (SIZE_MAX - (size_t)value) / 16U) return false;
            chunk = chunk * 16U + (size_t)value;
        }
        if (!any) return false;
        input = line_end + 2U;
        if (chunk == 0U) {
            if (decoded_length) *decoded_length = output_used;
            if (output_used < output_size) output[output_used] = '\0';
            return true;
        }
        if (chunk > encoded_length - input ||
            output_used > output_size ||
            chunk > output_size - output_used)
            return false;
        memcpy(output + output_used, encoded + input, chunk);
        output_used += chunk;
        input += chunk;
        if (input + 2U > encoded_length ||
            encoded[input] != '\r' || encoded[input + 1U] != '\n')
            return false;
        input += 2U;
    }
}
