# Validation

## Shipping evidence

CI validates the product as native software on both supported platforms.

Linux:
- configures and builds the C application against the pinned Common gitlink;
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

## 1.2.29 forensic qualification

The 1.2.29 Linux pass additionally verifies that the shipping tree has one Common-driven GTK style projection, no temporary forensic workflow or transformation script, the latest pinned Common revision, and no superseded tab-label naming. Provider refresh application was qualified after removing redundant visible-model rebuilds and transfer-time deep copies.

## Parity rule

A platform feature is not considered complete merely because the native binary compiles. Behavioural parity must be demonstrated for runner state, job correlation, history, service controls and persistence before claiming full cross-platform parity.
