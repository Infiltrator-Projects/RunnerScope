# Validation

## Shipping evidence

CI validates the product as native software on both supported platforms.

Linux:
- configures and builds the C application against the pinned Common gitlink;
- builds the first-party core/contracts;
- runs all CTest contracts and the native application self-test;
- verifies the application is an ELF binary;
- rejects Python/Tk runtime linkage;
- builds a clean Debian package;
- rejects Python/Tk package dependencies;
- installs, smoke-tests and purges the exact package.

Windows:
- configures and builds the native Win32 C application against the pinned Common gitlink;
- runs its C/Common self-test;
- stages and uploads the native EXE.

Release publication consumes only those qualified native artifacts.

## Dependency-minimisation evidence

`tools/check-first-party-native.sh` applies the stricter dependency boundary only to `src/native2/` and its contracts. This keeps long-term dependency reduction measurable without redefining the shipping product.

## Parity rule

A platform feature is not considered complete merely because the native binary compiles. Behavioural parity must be demonstrated for runner state, job correlation, history, service controls and persistence before claiming full cross-platform parity.
