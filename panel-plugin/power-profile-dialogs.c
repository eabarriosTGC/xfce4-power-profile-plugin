/*
 * xfce4-power-profile-plugin - Xfce panel plugin for power-profiles-daemon
 * Copyright (C) 2026  LinuxeroGuajiro
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Properties dialog — allows setting default profiles for AC and battery.
 * Uses xfconf for persistence. NOTE: these are informational defaults only;
 * the plugin does NOT auto-switch (that's handled by xfce4-power-manager).
 */

#include "power-profile-dialogs.h"

#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#define PPD_XFCONF_CHANNEL "xfce4-power-profile-plugin"
#define PROP_ON_AC      "/profile-on-ac"
#define PROP_ON_BATTERY "/profile-on-battery"
#define PROP_AUTO_SWITCH "/auto-switch-enabled"

static void
xfpm_power_profile_dialogs_fill_combo (GtkComboBox *combo)
{
    GtkCellRenderer *renderer;
    GtkListStore *store;

    store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
    gtk_combo_box_set_model (combo, GTK_TREE_MODEL (store));
    g_object_unref (store);

    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
                                    "text", 1, NULL);

    /* Add the three standard profiles (display, value) */
    gtk_list_store_insert_with_values (store, NULL, 0,
                                       0, "performance", 1, "performance", -1);
    gtk_list_store_insert_with_values (store, NULL, 1,
                                       0, "balanced",    1, "balanced",    -1);
    gtk_list_store_insert_with_values (store, NULL, 2,
                                       0, "power-saver", 1, "power-saver", -1);
}

static gint
xfpm_power_profile_dialogs_find_index (GtkComboBox *combo, const gchar *value)
{
    GtkTreeModel *model = gtk_combo_box_get_model (combo);
    GtkTreeIter iter;
    gchar *val;
    gint idx = 0;

    if (!gtk_tree_model_get_iter_first (model, &iter))
        return 1; /* default to balanced */

    do {
        gtk_tree_model_get (model, &iter, 0, &val, -1);
        if (g_strcmp0 (val, value) == 0)
        {
            g_free (val);
            return idx;
        }
        g_free (val);
        idx++;
    } while (gtk_tree_model_iter_next (model, &iter));

    return 1; /* balanced as fallback */
}

void
xfpm_power_profile_dialogs_show (GtkWindow *parent)
{
    GtkWidget *dialog, *content, *grid;
    GtkWidget *lbl_ac, *combo_ac, *lbl_bat, *combo_bat;
    XfconfChannel *channel;
    gchar *ac_val, *bat_val;
    gboolean auto_switch;

    if (!xfconf_init (NULL)) { /* already initialized */ }

    channel = xfconf_channel_get (PPD_XFCONF_CHANNEL);

    /* Read current values */
    ac_val = xfconf_channel_get_string (channel, PROP_ON_AC, "balanced");
    bat_val = xfconf_channel_get_string (channel, PROP_ON_BATTERY, "balanced");
    auto_switch = xfconf_channel_get_bool (channel, PROP_AUTO_SWITCH, FALSE);

    /* Build dialog */
    dialog = gtk_dialog_new_with_buttons (
        _("Power Profile Plugin Settings"),
        parent,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        _("_Close"), GTK_RESPONSE_CLOSE,
        NULL
    );

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);
    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

    content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    grid = gtk_grid_new ();
    gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
    gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
    gtk_container_set_border_width (GTK_CONTAINER (grid), 12);
    gtk_box_pack_start (GTK_BOX (content), grid, TRUE, TRUE, 0);

    /* Widgets */
    lbl_ac = gtk_label_new_with_mnemonic (_("Profile when on _AC power:"));
    gtk_widget_set_halign (lbl_ac, GTK_ALIGN_START);
    combo_ac = gtk_combo_box_new ();
    xfpm_power_profile_dialogs_fill_combo (GTK_COMBO_BOX (combo_ac));
    gtk_combo_box_set_active (GTK_COMBO_BOX (combo_ac),
                               xfpm_power_profile_dialogs_find_index (GTK_COMBO_BOX (combo_ac), ac_val));
    gtk_label_set_mnemonic_widget (GTK_LABEL (lbl_ac), combo_ac);

    lbl_bat = gtk_label_new_with_mnemonic (_("Profile when on _battery power:"));
    gtk_widget_set_halign (lbl_bat, GTK_ALIGN_START);
    combo_bat = gtk_combo_box_new ();
    xfpm_power_profile_dialogs_fill_combo (GTK_COMBO_BOX (combo_bat));
    gtk_combo_box_set_active (GTK_COMBO_BOX (combo_bat),
                               xfpm_power_profile_dialogs_find_index (GTK_COMBO_BOX (combo_bat), bat_val));

    gtk_grid_attach (GTK_GRID (grid), lbl_ac,    0, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), combo_ac,  1, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), lbl_bat,   0, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (grid), combo_bat, 1, 1, 1, 1);

    /* Auto-switch checkbox */
    GtkWidget *check_auto = gtk_check_button_new_with_mnemonic (
        _("_Enable automatic profile switching"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_auto), auto_switch);
    gtk_grid_attach (GTK_GRID (grid), check_auto, 0, 2, 2, 1);

    /* Info note about auto-switch */
    GtkWidget *note = gtk_label_new (_("When enabled, the plugin applies the profile\n"
                                       "above automatically on AC/battery change."));
    gtk_label_set_justify (GTK_LABEL (note), GTK_JUSTIFY_CENTER);
    gtk_widget_set_opacity (note, 0.7);
    gtk_grid_attach (GTK_GRID (grid), note, 0, 3, 2, 1);

    gtk_widget_show_all (dialog);

    /* Run — persist on close */
    gint response = gtk_dialog_run (GTK_DIALOG (dialog));
    if (response == GTK_RESPONSE_CLOSE || response == GTK_RESPONSE_DELETE_EVENT)
    {
        GtkTreeIter iter;

        /* Save AC */
        if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo_ac), &iter))
        {
            gchar *val;
            gtk_tree_model_get (gtk_combo_box_get_model (GTK_COMBO_BOX (combo_ac)),
                                &iter, 1, &val, -1);
            xfconf_channel_set_string (channel, PROP_ON_AC, val);
            g_free (val);
        }

        /* Save battery */
        if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo_bat), &iter))
        {
            gchar *val;
            gtk_tree_model_get (gtk_combo_box_get_model (GTK_COMBO_BOX (combo_bat)),
                                &iter, 1, &val, -1);
            xfconf_channel_set_string (channel, PROP_ON_BATTERY, val);
            g_free (val);
        }

        xfconf_channel_set_bool (channel, PROP_AUTO_SWITCH,
            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_auto)));
    }

    gtk_widget_destroy (dialog);
    g_free (ac_val);
    g_free (bat_val);
}
