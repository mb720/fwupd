/*
 * Copyright (C) 2022 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */
#pragma once

#include <fwupdplugin.h>

GHashTable *
fu_engine_integrity_new(void);
void
fu_engine_integrity_add_checksum(GHashTable *self, const gchar *id, const gchar *csum);
gboolean
fu_engine_integrity_measure(GHashTable *self, GError **error);
gchar *
fu_engine_integrity_to_string(GHashTable *self);
gboolean
fu_engine_integrity_from_string(GHashTable *self, const gchar *str, GError **error);
gboolean
fu_engine_integrity_compare(GHashTable *self, GHashTable *other, GError **error);
