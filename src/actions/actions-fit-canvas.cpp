#include <iostream>

#include <giomm.h>  // Not <gtkmm.h>! To eventually allow a headless version!
#include <glibmm/i18n.h>

#include "actions-fit-canvas.h"
#include "inkscape-application.h"
#include "inkscape-window.h"
#include "desktop.h"
#include "selection-chemistry.h"

void 
fit_canvas_to_selection(InkscapeWindow* win)
{
    SPDesktop* dt = win->get_desktop();
    dt->selection->fitCanvas(true);
}

void 
fit_canvas_drawing(InkscapeWindow* win)
{
    SPDesktop* dt = win->get_desktop();
    if (fit_canvas_to_drawing(dt->getDocument())) {
        Inkscape::DocumentUndo::done(dt->getDocument(), _("Fit Page to Drawing"), nullptr);
    }
}

void 
canvas_to_selection_or_drawing(InkscapeWindow* win)
{
    SPDesktop* dt = win->get_desktop();
    fit_canvas_to_selection_or_drawing(dt);
}

std::vector<std::vector<Glib::ustring>> raw_fit_canvas_data =
{
    // clang-format off
    {"win.fit-canvas-to-selection",                     N_("Fit Page to Selection"),        "Selection Desktop",  N_("Fit the page to the current selection")},
    {"win.fit-canvas-to-drawing",                       N_("Fit Page to Drawing"),          "Selection Desktop",  N_("Fit the page to the drawing")},
    {"win.fit-canvas-to-selection-or-drawing",          N_("_Resize Page to Selection"),    "Selection Desktop",  N_("Fit the page to the current selection or the drawing if there is no selection")}
    // clang-format on
};

void
add_actions_fit_canvas(InkscapeWindow* win)
{
    // clang-format off
    win->add_action( "fit-canvas-to-selection",                 sigc::bind<InkscapeWindow*>(sigc::ptr_fun(&fit_canvas_to_selection), win));
    win->add_action( "fit-canvas-to-drawing",                   sigc::bind<InkscapeWindow*>(sigc::ptr_fun(&fit_canvas_drawing), win));
    win->add_action( "fit-canvas-to-selection-or-drawing",      sigc::bind<InkscapeWindow*>(sigc::ptr_fun(&canvas_to_selection_or_drawing), win));
    // clang-format on

    auto app = InkscapeApplication::instance();
    if (!app) {
        std::cerr << "add_actions_fit_canvas: no app!" << std::endl;
        return;
    }
    app->get_action_extra_data().add_data(raw_fit_canvas_data);
}