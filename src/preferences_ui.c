/*
   FSearch - A fast file search utility
   Copyright © 2020 Christian Boxdörfer

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
   */

#include "preferences_ui.h"
#include "fsearch.h"
#include "fsearch_exclude_path.h"
#include "fsearch_include_path.h"
#include "fsearch_preferences_widgets.h"
#include "ui_utils.h"
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FsearchConfig *new_config;
    void (*finished_cb)(FsearchConfig *);

    GtkWindow *window;
    GtkBuilder *builder;
    GtkWidget *dialog;

    GtkWidget *main_notebook;

    // Interface page
    GtkToggleButton *enable_dark_theme_button;
    GtkToggleButton *show_menubar_button;
    GtkToggleButton *show_tooltips_button;
    GtkToggleButton *restore_win_size_button;
    GtkToggleButton *restore_sort_order_button;
    GtkToggleButton *restore_column_config_button;
    GtkToggleButton *double_click_path_button;
    GtkToggleButton *single_click_open_button;
    GtkToggleButton *show_icons_button;
    GtkToggleButton *highlight_search_terms;
    GtkToggleButton *show_base_2_units;
    GtkBox *action_after_file_open_box;
    GtkComboBox *action_after_file_open;
    GtkToggleButton *action_after_file_open_keyboard;
    GtkToggleButton *action_after_file_open_mouse;
    GtkToggleButton *show_indexing_status;

    // Search page
    GtkToggleButton *auto_search_in_path_button;
    GtkToggleButton *auto_match_case_button;
    GtkToggleButton *search_as_you_type_button;
    GtkToggleButton *hide_results_button;
    GtkToggleButton *limit_num_results_button;
    GtkWidget *limit_num_results_spin;

    // Database page
    GtkToggleButton *update_db_at_start_button;
    GtkToggleButton *auto_update_checkbox;
    GtkBox *auto_update_box;
    GtkWidget *auto_update_hours_spin_button;
    GtkWidget *auto_update_minutes_spin_button;

    // Dialog page
    GtkToggleButton *show_dialog_failed_opening;

    // Include page
    GtkTreeView *include_list;
    GtkTreeModel *include_model;
    GtkWidget *include_add_button;
    GtkWidget *include_remove_button;
    GtkTreeSelection *sel;

    // Exclude model
    GtkTreeView *exclude_list;
    GtkTreeModel *exclude_model;
    GtkWidget *exclude_add_button;
    GtkWidget *exclude_remove_button;
    GtkTreeSelection *exclude_selection;
    GtkToggleButton *exclude_hidden_items_button;
    GtkEntry *exclude_files_entry;
    gchar *exclude_files_str;
} FsearchPreferencesInterface;

enum { COLUMN_NAME, NUM_COLUMNS };

guint help_reset_timeout_id = 0;
static GtkWidget *help_stack = NULL;
static GtkWidget *help_description = NULL;

static void
on_toggle_set_sensitive(GtkToggleButton *togglebutton, gpointer user_data) {
    GtkWidget *spin = GTK_WIDGET(user_data);
    gtk_widget_set_sensitive(spin, gtk_toggle_button_get_active(togglebutton));
}

static void
on_auto_update_minutes_spin_button_changed(GtkSpinButton *spin_button, gpointer user_data) {
    GtkSpinButton *hours_spin = GTK_SPIN_BUTTON(user_data);
    double minutes = gtk_spin_button_get_value(spin_button);
    double hours = gtk_spin_button_get_value(hours_spin);

    if (hours == 0 && minutes == 0) {
        gtk_spin_button_set_value(spin_button, 1.0);
    }
}

static void
on_auto_update_hours_spin_button_changed(GtkSpinButton *spin_button, gpointer user_data) {
    GtkSpinButton *minutes_spin = GTK_SPIN_BUTTON(user_data);
    double hours = gtk_spin_button_get_value(spin_button);
    double minutes = gtk_spin_button_get_value(minutes_spin);

    if (hours == 0 && minutes == 0) {
        gtk_spin_button_set_value(minutes_spin, 1.0);
    }
}

static void
on_remove_button_clicked(GtkButton *button, gpointer user_data) {
    GtkTreeView *tree_view = GTK_TREE_VIEW(user_data);
    GtkTreeSelection *sel = gtk_tree_view_get_selection(tree_view);
    gtk_tree_selection_selected_foreach(sel, pref_treeview_row_remove, NULL);
}

static char *
run_file_chooser_dialog(GtkButton *button) {
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;

    GtkWidget *window = gtk_widget_get_toplevel(GTK_WIDGET(button));

#if !GTK_CHECK_VERSION(3, 20, 0)
    GtkWidget *dialog = gtk_file_chooser_dialog_new(_("Select folder"),
                                                    GTK_WINDOW(window),
                                                    action,
                                                    _("_Cancel"),
                                                    GTK_RESPONSE_CANCEL,
                                                    _("_Select"),
                                                    GTK_RESPONSE_ACCEPT,
                                                    NULL);

    GtkResponseType res = gtk_dialog_run(GTK_DIALOG(dialog));
#else
    GtkFileChooserNative *dialog =
        gtk_file_chooser_native_new(_("Select folder"), GTK_WINDOW(window), action, _("_Select"), _("_Cancel"));

    GtkResponseType res = gtk_native_dialog_run(GTK_NATIVE_DIALOG(dialog));
#endif
    char *path = NULL;
    if (res == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        char *uri = gtk_file_chooser_get_uri(chooser);
        path = g_filename_from_uri(uri, NULL, NULL);
        g_free(uri);
    }

    g_object_unref(dialog);
    return path;
}

static void
on_exclude_add_button_clicked(GtkButton *button, gpointer user_data) {
    GtkTreeModel *model = user_data;
    char *path = run_file_chooser_dialog(button);
    if (!path) {
        return;
    }
    pref_exclude_treeview_row_add(model, path);
}

static void
on_include_add_button_clicked(GtkButton *button, gpointer user_data) {
    GtkTreeModel *model = user_data;
    char *path = run_file_chooser_dialog(button);
    if (!path) {
        return;
    }
    pref_include_treeview_row_add(model, path);
}

static void
on_list_selection_changed(GtkTreeSelection *sel, gpointer user_data) {
    gboolean selected = gtk_tree_selection_get_selected(sel, NULL, NULL);
    gtk_widget_set_sensitive(GTK_WIDGET(user_data), selected);
}

static gboolean
on_help_update(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    if (help_reset_timeout_id != 0) {
        g_source_remove(help_reset_timeout_id);
        help_reset_timeout_id = 0;
    }
    if (help_stack != NULL) {
        gtk_stack_set_visible_child(GTK_STACK(help_stack), GTK_WIDGET(user_data));
    }
    return FALSE;
}

static gboolean
help_reset(gpointer user_data) {
    if (help_stack != NULL) {
        gtk_stack_set_visible_child(GTK_STACK(help_stack), GTK_WIDGET(help_description));
    }
    help_reset_timeout_id = 0;
    return G_SOURCE_REMOVE;
}

static gboolean
on_help_reset(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    help_reset_timeout_id = g_timeout_add(200, help_reset, NULL);
    return FALSE;
}

static GtkWidget *
builder_init_widget(GtkBuilder *builder, const char *name, const char *help) {
    GtkWidget *widget = GTK_WIDGET(gtk_builder_get_object(builder, name));
    GtkWidget *help_widget = GTK_WIDGET(gtk_builder_get_object(builder, help));
    g_signal_connect(widget, "enter-notify-event", G_CALLBACK(on_help_update), help_widget);
    g_signal_connect(widget, "leave-notify-event", G_CALLBACK(on_help_reset), NULL);
    g_signal_connect(widget, "focus-in-event", G_CALLBACK(on_help_update), help_widget);
    g_signal_connect(widget, "focus-out-event", G_CALLBACK(on_help_reset), NULL);
    return widget;
}

static GtkToggleButton *
toggle_button_get(GtkBuilder *builder, const char *name, const char *help, bool val) {
    GtkToggleButton *button = GTK_TOGGLE_BUTTON(builder_init_widget(builder, name, help));
    gtk_toggle_button_set_active(button, val);
    return button;
}

static void
action_after_file_open_changed(GtkComboBox *widget, gpointer user_data) {
    int active = gtk_combo_box_get_active(widget);
    if (active != ACTION_AFTER_OPEN_NOTHING) {
        gtk_widget_set_sensitive(GTK_WIDGET(user_data), TRUE);
    }
    else {
        gtk_widget_set_sensitive(GTK_WIDGET(user_data), FALSE);
    }
}

static void
preferences_ui_get_state(FsearchPreferencesInterface *ui) {
    FsearchConfig *new_config = ui->new_config;
    new_config->search_as_you_type = gtk_toggle_button_get_active(ui->search_as_you_type_button);
    new_config->enable_dark_theme = gtk_toggle_button_get_active(ui->enable_dark_theme_button);
    new_config->show_menubar = !gtk_toggle_button_get_active(ui->show_menubar_button);
    new_config->restore_column_config = gtk_toggle_button_get_active(ui->restore_column_config_button);
    new_config->restore_sort_order = gtk_toggle_button_get_active(ui->restore_sort_order_button);
    new_config->double_click_path = gtk_toggle_button_get_active(ui->double_click_path_button);
    new_config->enable_list_tooltips = gtk_toggle_button_get_active(ui->show_tooltips_button);
    new_config->restore_window_size = gtk_toggle_button_get_active(ui->restore_win_size_button);
    new_config->update_database_on_launch = gtk_toggle_button_get_active(ui->update_db_at_start_button);
    new_config->update_database_every = gtk_toggle_button_get_active(ui->auto_update_checkbox);
    new_config->update_database_every_hours =
        gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ui->auto_update_hours_spin_button));
    new_config->update_database_every_minutes =
        gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ui->auto_update_minutes_spin_button));
    new_config->show_base_2_units = gtk_toggle_button_get_active(ui->show_base_2_units);
    new_config->action_after_file_open = gtk_combo_box_get_active(ui->action_after_file_open);
    new_config->action_after_file_open_keyboard = gtk_toggle_button_get_active(ui->action_after_file_open_keyboard);
    new_config->action_after_file_open_mouse = gtk_toggle_button_get_active(ui->action_after_file_open_mouse);
    new_config->show_indexing_status = gtk_toggle_button_get_active(ui->show_indexing_status);
    // Dialogs
    new_config->show_dialog_failed_opening = gtk_toggle_button_get_active(ui->show_dialog_failed_opening);
    new_config->auto_search_in_path = gtk_toggle_button_get_active(ui->auto_search_in_path_button);
    new_config->auto_match_case = gtk_toggle_button_get_active(ui->auto_match_case_button);
    new_config->hide_results_on_empty_search = gtk_toggle_button_get_active(ui->hide_results_button);
    new_config->limit_results = gtk_toggle_button_get_active(ui->limit_num_results_button);
    new_config->num_results = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ui->limit_num_results_spin));
    new_config->highlight_search_terms = gtk_toggle_button_get_active(ui->highlight_search_terms);
    new_config->single_click_open = gtk_toggle_button_get_active(ui->single_click_open_button);
    new_config->show_listview_icons = gtk_toggle_button_get_active(ui->show_icons_button);
    new_config->exclude_hidden_items = gtk_toggle_button_get_active(ui->exclude_hidden_items_button);

    if (new_config->exclude_files) {
        g_strfreev(new_config->exclude_files);
        new_config->exclude_files = NULL;
    }
    new_config->exclude_files = g_strsplit(gtk_entry_get_text(ui->exclude_files_entry), ";", -1);

    if (new_config->locations) {
        g_list_free_full(new_config->locations, (GDestroyNotify)fsearch_include_path_free);
    }
    new_config->locations = pref_include_treeview_data_get(ui->include_list);

    if (new_config->exclude_locations) {
        g_list_free_full(new_config->exclude_locations, (GDestroyNotify)fsearch_exclude_path_free);
    }
    new_config->exclude_locations = pref_exclude_treeview_data_get(ui->exclude_list);
}

static void
preferences_ui_cleanup(FsearchPreferencesInterface *ui) {
    if (ui->exclude_files_str) {
        free(ui->exclude_files_str);
        ui->exclude_files_str = NULL;
    }

    if (help_reset_timeout_id != 0) {
        g_source_remove(help_reset_timeout_id);
        help_reset_timeout_id = 0;
    }
    help_stack = NULL;

    g_object_unref(ui->builder);
    gtk_widget_destroy(ui->dialog);

    free(ui);
    ui = NULL;
}

static void
on_preferences_ui_response(GtkDialog *dialog, GtkResponseType response, gpointer user_data) {
    FsearchPreferencesInterface *ui = user_data;

    if (response != GTK_RESPONSE_OK) {
        config_free(ui->new_config);
        ui->new_config = NULL;
    }
    else {
        preferences_ui_get_state(ui);
    }

    if (ui->finished_cb) {
        ui->finished_cb(ui->new_config);
    }

    preferences_ui_cleanup(ui);
}

static void
preferences_ui_init(FsearchPreferencesInterface *ui, FsearchPreferencesPage page) {
    FsearchConfig *new_config = ui->new_config;

    ui->builder = gtk_builder_new_from_resource("/org/fsearch/fsearch/preferences.ui");

    ui->dialog = GTK_WIDGET(gtk_builder_get_object(ui->builder, "FsearchPreferencesWindow"));
    gtk_window_set_transient_for(GTK_WINDOW(ui->dialog), ui->window);
    gtk_dialog_add_button(GTK_DIALOG(ui->dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(ui->dialog), _("_OK"), GTK_RESPONSE_OK);
    g_signal_connect(ui->dialog, "response", G_CALLBACK(on_preferences_ui_response), ui);

    ui->main_notebook = GTK_WIDGET(gtk_builder_get_object(ui->builder, "pref_main_notebook"));
    gtk_notebook_set_current_page(GTK_NOTEBOOK(ui->main_notebook), page);

    help_stack = GTK_WIDGET(gtk_builder_get_object(ui->builder, "help_stack"));
    help_description = GTK_WIDGET(gtk_builder_get_object(ui->builder, "help_help"));

    // Interface page
    ui->enable_dark_theme_button =
        toggle_button_get(ui->builder, "enable_dark_theme_button", "help_dark_theme", new_config->enable_dark_theme);

    ui->show_menubar_button =
        toggle_button_get(ui->builder, "show_menubar_button", "help_csd", !new_config->show_menubar);

    ui->show_tooltips_button =
        toggle_button_get(ui->builder, "show_tooltips_button", "help_show_tooltips", new_config->enable_list_tooltips);

    ui->restore_win_size_button =
        toggle_button_get(ui->builder, "restore_win_size_button", "help_window_size", new_config->restore_window_size);

    ui->restore_sort_order_button = toggle_button_get(ui->builder,
                                                      "restore_sort_order_button",
                                                      "help_restore_sort_order",
                                                      new_config->restore_sort_order);

    ui->restore_column_config_button = toggle_button_get(ui->builder,
                                                         "restore_column_config_button",
                                                         "help_restore_column_config",
                                                         new_config->restore_column_config);

    ui->double_click_path_button = toggle_button_get(ui->builder,
                                                     "double_click_path_button",
                                                     "help_double_click_path",
                                                     new_config->double_click_path);

    ui->single_click_open_button = toggle_button_get(ui->builder,
                                                     "single_click_open_button",
                                                     "help_single_click_open",
                                                     new_config->single_click_open);

    ui->show_icons_button =
        toggle_button_get(ui->builder, "show_icons_button", "help_show_icons", new_config->show_listview_icons);

    ui->highlight_search_terms = toggle_button_get(ui->builder,
                                                   "highlight_search_terms",
                                                   "help_highlight_search_terms",
                                                   new_config->highlight_search_terms);

    ui->show_base_2_units =
        toggle_button_get(ui->builder, "show_base_2_units", "help_units", new_config->show_base_2_units);

    ui->action_after_file_open_box =
        GTK_BOX(builder_init_widget(ui->builder, "action_after_file_open_box", "help_action_after_open"));
    ui->action_after_file_open =
        GTK_COMBO_BOX(builder_init_widget(ui->builder, "action_after_file_open", "help_action_after_open"));
    gtk_combo_box_set_active(ui->action_after_file_open, new_config->action_after_file_open);

    g_signal_connect(ui->action_after_file_open,
                     "changed",
                     G_CALLBACK(action_after_file_open_changed),
                     ui->action_after_file_open_box);

    if (new_config->action_after_file_open != ACTION_AFTER_OPEN_NOTHING) {
        gtk_widget_set_sensitive(GTK_WIDGET(ui->action_after_file_open_box), TRUE);
    }
    else {
        gtk_widget_set_sensitive(GTK_WIDGET(ui->action_after_file_open_box), FALSE);
    }

    ui->action_after_file_open_keyboard = toggle_button_get(ui->builder,
                                                            "action_after_file_open_keyboard",
                                                            "help_action_after_open",
                                                            new_config->action_after_file_open_keyboard);

    ui->action_after_file_open_mouse = toggle_button_get(ui->builder,
                                                         "action_after_file_open_mouse",
                                                         "help_action_after_open",
                                                         new_config->action_after_file_open_mouse);

    ui->show_indexing_status = toggle_button_get(ui->builder,
                                                 "show_indexing_status_button",
                                                 "help_show_indexing_status",
                                                 new_config->show_indexing_status);

    // Search page
    ui->auto_search_in_path_button =
        toggle_button_get(ui->builder, "auto_search_in_path_button", "help_auto_path", new_config->auto_search_in_path);

    ui->auto_match_case_button =
        toggle_button_get(ui->builder, "auto_match_case_button", "help_auto_case", new_config->auto_match_case);

    ui->search_as_you_type_button = toggle_button_get(ui->builder,
                                                      "search_as_you_type_button",
                                                      "help_search_as_you_type",
                                                      new_config->search_as_you_type);

    ui->hide_results_button = toggle_button_get(ui->builder,
                                                "hide_results_button",
                                                "help_hide_results",
                                                new_config->hide_results_on_empty_search);

    ui->limit_num_results_button =
        toggle_button_get(ui->builder, "limit_num_results_button", "help_limit_num_results", new_config->limit_results);

    ui->limit_num_results_spin = builder_init_widget(ui->builder, "limit_num_results_spin", "help_limit_num_results");
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui->limit_num_results_spin), (double)new_config->num_results);
    gtk_widget_set_sensitive(ui->limit_num_results_spin, new_config->limit_results);
    g_signal_connect(ui->limit_num_results_button,
                     "toggled",
                     G_CALLBACK(on_toggle_set_sensitive),
                     ui->limit_num_results_spin);

    // Database page
    ui->update_db_at_start_button = toggle_button_get(ui->builder,
                                                      "update_db_at_start_button",
                                                      "help_update_database_on_start",
                                                      new_config->update_database_on_launch);

    ui->auto_update_checkbox = toggle_button_get(ui->builder,
                                                 "auto_update_checkbox",
                                                 "help_update_database_every",
                                                 new_config->update_database_every);

    ui->auto_update_box = GTK_BOX(builder_init_widget(ui->builder, "auto_update_box", "help_update_database_every"));
    gtk_widget_set_sensitive(GTK_WIDGET(ui->auto_update_box), new_config->update_database_every);
    g_signal_connect(ui->auto_update_checkbox, "toggled", G_CALLBACK(on_toggle_set_sensitive), ui->auto_update_box);

    ui->auto_update_hours_spin_button =
        builder_init_widget(ui->builder, "auto_update_hours_spin_button", "help_update_database_every");
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui->auto_update_hours_spin_button),
                              (double)new_config->update_database_every_hours);

    ui->auto_update_minutes_spin_button =
        builder_init_widget(ui->builder, "auto_update_minutes_spin_button", "help_update_database_every");
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui->auto_update_minutes_spin_button),
                              (double)new_config->update_database_every_minutes);

    g_signal_connect(GTK_SPIN_BUTTON(ui->auto_update_hours_spin_button),
                     "value-changed",
                     G_CALLBACK(on_auto_update_hours_spin_button_changed),
                     ui->auto_update_minutes_spin_button);
    g_signal_connect(GTK_SPIN_BUTTON(ui->auto_update_minutes_spin_button),
                     "value-changed",
                     G_CALLBACK(on_auto_update_minutes_spin_button_changed),
                     ui->auto_update_hours_spin_button);

    // Dialog page
    ui->show_dialog_failed_opening = toggle_button_get(ui->builder,
                                                       "show_dialog_failed_opening",
                                                       "help_warn_failed_open",
                                                       new_config->show_dialog_failed_opening);

    // Include page
    ui->include_list = GTK_TREE_VIEW(builder_init_widget(ui->builder, "include_list", "help_include_list"));
    ui->include_model = pref_include_treeview_init(ui->include_list, new_config->locations);

    ui->include_add_button = builder_init_widget(ui->builder, "include_add_button", "help_include_add");
    g_signal_connect(ui->include_add_button, "clicked", G_CALLBACK(on_include_add_button_clicked), ui->include_model);

    ui->include_remove_button = builder_init_widget(ui->builder, "include_remove_button", "help_include_remove");
    g_signal_connect(ui->include_remove_button, "clicked", G_CALLBACK(on_remove_button_clicked), ui->include_list);

    ui->sel = gtk_tree_view_get_selection(ui->include_list);
    g_signal_connect(ui->sel, "changed", G_CALLBACK(on_list_selection_changed), ui->include_remove_button);

    // Exclude model
    ui->exclude_list = GTK_TREE_VIEW(builder_init_widget(ui->builder, "exclude_list", "help_exclude_list"));
    ui->exclude_model = pref_exclude_treeview_init(ui->exclude_list, new_config->exclude_locations);

    ui->exclude_add_button = builder_init_widget(ui->builder, "exclude_add_button", "help_exclude_add");
    g_signal_connect(ui->exclude_add_button, "clicked", G_CALLBACK(on_exclude_add_button_clicked), ui->exclude_model);

    ui->exclude_remove_button = builder_init_widget(ui->builder, "exclude_remove_button", "help_exclude_remove");
    g_signal_connect(ui->exclude_remove_button, "clicked", G_CALLBACK(on_remove_button_clicked), ui->exclude_list);

    ui->exclude_selection = gtk_tree_view_get_selection(ui->exclude_list);
    g_signal_connect(ui->exclude_selection,
                     "changed",
                     G_CALLBACK(on_list_selection_changed),
                     ui->exclude_remove_button);

    ui->exclude_hidden_items_button = toggle_button_get(ui->builder,
                                                        "exclude_hidden_items_button",
                                                        "help_exclude_hidden",
                                                        new_config->exclude_hidden_items);

    ui->exclude_files_entry = GTK_ENTRY(builder_init_widget(ui->builder, "exclude_files_entry", "help_exclude_files"));
    ui->exclude_files_str = NULL;
    if (new_config->exclude_files) {
        ui->exclude_files_str = g_strjoinv(";", new_config->exclude_files);
        gtk_entry_set_text(ui->exclude_files_entry, ui->exclude_files_str);
    }
}

void
preferences_ui_launch(FsearchConfig *config,
                      GtkWindow *window,
                      FsearchPreferencesPage page,
                      void (*finsihed_cb)(FsearchConfig *)) {
    FsearchPreferencesInterface *ui = calloc(1, sizeof(FsearchPreferencesInterface));
    ui->new_config = config;
    ui->finished_cb = finsihed_cb;
    ui->window = window;

    preferences_ui_init(ui, page);

    gtk_widget_show(ui->dialog);
}

