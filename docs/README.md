# Documentation

Runner Monitor follows the common Infiltrator documentation baseline.

- [Architecture](ARCHITECTURE.md) — native product structure and ownership.
- [Design](DESIGN.md) — state, Common and cross-platform presentation rules.
- [Shared UI contract migration](UI_CONTRACT_MIGRATION.md) — 1.3.x declarative UI model, GTK/Win32 renderer boundary, migration phases and completion criteria.
- [Decisions](DECISIONS.md) — durable architecture choices.
- [Roadmap](ROADMAP.md) — native parity and 1.3.x migration work.
- [Validation](VALIDATION.md) — current build/package evidence and future UI parity gates.
- [Project README](../README.md) — user-facing overview.
- [Changelog](../CHANGELOG.md) — release history.
- [Contributing](../CONTRIBUTING.md) — engineering rules.
- [Security](../SECURITY.md) — vulnerability policy.

The shipping application is native C on Linux and Windows. There is no supported Python/Tk compatibility product. The 1.3.x target keeps those native renderers while moving shared application structure into one declarative UI contract.
