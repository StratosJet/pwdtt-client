#include "tray_linux.h"
#include <libayatana-appindicator/app-indicator.h>
#include <gtk/gtk.h>
#include <stdio.h>

static AppIndicator *indicator = NULL;
static GtkWidget *menu = NULL;
static GtkWidget *status_label = NULL; /* GtkLabel внутри кастомного item */

extern void onShowClicked();
extern void onQuitClicked();

static void show_cb(GtkMenuItem *item, gpointer data) { onShowClicked(); }
static void quit_cb(GtkMenuItem *item, gpointer data) { onQuitClicked(); }

/* Создаёт GtkMenuItem с GtkLabel внутри (поддержка Pango markup) */
static GtkWidget *make_markup_item(GtkWidget **out_label, const char *markup) {
    GtkWidget *item = gtk_menu_item_new();
    GtkWidget *lbl  = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl), markup);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_container_add(GTK_CONTAINER(item), lbl);
    if (out_label) *out_label = lbl;
    return item;
}

/* Wails owns GTK initialization and gtk_main(). Schedule all GTK work there. */
static gboolean do_init_tray(gpointer data) {
    char *icon_path = (char *)data;

    indicator = app_indicator_new("pwdtt", icon_path, APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);

    menu = gtk_menu_new();

    /* Строка статуса: цветная точка + текст через Pango */
    GtkWidget *status_item = make_markup_item(&status_label,
        "<span color='#cc3333'>\xe2\x97\x8f</span> \xd0\x9e\xd1\x82\xd0\xba\xd0\xbb\xd1\x8e\xd1\x87\xd0\xb5\xd0\xbd\xd0\xbe");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), status_item);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    GtkWidget *show_item = gtk_menu_item_new_with_label(
        "\xd0\x9f\xd0\xbe\xd0\xba\xd0\xb0\xd0\xb7\xd0\xb0\xd1\x82\xd1\x8c");
    g_signal_connect(show_item, "activate", G_CALLBACK(show_cb), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), show_item);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    GtkWidget *quit_item = gtk_menu_item_new_with_label(
        "\xd0\x92\xd1\x8b\xd1\x85\xd0\xbe\xd0\xb4");
    g_signal_connect(quit_item, "activate", G_CALLBACK(quit_cb), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), quit_item);

    gtk_widget_show_all(menu);
    app_indicator_set_menu(indicator, GTK_MENU(menu));
    g_free(icon_path);
    return G_SOURCE_REMOVE;
}

void wdtt_tray_init(const char *icon_path) {
    /* Wails starts gtk_main on its own thread; OnStartup runs in a Go goroutine. */
    g_idle_add(do_init_tray, g_strdup(icon_path));
}

static void update_status_label(int connected, long long rx, long long tx, int workers) {
    if (!status_label) return;
    char buf[256];
    if (connected) {
        double total_mb = (double)(rx + tx) / (1024.0 * 1024.0);
        /* ● Подключено  ↑0.0 МБ  ворк: N */
        snprintf(buf, sizeof(buf),
            "<span color='#33cc33'>\xe2\x97\x8f</span>"
            " \xd0\x9f\xd0\xbe\xd0\xb4\xd0\xba\xd0\xbb\xd1\x8e\xd1\x87\xd0\xb5\xd0\xbd\xd0\xbe"
            "  \xe2\x86\x91%.1f \xd0\x9c\xd0\x91"
            "  \xd0\xb2\xd0\xbe\xd1\x80\xd0\xba: %d",
            total_mb, workers);
    } else {
        snprintf(buf, sizeof(buf),
            "<span color='#cc3333'>\xe2\x97\x8f</span>"
            " \xd0\x9e\xd1\x82\xd0\xba\xd0\xbb\xd1\x8e\xd1\x87\xd0\xb5\xd0\xbd\xd0\xbe");
    }
    gtk_label_set_markup(GTK_LABEL(status_label), buf);
}

typedef struct { int connected; long long rx, tx; int workers; } StatusArgs;

static gboolean do_update_status(gpointer data) {
    StatusArgs *a = (StatusArgs *)data;
    update_status_label(a->connected, a->rx, a->tx, a->workers);
    g_free(a);
    return G_SOURCE_REMOVE;
}

void wdtt_tray_set_status(int connected, long long rx, long long tx, int workers) {
    StatusArgs *a = g_new(StatusArgs, 1);
    a->connected = connected; a->rx = rx; a->tx = tx; a->workers = workers;
    g_idle_add(do_update_status, a);
}

static gboolean do_set_visible(gpointer data) {
    if (indicator != NULL) {
        app_indicator_set_status(indicator,
            GPOINTER_TO_INT(data) ? APP_INDICATOR_STATUS_ACTIVE : APP_INDICATOR_STATUS_PASSIVE);
    }
    return G_SOURCE_REMOVE;
}

void wdtt_tray_set_visible(int visible) {
    g_idle_add(do_set_visible, GINT_TO_POINTER(visible != 0));
}
