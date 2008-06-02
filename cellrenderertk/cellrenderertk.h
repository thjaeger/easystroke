/* gtkcellrendereraccel.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_CELL_RENDERER_TK_H__
#define __GTK_CELL_RENDERER_TK_H__

#include <gtk/gtkcellrenderertext.h>

G_BEGIN_DECLS

#define GTK_TYPE_CELL_RENDERER_TK		(gtk_cell_renderer_tk_get_type ())
#define GTK_CELL_RENDERER_TK(obj)		(GTK_CHECK_CAST ((obj), GTK_TYPE_CELL_RENDERER_TK, GtkCellRendererTK))
#define GTK_CELL_RENDERER_TK_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_CELL_RENDERER_TK, GtkCellRendererTKClass))
#define GTK_IS_CELL_RENDERER_TK(obj)		(GTK_CHECK_TYPE ((obj), GTK_TYPE_CELL_RENDERER_TK))
#define GTK_IS_CELL_RENDERER_TK_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CELL_RENDERER_TK))
#define GTK_CELL_RENDERER_TK_GET_CLASS(obj)   (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_CELL_RENDERER_TK, GtkCellRendererTKClass))

typedef struct _GtkCellRendererTK      GtkCellRendererTK;
typedef struct _GtkCellRendererTKClass GtkCellRendererTKClass;

GType gtk_cell_renderer_tk_cell_mode_get_type (void) G_GNUC_CONST;
#define GTK_TYPE_CELL_RENDERER_TK_CELL_MODE (gtk_cell_renderer_tk_cell_mode_get_type())

typedef enum
{
  GTK_CELL_RENDERER_TK_CELL_MODE_TEXT,
  GTK_CELL_RENDERER_TK_CELL_MODE_KEY
} GtkCellRendererTKMode;


struct _GtkCellRendererTK
{
  GtkCellRendererText parent;

  /*< private >*/
  guint accel_key;
  GdkModifierType accel_mods;
  GtkCellRendererTKMode cell_mode;
  guint keycode;

  GtkWidget *edit_widget;
  GtkWidget *grab_widget;
  GtkWidget *sizing_label;
};

struct _GtkCellRendererTKClass
{
  GtkCellRendererTextClass parent_class;

  void (* accel_edited)  (GtkCellRendererTK *accel,
		 	  const gchar          *path_string,
			  guint                 accel_key,
			  GdkModifierType       accel_mods,
			  guint                 hardware_keycode);

  void (* accel_cleared) (GtkCellRendererTK *accel,
			  const gchar          *path_string);

  void (* base_get_size) (GtkCellRenderer *cell,
		  GtkWidget            *widget,
		  GdkRectangle         *cell_area,
		  gint                 *x_offset,
		  gint                 *y_offset,
		  gint                 *width,
		  gint                 *height);

  GtkCellEditable *(* base_start_editing) (GtkCellRenderer      *cell,
		  GdkEvent             *event,
		  GtkWidget            *widget,
		  const gchar          *path,
		  GdkRectangle         *background_area,
		  GdkRectangle         *cell_area,
		  GtkCellRendererState  flags);

  /* Padding for future expansion */
  void (*_gtk_reserved0) (void);
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
};

GType            gtk_cell_renderer_tk_get_type        (void) G_GNUC_CONST;
GtkCellRenderer *gtk_cell_renderer_tk_new             (void);


G_END_DECLS


#endif /* __GTK_CELL_RENDERER_TK_H__ */
