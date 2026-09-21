// SPDX-License-Identifier: GPL-3.0-or-later
#include "model.h"

#include <string.h>

void rs_runner_session_init(RsRunnerSession *session,
                            RsRunnerState state,
                            double now_seconds)
{
    if (!session) return;
    memset(session, 0, sizeof(*session));
    session->state = state;
    session->state_since = now_seconds;
    if (state == RS_RUNNER_RUNNING) {
        session->busy_started = now_seconds;
        session->busy_active = true;
        session->jobs_started = 1U;
    }
}

bool rs_runner_session_apply(RsRunnerSession *session,
                             RsRunnerState next_state,
                             double now_seconds)
{
    if (!session || now_seconds < session->state_since)
        return false;
    if (session->state == next_state)
        return true;

    if (session->state == RS_RUNNER_RUNNING && session->busy_active) {
        if (now_seconds < session->busy_started)
            return false;
        session->accumulated_busy += now_seconds - session->busy_started;
        session->busy_started = 0.0;
        session->busy_active = false;
    }

    if (next_state == RS_RUNNER_RUNNING) {
        session->busy_started = now_seconds;
        session->busy_active = true;
        session->jobs_started++;
    }

    session->state = next_state;
    session->state_since = now_seconds;
    return true;
}

double rs_runner_session_busy_seconds(const RsRunnerSession *session,
                                      double now_seconds)
{
    if (!session) return 0.0;
    double busy = session->accumulated_busy;
    if (session->state == RS_RUNNER_RUNNING &&
        session->busy_active &&
        now_seconds >= session->busy_started)
        busy += now_seconds - session->busy_started;
    return busy;
}

double rs_runner_session_busy_percent(const RsRunnerSession *session,
                                      double session_started,
                                      double now_seconds)
{
    if (!session || now_seconds <= session_started) return 0.0;
    const double elapsed = now_seconds - session_started;
    double percent =
        rs_runner_session_busy_seconds(session, now_seconds) * 100.0 / elapsed;
    if (percent < 0.0) return 0.0;
    if (percent > 100.0) return 100.0;
    return percent;
}

const char *rs_runner_state_name(RsRunnerState state)
{
    switch (state) {
    case RS_RUNNER_OFFLINE: return "OFFLINE";
    case RS_RUNNER_IDLE: return "IDLE";
    case RS_RUNNER_RUNNING: return "RUNNING";
    default: return "UNKNOWN";
    }
}
