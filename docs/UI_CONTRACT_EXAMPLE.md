# Shared UI Contract Example

This file is illustrative. It is not the final API and does not commit Runner Monitor to a particular macro or builder syntax.

The application should be able to express product structure once in toolkit-neutral C, for example:

```c
UiNode *runnerscope_build_ui(AppModel *model)
{
    return ui_window("runner-monitor",
        ui_shell(
            ui_header("Runner Monitor", "Infiltrator OS"),
            ui_nav(
                ui_nav_item("runners", "Runners", ART_NAV_RUNNERS),
                ui_nav_item("active", "Active jobs", ART_NAV_ACTIVE),
                ui_nav_item("history", "History", ART_NAV_HISTORY),
                ui_nav_item("health", "Local health", ART_NAV_HEALTH)),
            ui_page("runners",
                ui_hero("Runner fleet", ART_HERO_RUNNERS),
                ui_metric_row(
                    ui_metric("total", "Total", BIND_RUNNER_TOTAL),
                    ui_metric("running", "Running", BIND_RUNNER_RUNNING),
                    ui_metric("idle", "Idle", BIND_RUNNER_IDLE),
                    ui_metric("offline", "Offline", BIND_RUNNER_OFFLINE)),
                ui_toolbar(
                    ui_view_toggle("fleet-view", "cards", "table"),
                    ui_search("runner-filter", ACTION_FILTER_RUNNERS)),
                ui_runner_grid("runner-grid", BIND_RUNNERS),
                ui_selection_details("runner-details", BIND_SELECTED_RUNNER)),
            ui_footer(
                ui_action("export", "Export CSV", ACTION_EXPORT_CSV),
                ui_action("refresh", "Refresh", ACTION_REFRESH)))));
}
```

The GTK renderer translates those nodes into GtkWidget trees and GTK signals. The Win32 renderer translates the same nodes into HWND/control/custom-drawing structures and message routing.

The application code does not decide whether a card is a GtkBox/GtkFrame or a custom Win32 child window. It decides that a card exists, what it means, what data it binds to, and which action it triggers.

The contract should prefer semantic intent over pixel-level platform instructions. Shared Common metrics can provide standard spacing, radius and typography roles; renderers translate those roles into toolkit-specific implementation.

A renderer capability table should be machine-testable. Conceptually:

| Component | GTK | Win32 |
| --- | --- | --- |
| window | required | required |
| header | required | required |
| navigation | required | required |
| hero | required | required |
| metric | required | required |
| card | required | required |
| table | required | required |
| search | required | required |
| detail surface | required | required |
| platform extension | explicit only | explicit only |

If a new shared component is added to the product definition, CI should fail until both renderers declare and test support for it.
