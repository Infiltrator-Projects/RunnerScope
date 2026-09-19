# Roadmap

## Current foundation

- proven Python/Tk cross-platform monitor;
- GitHub CLI authenticated provider path;
- runner/job discovery, history, CSV export and local service inspection;
- exact Common-backed native bridge;
- first-party native C model/HTTP/config/Linux-service core with self-tests;
- Debian packaging and release workflows.

## Near-term native milestones

1. prove provider HTTP/authentication and response parsing against the product contract;
2. reach parity for organisation runner state and active-job correlation;
3. preserve configuration/history compatibility;
4. match local Linux runner discovery/service semantics;
5. add a native presentation layer without changing the underlying model;
6. remove the GitHub CLI/Python/Tk runtime only after parity evidence is complete.

## Cross-platform priorities

- isolate Windows/Linux local-service mechanics behind one product state model;
- keep provider semantics platform-neutral;
- preserve System/Day/Night shared appearance behaviour as native shells evolve.

## Longer-term direction

- one first-party native application with minimal runtime dependencies;
- robust offline/provider-error behaviour and history browsing;
- additional runner diagnostics only when provider/local evidence can be represented explicitly.

## Completion rule

The native rewrite is complete when it can replace the compatibility application without losing supported monitoring, action safety, persistence or release behaviour.
