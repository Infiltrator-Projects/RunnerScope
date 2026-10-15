// SPDX-License-Identifier: GPL-3.0-or-later
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <winhttp.h>

#include "platform/http.h"
#include "core/runner_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* WinHTTP's phase timeouts are not a total request deadline. Async operations
 * let the provider worker enforce one 30-second deadline across DNS, connect,
 * send, headers, and all reads. Closing an async request cancels pending I/O;
 * its callback context/read buffer survives until HANDLE_CLOSING. */
typedef struct {
    HANDLE event;
    volatile LONG references;
    DWORD completion;
    DWORD failure;
    DWORD amount;
    unsigned char chunk[8192];
} HttpOperation;

static void http_error(char *error, size_t size, const char *message)
{
    if (error && size) (void)snprintf(error, size, "%s", message);
}

static void http_operation_release(HttpOperation *operation)
{
    if (InterlockedDecrement(&operation->references) == 0) {
        CloseHandle(operation->event);
        SecureZeroMemory(operation, sizeof(*operation));
        free(operation);
    }
}

static void CALLBACK http_completed(HINTERNET request, DWORD_PTR context,
                                    DWORD status, LPVOID information, DWORD length)
{
    (void)request;
    HttpOperation *operation = (HttpOperation *)context;
    if (!operation) return;
    if (status == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING) {
        http_operation_release(operation);
        return;
    }
    if (status == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR) {
        operation->failure = length >= sizeof(WINHTTP_ASYNC_RESULT)
            ? ((WINHTTP_ASYNC_RESULT *)information)->dwError : ERROR_WINHTTP_INTERNAL_ERROR;
    } else if (status == WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE) {
        operation->amount = length >= sizeof(DWORD) ? *(DWORD *)information : 0U;
    } else if (status == WINHTTP_CALLBACK_STATUS_READ_COMPLETE) {
        operation->amount = length;
    } else if (status != WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE &&
               status != WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE) return;
    operation->completion = status;
    SetEvent(operation->event);
}

static bool http_wait(HttpOperation *operation, DWORD expected, ULONGLONG deadline,
                       char *error, size_t error_size)
{
    const ULONGLONG now = GetTickCount64();
    if (now >= deadline || WaitForSingleObject(operation->event,
            (DWORD)(deadline - now)) != WAIT_OBJECT_0) {
        http_error(error, error_size, "GitHub request timed out after 30 seconds.");
        return false;
    }
    if (operation->completion == expected) return true;
    http_error(error, error_size,
        operation->failure == ERROR_WINHTTP_TIMEOUT ? "GitHub request timed out after 30 seconds."
        : operation->failure == ERROR_WINHTTP_SECURE_FAILURE ? "GitHub's HTTPS certificate could not be verified."
        : operation->failure == ERROR_WINHTTP_NAME_NOT_RESOLVED ? "Unable to resolve api.github.com. Check the network connection."
        : "Unable to reach GitHub securely. Check the network connection and try again.");
    return false;
}

static bool http_started(BOOL started)
{
    return started || GetLastError() == ERROR_IO_PENDING;
}

static char *http_link_header(HINTERNET request, bool *ok)
{
    char *joined = NULL;
    size_t used = 0U;
    DWORD index = 0U;
    *ok = true;
    for (unsigned int field = 0U; field < 100U; ++field) {
        DWORD bytes = 0U, probe_index = index;
        (void)WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM, L"Link", NULL,
                                  &bytes, &probe_index);
        const DWORD failure = GetLastError();
        if (failure == ERROR_WINHTTP_HEADER_NOT_FOUND) return joined;
        if (failure != ERROR_INSUFFICIENT_BUFFER || bytes > 16386U) break;
        wchar_t *wide = (wchar_t *)calloc((size_t)bytes + sizeof(wchar_t), 1U);
        DWORD next_index = index;
        if (!wide || !WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM, L"Link", wide,
                                          &bytes, &next_index) || next_index <= index ||
            wcslen(wide) * sizeof(wchar_t) != bytes) {
            free(wide);
            break;
        }
        index = next_index;
        const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1,
                                             NULL, 0, NULL, NULL);
        if (count <= 0 || used + (used ? 1U : 0U) + (size_t)count - 1U > 8192U) {
            free(wide);
            break;
        }
        if (count > 1) {
            const size_t separator = used ? 1U : 0U;
            char *grown = (char *)realloc(joined, used + separator + (size_t)count);
            if (!grown) { free(wide); break; }
            joined = grown;
            if (separator) joined[used++] = ',';
            if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1,
                                     joined + used, count, NULL, NULL)) {
                free(wide);
                break;
            }
            used += (size_t)count - 1U;
        }
        free(wide);
    }
    free(joined);
    *ok = false;
    return NULL;
}

bool runner_http_get(const char *url, const char *token,
    RunnerHttpResponse *response, char *error, size_t error_size, void *context)
{
    if (error && error_size) error[0] = '\0';
    if (!response) return false;
    memset(response, 0, sizeof(*response));
    const char *prefix = "https://api.github.com/";
    if (!url || strncmp(url, prefix, strlen(prefix)) != 0 || strlen(url) > 8192U) {
        http_error(error, error_size, "Only HTTPS requests to api.github.com are allowed.");
        return false;
    }
    for (const unsigned char *p = (const unsigned char *)url; *p; ++p)
        if (*p <= 32U || *p >= 127U || *p == '#') {
            http_error(error, error_size, "The GitHub request address is invalid.");
            return false;
        }
    if (!token || !*token || strlen(token) >= RUNNER_GITHUB_TOKEN_CAPACITY) {
        http_error(error, error_size, "Save a GitHub token in Settings before refreshing runners.");
        return false;
    }
    for (const unsigned char *p = (const unsigned char *)token; *p; ++p)
        if (*p < 33U || *p > 126U) {
            http_error(error, error_size, "The saved GitHub token contains invalid characters.");
            return false;
        }

    wchar_t path[8193];
    wchar_t headers[1400];
    const char *resource = url + strlen("https://api.github.com");
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, resource, -1,
                              path, (int)(sizeof(path) / sizeof(path[0])))) {
        http_error(error, error_size, "The GitHub request address is invalid.");
        return false;
    }
    _snwprintf_s(headers, sizeof(headers) / sizeof(headers[0]), _TRUNCATE,
        L"Authorization: Bearer %hs\r\nAccept: application/vnd.github+json\r\n"
        L"X-GitHub-Api-Version: 2022-11-28\r\n", token);

    ULONGLONG deadline = GetTickCount64() + RUNNER_HTTP_TIMEOUT_SECONDS * 1000U;
    /* A native refresh may impose an earlier overall pagination deadline. */
    if (context && *(const ULONGLONG *)context < deadline)
        deadline = *(const ULONGLONG *)context;
    if (GetTickCount64() >= deadline) {
        SecureZeroMemory(headers, sizeof(headers));
        http_error(error, error_size, "Runner refresh exceeded its 60-second deadline.");
        return false;
    }
    HINTERNET session = NULL, connection = NULL, request = NULL;
    HttpOperation *operation = (HttpOperation *)calloc(1U, sizeof(*operation));
    bool callbacks = false;
    bool ok = false;
    size_t used = 0U, capacity = 8192U;
    char *body = NULL;
    if (!operation) goto done;
    operation->references = 1;
    operation->event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!operation->event) goto done;
    session = WinHttpOpen(L"Runner-Monitor", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);
    if (!session || !WinHttpSetTimeouts(session, 30000, 30000, 30000, 30000)) goto done;
    DWORD retries = 1U;
    if (!WinHttpSetOption(session, WINHTTP_OPTION_CONNECT_RETRIES, &retries, sizeof(retries))) goto done;
    connection = WinHttpConnect(session, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0U);
    if (!connection) goto done;
    request = WinHttpOpenRequest(connection, L"GET", path, NULL, WINHTTP_NO_REFERER,
                                 WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request) goto done;
    DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    DWORD disabled = WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION;
    DWORD_PTR request_context = (DWORD_PTR)operation;
    if (!WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirect_policy, sizeof(redirect_policy)) ||
        !WinHttpSetOption(request, WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled)) ||
        !WinHttpSetOption(request, WINHTTP_OPTION_CONTEXT_VALUE, &request_context, sizeof(request_context)) ||
        !WinHttpAddRequestHeaders(request, headers, (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD)) goto done;
    SecureZeroMemory(headers, sizeof(headers));
    InterlockedIncrement(&operation->references);
    if (WinHttpSetStatusCallback(request, http_completed,
        WINHTTP_CALLBACK_FLAG_SENDREQUEST_COMPLETE | WINHTTP_CALLBACK_FLAG_HEADERS_AVAILABLE |
        WINHTTP_CALLBACK_FLAG_DATA_AVAILABLE | WINHTTP_CALLBACK_FLAG_READ_COMPLETE |
        WINHTTP_CALLBACK_FLAG_REQUEST_ERROR | WINHTTP_CALLBACK_FLAG_HANDLES, 0U) == WINHTTP_INVALID_STATUS_CALLBACK) {
        http_operation_release(operation);
        goto done;
    }
    callbacks = true;
    if (!http_started(WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0U,
            WINHTTP_NO_REQUEST_DATA, 0U, 0U, request_context)) ||
        !http_wait(operation, WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE, deadline, error, error_size) ||
        !http_started(WinHttpReceiveResponse(request, NULL)) ||
        !http_wait(operation, WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE, deadline, error, error_size)) goto done;
    DWORD status = 0U, status_size = sizeof(status);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
                             WINHTTP_NO_HEADER_INDEX)) goto done;
    response->status = (unsigned int)status;
    bool link_ok;
    response->link = http_link_header(request, &link_ok);
    if (!link_ok) goto done;
    body = (char *)calloc(capacity, 1U);
    if (!body) goto done;
    for (;;) {
        if (!http_started(WinHttpQueryDataAvailable(request, NULL)) ||
            !http_wait(operation, WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE, deadline, error, error_size)) goto done;
        if (operation->amount == 0U) break;
        const DWORD requested = operation->amount < sizeof(operation->chunk)
            ? operation->amount : (DWORD)sizeof(operation->chunk);
        if (!http_started(WinHttpReadData(request, operation->chunk, requested, NULL)) ||
            !http_wait(operation, WINHTTP_CALLBACK_STATUS_READ_COMPLETE, deadline, error, error_size)) goto done;
        if (operation->amount == 0U) break;
        if (operation->amount > sizeof(operation->chunk) ||
            memchr(operation->chunk, '\0', operation->amount)) {
            http_error(error, error_size, "GitHub returned an invalid response body.");
            goto done;
        }
        if (operation->amount > RUNNER_HTTP_BODY_LIMIT - used) {
            http_error(error, error_size, "GitHub's response exceeded the 16 MiB limit.");
            goto done;
        }
        const size_t needed = used + operation->amount + 1U;
        if (needed > capacity) {
            size_t next = capacity * 2U;
            while (next < needed) next *= 2U;
            if (next > RUNNER_HTTP_BODY_LIMIT + 1U) next = RUNNER_HTTP_BODY_LIMIT + 1U;
            char *grown = (char *)realloc(body, next);
            if (!grown) goto done;
            body = grown;
            capacity = next;
        }
        memcpy(body + used, operation->chunk, operation->amount);
        used += operation->amount;
        body[used] = '\0';
    }
    response->body = body;
    body = NULL;
    ok = true;

done:
    SecureZeroMemory(headers, sizeof(headers));
    free(body);
    if (request) WinHttpCloseHandle(request);
    if (connection) WinHttpCloseHandle(connection);
    if (session) WinHttpCloseHandle(session);
    if (operation) {
        /* The callback owns its reference until the asynchronous handle closes. */
        if (!callbacks) operation->references = 1;
        http_operation_release(operation);
    }
    if (!ok) {
        free(response->body);
        free(response->link);
        memset(response, 0, sizeof(*response));
        if (!error || !error_size || !error[0])
            http_error(error, error_size, "Unable to complete the GitHub HTTPS request. Check the connection and try again.");
    }
    return ok;
}
