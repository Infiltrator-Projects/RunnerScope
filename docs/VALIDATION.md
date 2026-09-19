# Validation

## Evidence model

Compilation, deterministic tests, integration tests and human/physical-environment validation demonstrate different properties and must not be conflated.

## Automated gates

- .github/workflows/ci.yml
- .github/workflows/release.yml

tests/ currently covers the first-party native core and the native bridge. CI also validates the packaged/runtime integration paths.

## Manual/environment evidence

Real organisation permissions, live workflow timing, local runner services and privileged restarts require a configured GitHub account and host service environment.

Manual observations should record the environment and behaviour actually tested; they supplement rather than replace deterministic regression coverage.

## Release criterion

The exact revision intended for release must satisfy its required automated checks and must not document planned or unverified behaviour as complete.

## Regression rule

Reproducible defects should become permanent tests at the narrowest useful layer. As the product grows, validation should grow with the owned behaviour rather than becoming a separate afterthought.
