# Decisions

## ADR-001 — Migrate incrementally, not by flag day

**Decision.** Keep the proven compatibility application while a first-party native core is qualified capability by capability.

**Rationale.** A rewrite that loses monitoring semantics or saved history would be regression, not progress.

**Consequence.** Python/Tk and native code coexist temporarily with explicit ownership boundaries.

## ADR-002 — The native target owns provider mechanics directly

**Decision.** The first-party native product will not depend on GitHub CLI, libcurl or a third-party TLS stack as runtime application dependencies.

**Rationale.** Provider protocol/authentication/output should not be redefined by helper-tool releases.

**Consequence.** Native HTTP/TLS/provider work has a higher implementation/testing burden and must be qualified before replacing the compatibility path.

## ADR-003 — Common is the only shared project dependency

**Decision.** The native product may use exact pinned Common for product-neutral primitives but not another shared Infiltrator application library.

**Rationale.** Runner-specific provider and service semantics belong in Runner Monitor.

**Consequence.** Common is consumed as-is; Runner Monitor does not distort Common to absorb project policy.

## ADR-004 — Cloud state and local service state are separate

**Decision.** GitHub registration/job state and local runner-service health are modelled independently.

**Rationale.** Either side can be stale/unavailable while the other remains observable.

**Consequence.** The UI can explain mismatches instead of collapsing them into one status light.

## ADR-005 — Restart is an explicit control action

**Decision.** Local service restart requires deliberate operator action and must not occur as automatic monitoring recovery.

**Rationale.** Restarting can interrupt an active job.

**Consequence.** Monitoring remains passive by default and the action path checks/communicates relevant state.

## ADR-006 — Saved state must survive implementation migration

**Decision.** Configuration/history are user data and cannot be casually invalidated by the native rewrite.

**Rationale.** The implementation language is not part of the user's intended data lifecycle.

**Consequence.** Migration compatibility is a release requirement for replacing the compatibility application.
