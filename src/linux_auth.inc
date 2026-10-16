// SPDX-License-Identifier: GPL-3.0-or-later
// GTK lifetime adapter for the supplied GitHub sign-in broker.
typedef void (*GithubLogin)(RunnerGithubAuth *, const char *, const RunnerAuthCallbacks *);
typedef struct {
    gint references;
    gboolean alive, active, busy, open_browser;
    guint generation;
    GtkWidget *dialog, *status, *code;
    RunnerGithubAuth auth;
    RunnerScopeApp *app;
    GCancellable *cancel;
    GithubLogin login;
} GithubSignIn;
typedef struct {
    RunnerGithubAuth auth;
    char organisation[128];
    guint generation;
    GCancellable *cancel;
    GithubSignIn *owner;
    GithubLogin login;
} GithubSignInWork;
typedef struct { GithubSignIn *owner; guint generation; char code[32]; } GithubCode;
static void github_sign_in_release(GithubSignIn *sign_in)
{
    if (!g_atomic_int_dec_and_test(&sign_in->references)) return;
    g_clear_object(&sign_in->cancel);
    runner_github_auth_clear(&sign_in->auth); g_free(sign_in);
}
static void github_sign_in_cancel(GithubSignIn *sign_in)
{
    sign_in->active = FALSE; sign_in->generation++;
    if (sign_in->cancel) g_cancellable_cancel(sign_in->cancel);
    g_clear_object(&sign_in->cancel);
    runner_github_auth_clear(&sign_in->auth);
    if (sign_in->alive) gtk_label_set_text(GTK_LABEL(sign_in->code), "");
}
static void github_sign_in_open(GtkButton *button, gpointer data)
{
    (void)button; GithubSignIn *sign_in = data;
    if (!sign_in->active || !sign_in->auth.user_code[0]) return;
    GError *error = NULL;
    if (!gtk_show_uri_on_window(GTK_WINDOW(sign_in->dialog),
        "https://github.com/login/device", GDK_CURRENT_TIME, &error))
        gtk_label_set_text(GTK_LABEL(sign_in->status), "Open github.com/login/device. Your sign-in code is copied.");
    g_clear_error(&error);
}
static void github_sign_in_copy(GtkButton *button, gpointer data)
{
    (void)button; GithubSignIn *sign_in = data;
    if (sign_in->active && sign_in->auth.user_code[0])
        gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), sign_in->auth.user_code, -1);
}
static gboolean github_sign_in_code_ready(gpointer data)
{
    GithubCode *message = data; GithubSignIn *sign_in = message->owner;
    if (sign_in->alive && sign_in->active && sign_in->generation == message->generation) {
        sign_in->auth.phase = RUNNER_AUTH_WAITING;
        g_strlcpy(sign_in->auth.user_code, message->code, sizeof(sign_in->auth.user_code));
        gtk_label_set_text(GTK_LABEL(sign_in->code), message->code);
        gtk_label_set_text(GTK_LABEL(sign_in->status), "Paste the copied code into GitHub and approve sign-in.");
        github_sign_in_copy(NULL, sign_in);
        if (sign_in->open_browser) github_sign_in_open(NULL, sign_in);
    }
    github_sign_in_release(sign_in); g_free(message); return G_SOURCE_REMOVE;
}
static bool github_sign_in_cancelled(void *context)
{
    return g_cancellable_is_cancelled(((GithubSignInWork *)context)->cancel);
}
static void github_sign_in_challenge(const char *code, void *context)
{
    GithubSignInWork *work = context;
    GithubCode *message = g_new0(GithubCode, 1U);
    message->owner = work->owner; message->generation = work->generation;
    g_strlcpy(message->code, code, sizeof(message->code));
    g_atomic_int_inc(&work->owner->references);
    g_main_context_invoke(NULL, github_sign_in_code_ready, message);
}
static void github_sign_in_worker(GTask *task, gpointer source, gpointer data, GCancellable *cancel)
{
    (void)source; (void)cancel; GithubSignInWork *work = data;
    const RunnerAuthCallbacks callbacks = {github_sign_in_cancelled, github_sign_in_challenge, work};
    work->login(&work->auth, work->organisation, &callbacks);
    g_task_return_boolean(task, TRUE);
}
static void github_sign_in_work_free(gpointer data)
{
    GithubSignInWork *work = data;
    runner_github_auth_clear(&work->auth); g_object_unref(work->cancel); g_free(work);
}
static void github_sign_in_completed(GObject *source, GAsyncResult *result, gpointer data)
{
    (void)source; GithubSignIn *sign_in = data;
    GithubSignInWork *work = g_task_get_task_data(G_TASK(result));
    sign_in->busy = FALSE;
    if (sign_in->alive && sign_in->active && work->generation == sign_in->generation) {
        runner_github_auth_clear(&sign_in->auth); sign_in->auth = work->auth;
        sign_in->active = FALSE;
        gtk_label_set_text(GTK_LABEL(sign_in->code), "");
        if (sign_in->auth.phase == RUNNER_AUTH_COMPLETE) {
            GtkWidget *entry = g_object_get_data(G_OBJECT(sign_in->dialog), "organisation-entry");
            GtkWidget *choices = g_object_get_data(G_OBJECT(sign_in->dialog), "organisation-choices");
            if (choices) for (size_t i = 0U; i < sign_in->auth.organisation_count; ++i)
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(choices), sign_in->auth.organisations[i]);
            if (entry && sign_in->auth.organisation[0])
                gtk_entry_set_text(GTK_ENTRY(entry), sign_in->auth.organisation);
            if (!entry || gtk_entry_get_text(GTK_ENTRY(entry))[0]) {
                gtk_label_set_text(GTK_LABEL(sign_in->status), "Signed in. Connecting…");
                gtk_dialog_response(GTK_DIALOG(sign_in->dialog), GTK_RESPONSE_ACCEPT);
            } else {
                gtk_label_set_text(GTK_LABEL(sign_in->status), "Signed in. Choose the organisation to monitor.");
                gtk_widget_grab_focus(choices ? choices : entry);
            }
        } else gtk_label_set_text(GTK_LABEL(sign_in->status), sign_in->auth.error);
    }
    github_sign_in_release(sign_in);
}
static void github_sign_in_start(GtkButton *button, gpointer data)
{
    (void)button; GithubSignIn *sign_in = data;
    if (sign_in->busy) return;
    github_sign_in_cancel(sign_in);
    GithubSignInWork *work = g_new0(GithubSignInWork, 1U);
    GtkWidget *entry = g_object_get_data(G_OBJECT(sign_in->dialog), "organisation-entry");
    g_strlcpy(work->organisation, entry ? gtk_entry_get_text(GTK_ENTRY(entry)) :
        sign_in->app ? sign_in->app->config.organisation : "", sizeof(work->organisation));
    sign_in->cancel = g_cancellable_new();
    work->cancel = g_object_ref(sign_in->cancel);
    work->owner = sign_in; work->generation = sign_in->generation; work->login = sign_in->login;
    sign_in->active = TRUE; sign_in->busy = TRUE;
    gtk_label_set_text(GTK_LABEL(sign_in->status), "Connecting to GitHub…");
    g_atomic_int_inc(&sign_in->references);
    GTask *task = g_task_new(NULL, sign_in->cancel, github_sign_in_completed, sign_in);
    g_task_set_task_data(task, work, github_sign_in_work_free);
    g_task_run_in_thread(task, github_sign_in_worker); g_object_unref(task);
}
static void github_sign_in_cancel_clicked(GtkButton *button, gpointer data)
{
    (void)button; GithubSignIn *sign_in = data;
    github_sign_in_cancel(sign_in);
    gtk_label_set_text(GTK_LABEL(sign_in->status), "Sign-in cancelled.");
}
static void github_sign_in_org_changed(GtkEditable *entry, gpointer data)
{
    (void)entry; GithubSignIn *sign_in = data;
    if (sign_in->active) github_sign_in_cancel(sign_in);
}
static GithubSignIn *github_sign_in_create(GtkWidget *dialog, GtkWidget *grid, GtkWidget *status)
{
    GithubSignIn *sign_in = g_new0(GithubSignIn, 1U);
    sign_in->references = 1; sign_in->alive = TRUE; sign_in->open_browser = TRUE;
    sign_in->dialog = dialog; sign_in->status = status; sign_in->login = runner_github_sign_in;
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *start = gtk_button_new_with_label("Sign in with GitHub");
    GtkWidget *cancel = gtk_button_new_with_label("Cancel sign-in");
    g_signal_connect(start, "clicked", G_CALLBACK(github_sign_in_start), sign_in);
    g_signal_connect(cancel, "clicked", G_CALLBACK(github_sign_in_cancel_clicked), sign_in);
    gtk_box_pack_start(GTK_BOX(actions), start, FALSE, FALSE, 0U);
    gtk_box_pack_start(GTK_BOX(actions), cancel, FALSE, FALSE, 0U);
    sign_in->code = gtk_label_new("");
    gtk_label_set_selectable(GTK_LABEL(sign_in->code), TRUE);
    gtk_widget_set_halign(sign_in->code, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), actions, FALSE, FALSE, 0U);
    gtk_box_pack_start(GTK_BOX(box), sign_in->code, FALSE, FALSE, 0U);
    gtk_grid_attach(GTK_GRID(grid), box, 1, 1, 1, 1);
    g_object_set_data(G_OBJECT(dialog), "github-sign-in", sign_in);
    g_object_set_data(G_OBJECT(dialog), "github-sign-in-button", start);
    return sign_in;
}
static void github_sign_in_close(GithubSignIn *sign_in)
{
    github_sign_in_cancel(sign_in); sign_in->alive = FALSE; github_sign_in_release(sign_in);
}
