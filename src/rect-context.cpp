#define __SP_RECT_CONTEXT_C__

/*
 * Rectangle drawing context
 *
 * Author:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * Copyright (C) 2000 Lauris Kaplinski
 * Copyright (C) 2000-2001 Ximian, Inc.
 * Copyright (C) 2002 Lauris Kaplinski
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <config.h>
#include <math.h>
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "macros.h"
#include "helper/sp-canvas.h"
#include "sp-rect.h"
#include "inkscape.h"
#include "document.h"
#include "selection.h"
#include "desktop-handles.h"
#include "desktop-affine.h"
#include "desktop-snap.h"
#include "pixmaps/cursor-rect.xpm"
#include "rect-context.h"
#include "sp-metrics.h"
#include "helper/sp-intl.h"

static void sp_rect_context_class_init (SPRectContextClass * klass);
static void sp_rect_context_init (SPRectContext * rect_context);
static void sp_rect_context_dispose (GObject *object);

static void sp_rect_context_setup (SPEventContext *ec);
static void sp_rect_context_set (SPEventContext *ec, const gchar *key, const gchar *val);

static gint sp_rect_context_root_handler (SPEventContext * event_context, GdkEvent * event);
static gint sp_rect_context_item_handler (SPEventContext * event_context, SPItem * item, GdkEvent * event);
static GtkWidget *sp_rect_context_config_widget (SPEventContext *ec);

static void sp_rect_drag(SPRectContext &rc, NR::Point const pt, guint state);
static void sp_rect_finish (SPRectContext * rc);

static SPEventContextClass * parent_class;

GtkType
sp_rect_context_get_type (void)
{
	static GType type = 0;
	if (!type) {
		GTypeInfo info = {
			sizeof (SPRectContextClass),
			NULL, NULL,
			(GClassInitFunc) sp_rect_context_class_init,
			NULL, NULL,
			sizeof (SPRectContext),
			4,
			(GInstanceInitFunc) sp_rect_context_init,
			NULL,	/* value_table */
		};
		type = g_type_register_static (SP_TYPE_EVENT_CONTEXT, "SPRectContext", &info, (GTypeFlags)0);
	}
	return type;
}

static void
sp_rect_context_class_init (SPRectContextClass * klass)
{
	GObjectClass *object_class;
	SPEventContextClass * event_context_class;

	object_class = (GObjectClass *) klass;
	event_context_class = (SPEventContextClass *) klass;

	parent_class = (SPEventContextClass*)g_type_class_peek_parent (klass);

	object_class->dispose = sp_rect_context_dispose;

#if 1
	event_context_class->setup = sp_rect_context_setup;
	event_context_class->set = sp_rect_context_set;
#endif
	event_context_class->root_handler  = sp_rect_context_root_handler;
	event_context_class->item_handler  = sp_rect_context_item_handler;
	event_context_class->config_widget = sp_rect_context_config_widget;
}

static void
sp_rect_context_init (SPRectContext * rect_context)
{
	SPEventContext * event_context;
	
	event_context = SP_EVENT_CONTEXT (rect_context);

	event_context->cursor_shape = cursor_rect_xpm;
	event_context->hot_x = 4;
	event_context->hot_y = 4;

	rect_context->item = NULL;

	rect_context->rx_ratio = 0.0;
	rect_context->ry_ratio = 0.0;
}

static void
sp_rect_context_dispose (GObject *object)
{
	SPRectContext * rc;

	rc = SP_RECT_CONTEXT (object);

	/* fixme: This is necessary because we do not grab */
	if (rc->item) sp_rect_finish (rc);

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
sp_rect_context_setup (SPEventContext *ec)
{
	SPRectContext * rc;

	rc = SP_RECT_CONTEXT (ec);

	if (((SPEventContextClass *) parent_class)->setup)
		((SPEventContextClass *) parent_class)->setup (ec);

	sp_event_context_read (ec, "rx_ratio");
	sp_event_context_read (ec, "ry_ratio");
}

static void
sp_rect_context_set (SPEventContext *ec, const gchar *key, const gchar *val)
{
	SPRectContext *rc;

	rc = SP_RECT_CONTEXT (ec);

	if (!strcmp (key, "rx_ratio")) {
		rc->rx_ratio = (val) ? atof (val) : 0.0;
		rc->rx_ratio = CLAMP (rc->rx_ratio, 0.0, 1.0);
	} else if (!strcmp (key, "ry_ratio")) {
		rc->ry_ratio = (val) ? atof (val) : 0.0;
		rc->ry_ratio = CLAMP (rc->ry_ratio, 0.0, 1.0);
	}
}

static gint
sp_rect_context_item_handler (SPEventContext * event_context, SPItem * item, GdkEvent * event)
{
	gint ret;

	ret = FALSE;

	if (((SPEventContextClass *) parent_class)->item_handler)
		ret = ((SPEventContextClass *) parent_class)->item_handler (event_context, item, event);

	return ret;
}

static gint sp_rect_context_root_handler(SPEventContext *event_context, GdkEvent *event)
{
	static bool dragging;

	SPDesktop *desktop = event_context->desktop;
	SPRectContext *rc = SP_RECT_CONTEXT(event_context);

	gint ret = FALSE;
	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if (event->button.button == 1) {
			dragging = true;
			/* Position center */
			NR::Point const button_w(event->button.x,
						 event->button.y);
			NR::Point const button_dt(sp_desktop_w2d_xy_point(event_context->desktop, button_w));
			/* Snap center to nearest magnetic point */
			rc->center = button_dt;
			sp_desktop_free_snap (event_context->desktop, rc->center);
			sp_canvas_item_grab (SP_CANVAS_ITEM (desktop->acetate),
					     GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | 
					     GDK_POINTER_MOTION_HINT_MASK | GDK_BUTTON_PRESS_MASK,
					     NULL, event->button.time);
			ret = TRUE;
		}
		break;
	case GDK_MOTION_NOTIFY:
		if (dragging && (event->motion.state & GDK_BUTTON1_MASK)) {
			NR::Point const motion_w(event->motion.x,
						 event->motion.y);
			NR::Point const motion_dt(sp_desktop_w2d_xy_point(event_context->desktop, motion_w));
			sp_rect_drag(*rc, motion_dt, event->motion.state);
			ret = TRUE;
		}
		break;
	case GDK_BUTTON_RELEASE:
		if (event->button.button == 1) {
			dragging = false;
			sp_rect_finish (rc);
			ret = TRUE;
			sp_canvas_item_ungrab (SP_CANVAS_ITEM (desktop->acetate), event->button.time);
		}
		break;
	case GDK_KEY_PRESS:
		switch (event->key.keyval) {
		case GDK_Up: 
		case GDK_Down: 
		case GDK_KP_Up: 
		case GDK_KP_Down: 
			// prevent the zoom field from activation
			if (!MOD__CTRL_ONLY)
				ret = TRUE;
			break;
		default:
			break;
		}
	default:
		break;
	}

	if (!ret) {
		if (((SPEventContextClass *) parent_class)->root_handler)
			ret = ((SPEventContextClass *) parent_class)->root_handler (event_context, event);
	}

	return ret;
}

static void sp_rect_drag(SPRectContext &rc, NR::Point const pt, guint state)
{
	SPDesktop *desktop = SP_EVENT_CONTEXT(&rc)->desktop;

	if (!rc.item) {
		SPRepr * repr, * style;
		SPCSSAttr * css;
		/* Create object */
		repr = sp_repr_new ("rect");
		/* Set style */
		style = inkscape_get_repr (INKSCAPE, "tools.shapes.rect");
		if (style) {
			css = sp_repr_css_attr_inherited (style, "style");
			sp_repr_css_set (repr, css, "style");
			sp_repr_css_attr_unref (css);
		}
		rc.item = (SPItem *) sp_document_add_repr(SP_DT_DOCUMENT(desktop), repr);
		sp_repr_unref (repr);
	}

	/* This is bit ugly, but so we are */

	NR::Point p0, p1;
	if (state & GDK_CONTROL_MASK) {
		NR::Point delta = pt - rc.center;
		/* fixme: Snapping */
		if ((fabs (delta[0]) > fabs (delta[1])) && (delta[1] != 0.0)) {
			delta[0] = floor (delta[0]/delta[1] + 0.5) * delta[1];
		} else if (delta[0] != 0.0) {
			delta[1] = floor (delta[1]/delta[0] + 0.5) * delta[0];
		}
		p1 = rc.center + delta;
		if (state & GDK_SHIFT_MASK) {
			p0 = rc.center - delta;
			const NR::Coord l0 = sp_desktop_vector_snap (desktop, p0, p0 - p1);
			const NR::Coord l1 = sp_desktop_vector_snap (desktop, p1, p1 - p0);
			
			if (l0 < l1) {
				p1 = 2 * rc.center - p0;
			} else {
				p0 = 2 * rc.center - p1;
			}
		} else {
			p0 = rc.center;
			sp_desktop_vector_snap (desktop, p1, 
									p1 - p0);
		}
	} else if (state & GDK_SHIFT_MASK) {
		/* Corner point movements are bound */
		p1 = pt;
		p0 = 2 * rc.center - p1;
		for (unsigned d = 0 ; d < 2 ; ++d) {
			double snap_movement[2];
			snap_movement[0] = sp_desktop_dim_snap(desktop, p0, d);
			snap_movement[1] = sp_desktop_dim_snap(desktop, p1, d);
			if ( snap_movement[0] <
			     snap_movement[1] ) {
				/* Use point 0 position. */
				p1[d] = 2 * rc.center[d] - p0[d];
			} else {
				p0[d] = 2 * rc.center[d] - p1[d];
			}
		}
	} else {
		/* Free movement for corner point */
		p0 = rc.center;
		p1 = pt;
		sp_desktop_free_snap (desktop, p1);
	}

	p0 = sp_desktop_dt2root_xy_point (desktop, p0);
	p1 = sp_desktop_dt2root_xy_point (desktop, p1);
	
	// FIXME: use NR::Rect
	const NR::Coord x0 = MIN (p0[NR::X], p1[NR::X]);
	const NR::Coord y0 = MIN (p0[NR::Y], p1[NR::Y]);
	const NR::Coord x1 = MAX (p0[NR::X], p1[NR::X]);
	const NR::Coord y1 = MAX (p0[NR::Y], p1[NR::Y]);
	const NR::Coord w  = x1 - x0;
	const NR::Coord h  = y1 - y0;

	sp_rect_position_set(SP_RECT(rc.item), x0, y0, w, h);
	if ( rc.rx_ratio != 0.0 ) {
		sp_rect_set_rx(SP_RECT(rc.item), TRUE, 0.5 * rc.rx_ratio * w);
	}
	if ( rc.ry_ratio != 0.0 ) {
		sp_rect_set_ry(SP_RECT(rc.item), TRUE, 0.5 * rc.ry_ratio * h);
	}

	// status text
	gchar status[80];
	GString *xs = SP_PT_TO_METRIC_STRING(fabs( x1 - x0 ), SP_DEFAULT_METRIC);
	GString *ys = SP_PT_TO_METRIC_STRING(fabs( y1 - y0 ), SP_DEFAULT_METRIC);
	g_snprintf (status, 80, "Draw rectangle  %s x %s", xs->str, ys->str);
	sp_view_set_status (SP_VIEW (desktop), status, FALSE);
	/* FIXME: I'd guess that arg2 should be TRUE below, that otherwise we'd have mem leak.
	 * Check with valgrind. */
	g_string_free (xs, FALSE);
	g_string_free (ys, FALSE);
}

static void
sp_rect_finish (SPRectContext *rc)
{
	if (rc->item != NULL) {
		SPDesktop * dt;

		dt = SP_EVENT_CONTEXT_DESKTOP (rc);

		sp_object_invoke_write (SP_OBJECT (rc->item), SP_OBJECT_REPR (rc->item), SP_OBJECT_WRITE_EXT);

		sp_selection_set_item (SP_DT_SELECTION (dt), rc->item);
		sp_document_done (SP_DT_DOCUMENT (dt));

		rc->item = NULL;
	}
}

static void
sp_rc_rx_ratio_value_changed (GtkAdjustment *adj, SPRectContext *rc)
{
  	sp_repr_set_double (SP_EVENT_CONTEXT_REPR (rc), "rx_ratio", adj->value);
}

static void
sp_rc_ry_ratio_value_changed (GtkAdjustment *adj, SPRectContext *rc)
{
	sp_repr_set_double (SP_EVENT_CONTEXT_REPR (rc), "ry_ratio", adj->value);
}

static void
sp_rc_defaults (GtkWidget *widget, GtkObject *obj)
{
	GtkAdjustment *adj;

	adj = (GtkAdjustment*)gtk_object_get_data (obj, "rx_ratio");
	gtk_adjustment_set_value (adj, 0.0);
	adj = (GtkAdjustment*)gtk_object_get_data (obj, "ry_ratio");
	gtk_adjustment_set_value (adj, 0.0);
}

static GtkWidget *
sp_rect_context_config_widget (SPEventContext *ec)
{
	SPRectContext *rc;
	GtkWidget *tbl, *l, *sb, *b;
	GtkObject *a;

	rc = SP_RECT_CONTEXT (ec);

	tbl = gtk_table_new (3, 2, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (tbl), 4);
	gtk_table_set_row_spacings (GTK_TABLE (tbl), 4);

	/* rx_ratio */
	l = gtk_label_new (_("Roundness ratio for x:"));
	gtk_widget_show (l);
	gtk_misc_set_alignment (GTK_MISC (l), 1.0, 0.5);
	gtk_table_attach (GTK_TABLE (tbl), l, 0, 1, 0, 1, 
			  (GtkAttachOptions)0, 
			  (GtkAttachOptions)0, 
			  0, 0);
	a = gtk_adjustment_new (rc->rx_ratio, 0.0, 1.0, 0.01, 0.1, 0.1);
	gtk_object_set_data (GTK_OBJECT (tbl), "rx_ratio", a);
	sb = gtk_spin_button_new (GTK_ADJUSTMENT (a), 0.1, 2);
	gtk_widget_show (sb);
	gtk_table_attach (GTK_TABLE (tbl), sb, 1, 2, 0, 1, 
			  (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 
			  (GtkAttachOptions)0, 
			  0, 0);
	gtk_signal_connect (GTK_OBJECT (a), "value_changed", GTK_SIGNAL_FUNC (sp_rc_rx_ratio_value_changed), rc);

	/* ry_ratio */
	l = gtk_label_new (_("Roundness ratio for y:"));
	gtk_widget_show (l);
	gtk_misc_set_alignment (GTK_MISC (l), 1.0, 0.5);
	gtk_table_attach (GTK_TABLE (tbl), l, 0, 1, 1, 2, 
			  (GtkAttachOptions)0, 
			  (GtkAttachOptions)0, 
			  0, 0);
	a = gtk_adjustment_new (rc->ry_ratio, 0.0, 1.0, 0.01, 0.1, 0.1);
	gtk_object_set_data (GTK_OBJECT (tbl), "ry_ratio", a);
	sb = gtk_spin_button_new (GTK_ADJUSTMENT (a), 0.1, 2);
	gtk_widget_show (sb);
	gtk_table_attach (GTK_TABLE (tbl), sb, 1, 2, 1, 2, 
			  (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 
			  (GtkAttachOptions)0, 
			  0, 0);
	gtk_signal_connect (GTK_OBJECT (a), "value_changed", GTK_SIGNAL_FUNC (sp_rc_ry_ratio_value_changed), rc);

	/* Reset */
	b = gtk_button_new_with_label (_("Defaults"));
	gtk_widget_show (b);
	gtk_table_attach (GTK_TABLE (tbl), b, 0, 2, 2, 3, 
			  (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 
			  (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 
			  0, 0);
	gtk_signal_connect (GTK_OBJECT (b), "clicked", GTK_SIGNAL_FUNC (sp_rc_defaults), tbl);

	return tbl;
}
