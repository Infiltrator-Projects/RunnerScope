# Roadmap

## Current foundation

- native C/GTK Linux Runner Monitor restored as the shipping Linux application;
- native C/Win32 Windows Runner Monitor built and released as an EXE;
- exact Common linked directly into both platform binaries;
- Python/Tk product files removed;
- Linux runner/job/history/local-service feature surface restored;
- first-party C model/HTTP/config/Linux-service core retained as a dependency-minimisation track;
- CI builds native Linux and Windows artifacts and package validation rejects Python/Tk runtime dependencies.

## Near-term work

1. bring the Win32 feature surface to parity with Linux for active-job correlation, history and local-service controls;
2. preserve/migrate all existing native configuration/history data;
3. move provider parsing/correlation into shared project-owned C modules where that improves parity;
4. qualify first-party provider HTTP/authentication work against the shipping GitHub CLI adapter;
5. replace helper/runtime dependencies only after parity evidence exists.

## Completion rule

Dependency minimisation is complete when the first-party core can replace the remaining helper/toolkit boundaries without functionality loss. Native C delivery itself is no longer conditional on that work.
