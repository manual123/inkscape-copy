/**
 * \brief Generic Panel widget - A generic dockable container.
 *
 * Authors:
 *   Bryce Harrington <bryce@bryceharrington.org>
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 2004 Bryce Harrington
 * Copyright (C) 2005 Jon A. Cruz
 *
 * Released under GNU GPL.  Read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_UI_WIDGET_PANEL_H
#define SEEN_INKSCAPE_UI_WIDGET_PANEL_H

#include <gtkmm/box.h>
#include <gtkmm/frame.h>
#include <gtkmm/table.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/menu.h>
#include <gtkmm/optionmenu.h>
#include "../previewfillable.h"

namespace Inkscape {
namespace UI {
namespace Widget {

class Panel : public Gtk::VBox
{
public:
    Panel();
    virtual ~Panel();
    Panel(Glib::ustring const &label);

    void setLabel(Glib::ustring const &label);
    Glib::ustring const &getLabel() const;

protected:
    void setTargetFillable( PreviewFillable *target );


private:
    void init();
    void bounceCall(int i, int j);


    Glib::ustring   label;

    Gtk::HBox       topBar;
    Gtk::Label      tabTitle;
    Gtk::OptionMenu tabButton;
    Gtk::Menu       menu;
    Gtk::Button     closeButton;
    PreviewFillable *_fillable;
};

} // namespace Widget
} // namespace UI
} // namespace Inkscape

#endif // SEEN_INKSCAPE_UI_WIDGET_PANEL_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=c++:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
