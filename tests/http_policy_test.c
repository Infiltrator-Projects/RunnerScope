// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/http.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *paths[] = {NULL, "", "/login/other", "https://github.com/login/device/code",
        "//evil.invalid/login/device/code", "/login/device/code?redirect=evil"};
    const char *private_value = "offline-private-value";
    for (size_t i = 0U; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        RunnerHttpResponse response = {0};
        char error[320] = "";
        if (runner_http_post_oauth(paths[i], private_value, &response, error, sizeof(error), NULL) ||
            response.body || response.link || !error[0] || strstr(error, private_value)) return 1;
    }
    const char *forms[] = {NULL, "client_id=offline-private-value\r\nInjected: header", "client_id=bad value"};
    for (size_t i = 0U; i < sizeof(forms) / sizeof(forms[0]); ++i) {
        RunnerHttpResponse response = {0};
        char error[320] = "";
        if (runner_http_post_oauth("/login/device/code", forms[i], &response, error, sizeof(error), NULL) ||
            response.body || response.link || !error[0] || strstr(error, private_value)) return 2;
    }
    puts("Native OAuth transport rejected unapproved endpoints and invalid forms without networking.");
    return 0;
}
