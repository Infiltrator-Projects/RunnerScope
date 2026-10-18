// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/json.h"
#include <infiltratr/utf8.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif

/* Reject lexical forms that a permissive decoder can accept, and embedded
 * NUL escapes that would truncate C strings after an otherwise valid parse. */
static bool lexical_json(const char *json)
{
    const unsigned char *p = (const unsigned char *)json;
    while (*p) {
        if (*p == '"') {
            p++;
            while (*p && *p != '"') {
                if (*p < 0x20U) return false;
                if (*p == '\\') {
                    p++;
                    if (!*p) return false;
                    if (*p == 'u' && strncmp((const char *)p, "u0000", 5U) == 0) return false;
                }
                p++;
            }
            if (*p != '"') return false;
            p++;
        } else if (*p == '-' || (*p >= '0' && *p <= '9')) {
            if (*p == '-') p++;
            if (*p == '0') p++;
            else {
                if (*p < '1' || *p > '9') return false;
                do { p++; } while (*p >= '0' && *p <= '9');
            }
            if (*p == '.') {
                p++;
                if (*p < '0' || *p > '9') return false;
                do { p++; } while (*p >= '0' && *p <= '9');
            }
            if (*p == 'e' || *p == 'E') {
                p++;
                if (*p == '+' || *p == '-') p++;
                if (*p < '0' || *p > '9') return false;
                do { p++; } while (*p >= '0' && *p <= '9');
            }
            if (*p && !strchr(" \t\r\n,}]", (int)*p)) return false;
        } else {
            if (*p < 0x20U && !strchr("\t\r\n", (int)*p)) return false;
            p++;
        }
    }
    return true;
}

static cJSON *parse_locked(const char *json, size_t length)
{
    const char *end = NULL;
#ifdef _WIN32
    static SRWLOCK lock = SRWLOCK_INIT;
    AcquireSRWLockExclusive(&lock);
#else
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    (void)pthread_mutex_lock(&lock);
#endif
    cJSON *root = cJSON_ParseWithLengthOpts(json, length + 1U, &end, 1);
#ifdef _WIN32
    ReleaseSRWLockExclusive(&lock);
#else
    (void)pthread_mutex_unlock(&lock);
#endif
    return root;
}


cJSON *runner_json_parse(const char *json, size_t limit)
{
    if (!json) return NULL;
    size_t length = 0U;
    while (length <= limit && json[length]) ++length;
    if (!length || length > limit || !infiltratr_utf8_validate(json, length) ||
        !lexical_json(json)) return NULL;
    return parse_locked(json, length);
}

const cJSON *runner_json_unique_member(const cJSON *object, const char *name)
{
    if (!cJSON_IsObject(object) || !name) return NULL;
    const cJSON *found = NULL, *item;
    cJSON_ArrayForEach(item, object) {
        if (item->string && !strcmp(item->string, name)) {
            if (found) return NULL;
            found = item;
        }
    }
    return found;
}
