// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/json.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif

static void exercise(void)
{
    const char *invalid[] = {"", "{", "{} trailing", "{\"n\":01}", "{\"n\":1.}",
        "{\"n\":1e}", "{\"s\":\"raw\nline\"}", "{\"s\":\"\xff\"}", "{\"s\":\"Org\\u0000suffix\"}"};
    for (size_t n = 0U; n < 500U; ++n) {
        for (size_t i = 0U; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
            assert(!runner_json_parse(invalid[i], 1024U));
        cJSON *json = runner_json_parse("{\"s\":\"valid \\u03a9 \\\\u0000\",\"n\":1}", 1024U);
        assert(json);
        const cJSON *s = runner_json_unique_member(json, "s");
        assert(cJSON_IsString(s) && !strcmp(s->valuestring, "valid Ω \\u0000"));
        cJSON_Delete(json);
        json = runner_json_parse("{\"n\":1,\"n\":2}", 1024U);
        assert(json && !runner_json_unique_member(json, "n"));
        cJSON_Delete(json);
    }
}
#ifdef _WIN32
static DWORD WINAPI worker(LPVOID ignored) { (void)ignored; exercise(); return 0U; }
#else
static void *worker(void *ignored) { (void)ignored; exercise(); return NULL; }
#endif

int main(void)
{
    assert(!runner_json_parse(NULL, 1024U));
    assert(!runner_json_parse("{}", 1U));
    enum { THREADS = 4 };
#ifdef _WIN32
    HANDLE threads[THREADS];
    for (size_t i = 0U; i < THREADS; ++i) {
        threads[i] = CreateThread(NULL, 0U, worker, NULL, 0U, NULL);
        assert(threads[i]);
    }
    assert(WaitForMultipleObjects(THREADS, threads, TRUE, 15000U) == WAIT_OBJECT_0);
    for (size_t i = 0U; i < THREADS; ++i) CloseHandle(threads[i]);
#else
    pthread_t threads[THREADS];
    for (size_t i = 0U; i < THREADS; ++i) assert(!pthread_create(&threads[i], NULL, worker, NULL));
    for (size_t i = 0U; i < THREADS; ++i) assert(!pthread_join(threads[i], NULL));
#endif
    return 0;
}
