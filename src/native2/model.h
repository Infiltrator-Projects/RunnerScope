// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RUNNERSCOPE_NATIVE_MODEL_H
#define RUNNERSCOPE_NATIVE_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    RS_RUNNER_OFFLINE = 0,
    RS_RUNNER_IDLE = 1,
    RS_RUNNER_RUNNING = 2
} RsRunnerState;

typedef struct {
    RsRunnerState state;
    double state_since;
    double busy_started;
    bool busy_active;
    double accumulated_busy;
    uint64_t jobs_started;
} RsRunnerSession;

void rs_runner_session_init(RsRunnerSession *session,
                            RsRunnerState state,
                            double now_seconds);

bool rs_runner_session_apply(RsRunnerSession *session,
                             RsRunnerState next_state,
                             double now_seconds);

double rs_runner_session_busy_seconds(const RsRunnerSession *session,
                                      double now_seconds);

double rs_runner_session_busy_percent(const RsRunnerSession *session,
                                      double session_started,
                                      double now_seconds);

const char *rs_runner_state_name(RsRunnerState state);

#endif
