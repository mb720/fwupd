/*
 * Copyright (C) 2022 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN "FuEngine"

#include "config.h"

#include "fu-engine-integrity.h"

/* exported for the self tests */
void
fu_engine_integrity_add_checksum(GHashTable *self, const gchar *id, const gchar *csum)
{
	g_hash_table_insert(self, g_strdup(id), g_strdup(csum));
}

static void
fu_engine_integrity_add_measurement(GHashTable *self, const gchar *id, GBytes *blob)
{
	g_autofree gchar *csum = g_compute_checksum_for_bytes(G_CHECKSUM_SHA256, blob);
	fu_engine_integrity_add_checksum(self, id, csum);
}

static void
fu_engine_integrity_measure_acpi(GHashTable *self)
{
	g_autofree gchar *path = fu_path_from_kind(FU_PATH_KIND_ACPI_TABLES);
	const gchar *tables[] = {"SLIC", NULL};

	for (guint i = 0; tables[i] != NULL; i++) {
		g_autofree gchar *fn = g_build_filename(path, tables[i], NULL);
		g_autoptr(GBytes) blob = NULL;

		blob = fu_bytes_get_contents(fn, NULL);
		if (blob != NULL && g_bytes_get_size(blob) > 0) {
			g_autofree gchar *id = g_strdup_printf("ACPI:%s", tables[i]);
			fu_engine_integrity_add_measurement(self, id, blob);
		}
	}
}

static void
fu_engine_integrity_measure_uefi(GHashTable *self)
{
	struct {
		const gchar *guid;
		const gchar *name;
	} keys[] = {{FU_EFIVAR_GUID_EFI_GLOBAL, "BootOrder"},
		    {FU_EFIVAR_GUID_EFI_GLOBAL, "BootCurrent"},
		    {FU_EFIVAR_GUID_EFI_GLOBAL, "KEK"},
		    {FU_EFIVAR_GUID_EFI_GLOBAL, "PK"},
		    {FU_EFIVAR_GUID_SECURITY_DATABASE, "db"},
		    {FU_EFIVAR_GUID_SECURITY_DATABASE, "dbx"},
		    {NULL, NULL}};

	/* important keys */
	for (guint i = 0; keys[i].guid != NULL; i++) {
		g_autoptr(GBytes) blob =
		    fu_efivar_get_data_bytes(keys[i].guid, keys[i].name, NULL, NULL);
		if (blob != NULL) {
			g_autofree gchar *id = g_strdup_printf("UEFI:%s", keys[i].name);
			fu_engine_integrity_add_measurement(self, id, blob);
		}
	}

	/* Boot#### */
	for (guint i = 0; i < 0xFF; i++) {
		g_autofree gchar *name = g_strdup_printf("Boot%04X", i);
		g_autoptr(GBytes) blob =
		    fu_efivar_get_data_bytes(FU_EFIVAR_GUID_EFI_GLOBAL, name, NULL, NULL);
		if (blob != NULL && g_bytes_get_size(blob) > 0) {
			g_autofree gchar *id = g_strdup_printf("UEFI:%s", name);
			fu_engine_integrity_add_measurement(self, id, blob);
		}
	}
}

GHashTable *
fu_engine_integrity_new(void)
{
	return g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
}

gboolean
fu_engine_integrity_measure(GHashTable *self, GError **error)
{
	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	fu_engine_integrity_measure_uefi(self);
	fu_engine_integrity_measure_acpi(self);

	/* nothing of use */
	if (g_hash_table_size(self) == 0) {
		g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "no measurements");
		return FALSE;
	}

	/* success */
	return TRUE;
}

gchar *
fu_engine_integrity_to_string(GHashTable *self)
{
	GHashTableIter iter;
	gpointer key, value;
	g_autoptr(GPtrArray) array = g_ptr_array_new_with_free_func(g_free);

	g_return_val_if_fail(self != NULL, NULL);

	/* sanity check */
	if (g_hash_table_size(self) == 0)
		return NULL;

	/* build into KV array */
	g_hash_table_iter_init(&iter, self);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		g_ptr_array_add(array,
				g_strdup_printf("%s=%s", (const gchar *)key, (const gchar *)value));
	}
	return fu_strjoin("\n", array);
}

gboolean
fu_engine_integrity_from_string(GHashTable *self, const gchar *str, GError **error)
{
	g_auto(GStrv) lines = NULL;

	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(str != NULL, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	lines = g_strsplit(str, "\n", -1);
	for (guint i = 0; lines[i] != NULL; i++) {
		g_auto(GStrv) tokens = NULL;
		if (lines[i][0] == '\0' || lines[i][0] == '#')
			continue;
		tokens = g_strsplit(lines[i], "=", 2);
		if (g_strv_length(tokens) != 2) {
			g_set_error(error,
				    G_IO_ERROR,
				    G_IO_ERROR_INVALID_DATA,
				    "failed to parse: %s",
				    str);
			return FALSE;
		}
		fu_engine_integrity_add_checksum(self, tokens[0], tokens[1]);
	}

	/* success */
	return TRUE;
}

/* self is what we have now, other is what we had at another time */
gboolean
fu_engine_integrity_compare(GHashTable *self, GHashTable *other, GError **error)
{
	GHashTableIter iter;
	gpointer key, value;
	g_autoptr(GPtrArray) array = g_ptr_array_new_with_free_func(g_free);

	g_return_val_if_fail(self != NULL, FALSE);
	g_return_val_if_fail(other != NULL, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	/* look at what we have now */
	g_hash_table_iter_init(&iter, self);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		const gchar *value2 = g_hash_table_lookup(other, key);
		if (value2 == NULL) {
			g_ptr_array_add(array,
					g_strdup_printf("%s=MISSING->%s",
							(const gchar *)key,
							(const gchar *)value));
			continue;
		}
		if (g_strcmp0((const gchar *)value, (const gchar *)value2) != 0) {
			g_ptr_array_add(array,
					g_strdup_printf("%s=%s->%s",
							(const gchar *)key,
							(const gchar *)value2,
							(const gchar *)value));
		}
	}

	/* look at what we had then */
	g_hash_table_iter_init(&iter, other);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		const gchar *value2 = g_hash_table_lookup(self, key);
		if (value2 == NULL) {
			g_ptr_array_add(array,
					g_strdup_printf("%s=%s->MISSING",
							(const gchar *)key,
							(const gchar *)value));
			continue;
		}
	}

	/* not okay */
	if (array->len > 0) {
		g_autofree gchar *str = fu_strjoin(", ", array);
		g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, str);
		return FALSE;
	}

	/* success */
	return TRUE;
}
