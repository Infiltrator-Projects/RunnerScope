# Validation

## Current automated evidence

CMake builds and tests two native targets:

- `runnerscope-native --self-test`;
- `runnerscope-firstparty-core --self-test` plus `runnerscope-firstparty-contract`.

`tests/test_native_bridge.py` exercises the integration contract around the native bridge. CI/release workflows also build/package the current application.

Strict compiler warnings are treated as errors for native targets.

## Compatibility-application evidence

The Python application remains the user-facing reference while migration is incomplete. Changes that affect behaviour shared with the native path should be compared against the established runner/job/history semantics rather than treating a native self-test as full parity.

## Native evidence layers

1. model/unit contract tests;
2. HTTP/provider parsing tests with controlled responses;
3. configuration/history migration tests;
4. local service tests on real Linux/Windows hosts;
5. provider integration using a real authenticated GitHub account;
6. UI parity/interaction testing.

A lower layer does not prove a higher one.

## Provider testing

Live GitHub behaviour is time-dependent and permission-dependent. Tests should separate deterministic parsing/correlation from live-provider qualification.

A missing permission or API field must be surfaced as unavailable/error, not accepted as proof of no active job.

## Local action testing

Service restart tests must avoid interrupting unrelated production runners. Active-job detection and confirmation behaviour are part of action safety.

## Release criterion

Until native parity is established, release validation must cover the compatibility product plus any packaged native bridge used by it. A future native-only release requires explicit evidence for provider, persistence and local-service parity.

## Regression rule

Every migration defect that changes runner/job interpretation, history, configuration or restart safety should become a permanent cross-implementation regression test where practical.
