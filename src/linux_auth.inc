// SPDX-License-Identifier: GPL-3.0-or-later
// Native GTK device sign-in; included by linux_main.c.
typedef struct {
    gint references;
    gboolean alive;
    gboolean active;
    gboolean busy;
    gboolean open_browser;
    guint timer;
    guint generation;
    GtkWidget *dialog;
    GtkWidget *status;
    GtkWidget *code;
    RunnerGithubAuth auth;
    RunnerHttpPost post;
    void *post_context;
    const char *client_id;
    char client_id_storage[128];
    RunnerScopeApp *app;
} GithubSignIn;

typedef struct {
    RunnerGithubAuth auth;
    RunnerHttpPost post;
    void *context;
    char client_id[128];
    guint generation;
} GithubSignInWork;

static void github_sign_in_release(GithubSignIn *sign_in)
{
    if (!g_atomic_int_dec_and_test(&sign_in->references)) return;
    runner_github_auth_clear(&sign_in->auth);
    g_free(sign_in);
}

static void github_sign_in_work_free(gpointer data)
{
    GithubSignInWork *work = data;
    if (!work) return;
    runner_github_auth_clear(&work->auth);
    g_free(work);
}

static void github_sign_in_cancel(GithubSignIn *sign_in)
{
    sign_in->active = FALSE;
    sign_in->generation++;
    if (sign_in->timer) {
        g_source_remove(sign_in->timer);
        sign_in->timer = 0U;
    }
    runner_github_auth_clear(&sign_in->auth);
    if (sign_in->alive) gtk_label_set_text(GTK_LABEL(sign_in->code), "");
}

static void github_sign_in_open(GtkButton *button, gpointer data)
{
    (void)button;
    GithubSignIn *sign_in = data;
    if (!sign_in->active || sign_in->auth.phase != RUNNER_AUTH_WAITING) return;
    GError *error = NULL;
    if (!gtk_show_uri_on_window(GTK_WINDOW(sign_in->dialog),
        "https://github.com/login/device", GDK_CURRENT_TIME, &error))
        gtk_label_set_text(GTK_LABEL(sign_in->status),
            "Open https://github.com/login/device in your browser and enter the displayed code.");
    g_clear_error(&error);
}

static void github_sign_in_copy(GtkButton *button, gpointer data)
{
    (void)button;
    GithubSignIn *sign_in = data;
    if (sign_in->active && sign_in->auth.user_code[0])
        gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), sign_in->auth.user_code, -1);
}

static void github_sign_in_cancel_clicked(GtkButton *button, gpointer data)
{
    (void)button;
    GithubSignIn *sign_in = data;
    github_sign_in_cancel(sign_in);
    gtk_label_set_text(GTK_LABEL(sign_in->status), "Sign-in cancelled. Your saved account has been kept.");
}

static void github_sign_in_worker(GTask *task, gpointer source, gpointer data, GCancellable *cancel)
{
    (void)source; (void)cancel;
    GithubSignInWork *work = data;
    runner_github_auth_step(&work->auth, work->client_id,
        (uint64_t)(g_get_monotonic_time() / 1000), work->post, work->context);
    g_task_return_boolean(task, TRUE);
}

static void github_sign_in_completed(GObject *source, GAsyncResult *result, gpointer data)
{
    (void)source;
    GithubSignIn *sign_in = data;
    GithubSignInWork *work = g_task_get_task_data(G_TASK(result));
    sign_in->busy = FALSE;
    if (sign_in->alive && sign_in->active && work->generation == sign_in->generation) {
        const gboolean first_code = sign_in->auth.phase == RUNNER_AUTH_NEW;
        runner_github_auth_clear(&sign_in->auth);
        sign_in->auth = work->auth;
        if (sign_in->auth.phase == RUNNER_AUTH_WAITING) {
            gtk_label_set_text(GTK_LABEL(sign_in->code), sign_in->auth.user_code);
            gtk_label_set_text(GTK_LABEL(sign_in->status),
                "Enter this code at github.com/login/device and approve Runner Monitor. You will connect automatically.");
            if (first_code && sign_in->open_browser) github_sign_in_open(NULL, sign_in);
        } else if (sign_in->auth.phase == RUNNER_AUTH_COMPLETE) {
            sign_in->active = FALSE;
            if (sign_in->timer) { g_source_remove(sign_in->timer); sign_in->timer = 0U; }
            gtk_label_set_text(GTK_LABEL(sign_in->code), "");
            gtk_label_set_text(GTK_LABEL(sign_in->status), "Signed in. Checking organisation runner access…");
            gtk_dialog_response(GTK_DIALOG(sign_in->dialog), GTK_RESPONSE_ACCEPT);
        } else {
            gtk_label_set_text(GTK_LABEL(sign_in->status), sign_in->auth.error);
            github_sign_in_cancel(sign_in);
        }
    }
    github_sign_in_release(sign_in);
}

static gboolean github_sign_in_tick(gpointer data)
{
    GithubSignIn *sign_in = data;
    if (!sign_in->alive || !sign_in->active) { sign_in->timer = 0U; return G_SOURCE_REMOVE; }
    const uint64_t now = (uint64_t)(g_get_monotonic_time() / 1000);
    if (sign_in->busy || (sign_in->auth.phase == RUNNER_AUTH_WAITING &&
        now < sign_in->auth.next_poll_ms && now < sign_in->auth.expires_at_ms)) return G_SOURCE_CONTINUE;
    GithubSignInWork *work = g_new0(GithubSignInWork, 1U);
    work->auth = sign_in->auth;
    work->post = sign_in->post;
    work->context = sign_in->post_context;
    g_strlcpy(work->client_id, sign_in->client_id, sizeof(work->client_id));
    work->generation = sign_in->generation;
    sign_in->busy = TRUE;
    g_atomic_int_inc(&sign_in->references);
    GTask *task = g_task_new(NULL, NULL, github_sign_in_completed, sign_in);
    g_task_set_task_data(task, work, github_sign_in_work_free);
    g_task_run_in_thread(task, github_sign_in_worker);
    g_object_unref(task);
    return G_SOURCE_CONTINUE;
}

static gboolean github_setup_dialog(GithubSignIn *sign_in)
{
    if (!sign_in->app) return FALSE;
    github_sign_in_cancel(sign_in);
    GtkWidget *dialog = gtk_dialog_new_with_buttons("GitHub sign-in setup",
        GTK_WINDOW(sign_in->dialog), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Cancel", GTK_RESPONSE_CANCEL, "_Save and sign in", GTK_RESPONSE_ACCEPT, NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 680, 650);
    GtkWidget *area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(area), 16U);
    gtk_box_set_spacing(GTK_BOX(area), 10);
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroll, TRUE);
    GtkWidget *guide = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(guide), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(guide), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(guide), GTK_WRAP_WORD_CHAR);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(guide)), runner_github_setup_instructions(), -1);
    gtk_container_add(GTK_CONTAINER(scroll), guide);
    gtk_box_pack_start(GTK_BOX(area), scroll, TRUE, TRUE, 0U);
    GtkWidget *open = gtk_link_button_new_with_label("https://github.com/settings/applications/new",
        "Open GitHub registration");
    gtk_widget_set_halign(open, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(area), open, FALSE, FALSE, 0U);
    GtkWidget *label = gtk_label_new("Public Client ID");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(area), label, FALSE, FALSE, 0U);
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(entry), 127);
    gtk_entry_set_text(GTK_ENTRY(entry), runner_github_auth_client_for_config(&sign_in->app->config));
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Client ID from the GitHub OAuth app page");
    gtk_box_pack_start(GTK_BOX(area), entry, FALSE, FALSE, 0U);
    GtkWidget *error_label = gtk_label_new("");
    gtk_label_set_line_wrap(GTK_LABEL(error_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(error_label), 0.0F);
    gtk_label_set_max_width_chars(GTK_LABEL(error_label), 65);
    gtk_box_pack_start(GTK_BOX(area), error_label, FALSE, FALSE, 0U);
    g_object_set_data(G_OBJECT(dialog), "github-client-entry", entry);
    g_object_set_data(G_OBJECT(dialog), "github-setup-error", error_label);
    g_object_set_data(G_OBJECT(dialog), "github-setup-guide", guide);
    g_object_set_data(G_OBJECT(dialog), "github-setup-link", open);
    gtk_widget_show_all(dialog);
    gboolean saved = FALSE;
    while (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        RunnerConfig candidate = sign_in->app->config;
        g_strlcpy(candidate.github_client_id, gtk_entry_get_text(GTK_ENTRY(entry)), sizeof(candidate.github_client_id));
        char detail[640] = "";
        if (!candidate.github_client_id[0] || !runner_config_validate(&candidate, detail, sizeof(detail))) {
            gtk_label_set_text(GTK_LABEL(error_label), detail[0] ? detail :
                "Paste the public Client ID shown after registering the OAuth app.");
            memset(candidate.github_token, 0, sizeof(candidate.github_token));
            continue;
        }
        const gboolean changed = strcmp(candidate.github_client_id,
            runner_github_auth_client_for_config(&sign_in->app->config)) != 0;
        if (changed) memset(candidate.github_token, 0, sizeof(candidate.github_token));
        GError *error = NULL;
        if (!save_config(&candidate, &error)) {
            gtk_label_set_text(GTK_LABEL(error_label), error ? error->message : "Unable to save GitHub setup.");
            g_clear_error(&error);
            memset(candidate.github_token, 0, sizeof(candidate.github_token));
            continue;
        }
        memset(sign_in->app->config.github_token, 0, sizeof(sign_in->app->config.github_token));
        sign_in->app->config = candidate;
        memset(candidate.github_token, 0, sizeof(candidate.github_token));
        sign_in->app->config_generation++;
        g_clear_pointer(&sign_in->app->config_error, g_free);
        if (changed) {
            sign_in->app->runner_data_stale = TRUE;
            sign_in->app->activity_data_stale = TRUE;
        }
        g_strlcpy(sign_in->client_id_storage, runner_github_auth_client_for_config(&sign_in->app->config),
            sizeof(sign_in->client_id_storage));
        sign_in->client_id = sign_in->client_id_storage;
        saved = TRUE;
        break;
    }
    gtk_widget_destroy(dialog);
    return saved;
}

static void github_sign_in_start(GtkButton *button, gpointer data)
{
    (void)button;
    GithubSignIn *sign_in = data;
    if (sign_in->busy) {
        gtk_label_set_text(GTK_LABEL(sign_in->status), "The previous sign-in request is finishing. Try again shortly.");
        return;
    }
    if (!sign_in->client_id[0] && !github_setup_dialog(sign_in)) return;
    github_sign_in_cancel(sign_in);
    sign_in->active = TRUE;
    gtk_label_set_text(GTK_LABEL(sign_in->status), "Connecting to GitHub for browser sign-in…");
    sign_in->timer = g_timeout_add(250U, github_sign_in_tick, sign_in);
    (void)github_sign_in_tick(sign_in);
}

static void github_setup_clicked(GtkButton *button, gpointer data)
{
    GithubSignIn *sign_in = data;
    if (github_setup_dialog(sign_in)) github_sign_in_start(button, sign_in);
}

static GithubSignIn *github_sign_in_create(GtkWidget *dialog, GtkWidget *grid, GtkWidget *status)
{
    GithubSignIn *sign_in = g_new0(GithubSignIn, 1U);
    sign_in->references = 1;
    sign_in->alive = TRUE;
    sign_in->open_browser = TRUE;
    sign_in->dialog = dialog;
    sign_in->status = status;
    sign_in->post = runner_http_post_oauth;
    sign_in->client_id = runner_github_auth_client_id();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *start = gtk_button_new_with_label("Sign in with GitHub");
    GtkWidget *copy = gtk_button_new_with_label("Copy code");
    GtkWidget *open = gtk_button_new_with_label("Open GitHub");
    GtkWidget *cancel = gtk_button_new_with_label("Cancel sign-in");
    g_signal_connect(start, "clicked", G_CALLBACK(github_sign_in_start), sign_in);
    g_signal_connect(copy, "clicked", G_CALLBACK(github_sign_in_copy), sign_in);
    g_signal_connect(open, "clicked", G_CALLBACK(github_sign_in_open), sign_in);
    g_signal_connect(cancel, "clicked", G_CALLBACK(github_sign_in_cancel_clicked), sign_in);
    gtk_box_pack_start(GTK_BOX(actions), start, FALSE, FALSE, 0U);
    gtk_box_pack_start(GTK_BOX(actions), copy, FALSE, FALSE, 0U);
    gtk_box_pack_start(GTK_BOX(actions), open, FALSE, FALSE, 0U);
    gtk_box_pack_start(GTK_BOX(actions), cancel, FALSE, FALSE, 0U);
    sign_in->code = gtk_label_new("");
    gtk_label_set_selectable(GTK_LABEL(sign_in->code), TRUE);
    gtk_widget_set_halign(sign_in->code, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), actions, FALSE, FALSE, 0U);
    gtk_box_pack_start(GTK_BOX(box), sign_in->code, FALSE, FALSE, 0U);
    gtk_grid_attach(GTK_GRID(grid), box, 1, 1, 1, 1);
    GtkWidget *setup = gtk_button_new_with_label("GitHub setup and instructions…");
    gtk_widget_set_halign(setup, GTK_ALIGN_START);
    g_signal_connect(setup, "clicked", G_CALLBACK(github_setup_clicked), sign_in);
    gtk_grid_attach(GTK_GRID(grid), setup, 0, 9, 2, 1);
    g_object_set_data(G_OBJECT(dialog), "github-setup-button", setup);
    g_object_set_data(G_OBJECT(dialog), "github-sign-in", sign_in);
    g_object_set_data(G_OBJECT(dialog), "github-sign-in-button", start);
    return sign_in;
}

static void github_sign_in_close(GithubSignIn *sign_in)
{
    github_sign_in_cancel(sign_in);
    sign_in->alive = FALSE;
    github_sign_in_release(sign_in);
}
