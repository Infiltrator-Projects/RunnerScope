// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RUNNERSCOPE_NATIVE_HTTP_H
#define RUNNERSCOPE_NATIVE_HTTP_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    unsigned status_code;
    const char *body;
    size_t body_length;
    bool chunked;
    size_t content_length;
} RsHttpResponseView;

bool rs_http_build_get(char *output, size_t output_size,
                       const char *host, const char *path,
                       const char *bearer_token,
                       const char *user_agent,
                       size_t *written);

bool rs_http_parse_response(const char *response, size_t response_length,
                            RsHttpResponseView *view);

bool rs_http_decode_chunked(const char *encoded, size_t encoded_length,
                            char *output, size_t output_size,
                            size_t *decoded_length);

#endif
