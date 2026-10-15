# Roadmap

## Current foundation — 1.3.8

- native C/GTK Linux Runner Monitor is the shipping Linux application;
- native C/Win32 Windows Runner Monitor is built, runtime-qualified and released as an EXE;
- the exact Common dependency is the repository gitlink and is linked directly into both native binaries;
- both platforms use the same Common-verified MB typography and product artwork;
- Python/Tk product files are removed;
- Linux runner/job/history/local-service functionality is mature;
- the Windows fleet shell is in the current Infiltrator OS visual family;
- the toolkit-neutral UI contract owns the migrated navigation, shell chrome, page transitions, workspace summary and runner-card/detail semantics;
- `src/core/runner_config.c` owns the tested extracted configuration schema/validation/serialisation contract;
- Windows durable configuration storage is isolated from the Win32 controller and preserves unrelated configuration fields on Settings saves;
- Windows provider process execution is isolated, bounded and timeout-tested;
- hosted CI builds and tests the exact Debian package and exact Windows EXE that it publishes; and
- `VERSION`, the release tag and the validated `main` commit are mechanically kept in sync.

Two architectural migrations remain. Issue #2 owns completion of the shared UI definition. Issue #14 now primarily owns migration of the Linux controller onto the extracted configuration/provider/persistence boundaries and removal of its superseded inline policy.

## 1.3.x UI migration objective

Create one toolkit-neutral declarative Runner Monitor UI contract and make GTK/Win32 native renderers of that contract. Application structure must no longer be independently invented in `linux_main.c` and `windows_main.c`.

The migration follows [UI_CONTRACT_MIGRATION.md](UI_CONTRACT_MIGRATION.md) and uses Branch by Abstraction so `main` remains releasable throughout.

## UI migration status

Completed:

1. freeze and machine-test the page/component contract;
2. add the toolkit-neutral C UI model and stable component IDs;
3. route navigation through native renderer adapters;
4. route product header/general footer through the shared renderer seam;
5. route page-transition policy through the shared contract;
6. route workspace page identity/hero/metrics through shared emission; and
7. route runner-card presentation and selected-runner details through shared emission.

Remaining:

8. route Cards/Table and search/filter composition through the shared definition;
9. bring Active Jobs onto the shared definition and complete the Windows backend parity required by it;
10. bring History onto the shared definition and complete Windows history parity;
11. bring Local Health/service actions onto the shared definition and complete Windows local-service parity;
12. bring Settings/contextual actions through the contract where they are genuinely shared; and
13. add renderer-coverage/no-direct-layout CI gates, then delete superseded duplicated composition code.

## Controller hardening objective

Issue #14 is a separate ownership cleanup, not a second implementation. The first extraction is complete on Windows: configuration schema semantics live in `runner_config`, durable Windows storage lives in `windows_config_store`, and bounded provider process execution lives in `windows_process`. Refresh workers consume immutable request snapshots rather than mutable UI-owned configuration.

The remaining hardening work is to migrate Linux onto those same application contracts, extract Linux provider/persistence mechanics behind native adapters, surface Linux persisted-state failures consistently, and delete the superseded inline parser/process policy as each slice becomes authoritative.

## Version policy

- `1.2.39` is the last pre-migration parity baseline.
- `1.3.x` is the incremental shared-contract and maintainability line.
- Every accepted `main` state is expected to be releasable. A `main` change therefore advances `VERSION` before it can be green if the current version tag already exists.
- A major version is reserved for an actual incompatible public/user-data contract change, not for an internal refactor.

## Completion rule

Architecture work is complete only when one authoritative UI tree defines the supported Runner Monitor product structure, GTK and Win32 render that tree, provider/persistence ownership is explicit, CI proves required native and shared contracts, user data remains migration-compatible, and neither platform contains a second independently authored application policy/layout for concerns already migrated.
