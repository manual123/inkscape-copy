#ifndef __SP_SVG_VIEW_H__
#define __SP_SVG_VIEW_H__

/** \file
 * SPSVGView, SPSVGViewWidget: Generic SVG view and widget
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * Copyright (C) 2001-2002 Lauris Kaplinski
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

class SPSVGView;
class SPSVGViewClass;

#define SP_TYPE_SVG_VIEW (sp_svg_view_get_type ())
#define SP_SVG_VIEW(obj) (GTK_CHECK_CAST ((obj), SP_TYPE_SVG_VIEW, SPSVGView))
#define SP_SVG_VIEW_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), SP_TYPE_SVG_VIEW, SPSVGViewClass))
#define SP_IS_SVG_VIEW(obj) (GTK_CHECK_TYPE ((obj), SP_TYPE_SVG_VIEW))
#define SP_IS_SVG_VIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), SP_TYPE_SVG_VIEW))

#include "display/display-forward.h"
#include "ui/view/view.h"
#include "ui/view/view-widget.h"


void sp_svg_view_set_scale (SPSVGView *view, gdouble hscale, gdouble vscale);
void sp_svg_view_set_rescale (SPSVGView *view, bool rescale, bool keepaspect, gdouble width, gdouble height);

/**
 * Generic SVG view.
 */
struct SPSVGView : public Inkscape::UI::View::View {
    public:
	unsigned int dkey;

	SPCanvasGroup *parent;
	SPCanvasItem *drawing;

	/// Horizontal and vertical scale
	gdouble hscale, vscale;
	/// Whether to rescale automatically
	bool rescale, keepaspect;
	gdouble width, height;

    // C++ Wrappers to functions
    /// Rescales SPSVGView to given proportions.
    void setScale(gdouble hscale, gdouble vscale) {
	sp_svg_view_set_scale(this, hscale, vscale);
    }

    /// Rescales SPSVGView and keeps aspect ratio.
    void setRescale(bool rescale, bool keepAspect, gdouble width, gdouble height) {
	sp_svg_view_set_rescale(this, rescale, keepAspect, width, height);
    }

};

/// The SPSVGView vtable.
struct SPSVGViewClass {
	SPViewClass parent_class;
};

GtkType sp_svg_view_get_type (void);

Inkscape::UI::View::View *sp_svg_view_new (SPCanvasGroup *parent);

/* SPSVGViewWidget */

class SPSVGViewWidget;
class SPSVGViewWidgetClass;

#define SP_TYPE_SVG_VIEW_WIDGET (sp_svg_view_widget_get_type ())
#define SP_SVG_VIEW_WIDGET(obj) (GTK_CHECK_CAST ((obj), SP_TYPE_SVG_VIEW_WIDGET, SPSVGViewWidget))
#define SP_SVG_VIEW_WIDGET_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), SP_TYPE_SVG_VIEW_WIDGET, SPSVGViewWidgetClass))
#define SP_IS_SVG_VIEW_WIDGET(obj) (GTK_CHECK_TYPE ((obj), SP_TYPE_SVG_VIEW_WIDGET))
#define SP_IS_SVG_VIEW_WIDGET_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), SP_TYPE_SVG_VIEW_WIDGET))

void sp_svg_view_widget_set_resize (SPSVGViewWidget *vw, bool resize, gdouble width, gdouble height);

/**
 * An SPSVGViewWidget is an SVG view together with a canvas.
 */
struct SPSVGViewWidget {
    public:
	SPViewWidget widget;

	GtkWidget *sw;
	GtkWidget *canvas;

	/// Whether to resize automatically
	bool resize;
	gdouble maxwidth, maxheight;

    // C++ Wrappers
    /// Flags the SPSVGViewWidget to have its size changed with Gtk.
    void setResize(bool resize, gdouble width, gdouble height) {
	sp_svg_view_widget_set_resize(this, resize, width, height);
    }
};

/// The SPSVGViewWidget vtable.
struct SPSVGViewWidgetClass {
	SPViewWidgetClass parent_class;
};

GtkType sp_svg_view_widget_get_type (void);

GtkWidget *sp_svg_view_widget_new (SPDocument *doc);


#endif
