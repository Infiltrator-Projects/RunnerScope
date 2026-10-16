// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RUNNER_HTTP_H
#define RUNNER_HTTP_H

#include <stdbool.h>
#include <stddef.h>

#define RUNNER_HTTP_BODY_LIMIT (16U * 1024U * 1024U)
#define RUNNER_HTTP_TIMEOUT_SECONDS 30U

typedef struct {
    unsigned int status;
    char *body;
    char *link;
} RunnerHttpResponse;

typedef bool (*RunnerHttpGet)(const char *url, const char *token,
    RunnerHttpResponse *response, char *error, size_t error_size, void *context);

typedef bool (*RunnerHttpPost)(const char *path, const char *form,
    RunnerHttpResponse *response, char *error, size_t error_size, void *context);

/* Native HTTPS transport. Tokens never appear in URLs or diagnostics. */
bool runner_http_get(const char *url, const char *token,
    RunnerHttpResponse *response, char *error, size_t error_size, void *context);
void runner_http_response_destroy(RunnerHttpResponse *response);

/* Only GitHub's two device-authorization endpoints are accepted. No bearer
 * credentials are attached, and redirects are never followed. */
bool runner_http_post_oauth(const char *path, const char *form,
    RunnerHttpResponse *response, char *error, size_t error_size, void *context);

#endif
