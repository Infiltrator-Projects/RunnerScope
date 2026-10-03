# Design

## First-principles position

Runner Monitor should remain understandable, testable and native even while dependencies are progressively reduced.

The application therefore separates provider facts from application interpretation and keeps Common limited to genuinely shared primitives.

## Goals

- compiled native C binaries on Linux and Windows;
- direct, pinned Common integration;
- accurate runner connectivity/busy/active-job interpretation;
- explicit unavailable/error states rather than plausible guesses;
- durable per-user configuration/history;
- local runner health distinct from provider state;
- platform-native presentation without semantic drift;
- dependency changes made directly against the supported native product without parallel replacement shells.

## Provider semantics

GitHub is authoritative for GitHub runner/workflow facts. Runner Monitor owns caching, correlation, timing, presentation and persistence.

The current release line uses GitHub CLI as the authenticated provider adapter. Any future provider replacement must be integrated and qualified in the shipping native path rather than maintained as a second implementation.

## Common usage

Common owns shared palette, design metrics, typography identity, formatting and other product-neutral helpers. Runner Monitor links it directly into each native binary.

Runner-specific HTTP/provider/service policy must not be moved into Common merely to reduce local code.

## UI rule

Linux and Windows may use different native presentation APIs, but running, idle, offline, queued and active must mean the same thing.

The footer must show the Runner Monitor version followed by the linked Common version, with Common as the final line.

## Local service safety

Restarting a runner is an action, not telemetry. It remains separate from passive monitoring and requires explicit operator intent.

## Persistence

Configuration/history writes must use durable publication appropriate to the platform. Corrupt or unreadable state must be surfaced rather than silently replaced.


## Infiltrator OS shell

The Linux shell consumes the current Common semantic palette, typography and
design metrics directly. Screen/content spacing, panel/card/control radii,
titlebar chrome, navigation selection, buttons, tooltips and detail surfaces
follow that contract rather than maintaining a second private geometry system.

Repeated telemetry updates update only visible/time-dependent cells and do not
rebuild a visible model when its static provider snapshot is unchanged. Theme
changes reuse one CSS provider and one Common-driven CSS projection; provider
and service work stays off the GTK main thread.


## Graphical presentation contract

Runner Monitor is an Infiltrator OS graphical application, not a text-first GTK data viewer. Linux packages the Common-verified MB Corpo UI, bold and brand faces and refuses to launch the graphical shell if the required UI or brand family is unavailable. Product-local raster artwork supplies navigation wells, page hero imagery and persistent Infiltrator OS identity; Common continues to own semantic palette, structural metrics and typography identity. Tables remain the detailed evidence surface, subordinate to the graphical state overview.

Raster artwork must show recognisable solid objects, material detail and tonal
depth at its displayed size. Saving procedural grids and outline drawings as
PNG does not satisfy this visual requirement. The four workspace identities,
their banners and the sidebar use a coordinated graphite/cyan hardware art
family. The application/desktop icon retains the shared flat cyan identity.
Telemetry refreshes reuse the displayed bitmap; a page change replaces it.

The Linux fleet opens as a responsive grid of bitmap runner cards. Each card
shows identity, OS, a labelled semantic state indicator and a bar measuring
busy time as a fraction of the observed session. The bar is not CPU usage.
Only running cards display current-job text. Selecting a runner reveals all
technical metadata, including labels; a Table switch retains sorting and CSV
export retains all fields. Search covers the same complete metadata in both
views. Snapshot refreshes reuse card widgets and preserve a still-visible
selection; removed or filtered-out selections are cleared.
