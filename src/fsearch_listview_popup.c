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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "fsearch.h"
#include "fsearch_database_search.h"
#include "fsearch_listview_popup.h"

static void
add_file_properties_entry(GtkBuilder *builder) {
    FsearchApplication *app = FSEARCH_APPLICATION_DEFAULT;
    if (app && fsearch_application_has_file_manager_on_bus(app)) {
        GMenu *menu_properties_section =
            G_MENU(gtk_builder_get_object(builder, "fsearch_listview_menu_file_properties_section"));
        if (menu_properties_section) {
            GMenuItem *properties_item = g_menu_item_new(_("Properties…"), "win.file_properties");
            g_menu_append_item(menu_properties_section, properties_item);
            g_clear_object(&properties_item);
        }
    }
}

static void
fill_open_with_menu(GtkBuilder *builder, const char *name, FsearchDatabaseEntryType type) {
    GList *app_list = NULL;
    char *content_type = NULL;

    if (type == DATABASE_ENTRY_TYPE_FOLDER) {
        content_type = g_content_type_from_mime_type("inode/directory");
    }
    else {
        content_type = g_content_type_guess(name, NULL, 0, NULL);
    }

    if (!content_type) {
        goto clean_up;
    }

    app_list = g_app_info_get_all_for_type(content_type);

    GMenu *menu_mime = G_MENU(gtk_builder_get_object(builder, "fsearch_listview_menu_open_with_mime_section"));

    for (GList *list_iter = app_list; list_iter; list_iter = list_iter->next) {
        GAppInfo *app_info = list_iter->data;
        const char *display_name = g_app_info_get_display_name(app_info);
        const char *app_id = g_app_info_get_id(app_info);

        char detailed_action[1024] = "";
        snprintf(detailed_action, sizeof(detailed_action), "win.open_with('%s')", app_id);

        GMenuItem *menu_item = g_menu_item_new(display_name, detailed_action);
        g_menu_item_set_icon(menu_item, g_app_info_get_icon(app_info));
        g_menu_append_item(menu_mime, menu_item);
        g_clear_object(&menu_item);
    }

    char detailed_action[1024] = "";
    snprintf(detailed_action, sizeof(detailed_action), "win.open_with_other('%s')", content_type);
    GMenuItem *open_with_item = g_menu_item_new(_("Other Application…"), detailed_action);
    g_menu_append_item(menu_mime, open_with_item);
    g_clear_object(&open_with_item);

clean_up:
    g_clear_pointer(&content_type, g_free);
    if (app_list) {
        g_list_free_full(g_steal_pointer(&app_list), g_object_unref);
    }
}

gboolean
listview_popup_menu(GtkWidget *widget, const char *name, FsearchDatabaseEntryType type) {
    GtkBuilder *builder = gtk_builder_new_from_resource("/io/github/cboxdoerfer/fsearch/ui/menus.ui");

    fill_open_with_menu(builder, name, type);
    add_file_properties_entry(builder);

    GMenu *menu_root = G_MENU(gtk_builder_get_object(builder, "fsearch_listview_popup_menu"));
    GtkWidget *menu_widget = gtk_menu_new_from_model(G_MENU_MODEL(menu_root));
    g_clear_object(&builder);

    gtk_menu_attach_to_widget(GTK_MENU(menu_widget), GTK_WIDGET(widget), NULL);
#if !GTK_CHECK_VERSION(3, 22, 0)
    gtk_menu_popup(GTK_MENU(menu_widget), NULL, NULL, NULL, NULL, GDK_BUTTON_SECONDARY, time);
#else
    gtk_menu_popup_at_pointer(GTK_MENU(menu_widget), NULL);
#endif
    return TRUE;
}
