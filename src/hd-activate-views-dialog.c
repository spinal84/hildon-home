/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib/gi18n.h>

#include <hildon/hildon-file-chooser-dialog.h>
#include <hildon/hildon-file-selection.h>

#include <gconf/gconf-client.h>

#include "hd-activate-views-dialog.h"

/* Background gconf key */
#define GCONF_BACKGROUND_KEY(i) g_strdup_printf ("/apps/osso/hildon-desktop/views/%u/bg-image", i)

#define CURRENT_THEME_DIR "/etc/hildon/theme"
#define BACKGROUNDS_DESKTOP_FILE CURRENT_THEME_DIR "/backgrounds/theme_bg.desktop"
#define DEFAULT_THEME_DIR "/usr/share/themes/default"
#define BACKGROUNDS_DEFAULT_DESKTOP_FILE DEFAULT_THEME_DIR "/backgrounds/theme_bg.desktop"
#define BACKGROUNDS_DESKTOP_KEY_FILE "X-File%u"

#define HD_MAX_HOME_VIEWS 4
#define HD_GCONF_KEY_ACTIVE_VIEWS "/apps/osso/hildon-desktop/views/active"

/* Images folder */
#define USER_IMAGES_FOLDER "MyDocs", ".images"

enum
{
  COL_PIXBUF,
  NUM_COLS
};

#define HD_ACTIVATE_VIEWS_DIALOG_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_ACTIVATE_VIEWS_DIALOG, HDActivateViewsDialogPrivate))

struct _HDActivateViewsDialogPrivate
{
  GtkTreeModel *model;

  GtkWidget    *icon_view;

  GConfClient  *gconf_client;
};

G_DEFINE_TYPE (HDActivateViewsDialog, hd_activate_views_dialog, GTK_TYPE_DIALOG);

static void
hd_activate_views_dialog_dispose (GObject *object)
{
  HDActivateViewsDialogPrivate *priv = HD_ACTIVATE_VIEWS_DIALOG (object)->priv;

  if (priv->model)
    priv->model = (g_object_unref (priv->model), NULL);

  if (priv->gconf_client)
    priv->gconf_client = (g_object_unref (priv->gconf_client), NULL);

  G_OBJECT_CLASS (hd_activate_views_dialog_parent_class)->dispose (object);
}

static void
hd_activate_views_dialog_response (GtkDialog *dialog,
                                   gint       response_id)
{
  HDActivateViewsDialogPrivate *priv = HD_ACTIVATE_VIEWS_DIALOG (dialog)->priv;

  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      GtkTreeIter iter;

      if (gtk_tree_model_get_iter_first (priv->model, &iter))
        {
          GSList *list = NULL;
          gint i = 1;
          GError *error = NULL;

          do
            {
              GtkTreePath *path;

              path = gtk_tree_model_get_path (priv->model, &iter);

              if (gtk_icon_view_path_is_selected (GTK_ICON_VIEW (priv->icon_view),
                                                  path))
                list = g_slist_append (list, GINT_TO_POINTER (i));

              gtk_tree_path_free (path);

              i++;
            }
          while (gtk_tree_model_iter_next (priv->model, &iter));

          gconf_client_set_list (priv->gconf_client,
                                 HD_GCONF_KEY_ACTIVE_VIEWS,
                                 GCONF_VALUE_INT,
                                 list,
                                 &error);
          if (error)
            {
              g_warning ("Could not activate/deactivate view via GConf. %s", error->message);
              g_error_free (error);
            }

          g_slist_free (list);
        }
    }
}

static void
hd_activate_views_dialog_class_init (HDActivateViewsDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

  object_class->dispose = hd_activate_views_dialog_dispose;

  dialog_class->response = hd_activate_views_dialog_response;

  g_type_class_add_private (klass, sizeof (HDActivateViewsDialogPrivate));
}

static void
hd_activate_views_dialog_init (HDActivateViewsDialog *dialog)
{
  HDActivateViewsDialogPrivate *priv = HD_ACTIVATE_VIEWS_DIALOG_GET_PRIVATE (dialog);
  GtkCellRenderer *renderer;
  guint i;
  GtkWidget *pannable;
  GSList *list = NULL;
  gboolean active_views[HD_MAX_HOME_VIEWS] = { 0,};
  gboolean none_active = TRUE;
  GKeyFile *current_theme_backgrounds = NULL;
  GKeyFile *default_theme_backgrounds = NULL;
  GError *error = NULL;

  dialog->priv = priv;

  /* Set dialog title */
  gtk_window_set_title (GTK_WINDOW (dialog), dgettext (GETTEXT_PACKAGE, "home_ti_manage_views"));

  /* Add buttons */
  gtk_dialog_add_button (GTK_DIALOG (dialog), dgettext ("hildon-libs", "wdgt_bd_done"), GTK_RESPONSE_ACCEPT);

  /* Create Touch grid list */
  priv->model = (GtkTreeModel *) gtk_list_store_new (NUM_COLS, GDK_TYPE_PIXBUF);

  priv->icon_view = hildon_gtk_icon_view_new_with_model (HILDON_UI_MODE_EDIT,
                                                         priv->model);
  gtk_icon_view_set_selection_mode (GTK_ICON_VIEW (priv->icon_view),
                                    GTK_SELECTION_MULTIPLE);
  gtk_icon_view_set_columns (GTK_ICON_VIEW (priv->icon_view),
                             HD_MAX_HOME_VIEWS);

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->icon_view),
                              renderer,
                              TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (priv->icon_view),
                                  renderer,
                                  "pixbuf", COL_PIXBUF,
                                  NULL);

  /* Read active views from GConf */
  priv->gconf_client = gconf_client_get_default ();
  list = gconf_client_get_list (priv->gconf_client,
                                HD_GCONF_KEY_ACTIVE_VIEWS,
                                GCONF_VALUE_INT,
                                &error);

  if (!error)
    {

      GSList *l;

      for (l = list; l; l = l->next)
        {
          gint id = GPOINTER_TO_INT (l->data);

          /* Stored in GConf 1..HD_MAX_HOME_VIEWS */

          if (id > 0 && id <= HD_MAX_HOME_VIEWS)
            {
              active_views[id - 1] = TRUE;
              none_active = FALSE;
            }
        }
    }
  else
    {
      /* Error */
      g_warning ("Error reading active views from GConf. %s", error->message);
      error = (g_error_free (error), NULL);
    }

  g_slist_free (list);

  /* Check if there is an view active */
  if (none_active)
    {
      g_warning ("No active views. Make first view active");
      active_views[0] = TRUE;
    }

  /* Append views */
  for (i = 1; i <= HD_MAX_HOME_VIEWS; i++)
    {
      gchar *key;
      gchar *bg_image;
      GdkPixbuf *pixbuf = NULL;
      GtkTreeIter iter;
      GtkTreePath *path = NULL;

      /* Try to get the background image from GConf */
      key = GCONF_BACKGROUND_KEY (i);
      bg_image = gconf_client_get_string (priv->gconf_client,
                                          key,
                                          &error);
      g_free (key);

      if (error)
        {
          g_warning ("Could not get background image for view %u, '%s'. %s",
                     i, key, error->message);
          g_error_free (error);
          error = NULL;
        }

      if (!bg_image)
        {
          gchar *desktop_key = g_strdup_printf (BACKGROUNDS_DESKTOP_KEY_FILE, i);

          /* Try to get background from current theme */
          if (!current_theme_backgrounds)
            {
              current_theme_backgrounds = g_key_file_new ();
              g_key_file_load_from_file (current_theme_backgrounds,
                                         BACKGROUNDS_DESKTOP_FILE,
                                         G_KEY_FILE_NONE,
                                         NULL);
            }

          bg_image = g_key_file_get_string (current_theme_backgrounds,
                                            G_KEY_FILE_DESKTOP_GROUP,
                                            desktop_key,
                                            NULL);

          /* Try to get background from default theme */
          if (!bg_image)
            {
              if (!default_theme_backgrounds)
                {
                  default_theme_backgrounds = g_key_file_new ();
                  g_key_file_load_from_file (default_theme_backgrounds,
                                             BACKGROUNDS_DEFAULT_DESKTOP_FILE,
                                             G_KEY_FILE_NONE,
                                             NULL);
                }

              bg_image = g_key_file_get_string (default_theme_backgrounds,
                                                G_KEY_FILE_DESKTOP_GROUP,
                                                desktop_key,
                                                NULL);
            }

          g_free (desktop_key);
        }

      if (bg_image)
        pixbuf = gdk_pixbuf_new_from_file_at_scale (bg_image, 125, 75, TRUE, &error);

      if (error)
        {
          g_warning ("Could not get background image for view %u, '%s'. %s",
                     i, bg_image, error->message);
          g_error_free (error);
          error = NULL;
        }

      if (!pixbuf)
        {
          pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                                   TRUE,
                                   8,
                                   125, 75);
          gdk_pixbuf_fill (pixbuf,
                           0x000000ff);
        }

      gtk_list_store_insert_with_values (GTK_LIST_STORE (priv->model),
                                         &iter,
                                         -1,
                                         COL_PIXBUF, pixbuf,
                                         -1);

      path = gtk_tree_model_get_path (priv->model, &iter);

      if (active_views[i - 1])
        {
          gtk_icon_view_select_path (GTK_ICON_VIEW (priv->icon_view),
                                     path);
        }
      else
        {
          gtk_icon_view_unselect_path (GTK_ICON_VIEW (priv->icon_view),
                                       path);
        }

      if (pixbuf)
        g_object_unref (pixbuf);
      if (path)
        gtk_tree_path_free (path);
    }

  gtk_widget_show (priv->icon_view);

  pannable = hildon_pannable_area_new ();
  g_object_set (pannable,
                "mov_mode", 0,
                "vscrollbar_policy", GTK_POLICY_NEVER,
                "hscrollbar_policy", GTK_POLICY_NEVER,
/*                "size-request-policy", HILDON_SIZE_REQUEST_CHILDREN, */
                NULL);
  gtk_widget_show (pannable);
  hildon_pannable_area_add_with_viewport (HILDON_PANNABLE_AREA (pannable),
                                          priv->icon_view);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox),
/*                     priv->icon_view); */
                    pannable);

  if (current_theme_backgrounds)
    g_key_file_free (current_theme_backgrounds);
  if (default_theme_backgrounds)
    g_key_file_free (default_theme_backgrounds);
}

GtkWidget *
hd_activate_views_dialog_new (void)
{
  GtkWidget *window;

  window = g_object_new (HD_TYPE_ACTIVATE_VIEWS_DIALOG,
                         NULL);

  return window;
}
