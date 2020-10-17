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

#define _GNU_SOURCE
#include "database_search.h"

#include <assert.h>
#include <ctype.h>
#include <fnmatch.h>
#include <pcre.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "debug.h"
#include "fsearch_timer.h"
#include "fsearch_window.h"
#include "string_utils.h"
#include "token.h"

struct _DatabaseSearchEntry {
    BTreeNode *node;
    uint32_t pos;
};

typedef struct search_context_s {
    FsearchQuery *query;
    BTreeNode **results;
    bool *terminate;
    uint32_t num_results;
    uint32_t start_pos;
    uint32_t end_pos;
} search_thread_context_t;

static DatabaseSearchResult *
db_search(DatabaseSearch *search, FsearchQuery *q);

static DatabaseSearchResult *
db_search_empty(FsearchQuery *query);

DatabaseSearchEntry *
db_search_entry_new(BTreeNode *node, uint32_t pos);

static void
db_search_entry_free(DatabaseSearchEntry *entry);

static void
db_search_notify_cancelled(FsearchQuery *query) {
    if (query->db) {
        db_unref(query->db);
    }
    if (query->callback_cancelled) {
        query->callback_cancelled(query->callback_cancelled_data);
    }
}

static gpointer
db_search_thread(gpointer user_data) {
    DatabaseSearch *search = user_data;

    g_mutex_lock(&search->query_mutex);
    while (true) {
        g_cond_wait(&search->search_thread_start_cond, &search->query_mutex);
        if (search->search_thread_terminate) {
            break;
        }
        while (search->query_ctx) {
            FsearchQuery *query = search->query_ctx;
            if (!query) {
                break;
            }
            search->query_ctx = NULL;
            search->search_terminate = false;
            g_mutex_unlock(&search->query_mutex);
            // if query is empty string we are done here
            DatabaseSearchResult *result = NULL;
            if (fs_str_is_empty(query->text)) {
                if (query->pass_on_empty_query) {
                    result = db_search_empty(query);
                }
                else {
                    result = calloc(1, sizeof(DatabaseSearchResult));
                }
            }
            else {
                result = db_search(search, query);
            }
            g_mutex_lock(&search->query_mutex);
            if (result) {
                result->cb_data = query->callback_data;
                result->db = query->db;
                query->callback(result);
            }
            else {
                db_search_notify_cancelled(query);
            }
            fsearch_query_free(query);
            query = NULL;
        }
    }
    g_mutex_unlock(&search->query_mutex);
    return NULL;
}

static void
search_thread_context_free(search_thread_context_t *ctx) {
    if (!ctx) {
        return;
    }
    if (ctx->results) {
        g_free(ctx->results);
        ctx->results = NULL;
    }
    if (ctx) {
        g_free(ctx);
        ctx = NULL;
    }
}

static search_thread_context_t *
search_thread_context_new(FsearchQuery *query,
                          bool *terminate,
                          uint32_t start_pos,
                          uint32_t end_pos) {
    search_thread_context_t *ctx = calloc(1, sizeof(search_thread_context_t));
    assert(ctx != NULL);
    assert(end_pos >= start_pos);

    ctx->query = query;
    ctx->terminate = terminate;
    ctx->results = calloc(end_pos - start_pos + 1, sizeof(BTreeNode *));
    assert(ctx->results != NULL);

    ctx->num_results = 0;
    ctx->start_pos = start_pos;
    ctx->end_pos = end_pos;
    return ctx;
}

static inline bool
filter_node(BTreeNode *node, FsearchQuery *query, const char *haystack) {
    if (!query->filter) {
        return true;
    }
    if (query->filter->type == FSEARCH_FILTER_NONE && query->filter->query == NULL) {
        return true;
    }
    bool is_dir = node->is_dir;
    if (query->filter->type == FSEARCH_FILTER_FILES && is_dir) {
        return false;
    }
    if (query->filter->type == FSEARCH_FILTER_FOLDERS && !is_dir) {
        return false;
    }
    if (query->filter_token) {
        uint32_t num_found = 0;
        while (true) {
            if (num_found == query->num_filter_token) {
                return true;
            }
            FsearchToken *t = query->filter_token[num_found++];
            if (!t) {
                return false;
            }

            if (!t->search_func(haystack, t->text, t)) {
                return false;
            }
        }
        return false;
    }
    return true;
}

static void *
db_search_worker(void *user_data) {
    search_thread_context_t *ctx = (search_thread_context_t *)user_data;
    assert(ctx != NULL);
    assert(ctx->results != NULL);

    FsearchQuery *query = ctx->query;
    const uint32_t start = ctx->start_pos;
    const uint32_t end = ctx->end_pos;
    const uint32_t max_results = query->max_results;
    const uint32_t num_token = query->num_token;
    FsearchToken **token = query->token;
    const uint32_t search_in_path = query->flags.search_in_path;
    const uint32_t auto_search_in_path = query->flags.auto_search_in_path;
    DynamicArray *entries = db_get_entries(query->db);
    BTreeNode **results = ctx->results;

    if (!entries) {
        ctx->num_results = 0;
        trace("[database_search] entries empty\n");
        return NULL;
    }

    uint32_t num_results = 0;
    char full_path[PATH_MAX] = "";
    for (uint32_t i = start; i <= end; i++) {
        if (*ctx->terminate) {
            return NULL;
        }
        if (max_results && num_results == max_results) {
            break;
        }
        BTreeNode *node = darray_get_item(entries, i);
        if (!node) {
            continue;
        }
        const char *haystack_path = NULL;
        const char *haystack_name = node->name;
        if (search_in_path || query->filter->search_in_path) {
            btree_node_get_path_full(node, full_path, sizeof(full_path));
            haystack_path = full_path;
        }

        if (!filter_node(
                node, query, query->filter->search_in_path ? haystack_path : haystack_name)) {
            continue;
        }

        uint32_t num_found = 0;
        while (true) {
            if (num_found == num_token) {
                results[num_results] = node;
                num_results++;
                break;
            }
            FsearchToken *t = token[num_found++];
            if (!t) {
                break;
            }

            const char *haystack = NULL;
            if (search_in_path || (auto_search_in_path && t->has_separator)) {
                if (!haystack_path) {
                    btree_node_get_path_full(node, full_path, sizeof(full_path));
                    haystack_path = full_path;
                }
                haystack = haystack_path;
            }
            else {
                haystack = haystack_name;
            }
            if (!t->search_func(haystack, t->text, t)) {
                break;
            }
        }
    }
    ctx->num_results = num_results;
    return NULL;
}

static DatabaseSearchResult *
db_search_result_new(GPtrArray *results, uint32_t num_folders, uint32_t num_files) {
    DatabaseSearchResult *result_ctx = calloc(1, sizeof(DatabaseSearchResult));
    assert(result_ctx != NULL);
    result_ctx->results = results;
    result_ctx->num_folders = num_folders;
    result_ctx->num_files = num_files;
    return result_ctx;
}

static DatabaseSearchResult *
db_search_empty(FsearchQuery *query) {
    assert(query != NULL);
    assert(query->db != NULL);

    const uint32_t num_entries = db_get_num_entries(query->db);
    const uint32_t num_results = MIN(query->max_results, num_entries);
    GPtrArray *results = g_ptr_array_sized_new(num_results);
    g_ptr_array_set_free_func(results, (GDestroyNotify)db_search_entry_free);

    DynamicArray *entries = db_get_entries(query->db);

    uint32_t num_folders = 0;
    uint32_t num_files = 0;
    uint32_t pos = 0;

    char full_path[PATH_MAX] = "";
    for (uint32_t i = 0; pos < num_results && i < num_entries; ++i) {
        BTreeNode *node = darray_get_item(entries, i);
        if (!node) {
            continue;
        }

        const char *haystack_path = NULL;
        const char *haystack_name = node->name;
        if (query->filter->search_in_path) {
            btree_node_get_path_full(node, full_path, sizeof(full_path));
            haystack_path = full_path;
        }
        if (!filter_node(
                node, query, query->filter->search_in_path ? haystack_path : haystack_name)) {
            continue;
        }
        if (node->is_dir) {
            num_folders++;
        }
        else {
            num_files++;
        }
        DatabaseSearchEntry *entry = db_search_entry_new(node, pos);
        g_ptr_array_add(results, entry);
        pos++;
    }
    return db_search_result_new(results, num_folders, num_files);
}

static DatabaseSearchResult *
db_search(DatabaseSearch *search, FsearchQuery *q) {
    assert(search != NULL);

    const uint32_t num_entries = db_get_num_entries(q->db);
    if (num_entries == 0) {
        return db_search_result_new(NULL, 0, 0);
    }
    const uint32_t num_threads =
        MIN(fsearch_thread_pool_get_num_threads(search->pool), num_entries);
    const uint32_t num_items_per_thread = num_entries / num_threads;

    search_thread_context_t *thread_data[num_threads];
    memset(thread_data, 0, num_threads * sizeof(search_thread_context_t *));

    const uint32_t max_results = q->max_results;
    const bool limit_results = max_results ? true : false;
    uint32_t start_pos = 0;
    uint32_t end_pos = num_items_per_thread - 1;

    if (!q->token) {
        return db_search_result_new(NULL, 0, 0);
    }

    GTimer *timer = fsearch_timer_start();
    GList *threads = fsearch_thread_pool_get_threads(search->pool);
    for (uint32_t i = 0; i < num_threads; i++) {
        thread_data[i] =
            search_thread_context_new(q,
                                      &search->search_terminate,
                                      start_pos,
                                      i == num_threads - 1 ? num_entries - 1 : end_pos);

        start_pos = end_pos + 1;
        end_pos += num_items_per_thread;

        fsearch_thread_pool_push_data(search->pool, threads, db_search_worker, thread_data[i]);
        threads = threads->next;
    }

    threads = fsearch_thread_pool_get_threads(search->pool);
    while (threads) {
        fsearch_thread_pool_wait_for_thread(search->pool, threads);
        threads = threads->next;
    }
    if (search->search_terminate) {
        for (uint32_t i = 0; i < num_threads; i++) {
            search_thread_context_t *ctx = thread_data[i];
            search_thread_context_free(ctx);
        }
        fsearch_timer_stop(timer, "[search] search aborted after %.2f ms\n");
        timer = NULL;
        return NULL;
    }

    // get total number of entries found
    uint32_t num_results = 0;
    for (uint32_t i = 0; i < num_threads; ++i) {
        num_results += thread_data[i]->num_results;
    }

    GPtrArray *results = g_ptr_array_sized_new(MIN(num_results, max_results));
    g_ptr_array_set_free_func(results, (GDestroyNotify)db_search_entry_free);

    uint32_t num_folders = 0;
    uint32_t num_files = 0;

    uint32_t pos = 0;
    for (uint32_t i = 0; i < num_threads; i++) {
        search_thread_context_t *ctx = thread_data[i];
        if (!ctx) {
            break;
        }
        for (uint32_t j = 0; j < ctx->num_results; ++j) {
            if (limit_results) {
                if (pos >= max_results) {
                    break;
                }
            }
            BTreeNode *node = ctx->results[j];
            if (node->is_dir) {
                num_folders++;
            }
            else {
                num_files++;
            }
            DatabaseSearchEntry *entry = db_search_entry_new(node, pos);
            g_ptr_array_add(results, entry);
            pos++;
        }
        search_thread_context_free(ctx);
    }

    fsearch_timer_stop(timer, "[search] search finished in %.2f ms\n");
    timer = NULL;

    return db_search_result_new(results, num_folders, num_files);
}

void
db_search_results_clear(DatabaseSearch *search) {
    assert(search != NULL);

    // free entries
    if (search->results) {
        g_ptr_array_free(search->results, TRUE);
        search->results = NULL;
    }
    search->num_folders = 0;
    search->num_files = 0;
    return;
}

void
db_search_free(DatabaseSearch *search) {
    assert(search != NULL);

    db_search_results_clear(search);
    g_mutex_lock(&search->query_mutex);
    if (search->query_ctx) {
        fsearch_query_free(search->query_ctx);
        search->query_ctx = NULL;
    }
    g_mutex_unlock(&search->query_mutex);

    search->search_thread_terminate = true;
    g_cond_signal(&search->search_thread_start_cond);
    g_thread_join(search->search_thread);
    g_mutex_clear(&search->query_mutex);
    g_cond_clear(&search->search_thread_start_cond);
    g_free(search);
    search = NULL;
    return;
}

BTreeNode *
db_search_entry_get_node(DatabaseSearchEntry *entry) {
    return entry->node;
}

uint32_t
db_search_entry_get_pos(DatabaseSearchEntry *entry) {
    return entry->pos;
}

void
db_search_entry_set_pos(DatabaseSearchEntry *entry, uint32_t pos) {
    entry->pos = pos;
}

static void
db_search_entry_free(DatabaseSearchEntry *entry) {
    if (entry) {
        g_free(entry);
        entry = NULL;
    }
}

DatabaseSearchEntry *
db_search_entry_new(BTreeNode *node, uint32_t pos) {
    DatabaseSearchEntry *entry = calloc(1, sizeof(DatabaseSearchEntry));
    assert(entry != NULL);

    entry->node = node;
    entry->pos = pos;
    return entry;
}

DatabaseSearch *
db_search_new(FsearchThreadPool *pool) {
    DatabaseSearch *db_search = calloc(1, sizeof(DatabaseSearch));
    assert(db_search != NULL);

    db_search->pool = pool;
    g_mutex_init(&db_search->query_mutex);
    g_cond_init(&db_search->search_thread_start_cond);
    db_search->search_thread = g_thread_new("fsearch_search_thread", db_search_thread, db_search);
    return db_search;
}

uint32_t
db_search_get_num_results(DatabaseSearch *search) {
    assert(search != NULL);
    return search->results->len;
}

uint32_t
db_search_get_num_files(DatabaseSearch *search) {
    assert(search != NULL);
    return search->num_files;
}

uint32_t
db_search_get_num_folders(DatabaseSearch *search) {
    assert(search != NULL);
    return search->num_folders;
}

static void
update_index(DatabaseSearch *search) {
    assert(search != NULL);

    for (uint32_t i = 0; i < search->results->len; ++i) {
        DatabaseSearchEntry *entry = g_ptr_array_index(search->results, i);
        entry->pos = i;
    }
}

void
db_search_remove_entry(DatabaseSearch *search, DatabaseSearchEntry *entry) {
    if (search == NULL) {
        return;
    }
    if (entry == NULL) {
        return;
    }

    g_ptr_array_remove(search->results, (void *)entry);
    update_index(search);
}

GPtrArray *
db_search_get_results(DatabaseSearch *search) {
    assert(search != NULL);
    return search->results;
}

void
db_search_queue(DatabaseSearch *search, FsearchQuery *query) {
    g_mutex_lock(&search->query_mutex);
    if (search->query_ctx) {
        db_search_notify_cancelled(search->query_ctx);
        fsearch_query_free(search->query_ctx);
        search->query_ctx = NULL;
    }
    search->query_ctx = query;
    search->search_terminate = true;
    g_mutex_unlock(&search->query_mutex);
    g_cond_signal(&search->search_thread_start_cond);
}

