/*
   FSearch - A fast file search utility
   Copyright © 2016 Christian Boxdörfer

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

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>
#include "fsearch.h"
#include "ui_utils.h"

enum
{
    COLUMN_NAME,
    NUM_COLUMNS
};

typedef struct _FsearchPreferences {
    FsearchConfig *config;
    GtkTreeModel *include_model;
    GtkTreeModel *exclude_model;
    bool update_db;
    bool update_list;
} FsearchPreferences;

static void
location_tree_row_modified (GtkTreeModel *tree_model,
                           GtkTreePath  *path,
                           gpointer      user_data)
{
    FsearchPreferences *pref = user_data;
    pref->update_db = true;
}

static GtkTreeModel *
create_tree_model (FsearchPreferences *pref , GList *list)
{
    /* create list store */
    GtkListStore *store = gtk_list_store_new (NUM_COLUMNS,
                                              G_TYPE_STRING);

    /* add data to the list store */
    for (GList *l = list; l != NULL; l = l->next) {
        GtkTreeIter iter;
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            COLUMN_NAME, l->data,
                            -1);
    }
    g_signal_connect ((gpointer)store,
                      "row-changed",
                      G_CALLBACK (location_tree_row_modified),
                      pref);
    g_signal_connect ((gpointer)store,
                      "row-deleted",
                      G_CALLBACK (location_tree_row_modified),
                      pref);

    return GTK_TREE_MODEL (store);
}

static void
enable_dark_theme_infobar_response (GtkInfoBar *info_bar,
                                    gint response_id,
                                    gpointer user_data)
{
    if (response_id == GTK_RESPONSE_CLOSE) {
        gtk_widget_hide (GTK_WIDGET (info_bar));
        return;
    }
}

static void
enable_dark_theme_button_toggled (GtkToggleButton *togglebutton,
                                  gpointer user_data)
{
    GtkWidget *infobar = GTK_WIDGET (user_data);
    if (gtk_toggle_button_get_active (togglebutton)) {
        gtk_widget_show (infobar);
    }
    else {
        gtk_widget_hide (infobar);
    }
}

static void
limit_num_results_toggled (GtkToggleButton *togglebutton,
                           gpointer user_data)
{
    GtkWidget *spin = GTK_WIDGET (user_data);
    gtk_widget_set_sensitive (spin, gtk_toggle_button_get_active (togglebutton));
}

static void
remove_list_store_item (GtkTreeModel *model,
                        GtkTreePath  *path,
                        GtkTreeIter  *iter,
                        gpointer      userdata)
{
    gtk_list_store_remove (GTK_LIST_STORE (model), iter);
}

static void
on_remove_button_clicked (GtkButton *button,
                          gpointer user_data)
{
    GtkTreeView *tree_view = GTK_TREE_VIEW (user_data);
    GtkTreeSelection *sel = gtk_tree_view_get_selection (tree_view);
    gtk_tree_selection_selected_foreach (sel, remove_list_store_item, NULL);
}

static GList *
run_file_chooser_dialog (GtkButton *button, GtkTreeModel *model, GList *list)
{
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;

    GtkWidget *window = gtk_widget_get_toplevel (GTK_WIDGET (button));
    GtkWidget *dialog = gtk_file_chooser_dialog_new ("Open File",
                                                     GTK_WINDOW (window),
                                                     action,
                                                     "_Cancel",
                                                     GTK_RESPONSE_CANCEL,
                                                     "_Open",
                                                     GTK_RESPONSE_ACCEPT,
                                                     NULL);

    gint res = gtk_dialog_run (GTK_DIALOG (dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
        char *uri = gtk_file_chooser_get_uri (chooser);
        char *path = g_filename_from_uri (uri, NULL, NULL);
        g_free (uri);
        if (path) {
            if (!g_list_find_custom (list, path, (GCompareFunc)strcmp)) {
                GtkTreeIter iter;
                gtk_list_store_append (GTK_LIST_STORE (model), &iter);
                gtk_list_store_set (GTK_LIST_STORE (model), &iter, COLUMN_NAME, path, -1);
                list = g_list_append (list, path);
            }
            else {
                g_free (path);
            }
        }
    }

    gtk_widget_destroy (dialog);
    return list;
}

static void
on_exclude_add_button_clicked (GtkButton *button,
                               gpointer user_data)
{
    FsearchPreferences *pref = user_data;
    pref->config->exclude_locations = run_file_chooser_dialog (button, pref->exclude_model, pref->config->exclude_locations);
}

static void
on_include_add_button_clicked (GtkButton *button,
                               gpointer user_data)
{
    FsearchPreferences *pref = user_data;
    pref->config->locations = run_file_chooser_dialog (button, pref->include_model, pref->config->locations);
}

static void
on_list_selection_changed (GtkTreeSelection *sel,
                           gpointer user_data)
{
    gboolean selected = gtk_tree_selection_get_selected (sel, NULL, NULL);
    gtk_widget_set_sensitive (GTK_WIDGET (user_data), selected);
}

static GtkWidget *
builder_get_object (GtkBuilder *builder, const char *name)
{
    return GTK_WIDGET (gtk_builder_get_object (builder, name));
}

FsearchConfig *
preferences_ui_launch (FsearchConfig *config, GtkWindow *window, bool *update_db, bool *update_list)
{
    FsearchPreferences pref = {};
    pref.config = config_copy (config);
    GtkBuilder *builder = gtk_builder_new_from_resource ("/org/fsearch/fsearch/preferences.ui");
    GtkWidget *dialog = GTK_WIDGET (gtk_builder_get_object (builder, "FsearchPreferencesWindow"));
    gtk_window_set_transient_for (GTK_WINDOW (dialog), window);

    gtk_dialog_add_button (GTK_DIALOG (dialog), "_OK", GTK_RESPONSE_OK);
    gtk_dialog_add_button (GTK_DIALOG (dialog), "_Cancel", GTK_RESPONSE_CANCEL);

    // Interface page
    GtkToggleButton *enable_dark_theme_button = GTK_TOGGLE_BUTTON (builder_get_object (builder,
                                                                                       "enable_dark_theme_button"));
    gtk_toggle_button_set_active (enable_dark_theme_button,
                                  pref.config->enable_dark_theme);

    GtkInfoBar *enable_dark_theme_infobar = GTK_INFO_BAR (builder_get_object (builder,
                                                                              "enable_dark_theme_infobar"));
    g_signal_connect (enable_dark_theme_infobar,
                      "response",
                      G_CALLBACK (enable_dark_theme_infobar_response),
                      NULL);

    g_signal_connect (enable_dark_theme_button,
                      "toggled",
                      G_CALLBACK (enable_dark_theme_button_toggled),
                      enable_dark_theme_infobar);

    GtkToggleButton *show_tooltips_button = GTK_TOGGLE_BUTTON (builder_get_object (builder,
                                                                                   "show_tooltips_button"));
    gtk_toggle_button_set_active (show_tooltips_button,
                                  pref.config->enable_list_tooltips);

    GtkToggleButton *restore_win_size_button = GTK_TOGGLE_BUTTON (builder_get_object (builder,
                                                                                      "restore_win_size_button"));
    gtk_toggle_button_set_active (restore_win_size_button,
                                  pref.config->restore_window_size);

    GtkToggleButton *restore_column_config_button = GTK_TOGGLE_BUTTON (builder_get_object (builder,
                                                                                           "restore_column_config_button"));
    gtk_toggle_button_set_active (restore_column_config_button,
                                  pref.config->restore_column_config);

    GtkToggleButton *double_click_path_button = GTK_TOGGLE_BUTTON (builder_get_object (builder,
                                                                                       "double_click_path_button"));
    gtk_toggle_button_set_active (double_click_path_button,
                                  pref.config->double_click_path);

    GtkToggleButton *single_click_open_button = GTK_TOGGLE_BUTTON (builder_get_object (builder,
                                                                                       "single_click_open_button"));
    gtk_toggle_button_set_active (single_click_open_button,
                                  pref.config->single_click_open);

    GtkToggleButton *show_icons_button = GTK_TOGGLE_BUTTON (builder_get_object (builder,
                                                                                "show_icons_button"));
    gtk_toggle_button_set_active (show_icons_button,
                                  pref.config->show_listview_icons);

    GtkToggleButton *show_base_2_units = GTK_TOGGLE_BUTTON (builder_get_object (builder,
                                                                                "show_base_2_units"));
    gtk_toggle_button_set_active(show_base_2_units,
                                 pref.config->show_base_2_units);

    GtkComboBox *action_after_file_open = GTK_COMBO_BOX ( builder_get_object(builder,
                                                                             "action_after_file_open"));
    gtk_combo_box_set_active(action_after_file_open,
                             pref.config->action_after_file_open);

    GtkToggleButton *action_after_file_open_keyboard = GTK_TOGGLE_BUTTON (builder_get_object (builder,
                                                                                              "action_after_file_open_keyboard"));
    gtk_toggle_button_set_active(action_after_file_open_keyboard,
                                 pref.config->action_after_file_open_keyboard);

    GtkToggleButton *action_after_file_open_mouse = GTK_TOGGLE_BUTTON (builder_get_object (builder,
                                                                                           "action_after_file_open_mouse"));
    gtk_toggle_button_set_active(action_after_file_open_mouse,
                                 pref.config->action_after_file_open_mouse);

    // Search page
    GtkToggleButton *auto_search_in_path_button = GTK_TOGGLE_BUTTON (builder_get_object (builder,
                                                                                         "auto_search_in_path_button"));
    gtk_toggle_button_set_active (auto_search_in_path_button,
                                  pref.config->auto_search_in_path);

    GtkToggleButton *search_as_you_type_button = GTK_TOGGLE_BUTTON (builder_get_object (builder,
                                                                                        "search_as_you_type_button"));
    gtk_toggle_button_set_active (search_as_you_type_button,
                                  pref.config->search_as_you_type);

    GtkToggleButton *hide_results_button = GTK_TOGGLE_BUTTON (builder_get_object (builder,
                                                                                  "hide_results_button"));
    gtk_toggle_button_set_active (hide_results_button,
                                  pref.config->hide_results_on_empty_search);

    GtkToggleButton *limit_num_results_button = GTK_TOGGLE_BUTTON (builder_get_object (builder,
                                                                                       "limit_num_results_button"));
    gtk_toggle_button_set_active (limit_num_results_button,
                                  pref.config->limit_results);

    GtkWidget *limit_num_results_spin = builder_get_object (builder,
                                                            "limit_num_results_spin");
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (limit_num_results_spin),
                               (double)pref.config->num_results);
    gtk_widget_set_sensitive (limit_num_results_spin,
                              pref.config->limit_results);
    g_signal_connect (limit_num_results_button,
                      "toggled",
                      G_CALLBACK (limit_num_results_toggled),
                      limit_num_results_spin);

    // Database page
    GtkToggleButton *update_db_at_start_button = GTK_TOGGLE_BUTTON (builder_get_object (builder,
                                                                                        "update_db_at_start_button"));
    gtk_toggle_button_set_active (update_db_at_start_button,
                                  pref.config->update_database_on_launch);

    // Dialog page
    GtkToggleButton *show_dialog_failed_opening = GTK_TOGGLE_BUTTON (builder_get_object (builder,
                                                                                         "show_dialog_failed_opening"));
    gtk_toggle_button_set_active(show_dialog_failed_opening,
                                 pref.config->show_dialog_failed_opening);

    // Include page
    pref.include_model = create_tree_model (&pref, pref.config->locations);
    GtkTreeView *include_list = GTK_TREE_VIEW (builder_get_object (builder,
                                                                   "include_list"));
    gtk_tree_view_set_model (include_list, pref.include_model);
    gtk_tree_view_set_search_column (include_list, COLUMN_NAME);
    gtk_tree_view_set_headers_visible (include_list, FALSE);

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes ("Name",
                                                                       renderer,
                                                                       "text",
                                                                       COLUMN_NAME,
                                                                       NULL);
    gtk_tree_view_append_column (include_list, col);

    GtkWidget *include_add_button = builder_get_object (builder,
                                                        "include_add_button");
    g_signal_connect (include_add_button,
                      "clicked",
                      G_CALLBACK (on_include_add_button_clicked),
                      &pref);

    GtkWidget *include_remove_button = builder_get_object (builder,
                                                           "include_remove_button");
    g_signal_connect (include_remove_button,
                      "clicked",
                      G_CALLBACK (on_remove_button_clicked),
                      include_list);

    GtkTreeSelection *sel = gtk_tree_view_get_selection (include_list);
    g_signal_connect (sel,
                      "changed",
                      G_CALLBACK (on_list_selection_changed),
                      include_remove_button);

    GtkToggleButton *follow_symlinks_button = GTK_TOGGLE_BUTTON (builder_get_object (builder,
                                                                                     "follow_symlinks_button"));
    gtk_toggle_button_set_active (follow_symlinks_button,
                                  pref.config->follow_symlinks);


    // Exclude model
    pref.exclude_model = create_tree_model (&pref, pref.config->exclude_locations);
    GtkTreeView *exclude_list = GTK_TREE_VIEW (builder_get_object (builder,
                                                                   "exclude_list"));
    gtk_tree_view_set_model (exclude_list, pref.exclude_model);
    gtk_tree_view_set_search_column (exclude_list, COLUMN_NAME);
    gtk_tree_view_set_headers_visible (exclude_list, FALSE);

    renderer = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes ("Name",
                                                    renderer,
                                                    "text",
                                                    COLUMN_NAME,
                                                    NULL);
    gtk_tree_view_append_column (exclude_list, col);

    GtkWidget *exclude_add_button = builder_get_object (builder,
                                                        "exclude_add_button");
    g_signal_connect (exclude_add_button,
                      "clicked",
                      G_CALLBACK (on_exclude_add_button_clicked),
                      &pref);

    GtkWidget *exclude_remove_button = builder_get_object (builder,
                                                           "exclude_remove_button");
    g_signal_connect (exclude_remove_button,
                      "clicked",
                      G_CALLBACK (on_remove_button_clicked),
                      exclude_list);

    GtkTreeSelection *exclude_selection = gtk_tree_view_get_selection (exclude_list);
    g_signal_connect (exclude_selection,
                      "changed",
                      G_CALLBACK (on_list_selection_changed),
                      exclude_remove_button);

    GtkToggleButton *exclude_hidden_items_button = GTK_TOGGLE_BUTTON (builder_get_object (builder,
                                                                                          "exclude_hidden_items_button"));
    gtk_toggle_button_set_active (exclude_hidden_items_button,
                                  pref.config->exclude_hidden_items);

    GtkEntry *exclude_files_entry = GTK_ENTRY (builder_get_object (builder,
                                                                   "exclude_files_entry"));
    gchar *exclude_files_str = NULL;
    if (pref.config->exclude_files) {
        exclude_files_str = g_strjoinv (";", pref.config->exclude_files);
        gtk_entry_set_text (exclude_files_entry, exclude_files_str);
    }

    gint response = gtk_dialog_run (GTK_DIALOG (dialog));

    if (response == GTK_RESPONSE_OK) {
        pref.config->search_as_you_type = gtk_toggle_button_get_active (search_as_you_type_button);
        pref.config->auto_search_in_path = gtk_toggle_button_get_active (auto_search_in_path_button);
        pref.config->hide_results_on_empty_search = gtk_toggle_button_get_active (hide_results_button);
        pref.config->limit_results = gtk_toggle_button_get_active (limit_num_results_button);
        pref.config->num_results = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (limit_num_results_spin));
        pref.config->enable_dark_theme = gtk_toggle_button_get_active (enable_dark_theme_button);
        pref.config->restore_column_config = gtk_toggle_button_get_active (restore_column_config_button);
        pref.config->double_click_path = gtk_toggle_button_get_active (double_click_path_button);
        pref.config->enable_list_tooltips = gtk_toggle_button_get_active (show_tooltips_button);
        pref.config->restore_window_size = gtk_toggle_button_get_active (restore_win_size_button);
        pref.config->update_database_on_launch = gtk_toggle_button_get_active (update_db_at_start_button);
        pref.config->show_base_2_units = gtk_toggle_button_get_active (show_base_2_units);
        pref.config->action_after_file_open = gtk_combo_box_get_active(action_after_file_open);
        pref.config->action_after_file_open_keyboard = gtk_toggle_button_get_active (action_after_file_open_keyboard);
        pref.config->action_after_file_open_mouse = gtk_toggle_button_get_active (action_after_file_open_mouse);
        // Dialogs
        pref.config->show_dialog_failed_opening = gtk_toggle_button_get_active (show_dialog_failed_opening);

        bool old_single_click_open = pref.config->single_click_open;
        pref.config->single_click_open = gtk_toggle_button_get_active (single_click_open_button);
        if (old_single_click_open != pref.config->single_click_open) {
            pref.update_list = true;
        }

        bool old_show_icons = pref.config->show_listview_icons;
        pref.config->show_listview_icons = gtk_toggle_button_get_active (show_icons_button);
        if (old_show_icons != pref.config->show_listview_icons) {
            pref.update_list = true;
        }

        bool old_exclude_hidden_items = pref.config->exclude_hidden_items;
        pref.config->exclude_hidden_items = gtk_toggle_button_get_active (exclude_hidden_items_button);
        if (old_exclude_hidden_items != pref.config->exclude_hidden_items) {
            pref.update_db = true;
        }

        bool old_follow_symlinks = pref.config->follow_symlinks;
        pref.config->follow_symlinks = gtk_toggle_button_get_active (follow_symlinks_button);
        if (old_follow_symlinks != pref.config->follow_symlinks) {
            pref.update_db = true;
        }

        if ((exclude_files_str
            && strcmp (exclude_files_str, gtk_entry_get_text (exclude_files_entry)))
            || (!exclude_files_str && strlen (gtk_entry_get_text (exclude_files_entry)) > 0)) {
            pref.update_db = true;
        }

        g_object_set(gtk_settings_get_default(),
                     "gtk-application-prefer-dark-theme",
                     pref.config->enable_dark_theme,
                     NULL );

        if (pref.update_db) {
            if (pref.config->exclude_files) {
                g_strfreev (pref.config->exclude_files);
                pref.config->exclude_files = NULL;
            }
            pref.config->exclude_files = g_strsplit (gtk_entry_get_text (exclude_files_entry), ";", -1);
        }
    }
    else {
        config_free (pref.config);
        pref.config = NULL;
    }

    if (exclude_files_str) {
        free (exclude_files_str);
        exclude_files_str = NULL;
    }

    g_object_unref (builder);
    gtk_widget_destroy (dialog);

    *update_db = pref.update_db;
    *update_list = pref.update_list;
    return pref.config;
}


