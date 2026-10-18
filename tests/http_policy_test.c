// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/http.h"
#include <stdio.h>
#include <string.h>

static bool cancelled(void *context) { (void)context; return true; }

int main(void)
{
    const char *urls[] = {NULL, "", "http://api.github.com/x", "https://github.com/login/device/code",
        "https://api.github.com.evil.invalid/x", "https://api.github.com@evil.invalid/x",
        "https://api.github.com/x#fragment", "https://api.github.com/x\\escape",
        "https://api.github.com/x\r\nInjected: header", "https://api.github.com/x y"};
    const char *private_value = "offline-private-value";
    for (size_t i = 0U; i < sizeof(urls) / sizeof(urls[0]); ++i) {
        RunnerHttpResponse response = {0};
        char error[320] = "";
        if (runner_http_get(urls[i], private_value, &response, error, sizeof(error), NULL) ||
            response.body || response.link || !error[0] || strstr(error, private_value)) return 1;
    }
    const char *tokens[] = {"invalid token", "invalid\r\ntoken", "invalid\ttoken"};
    for (size_t i = 0U; i < sizeof(tokens) / sizeof(tokens[0]); ++i) {
        RunnerHttpResponse response = {0};
        char error[320] = "";
        if (runner_http_get("https://api.github.com/", tokens[i], &response, error, sizeof(error), NULL) ||
            response.body || response.link || !error[0] || strstr(error, tokens[i])) return 2;
    }
    RunnerHttpOptions limits[] = {{1U, NULL, NULL}, {0U, cancelled, NULL}};
    for (size_t i = 0U; i < sizeof(limits) / sizeof(limits[0]); ++i) {
        RunnerHttpResponse response = {0};
        char error[320] = "";
        if (runner_http_get("https://api.github.com/", private_value, &response, error, sizeof(error), &limits[i]) ||
            response.body || response.link || !error[0] || strstr(error, private_value)) return 3;
    }
    puts("Native HTTP rejected untrusted addresses and invalid credentials without networking.");
    return 0;
}
