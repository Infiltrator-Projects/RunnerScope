# Contributing

## Engineering standard

Start by identifying which layer owns the behaviour and what evidence will prove the change.

## Required practice

1. Read README.md, docs/ARCHITECTURE.md and docs/DESIGN.md.
2. Search for existing shared/project code before creating a parallel implementation.
3. Keep uncertain or unsupported behaviour explicit.
4. Add regression coverage for changed rules and failure cases.
5. Update roadmap/validation/specialist documents when support boundaries change.

## Verification

Run the repository's normal build/test path and keep relevant CI green. Hardware, live-service or playability claims require corresponding manual evidence.

## Repository policy

main is the working branch. Published tags/releases are immutable source identities.
