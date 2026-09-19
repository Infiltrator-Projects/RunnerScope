# Architecture

## Purpose

Runner Monitor is a cross-platform desktop monitor for GitHub Actions self-hosted runners, workflow activity, local runner health and session history.

## System decomposition

- shared application core
- Windows and Linux launchers
- native C/Common bridge
- GitHub CLI integration
- local systemd/service integration
- configuration/history persistence
- native bridge and Python regression tests

## Ownership boundaries

GitHub/GitHub CLI provide runner/workflow data and authentication mechanisms. Runner Monitor owns interpretation, history, presentation, configuration and local service behaviour. Migration toward first-party native code must not change those semantics accidentally.

Platform APIs, hosted services, research sources and first-party shared libraries provide mechanisms or evidence behind explicit boundaries. They do not silently own the product's interpretation or policy.

## Source of truth

Executable behaviour is defined by code and tests. This document defines architectural ownership and dependency direction. Specialist documents refine narrower domains and must remain consistent with it.

## Change discipline

Keep platform handles/toolkit details out of domain contracts where practical. Keep generic behaviour in its shared owner rather than copying it. Preserve explicit unavailable/unsupported/failure states across layers.

## Specialist documentation

- docs/FIRST_PARTY_NATIVE.md
