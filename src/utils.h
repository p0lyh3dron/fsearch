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

#include "btree.h"
#include <glib.h>
#include <stdbool.h>

gboolean
build_path(gchar *dest, size_t dest_len, const gchar *path, const gchar *name);

bool
node_move_to_trash(BTreeNode *node);

bool
node_delete(BTreeNode *node);

bool
launch_node(BTreeNode *node);

bool
launch_node_path(BTreeNode *node, const char *cmd);
