#define __SP_DESKTOP_C__

/*
 * Editable view and widget implementation
 *
 * Author:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * Copyright (C) 1999-2002 Lauris Kaplinski
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <config.h>

#include <math.h>
#include <string.h>

#include <glib.h>

#include <gtk/gtk.h>

#include <gtk/gtkdialog.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkalignment.h>

#include "macros.h"
#include "helper/sp-intl.h"
#include "helper/sp-marshal.h"
#include "display/gnome-canvas-acetate.h"
#include "display/sodipodi-ctrlrect.h"
#include "helper/units.h"
#include "helper/sp-intl.h"
#include "libnr/nr-matrix-ops.h"
#include "widgets/button.h"
#include "widgets/ruler.h"
#include "widgets/icon.h"
#include "widgets/widget-sizes.h"
#include "widgets/spw-utilities.h"
#include "widgets/spinbutton-events.h"
#include "display/canvas-arena.h"
#include "forward.h"
#include "inkscape-private.h"
#include "desktop.h"
#include "desktop-events.h"
#include "desktop-affine.h"
#include "document.h"
#include "selection.h"
#include "select-context.h"
#include "sp-namedview.h"
#include "sp-item.h"
#include "sp-root.h"
#include "interface.h"
#include "dialogs/dialog-events.h"
#include "toolbox.h"
#include "prefs-utils.h"

#include "file.h"

#include <extension/extension.h>
#include <extension/db.h>

enum {
    ACTIVATE,
    DEACTIVATE,
    MODIFIED,
    EVENT_CONTEXT_CHANGED,
    LAST_SIGNAL
};

static void sp_desktop_class_init(SPDesktopClass *klass);
static void sp_desktop_init(SPDesktop *desktop);
static void sp_desktop_dispose(GObject *object);

static void sp_desktop_request_redraw (SPView *view);
static void sp_desktop_set_document (SPView *view, SPDocument *doc);
static void sp_desktop_document_resized (SPView *view, SPDocument *doc, gdouble width, gdouble height);

/* Constructor */

static SPView *sp_desktop_new (SPNamedView *nv, SPCanvas *canvas);

static void sp_desktop_prepare_shutdown (SPDesktop *dt);

static void sp_dt_namedview_modified (SPNamedView *nv, guint flags, SPDesktop *desktop);
static void sp_desktop_selection_modified (SPSelection *selection, guint flags, SPDesktop *desktop);

static void sp_dt_update_snap_distances (SPDesktop *desktop);

static gint sp_dtw_zoom_input (GtkSpinButton *spin, gdouble *new_val, gpointer data);
static gboolean sp_dtw_zoom_output (GtkSpinButton *spin, gpointer data);
static void sp_dtw_zoom_value_changed (GtkSpinButton *spin, gpointer data);
static void sp_dtw_zoom_populate_popup (GtkEntry *entry, GtkMenu *menu, gpointer data);
static void sp_dtw_zoom_50 (GtkMenuItem *item, gpointer data);
static void sp_dtw_zoom_100 (GtkMenuItem *item, gpointer data);
static void sp_dtw_zoom_200 (GtkMenuItem *item, gpointer data);
static void sp_dtw_zoom_page (GtkMenuItem *item, gpointer data);
static void sp_dtw_zoom_drawing (GtkMenuItem *item, gpointer data);
static void sp_dtw_zoom_selection (GtkMenuItem *item, gpointer data);

/* fixme: This is here, but shouldn't in theory (Lauris) */
static void sp_desktop_widget_update_rulers (SPDesktopWidget *dtw);
static void sp_desktop_update_scrollbars (SPDesktop *desktop);

SPViewClass *parent_class;
static guint signals[LAST_SIGNAL] = { 0 };

GType
sp_desktop_get_type (void)
{
    static GType type = 0;
    if (!type) {
        GTypeInfo info = {
            sizeof (SPDesktopClass),
            NULL, NULL,
            (GClassInitFunc) sp_desktop_class_init,
            NULL, NULL,
            sizeof (SPDesktop),
            4,
            (GInstanceInitFunc) sp_desktop_init,
            NULL
        };
        type = g_type_register_static (SP_TYPE_VIEW, "SPDesktop", &info, (GTypeFlags)0);
    }
    return type;
}

static void
sp_desktop_class_init (SPDesktopClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    SPViewClass *view_class = (SPViewClass *) klass;

    parent_class = (SPViewClass*)g_type_class_peek_parent (klass);

    signals[ACTIVATE] = g_signal_new ("activate",
                                      G_TYPE_FROM_CLASS(klass),
                                      G_SIGNAL_RUN_FIRST,
                                      G_STRUCT_OFFSET (SPDesktopClass, activate),
                                      NULL, NULL,
                                      sp_marshal_NONE__NONE,
                                      G_TYPE_NONE, 0);
    signals[DEACTIVATE] = g_signal_new ("deactivate",
                                        G_TYPE_FROM_CLASS(klass),
                                        G_SIGNAL_RUN_FIRST,
                                        G_STRUCT_OFFSET (SPDesktopClass, deactivate),
                                        NULL, NULL,
                                        sp_marshal_NONE__NONE,
                                        G_TYPE_NONE, 0);
    signals[MODIFIED] = g_signal_new ("modified",
                                      G_TYPE_FROM_CLASS(klass),
                                      G_SIGNAL_RUN_FIRST,
                                      G_STRUCT_OFFSET (SPDesktopClass, modified),
                                      NULL, NULL,
                                      sp_marshal_NONE__UINT,
                                      G_TYPE_NONE, 1,
                                      G_TYPE_UINT);
    signals[EVENT_CONTEXT_CHANGED] = g_signal_new ("event_context_changed",
                                                   G_TYPE_FROM_CLASS(klass),
                                                   G_SIGNAL_RUN_FIRST,
                                                   G_STRUCT_OFFSET (SPDesktopClass, event_context_changed),
                                                   NULL, NULL,
                                                   sp_marshal_NONE__POINTER,
                                                   G_TYPE_NONE, 1,
                                                   G_TYPE_POINTER);

    object_class->dispose = sp_desktop_dispose;

    view_class->request_redraw = sp_desktop_request_redraw;
    view_class->set_document = sp_desktop_set_document;
    view_class->document_resized = sp_desktop_document_resized;
}

static void
sp_desktop_init (SPDesktop *desktop)
{
    desktop->namedview = NULL;
    desktop->selection = NULL;
    desktop->acetate = NULL;
    desktop->main = NULL;
    desktop->grid = NULL;
    desktop->guides = NULL;
    desktop->drawing = NULL;
    desktop->sketch = NULL;
    desktop->controls = NULL;

    desktop->d2w.set_identity();
    desktop->w2d.set_identity();
    desktop->doc2dt = NR::Matrix(NR::scale(0.8, -0.8));

    desktop->guides_active = FALSE;

    desktop->zooms_past = NULL;
    desktop->zooms_future = NULL;
    desktop->can_go_forward = FALSE;
#ifdef HAVE_GTK_WINDOW_FULLSCREEN
    desktop->is_fullscreen = FALSE;
#endif

    new (&desktop->sel_modified_connection) SigC::Connection();
}

static void
sp_desktop_dispose (GObject *object)
{
    SPDesktop *dt = SP_DESKTOP (object);

    if (dt->inkscape) {
        inkscape_remove_desktop (dt);
        dt->inkscape = NULL;
    }

    while (dt->event_context) {
        SPEventContext *ec = dt->event_context;
        dt->event_context = ec->next;
        sp_event_context_finish (ec);
        g_object_unref (G_OBJECT (ec));
    }

    if (dt->selection) {
	dt->selection->unreference();
        dt->selection = NULL;
    }

    if (dt->drawing) {
        sp_item_invoke_hide (SP_ITEM (sp_document_root (SP_VIEW_DOCUMENT (dt))), dt->dkey);
        dt->drawing = NULL;
    }

    G_OBJECT_CLASS (parent_class)->dispose (object);

    g_list_free (dt->zooms_past);
    g_list_free (dt->zooms_future);

    dt->sel_modified_connection.disconnect();
    dt->sel_modified_connection.~Connection();
}

static void
sp_desktop_request_redraw (SPView *view)
{
    SPDesktop *dt = SP_DESKTOP (view);

    if (dt->main) {
        gtk_widget_queue_draw (GTK_WIDGET (SP_CANVAS_ITEM (dt->main)->canvas));
    }
}

static void
sp_desktop_document_resized (SPView *view, SPDocument *doc, gdouble width, gdouble height)
{
    SPDesktop *desktop = SP_DESKTOP (view);

    desktop->doc2dt[5] = height;

    sp_canvas_item_affine_absolute (SP_CANVAS_ITEM (desktop->drawing), desktop->doc2dt);

    sp_ctrlrect_set_area (SP_CTRLRECT (desktop->page), 0.0, 0.0, width, height);
}

void
sp_desktop_set_active (SPDesktop *desktop, gboolean active)
{
    if (active != desktop->active) {
        desktop->active = active;
        if (active) {
            g_signal_emit (G_OBJECT (desktop), signals[ACTIVATE], 0);
        } else {
            g_signal_emit (G_OBJECT (desktop), signals[DEACTIVATE], 0);
        }
    }
}

/* fixme: */

static gint
arena_handler (SPCanvasArena *arena, NRArenaItem *ai, GdkEvent *event, SPDesktop *desktop)
{
    if (ai) {
        SPItem *spi = (SPItem*)NR_ARENA_ITEM_GET_DATA (ai);
        return sp_event_context_item_handler (desktop->event_context, spi, event);
    } else {
        return sp_event_context_root_handler (desktop->event_context, event);
    }
}

/* Constructor */

static SPView *
sp_desktop_new (SPNamedView *namedview, SPCanvas *canvas)
{
    SPDocument *document = SP_OBJECT_DOCUMENT (namedview);
    /* Kill flicker */
    sp_document_ensure_up_to_date (document);

    /* Setup widget */
    SPDesktop *desktop = (SPDesktop *) g_object_new (SP_TYPE_DESKTOP, NULL);

    /* Connect document */
    sp_view_set_document (SP_VIEW (desktop), document);

    desktop->namedview = namedview;
    g_signal_connect (G_OBJECT (namedview), "modified", G_CALLBACK (sp_dt_namedview_modified), desktop);
    desktop->number = sp_namedview_viewcount (namedview);

    /* Setup Canvas */
    g_object_set_data (G_OBJECT (canvas), "SPDesktop", desktop);

    SPCanvasGroup *root = sp_canvas_root (canvas);

    /* Setup adminstrative layers */
    desktop->acetate = sp_canvas_item_new (root, GNOME_TYPE_CANVAS_ACETATE, NULL);
    g_signal_connect (G_OBJECT (desktop->acetate), "event", G_CALLBACK (sp_desktop_root_handler), desktop);
    desktop->main = (SPCanvasGroup *) sp_canvas_item_new (root, SP_TYPE_CANVAS_GROUP, NULL);
    g_signal_connect (G_OBJECT (desktop->main), "event", G_CALLBACK (sp_desktop_root_handler), desktop);
    /* fixme: */
    /* page = (SPCanvasGroup *) sp_canvas_item_new (desktop->main, SP_TYPE_CANVAS_GROUP, NULL); */
    desktop->page = sp_canvas_item_new (desktop->main, SP_TYPE_CTRLRECT, NULL);

    desktop->drawing = sp_canvas_item_new (desktop->main, SP_TYPE_CANVAS_ARENA, NULL);
    g_signal_connect (G_OBJECT (desktop->drawing), "arena_event", G_CALLBACK (arena_handler), desktop);

    desktop->grid = (SPCanvasGroup *) sp_canvas_item_new (desktop->main, SP_TYPE_CANVAS_GROUP, NULL);
    desktop->guides = (SPCanvasGroup *) sp_canvas_item_new (desktop->main, SP_TYPE_CANVAS_GROUP, NULL);
    desktop->sketch = (SPCanvasGroup *) sp_canvas_item_new (desktop->main, SP_TYPE_CANVAS_GROUP, NULL);
    desktop->controls = (SPCanvasGroup *) sp_canvas_item_new (desktop->main, SP_TYPE_CANVAS_GROUP, NULL);

    desktop->selection = sp_selection_new (desktop);

    /* Push select tool to the bottom of stack */
    sp_desktop_push_event_context (desktop, SP_TYPE_SELECT_CONTEXT, "tools.select", SP_EVENT_CONTEXT_STATIC);

    // display rect and zoom are now handled in sp_desktop_widget_realize(), 
    /* desktop->page = sp_canvas_item_new (page, SP_TYPE_CTRLRECT, NULL); */
    sp_ctrlrect_set_area (SP_CTRLRECT (desktop->page), 0.0, 0.0, sp_document_width (document), sp_document_height (document));
        
    /* the following sets the page shadow on the canvas
       It was originally set to 5, which is really cheesy!
       It now is an attribute in the document's namedview. If a value of
       0 is used, then the constructor for a shadow is not initialized.
    */        

    if ( desktop->namedview->pageshadow != 0 ) {
        sp_ctrlrect_set_shadow (SP_CTRLRECT (desktop->page), desktop->namedview->pageshadow, 0x3f3f3fff);
    }

    /* Connect event for page resize */
    desktop->doc2dt[5] = sp_document_height (document);
    sp_canvas_item_affine_absolute (SP_CANVAS_ITEM (desktop->drawing), desktop->doc2dt);

    desktop->sel_modified_connection.disconnect();
    desktop->sel_modified_connection = desktop->selection->connectModified(
        SigC::bind(
            SigC::slot(&sp_desktop_selection_modified),
            desktop
        )
    );

    desktop->dkey = sp_item_display_key_new (1);
    NRArenaItem *ai = sp_item_invoke_show (SP_ITEM (sp_document_root (SP_VIEW_DOCUMENT (desktop))),
                                           SP_CANVAS_ARENA (desktop->drawing)->arena, desktop->dkey, SP_ITEM_SHOW_DISPLAY);
    if (ai) {
        nr_arena_item_add_child (SP_CANVAS_ARENA (desktop->drawing)->root, ai, NULL);
        nr_arena_item_unref (ai);
    }

    sp_namedview_show (desktop->namedview, desktop);
    /* Ugly hack */
    sp_desktop_activate_guides (desktop, TRUE);
    /* Ugly hack */
    sp_dt_namedview_modified (desktop->namedview, SP_OBJECT_MODIFIED_FLAG, desktop);

    // ?
    // sp_active_desktop_set (desktop);
    inkscape_add_desktop (desktop);
    desktop->inkscape = INKSCAPE;

    return SP_VIEW (desktop);
}

static void
sp_desktop_prepare_shutdown (SPDesktop *dt)
{
    while (dt->event_context) {
        SPEventContext *ec = dt->event_context;
        dt->event_context = ec->next;
        sp_event_context_finish (ec);
        g_object_unref (G_OBJECT (ec));
    }

    dt->sel_modified_connection.disconnect();

    if (dt->selection) {
        dt->selection->unreference();
        dt->selection = NULL;
    }

    if (dt->namedview) {
        sp_signal_disconnect_by_data (dt->namedview, dt);
        sp_namedview_hide (dt->namedview, dt);
        dt->namedview = NULL;
    }

    if (dt->drawing) {
        sp_item_invoke_hide (SP_ITEM (sp_document_root (SP_VIEW_DOCUMENT (dt))), dt->dkey);
        dt->drawing = NULL;
    }

    if (dt->inkscape) {
        inkscape_remove_desktop (dt);
        dt->inkscape = NULL;
    }
}

static void
sp_dt_namedview_modified (SPNamedView *nv, guint flags, SPDesktop *desktop)
{
    if (flags && SP_OBJECT_MODIFIED_FLAG) {
        /* Recalculate snap distances */
        sp_dt_update_snap_distances (desktop);
        /* Show/hide page border */
        if (nv->showborder) {
            sp_canvas_item_show (desktop->page);
            int order = sp_canvas_item_order (desktop->page);
            if (nv->borderlayer == SP_BORDER_LAYER_BOTTOM) {
                if (order) sp_canvas_item_lower (desktop->page, order);
            } else {
                int morder = sp_canvas_item_order (desktop->drawing);
                if (morder > order) sp_canvas_item_raise (desktop->page, morder - order);
            }
            sp_ctrlrect_set_color ((SPCtrlRect *) desktop->page,
                                   nv->bordercolor,
                                   (nv->pagecolor & 0xffffff00) != 0xffffff00,
                                   nv->pagecolor | 0xff);
            if (nv->pageshadow)
                sp_ctrlrect_set_shadow ((SPCtrlRect *)desktop->page, nv->pageshadow, nv->bordercolor);
        } else {
            if (!(nv->bordercolor & 0xff) && !(nv->pagecolor & 0xff)) {
                sp_canvas_item_hide (desktop->page);
            } else {
                sp_ctrlrect_set_color ((SPCtrlRect *) desktop->page,
                                       0x00000000,
                                       nv->pagecolor & 0xff,
                                       nv->pagecolor);
                if (nv->pageshadow)
                    sp_ctrlrect_set_shadow ((SPCtrlRect *)desktop->page, 0,
                                            0x00000000);
            }
        }
    }
}

static void
sp_dt_update_snap_distances (SPDesktop *desktop)
{
    static const SPUnit *px = NULL;

    if (!px) {
	px = sp_unit_get_by_abbreviation("px");
    }

    // Fixme: expansion?
    gdouble const px2doc = sqrt (fabs (desktop->w2d[0] * desktop->w2d[3]));
    desktop->gridsnap = (desktop->namedview->snaptogrid) ? desktop->namedview->gridtolerance : 0.0;
    sp_convert_distance_full (&desktop->gridsnap, desktop->namedview->gridtoleranceunit, px, 1.0, px2doc);
    desktop->guidesnap = (desktop->namedview->snaptoguides) ? desktop->namedview->guidetolerance : 0.0;
    sp_convert_distance_full (&desktop->guidesnap, desktop->namedview->guidetoleranceunit, px, 1.0, px2doc);
}

void
sp_desktop_activate_guides(SPDesktop *desktop, gboolean activate)
{
    desktop->guides_active = activate;
    sp_namedview_activate_guides (desktop->namedview, desktop, activate);
}

static void
sp_desktop_set_document (SPView *view, SPDocument *doc)
{
    SPDesktop *desktop = SP_DESKTOP (view);

    if (view->doc) {
        sp_namedview_hide (desktop->namedview, desktop);
        sp_item_invoke_hide (SP_ITEM (sp_document_root (SP_VIEW_DOCUMENT (desktop))), desktop->dkey);
    }

    /* fixme: */
    if (desktop->drawing) {
        NRArenaItem *ai;

        desktop->namedview = sp_document_namedview (doc, NULL);

        ai = sp_item_invoke_show (SP_ITEM (sp_document_root (doc)), SP_CANVAS_ARENA (desktop->drawing)->arena,
                                  desktop->dkey, SP_ITEM_SHOW_DISPLAY);
        if (ai) {
            nr_arena_item_add_child (SP_CANVAS_ARENA (desktop->drawing)->root, ai, NULL);
            nr_arena_item_unref (ai);
        }
        sp_namedview_show (desktop->namedview, desktop);
        /* Ugly hack */
        sp_desktop_activate_guides (desktop, TRUE);
	/* Ugly hack */
	sp_dt_namedview_modified (desktop->namedview, SP_OBJECT_MODIFIED_FLAG, desktop);
    }
}

void
sp_desktop_change_document (SPDesktop *desktop, SPDocument *document)
{
    g_return_if_fail (desktop != NULL);
    g_return_if_fail (SP_IS_DESKTOP (desktop));
    g_return_if_fail (document != NULL);
    g_return_if_fail (SP_IS_DOCUMENT (document));

    sp_view_set_document (SP_VIEW (desktop), document);
}

/* Private methods */

static void
sp_desktop_selection_modified (SPSelection *selection, guint flags, SPDesktop *desktop)
{
    sp_desktop_update_scrollbars (desktop);
}

/* Public methods */

/* Context switching */

void
sp_desktop_set_event_context (SPDesktop *dt, GtkType type, const gchar *config)
{
    SPEventContext *ec;
    while (dt->event_context) {
        ec = dt->event_context;
        sp_event_context_deactivate (ec);
        dt->event_context = ec->next;
        sp_event_context_finish (ec);
        g_object_unref (G_OBJECT (ec));
    }

    SPRepr *repr = (config) ? inkscape_get_repr (INKSCAPE, config) : NULL;
    ec = sp_event_context_new (type, dt, repr, SP_EVENT_CONTEXT_STATIC);
    ec->next = dt->event_context;
    dt->event_context = ec;
    sp_event_context_activate (ec);
    g_signal_emit (G_OBJECT (dt), signals[EVENT_CONTEXT_CHANGED], 0, ec);
}

void
sp_desktop_push_event_context (SPDesktop *dt, GtkType type, const gchar *config, unsigned int key)
{
    SPEventContext *ref, *ec;
    SPRepr *repr;

    if (dt->event_context && dt->event_context->key == key) return;
    ref = dt->event_context;
    while (ref && ref->next && ref->next->key != key) ref = ref->next;
    if (ref && ref->next) {
        ec = ref->next;
        ref->next = ec->next;
        sp_event_context_finish (ec);
        g_object_unref (G_OBJECT (ec));
    }

    if (dt->event_context) sp_event_context_deactivate (dt->event_context);
    repr = (config) ? inkscape_get_repr (INKSCAPE, config) : NULL;
    ec = sp_event_context_new (type, dt, repr, key);
    ec->next = dt->event_context;
    dt->event_context = ec;
    sp_event_context_activate (ec);
    g_signal_emit (G_OBJECT (dt), signals[EVENT_CONTEXT_CHANGED], 0, ec);
}

void
sp_desktop_pop_event_context (SPDesktop *dt, unsigned int key)
{
    SPEventContext *ec = NULL;

    if (dt->event_context && dt->event_context->key == key) {
        g_return_if_fail (dt->event_context);
        g_return_if_fail (dt->event_context->next);
        ec = dt->event_context;
        sp_event_context_deactivate (ec);
        dt->event_context = ec->next;
        sp_event_context_activate (dt->event_context);
        g_signal_emit (G_OBJECT (dt), signals[EVENT_CONTEXT_CHANGED], 0, ec);
    }

    SPEventContext *ref = dt->event_context;
    while (ref && ref->next && ref->next->key != key)
        ref = ref->next;
        
    if (ref && ref->next) {
        ec = ref->next;
        ref->next = ec->next;
    }

    if (ec) {
        sp_event_context_finish (ec);
        g_object_unref (G_OBJECT (ec));
    }
}

/* Private helpers */

//TODO: below comment is by lauris; I don't quite understand his underlines idea. Let's wait and see if this 
//ever gets implemented in sodipodi. I filled in a simple implementation for sp_desktop_set_coordinate_status
//which was empty. --bb

/* fixme: The idea to have underlines is good, but have to fit it into desktop/widget framework (Lauris) */
/* set the coordinate statusbar underline single coordinates with undeline-mask 
 * x and y are document coordinates
 * underline :
 *   0 - don't underline;
 *   1 - underlines x;  (i.e. 1 << NR::X)
 *   2 - underlines y;  (i.e. 1 << NR::Y)
 *   3 - underline both
 * Currently this is unimplemented and most callers don't use it.
 * It doesn't work well with non-axis-aligned guideline moving.
 * Thus we may just get rid of it.
 */
void
sp_desktop_set_coordinate_status (SPDesktop *desktop, NR::Point p, guint underline)
{
    gchar cstr[64];
    
    g_snprintf (cstr, 64, "%6.1f, %6.1f", p[0], p[1]);

    gtk_label_set_text (GTK_LABEL (desktop->owner->coord_status), cstr);
}

const SPUnit *
sp_desktop_get_default_unit (SPDesktop *dt)
{
    return dt->namedview->gridunit;
}

SPItem *
sp_desktop_item_at_point (SPDesktop const *desktop, NR::Point const p, gboolean into_groups)
{
    SPDocument *document = SP_VIEW (desktop)->doc;
    g_return_val_if_fail (document != NULL, NULL);
    return sp_document_item_at_point (document, desktop->dkey, p, into_groups);
}

SPItem *
sp_desktop_group_at_point (SPDesktop const *desktop, NR::Point const p)
{
    SPDocument *document = SP_VIEW (desktop)->doc;
    g_return_val_if_fail (document != NULL, NULL);
    return sp_document_group_at_point (document, desktop->dkey, p);
}

/**
\brief  Returns the mouse point in document coordinates; if mouse is outside the canvas, returns the center of canvas viewpoint
*/
NR::Point 
sp_desktop_point (SPDesktop const *desktop)
{
	gint x, y;
	gdk_window_get_pointer (GTK_WIDGET (desktop->owner->canvas)->window, &x, &y, NULL);
	NR::Point pw = sp_canvas_window_to_world (desktop->owner->canvas, NR::Point(x, y));
	NR::Point p = sp_desktop_w2d_xy_point (desktop, pw);

	NRRect r;
	sp_canvas_get_viewbox (desktop->owner->canvas, &r);

	NR::Point r0 = sp_desktop_w2d_xy_point (desktop, NR::Point(r.x0, r.y0));
	NR::Point r1 = sp_desktop_w2d_xy_point (desktop, NR::Point(r.x1, r.y1));

	if (p[NR::X] >= r0[NR::X] && p[NR::X] <= r1[NR::X] && p[NR::Y] >= r1[NR::Y] && p[NR::Y] <= r0[NR::Y]) {
		return p;
	} else {
		return (r0 + r1) / 2;
	}
}



/* SPDesktopWidget */

static void sp_desktop_widget_class_init (SPDesktopWidgetClass *klass);
static void sp_desktop_widget_init (SPDesktopWidget *widget);
static void sp_desktop_widget_destroy (GtkObject *object);

static void sp_desktop_widget_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static void sp_desktop_widget_realize (GtkWidget *widget);

static gint sp_desktop_widget_event (GtkWidget *widget, GdkEvent *event, SPDesktopWidget *dtw);

static void sp_desktop_widget_view_position_set (SPView *view, gdouble x, gdouble y, SPDesktopWidget *dtw);
static void sp_desktop_widget_view_status_set (SPView *view, const gchar *status, guint msec, SPDesktopWidget *dtw);
gboolean sp_desktop_widget_view_status_remove (gpointer data);

static void sp_dtw_desktop_activate (SPDesktop *desktop, SPDesktopWidget *dtw);
static void sp_dtw_desktop_deactivate (SPDesktop *desktop, SPDesktopWidget *dtw);

static void sp_desktop_widget_adjustment_value_changed (GtkAdjustment *adj, SPDesktopWidget *dtw);
static void sp_desktop_widget_namedview_modified (SPNamedView *nv, guint flags, SPDesktopWidget *dtw);

static void sp_desktop_widget_update_zoom (SPDesktopWidget *dtw);

SPViewWidgetClass *dtw_parent_class;

GtkType
sp_desktop_widget_get_type (void)
{
    static GtkType type = 0;
    if (!type) {
        static const GtkTypeInfo info = {
            "SPDesktopWidget",
            sizeof (SPDesktopWidget),
            sizeof (SPDesktopWidgetClass),
            (GtkClassInitFunc) sp_desktop_widget_class_init,
            (GtkObjectInitFunc) sp_desktop_widget_init,
            NULL, NULL, NULL
        };
        type = gtk_type_unique (SP_TYPE_VIEW_WIDGET, &info);
    }
    return type;
}

static void
sp_desktop_widget_class_init (SPDesktopWidgetClass *klass)
{
    dtw_parent_class = (SPViewWidgetClass*)gtk_type_class (SP_TYPE_VIEW_WIDGET);

    GtkObjectClass *object_class = (GtkObjectClass *) klass;
    GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

    object_class->destroy = sp_desktop_widget_destroy;

    widget_class->size_allocate = sp_desktop_widget_size_allocate;
    widget_class->realize = sp_desktop_widget_realize;
}

static void
sp_desktop_widget_init (SPDesktopWidget *dtw)
{
    GtkWidget *widget;
    GtkWidget *tbl;
    GtkWidget *w;

    GtkWidget *hbox, *vbox;
    GtkWidget *sbar;
    GtkWidget *coord_box;
    GtkWidget *eventbox;
    GtkTooltips *tt;
    GtkStyle *style;

    widget = GTK_WIDGET (dtw);

    dtw->desktop = NULL;

    dtw->decorations = TRUE;
    dtw->statusbar = TRUE;

    tt = gtk_tooltips_new ();

    /* Main table */
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (dtw), vbox);

    sbar = gtk_hbox_new (FALSE, 0);
    gtk_widget_set_usize (sbar, -1, BOTTOM_BAR_HEIGHT);
    gtk_box_pack_end (GTK_BOX (vbox), sbar, FALSE, TRUE, 0);

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_end (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
    gtk_widget_show (hbox);

    dtw->aux_toolbox = sp_aux_toolbox_new ();
    gtk_box_pack_end (GTK_BOX (vbox), dtw->aux_toolbox, FALSE, TRUE, 0);

    dtw->tool_toolbox = sp_tool_toolbox_new ();
    gtk_box_pack_start (GTK_BOX (hbox), dtw->tool_toolbox, FALSE, TRUE, 0);

    tbl = gtk_table_new (4, 3, FALSE);
    gtk_box_pack_start (GTK_BOX (hbox), tbl, TRUE, TRUE, 1);

    /* Horizontal ruler */
    eventbox = gtk_event_box_new ();
    dtw->hruler = sp_hruler_new ();
    sp_ruler_set_metric (GTK_RULER (dtw->hruler), SP_PT);
    gtk_container_add (GTK_CONTAINER (eventbox), dtw->hruler);
    gtk_table_attach (GTK_TABLE (tbl), eventbox, 1, 2, 0, 1, (GtkAttachOptions)(GTK_FILL), (GtkAttachOptions)(GTK_FILL), widget->style->xthickness, 0);
    g_signal_connect (G_OBJECT (eventbox), "button_press_event", G_CALLBACK (sp_dt_hruler_event), dtw);
    g_signal_connect (G_OBJECT (eventbox), "button_release_event", G_CALLBACK (sp_dt_hruler_event), dtw);
    g_signal_connect (G_OBJECT (eventbox), "motion_notify_event", G_CALLBACK (sp_dt_hruler_event), dtw);

    /* Vertical ruler */
    eventbox = gtk_event_box_new ();
    dtw->vruler = sp_vruler_new ();
    sp_ruler_set_metric (GTK_RULER (dtw->vruler), SP_PT);
    gtk_container_add (GTK_CONTAINER (eventbox), GTK_WIDGET (dtw->vruler));
    gtk_table_attach (GTK_TABLE (tbl), eventbox, 0, 1, 1, 2, (GtkAttachOptions)(GTK_FILL), (GtkAttachOptions)(GTK_FILL), 0, widget->style->ythickness);
    g_signal_connect (G_OBJECT (eventbox), "button_press_event", G_CALLBACK (sp_dt_vruler_event), dtw);
    g_signal_connect (G_OBJECT (eventbox), "button_release_event", G_CALLBACK (sp_dt_vruler_event), dtw);
    g_signal_connect (G_OBJECT (eventbox), "motion_notify_event", G_CALLBACK (sp_dt_vruler_event), dtw);

    /* Horizontal scrollbar */
    dtw->hadj = (GtkAdjustment *) gtk_adjustment_new (0.0, -4000.0, 4000.0, 10.0, 100.0, 4.0);
    dtw->hscrollbar = gtk_hscrollbar_new (GTK_ADJUSTMENT (dtw->hadj));
    gtk_table_attach (GTK_TABLE (tbl), dtw->hscrollbar, 1, 2, 2, 3, (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), (GtkAttachOptions)(GTK_FILL), 0, 0);
    /* Vertical scrollbar */
    dtw->vadj = (GtkAdjustment *) gtk_adjustment_new (0.0, -4000.0, 4000.0, 10.0, 100.0, 4.0);
    dtw->vscrollbar = gtk_vscrollbar_new (GTK_ADJUSTMENT (dtw->vadj));
    gtk_table_attach (GTK_TABLE (tbl), dtw->vscrollbar, 2, 3, 1, 2, (GtkAttachOptions)(GTK_FILL), (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 0, 0);

    /* Canvas */
    w = gtk_frame_new (NULL);
    //        gtk_frame_set_shadow_type (GTK_FRAME (w), GTK_SHADOW_IN);
    gtk_table_attach (GTK_TABLE (tbl), w, 1, 2, 1, 2, (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 0, 0);
    dtw->canvas = SP_CANVAS (sp_canvas_new_aa ());
    GTK_WIDGET_SET_FLAGS (GTK_WIDGET (dtw->canvas), GTK_CAN_FOCUS);
    style = gtk_style_copy (GTK_WIDGET (dtw->canvas)->style);
    style->bg[GTK_STATE_NORMAL] = style->white;
    gtk_widget_set_style (GTK_WIDGET (dtw->canvas), style);
    g_signal_connect (G_OBJECT (dtw->canvas), "event", G_CALLBACK (sp_desktop_widget_event), dtw);
    gtk_container_add (GTK_CONTAINER (w), GTK_WIDGET (dtw->canvas));

    // sticky zoom buton (FIXME: to be removed when we have this setting in preferences)
    dtw->sticky_zoom = sp_button_new_from_data (BOTTOM_BUTTON_SIZE,
                                                SP_BUTTON_TYPE_TOGGLE,
                                                NULL,
                                                "sticky_zoom",
                                                _("Zoom drawing if window size changes"),
                                                tt);
    gtk_box_pack_start (GTK_BOX (sbar), dtw->sticky_zoom, FALSE, FALSE, 0);

    // zoom status spinbutton
    dtw->zoom_status = gtk_spin_button_new_with_range (log(SP_DESKTOP_ZOOM_MIN)/log(2), log(SP_DESKTOP_ZOOM_MAX)/log(2), 0.1);
    gtk_tooltips_set_tip (tt, dtw->zoom_status, _("Zoom"), NULL);
    gtk_widget_set_usize (dtw->zoom_status, STATUS_ZOOM_WIDTH, -1);
    gtk_entry_set_width_chars (GTK_ENTRY (dtw->zoom_status), 6);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (dtw->zoom_status), FALSE);
    gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (dtw->zoom_status), GTK_UPDATE_ALWAYS);
    g_signal_connect (G_OBJECT (dtw->zoom_status), "input", G_CALLBACK (sp_dtw_zoom_input), dtw);
    g_signal_connect (G_OBJECT (dtw->zoom_status), "output", G_CALLBACK (sp_dtw_zoom_output), dtw);
    gtk_object_set_data (GTK_OBJECT (dtw->zoom_status), "dtw", dtw->canvas);
    gtk_signal_connect (GTK_OBJECT (dtw->zoom_status), "focus-in-event", GTK_SIGNAL_FUNC (spinbutton_focus_in), dtw->zoom_status);
    gtk_signal_connect (GTK_OBJECT (dtw->zoom_status), "key-press-event", GTK_SIGNAL_FUNC (spinbutton_keypress), dtw->zoom_status);
    dtw->zoom_update = g_signal_connect (G_OBJECT (dtw->zoom_status), "value_changed", G_CALLBACK (sp_dtw_zoom_value_changed), dtw);
    dtw->zoom_update = g_signal_connect (G_OBJECT (dtw->zoom_status), "populate_popup", G_CALLBACK (sp_dtw_zoom_populate_popup), dtw);
    sp_set_font_size (dtw->zoom_status, STATUS_ZOOM_FONT_SIZE);
    gtk_box_pack_start (GTK_BOX (sbar), dtw->zoom_status, FALSE, FALSE, 0);

    /* connecting canvas, scrollbars, rulers, statusbar */
    g_signal_connect (G_OBJECT (dtw->hadj), "value-changed", G_CALLBACK (sp_desktop_widget_adjustment_value_changed), dtw);
    g_signal_connect (G_OBJECT (dtw->vadj), "value-changed", G_CALLBACK (sp_desktop_widget_adjustment_value_changed), dtw);

    // cursor coordinates
    coord_box = gtk_vbox_new (FALSE, 0);
    dtw->coord_status = gtk_label_new ("");
    // FIXME: gtk seems to be unable to display tooltips for labels, let's hope they'll fix it sometime
    gtk_tooltips_set_tip (tt, dtw->coord_status, _("Cursor coordinates"), NULL);
    gtk_widget_set_usize (dtw->coord_status, STATUS_COORD_WIDTH, SP_ICON_SIZE_BUTTON);
    sp_set_font_size (dtw->coord_status, STATUS_COORD_FONT_SIZE);
    gtk_box_pack_start (GTK_BOX (coord_box), dtw->coord_status, FALSE, FALSE, STATUS_COORD_SKIP);
    gtk_box_pack_start (GTK_BOX (sbar), coord_box, FALSE, FALSE, 1);

    // statusbar
    dtw->select_status = gtk_statusbar_new ();
    sp_set_font_size (dtw->select_status, STATUS_BAR_FONT_SIZE);
    // display the initial welcome message in the statusbar
    gtk_statusbar_push (GTK_STATUSBAR (dtw->select_status), 0, _("Welcome to Inkscape! Use shape or freehand tools to create objects; use selector (arrow) to move or transform them."));
    gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (dtw->select_status), TRUE);
    gtk_box_pack_start (GTK_BOX (sbar), dtw->select_status, TRUE, TRUE, 0);

    gtk_widget_show_all (vbox);

    if (prefs_get_int_attribute ("options.showscrollbars", "value", 1) == 0) {
        gtk_widget_hide_all (dtw->hscrollbar);
        gtk_widget_hide_all (dtw->vscrollbar);
    }

    if (prefs_get_int_attribute ("options.showrulers", "value", 1) == 0) {
        gtk_widget_hide_all (dtw->hruler);
        gtk_widget_hide_all (dtw->vruler);
    }
}

static void
sp_desktop_widget_destroy (GtkObject *object)
{
    SPDesktopWidget *dtw = SP_DESKTOP_WIDGET (object);

    if (dtw->desktop) {
        g_object_unref (G_OBJECT (dtw->desktop));
        dtw->desktop = NULL;
    }

    if (GTK_OBJECT_CLASS (dtw_parent_class)->destroy) {
        (* GTK_OBJECT_CLASS (dtw_parent_class)->destroy) (object);
    }
}

/*
 * set the title in the desktop-window (if desktop has an own window)
 * the title has form file name: desktop number - Inkscape
 * desktop number is only shown if it's 2 or higher
 * the file name is read from the respective document
 */
static void
sp_desktop_widget_set_title (SPDesktopWidget *dtw)
{
    GtkWindow *window = GTK_WINDOW (gtk_object_get_data (GTK_OBJECT(dtw), "window"));
    if (window) {
        gchar const *uri = SP_DOCUMENT_NAME (SP_VIEW_WIDGET_DOCUMENT (dtw));
        gchar const *fname = ( SPShowFullFielName
			       ? uri
			       : g_basename(uri) );
        GString *name = g_string_new ("");
        if (dtw->desktop->number > 1) {
            g_string_sprintf (name, _("%s: %d - Inkscape"), fname, dtw->desktop->number);
        } else {
            g_string_sprintf (name, _("%s - Inkscape"), fname);
        }
        gtk_window_set_title (window, name->str);
        g_string_free (name, TRUE);
    }
}

static void
sp_desktop_widget_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
    SPDesktopWidget *dtw = SP_DESKTOP_WIDGET (widget);

    if ((allocation->x == widget->allocation.x) &&
        (allocation->y == widget->allocation.y) &&
        (allocation->width == widget->allocation.width) &&
        (allocation->height == widget->allocation.height)) {
        if (GTK_WIDGET_CLASS (dtw_parent_class)->size_allocate)
            GTK_WIDGET_CLASS (dtw_parent_class)->size_allocate (widget, allocation);
        return;
    }

    if (GTK_WIDGET_REALIZED (widget)) {
        NRRect area;
        double zoom;
        sp_desktop_get_display_area (dtw->desktop, &area);
        zoom = SP_DESKTOP_ZOOM (dtw->desktop);
        if (GTK_WIDGET_CLASS (dtw_parent_class)->size_allocate)
            GTK_WIDGET_CLASS (dtw_parent_class)->size_allocate (widget, allocation);
        if (SP_BUTTON_IS_DOWN (dtw->sticky_zoom)) {
            NRRect newarea;
            double zpsp;
            /* Calculate zoom per pixel */
            zpsp = zoom / hypot (area.x1 - area.x0, area.y1 - area.y0);
            /* Find new visible area */
            sp_desktop_get_display_area (dtw->desktop, &newarea);
            /* Calculate adjusted zoom */
            zoom = zpsp * hypot (newarea.x1 - newarea.x0, newarea.y1 - newarea.y0);
            sp_desktop_zoom_absolute (dtw->desktop, 0.5F * (area.x1 + area.x0), 0.5F * (area.y1 + area.y0), zoom);
        } else {
            sp_desktop_zoom_absolute (dtw->desktop, 0.5F * (area.x1 + area.x0), 0.5F * (area.y1 + area.y0), zoom);
        }
    } else {
        if (GTK_WIDGET_CLASS (dtw_parent_class)->size_allocate)
            GTK_WIDGET_CLASS (dtw_parent_class)->size_allocate (widget, allocation);
    }
}


static void
sp_desktop_widget_realize (GtkWidget *widget)
{

    SPDesktopWidget *dtw = SP_DESKTOP_WIDGET (widget);

    if (GTK_WIDGET_CLASS (dtw_parent_class)->realize)
        (* GTK_WIDGET_CLASS (dtw_parent_class)->realize) (widget);

    NRRect d;
    d.x0 = 0.0;
    d.y0 = 0.0;
    d.x1 = sp_document_width (SP_VIEW_WIDGET_DOCUMENT (dtw));
    d.y1 = sp_document_height (SP_VIEW_WIDGET_DOCUMENT (dtw));

    if ((fabs (d.x1 - d.x0) < 1.0) || (fabs (d.y1 - d.y0) < 1.0)) return;

    sp_desktop_set_display_area (dtw->desktop, d.x0, d.y0, d.x1, d.y1, 10);
        
    sp_desktop_widget_set_title (dtw);
}

static gint
sp_desktop_widget_event (GtkWidget *widget, GdkEvent *event, SPDesktopWidget *dtw)
{
    if ((event->type == GDK_BUTTON_PRESS) && (event->button.button == 3)) {
        if (event->button.state & GDK_SHIFT_MASK) {
            sp_canvas_arena_set_sticky (SP_CANVAS_ARENA (dtw->desktop->drawing), TRUE);
        } else {
            sp_canvas_arena_set_sticky (SP_CANVAS_ARENA (dtw->desktop->drawing), FALSE);
        }
    }

    if (GTK_WIDGET_CLASS (dtw_parent_class)->event) {
        return (* GTK_WIDGET_CLASS (dtw_parent_class)->event) (widget, event);
    } else {
        // The keypress events need to be passed to desktop handler explicitly, 
        // because otherwise the event contexts only receive keypresses when the mouse cursor 
        // is over the canvas. This redirection is only done for keypresses and only if there's no 
        // current item on the canvas, because item events and all mouse events are caught
        // and passed on by the canvas acetate (I think). --bb
        if (event->type == GDK_KEY_PRESS && !dtw->canvas->current_item) {
            return sp_desktop_root_handler (NULL, event, dtw->desktop);
        }
    }

    return FALSE;
}

void
sp_dtw_desktop_activate (SPDesktop *desktop, SPDesktopWidget *dtw)
{
    /* update active desktop indicator */
}

void
sp_dtw_desktop_deactivate (SPDesktop *desktop, SPDesktopWidget *dtw)
{
    /* update inactive desktop indicator */
}

/**
 *  Shuts down the desktop object for the view being closed.  It checks
 *  to see if the document has been edited, and if so prompts the user
 *  to save, discard, or cancel.  Returns TRUE if the shutdown operation
 *  is cancelled or if the save is cancelled or fails, FALSE otherwise.
 */
static gboolean
sp_dtw_desktop_shutdown (SPView *view, SPDesktopWidget *dtw)
{
    SPDocument *doc = SP_VIEW_DOCUMENT (view);

    if (doc && (((GObject *) doc)->ref_count == 1)) {
        if (sp_repr_attr (sp_document_repr_root (doc), "sodipodi:modified") != NULL) {
            GtkWidget *dialog;
                        
            dialog = gtk_message_dialog_new(
                GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(dtw))),
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_WARNING,
                GTK_BUTTONS_NONE,
                "Document modified");

            gchar *markup;
            /* FIXME !!! obviously this will have problems if the document name contains markup characters */
            markup = g_strdup_printf(
                _("<span weight=\"bold\" size=\"larger\">Save changes to document \"%s\" before closing?</span>\n\n"
                  "If you close without saving, your changes will be discarded."),
                SP_DOCUMENT_NAME(doc));

            /* FIXME !!! Gtk 2.3+ gives us
	       gtk_message_dialog_set_markup() (and actually even
	       gtk_message_dialog_new_with_markup(..., format, ...)!) --
	       until then, we will have to be a little bit evil here and
	       poke at GtkMessageDialog::label, which is private... */

            gtk_label_set_markup(GTK_LABEL(GTK_MESSAGE_DIALOG(dialog)->label), markup);
            g_free(markup);

            GtkWidget *close_button;
            close_button = gtk_button_new_with_mnemonic(_("Close _without saving"));
            gtk_widget_show(close_button);
            gtk_dialog_add_action_widget(GTK_DIALOG(dialog), close_button, GTK_RESPONSE_NO);
                        
            gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
            gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_SAVE, GTK_RESPONSE_YES);
            gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_YES);
                        
            gint response;
            response = gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);

            switch (response) {
            case GTK_RESPONSE_YES:
                sp_document_ref(doc);
                if (sp_file_save_document(doc)) {
                    sp_document_unref(doc);
                } else { // save dialog cancelled or save failed
                    sp_document_unref(doc);
                    return TRUE;
                }
		break;
            case GTK_RESPONSE_NO:
                break;
            default: // cancel pressed, or dialog was closed
                return TRUE;
                break;
            }
        }
		/* Code to check data loss */
		bool allow_data_loss = FALSE;
        while (sp_repr_attr (sp_document_repr_root (doc), "inkscape:dataloss") != NULL && allow_data_loss == FALSE) {
            GtkWidget *dialog;
                        
            dialog = gtk_message_dialog_new(
                GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(dtw))),
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_WARNING,
                GTK_BUTTONS_NONE,
                "Document modified");

            gchar *markup;
            /* FIXME !!! obviously this will have problems if the document name contains markup characters */
            markup = g_strdup_printf(
                _("<span weight=\"bold\" size=\"larger\">The file \"%s\" was saved with a format (%s) that may cause data loss!</span>\n\n"
                  "Do you want to save this file in another format?"),
                SP_DOCUMENT_NAME(doc),
				Inkscape::Extension::db.get(sp_repr_attr(sp_document_repr_root(doc), "inkscape:output_extension"))->get_name());

            /* FIXME !!! Gtk 2.3+ gives us
	       gtk_message_dialog_set_markup() (and actually even
	       gtk_message_dialog_new_with_markup(..., format, ...)!) --
	       until then, we will have to be a little bit evil here and
	       poke at GtkMessageDialog::label, which is private... */

            gtk_label_set_markup(GTK_LABEL(GTK_MESSAGE_DIALOG(dialog)->label), markup);
            g_free(markup);

            GtkWidget *close_button;
            close_button = gtk_button_new_with_mnemonic(_("Close _without saving"));
            gtk_widget_show(close_button);
            gtk_dialog_add_action_widget(GTK_DIALOG(dialog), close_button, GTK_RESPONSE_NO);
                        
            gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
            gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_SAVE, GTK_RESPONSE_YES);
            gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_YES);
                        
            gint response;
            response = gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);

            switch (response) {
            case GTK_RESPONSE_YES:
                sp_document_ref(doc);
                if (sp_file_save_dialog(doc)) {
                    sp_document_unref(doc);
                } else { // save dialog cancelled or save failed
                    sp_document_unref(doc);
                    return TRUE;
                }
		break;
            case GTK_RESPONSE_NO:
				allow_data_loss = TRUE;
                break;
            default: // cancel pressed, or dialog was closed
                return TRUE;
                break;
            }
        }
    }

    sp_desktop_prepare_shutdown (SP_DESKTOP (view));

    return FALSE;
}

/* Constructor */

static void
sp_desktop_uri_set (SPView *view, const gchar *uri, SPDesktopWidget *dtw)
{
    sp_desktop_widget_set_title (dtw);
}

SPViewWidget *
sp_desktop_widget_new (SPNamedView *namedview)
{
    GtkWidget *mbar;

    SPDesktopWidget *dtw = (SPDesktopWidget*)gtk_type_new (SP_TYPE_DESKTOP_WIDGET);

    dtw->dt2r = 1.0 / namedview->gridunit->unittobase;
    dtw->ruler_origin = namedview->gridorigin;

    dtw->desktop = (SPDesktop *) sp_desktop_new (namedview, dtw->canvas);
    dtw->desktop->owner = dtw;
    g_object_set_data (G_OBJECT (dtw->desktop), "widget", dtw);

    /* Once desktop is set, we can update rulers */
    sp_desktop_widget_update_rulers (dtw);

    g_signal_connect (G_OBJECT (dtw->desktop), "uri_set", G_CALLBACK (sp_desktop_uri_set), dtw);
    sp_view_widget_set_view (SP_VIEW_WIDGET (dtw), SP_VIEW (dtw->desktop));

    g_signal_connect (G_OBJECT (dtw->desktop), "position_set", G_CALLBACK (sp_desktop_widget_view_position_set), dtw);
    g_signal_connect (G_OBJECT (dtw->desktop), "status_set", G_CALLBACK (sp_desktop_widget_view_status_set), dtw);

    /* Connect activation signals to update indicator */
    g_signal_connect (G_OBJECT (dtw->desktop), "activate", G_CALLBACK (sp_dtw_desktop_activate), dtw);
    g_signal_connect (G_OBJECT (dtw->desktop), "deactivate", G_CALLBACK (sp_dtw_desktop_deactivate), dtw);

    g_signal_connect (G_OBJECT (dtw->desktop), "shutdown", G_CALLBACK (sp_dtw_desktop_shutdown), dtw);

    /* Listen on namedview modification */
    g_signal_connect (G_OBJECT (namedview), "modified", G_CALLBACK (sp_desktop_widget_namedview_modified), dtw);

    mbar = sp_ui_main_menubar (SP_VIEW (dtw->desktop));
    gtk_widget_show_all (mbar);
    gtk_box_pack_start (GTK_BOX (gtk_bin_get_child (GTK_BIN (dtw))), mbar, FALSE, FALSE, 0);

    sp_tool_toolbox_set_desktop (dtw->tool_toolbox, dtw->desktop);
    sp_aux_toolbox_set_desktop (dtw->aux_toolbox, dtw->desktop);
        
    return SP_VIEW_WIDGET (dtw);
}

static void
sp_desktop_widget_view_position_set (SPView *view, double x, double y, SPDesktopWidget *dtw)
{
    using NR::X;
    using NR::Y;

    NR::Point const origin = dtw->dt2r * ( NR::Point(x, y) - dtw->ruler_origin );
    /* fixme: */
    GTK_RULER(dtw->hruler)->position = origin[X];
    gtk_ruler_draw_pos (GTK_RULER (dtw->hruler));
    GTK_RULER(dtw->vruler)->position = origin[Y];
    gtk_ruler_draw_pos (GTK_RULER (dtw->vruler));

    sp_desktop_set_coordinate_status(SP_DESKTOP(view), origin, 0);
}

/*
 * the statusbars
 *
 * we have 
 * - coordinate status   set with sp_desktop_coordinate_status which is currently not unset
 * - selection status    which is used in two ways:
 *    * sp_desktop_default_status sets the default status text which is visible
 *      if no other text is displayed
 *    * sp_desktop_set_status sets the status text and can be cleared
 with sp_desktop_clear_status making the default visible
*/

typedef struct {
    GtkStatusbar *sb; 
    guint message_id; 
} statusbar_data;

static void 
sp_desktop_widget_view_status_set (SPView *view, const gchar *status, guint msec, SPDesktopWidget *dtw)
{
    guint message_id = gtk_statusbar_push (GTK_STATUSBAR (dtw->select_status), 0, status ? status : "");

    if (msec != 0) { // we want to remove the message after msec milliseconds
        // save  the statusbar pointer and the message id
        statusbar_data *d = (statusbar_data *) g_malloc (sizeof (statusbar_data));
        d->sb = GTK_STATUSBAR (dtw->select_status);
        d->message_id = message_id; 
        // call the remove function with this data
        gtk_timeout_add (msec, (GtkFunction) sp_desktop_widget_view_status_remove, d);  
    }
}

gboolean
sp_desktop_widget_view_status_remove (gpointer d)
{
    gtk_statusbar_remove ((GtkStatusbar *) ((statusbar_data *) d)->sb, 0, ((statusbar_data *) d)->message_id);
    g_free (d); // freeing memory that was allocated in _status_set; ugly, but inevitable
    return FALSE; // this is a one-time timeout
}

static void
sp_desktop_widget_namedview_modified (SPNamedView *nv, guint flags, SPDesktopWidget *dtw)
{
    if (flags & SP_OBJECT_MODIFIED_FLAG) {
        dtw->dt2r = 1.0 / nv->gridunit->unittobase;
        dtw->ruler_origin = nv->gridorigin;
        sp_desktop_widget_update_rulers (dtw);
    }
}

static void
sp_desktop_widget_adjustment_value_changed (GtkAdjustment *adj, SPDesktopWidget *dtw)
{
    if (dtw->update)
        return;

    dtw->update = 1;

    sp_canvas_scroll_to (dtw->canvas, dtw->hadj->value, dtw->vadj->value, FALSE);
    sp_desktop_widget_update_rulers (dtw);

    dtw->update = 0;
}

/* we make the desktop window with focus active, signal is connected in interface.c */

gint
sp_desktop_widget_set_focus (GtkWidget *widget, GdkEvent *event, SPDesktopWidget *dtw)
{
    inkscape_activate_desktop (dtw->desktop);

    /* give focus to canvas widget */
    gtk_widget_grab_focus (GTK_WIDGET (dtw->canvas));

    return FALSE;
}


/* fixme: this are UI functions - find a better place for them (lauris) */

void
sp_desktop_toggle_rulers (SPDesktop *dt)
{
    if (GTK_WIDGET_VISIBLE (dt->owner->hruler)) {
        gtk_widget_hide_all (dt->owner->hruler);
        gtk_widget_hide_all (dt->owner->vruler);
        prefs_set_int_attribute ("options.showrulers", "value", 0);
    } else {
        gtk_widget_show_all (dt->owner->hruler);
        gtk_widget_show_all (dt->owner->vruler);
        prefs_set_int_attribute ("options.showrulers", "value", 1);
    }
}

void
sp_desktop_toggle_scrollbars (SPDesktop *dt)
{
    if (GTK_WIDGET_VISIBLE (dt->owner->hscrollbar)) {
        gtk_widget_hide_all (dt->owner->hscrollbar);
        gtk_widget_hide_all (dt->owner->vscrollbar);
        prefs_set_int_attribute ("options.showscrollbars", "value", 0);
    } else {
        gtk_widget_show_all (dt->owner->hscrollbar);
        gtk_widget_show_all (dt->owner->vscrollbar);
        prefs_set_int_attribute ("options.showscrollbars", "value", 1);
    }
}

/*
 * Sooner or later we want to implement two sets of methods
 * sp_desktop_... and sp_desktop_widget_...
 * To jump from first to second we probably want to
 * add single VEPV instead of filling desktop with signals
 * (Lauris)
 */

void
sp_desktop_set_display_area (SPDesktop *dt, double x0, double y0, double x1, double y1, double border)
{
    SPDesktopWidget *dtw = (SPDesktopWidget*)g_object_get_data (G_OBJECT (dt), "widget");
    if (!dtw) return;

    double cx = 0.5 * (x0 + x1);
    double cy = 0.5 * (y0 + y1);

    NRRect viewbox;
    sp_canvas_get_viewbox (dtw->canvas, &viewbox);

    // save the zoom 
    NRRect *old_zoom = g_new(NRRect, 1);
    old_zoom->x0 = x0;
    old_zoom->x1 = x1;
    old_zoom->y0 = y0;
    old_zoom->y1 = y1;
    if ( dt->zooms_past == NULL
	 || !( ( ((NRRect *) dt->zooms_past->data)->x0 == old_zoom->x0 ) &&
	       ( ((NRRect *) dt->zooms_past->data)->x1 == old_zoom->x1 ) &&
	       ( ((NRRect *) dt->zooms_past->data)->y0 == old_zoom->y0 ) &&
	       ( ((NRRect *) dt->zooms_past->data)->y1 == old_zoom->y1 ) ) )
    {
        dt->zooms_past = g_list_prepend (dt->zooms_past, old_zoom);
        if (dt->can_go_forward == FALSE) {
            g_list_free (dt->zooms_future);
            dt->zooms_future = NULL;
        }
        dt->can_go_forward = FALSE;
    }

    viewbox.x0 += border;
    viewbox.y0 += border;
    viewbox.x1 -= border;
    viewbox.y1 -= border;

    double scale = SP_DESKTOP_ZOOM (dt);
    double newscale;
    if (((x1 - x0) * (viewbox.y1 - viewbox.y0)) > ((y1 - y0) * (viewbox.x1 - viewbox.x0))) {
        newscale = (viewbox.x1 - viewbox.x0) / (x1 - x0);
    } else {
        newscale = (viewbox.y1 - viewbox.y0) / (y1 - y0);
    }

    newscale = CLAMP (newscale, SP_DESKTOP_ZOOM_MIN, SP_DESKTOP_ZOOM_MAX);

    int clear = FALSE;
    if (!NR_DF_TEST_CLOSE (newscale, scale, 1e-4 * scale)) {
        /* Set zoom factors */
        dt->d2w = NR::Matrix(NR::scale(newscale, -newscale));
        dt->w2d = NR::Matrix(NR::scale(1/newscale, 1/-newscale));
        sp_canvas_item_affine_absolute (SP_CANVAS_ITEM (dt->main), dt->d2w);
        clear = TRUE;
    }

    /* Calculate top left corner */
    x0 = cx - 0.5 * (viewbox.x1 - viewbox.x0) / newscale;
    y1 = cy + 0.5 * (viewbox.y1 - viewbox.y0) / newscale;

    /* Scroll */
    sp_canvas_scroll_to (dtw->canvas, x0 * newscale - border, y1 * -newscale - border, clear);

    sp_desktop_widget_update_rulers (dtw);
    sp_desktop_update_scrollbars (dt);
    sp_desktop_widget_update_zoom (dtw);
}

void
sp_desktop_prev_zoom (SPDesktop *dt)
{
    if (dt->zooms_past == NULL || dt->zooms_past->next == NULL) {
        sp_view_set_statusf_flash (SP_VIEW(dt), _("No previous zoom."));
        return;
    }

    // restore previous zoom
    sp_desktop_set_display_area (dt, 
                                 ((NRRect *) dt->zooms_past->next->data)->x0,
                                 ((NRRect *) dt->zooms_past->next->data)->y0, 
                                 ((NRRect *) dt->zooms_past->next->data)->x1, 
                                 ((NRRect *) dt->zooms_past->next->data)->y1, 
                                 0);

    // having stepped backwards, we can now go forward
    dt->can_go_forward = TRUE;

    // remove the just-added zoom from the list
    if (dt->zooms_past)
        dt->zooms_past = g_list_remove (dt->zooms_past, ((NRRect *) dt->zooms_past->data));
    // remove the current zoom, but save it in the zooms_future list
    if (dt->zooms_past) {
        dt->zooms_future = g_list_prepend (dt->zooms_future, ((NRRect *) dt->zooms_past->data));
        dt->zooms_past = g_list_remove (dt->zooms_past, ((NRRect *) dt->zooms_past->data));
    }
}

void
sp_desktop_next_zoom (SPDesktop *dt)
{
    if (dt->zooms_future == NULL || dt->can_go_forward == FALSE) {
        sp_view_set_statusf_flash (SP_VIEW(dt), _("No next zoom."));
        return;
    }

    // restore next zoom
    sp_desktop_set_display_area (dt, 
                                 ((NRRect *) dt->zooms_future->data)->x0,
                                 ((NRRect *) dt->zooms_future->data)->y0, 
                                 ((NRRect *) dt->zooms_future->data)->x1, 
                                 ((NRRect *) dt->zooms_future->data)->y1, 
                                 0);

    // we still can go forward
    dt->can_go_forward = TRUE;

    // remove the just-used zoom from the zooms_future list
    dt->zooms_future = g_list_remove (dt->zooms_future, ((NRRect *) dt->zooms_future->data));
}

NRRect *
sp_desktop_get_display_area (SPDesktop *dt, NRRect *area)
{
    NRRect viewbox;

    SPDesktopWidget *dtw = (SPDesktopWidget*)g_object_get_data (G_OBJECT (dt), "widget");
    if (!dtw) return NULL;

    sp_canvas_get_viewbox (dtw->canvas, &viewbox);

    double scale = dt->d2w[0];

    area->x0 = viewbox.x0 / scale;
    area->y0 = viewbox.y1 / -scale;
    area->x1 = viewbox.x1 / scale;
    area->y1 = viewbox.y0 / -scale;

    return area;
}

void
sp_desktop_zoom_absolute_keep_point (SPDesktop *dt, double cx, double cy, double px, double py, double zoom)
{
    SPDesktopWidget *dtw = (SPDesktopWidget*)g_object_get_data (G_OBJECT (dt), "widget");
    if (!dtw) return;

    zoom = CLAMP (zoom, SP_DESKTOP_ZOOM_MIN, SP_DESKTOP_ZOOM_MAX);

    // maximum or minimum zoom reached, but there's no exact equality because of rounding errors;
    // this check prevents "sliding" when trying to zoom in at maximum zoom;
    // someone please fix calculations properly and remove this hack
    if (fabs(SP_DESKTOP_ZOOM (dt) - zoom) < 0.025 && (fabs(SP_DESKTOP_ZOOM_MAX - zoom) < 0.01 || fabs(SP_DESKTOP_ZOOM_MIN - zoom) < 0.01)) 
        return;

    NRRect viewbox;
    sp_canvas_get_viewbox (dtw->canvas, &viewbox);

    const double width2 = (viewbox.x1 - viewbox.x0) / zoom;
    const double height2 = (viewbox.y1 - viewbox.y0) / zoom;

    sp_desktop_set_display_area (dt, cx - px * width2, cy - py * height2, cx + (1 - px) * width2, cy + (1 - py) * height2, 0.0);
}

void
sp_desktop_zoom_absolute (SPDesktop *dt, double cx, double cy, double zoom)
{
    sp_desktop_zoom_absolute_keep_point (dt, cx, cy, 0.5, 0.5, zoom);
}

void
sp_desktop_zoom_relative_keep_point (SPDesktop *dt, double cx, double cy, double zoom)
{
    NRRect area;
    sp_desktop_get_display_area (dt, &area);

    if (cx < area.x0)
        cx = area.x0;
    if (cx > area.x1)
        cx = area.x1;
    if (cy < area.y0)
        cy = area.y0;
    if (cy > area.y1)
        cy = area.y1;

    gdouble scale = SP_DESKTOP_ZOOM (dt) * zoom;
    double px = (cx - area.x0)/(area.x1 - area.x0);
    double py = (cy - area.y0)/(area.y1 - area.y0);

    sp_desktop_zoom_absolute_keep_point (dt, cx, cy, px, py, scale);
}

void
sp_desktop_zoom_relative (SPDesktop *dt, double cx, double cy, double zoom)
{
    gdouble scale = SP_DESKTOP_ZOOM (dt) * zoom;
    sp_desktop_zoom_absolute (dt, cx, cy, scale);
}

void
sp_desktop_zoom_page (SPDesktop *dt)
{
    NRRect d;

    d.x0 = d.y0 = 0.0;
    d.x1 = sp_document_width (SP_DT_DOCUMENT (dt));
    d.y1 = sp_document_height (SP_DT_DOCUMENT (dt));

    if ((fabs (d.x1 - d.x0) < 1.0) || (fabs (d.y1 - d.y0) < 1.0)) return;

    sp_desktop_set_display_area (dt, d.x0, d.y0, d.x1, d.y1, 10);
}

void
sp_desktop_zoom_page_width (SPDesktop *dt)
{
    NRRect d;

    sp_desktop_get_display_area (dt, &d);

    d.x0 = 0.0;
    d.x1 = sp_document_width (SP_DT_DOCUMENT (dt));

    if ((fabs (d.x1 - d.x0) < 1.0)) return;

    d.y1 = d.y0 = (d.y1 + d.y0) / 2;

    sp_desktop_set_display_area (dt, d.x0, d.y0, d.x1, d.y1, 10);
}

void
sp_desktop_zoom_selection (SPDesktop *dt)
{
    SPSelection *selection = SP_DT_SELECTION (dt);
    g_return_if_fail (selection != NULL);

    NRRect d;
    sp_selection_bbox (selection, &d);
    if ((fabs (d.x1 - d.x0) < 0.1) || (fabs (d.y1 - d.y0) < 0.1)) return;
    sp_desktop_set_display_area (dt, d.x0, d.y0, d.x1, d.y1, 10);
}

void
sp_desktop_zoom_drawing (SPDesktop *dt)
{
    SPDocument *doc = SP_VIEW_DOCUMENT (SP_VIEW (dt));
    g_return_if_fail (doc != NULL);
    SPItem *docitem = SP_ITEM (sp_document_root (doc));
    g_return_if_fail (docitem != NULL);

    NRRect d;
    sp_item_bbox_desktop (docitem, &d);
    if ((fabs (d.x1 - d.x0) < 1.0) || (fabs (d.y1 - d.y0) < 1.0)) return;
    sp_desktop_set_display_area (dt, d.x0, d.y0, d.x1, d.y1, 10);
}

void sp_desktop_scroll_world(SPDesktop *dt, double dx, double dy)
{
    SPDesktopWidget *dtw = (SPDesktopWidget*)g_object_get_data (G_OBJECT (dt), "widget");
    if (!dtw) return;

    NRRect viewbox;
    sp_canvas_get_viewbox (dtw->canvas, &viewbox);

    sp_canvas_scroll_to (dtw->canvas, viewbox.x0 - dx, viewbox.y0 - dy, FALSE);

    sp_desktop_widget_update_rulers (dtw);
    sp_desktop_update_scrollbars (dt);
}

bool
sp_desktop_scroll_to_point (SPDesktop *desktop, NR::Point const *p, gdouble autoscrollspeed)
{
	NRRect dbox;
	sp_desktop_get_display_area (desktop, &dbox);

	gdouble autoscrolldistance = (gdouble) prefs_get_int_attribute_limited ("options.autoscrolldistance", "value", 0, -1000, 10000);

	// autoscrolldistance is in screen pixels, but the display area is in document units
	autoscrolldistance /= SP_DESKTOP_ZOOM (desktop);

	// FIXME: njh: we need an expandBy function for rects
	dbox.x0 -= autoscrolldistance;
	dbox.x1 += autoscrolldistance;
	dbox.y0 -= autoscrolldistance;
	dbox.y1 += autoscrolldistance;

	if (!((*p)[NR::X] > dbox.x0 && (*p)[NR::X] < dbox.x1) || !((*p)[NR::Y] > dbox.y0 && (*p)[NR::Y] < dbox.y1)) {

		NR::Point const s_w( (*p) * desktop->d2w );

		gdouble x_to;
		if ((*p)[NR::X] < dbox.x0)
			x_to = dbox.x0;
		else if ((*p)[NR::X] > dbox.x1)
			x_to = dbox.x1;
		else 
			x_to = (*p)[NR::X];

		gdouble y_to;
		if ((*p)[NR::Y] < dbox.y0)
			y_to = dbox.y0;
		else if ((*p)[NR::Y] > dbox.y1)
			y_to = dbox.y1;
		else 
			y_to = (*p)[NR::Y];

		NR::Point const d_dt(x_to, y_to);
		NR::Point const d_w( d_dt * desktop->d2w );
		NR::Point const moved_w( d_w - s_w );

		if (autoscrollspeed == 0)
			autoscrollspeed = prefs_get_double_attribute_limited ("options.autoscrollspeed", "value", 1, 0, 10);

		if (autoscrollspeed != 0)
			sp_desktop_scroll_world(desktop, autoscrollspeed * moved_w);

		return true;
	}
	return false;
}

static void
sp_desktop_widget_update_rulers (SPDesktopWidget *dtw)
{
    NRRect viewbox;
    sp_canvas_get_viewbox (dtw->canvas, &viewbox);
    double scale = SP_DESKTOP_ZOOM (dtw->desktop);
    double s = viewbox.x0 / scale - dtw->ruler_origin[NR::X];
    double e = viewbox.x1 / scale - dtw->ruler_origin[NR::X];
    gtk_ruler_set_range (GTK_RULER (dtw->hruler), dtw->dt2r * s, dtw->dt2r * e, GTK_RULER (dtw->hruler)->position, dtw->dt2r * (e - s));
    s = viewbox.y0 / -scale - dtw->ruler_origin[NR::Y];
    e = viewbox.y1 / -scale - dtw->ruler_origin[NR::Y];
    gtk_ruler_set_range (GTK_RULER (dtw->vruler), dtw->dt2r * s, dtw->dt2r * e, GTK_RULER (dtw->vruler)->position, dtw->dt2r * (e - s));
}

static void
set_adjustment (GtkAdjustment *adj, double l, double u, double ps, double si, double pi)
{
    if ((l != adj->lower) ||
        (u != adj->upper) ||
        (ps != adj->page_size) ||
        (si != adj->step_increment) ||
        (pi != adj->page_increment)) {
        adj->lower = l;
        adj->upper = u;
        adj->page_size = ps;
        adj->step_increment = si;
        adj->page_increment = pi;
        gtk_adjustment_changed (adj);
    }
}

static void
sp_desktop_update_scrollbars (SPDesktop *dt)
{
    SPDesktopWidget *dtw = (SPDesktopWidget*)g_object_get_data (G_OBJECT (dt), "widget");
    if (!dtw) return;

    if (dtw->update) return;
    dtw->update = 1;

    SPDocument *doc = SP_VIEW_DOCUMENT (dt);
    double scale = SP_DESKTOP_ZOOM (dt);

    /* The desktop region we always show unconditionally */
    NRRect darea;
    sp_item_bbox_desktop (SP_ITEM (SP_DOCUMENT_ROOT (doc)), &darea);
    darea.x0 = MIN (darea.x0, -sp_document_width (doc));
    darea.y0 = MIN (darea.y0, -sp_document_height (doc));
    darea.x1 = MAX (darea.x1, 2 * sp_document_width (doc));
    darea.y1 = MAX (darea.y1, 2 * sp_document_height (doc));

    /* Canvas region we always show unconditionally */
    NRRect carea;
    carea.x0 = darea.x0 * scale - 64;
    carea.y0 = darea.y1 * -scale - 64;
    carea.x1 = darea.x1 * scale + 64;
    carea.y1 = darea.y0 * -scale + 64;

    NRRect viewbox;
    sp_canvas_get_viewbox (dtw->canvas, &viewbox);

    /* Viewbox is always included into scrollable region */
    nr_rect_d_union (&carea, &carea, &viewbox);

    set_adjustment (dtw->hadj, carea.x0, carea.x1,
                    (viewbox.x1 - viewbox.x0),
                    0.1 * (viewbox.x1 - viewbox.x0),
                    (viewbox.x1 - viewbox.x0));
    gtk_adjustment_set_value (dtw->hadj, viewbox.x0);

    set_adjustment (dtw->vadj, carea.y0, carea.y1,
                    (viewbox.y1 - viewbox.y0),
                    0.1 * (viewbox.y1 - viewbox.y0),
                    (viewbox.y1 - viewbox.y0));
    gtk_adjustment_set_value (dtw->vadj, viewbox.y0);

    dtw->update = 0;
}

static void
sp_desktop_widget_update_zoom (SPDesktopWidget *dtw)
{
    g_signal_handlers_block_by_func (G_OBJECT (dtw->zoom_status), (gpointer)G_CALLBACK (sp_dtw_zoom_value_changed), dtw);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (dtw->zoom_status), log(SP_DESKTOP_ZOOM(dtw->desktop)) / log(2));
    g_signal_handlers_unblock_by_func (G_OBJECT (dtw->zoom_status), (gpointer)G_CALLBACK (sp_dtw_zoom_value_changed), dtw);
}

gdouble
sp_dtw_zoom_value_to_display (gdouble value)
{
    return floor (pow (2, value) * 100.0 + 0.5);
}

gdouble
sp_dtw_zoom_display_to_value (gdouble value)
{
    return  log (value / 100.0) / log (2);
}

gint
sp_dtw_zoom_input (GtkSpinButton *spin, gdouble *new_val, gpointer data)
{
    gdouble new_scrolled = gtk_spin_button_get_value (spin);
    const gchar *b = gtk_entry_get_text (GTK_ENTRY (spin));
    gdouble new_typed = atof (b);
    
    if (sp_dtw_zoom_value_to_display (new_scrolled) == new_typed) { // the new value is set by scrolling
        *new_val = new_scrolled;
    } else { // the new value is typed in
        *new_val = sp_dtw_zoom_display_to_value (new_typed);
    }
    
    spinbutton_defocus (GTK_OBJECT (spin));
    return TRUE;
}

gboolean
sp_dtw_zoom_output (GtkSpinButton *spin, gpointer data)
{
    gchar b[64];
    g_snprintf (b, 64, "%4.0f%%", sp_dtw_zoom_value_to_display (gtk_spin_button_get_value (spin)));
    gtk_entry_set_text (GTK_ENTRY (spin), b);
    return TRUE;
}

void
sp_dtw_zoom_value_changed (GtkSpinButton *spin, gpointer data)
{
    NRRect d;
    double zoom_factor = pow (2, gtk_spin_button_get_value (spin));

    SPDesktopWidget *dtw = SP_DESKTOP_WIDGET (data);
    SPDesktop *desktop = dtw->desktop;

    sp_desktop_get_display_area (desktop, &d);
    g_signal_handler_block (spin, dtw->zoom_update);
    sp_desktop_zoom_absolute (desktop, (d.x0 + d.x1) / 2, (d.y0 + d.y1) / 2, zoom_factor);
    g_signal_handler_unblock (spin, dtw->zoom_update);
}

void
sp_dtw_zoom_populate_popup (GtkEntry *entry, GtkMenu *menu, gpointer data)
{
    GList *children, *iter;
    GtkWidget *item;
    SPDesktop *dt = SP_DESKTOP_WIDGET (data)->desktop;

    children = gtk_container_get_children (GTK_CONTAINER (menu));
    for ( iter = children ; iter ; iter = g_list_next (iter)) {
        gtk_container_remove (GTK_CONTAINER (menu), GTK_WIDGET (iter->data));
    }
    g_list_free (children);

    item = gtk_menu_item_new_with_label ("200%");
    g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (sp_dtw_zoom_200), dt);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    item = gtk_menu_item_new_with_label ("100%");
    g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (sp_dtw_zoom_100), dt);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    item = gtk_menu_item_new_with_label ("50%");
    g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (sp_dtw_zoom_50), dt);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    item = gtk_separator_menu_item_new ();
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    item = gtk_menu_item_new_with_label (_("Page"));
    g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (sp_dtw_zoom_page), dt);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    item = gtk_menu_item_new_with_label (_("Drawing"));
    g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (sp_dtw_zoom_drawing), dt);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    item = gtk_menu_item_new_with_label (_("Selection"));
    g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (sp_dtw_zoom_selection), dt);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

void
sp_dtw_zoom_menu_handler (SPDesktop *dt, gdouble factor)
{
    NRRect d;

    sp_desktop_get_display_area (dt, &d);
    sp_desktop_zoom_absolute (dt, ( d.x0 + d.x1 ) / 2, ( d.y0 + d.y1 ) / 2, factor);
}

void
sp_dtw_zoom_50 (GtkMenuItem *item, gpointer data)
{
    sp_dtw_zoom_menu_handler (SP_DESKTOP (data), 0.5);
}

void
sp_dtw_zoom_100 (GtkMenuItem *item, gpointer data)
{
    sp_dtw_zoom_menu_handler (SP_DESKTOP (data), 1.0);
}

void
sp_dtw_zoom_200 (GtkMenuItem *item, gpointer data)
{
    sp_dtw_zoom_menu_handler (SP_DESKTOP (data), 2.0);
}

void
sp_dtw_zoom_page (GtkMenuItem *item, gpointer data)
{
    sp_desktop_zoom_page (SP_DESKTOP (data));
}

void
sp_dtw_zoom_drawing (GtkMenuItem *item, gpointer data)
{
    sp_desktop_zoom_drawing (SP_DESKTOP (data));
}

void
sp_dtw_zoom_selection (GtkMenuItem *item, gpointer data)
{
    sp_desktop_zoom_selection (SP_DESKTOP (data));
}

#ifdef HAVE_GTK_WINDOW_FULLSCREEN
void
fullscreen(SPDesktop *dt)
{
    GtkWindow *topw = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(dt->owner->canvas)));
    if (GTK_IS_WINDOW(topw)) {
        if (dt->is_fullscreen) {
            dt->is_fullscreen = FALSE;
            gtk_window_unfullscreen(topw);        
        } else {
            dt->is_fullscreen = TRUE;
            gtk_window_fullscreen(topw);
        }
    }
}
#endif /* HAVE_GTK_WINDOW_FULLSCREEN */
