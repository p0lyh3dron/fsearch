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

#pragma once

#include "fsearch_array.h"
#include "fsearch_filter.h"
#include "fsearch_query.h"

#include <gio/gio.h>
#include <glib.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DATABASE_ENTRY_TYPE_NONE,
    DATABASE_ENTRY_TYPE_FOLDER,
    DATABASE_ENTRY_TYPE_FILE,
    NUM_DATABASE_ENTRY_TYPES,
} FsearchDatabaseEntryType;

typedef enum {
    DATABASE_INDEX_FLAG_NAME = 1 << 0,
    DATABASE_INDEX_FLAG_PATH = 1 << 1,
    DATABASE_INDEX_FLAG_SIZE = 1 << 2,
    DATABASE_INDEX_FLAG_MODIFICATION_TIME = 1 << 3,
    DATABASE_INDEX_FLAG_ACCESS_TIME = 1 << 4,
    DATABASE_INDEX_FLAG_CREATION_TIME = 1 << 5,
    DATABASE_INDEX_FLAG_STATUS_CHANGE_TIME = 1 << 6,
} FsearchDatabaseIndexFlags;

typedef enum {
    DATABASE_INDEX_TYPE_NAME,
    DATABASE_INDEX_TYPE_PATH,
    DATABASE_INDEX_TYPE_SIZE,
    DATABASE_INDEX_TYPE_MODIFICATION_TIME,
    DATABASE_INDEX_TYPE_ACCESS_TIME,
    DATABASE_INDEX_TYPE_CREATION_TIME,
    DATABASE_INDEX_TYPE_STATUS_CHANGE_TIME,
    DATABASE_INDEX_TYPE_FILETYPE,
    NUM_DATABASE_INDEX_TYPES,
} FsearchDatabaseIndexType;

typedef struct FsearchDatabaseEntryFile FsearchDatabaseEntry;
typedef struct FsearchDatabaseEntryFile FsearchDatabaseEntryFile;
typedef struct FsearchDatabaseEntryFolder FsearchDatabaseEntryFolder;

typedef struct FsearchDatabaseView FsearchDatabaseView;
typedef struct FsearchDatabase FsearchDatabase;

typedef void (*FsearchDatabaseViewNotifyFunc)(FsearchDatabaseView *view, gpointer user_data);

void
db_view_free(FsearchDatabaseView *view);

FsearchDatabaseView *
db_view_new(const char *query_text,
            FsearchQueryFlags flags,
            FsearchFilter *filter,
            FsearchDatabaseIndexType sort_order,
            FsearchDatabaseViewNotifyFunc view_changed_func,
            FsearchDatabaseViewNotifyFunc search_started_func,
            FsearchDatabaseViewNotifyFunc search_finished_func,
            FsearchDatabaseViewNotifyFunc sort_started_func,
            FsearchDatabaseViewNotifyFunc sort_finished_func,
            gpointer user_data);

void
db_view_set_filter(FsearchDatabaseView *view, FsearchFilter *filter);

void
db_view_set_query_flags(FsearchDatabaseView *view, FsearchQueryFlags query_flags);

void
db_view_set_query_text(FsearchDatabaseView *view, const char *query_text);

void
db_view_set_sort_order(FsearchDatabaseView *view, FsearchDatabaseIndexType sort_order);

uint32_t
db_view_get_num_folders(FsearchDatabaseView *view);

uint32_t
db_view_get_num_files(FsearchDatabaseView *view);

uint32_t
db_view_get_num_entries(FsearchDatabaseView *view);

FsearchDatabaseIndexType
db_view_get_sort_order(FsearchDatabaseView *view);

void
db_view_register(FsearchDatabase *db, FsearchDatabaseView *view);

void
db_view_unregister(FsearchDatabaseView *view);

FsearchDatabaseEntry *
db_view_get_entry(FsearchDatabaseView *view, uint32_t idx);

FsearchQueryFlags
db_view_get_query_flags(FsearchDatabaseView *view);

FsearchQuery *
db_view_get_query(FsearchDatabaseView *view);

bool
db_load(FsearchDatabase *db, const char *path, void (*status_cb)(const char *));

bool
db_scan(FsearchDatabase *db, GCancellable *cancellable, void (*status_cb)(const char *));

FsearchDatabase *
db_ref(FsearchDatabase *db);

void
db_unref(FsearchDatabase *db);

FsearchDatabase *
db_new(GList *includes, GList *excludes, char **exclude_files, bool exclude_hidden);

bool
db_save(FsearchDatabase *db, const char *path);

time_t
db_get_timestamp(FsearchDatabase *db);

uint32_t
db_get_num_files(FsearchDatabase *db);

uint32_t
db_get_num_folders(FsearchDatabase *db);

uint32_t
db_get_num_entries(FsearchDatabase *db);

void
db_unlock(FsearchDatabase *db);

void
db_lock(FsearchDatabase *db);

bool
db_try_lock(FsearchDatabase *db);

DynamicArray *
db_get_folders_copy(FsearchDatabase *db);

DynamicArray *
db_get_files_copy(FsearchDatabase *db);

DynamicArray *
db_get_folders(FsearchDatabase *db);

DynamicArray *
db_get_files(FsearchDatabase *db);

bool
db_has_entries_sorted_by_type(FsearchDatabase *db, FsearchDatabaseIndexType sort_type);

DynamicArray *
db_get_folders_sorted_copy(FsearchDatabase *db, FsearchDatabaseIndexType sort_type);

DynamicArray *
db_get_files_sorted_copy(FsearchDatabase *db, FsearchDatabaseIndexType sort_type);

DynamicArray *
db_get_folders_sorted(FsearchDatabase *db, FsearchDatabaseIndexType sort_type);

DynamicArray *
db_get_files_sorted(FsearchDatabase *db, FsearchDatabaseIndexType sort_type);

FsearchDatabaseEntryFolder *
db_entry_get_parent(FsearchDatabaseEntry *entry);

time_t
db_entry_get_mtime(FsearchDatabaseEntry *entry);

off_t
db_entry_get_size(FsearchDatabaseEntry *entry);

const char *
db_entry_get_name(FsearchDatabaseEntry *entry);

GString *
db_entry_get_path_full(FsearchDatabaseEntry *entry);

GString *
db_entry_get_path(FsearchDatabaseEntry *entry);

void
db_entry_append_path(FsearchDatabaseEntry *entry, GString *str);

FsearchDatabaseEntryType
db_entry_get_type(FsearchDatabaseEntry *entry);

int
db_entry_compare_entries_by_path(FsearchDatabaseEntry **a, FsearchDatabaseEntry **b);

int
db_entry_compare_entries_by_name(FsearchDatabaseEntry **a, FsearchDatabaseEntry **b);

int
db_entry_compare_entries_by_size(FsearchDatabaseEntry **a, FsearchDatabaseEntry **b);

int
db_entry_compare_entries_by_type(FsearchDatabaseEntry **a, FsearchDatabaseEntry **b);

int
db_entry_compare_entries_by_modification_time(FsearchDatabaseEntry **a, FsearchDatabaseEntry **b);

int
db_entry_compare_entries_by_position(FsearchDatabaseEntry **a, FsearchDatabaseEntry **b);
