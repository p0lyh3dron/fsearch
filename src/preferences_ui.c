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

enum { COLUMN_NAME, NUM_COLUMNS };

guint help_reset_timeout_id = 0;
static GtkWidget *help_stack = NULL;
static GtkWidget *help_description = NULL;

static void
limit_num_results_toggled(GtkToggleButton *togglebutton, gpointer user_data) {
    GtkWidget *spin = GTK_WIDGET(user_data);
    gtk_widget_set_sensitive(spin, gtk_toggle_button_get_active(togglebutton));
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

    gint res = gtk_dialog_run(GTK_DIALOG(dialog));
#else
    GtkFileChooserNative *dialog =
        gtk_file_chooser_native_new(_("Select folder"), GTK_WINDOW(window), action, _("_Select"), _("_Cancel"));

    gint res = gtk_native_dialog_run(GTK_NATIVE_DIALOG(dialog));
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
    FsearchPreferences *pref = user_data;
    char *path = run_file_chooser_dialog(button);
    if (!path) {
        return;
    }
    pref_exclude_treeview_row_add(pref, path);
}

static void
on_include_add_button_clicked(GtkButton *button, gpointer user_data) {
    FsearchPreferences *pref = user_data;
    char *path = run_file_chooser_dialog(button);
    if (!path) {
        return;
    }
    pref_include_treeview_row_add(pref, path);
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
void
on_help_expander_activate(GtkExpander *expander, gpointer user_data) {
    GtkWindow *win = GTK_WINDOW(user_data);
    if (!win) {
        return;
    }
    // this obeys geometry constraints, so by requesting width/height of 1
    // the window actually get resized to its preferred size
    gtk_window_resize(win, 1, 1);
}

FsearchConfig *
preferences_ui_launch(FsearchConfig *config,
                      GtkWindow *window,
                      FsearchPreferencesPage page,
                      bool *update_db,
                      bool *update_list,
                      bool *update_search) {
    FsearchPreferences pref = {};
    pref.config = config_copy(config);
    GtkBuilder *builder = gtk_builder_new_from_resource("/org/fsearch/fsearch/preferences.ui");
    GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(builder, "FsearchPreferencesWindow"));
    gtk_window_set_transient_for(GTK_WINDOW(dialog), window);

    gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog), _("_OK"), GTK_RESPONSE_OK);

    GtkWidget *main_notebook = GTK_WIDGET(gtk_builder_get_object(builder, "pref_main_notebook"));
    gtk_notebook_set_current_page(GTK_NOTEBOOK(main_notebook), page);

    help_stack = GTK_WIDGET(gtk_builder_get_object(builder, "help_stack"));
    help_description = GTK_WIDGET(gtk_builder_get_object(builder, "help_help"));
    GtkWidget *help_expander = GTK_WIDGET(gtk_builder_get_object(builder, "help_expander"));
    g_signal_connect(help_expander, "activate", G_CALLBACK(on_help_expander_activate), dialog);

    // Interface page
    GtkToggleButton *enable_dark_theme_button =
        toggle_button_get(builder, "enable_dark_theme_button", "help_dark_theme", pref.config->enable_dark_theme);

    GtkToggleButton *show_menubar_button =
        toggle_button_get(builder, "show_menubar_button", "help_csd", !pref.config->show_menubar);

    GtkToggleButton *show_tooltips_button =
        toggle_button_get(builder, "show_tooltips_button", "help_show_tooltips", pref.config->enable_list_tooltips);

    GtkToggleButton *restore_win_size_button =
        toggle_button_get(builder, "restore_win_size_button", "help_window_size", pref.config->restore_window_size);

    GtkToggleButton *restore_sort_order_button = toggle_button_get(builder,
                                                                   "restore_sort_order_button",
                                                                   "help_restore_sort_order",
                                                                   pref.config->restore_sort_order);

    GtkToggleButton *restore_column_config_button = toggle_button_get(builder,
                                                                      "restore_column_config_button",
                                                                      "help_restore_column_config",
                                                                      pref.config->restore_column_config);

    GtkToggleButton *double_click_path_button = toggle_button_get(builder,
                                                                  "double_click_path_button",
                                                                  "help_double_click_path",
                                                                  pref.config->double_click_path);

    GtkToggleButton *single_click_open_button = toggle_button_get(builder,
                                                                  "single_click_open_button",
                                                                  "help_single_click_open",
                                                                  pref.config->single_click_open);

    GtkToggleButton *show_icons_button =
        toggle_button_get(builder, "show_icons_button", "help_show_icons", pref.config->show_listview_icons);

    GtkToggleButton *highlight_search_terms = toggle_button_get(builder,
                                                                "highlight_search_terms",
                                                                "help_highlight_search_terms",
                                                                pref.config->highlight_search_terms);

    GtkToggleButton *show_base_2_units =
        toggle_button_get(builder, "show_base_2_units", "help_units", pref.config->show_base_2_units);

    GtkBox *action_after_file_open_box =
        GTK_BOX(builder_init_widget(builder, "action_after_file_open_box", "help_action_after_open"));
    GtkComboBox *action_after_file_open =
        GTK_COMBO_BOX(builder_init_widget(builder, "action_after_file_open", "help_action_after_open"));
    gtk_combo_box_set_active(action_after_file_open, pref.config->action_after_file_open);

    g_signal_connect(action_after_file_open,
                     "changed",
                     G_CALLBACK(action_after_file_open_changed),
                     action_after_file_open_box);

    if (pref.config->action_after_file_open != ACTION_AFTER_OPEN_NOTHING) {
        gtk_widget_set_sensitive(GTK_WIDGET(action_after_file_open_box), TRUE);
    }
    else {
        gtk_widget_set_sensitive(GTK_WIDGET(action_after_file_open_box), FALSE);
    }

    GtkToggleButton *action_after_file_open_keyboard = toggle_button_get(builder,
                                                                         "action_after_file_open_keyboard",
                                                                         "help_action_after_open",
                                                                         pref.config->action_after_file_open_keyboard);

    GtkToggleButton *action_after_file_open_mouse = toggle_button_get(builder,
                                                                      "action_after_file_open_mouse",
                                                                      "help_action_after_open",
                                                                      pref.config->action_after_file_open_mouse);

    GtkToggleButton *show_indexing_status = toggle_button_get(builder,
                                                              "show_indexing_status_button",
                                                              "help_show_indexing_status",
                                                              pref.config->show_indexing_status);

    // Search page
    GtkToggleButton *auto_search_in_path_button =
        toggle_button_get(builder, "auto_search_in_path_button", "help_auto_path", pref.config->auto_search_in_path);

    GtkToggleButton *auto_match_case_button =
        toggle_button_get(builder, "auto_match_case_button", "help_auto_case", pref.config->auto_match_case);

    GtkToggleButton *search_as_you_type_button = toggle_button_get(builder,
                                                                   "search_as_you_type_button",
                                                                   "help_search_as_you_type",
                                                                   pref.config->search_as_you_type);

    GtkToggleButton *hide_results_button = toggle_button_get(builder,
                                                             "hide_results_button",
                                                             "help_hide_results",
                                                             pref.config->hide_results_on_empty_search);

    GtkToggleButton *limit_num_results_button =
        toggle_button_get(builder, "limit_num_results_button", "help_limit_num_results", pref.config->limit_results);

    GtkWidget *limit_num_results_spin =
        builder_init_widget(builder, "limit_num_results_spin", "help_limit_num_results");
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(limit_num_results_spin), (double)pref.config->num_results);
    gtk_widget_set_sensitive(limit_num_results_spin, pref.config->limit_results);
    g_signal_connect(limit_num_results_button,
                     "toggled",
                     G_CALLBACK(limit_num_results_toggled),
                     limit_num_results_spin);

    // Database page
    GtkToggleButton *update_db_at_start_button = toggle_button_get(builder,
                                                                   "update_db_at_start_button",
                                                                   "help_update_database_on_start",
                                                                   pref.config->update_database_on_launch);

    // Dialog page
    GtkToggleButton *show_dialog_failed_opening = toggle_button_get(builder,
                                                                    "show_dialog_failed_opening",
                                                                    "help_warn_failed_open",
                                                                    pref.config->show_dialog_failed_opening);

    // Include page
    // pref.include_model = create_include_tree_model (&pref,
    // pref.config->locations);
    GtkTreeView *include_list = GTK_TREE_VIEW(builder_init_widget(builder, "include_list", "help_include_list"));
    pref_include_treeview_init(include_list, &pref);

    GtkWidget *include_add_button = builder_init_widget(builder, "include_add_button", "help_include_add");
    g_signal_connect(include_add_button, "clicked", G_CALLBACK(on_include_add_button_clicked), &pref);

    GtkWidget *include_remove_button = builder_init_widget(builder, "include_remove_button", "help_include_remove");
    g_signal_connect(include_remove_button, "clicked", G_CALLBACK(on_remove_button_clicked), include_list);

    GtkTreeSelection *sel = gtk_tree_view_get_selection(include_list);
    g_signal_connect(sel, "changed", G_CALLBACK(on_list_selection_changed), include_remove_button);

    // Exclude model
    // pref.exclude_model = create_exclude_tree_model (&pref,
    // pref.config->exclude_locations);
    GtkTreeView *exclude_list = GTK_TREE_VIEW(builder_init_widget(builder, "exclude_list", "help_exclude_list"));
    pref_exclude_treeview_init(exclude_list, &pref);

    GtkWidget *exclude_add_button = builder_init_widget(builder, "exclude_add_button", "help_exclude_add");
    g_signal_connect(exclude_add_button, "clicked", G_CALLBACK(on_exclude_add_button_clicked), &pref);

    GtkWidget *exclude_remove_button = builder_init_widget(builder, "exclude_remove_button", "help_exclude_remove");
    g_signal_connect(exclude_remove_button, "clicked", G_CALLBACK(on_remove_button_clicked), exclude_list);

    GtkTreeSelection *exclude_selection = gtk_tree_view_get_selection(exclude_list);
    g_signal_connect(exclude_selection, "changed", G_CALLBACK(on_list_selection_changed), exclude_remove_button);

    GtkToggleButton *exclude_hidden_items_button = toggle_button_get(builder,
                                                                     "exclude_hidden_items_button",
                                                                     "help_exclude_hidden",
                                                                     pref.config->exclude_hidden_items);

    GtkEntry *exclude_files_entry =
        GTK_ENTRY(builder_init_widget(builder, "exclude_files_entry", "help_exclude_files"));
    gchar *exclude_files_str = NULL;
    if (pref.config->exclude_files) {
        exclude_files_str = g_strjoinv(";", pref.config->exclude_files);
        gtk_entry_set_text(exclude_files_entry, exclude_files_str);
    }

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    if (response == GTK_RESPONSE_OK) {
        pref.config->search_as_you_type = gtk_toggle_button_get_active(search_as_you_type_button);
        pref.config->enable_dark_theme = gtk_toggle_button_get_active(enable_dark_theme_button);
        pref.config->show_menubar = !gtk_toggle_button_get_active(show_menubar_button);
        pref.config->restore_column_config = gtk_toggle_button_get_active(restore_column_config_button);
        pref.config->restore_sort_order = gtk_toggle_button_get_active(restore_sort_order_button);
        pref.config->double_click_path = gtk_toggle_button_get_active(double_click_path_button);
        pref.config->enable_list_tooltips = gtk_toggle_button_get_active(show_tooltips_button);
        pref.config->restore_window_size = gtk_toggle_button_get_active(restore_win_size_button);
        pref.config->update_database_on_launch = gtk_toggle_button_get_active(update_db_at_start_button);
        pref.config->show_base_2_units = gtk_toggle_button_get_active(show_base_2_units);
        pref.config->action_after_file_open = gtk_combo_box_get_active(action_after_file_open);
        pref.config->action_after_file_open_keyboard = gtk_toggle_button_get_active(action_after_file_open_keyboard);
        pref.config->action_after_file_open_mouse = gtk_toggle_button_get_active(action_after_file_open_mouse);
        pref.config->show_indexing_status = gtk_toggle_button_get_active(show_indexing_status);
        // Dialogs
        pref.config->show_dialog_failed_opening = gtk_toggle_button_get_active(show_dialog_failed_opening);

        pref.config->auto_search_in_path = gtk_toggle_button_get_active(auto_search_in_path_button);
        pref.config->auto_match_case = gtk_toggle_button_get_active(auto_match_case_button);
        pref.config->hide_results_on_empty_search = gtk_toggle_button_get_active(hide_results_button);
        pref.config->limit_results = gtk_toggle_button_get_active(limit_num_results_button);
        pref.config->num_results = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(limit_num_results_spin));
        pref.config->highlight_search_terms = gtk_toggle_button_get_active(highlight_search_terms);
        pref.config->single_click_open = gtk_toggle_button_get_active(single_click_open_button);
        pref.config->show_listview_icons = gtk_toggle_button_get_active(show_icons_button);
        pref.config->exclude_hidden_items = gtk_toggle_button_get_active(exclude_hidden_items_button);

        if (config->auto_search_in_path != pref.config->auto_search_in_path ||
            config->auto_match_case != pref.config->auto_match_case ||
            config->hide_results_on_empty_search != pref.config->hide_results_on_empty_search ||
            config->limit_results != pref.config->limit_results || config->num_results != pref.config->num_results) {
            pref.update_search = true;
        }

        if (config->highlight_search_terms != pref.config->highlight_search_terms ||
            config->single_click_open != pref.config->single_click_open ||
            config->show_listview_icons != pref.config->show_listview_icons) {
            pref.update_list = true;
        }

        if (config->exclude_hidden_items != pref.config->exclude_hidden_items) {
            pref.update_db = true;
        }

        if ((exclude_files_str && strcmp(exclude_files_str, gtk_entry_get_text(exclude_files_entry))) ||
            (!exclude_files_str && strlen(gtk_entry_get_text(exclude_files_entry)) > 0)) {
            pref.update_db = true;
        }

        g_object_set(gtk_settings_get_default(),
                     "gtk-application-prefer-dark-theme",
                     pref.config->enable_dark_theme,
                     NULL);

        if (pref.update_db) {
            if (pref.config->exclude_files) {
                g_strfreev(pref.config->exclude_files);
                pref.config->exclude_files = NULL;
            }
            pref.config->exclude_files = g_strsplit(gtk_entry_get_text(exclude_files_entry), ";", -1);
        }

        if (pref.config->locations) {
            g_list_free_full(pref.config->locations, (GDestroyNotify)fsearch_include_path_free);
        }
        pref.config->locations = pref_include_treeview_data_get(include_list);

        if (pref.config->exclude_locations) {
            g_list_free_full(pref.config->exclude_locations, (GDestroyNotify)fsearch_exclude_path_free);
        }
        pref.config->exclude_locations = pref_exclude_treeview_data_get(exclude_list);
    }
    else {
        config_free(pref.config);
        pref.config = NULL;
    }

    if (exclude_files_str) {
        free(exclude_files_str);
        exclude_files_str = NULL;
    }

    if (help_reset_timeout_id != 0) {
        g_source_remove(help_reset_timeout_id);
        help_reset_timeout_id = 0;
    }
    help_stack = NULL;

    g_object_unref(builder);
    gtk_widget_destroy(dialog);

    *update_db = pref.update_db;
    *update_list = pref.update_list;
    *update_search = pref.update_search;
    return pref.config;
}

