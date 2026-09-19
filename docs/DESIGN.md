# Design

## First-principles position

Runner Monitor starts from the behaviour it must own. Existing products, research, provider APIs and tools are studied as evidence, then accepted, changed or rejected according to the needs of this project.

## Goals

- make runner state and active-job interpretation predictable
- keep credentials owned by the authenticated GitHub CLI rather than stored by the app
- migrate critical mechanics toward first-party native code without throwing away proven UI behaviour
- persist configuration/history durably

## Non-goals

The monitor is not a replacement for GitHub Actions itself and does not claim that every GitHub API field is always available. Missing provider data remains unavailable rather than invented.

## Dependency and language policy

Prefer first-party C/C++ implementation for native/core behaviour where suitable. Use platform-native services where they provide a stronger documented contract. A dependency or external source must not become an undocumented source of semantic truth.

## Failure and uncertainty

Unavailable, unsupported, uncertain and failed are distinct. Prefer visible uncertainty or refusal to guessed success. Persistent or destructive operations require explicit preconditions and post-verification appropriate to their risk.

## Decision quality

A change should improve correctness, safety, fidelity, performance, usability or maintainability and include a validation method. Newness alone is not a design argument.
