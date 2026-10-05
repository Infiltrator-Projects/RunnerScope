# Roadmap

## Current foundation — 1.2.39

- native C/GTK Linux Runner Monitor is the shipping Linux application;
- native C/Win32 Windows Runner Monitor is built and released as an EXE;
- exact Common is linked directly into both platform binaries;
- both platforms use the same Common-verified MB typography and product artwork;
- Python/Tk product files are removed;
- Linux runner/job/history/local-service feature surface is mature;
- the Windows fleet shell has been brought back into the current Infiltrator OS visual family;
- CI builds native Linux and Windows artifacts, executes Windows runtime qualification on hosted x64, and validates the clean Debian package.

The remaining architectural weakness is that GTK and Win32 still author too much application structure independently.

## 1.3.x migration objective

Create one toolkit-neutral declarative Runner Monitor UI contract and make GTK/Win32 native renderers of that contract. Application structure must no longer be independently invented in `linux_main.c` and `windows_main.c`.

The migration follows [UI_CONTRACT_MIGRATION.md](UI_CONTRACT_MIGRATION.md) and uses Branch by Abstraction so main remains releasable throughout.

## Migration tranches

1. Freeze and machine-test the current page/component contract.
2. Add the toolkit-neutral C UI model and stable component IDs.
3. Route shell/header/footer through GTK and Win32 renderers.
4. Route navigation, hero and fleet metrics through the shared tree.
5. Route runner cards, selection details, Cards/Table and search/filter controls.
6. Bring Active Jobs onto the same shared application definition and complete the Windows backend parity required by it.
7. Bring History onto the same shared application definition and complete Windows history parity.
8. Bring Local Health/service actions onto the same shared definition and complete Windows local-service parity.
9. Bring Settings/contextual actions through the contract where they are genuinely shared.
10. Add renderer-coverage and no-direct-layout CI gates, then remove superseded duplicated composition code.

## Version policy

- `1.2.39` is the last pre-migration parity baseline.
- Preparatory documentation/tests may land without a release.
- `1.3.0` is published when the first functional shared contract is actually shipping through both native renderers and qualified in CI.
- Compatible migration slices may continue as `1.3.1`, `1.3.2`, and so on.
- A major version is reserved for an actual incompatible public/user-data contract change, not for an internal refactor.

## Completion rule

Architecture work is complete only when one authoritative UI tree defines the supported Runner Monitor product structure, GTK and Win32 render that tree, CI proves renderer coverage and required component parity, user data remains migration-compatible, and neither platform contains a second independently authored application layout.
