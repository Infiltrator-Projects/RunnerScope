# Shared UI Contract Migration

## Purpose

Runner Monitor currently has two native presentation implementations: GTK on Linux and Win32 on Windows. They share Common, artwork, typography and product semantics, but the application structure is still expressed twice. That leaves room for visual and feature drift whenever one shell evolves without the other.

The target architecture is a small declarative, platform-neutral UI contract with native renderers. Application structure is declared once; GTK and Win32 translate that structure into their own native controls and drawing APIs.

The rule is:

> Application structure lives once; platform mechanics live twice.

This is not a new cross-platform widget toolkit and it must not grow into one. The shared layer describes only Runner Monitor/Infiltrator OS concepts that are genuinely common to both shells.

## Target shape

```text
Runner Monitor application model
            |
            v
   shared declarative UI tree
            |
      +-----+-----+
      |           |
      v           v
 GTK renderer   Win32 renderer
      |           |
      v           v
   GTK widgets   HWND/GDI/WIC
```

The application-facing vocabulary should remain small and stable. Initial primitives are expected to include:

- window/workspace;
- header and footer;
- navigation rail and navigation item;
- page and hero;
- card and metric;
- row/column/grid/stack/scroll containers;
- text, image and separator;
- button, toggle and search/filter input;
- table/list evidence surface;
- status chip/progress/utilisation indicator;
- selection/detail surface.

Runner-specific composites such as the runner fleet card may be built from those primitives or exposed as a project-local composite where that keeps the application definition clearer.

## Non-goals

The migration must not:

- replace GTK or Win32;
- introduce Electron, web rendering, Python/Tk or another compatibility product;
- force every native capability through a lowest-common-denominator API;
- move runner/provider/service semantics into the UI layer;
- create a huge generic widget framework;
- require a big-bang rewrite or long-lived migration branch.

Platform-specific extensions remain possible, but they must be explicit and rare. A platform extension cannot silently redefine shared application structure.

## Ownership boundary

The shared UI contract owns:

- page hierarchy and stable component identifiers;
- component type and parent/child relationships;
- semantic layout intent;
- shared text roles, images, actions and state bindings;
- product-level visibility/enabled rules;
- parity metadata used by tests.

The GTK renderer owns:

- GtkWidget creation and lifetime;
- GTK-specific packing/layout translation;
- signal hookup;
- CSS projection and GTK event-loop details.

The Win32 renderer owns:

- HWND/control creation and lifetime;
- Win32 layout translation;
- GDI/WIC/custom drawing where required;
- message routing, DPI and native window behaviour.

Common continues to own product-neutral palette, structural metrics, typography identity and other genuinely shared primitives. Runner Monitor owns the initial application UI contract. A reusable Infiltrator-wide layer should only move into Common after the vocabulary has proven itself in Runner Monitor and at least one other application.

## Migration method

The migration uses Branch by Abstraction: introduce a stable seam, move existing callers through it, add the new implementations behind the seam, cut over incrementally, then delete superseded direct layout code. Main must remain buildable, testable and releasable at every step.

No long-lived migration branch is permitted. Each tranche should be small enough to merge after Linux and Windows qualification.

### Phase 0 — freeze the contract

Document the current 1.2.39 product structure and identify the common page/component tree. Add machine-readable component IDs and parity assertions before changing rendering behaviour.

### Phase 1 — shared UI model

Add a small C UI model/API, for example:

```text
src/ui/ui.h
src/ui/ui_model.c
src/ui/ui_layout.c
```

The model must be toolkit-neutral and independently testable. It describes the tree; it does not create native widgets.

### Phase 2 — adapter existing shells

Add renderers such as:

```text
src/ui/renderers/ui_gtk.c
src/ui/renderers/ui_win32.c
```

Initially they may bridge the shared model to existing helper code. The visible product must remain unchanged while direct application layout calls are gradually removed from `linux_main.c` and `windows_main.c`.

### Phase 3 — migrate one vertical slice at a time

Recommended order:

1. shell/header/footer;
2. navigation rail and page switching;
3. workspace hero and fleet metrics;
4. runner cards and selection details;
5. cards/table/search controls;
6. Active Jobs;
7. History;
8. Local Health and explicit service actions;
9. Settings and remaining contextual actions.

A slice is complete only when both native renderers consume the same shared definition and CI proves the contract on both platforms.

### Phase 4 — enforce parity

The shared UI tree becomes the source of truth. CI must fail if a required page, action or stable component ID is missing from the contract or unsupported by either renderer.

Tests should validate at least:

- required pages and navigation items;
- component IDs and hierarchy;
- action availability;
- card/table/search/detail surfaces;
- platform renderer coverage for every non-extension component type;
- no direct application-level GTK/Win32 layout creation remains outside renderer/platform modules.

Platform-native rendering tests remain necessary in addition to contract tests.

### Phase 5 — remove duplicate structure

Once all slices are rendered from the shared tree, delete the obsolete duplicated layout/composition code. GTK and Win32 files should contain renderer and platform mechanics, not independently authored product layout.

## Versioning policy

This migration is a substantial internal architecture improvement with the intention of preserving the public product behaviour and data compatibility. Under Semantic Versioning, that fits a MINOR release line rather than a MAJOR release, because no incompatible public API or user-data break is intended.

The migration target is therefore **1.3.x**.

- `1.2.39` remains the last pre-migration parity baseline.
- Documentation and preparatory tests may land on main without pretending the migration is complete.
- `1.3.0` should be published only when the first functional shared UI contract and both native renderers are shipping and qualified.
- Subsequent migration tranches can ship as `1.3.1`, `1.3.2`, etc. while remaining backward compatible.
- If an incompatible public/user-facing contract becomes necessary, stop and reassess the major version rather than hiding the break inside 1.3.x.

A documentation-only release is deliberately avoided; version numbers should represent shipped product state, not merely the existence of a plan.

## Completion criteria

The migration is complete when:

1. one authoritative Runner Monitor UI tree defines the supported product structure;
2. GTK and Win32 render that same tree;
3. CI checks the shared contract and renderer coverage;
4. application-level layout is no longer duplicated between Linux and Windows;
5. platform-specific extensions are explicit and tested;
6. runner semantics, persistence and provider/service behaviour remain unchanged unless separately documented;
7. Linux and Windows releases remain independently native and releasable.

The end state is not identical implementation code. It is one product definition with two native renderers.
