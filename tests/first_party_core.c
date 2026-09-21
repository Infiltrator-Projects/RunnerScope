// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/native2/http.h"
#include "../src/native2/model.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define EXPECT(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "EXPECT failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static int model_contract(void)
{
    RsRunnerSession session;
    rs_runner_session_init(&session, RS_RUNNER_IDLE, 100.0);
    EXPECT(session.jobs_started == 0U);
    EXPECT(rs_runner_session_apply(&session, RS_RUNNER_RUNNING, 110.0));
    EXPECT(session.jobs_started == 1U);
    EXPECT(fabs(rs_runner_session_busy_seconds(&session, 115.0) - 5.0) < 0.001);
    EXPECT(rs_runner_session_apply(&session, RS_RUNNER_IDLE, 120.0));
    EXPECT(fabs(rs_runner_session_busy_seconds(&session, 140.0) - 10.0) < 0.001);
    EXPECT(fabs(rs_runner_session_busy_percent(&session, 100.0, 140.0) - 25.0) < 0.001);

    RsRunnerSession epoch_session;
    rs_runner_session_init(&epoch_session, RS_RUNNER_RUNNING, 0.0);
    EXPECT(epoch_session.busy_active);
    EXPECT(fabs(rs_runner_session_busy_seconds(&epoch_session, 5.0) - 5.0) < 0.001);
    EXPECT(rs_runner_session_apply(&epoch_session, RS_RUNNER_IDLE, 10.0));
    EXPECT(fabs(rs_runner_session_busy_seconds(&epoch_session, 20.0) - 10.0) < 0.001);
    return 0;
}

static int http_contract(void)
{
    char request[1024];
    size_t written = 0U;
    EXPECT(rs_http_build_get(request, sizeof(request),
                             "api.github.com", "/rate_limit",
                             "abc123", "RunnerMonitor/test", &written));
    EXPECT(written == strlen(request));
    EXPECT(strstr(request, "GET /rate_limit HTTP/1.1\r\n") != NULL);
    EXPECT(strstr(request, "Host: api.github.com\r\n") != NULL);
    EXPECT(strstr(request, "Authorization: Bearer abc123\r\n") != NULL);
    EXPECT(!rs_http_build_get(request, sizeof(request),
                              "api.github.com\r\nInjected: yes", "/",
                              NULL, "RunnerMonitor/test", NULL));

    const char response[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 5\r\n"
        "Connection: close\r\n\r\n"
        "hello";
    RsHttpResponseView view;
    EXPECT(rs_http_parse_response(response, sizeof(response) - 1U, &view));
    EXPECT(view.status_code == 200U);
    EXPECT(view.content_length == 5U);
    EXPECT(view.body_length == 5U);
    EXPECT(memcmp(view.body, "hello", 5U) == 0);

    const char bounded_response[] = {
        'H','T','T','P','/','1','.','1',' ','2','0','4',' ','N','o',' ',
        'C','o','n','t','e','n','t','\r','\n',
        'C','o','n','t','e','n','t','-','L','e','n','g','t','h',':',' ','0','\r','\n',
        '\r','\n'
    };
    EXPECT(rs_http_parse_response(
        bounded_response, sizeof(bounded_response), &view));
    EXPECT(view.status_code == 204U);
    EXPECT(view.body_length == 0U);

    const char chunked[] =
        "4\r\nWiki\r\n"
        "5\r\npedia\r\n"
        "0\r\n\r\n";
    char decoded[32];
    size_t decoded_length = 0U;
    EXPECT(rs_http_decode_chunked(
        chunked, sizeof(chunked) - 1U,
        decoded, sizeof(decoded), &decoded_length));
    EXPECT(decoded_length == 9U);
    EXPECT(strcmp(decoded, "Wikipedia") == 0);
    return 0;
}

int main(void)
{
    if (model_contract() != 0) return 1;
    if (http_contract() != 0) return 1;
    puts("Runner Monitor first-party core contracts passed");
    return 0;
}
