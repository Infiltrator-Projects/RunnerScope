#!/usr/bin/env python3
from pathlib import Path


def read(path):
    return Path(path).read_text(encoding="utf-8")


def write(path, text):
    Path(path).write_text(text, encoding="utf-8")


def replace_once(text, old, new, label):
    if text.count(old) != 1:
        raise SystemExit(f"{label}: expected exactly one match, found {text.count(old)}")
    return text.replace(old, new, 1)


def replace_between(text, start, end, replacement, label):
    a = text.find(start)
    if a < 0:
        raise SystemExit(f"{label}: start marker not found")
    b = text.find(end, a)
    if b < 0:
        raise SystemExit(f"{label}: end marker not found")
    return text[:a] + replacement + text[b:]


# ---------------------------------------------------------------------------
# Shared contract: card/grid component identities.
# ---------------------------------------------------------------------------
path = "src/ui/ui_contract.h"
text = read(path)
text = replace_once(
    text,
    "    RUNNER_UI_COMPONENT_PAGE,\n    RUNNER_UI_COMPONENT_SELECTION,\n",
    "    RUNNER_UI_COMPONENT_PAGE,\n    RUNNER_UI_COMPONENT_GRID,\n    RUNNER_UI_COMPONENT_CARD,\n    RUNNER_UI_COMPONENT_SELECTION,\n",
    "ui_contract component kinds",
)
write(path, text)

path = "src/ui/ui_contract.c"
text = read(path)
text = replace_once(
    text,
    "    {\"page.runners\", \"shell.workspace.page-host\", RUNNER_UI_COMPONENT_PAGE},\n"
    "    {\"page.active-jobs\", \"shell.workspace.page-host\", RUNNER_UI_COMPONENT_PAGE},\n",
    "    {\"page.runners\", \"shell.workspace.page-host\", RUNNER_UI_COMPONENT_PAGE},\n"
    "    {\"page.runners.cards\", \"page.runners\", RUNNER_UI_COMPONENT_GRID},\n"
    "    {\"page.runners.cards.runner\", \"page.runners.cards\", RUNNER_UI_COMPONENT_CARD},\n"
    "    {\"page.active-jobs\", \"shell.workspace.page-host\", RUNNER_UI_COMPONENT_PAGE},\n",
    "ui_contract runner card components",
)
write(path, text)


# ---------------------------------------------------------------------------
# Shared renderer API: toolkit-neutral runner items, card presentation and
# selection detail emission.
# ---------------------------------------------------------------------------
path = "src/ui/ui_renderer.h"
text = read(path)
insert = '''typedef enum {
    RUNNER_UI_RUNNER_IDLE = 0,
    RUNNER_UI_RUNNER_RUNNING,
    RUNNER_UI_RUNNER_OFFLINE,
    RUNNER_UI_RUNNER_UNKNOWN
} RunnerUiRunnerTone;

typedef struct {
    const char *name;
    const char *os;
    const char *state;
    const char *repo;
    const char *job;
    const char *runtime;
    const char *state_for;
    const char *jobs;
    const char *busy_pct;
    const char *labels;
    double busy_fraction;
} RunnerUiRunnerItem;

typedef struct {
    const RunnerUiRunnerItem *item;
    const char *state_label;
    RunnerUiRunnerTone tone;
    double busy_fraction;
    bool selected;
    bool show_job;
} RunnerUiRunnerCardSpec;

typedef struct {
    const char *eyebrow;
    const char *title;
    char primary[512];
    char secondary[1024];
} RunnerUiRunnerSelectionSpec;

typedef struct {
    const RunnerUiRunnerItem *items;
    size_t item_count;
    ptrdiff_t selected_index;
    bool refreshing;
    bool filter_active;
} RunnerUiRunnerCardsState;

'''
text = replace_once(
    text,
    "typedef struct {\n    RunnerUiPageId current_page;\n",
    insert + "typedef struct {\n    RunnerUiPageId current_page;\n",
    "ui_renderer runner state types",
)
callbacks = '''
    bool (*begin_runner_cards)(void *context, size_t item_count);
    bool (*runner_card)(void *context,
                        size_t index,
                        const RunnerUiRunnerCardSpec *card);
    bool (*runner_cards_empty)(void *context, const char *message);
    bool (*runner_selection)(void *context,
                             const RunnerUiRunnerSelectionSpec *selection);
    bool (*end_runner_cards)(void *context);
'''
text = replace_once(
    text,
    "    bool (*end_workspace_summary)(void *context);\n\n    bool (*begin_footer)(void *context);\n",
    "    bool (*end_workspace_summary)(void *context);\n" + callbacks + "\n    bool (*begin_footer)(void *context);\n",
    "ui_renderer runner callbacks",
)
decls = '''bool runner_ui_prepare_runner_card(const RunnerUiRunnerItem *item,
                                   bool selected,
                                   RunnerUiRunnerCardSpec *card);
bool runner_ui_prepare_runner_selection(
    const RunnerUiRunnerItem *item,
    RunnerUiRunnerSelectionSpec *selection);
bool runner_ui_render_runner_cards(const RunnerUiRenderer *renderer,
                                   const RunnerUiRunnerCardsState *state);
'''
text = replace_once(
    text,
    "bool runner_ui_render_footer(const RunnerUiRenderer *renderer,\n",
    decls + "bool runner_ui_render_footer(const RunnerUiRenderer *renderer,\n",
    "ui_renderer runner declarations",
)
write(path, text)

path = "src/ui/ui_renderer.c"
text = read(path)
text = replace_once(
    text,
    '#include "ui_renderer.h"\n',
    '#include "ui_renderer.h"\n\n#include <stdio.h>\n#include <string.h>\n',
    "ui_renderer includes",
)
runner_impl = r'''
static const char *runner_ui_text(const char *value)
{
    return value && value[0] ? value : "—";
}

static bool runner_ui_meaningful(const char *value)
{
    return value && value[0] && strcmp(value, "—") != 0;
}

bool runner_ui_prepare_runner_card(const RunnerUiRunnerItem *item,
                                   bool selected,
                                   RunnerUiRunnerCardSpec *card)
{
    if (!item || !card) return false;
    memset(card, 0, sizeof(*card));
    card->item = item;
    card->selected = selected;
    card->busy_fraction = item->busy_fraction;
    if (card->busy_fraction < 0.0) card->busy_fraction = 0.0;
    if (card->busy_fraction > 1.0) card->busy_fraction = 1.0;

    if (item->state && strcmp(item->state, "RUNNING") == 0) {
        card->tone = RUNNER_UI_RUNNER_RUNNING;
        card->state_label = "● Running";
    } else if (item->state && strcmp(item->state, "IDLE") == 0) {
        card->tone = RUNNER_UI_RUNNER_IDLE;
        card->state_label = "● Idle";
    } else if (item->state && strcmp(item->state, "OFFLINE") == 0) {
        card->tone = RUNNER_UI_RUNNER_OFFLINE;
        card->state_label = "● Offline";
    } else {
        card->tone = RUNNER_UI_RUNNER_UNKNOWN;
        card->state_label = "● Unknown";
    }
    card->show_job = card->tone == RUNNER_UI_RUNNER_RUNNING &&
                     runner_ui_meaningful(item->job);
    return true;
}

bool runner_ui_prepare_runner_selection(
    const RunnerUiRunnerItem *item,
    RunnerUiRunnerSelectionSpec *selection)
{
    if (!item || !selection) return false;
    memset(selection, 0, sizeof(*selection));
    selection->eyebrow = "Selected runner";
    selection->title = runner_ui_text(item->name);
    (void)snprintf(
        selection->primary, sizeof(selection->primary),
        "%s  •  %s for %s  •  %s jobs  •  %s session utilisation",
        runner_ui_text(item->os), runner_ui_text(item->state),
        runner_ui_text(item->state_for), runner_ui_text(item->jobs),
        runner_ui_text(item->busy_pct));
    (void)snprintf(
        selection->secondary, sizeof(selection->secondary),
        "Repository: %s  •  Job: %s  •  Runtime: %s\nLabels: %s",
        runner_ui_text(item->repo), runner_ui_text(item->job),
        runner_ui_text(item->runtime), runner_ui_text(item->labels));
    selection->primary[sizeof(selection->primary) - 1U] = '\0';
    selection->secondary[sizeof(selection->secondary) - 1U] = '\0';
    return true;
}

bool runner_ui_render_runner_cards(const RunnerUiRenderer *renderer,
                                   const RunnerUiRunnerCardsState *state)
{
    if (!renderer || !renderer->begin_runner_cards || !renderer->runner_card ||
        !renderer->runner_cards_empty || !renderer->runner_selection ||
        !renderer->end_runner_cards || !state)
        return false;
    if (state->item_count != 0U && !state->items) return false;
    if (state->selected_index < -1 ||
        (state->selected_index >= 0 &&
         (size_t)state->selected_index >= state->item_count))
        return false;
    if (!renderer->begin_runner_cards(renderer->context, state->item_count))
        return false;

    if (state->item_count == 0U) {
        const char *message = state->refreshing
            ? "Loading runners…"
            : state->filter_active
                ? "No runners match this filter."
                : "No runners detected.";
        if (!renderer->runner_cards_empty(renderer->context, message))
            return false;
        return renderer->end_runner_cards(renderer->context);
    }

    for (size_t i = 0U; i < state->item_count; ++i) {
        RunnerUiRunnerCardSpec card;
        if (!runner_ui_prepare_runner_card(
                &state->items[i], (ptrdiff_t)i == state->selected_index, &card))
            return false;
        if (!renderer->runner_card(renderer->context, i, &card))
            return false;
    }

    if (state->selected_index >= 0) {
        RunnerUiRunnerSelectionSpec selection;
        if (!runner_ui_prepare_runner_selection(
                &state->items[state->selected_index], &selection))
            return false;
        if (!renderer->runner_selection(renderer->context, &selection))
            return false;
    }

    return renderer->end_runner_cards(renderer->context);
}

'''
text = replace_once(
    text,
    "bool runner_ui_render_footer(const RunnerUiRenderer *renderer,\n",
    runner_impl + "bool runner_ui_render_footer(const RunnerUiRenderer *renderer,\n",
    "ui_renderer runner implementation",
)
write(path, text)


# ---------------------------------------------------------------------------
# Linux: adapt the existing GtkFlowBox card implementation and selection card
# to the shared runner-card presentation without changing provider behaviour.
# ---------------------------------------------------------------------------
path = "src/linux_main.c"
text = read(path)
item_helper = r'''static RunnerUiRunnerItem runner_ui_item_from_row(const RunnerRow *row)
{
    RunnerUiRunnerItem item = {0};
    if (!row) return item;
    item.name = row->name;
    item.os = row->os;
    item.state = row->state;
    item.repo = row->repo;
    item.job = row->job;
    item.runtime = row->runtime;
    item.state_for = row->state_for;
    item.jobs = row->jobs;
    item.busy_pct = row->busy_pct;
    item.labels = row->labels;
    item.busy_fraction = CLAMP(
        g_ascii_strtod(row->busy_pct ? row->busy_pct : "0", NULL) / 100.0,
        0.0, 1.0);
    return item;
}

'''
text = replace_once(
    text,
    "static void clear_selection_card(RunnerScopeApp *app)\n",
    item_helper + "static void clear_selection_card(RunnerScopeApp *app)\n",
    "linux runner item helper",
)
new_show = r'''static void show_runner_details(RunnerScopeApp *app, const RunnerRow *row)
{
    if (!row) return;
    RunnerUiRunnerItem item = runner_ui_item_from_row(row);
    RunnerUiRunnerSelectionSpec selection;
    if (!runner_ui_prepare_runner_selection(&item, &selection)) return;
    show_selection_card(app, selection.title,
                        selection.primary, selection.secondary);
}

'''
text = replace_between(
    text,
    "static void show_runner_details(RunnerScopeApp *app, const RunnerRow *row)\n",
    "static void on_runner_card_selection",
    new_show,
    "linux shared runner selection",
)
card_block = r'''static GtkWidget *runner_card_label(const char *text, const char *css_class)
{
    GtkWidget *label = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0F);
    gtk_style_context_add_class(gtk_widget_get_style_context(label), css_class);
    return label;
}

static const char *runner_tone_css(RunnerUiRunnerTone tone)
{
    switch (tone) {
        case RUNNER_UI_RUNNER_RUNNING: return "running";
        case RUNNER_UI_RUNNER_IDLE: return "idle";
        case RUNNER_UI_RUNNER_OFFLINE: return "offline";
        default: return "offline";
    }
}

static GtkWidget *make_runner_card(RunnerScopeApp *app,
                                   const RunnerUiRunnerCardSpec *spec)
{
    const RunnerUiRunnerItem *item = spec->item;
    GtkWidget *child = gtk_flow_box_child_new();
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_size_request(card, 205, -1);
    gtk_style_context_add_class(gtk_widget_get_style_context(card), "runner-card");
    gtk_container_add(GTK_CONTAINER(child), card);
    g_object_set_data_full(
        G_OBJECT(child), "runner-name", g_strdup(item->name), g_free);

    GtkWidget *identity = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *art = gtk_image_new_from_pixbuf(app->runner_card_art);
    GtkWidget *copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *name = runner_card_label(item->name, "runner-card-name");
    gtk_label_set_ellipsize(GTK_LABEL(name), PANGO_ELLIPSIZE_END);
    gtk_label_set_width_chars(GTK_LABEL(name), 15);
    GtkWidget *os = runner_card_label(item->os, "runner-card-os");
    gtk_box_pack_start(GTK_BOX(copy), name, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(copy), os, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(identity), art, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(identity), copy, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(card), identity, FALSE, FALSE, 0);

    GtkWidget *status_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *state = runner_card_label("", "runner-state");
    GtkWidget *bar = gtk_progress_bar_new();
    gtk_widget_set_hexpand(bar, TRUE);
    gtk_widget_set_valign(bar, GTK_ALIGN_CENTER);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(bar), "runner-utilisation");
    gtk_box_pack_start(GTK_BOX(status_row), state, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(status_row), bar, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(card), status_row, FALSE, FALSE, 0);

    GtkWidget *job = runner_card_label("", "runner-card-job");
    gtk_label_set_ellipsize(GTK_LABEL(job), PANGO_ELLIPSIZE_END);
    gtk_widget_set_no_show_all(job, TRUE);
    gtk_box_pack_start(GTK_BOX(card), job, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(child), "runner-os", os);
    g_object_set_data(G_OBJECT(child), "runner-state", state);
    g_object_set_data(G_OBJECT(child), "runner-utilisation", bar);
    g_object_set_data(G_OBJECT(child), "runner-job", job);
    gtk_flow_box_insert(GTK_FLOW_BOX(app->runner_cards), child, -1);
    return child;
}

static void update_runner_card_spec(
    GtkWidget *child, const RunnerUiRunnerCardSpec *spec)
{
    const RunnerUiRunnerItem *item = spec->item;
    GtkWidget *state = g_object_get_data(G_OBJECT(child), "runner-state");
    GtkWidget *bar = g_object_get_data(G_OBJECT(child), "runner-utilisation");
    GtkWidget *job = g_object_get_data(G_OBJECT(child), "runner-job");
    GtkWidget *os = g_object_get_data(G_OBJECT(child), "runner-os");
    label_set_if_changed(os, item->os);
    label_set_if_changed(state, spec->state_label);
    GtkStyleContext *context = gtk_widget_get_style_context(state);
    const char *state_class = runner_tone_css(spec->tone);
    if (!gtk_style_context_has_class(context, state_class)) {
        gtk_style_context_remove_class(context, "running");
        gtk_style_context_remove_class(context, "idle");
        gtk_style_context_remove_class(context, "offline");
        gtk_style_context_add_class(context, state_class);
    }
    if (gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(bar)) != spec->busy_fraction)
        gtk_progress_bar_set_fraction(
            GTK_PROGRESS_BAR(bar), spec->busy_fraction);
    char *tooltip = g_strdup_printf(
        "%s\nSession utilisation: %s", item->name, item->busy_pct);
    gtk_widget_set_tooltip_text(child, tooltip);
    gtk_widget_set_tooltip_text(bar, tooltip);
    g_free(tooltip);
    label_set_if_changed(job, item->job);
    gtk_widget_show_all(child);
    gtk_widget_set_visible(job, spec->show_job);
}

static void update_runner_card(GtkWidget *child, const RunnerRow *row)
{
    RunnerUiRunnerItem item = runner_ui_item_from_row(row);
    RunnerUiRunnerCardSpec spec;
    if (runner_ui_prepare_runner_card(&item, FALSE, &spec))
        update_runner_card_spec(child, &spec);
}

typedef struct {
    RunnerScopeApp *app;
    bool selection_emitted;
} GtkRunnerCardsRendererContext;

static bool gtk_renderer_begin_runner_cards(void *context, size_t item_count)
{
    (void)item_count;
    GtkRunnerCardsRendererContext *renderer = context;
    if (!renderer || !renderer->app) return false;
    renderer->selection_emitted = false;
    gtk_widget_hide(renderer->app->runner_cards_empty);
    return true;
}

static bool gtk_renderer_runner_card(
    void *context, size_t index, const RunnerUiRunnerCardSpec *spec)
{
    (void)index;
    GtkRunnerCardsRendererContext *renderer = context;
    if (!renderer || !renderer->app || !spec || !spec->item) return false;
    RunnerScopeApp *app = renderer->app;
    GtkWidget *card = g_hash_table_lookup(
        app->runner_card_by_name, spec->item->name);
    if (!card) {
        card = make_runner_card(app, spec);
        g_hash_table_insert(
            app->runner_card_by_name, g_strdup(spec->item->name), card);
    }
    update_runner_card_spec(card, spec);
    if (spec->selected) {
        renderer->selection_emitted = true;
        gtk_flow_box_select_child(
            GTK_FLOW_BOX(app->runner_cards), GTK_FLOW_BOX_CHILD(card));
        GtkTreeIter iter;
        gboolean valid = gtk_tree_model_get_iter_first(
            GTK_TREE_MODEL(app->runner_store), &iter);
        while (valid) {
            char *name = NULL;
            gtk_tree_model_get(
                GTK_TREE_MODEL(app->runner_store), &iter,
                RUNNER_COL_NAME, &name, -1);
            const gboolean match =
                g_strcmp0(name, spec->item->name) == 0;
            g_free(name);
            if (match) {
                gtk_tree_selection_select_iter(
                    gtk_tree_view_get_selection(GTK_TREE_VIEW(app->runner_tree)),
                    &iter);
                break;
            }
            valid = gtk_tree_model_iter_next(
                GTK_TREE_MODEL(app->runner_store), &iter);
        }
    }
    return true;
}

static bool gtk_renderer_runner_cards_empty(void *context, const char *message)
{
    GtkRunnerCardsRendererContext *renderer = context;
    if (!renderer || !renderer->app || !message) return false;
    label_set_if_changed(renderer->app->runner_cards_empty, message);
    gtk_widget_show(renderer->app->runner_cards_empty);
    return true;
}

static bool gtk_renderer_runner_selection(
    void *context, const RunnerUiRunnerSelectionSpec *selection)
{
    GtkRunnerCardsRendererContext *renderer = context;
    if (!renderer || !renderer->app || !selection) return false;
    renderer->selection_emitted = true;
    show_selection_card(renderer->app, selection->title,
                        selection->primary, selection->secondary);
    return true;
}

static bool gtk_renderer_end_runner_cards(void *context)
{
    GtkRunnerCardsRendererContext *renderer = context;
    if (!renderer || !renderer->app) return false;
    if (!renderer->selection_emitted) {
        g_clear_pointer(&renderer->app->selected_runner_name, g_free);
        gtk_flow_box_unselect_all(GTK_FLOW_BOX(renderer->app->runner_cards));
        clear_selection_card(renderer->app);
    }
    return true;
}

'''
text = replace_between(
    text,
    "static GtkWidget *runner_card_label(const char *text, const char *css_class)\n",
    "static void on_runner_view_toggled",
    card_block,
    "linux runner card renderer block",
)
new_render = r'''static void render_runners(RunnerScopeApp *app)
{
    char *folded_needle = filter_needle_casefold(app);
    app->runner_rendering = TRUE;
    GHashTableIter cards;
    gpointer card_name, card_widget;
    g_hash_table_iter_init(&cards, app->runner_card_by_name);
    while (g_hash_table_iter_next(&cards, &card_name, &card_widget)) {
        if (!g_hash_table_contains(app->runner_row_by_name, card_name)) {
            gtk_widget_destroy(card_widget);
            g_hash_table_iter_remove(&cards);
        } else {
            gtk_widget_hide(card_widget);
        }
    }

    GArray *items = g_array_new(FALSE, FALSE, sizeof(RunnerUiRunnerItem));
    ptrdiff_t selected_index = -1;
    tree_model_rebuild_begin(app->runner_tree);
    gtk_list_store_clear(app->runner_store);
    for (guint i = 0U; i < app->runner_rows->len; i++) {
        RunnerRow *row = g_ptr_array_index(app->runner_rows, i);
        char *search = g_strdup_printf(
            "%s %s %s %s %s %s", row->name, row->os,
            row->state, row->repo, row->job, row->labels);
        const gboolean visible = text_matches_folded(folded_needle, search);
        g_free(search);
        if (!visible) continue;

        RunnerUiRunnerItem item = runner_ui_item_from_row(row);
        if (g_strcmp0(row->name, app->selected_runner_name) == 0)
            selected_index = (ptrdiff_t)items->len;
        g_array_append_val(items, item);

        GtkTreeIter iter;
        gtk_list_store_append(app->runner_store, &iter);
        gtk_list_store_set(app->runner_store, &iter,
            RUNNER_COL_NAME, row->name, RUNNER_COL_OS, row->os,
            RUNNER_COL_STATE, row->state, RUNNER_COL_REPO, row->repo,
            RUNNER_COL_JOB, row->job, RUNNER_COL_RUNTIME, row->runtime,
            RUNNER_COL_STATE_FOR, row->state_for, RUNNER_COL_JOBS, row->jobs,
            RUNNER_COL_BUSY, row->busy_pct, RUNNER_COL_LABELS, row->labels, -1);
    }
    tree_model_rebuild_end(app->runner_tree, app->runner_store);

    GtkRunnerCardsRendererContext context = {app, false};
    RunnerUiRenderer renderer = {0};
    renderer.context = &context;
    renderer.begin_runner_cards = gtk_renderer_begin_runner_cards;
    renderer.runner_card = gtk_renderer_runner_card;
    renderer.runner_cards_empty = gtk_renderer_runner_cards_empty;
    renderer.runner_selection = gtk_renderer_runner_selection;
    renderer.end_runner_cards = gtk_renderer_end_runner_cards;
    RunnerUiRunnerCardsState state = {
        (const RunnerUiRunnerItem *)items->data,
        items->len,
        selected_index,
        g_atomic_int_get(&app->runner_refreshing) != 0,
        folded_needle && *folded_needle
    };
    if (!runner_ui_render_runner_cards(&renderer, &state))
        g_warning("Unable to render the shared runner-card contract.");

    g_array_unref(items);
    app->runner_rendering = FALSE;
    g_free(folded_needle);
}

'''
text = replace_between(
    text,
    "static void render_runners(RunnerScopeApp *app)\n",
    "static void render_activity(RunnerScopeApp *app)\n",
    new_render,
    "linux render_runners shared emission",
)
write(path, text)


# ---------------------------------------------------------------------------
# Windows: convert native GDI cards and the selection surface into callbacks
# driven by the same shared card/selection specs.
# ---------------------------------------------------------------------------
path = "src/windows_main.c"
text = read(path)
text = replace_once(
    text,
    "    int selection_height = g_selected_row >= 0 && g_page == 0 ? sx(76) : 0;\n",
    "    int selection_height = g_selected_row >= 0 && g_page == 0 ? sx(112) : 0;\n",
    "windows selection height",
)
windows_cards = r'''typedef struct {
    char state_for[64];
    char jobs[32];
    char busy_pct[32];
} WinRunnerUiStorage;

static void prepare_win_runner_item(const RunnerRow *row,
                                    WinRunnerUiStorage *storage,
                                    RunnerUiRunnerItem *item)
{
    ZeroMemory(storage, sizeof(*storage));
    ZeroMemory(item, sizeof(*item));
    strcpy_s(storage->state_for, sizeof(storage->state_for), "—");
    strcpy_s(storage->jobs, sizeof(storage->jobs), "0");
    strcpy_s(storage->busy_pct, sizeof(storage->busy_pct), "0.0%");
    double fraction = 0.0;
    RunnerSession *session = session_for(row->name, false);
    if (session) {
        wchar_t state_for_w[64] = L"—";
        format_duration(GetTickCount64() - session->state_since_ms,
                        state_for_w, 64U);
        (void)wide_to_utf8(state_for_w, storage->state_for,
                           sizeof(storage->state_for));
        (void)snprintf(storage->jobs, sizeof(storage->jobs),
                       "%u", session->jobs);
        fraction = session_busy_fraction(session);
        (void)snprintf(storage->busy_pct, sizeof(storage->busy_pct),
                       "%.1f%%", fraction * 100.0);
    }
    item->name = row->name;
    item->os = row->os;
    item->state = row->state;
    item->repo = "—";
    item->job = "—";
    item->runtime = strcmp(row->state, "RUNNING") == 0
        ? storage->state_for : "—";
    item->state_for = storage->state_for;
    item->jobs = storage->jobs;
    item->busy_pct = storage->busy_pct;
    item->labels = row->labels[0] ? row->labels : "—";
    item->busy_fraction = fraction;
}

static COLORREF runner_tone_color(RunnerUiRunnerTone tone)
{
    if (tone == RUNNER_UI_RUNNER_RUNNING) return rgb(g_palette.success_rgb);
    if (tone == RUNNER_UI_RUNNER_IDLE) return rgb(g_palette.info_rgb);
    return rgb(g_palette.fault_rgb);
}

static void draw_runner_card(HDC dc, const RunnerUiRunnerCardSpec *spec,
                             RECT card)
{
    const RunnerUiRunnerItem *item = spec->item;
    COLORREF border = spec->selected ? rgb(g_palette.neutral_accent_rgb)
                                     : rgb(g_palette.border_rgb);
    fill_round_rect(dc, card,
                    spec->selected
                        ? mix_color(g_palette.card_rgb,
                                    g_palette.neutral_accent_rgb, 10U)
                        : rgb(g_palette.card_rgb),
                    border, sx(12));
    RECT stripe = {card.left + sx(1), card.top + sx(1),
                   card.left + sx(6), card.bottom - sx(1)};
    fill_rect_color(dc, stripe, runner_tone_color(spec->tone));

    RECT icon = {card.left + sx(16), card.top + sx(16),
                 card.left + sx(56), card.top + sx(56)};
    draw_image(dc, &g_nav_images[0], &icon);

    wchar_t name[192], os[64], state[64], labels[512], job[256];
    utf8_to_wide(item->name, name, 192U);
    utf8_to_wide(item->os, os, 64U);
    utf8_to_wide(spec->state_label, state, 64U);
    utf8_to_wide(item->labels, labels, 512U);
    utf8_to_wide(item->job, job, 256U);

    RECT name_rect = {card.left + sx(66), card.top + sx(13),
                      card.right - sx(12), card.top + sx(38)};
    draw_text(dc, name, name_rect, g_font_bold, rgb(g_palette.title_rgb),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT os_rect = {card.left + sx(66), card.top + sx(37),
                    card.right - sx(12), card.top + sx(57)};
    draw_text(dc, os, os_rect, g_font_small, rgb(g_palette.muted_rgb),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT state_rect = {card.left + sx(16), card.top + sx(67),
                       card.left + sx(116), card.top + sx(93)};
    uint32_t accent = spec->tone == RUNNER_UI_RUNNER_RUNNING
        ? g_palette.success_rgb
        : spec->tone == RUNNER_UI_RUNNER_IDLE
            ? g_palette.info_rgb : g_palette.fault_rgb;
    fill_round_rect(dc, state_rect,
                    mix_color(g_palette.surface_rgb, accent, 18U),
                    runner_tone_color(spec->tone), sx(8));
    draw_text(dc, state, state_rect, g_font_bold, rgb(g_palette.text_rgb),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    wchar_t state_for[64], jobs[64], busy[64];
    utf8_to_wide(item->state_for, state_for, 64U);
    wchar_t jobs_value[32], busy_value[32];
    utf8_to_wide(item->jobs, jobs_value, 32U);
    utf8_to_wide(item->busy_pct, busy_value, 32U);
    _snwprintf_s(jobs, 64U, _TRUNCATE, L"Jobs %ls", jobs_value);
    _snwprintf_s(busy, 64U, _TRUNCATE, L"Busy %ls", busy_value);
    RECT state_for_rect = {card.left + sx(126), card.top + sx(67),
                           card.right - sx(14), card.top + sx(91)};
    draw_text(dc, state_for, state_for_rect, g_font_small,
              rgb(g_palette.muted_rgb), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    wchar_t detail[800];
    if (spec->show_job)
        _snwprintf_s(detail, 800U, _TRUNCATE,
                     L"Job: %ls  •  %ls", job, labels);
    else
        wcsncpy_s(detail, 800U, labels[0] ? labels : L"No labels", _TRUNCATE);
    RECT labels_rect = {card.left + sx(16), card.top + sx(99),
                        card.right - sx(16), card.top + sx(121)};
    draw_text(dc, detail, labels_rect, g_font_small,
              rgb(g_palette.detail_label_rgb),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT bar = {card.left + sx(16), card.bottom - sx(31),
                card.right - sx(16), card.bottom - sx(22)};
    fill_round_rect(dc, bar, rgb(g_palette.input_rgb),
                    rgb(g_palette.border_rgb), sx(5));
    RECT progress = bar;
    progress.right = progress.left +
        (int)((double)(bar.right - bar.left) * spec->busy_fraction);
    if (progress.right > progress.left)
        fill_round_rect(dc, progress, rgb(g_palette.operation_rgb),
                        rgb(g_palette.operation_rgb), sx(5));
    RECT jobs_rect = {card.left + sx(16), card.bottom - sx(20),
                      card.left + sx(105), card.bottom - sx(3)};
    RECT busy_rect = {card.right - sx(125), card.bottom - sx(20),
                      card.right - sx(16), card.bottom - sx(3)};
    draw_text(dc, jobs, jobs_rect, g_font_small, rgb(g_palette.muted_rgb),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_text(dc, busy, busy_rect, g_font_small, rgb(g_palette.muted_rgb),
              DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

static int card_columns(void)
{
    int width = g_cards_area_rect.right - g_cards_area_rect.left;
    int card_width = sx(248);
    int gap = sx(10);
    int columns = (width + gap) / (card_width + gap);
    if (columns < 1) columns = 1;
    if (columns > 5) columns = 5;
    return columns;
}

static RECT card_rect_for_visible_index(size_t index)
{
    int columns = card_columns();
    int gap = sx(10);
    int total_width = g_cards_area_rect.right - g_cards_area_rect.left;
    int card_width = (total_width - gap * (columns - 1)) / columns;
    int card_height = sx(154);
    int row = (int)(index / (size_t)columns);
    int column = (int)(index % (size_t)columns);
    int x = g_cards_area_rect.left + column * (card_width + gap);
    int y = g_cards_area_rect.top + row * (card_height + gap) - g_card_scroll_y;
    RECT rect;
    SetRect(&rect, x, y, x + card_width, y + card_height);
    return rect;
}

static int max_card_scroll(void)
{
    size_t count = visible_runner_count();
    int columns = card_columns();
    int rows = (int)((count + (size_t)columns - 1U) / (size_t)columns);
    int content_height = rows > 0 ? rows * sx(154) + (rows - 1) * sx(10) : 0;
    int viewport = g_cards_area_rect.bottom - g_cards_area_rect.top;
    return content_height > viewport ? content_height - viewport : 0;
}

typedef struct {
    HDC dc;
    bool draw_cards;
} WinRunnerCardsRendererContext;

static bool win_renderer_begin_runner_cards(void *context, size_t item_count)
{
    (void)item_count;
    WinRunnerCardsRendererContext *renderer = context;
    return renderer && renderer->dc;
}

static bool win_renderer_runner_card(
    void *context, size_t index, const RunnerUiRunnerCardSpec *spec)
{
    WinRunnerCardsRendererContext *renderer = context;
    if (!renderer || !renderer->dc || !spec) return false;
    if (!renderer->draw_cards) return true;
    RECT card = card_rect_for_visible_index(index);
    if (card.bottom < g_cards_area_rect.top || card.top > g_cards_area_rect.bottom)
        return true;
    int saved = SaveDC(renderer->dc);
    IntersectClipRect(renderer->dc, g_cards_area_rect.left, g_cards_area_rect.top,
                      g_cards_area_rect.right, g_cards_area_rect.bottom);
    draw_runner_card(renderer->dc, spec, card);
    RestoreDC(renderer->dc, saved);
    return true;
}

static bool win_renderer_runner_cards_empty(void *context, const char *message)
{
    WinRunnerCardsRendererContext *renderer = context;
    if (!renderer || !renderer->dc || !message) return false;
    if (!renderer->draw_cards) return true;
    fill_round_rect(renderer->dc, g_cards_area_rect, rgb(g_palette.surface_rgb),
                    rgb(g_palette.border_rgb), sx(10));
    wchar_t text_value[256];
    utf8_to_wide(message, text_value, 256U);
    RECT text = g_cards_area_rect;
    draw_text(renderer->dc, text_value, text, g_font_bold,
              rgb(g_palette.muted_rgb),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    return true;
}

static bool win_renderer_runner_selection(
    void *context, const RunnerUiRunnerSelectionSpec *selection)
{
    WinRunnerCardsRendererContext *renderer = context;
    if (!renderer || !renderer->dc || !selection) return false;
    fill_round_rect(renderer->dc, g_selection_rect,
                    mix_color(g_palette.surface_rgb,
                              g_palette.neutral_accent_rgb, 8U),
                    rgb(g_palette.neutral_accent_rgb), sx(10));
    wchar_t eyebrow[64], title_text[192], primary[768], secondary[1400];
    utf8_to_wide(selection->eyebrow, eyebrow, 64U);
    utf8_to_wide(selection->title, title_text, 192U);
    utf8_to_wide(selection->primary, primary, 768U);
    utf8_to_wide(selection->secondary, secondary, 1400U);
    RECT eyebrow_rect = {
        g_selection_rect.left + sx(16), g_selection_rect.top + sx(6),
        g_selection_rect.right - sx(16), g_selection_rect.top + sx(24)
    };
    draw_text(renderer->dc, eyebrow, eyebrow_rect, g_font_small,
              rgb(g_palette.kicker_rgb),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT title_rect = {
        eyebrow_rect.left, g_selection_rect.top + sx(22),
        eyebrow_rect.right, g_selection_rect.top + sx(46)
    };
    draw_text(renderer->dc, title_text, title_rect, g_font_bold,
              rgb(g_palette.title_rgb),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT primary_rect = {
        eyebrow_rect.left, g_selection_rect.top + sx(45),
        eyebrow_rect.right, g_selection_rect.top + sx(68)
    };
    draw_text(renderer->dc, primary, primary_rect, g_font_small,
              rgb(g_palette.text_rgb),
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT secondary_rect = {
        eyebrow_rect.left, g_selection_rect.top + sx(68),
        eyebrow_rect.right, g_selection_rect.bottom - sx(6)
    };
    draw_text(renderer->dc, secondary, secondary_rect, g_font_small,
              rgb(g_palette.muted_rgb), DT_LEFT | DT_WORDBREAK);
    return true;
}

static bool win_renderer_end_runner_cards(void *context)
{
    return context != NULL;
}

static void draw_cards(HDC dc)
{
    if (g_page != RUNNER_UI_PAGE_RUNNERS) {
        fill_round_rect(dc, g_cards_area_rect, rgb(g_palette.surface_rgb),
                        rgb(g_palette.border_rgb), sx(10));
        RECT text = g_cards_area_rect;
        text.left += sx(30);
        text.right -= sx(30);
        const wchar_t *message =
            g_page == RUNNER_UI_PAGE_ACTIVE_JOBS
                ? L"Active Jobs uses the same Windows parity workspace."
                : g_page == RUNNER_UI_PAGE_HISTORY
                    ? L"History uses the same Windows parity workspace."
                    : L"Local Windows health uses the same Windows parity workspace.";
        draw_text(dc, message, text, g_font_bold, rgb(g_palette.muted_rgb),
                  DT_CENTER | DT_VCENTER | DT_WORDBREAK);
        return;
    }

    const size_t visible = visible_runner_count();
    RunnerUiRunnerItem *items = visible
        ? (RunnerUiRunnerItem *)calloc(visible, sizeof(*items)) : NULL;
    WinRunnerUiStorage *storage = visible
        ? (WinRunnerUiStorage *)calloc(visible, sizeof(*storage)) : NULL;
    if (visible && (!items || !storage)) {
        free(items);
        free(storage);
        return;
    }

    ptrdiff_t selected_index = -1;
    for (size_t i = 0U; i < visible; ++i) {
        size_t source_index = 0U;
        RunnerRow *row = visible_runner_at(i, &source_index);
        if (!row) continue;
        prepare_win_runner_item(row, &storage[i], &items[i]);
        if ((int)source_index == g_selected_row)
            selected_index = (ptrdiff_t)i;
    }

    WinRunnerCardsRendererContext context = {dc, !g_table_view};
    RunnerUiRenderer renderer = {0};
    renderer.context = &context;
    renderer.begin_runner_cards = win_renderer_begin_runner_cards;
    renderer.runner_card = win_renderer_runner_card;
    renderer.runner_cards_empty = win_renderer_runner_cards_empty;
    renderer.runner_selection = win_renderer_runner_selection;
    renderer.end_runner_cards = win_renderer_end_runner_cards;
    RunnerUiRunnerCardsState state = {
        items, visible, selected_index, g_refreshing, g_filter[0] != L'\0'
    };
    (void)runner_ui_render_runner_cards(&renderer, &state);
    free(storage);
    free(items);
}

'''
text = replace_between(
    text,
    "static COLORREF state_color(const char *state)\n",
    "static bool win_renderer_begin_footer(void *context)\n",
    windows_cards,
    "windows shared runner card block",
)
text = replace_once(
    text,
    "    if (!g_table_view || g_page != 0) draw_cards(dc);\n    draw_selection(dc);\n",
    "    draw_cards(dc);\n",
    "windows draw runner surface call",
)
write(path, text)


# ---------------------------------------------------------------------------
# Toolkit-free contract tests for card state, empty state and shared selection
# detail formatting.
# ---------------------------------------------------------------------------
path = "tests/ui_contract_test.c"
text = read(path)
runner_test = r'''typedef struct {
    size_t begin_count;
    size_t expected_items;
    size_t card_count;
    size_t empty_count;
    size_t selection_count;
    size_t end_count;
    RunnerUiRunnerTone tones[4];
    bool selected[4];
    bool show_job[4];
    double fractions[4];
    char state_labels[4][64];
    char empty_message[128];
    char selection_eyebrow[64];
    char selection_title[128];
    char selection_primary[512];
    char selection_secondary[1024];
} RunnerCardsRecorder;

static bool record_begin_runner_cards(void *context, size_t item_count)
{
    RunnerCardsRecorder *recorder = context;
    if (!recorder) return false;
    recorder->begin_count++;
    recorder->expected_items = item_count;
    return true;
}

static bool record_runner_card(
    void *context, size_t index, const RunnerUiRunnerCardSpec *card)
{
    RunnerCardsRecorder *recorder = context;
    if (!recorder || !card || index >= 4U || recorder->card_count != index)
        return false;
    recorder->tones[index] = card->tone;
    recorder->selected[index] = card->selected;
    recorder->show_job[index] = card->show_job;
    recorder->fractions[index] = card->busy_fraction;
    (void)snprintf(recorder->state_labels[index],
                   sizeof(recorder->state_labels[index]),
                   "%s", card->state_label);
    recorder->card_count++;
    return true;
}

static bool record_runner_cards_empty(void *context, const char *message)
{
    RunnerCardsRecorder *recorder = context;
    if (!recorder || !message) return false;
    recorder->empty_count++;
    (void)snprintf(recorder->empty_message,
                   sizeof(recorder->empty_message), "%s", message);
    return true;
}

static bool record_runner_selection(
    void *context, const RunnerUiRunnerSelectionSpec *selection)
{
    RunnerCardsRecorder *recorder = context;
    if (!recorder || !selection) return false;
    recorder->selection_count++;
    (void)snprintf(recorder->selection_eyebrow,
                   sizeof(recorder->selection_eyebrow),
                   "%s", selection->eyebrow);
    (void)snprintf(recorder->selection_title,
                   sizeof(recorder->selection_title),
                   "%s", selection->title);
    (void)snprintf(recorder->selection_primary,
                   sizeof(recorder->selection_primary),
                   "%s", selection->primary);
    (void)snprintf(recorder->selection_secondary,
                   sizeof(recorder->selection_secondary),
                   "%s", selection->secondary);
    return true;
}

static bool record_end_runner_cards(void *context)
{
    RunnerCardsRecorder *recorder = context;
    if (!recorder) return false;
    recorder->end_count++;
    return true;
}

static int verify_runner_cards_renderer(void)
{
    const RunnerUiRunnerItem items[] = {
        {
            .name = "alpha", .os = "Linux", .state = "IDLE",
            .repo = "—", .job = "—", .runtime = "—",
            .state_for = "8m", .jobs = "0", .busy_pct = "10.0%",
            .labels = "self-hosted,linux", .busy_fraction = 0.10
        },
        {
            .name = "bravo", .os = "Windows", .state = "RUNNING",
            .repo = "Example/Project", .job = "Build", .runtime = "2m",
            .state_for = "3m", .jobs = "2", .busy_pct = "25.0%",
            .labels = "self-hosted,windows", .busy_fraction = 0.25
        },
        {
            .name = "charlie", .os = "Linux", .state = "OFFLINE",
            .repo = "—", .job = "—", .runtime = "—",
            .state_for = "1h", .jobs = "1", .busy_pct = "150.0%",
            .labels = "self-hosted", .busy_fraction = 1.50
        }
    };
    RunnerCardsRecorder recorder = {0};
    RunnerUiRenderer renderer = {0};
    renderer.context = &recorder;
    renderer.begin_runner_cards = record_begin_runner_cards;
    renderer.runner_card = record_runner_card;
    renderer.runner_cards_empty = record_runner_cards_empty;
    renderer.runner_selection = record_runner_selection;
    renderer.end_runner_cards = record_end_runner_cards;
    RunnerUiRunnerCardsState state = {
        items, 3U, 1, false, false
    };
    if (!runner_ui_render_runner_cards(&renderer, &state)) return 1;
    if (recorder.begin_count != 1U || recorder.expected_items != 3U ||
        recorder.card_count != 3U || recorder.empty_count != 0U ||
        recorder.selection_count != 1U || recorder.end_count != 1U)
        return 2;
    if (recorder.tones[0] != RUNNER_UI_RUNNER_IDLE ||
        recorder.tones[1] != RUNNER_UI_RUNNER_RUNNING ||
        recorder.tones[2] != RUNNER_UI_RUNNER_OFFLINE)
        return 3;
    if (strcmp(recorder.state_labels[0], "● Idle") != 0 ||
        strcmp(recorder.state_labels[1], "● Running") != 0 ||
        strcmp(recorder.state_labels[2], "● Offline") != 0)
        return 4;
    if (recorder.show_job[0] || !recorder.show_job[1] ||
        recorder.show_job[2] || !recorder.selected[1])
        return 5;
    if (recorder.fractions[1] != 0.25 || recorder.fractions[2] != 1.0)
        return 6;
    if (strcmp(recorder.selection_eyebrow, "Selected runner") != 0 ||
        strcmp(recorder.selection_title, "bravo") != 0 ||
        strstr(recorder.selection_primary, "RUNNING for 3m") == NULL ||
        strstr(recorder.selection_primary, "2 jobs") == NULL ||
        strstr(recorder.selection_primary, "25.0% session utilisation") == NULL ||
        strstr(recorder.selection_secondary, "Repository: Example/Project") == NULL ||
        strstr(recorder.selection_secondary, "Job: Build") == NULL ||
        strstr(recorder.selection_secondary, "Labels: self-hosted,windows") == NULL)
        return 7;

    RunnerCardsRecorder filtered = {0};
    renderer.context = &filtered;
    RunnerUiRunnerCardsState empty = {NULL, 0U, -1, false, true};
    if (!runner_ui_render_runner_cards(&renderer, &empty)) return 8;
    if (filtered.card_count != 0U || filtered.empty_count != 1U ||
        strcmp(filtered.empty_message, "No runners match this filter.") != 0)
        return 9;

    RunnerCardsRecorder loading = {0};
    renderer.context = &loading;
    empty.refreshing = true;
    if (!runner_ui_render_runner_cards(&renderer, &empty)) return 10;
    if (strcmp(loading.empty_message, "Loading runners…") != 0) return 11;

    state.selected_index = 3;
    if (runner_ui_render_runner_cards(&renderer, &state)) return 12;
    RunnerUiRenderer incomplete = {0};
    state.selected_index = -1;
    if (runner_ui_render_runner_cards(&incomplete, &state)) return 13;
    return 0;
}

'''
text = replace_once(
    text,
    "static int verify_page_transition_policy(void)\n",
    runner_test + "static int verify_page_transition_policy(void)\n",
    "runner cards contract test",
)
text = replace_once(
    text,
    '        "shell.workspace.page-host", "page.runners", "page.active-jobs",\n',
    '        "shell.workspace.page-host", "page.runners", "page.runners.cards",\n'
    '        "page.runners.cards.runner", "page.active-jobs",\n',
    "required runner card components",
)
text = replace_once(
    text,
    "    if (verify_workspace_summary_renderer() != 0) return 18;\n\n"
    "    puts(\"Runner Monitor shared UI contract, navigation, page transitions, workspace summary and shell chrome renderers passed.\");\n",
    "    if (verify_workspace_summary_renderer() != 0) return 18;\n"
    "    if (verify_runner_cards_renderer() != 0) return 19;\n\n"
    "    puts(\"Runner Monitor shared UI contract, navigation, page transitions, workspace summary, runner cards/details and shell chrome renderers passed.\");\n",
    "runner cards test invocation",
)
write(path, text)


# Remove the one-shot migration scaffolding from the final branch diff.
Path(".github/workflows/apply-runner-cards-1.3.5.yml").unlink(missing_ok=True)
Path("tools/apply_runner_cards_renderer_migration.py").unlink(missing_ok=True)
