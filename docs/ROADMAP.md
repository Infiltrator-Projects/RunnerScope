# Roadmap

## Current foundation

- native C/GTK Linux Runner Monitor restored as the shipping Linux application;
- native C/Win32 Windows Runner Monitor built and released as an EXE;
- exact Common linked directly into both platform binaries;
- Python/Tk product files removed;
- Linux runner/job/history/local-service feature surface restored;
- CI builds native Linux and Windows artifacts and package validation rejects Python/Tk runtime dependencies.

## Near-term work

1. bring the Win32 feature surface to parity with Linux for active-job correlation, history and local-service controls;
2. preserve/migrate all existing native configuration/history data;
3. move provider parsing/correlation into project-owned C modules inside the shipping implementation where that improves parity;
4. replace helper/runtime dependencies only when the shipping path retains behavioural parity.

## Completion rule

Architecture work is complete only when the shipping native applications preserve behaviour, responsiveness and release quality without parallel compatibility or experimental product paths.
