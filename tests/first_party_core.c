// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/native2/http.h"
#include "../src/native2/model.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void model_contract(void)
{
    RsRunnerSession session;
    rs_runner_session_init(&session, RS_RUNNER_IDLE, 100.0);
    assert(session.jobs_started == 0U);
    assert(rs_runner_session_apply(&session, RS_RUNNER_RUNNING, 110.0));
    assert(session.jobs_started == 1U);
    assert(fabs(rs_runner_session_busy_seconds(&session, 115.0) - 5.0) < 0.001);
    assert(rs_runner_session_apply(&session, RS_RUNNER_IDLE, 120.0));
    assert(fabs(rs_runner_session_busy_seconds(&session, 140.0) - 10.0) < 0.001);
    assert(fabs(rs_runner_session_busy_percent(&session, 100.0, 140.0) - 25.0) < 0.001);
}

static void http_contract(void)
{
    char request[1024];
    size_t written = 0U;
    assert(rs_http_build_get(request, sizeof(request),
                             "api.github.com", "/rate_limit",
                             "abc123", "RunnerScope/test", &written));
    assert(written == strlen(request));
    assert(strstr(request, "GET /rate_limit HTTP/1.1\r\n") != NULL);
    assert(strstr(request, "Host: api.github.com\r\n") != NULL);
    assert(strstr(request, "Authorization: Bearer abc123\r\n") != NULL);
    assert(!rs_http_build_get(request, sizeof(request),
                              "api.github.com\r\nInjected: yes", "/",
                              NULL, "RunnerScope/test", NULL));

    const char response[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 5\r\n"
        "Connection: close\r\n\r\n"
        "hello";
    RsHttpResponseView view;
    assert(rs_http_parse_response(response, sizeof(response) - 1U, &view));
    assert(view.status_code == 200U);
    assert(view.content_length == 5U);
    assert(view.body_length == 5U);
    assert(memcmp(view.body, "hello", 5U) == 0);

    const char chunked[] =
        "4\r\nWiki\r\n"
        "5\r\npedia\r\n"
        "0\r\n\r\n";
    char decoded[32];
    size_t decoded_length = 0U;
    assert(rs_http_decode_chunked(
        chunked, sizeof(chunked) - 1U,
        decoded, sizeof(decoded), &decoded_length));
    assert(decoded_length == 9U);
    assert(strcmp(decoded, "Wikipedia") == 0);
}

int main(void)
{
    model_contract();
    http_contract();
    puts("RunnerScope first-party core contracts passed");
    return 0;
}
