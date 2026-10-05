# Decisions

## ADR-001 — Native C is the product

**Decision.** Runner Monitor ships as compiled C on Linux and Windows.

**Rationale.** The native implementation already existed and directly used Common. Treating Python/Tk as the supported product while a stricter rewrite was underway inverted the intended architecture.

**Consequence.** Python/Tk product entry points are removed. Dependency-reduction work proceeds inside the native line rather than replacing it.

## ADR-002 — Common is linked directly

**Decision.** Shipping binaries link directly to the exact pinned Common revision.

**Rationale.** Common is a shared implementation dependency, not an out-of-process helper service.

**Consequence.** The UI can report the linked Common version directly and cannot silently degrade to a compatibility label.

## ADR-003 — Platform-native rendering, shared product structure

**Decision.** Linux continues to render through GTK and Windows through Win32, but the application structure is being moved to one shared declarative UI contract.

**Rationale.** Native delivery does not require independently authored product layouts. The 1.2.x split allowed the two shells to drift even though they shared Common, artwork and semantics.

**Consequence.** The 1.3.x target has one toolkit-neutral UI tree defining pages, component identity, layout intent, actions and state bindings, with GTK and Win32 acting as native renderers/adapters. Platform-only extensions must be explicit and tested.

## ADR-004 — One implementation per shipping platform

**Decision.** Experimental replacement cores do not live beside the shipping product.

**Rationale.** Parallel implementations duplicate semantics, tests and maintenance while making it easier for behaviour to drift. Dependency or provider improvements belong in the shipping native path once they are ready.

**Consequence.** Linux and Windows each have one supported renderer/runtime implementation and CI validates those directly.

## ADR-005 — Cloud and local service state are independent

**Decision.** GitHub registration/job state and local runner-service health are modelled separately.

## ADR-006 — Restart is explicit

**Decision.** Local service restart requires deliberate operator action and is never automatic recovery.

## ADR-007 — Saved state survives architecture work

**Decision.** Configuration/history are user data and cannot be discarded merely because implementation details change.

## ADR-008 — Migrate by abstraction, not by long-lived branch

**Decision.** The shared UI contract migration uses Branch by Abstraction: introduce the seam, route existing structure through it incrementally, qualify both native renderers at each tranche, and remove duplicated direct layout only after replacement coverage exists.

**Rationale.** A big-bang rewrite or long-lived migration branch would recreate the divergence problem and weaken continuous qualification.

**Consequence.** Main remains buildable, testable and releasable throughout the migration. The shared UI model is independently testable, and CI gains parity/renderer-coverage checks as the 1.3.x work proceeds.

## ADR-009 — 1.3.x is the UI-contract migration line

**Decision.** The first shipping shared UI contract will start the 1.3.x release line; documentation alone does not justify a product release.

**Rationale.** The migration is a substantial backward-compatible internal architecture improvement, not an intended incompatible public/user-data change.

**Consequence.** 1.2.39 remains the pre-migration parity baseline. 1.3.0 is released when the first functional shared contract and both native renderers ship and pass qualification; later migration slices may ship as 1.3.x patches while compatibility is preserved.
