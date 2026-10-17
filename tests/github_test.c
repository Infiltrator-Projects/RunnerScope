// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/github.h"
#include "core/runner_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "GitHub contract failed at line %d\n", __LINE__); return false; } } while (0)
#define PRIVATE_TOKEN "PAT_PRIVATE_TEST_VALUE"

static char *copy(const char *text)
{
    if (!text) return NULL;
    const size_t size = strlen(text) + 1U;
    char *result = malloc(size);
    if (result) memcpy(result, text, size);
    return result;
}

typedef struct {
    const char *url, *body, *link;
    unsigned int status;
    bool failed;
} Page;
typedef struct { const Page *pages; size_t count, calls; bool valid; } Fixture;

static bool get(const char *url, const char *token, RunnerHttpResponse *response,
                char *error, size_t error_size, void *context)
{
    Fixture *fixture = context;
    if (fixture->calls >= fixture->count || strcmp(token, PRIVATE_TOKEN) != 0 ||
        strstr(url, PRIVATE_TOKEN)) {
        fixture->valid = false;
        return false;
    }
    const Page *page = &fixture->pages[fixture->calls++];
    if (strcmp(url, page->url)) fixture->valid = false;
    response->body = copy(page->body);
    response->link = copy(page->link);
    response->status = page->status;
    if (page->failed) {
        (void)snprintf(error, error_size, "unsafe transport diagnostic %s", PRIVATE_TOKEN);
        return false;
    }
    return true;
}

static bool project(const char *json, RunnerGithubProjection projection,
                     const char *expected)
{
    char error[320] = "";
    char *rows = runner_github_project(json, projection, error, sizeof(error));
    CHECK(rows && strcmp(rows, expected) == 0 && !error[0]);
    free(rows);
    return true;
}

static bool decoding(void)
{
    CHECK(project("{\"runners\":[{\"name\":\"Runner\\t\\u03a9\\nTwo\",\"os\":\"Linux\",\"status\":\"online\",\"busy\":false,\"labels\":[{\"name\":\"self-hosted\"},{\"name\":\"gpu\\rworker\"}]}]}",
        RUNNER_GITHUB_RUNNERS, "Runner Ω Two\tLinux\tonline\tfalse\tself-hosted,gpu worker\n"));
    CHECK(project("[{\"name\":\"visible\",\"archived\":false,\"disabled\":false},{\"name\":\"old\",\"archived\":true,\"disabled\":false},{\"name\":\"off\",\"archived\":false,\"disabled\":true}]",
        RUNNER_GITHUB_REPOSITORIES, "visible\n"));
    CHECK(project("{\"workflow_runs\":[{\"id\":12345678901,\"name\":null,\"display_title\":\"Build\\nmain\",\"status\":\"in_progress\",\"created_at\":\"2026-10-15T02:00:00Z\"},{\"status\":\"completed\"}]}",
        RUNNER_GITHUB_RUNS, "12345678901\tBuild main\t—\t—\t2026-10-15T02:00:00Z\t\tin_progress\n"));
    CHECK(project("{\"jobs\":[{\"name\":\"Compile\",\"status\":\"in_progress\",\"runner_name\":\"Runner\",\"labels\":[\"self-hosted\",\"linux\"],\"steps\":[{\"name\":\"Checkout\",\"status\":\"completed\"},{\"name\":\"Build\\tcode\",\"status\":\"in_progress\"}]}]}",
        RUNNER_GITHUB_JOBS, "Compile\tin_progress\t\tRunner\t\tself-hosted,linux\t\t\tBuild code\n"));
    CHECK(project("{\"runners\":[]}", RUNNER_GITHUB_RUNNERS, ""));
    return true;
}

static bool invalid_payloads(void)
{
    const char *invalid[] = {
        "{}", "{\"runners\":null}", "{\"runners\":{}}", "{\"runners\":[null]}",
        "{\"runners\":[{\"name\":\"PAT_PRIVATE_TEST_VALUE\"}]}",
        "{\"runners\":[],\"runners\":[]}",
        "{\"runners\":[]} trailing", "{\"runners\":[],\"n\":01}",
        "{\"runners\":[],\"n\":1.}", "{\"runners\":[],\"n\":1e}",
        "{\"runners\":[{\"name\":\"nul\\u0000suffix\"}]}",
        "{\"runners\":[{\"name\":\"raw\nnewline\"}]}",
        "{\"runners\":[],\"bad\":\"\xff\"}",
        "{\"runners\":[{\"name\":\"R\",\"os\":\"Linux\",\"status\":\"online\",\"busy\":\"false\",\"labels\":[]}]}",
        "{\"runners\":[{\"name\":\"R\",\"os\":\"Linux\",\"status\":\"made-up\",\"busy\":false,\"labels\":[]}]}"
    };
    for (size_t i = 0U; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        char error[320] = "";
        char *rows = runner_github_project(invalid[i], RUNNER_GITHUB_RUNNERS, error, sizeof(error));
        CHECK(!rows && error[0] && !strstr(error, PRIVATE_TOKEN));
    }
    char error[320];
    CHECK(!runner_github_project("{\"workflow_runs\":[{\"status\":\"queued\",\"id\":1.5}]}", RUNNER_GITHUB_RUNS, error, sizeof(error)));
    CHECK(!runner_github_project("{\"jobs\":[{\"status\":\"queued\",\"labels\":[]}]}", RUNNER_GITHUB_JOBS, error, sizeof(error)));
    CHECK(!runner_github_project("[]", (RunnerGithubProjection)99, error, sizeof(error)));
    return true;
}

static bool pagination(void)
{
    const Page pages[] = {
        {"https://api.github.com/orgs/A/actions/runners?per_page=100", "{\"runners\":[{\"name\":\"A\",\"os\":\"Linux\",\"status\":\"online\",\"busy\":true,\"labels\":[]}]}",
         "<https://api.github.com/orgs/A/actions/runners?page=3>; rel=\"last\", <https://api.github.com/orgs/A/actions/runners?page=2>; rel=\"next\"", 200U, false},
        {"https://api.github.com/orgs/A/actions/runners?page=2", "{\"runners\":[{\"name\":\"B\",\"os\":\"Windows\",\"status\":\"offline\",\"busy\":false,\"labels\":[]}]}", NULL, 200U, false}
    };
    Fixture fixture = {pages, 2U, 0U, true};
    char error[320] = "";
    char *rows = runner_github_fetch("/orgs/A/actions/runners?per_page=100", PRIVATE_TOKEN,
        RUNNER_GITHUB_RUNNERS, get, &fixture, error, sizeof(error));
    CHECK(rows && fixture.valid && fixture.calls == 2U && !error[0]);
    CHECK(strcmp(rows, "A\tLinux\tonline\ttrue\t\nB\tWindows\toffline\tfalse\t\n") == 0);
    free(rows);
    return true;
}

static bool active_run_filtering(void)
{
    const Page pages[] = {
        {"https://api.github.com/repos/A/R/actions/runs?per_page=100&exclude_pull_requests=true&status=in_progress",
         "{\"workflow_runs\":[{\"id\":101,\"name\":\"Build\",\"status\":\"in_progress\",\"created_at\":\"2026-10-17T01:00:00Z\"}]}",
         NULL, 200U, false},
        {"https://api.github.com/repos/A/R/actions/runs?per_page=100&exclude_pull_requests=true&status=queued",
         "{\"workflow_runs\":[{\"id\":102,\"name\":\"Package\",\"status\":\"queued\",\"created_at\":\"2026-10-17T01:01:00Z\"}]}",
         NULL, 200U, false}
    };
    Fixture fixture = {pages, 2U, 0U, true};
    char error[320] = "";
    char *rows = runner_github_fetch(
        "/repos/A/R/actions/runs?per_page=100&exclude_pull_requests=true",
        PRIVATE_TOKEN, RUNNER_GITHUB_RUNS, get, &fixture, error, sizeof(error));
    CHECK(rows && fixture.valid && fixture.calls == 2U && !error[0]);
    CHECK(strcmp(rows,
        "101\tBuild\t—\t—\t2026-10-17T01:00:00Z\t\tin_progress\n"
        "102\tPackage\t—\t—\t2026-10-17T01:01:00Z\t\tqueued\n") == 0);
    free(rows);
    return true;
}

static bool refused_requests(void)
{
    const char *bad_urls[] = {"http://api.github.com/x", "https://evil.example/x", "https://api.github.com.evil.example/x", "https://api.github.com@evil.example/x", "//evil.example/x", "/x#fragment", "/x\r\nheader"};
    Fixture fixture = {NULL, 0U, 0U, true};
    char error[320];
    for (size_t i = 0U; i < sizeof(bad_urls) / sizeof(bad_urls[0]); i++) {
        CHECK(!runner_github_fetch(bad_urls[i], PRIVATE_TOKEN, RUNNER_GITHUB_RUNNERS,
                                  get, &fixture, error, sizeof(error)));
        CHECK(error[0] && fixture.calls == 0U);
    }
    const char *bad_tokens[] = {NULL, "", "PAT_PRIVATE_TEST_VALUE x", "PAT_PRIVATE_TEST_VALUE\nheader"};
    for (size_t i = 0U; i < sizeof(bad_tokens) / sizeof(bad_tokens[0]); i++) {
        CHECK(!runner_github_fetch("/x", bad_tokens[i], RUNNER_GITHUB_RUNNERS,
                                  get, &fixture, error, sizeof(error)));
        CHECK(strstr(error, "Settings") && !strstr(error, PRIVATE_TOKEN));
    }
    char long_token[RUNNER_GITHUB_TOKEN_CAPACITY + 1U];
    memset(long_token, 'x', sizeof(long_token) - 1U);
    long_token[sizeof(long_token) - 1U] = '\0';
    CHECK(!runner_github_fetch("/x", long_token, RUNNER_GITHUB_RUNNERS,
                              get, &fixture, error, sizeof(error)));
    CHECK(fixture.calls == 0U);
    return true;
}

static bool safe_failures(void)
{
    const unsigned int statuses[] = {401U, 403U, 404U, 429U, 500U};
    char error[320];
    for (size_t i = 0U; i < sizeof(statuses) / sizeof(statuses[0]); i++) {
        const Page page = {"https://api.github.com/x", PRIVATE_TOKEN, NULL, statuses[i], false};
        Fixture fixture = {&page, 1U, 0U, true};
        CHECK(!runner_github_fetch("/x", PRIVATE_TOKEN, RUNNER_GITHUB_RUNNERS,
                                  get, &fixture, error, sizeof(error)));
        CHECK(error[0] && !strstr(error, PRIVATE_TOKEN));
        if (statuses[i] == 401U || statuses[i] == 403U) CHECK(strstr(error, "Settings"));
        if (statuses[i] == 403U) CHECK(strstr(error, "Self-hosted runners: Read"));
    }
    const Page denied_activity = {"https://api.github.com/x", PRIVATE_TOKEN, NULL, 403U, false};
    Fixture denied = {&denied_activity, 1U, 0U, true};
    CHECK(!runner_github_fetch("/x", PRIVATE_TOKEN, RUNNER_GITHUB_JOBS,
                              get, &denied, error, sizeof(error)));
    CHECK(strstr(error, "Actions: Read") && strstr(error, "repositories") &&
          !strstr(error, "Self-hosted runners") && !strstr(error, PRIVATE_TOKEN));
    Page partial[] = {
        {"https://api.github.com/x", "{\"runners\":[]}", "<https://api.github.com/x?page=2>; rel=next", 200U, false},
        {"https://api.github.com/x?page=2", PRIVATE_TOKEN, NULL, 0U, true}
    };
    Fixture fixture = {partial, 2U, 0U, true};
    CHECK(!runner_github_fetch("/x", PRIVATE_TOKEN, RUNNER_GITHUB_RUNNERS,
                              get, &fixture, error, sizeof(error)));
    CHECK(fixture.calls == 2U && fixture.valid && !strstr(error, PRIVATE_TOKEN));
    partial[1].failed = false;
    partial[1].status = 200U;
    partial[1].body = "{\"unexpected\":[]}";
    fixture.calls = 0U;
    CHECK(!runner_github_fetch("/x", PRIVATE_TOKEN, RUNNER_GITHUB_RUNNERS,
                              get, &fixture, error, sizeof(error)));
    const char *bad_links[] = {
        "<https://evil.example/x>; rel=next", "<http://api.github.com/x>; rel=\"next\"",
        "<https://api.github.com/x>; rel=next", "<https://api.github.com/x> rel=next",
        "<https://api.github.com/x?page=2>; rel=next, <https://api.github.com/x?page=3>; rel=next"
    };
    for (size_t i = 0U; i < sizeof(bad_links) / sizeof(bad_links[0]); i++) {
        partial[0].link = bad_links[i];
        fixture.calls = 0U;
        CHECK(!runner_github_fetch("/x", PRIVATE_TOKEN, RUNNER_GITHUB_RUNNERS,
                                  get, &fixture, error, sizeof(error)));
        CHECK(fixture.calls == 1U && error[0]);
    }
    return true;
}

typedef struct { size_t calls; } ManyPages;

static bool many_pages(const char *url, const char *token, RunnerHttpResponse *response,
                       char *error, size_t error_size, void *context)
{
    (void)url; (void)token; (void)error; (void)error_size;
    ManyPages *fixture = context;
    fixture->calls++;
    char link[160];
    (void)snprintf(link, sizeof(link), "<https://api.github.com/x?page=%zu>; rel=next", fixture->calls + 1U);
    response->status = 200U;
    response->body = copy("{\"runners\":[]}");
    response->link = copy(link);
    return true;
}

static bool bounds(void)
{
    char error[320];
    ManyPages fixture = {0U};
    CHECK(!runner_github_fetch("/x?page=1", PRIVATE_TOKEN, RUNNER_GITHUB_RUNNERS,
                              many_pages, &fixture, error, sizeof(error)));
    CHECK(fixture.calls == 100U && strstr(error, "limit"));

    const size_t size = 9U * 1024U * 1024U;
    char *body = malloc(size + 1U);
    CHECK(body);
    memset(body, ' ', size);
    const char *prefix = "{\"runners\":[]}";
    memcpy(body, prefix, strlen(prefix));
    body[size] = '\0';
    const Page pages[] = {
        {"https://api.github.com/x", body, "<https://api.github.com/x?page=2>; rel=next", 200U, false},
        {"https://api.github.com/x?page=2", body, NULL, 200U, false}
    };
    Fixture two_pages = {pages, 2U, 0U, true};
    CHECK(!runner_github_fetch("/x", PRIVATE_TOKEN, RUNNER_GITHUB_RUNNERS,
                              get, &two_pages, error, sizeof(error)));
    CHECK(two_pages.calls == 2U && strstr(error, "size"));
    free(body);

    body = malloc(RUNNER_HTTP_BODY_LIMIT + 2U);
    CHECK(body);
    memset(body, ' ', RUNNER_HTTP_BODY_LIMIT + 1U);
    body[RUNNER_HTTP_BODY_LIMIT + 1U] = '\0';
    CHECK(!runner_github_project(body, RUNNER_GITHUB_RUNNERS, error, sizeof(error)));
    free(body);
    return true;
}

int main(void)
{
    if (!decoding() || !invalid_payloads() || !pagination() ||
        !active_run_filtering() || !refused_requests() || !safe_failures() || !bounds()) return 1;
    RunnerHttpResponse response = {200U, copy("body"), copy("link")};
    runner_http_response_destroy(&response);
    if (response.status || response.body || response.link) return 2;
    puts("Native GitHub projection, pagination, active-run filtering, Settings credentials and safe failures passed.");
    return 0;
}
