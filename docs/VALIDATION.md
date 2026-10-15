# Validation

Runner Monitor validation is a current contract, not a release-by-release diary. Historical release detail belongs in `CHANGELOG.md` and Git; this document defines what must be true now before `main` is considered releasable.

## Source and dependency identity

- `VERSION` is the Runner Monitor release identity.
- The `infiltratr-common` gitlink is the sole exact Common identity. CMake derives the expected Common commit from that gitlink when configuring from Git and rejects a populated submodule at any other commit.
- CI independently verifies that the checked-out Common commit equals the parent repository gitlink.
- The build does not carry a second expected Common version/commit literal.
- Third-party GitHub Actions used by the workflow are pinned to immutable commit SHAs.

## Core configuration contract

The toolkit-neutral configuration test runs on every platform and must prove:

1. canonical defaults;
2. complete schema parsing;
3. deterministic serialisation and parse/serialise round-tripping;
4. duplicate known-key rejection;
5. invalid organisation and interval rejection; and
6. preservation of every unrelated field when only the organisation is changed.

The last point is a regression gate for the pre-1.3.8 Windows Settings defect that rewrote non-Windows settings with hard-coded defaults.

## Linux qualification

The hosted Ubuntu job is both the Linux build gate and the package gate. It must:

1. check out the exact source revision and recursive Common gitlink;
2. build the Debian package from that source;
3. run the package build's full CTest suite, including the native self-test, configuration contract, shared UI-contract test, failure-semantics test and offline GitHub authorization tests;
4. install the exact `.deb` that will be published;
5. execute the installed native self-test and GTK UI self-test under Xvfb;
6. verify ELF/native runtime identity and reject Python/Tk runtime dependencies;
7. verify the installed desktop entry, icons, bitmap artwork and Common-verified MB Corpo faces; and
8. upload that exact qualified package as the release artifact.

There is deliberately no second Linux build whose output is discarded before packaging. The package build is the release build.

## Windows qualification

The hosted Windows x64 job must:

1. check out the exact source revision and recursive Common gitlink;
2. configure and build the Release/x64 executable;
3. run CTest against that build, including the native executable self-test, configuration contract, shared UI tests, Windows durable-config-store test and Windows bounded-process test;
4. prove a short child process is captured successfully and an intentionally long child is terminated/classified as a timeout;
5. prove Windows configuration save/load round-trips all fields and corrupt persisted configuration fails closed;
6. verify the staged executable is AMD64 PE; and
7. upload that same executable as the release artifact.

The release EXE must be the executable that was runtime-qualified. A separate self-hosted cross-build is not a substitute for testing the published binary.

## GitHub connection recovery

Authorization tests use fake child processes and must never change real account
credentials. Linux CTest covers streamed device codes, login versus scope
refresh, active-account probing, failed authorization, cancellation and reaping,
bounded output drain, environment-token overrides and suppressed debug output.
Windows process tests additionally cover incremental output, stdin EOF,
cancellation and descendants retaining output handles; the native self-test
checks device-code parsing and permission-error classification.

Native UI qualification must prove that missing runner permissions provide a
visible Connect GitHub action, the one-time code arrives while authorization is
still running, copying works, and success triggers a new runner request. A
continued HTTP 403 must leave the connection notice and stale data visible.
Closing the authorization dialog or application must cancel and reap the child.
No account is reported connected until the runner endpoint succeeds.

## Shared UI contract

The toolkit-neutral UI layer is the application-structure authority for every migrated surface. Its tests must run without GTK or Win32 and must cover:

- stable page and component identities;
- navigation/header/footer/workspace emission and ordering;
- runner cards, selection details, loading/empty states and utilisation clamping;
- page-transition policy;
- invalid platform/page handling that fails closed rather than silently selecting a plausible page; and
- renderer capability rejection when a required callback is absent.

Native GTK and Win32 qualification remains necessary in addition to shared-contract tests. Shared semantics passing alone is not proof that either native renderer can realise the product.

## Release-state invariant

A successful push to `main` must correspond to exactly one release identity:

- if `v<VERSION>` does not exist, the same CI run qualifies the Linux and Windows artifacts and then publishes them;
- if `v<VERSION>` already exists, it must resolve to the exact `main` commit being validated and a GitHub release for that tag must exist;
- if the version tag points anywhere else, CI fails and requires a version bump before `main` can be considered green.

Release publication no longer depends on commit-message wording or a second workflow. It consumes only artifacts produced by the same successful CI run and targets the exact validated commit.

## Packaging

Debian's `debian/changelog` version must equal `VERSION`. The package build runs all CTest gates through `dh_auto_test`. Font cache maintenance is left to Debian/fontconfig triggers rather than duplicated package maintainer scripts.

## Current parity boundary

Linux currently has the mature runner, active-job, history and local-service surfaces. Windows has the native fleet shell, organisation configuration and live runner-state monitoring; Active Jobs, History and Local Health provider parity remain follow-on work under the 1.3.x UI migration.

A platform feature is complete only when its behaviour is present, its shared product semantics are represented once where applicable, and the native implementation is qualified. A build that merely compiles does not establish parity.

## Remaining architectural hardening

Issue #2 remains the authority for completion of the shared declarative UI migration. Issue #14 now tracks the remaining Linux-side provider/configuration/persistence extraction and the eventual removal of the inline Linux schema/parser after that controller consumes the tested core contract. Windows configuration storage and bounded process execution are no longer inline controller policy.
