# Decisions

## ADR-001 — Native C is the product

**Decision.** Runner Monitor ships as compiled C on Linux and Windows.

**Rationale.** The native implementation already existed and directly used Common. Treating Python/Tk as the supported product while a stricter rewrite was underway inverted the intended architecture.

**Consequence.** Python/Tk product entry points are removed. Dependency-reduction work proceeds inside the native line rather than replacing it.

## ADR-002 — Common is linked directly

**Decision.** Shipping binaries link directly to the exact pinned Common revision.

**Rationale.** Common is a shared implementation dependency, not an out-of-process helper service.

**Consequence.** The UI can report the linked Common version directly and cannot silently degrade to a compatibility label.

## ADR-003 — Platform-native shells may differ

**Decision.** Linux may use GTK and Windows may use Win32 while sharing the same product semantics.

**Rationale.** Native executable delivery and behavioural consistency matter more than forcing both systems through one toolkit.

**Consequence.** Platform UI code stays local while runner/provider semantics remain aligned.

## ADR-004 — Dependency minimisation is incremental

**Decision.** `src/native2/` remains the first-party dependency-minimisation core, but it does not replace the shipping application until it has parity.

**Rationale.** Removing GTK/GLib or GitHub CLI is useful only if functionality and correctness are preserved.

**Consequence.** The product remains native C throughout the migration.

## ADR-005 — Cloud and local service state are independent

**Decision.** GitHub registration/job state and local runner-service health are modelled separately.

## ADR-006 — Restart is explicit

**Decision.** Local service restart requires deliberate operator action and is never automatic recovery.

## ADR-007 — Saved state survives architecture work

**Decision.** Configuration/history are user data and cannot be discarded merely because implementation details change.
