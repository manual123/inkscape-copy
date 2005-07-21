/**
 * \brief Object Transformation dialog
 *
 * Author:
 *   Bryce W. Harrington <bryce@bryceharrington.org>
 *
 * Copyright (C) 2004 Bryce Harrington
 *
 * Released under GNU GPL.  Read the file 'COPYING' for more information.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <gtkmm/stock.h>

#include "document.h"
#include "desktop.h"
#include "desktop-handles.h"
#include "transformation.h"
#include "libnr/nr-rect.h"
#include "libnr/nr-scale-ops.h"
#include "selection.h"
#include "selection-chemistry.h"
#include "verbs.h"

namespace Inkscape {
namespace UI {
namespace Dialog {

/*########################################################################
# C O N S T R U C T O R
########################################################################*/

/**
 * Constructor for Transformation.  This does the initialization
 * and layout of the dialog used for transforming SVG objects.  It
 * consists of four pages for the four operations it handles:
 * 'Move' allows x,y translation of SVG objects
 * 'Scale' allows linear resizing of SVG objects
 * 'Rotate' allows rotating SVG objects by a degree
 * 'Skew' allows skewing SVG objects
 * 'Transform' allows applying a generic affine transform on SVG objects,
 *     with the user specifying the 6 degrees of freedom manually.
 *
 * The dialog is implemented as a Gtk::Notebook with five pages.
 * The pages are implemented using Inkscape's NotebookPage which
 * is used to help make sure all of Inkscape's notebooks follow
 * the same style.  We then populate the pages with our widgets,
 * we use the ScalarUnit class for this.
 *
 */
Transformation::Transformation()
    : Dialog ("dialogs.transformation", SP_VERB_DIALOG_TRANSFORM),
      _page_move              ("Move",      4, 2),
      _page_scale             ("Scale",     4, 2),
      _page_rotate            ("Rotate",    4, 2),
      _page_skew              ("Skew",      4, 2),
      _page_transform         ("Transform", 3, 3),
      _scalar_move_horizontal ("Horizontal", UNIT_TYPE_LINEAR, "",
                               "arrows_hor", &_units_move),
      _scalar_move_vertical   ("Vertical",   UNIT_TYPE_LINEAR, "",
                               "arrows_ver", &_units_move),
      _scalar_scale_horizontal("Width",      UNIT_TYPE_DIMENSIONLESS, "",
                               "transform_scale_hor", &_units_scale),
      _scalar_scale_vertical  ("Height",     UNIT_TYPE_DIMENSIONLESS, "",
                               "transform_scale_ver", &_units_scale),
      _scalar_rotate          ("Rotate",     UNIT_TYPE_RADIAL, "",
                               "transform_rotate"),
      _scalar_skew_horizontal ("Horizontal", UNIT_TYPE_LINEAR, "",
                               "arrows_hor", &_units_skew),
      _scalar_skew_vertical   ("Vertical",   UNIT_TYPE_LINEAR, "",
                               "arrows_ver", &_units_skew),

      _scalar_transform_a     ("A"),
      _scalar_transform_b     ("B"),
      _scalar_transform_c     ("C"),
      _scalar_transform_d     ("D"),
      _scalar_transform_e     ("E"),
      _scalar_transform_f     ("F"),

      _check_move_relative    ("Relative move")
{
    // Top level vbox
    Gtk::VBox *vbox = get_vbox();
    vbox->set_spacing(4);

    // Notebook for individual transformations
    vbox->pack_start(_notebook, true, true);

    _notebook.append_page(_page_move, _("Move"));
    layoutPageMove();

    _notebook.append_page(_page_scale, _("Scale"));
    layoutPageScale();

    _notebook.append_page(_page_rotate, _("Rotate"));
    layoutPageRotate();

    _notebook.append_page(_page_skew, _("Skew"));
    layoutPageSkew();

    _notebook.append_page(_page_transform, _("Transform"));
    layoutPageTransform();

    updateSelection(PAGE_MOVE, _getSelection());

    applyButton = add_button(Gtk::Stock::APPLY,   Gtk::RESPONSE_APPLY);
    if (applyButton)
        {
        tooltips.set_tip((*applyButton), _("Apply transform to object"));
        applyButton->set_sensitive(false);
        }

    show_all_children();
}

Transformation::~Transformation()
{
}




/*########################################################################
# U T I L I T Y
########################################################################*/

void
Transformation::present(Transformation::PageType page)
{
    _notebook.set_current_page(page);
    Gtk::Dialog::show();
    Gtk::Dialog::present();
}




/*########################################################################
# S E T U P   L A Y O U T
########################################################################*/


void
Transformation::layoutPageMove()
{
    _units_move.setUnitType(UNIT_TYPE_LINEAR);
    _scalar_move_horizontal.initScalar(-1e6, 1e6);
    _scalar_move_horizontal.setDigits(3);
    _scalar_move_horizontal.setIncrements(0.1, 1.0);

    _scalar_move_vertical.initScalar(-1e6, 1e6);
    _scalar_move_vertical.setDigits(3);
    _scalar_move_vertical.setIncrements(0.1, 1.0);

    //_scalar_move_vertical.set_label_image( INKSCAPE_STOCK_ARROWS_HOR );
    _page_move.table()
        .attach(_scalar_move_horizontal, 0, 2, 0, 1, Gtk::FILL, Gtk::SHRINK);

    _page_move.table()
        .attach(_units_move, 2, 3, 0, 1, Gtk::SHRINK, Gtk::SHRINK);

    _scalar_move_horizontal.signal_value_changed()
        .connect(sigc::mem_fun(*this, &Transformation::onMoveValueChanged));

    //_scalar_move_vertical.set_label_image( INKSCAPE_STOCK_ARROWS_VER );
    _page_move.table()
        .attach(_scalar_move_vertical, 0, 2, 1, 2, Gtk::FILL, Gtk::SHRINK);

    _scalar_move_vertical.signal_value_changed()
        .connect(sigc::mem_fun(*this, &Transformation::onMoveValueChanged));

    // Relative moves
    _page_move.table()
        .attach(_check_move_relative, 1, 2, 2, 3, Gtk::FILL, Gtk::SHRINK);
    _check_move_relative.set_active(true);
    _check_move_relative.signal_toggled()
        .connect(sigc::mem_fun(*this, &Transformation::onMoveRelativeToggled));
}

void
Transformation::layoutPageScale()
{

    _units_scale.setUnitType(UNIT_TYPE_DIMENSIONLESS);

    _scalar_scale_horizontal.initScalar(0, 1e6);
    _scalar_scale_horizontal.setValue(100.0, "%");
    _scalar_scale_horizontal.setDigits(3);
    _scalar_scale_horizontal.setIncrements(0.1, 1.0);

    _scalar_scale_vertical.initScalar(0, 1e6);
    _scalar_scale_vertical.setValue(100.0, "%");
    _scalar_scale_vertical.setDigits(3);
    _scalar_scale_vertical.setIncrements(0.1, 1.0);

    _page_scale.table()
        .attach(_scalar_scale_horizontal, 0, 2, 0, 1, Gtk::FILL, Gtk::SHRINK);
    _scalar_scale_horizontal.signal_value_changed()
        .connect(sigc::mem_fun(*this, &Transformation::onScaleValueChanged));

    _page_scale.table()
        .attach(_units_scale, 2, 3, 0, 1, Gtk::SHRINK, Gtk::SHRINK);

    _page_scale.table()
        .attach(_scalar_scale_vertical, 0, 2, 1, 2, Gtk::FILL, Gtk::SHRINK);
    _scalar_scale_vertical.signal_value_changed()
        .connect(sigc::mem_fun(*this, &Transformation::onScaleValueChanged));
}

void
Transformation::layoutPageRotate()
{
    _scalar_rotate.initScalar(-360.0, 360.0);
    _scalar_rotate.setDigits(3);
    _scalar_rotate.setIncrements(0.1, 1.0);

    _page_rotate.table()
        .attach(_scalar_rotate, 0, 2, 0, 1, Gtk::FILL, Gtk::SHRINK);

    _scalar_rotate.signal_value_changed()
        .connect(sigc::mem_fun(*this, &Transformation::onRotateValueChanged));
}

void
Transformation::layoutPageSkew()
{
    _units_skew.setUnitType(UNIT_TYPE_LINEAR);

    _scalar_skew_horizontal.initScalar(-1e6, 1e6);
    _scalar_skew_horizontal.setDigits(3);
    _scalar_skew_horizontal.setIncrements(0.1, 1.0);

    _scalar_skew_vertical.initScalar(-1e6, 1e6);
    _scalar_skew_vertical.setDigits(3);
    _scalar_skew_vertical.setIncrements(0.1, 1.0);

    _page_skew.table()
        .attach(_scalar_skew_horizontal, 0, 2, 0, 1, Gtk::FILL, Gtk::SHRINK);
    _scalar_skew_horizontal.signal_value_changed()
        .connect(sigc::mem_fun(*this, &Transformation::onSkewValueChanged));

    _page_skew.table()
        .attach(_units_skew, 2, 3, 0, 1, Gtk::SHRINK, Gtk::SHRINK);


    _page_skew.table()
        .attach(_scalar_skew_vertical, 0, 2, 1, 2, Gtk::FILL, Gtk::SHRINK);
    _scalar_skew_vertical.signal_value_changed()
        .connect(sigc::mem_fun(*this, &Transformation::onSkewValueChanged));
}



void
Transformation::layoutPageTransform()
{
    _scalar_transform_a.setWidgetSizeRequest(75, -1);
    _scalar_transform_a.setRange(-1e10, 1e10);
    _scalar_transform_a.setDigits(3);
    _scalar_transform_a.setIncrements(0.1, 1.0);
    _scalar_transform_a.setValue(1.0);
    _page_transform.table()
        .attach(_scalar_transform_a, 0, 1, 0, 1, Gtk::SHRINK, Gtk::SHRINK);
    _scalar_transform_a.signal_value_changed()
        .connect(sigc::mem_fun(*this, &Transformation::onTransformValueChanged));


    _scalar_transform_b.setWidgetSizeRequest(75, -1);
    _scalar_transform_b.setRange(-1e10, 1e10);
    _scalar_transform_b.setDigits(3);
    _scalar_transform_b.setIncrements(0.1, 1.0);
    _scalar_transform_b.setValue(0.0);
    _page_transform.table()
        .attach(_scalar_transform_b, 0, 1, 1, 2, Gtk::SHRINK, Gtk::SHRINK);
    _scalar_transform_b.signal_value_changed()
        .connect(sigc::mem_fun(*this, &Transformation::onTransformValueChanged));


    _scalar_transform_c.setWidgetSizeRequest(75, -1);
    _scalar_transform_c.setRange(-1e10, 1e10);
    _scalar_transform_c.setDigits(3);
    _scalar_transform_c.setIncrements(0.1, 1.0);
    _scalar_transform_c.setValue(0.0);
    _page_transform.table()
        .attach(_scalar_transform_c, 1, 2, 0, 1, Gtk::SHRINK, Gtk::SHRINK);
    _scalar_transform_c.signal_value_changed()
        .connect(sigc::mem_fun(*this, &Transformation::onTransformValueChanged));


    _scalar_transform_d.setWidgetSizeRequest(75, -1);
    _scalar_transform_d.setRange(-1e10, 1e10);
    _scalar_transform_d.setDigits(3);
    _scalar_transform_d.setIncrements(0.1, 1.0);
    _scalar_transform_d.setValue(1.0);
    _page_transform.table()
        .attach(_scalar_transform_d, 1, 2, 1, 2, Gtk::SHRINK, Gtk::SHRINK);
    _scalar_transform_d.signal_value_changed()
        .connect(sigc::mem_fun(*this, &Transformation::onTransformValueChanged));


    _scalar_transform_e.setWidgetSizeRequest(75, -1);
    _scalar_transform_e.setRange(-1e10, 1e10);
    _scalar_transform_e.setDigits(3);
    _scalar_transform_e.setIncrements(0.1, 1.0);
    _scalar_transform_e.setValue(0.0);
    _page_transform.table()
        .attach(_scalar_transform_e, 2, 3, 0, 1, Gtk::SHRINK, Gtk::SHRINK);
    _scalar_transform_e.signal_value_changed()
        .connect(sigc::mem_fun(*this, &Transformation::onTransformValueChanged));


    _scalar_transform_f.setWidgetSizeRequest(75, -1);
    _scalar_transform_f.setRange(-1e10, 1e10);
    _scalar_transform_f.setDigits(3);
    _scalar_transform_f.setIncrements(0.1, 1.0);
    _scalar_transform_f.setValue(0.0);
    _page_transform.table()
        .attach(_scalar_transform_f, 2, 3, 1, 2, Gtk::SHRINK, Gtk::SHRINK);
    _scalar_transform_f.signal_value_changed()
        .connect(sigc::mem_fun(*this, &Transformation::onTransformValueChanged));

    char *svgData =
        "<?xml version='1.0' encoding='UTF-8' standalone='no'?>"
        "<svg"
        "   xmlns:svg='http://www.w3.org/2000/svg' xmlns='http://www.w3.org/2000/svg'"
        "   height='25.0000' width='60.000'"
        "   y='0.00000000' x='0.00000000' version='1.0'>"
        "  <g transform='matrix(0.3,0.000000,0.000000,0.3,-16.0,-37.0)'>"
        "    <path"
        "       style='font-size:12.000000;font-style:normal;font-weight:normal;fill:#000000;fill-opacity:1.0000000;stroke:none;stroke-width:1.0000000px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1.0000000;font-family:BitstreamVeraSans'"
        "       d='M 69.340121,135.36498 L 69.340121,137.22826 L 71.560824,137.22826 L 71.560824,138.06615 L 69.340121,138.06615 L 69.340121,141.62865 C 69.340119,142.16381 69.412384,142.50756 69.556918,142.65990 C 69.705353,142.81225 70.004181,142.88842 70.453402,142.88842 L 71.560824,142.88842 L 71.560824,143.79076 L 70.453402,143.79076 C 69.621369,143.79076 69.047150,143.63647 68.730746,143.32787 C 68.414338,143.01537 68.256136,142.44897 68.256137,141.62865 L 68.256137,138.06615 L 67.465121,138.06615 L 67.465121,137.22826 L 68.256137,137.22826 L 68.256137,135.36498 L 69.340121,135.36498 M 76.787387,138.23608 C 76.666288,138.16577 76.533476,138.11499 76.388949,138.08373 C 76.248320,138.04858 76.092070,138.03100 75.920199,138.03100 C 75.310821,138.03100 74.842071,138.23022 74.513949,138.62865 C 74.189728,139.02319 74.027619,139.59155 74.027621,140.33373 L 74.027621,143.79076 L 72.943637,143.79076 L 72.943637,137.22826 L 74.027621,137.22826 L 74.027621,138.24779 C 74.254181,137.84936 74.549103,137.55444 74.912387,137.36303 C 75.275664,137.16772 75.717070,137.07007 76.236605,137.07006 C 76.310820,137.07007 76.392851,137.07593 76.482699,137.08764 C 76.572538,137.09546 76.672147,137.10913 76.781527,137.12865 L 76.787387,138.23608 M 80.888949,140.49194 C 80.017849,140.49194 79.414339,140.59155 79.078399,140.79076 C 78.742459,140.98998 78.574499,141.32983 78.574499,141.81029 C 78.574499,142.19311 78.699499,142.49780 78.949499,142.72436 C 79.203399,142.94701 79.547149,143.05834 79.980749,143.05834 C 80.578399,143.05834 81.056909,142.84741 81.416289,142.42553 C 81.779569,141.99975 81.961209,141.43530 81.961219,140.73217 L 81.961219,140.49194 L 80.888949,140.49194 M 83.039339,140.04662 L 83.039339,143.79076 L 81.961219,143.79076 L 81.961219,142.79467 C 81.715119,143.19311 81.408479,143.48803 81.041289,143.67944 C 80.674099,143.86694 80.224879,143.96069 79.693639,143.96069 C 79.021759,143.96069 78.486604,143.77319 78.088168,143.39819 C 77.693636,143.01928 77.496370,142.51342 77.496371,141.88061 C 77.496370,141.14233 77.742464,140.58569 78.234652,140.21069 C 78.730749,139.83569 79.469029,139.64819 80.449499,139.64819 L 81.961219,139.64819 L 81.961219,139.54272 C 81.961209,139.04663 81.797149,138.66382 81.469029,138.39428 C 81.144809,138.12085 80.687769,137.98413 80.097929,137.98412 C 79.722929,137.98413 79.357699,138.02905 79.002229,138.11889 C 78.646759,138.20874 78.304963,138.34350 77.976840,138.52319 L 77.976840,137.52709 C 78.371369,137.37475 78.754179,137.26147 79.125279,137.18725 C 79.496369,137.10913 79.857699,137.07007 80.209259,137.07006 C 81.158479,137.07007 81.867459,137.31616 82.336219,137.80834 C 82.804959,138.30053 83.039329,139.04663 83.039339,140.04662 M 90.744419,139.82983 L 90.744419,143.79076 L 89.666289,143.79076 L 89.666289,139.86498 C 89.666289,139.24389 89.545189,138.77905 89.303009,138.47045 C 89.060819,138.16186 88.697539,138.00757 88.213169,138.00756 C 87.631129,138.00757 87.172149,138.19311 86.836219,138.56420 C 86.500279,138.93530 86.332309,139.44116 86.332309,140.08178 L 86.332309,143.79076 L 85.248329,143.79076 L 85.248329,137.22826 L 86.332309,137.22826 L 86.332309,138.24779 C 86.590119,137.85327 86.892849,137.55835 87.240509,137.36303 C 87.592069,137.16772 87.996369,137.07007 88.453399,137.07006 C 89.207299,137.07007 89.777619,137.30444 90.164339,137.77319 C 90.551049,138.23803 90.744409,138.92358 90.744419,139.82983 M 97.090119,137.42162 L 97.090119,138.44115 C 96.785429,138.28491 96.469019,138.16772 96.140899,138.08959 C 95.812769,138.01147 95.472929,137.97241 95.121369,137.97240 C 94.586209,137.97241 94.183869,138.05444 93.914339,138.21850 C 93.648709,138.38257 93.515899,138.62866 93.515899,138.95678 C 93.515899,139.20678 93.611599,139.40405 93.803009,139.54858 C 93.994419,139.68921 94.379179,139.82397 94.957309,139.95287 L 95.326449,140.03490 C 96.092069,140.19897 96.635039,140.43139 96.955359,140.73217 C 97.279569,141.02905 97.441679,141.44506 97.441679,141.98022 C 97.441679,142.58959 97.199489,143.07201 96.715119,143.42748 C 96.234649,143.78295 95.572539,143.96069 94.728789,143.96069 C 94.377229,143.96069 94.010039,143.92553 93.627229,143.85522 C 93.248319,143.78881 92.847929,143.68725 92.426059,143.55053 L 92.426059,142.43725 C 92.824499,142.64428 93.217069,142.80053 93.603789,142.90600 C 93.990509,143.00756 94.373319,143.05834 94.752229,143.05834 C 95.260039,143.05834 95.650669,142.97240 95.924109,142.80053 C 96.197539,142.62475 96.334259,142.37866 96.334259,142.06225 C 96.334259,141.76928 96.234649,141.54467 96.035429,141.38842 C 95.840119,141.23217 95.408479,141.08178 94.740509,140.93725 L 94.365509,140.84936 C 93.697539,140.70874 93.215119,140.49389 92.918249,140.20483 C 92.621369,139.91186 92.472929,139.51147 92.472929,139.00365 C 92.472929,138.38647 92.691679,137.90991 93.129179,137.57397 C 93.566679,137.23804 94.187779,137.07007 94.992469,137.07006 C 95.390899,137.07007 95.765899,137.09936 96.117469,137.15795 C 96.469019,137.21655 96.793239,137.30444 97.090119,137.42162 M 102.46317,134.67358 L 102.46317,135.57006 L 101.43192,135.57006 C 101.04520,135.57007 100.77567,135.64819 100.62333,135.80444 C 100.47489,135.96069 100.40067,136.24194 100.40067,136.64819 L 100.40067,137.22826 L 102.17606,137.22826 L 102.17606,138.06615 L 100.40067,138.06615 L 100.40067,143.79076 L 99.316679,143.79076 L 99.316679,138.06615 L 98.285429,138.06615 L 98.285429,137.22826 L 99.316679,137.22826 L 99.316679,136.77123 C 99.316679,136.04077 99.486599,135.50952 99.826449,135.17748 C 100.16629,134.84155 100.70535,134.67359 101.44364,134.67358 L 102.46317,134.67358 M 105.90262,137.98412 C 105.32449,137.98413 104.86746,138.21069 104.53153,138.66381 C 104.19559,139.11303 104.02762,139.73022 104.02762,140.51537 C 104.02762,141.30053 104.19364,141.91967 104.52567,142.37279 C 104.86160,142.82201 105.32059,143.04662 105.90262,143.04662 C 106.47684,143.04662 106.93191,142.82006 107.26786,142.36694 C 107.60379,141.91381 107.77176,141.29663 107.77176,140.51537 C 107.77176,139.73803 107.60379,139.12280 107.26786,138.66967 C 106.93191,138.21264 106.47684,137.98413 105.90262,137.98412 M 105.90262,137.07006 C 106.84012,137.07007 107.57644,137.37475 108.11161,137.98412 C 108.64676,138.59350 108.91433,139.43725 108.91434,140.51537 C 108.91433,141.58959 108.64676,142.43334 108.11161,143.04662 C 107.57644,143.65600 106.84012,143.96069 105.90262,143.96069 C 104.96121,143.96069 104.22293,143.65600 103.68778,143.04662 C 103.15653,142.43334 102.89090,141.58959 102.89090,140.51537 C 102.89090,139.43725 103.15653,138.59350 103.68778,137.98412 C 104.22293,137.37475 104.96121,137.07007 105.90262,137.07006 M 114.49833,138.23608 C 114.37723,138.16577 114.24441,138.11499 114.09989,138.08373 C 113.95926,138.04858 113.80301,138.03100 113.63114,138.03100 C 113.02176,138.03100 112.55301,138.23022 112.22489,138.62865 C 111.90067,139.02319 111.73856,139.59155 111.73856,140.33373 L 111.73856,143.79076 L 110.65458,143.79076 L 110.65458,137.22826 L 111.73856,137.22826 L 111.73856,138.24779 C 111.96512,137.84936 112.26004,137.55444 112.62333,137.36303 C 112.98660,137.16772 113.42801,137.07007 113.94754,137.07006 C 114.02176,137.07007 114.10379,137.07593 114.19364,137.08764 C 114.28348,137.09546 114.38309,137.10913 114.49247,137.12865 L 114.49833,138.23608 M 120.51590,138.48803 C 120.78543,138.00366 121.10769,137.64624 121.48270,137.41576 C 121.85769,137.18530 122.29910,137.07007 122.80692,137.07006 C 123.49050,137.07007 124.01785,137.31030 124.38895,137.79076 C 124.76003,138.26733 124.94558,138.94702 124.94559,139.82983 L 124.94559,143.79076 L 123.86161,143.79076 L 123.86161,139.86498 C 123.86160,139.23608 123.75027,138.76928 123.52762,138.46459 C 123.30496,138.15991 122.96511,138.00757 122.50809,138.00756 C 121.94949,138.00757 121.50808,138.19311 121.18387,138.56420 C 120.85965,138.93530 120.69754,139.44116 120.69754,140.08178 L 120.69754,143.79076 L 119.61356,143.79076 L 119.61356,139.86498 C 119.61355,139.23217 119.50223,138.76538 119.27958,138.46459 C 119.05691,138.15991 118.71316,138.00757 118.24833,138.00756 C 117.69754,138.00757 117.26004,138.19507 116.93583,138.57006 C 116.61160,138.94116 116.44949,139.44506 116.44950,140.08178 L 116.44950,143.79076 L 115.36551,143.79076 L 115.36551,137.22826 L 116.44950,137.22826 L 116.44950,138.24779 C 116.69559,137.84546 116.99051,137.54858 117.33426,137.35717 C 117.67801,137.16577 118.08621,137.07007 118.55887,137.07006 C 119.03543,137.07007 119.43973,137.19116 119.77176,137.43334 C 120.10769,137.67554 120.35574,138.02710 120.51590,138.48803 M 129.66825,134.68529 C 129.14481,135.58374 128.75613,136.47241 128.50223,137.35131 C 128.24832,138.23022 128.12137,139.12085 128.12137,140.02319 C 128.12137,140.92553 128.24832,141.82006 128.50223,142.70678 C 128.76004,143.58959 129.14871,144.47826 129.66825,145.37279 L 128.73075,145.37279 C 128.14481,144.45483 127.70535,143.55248 127.41239,142.66576 C 127.12332,141.77905 126.97879,140.89819 126.97879,140.02319 C 126.97879,139.15210 127.12332,138.27514 127.41239,137.39233 C 127.70145,136.50952 128.14090,135.60718 128.73075,134.68529 L 129.66825,134.68529 M 134.73661,136.20873 L 133.13114,140.56225 L 136.34793,140.56225 L 134.73661,136.20873 M 134.06864,135.04272 L 135.41043,135.04272 L 138.74442,143.79076 L 137.51395,143.79076 L 136.71708,141.54662 L 132.77372,141.54662 L 131.97684,143.79076 L 130.72879,143.79076 L 134.06864,135.04272 M 140.24442,142.30248 L 141.48075,142.30248 L 141.48075,143.31029 L 140.51981,145.18529 L 139.76395,145.18529 L 140.24442,143.31029 L 140.24442,142.30248 M 148.84012,139.61303 L 148.84012,142.81811 L 150.73856,142.81811 C 151.37527,142.81811 151.84598,142.68725 152.15067,142.42553 C 152.45926,142.15991 152.61355,141.75561 152.61356,141.21264 C 152.61355,140.66577 152.45926,140.26342 152.15067,140.00561 C 151.84598,139.74389 151.37527,139.61303 150.73856,139.61303 L 148.84012,139.61303 M 148.84012,136.01537 L 148.84012,138.65209 L 150.59208,138.65209 C 151.17020,138.65210 151.59988,138.54467 151.88114,138.32983 C 152.16629,138.11108 152.30887,137.77905 152.30887,137.33373 C 152.30887,136.89233 152.16629,136.56226 151.88114,136.34350 C 151.59988,136.12476 151.17020,136.01538 150.59208,136.01537 L 148.84012,136.01537 M 147.65653,135.04272 L 150.67997,135.04272 C 151.58230,135.04273 152.27762,135.23023 152.76590,135.60522 C 153.25418,135.98022 153.49832,136.51343 153.49833,137.20483 C 153.49832,137.73999 153.37332,138.16577 153.12333,138.48217 C 152.87332,138.79858 152.50613,138.99585 152.02176,139.07397 C 152.60379,139.19897 153.05496,139.46069 153.37528,139.85912 C 153.69949,140.25366 153.86160,140.74780 153.86161,141.34154 C 153.86160,142.12280 153.59597,142.72631 153.06473,143.15209 C 152.53348,143.57787 151.77762,143.79076 150.79715,143.79076 L 147.65653,143.79076 L 147.65653,135.04272 M 156.11161,142.30248 L 157.34793,142.30248 L 157.34793,143.31029 L 156.38700,145.18529 L 155.63114,145.18529 L 156.11161,143.31029 L 156.11161,142.30248 M 170.07450,135.71654 L 170.07450,136.96459 C 169.67605,136.59351 169.25027,136.31616 168.79715,136.13256 C 168.34793,135.94897 167.86941,135.85718 167.36161,135.85717 C 166.36160,135.85718 165.59598,136.16382 165.06473,136.77709 C 164.53348,137.38647 164.26785,138.26928 164.26786,139.42553 C 164.26785,140.57788 164.53348,141.46069 165.06473,142.07397 C 165.59598,142.68334 166.36160,142.98803 167.36161,142.98803 C 167.86941,142.98803 168.34793,142.89623 168.79715,142.71264 C 169.25027,142.52905 169.67605,142.25170 170.07450,141.88061 L 170.07450,143.11694 C 169.66043,143.39819 169.22097,143.60912 168.75614,143.74975 C 168.29519,143.89037 167.80691,143.96069 167.29129,143.96069 C 165.96707,143.96069 164.92410,143.55639 164.16239,142.74779 C 163.40067,141.93530 163.01981,140.82788 163.01981,139.42553 C 163.01981,138.01928 163.40067,136.91186 164.16239,136.10326 C 164.92410,135.29077 165.96707,134.88452 167.29129,134.88451 C 167.81473,134.88452 168.30691,134.95483 168.76786,135.09545 C 169.23269,135.23218 169.66824,135.43921 170.07450,135.71654 M 172.14286,142.30248 L 173.37918,142.30248 L 173.37918,143.31029 L 172.41825,145.18529 L 171.66239,145.18529 L 172.14286,143.31029 L 172.14286,142.30248 M 180.73856,136.01537 L 180.73856,142.81811 L 182.16825,142.81811 C 183.37527,142.81811 184.25808,142.54467 184.81668,141.99779 C 185.37918,141.45092 185.66043,140.58764 185.66043,139.40795 C 185.66043,138.23608 185.37918,137.37866 184.81668,136.83569 C 184.25808,136.28882 183.37527,136.01538 182.16825,136.01537 L 180.73856,136.01537 M 179.55497,135.04272 L 181.98661,135.04272 C 183.68191,135.04273 184.92605,135.39624 185.71903,136.10326 C 186.51199,136.80640 186.90847,137.90796 186.90848,139.40795 C 186.90847,140.91577 186.51004,142.02319 185.71317,142.73022 C 184.91629,143.43725 183.67410,143.79076 181.98661,143.79076 L 179.55497,143.79076 L 179.55497,135.04272 M 189.01786,142.30248 L 190.25418,142.30248 L 190.25418,143.31029 L 189.29325,145.18529 L 188.53739,145.18529 L 189.01786,143.31029 L 189.01786,142.30248 M 196.42997,135.04272 L 201.96122,135.04272 L 201.96122,136.03881 L 197.61356,136.03881 L 197.61356,138.62865 L 201.77958,138.62865 L 201.77958,139.62475 L 197.61356,139.62475 L 197.61356,142.79467 L 202.06668,142.79467 L 202.06668,143.79076 L 196.42997,143.79076 L 196.42997,135.04272 M 204.25223,142.30248 L 205.48856,142.30248 L 205.48856,143.31029 L 204.52762,145.18529 L 203.77176,145.18529 L 204.25223,143.31029 L 204.25223,142.30248 M 211.66434,135.04272 L 216.69168,135.04272 L 216.69168,136.03881 L 212.84793,136.03881 L 212.84793,138.61694 L 216.31668,138.61694 L 216.31668,139.61303 L 212.84793,139.61303 L 212.84793,143.79076 L 211.66434,143.79076 L 211.66434,135.04272 M 218.36161,134.68529 L 219.29911,134.68529 C 219.88504,135.60718 220.32254,136.50952 220.61161,137.39233 C 220.90457,138.27514 221.05106,139.15210 221.05106,140.02319 C 221.05106,140.89819 220.90457,141.77905 220.61161,142.66576 C 220.32254,143.55248 219.88504,144.45483 219.29911,145.37279 L 218.36161,145.37279 C 218.88114,144.47826 219.26785,143.58959 219.52176,142.70678 C 219.77957,141.82006 219.90848,140.92553 219.90848,140.02319 C 219.90848,139.12085 219.77957,138.23022 219.52176,137.35131 C 219.26785,136.47241 218.88114,135.58374 218.36161,134.68529' />"
        "    <path"
        "       style='fill:none;fill-opacity:1.0000000;stroke:#00007e;stroke-width:1.2074658;stroke-miterlimit:4.0000000;stroke-dasharray:none;stroke-opacity:1.0000000'"
        "       d='M 124.99344,196.68088 L 113.98199,196.68088 L 113.98199,147.70445 L 124.99344,147.70445' />"
        "    <path"
        "       style='font-size:12.000000;font-style:normal;font-weight:normal;fill:#000000;fill-opacity:1.0000000;stroke:none;stroke-width:1.0000000px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1.0000000;font-family:BitstreamVeraSans'"
        "       d='M 123.08420,153.49897 L 121.47873,157.85249 L 124.69553,157.85249 L 123.08420,153.49897 M 122.41623,152.33295 L 123.75803,152.33295 L 127.09201,161.08100 L 125.86155,161.08100 L 125.06467,158.83686 L 121.12131,158.83686 L 120.32444,161.08100 L 119.07639,161.08100 L 122.41623,152.33295 M 146.37522,153.00678 L 146.37522,154.25483 C 145.97677,153.88374 145.55099,153.60640 145.09787,153.42280 C 144.64865,153.23921 144.17013,153.14742 143.66233,153.14741 C 142.66232,153.14742 141.89670,153.45406 141.36545,154.06733 C 140.83420,154.67671 140.56857,155.55952 140.56858,156.71577 C 140.56857,157.86811 140.83420,158.75093 141.36545,159.36420 C 141.89670,159.97358 142.66232,160.27827 143.66233,160.27827 C 144.17013,160.27827 144.64865,160.18647 145.09787,160.00288 C 145.55099,159.81928 145.97677,159.54194 146.37522,159.17084 L 146.37522,160.40717 C 145.96115,160.68842 145.52169,160.89936 145.05686,161.03999 C 144.59591,161.18061 144.10763,161.25092 143.59201,161.25092 C 142.26779,161.25092 141.22482,160.84663 140.46311,160.03803 C 139.70139,159.22553 139.32053,158.11811 139.32053,156.71577 C 139.32053,155.30952 139.70139,154.20210 140.46311,153.39350 C 141.22482,152.58101 142.26779,152.17476 143.59201,152.17475 C 144.11545,152.17476 144.60763,152.24507 145.06858,152.38569 C 145.53341,152.52242 145.96896,152.72945 146.37522,153.00678 M 159.67600,152.33295 L 165.20725,152.33295 L 165.20725,153.32905 L 160.85959,153.32905 L 160.85959,155.91889 L 165.02561,155.91889 L 165.02561,156.91499 L 160.85959,156.91499 L 160.85959,160.08491 L 165.31272,160.08491 L 165.31272,161.08100 L 159.67600,161.08100 L 159.67600,152.33295 M 121.34397,171.90327 L 121.34397,175.10834 L 123.24240,175.10834 C 123.87912,175.10835 124.34982,174.97749 124.65451,174.71577 C 124.96310,174.45014 125.11740,174.04585 125.11740,173.50288 C 125.11740,172.95600 124.96310,172.55366 124.65451,172.29584 C 124.34982,172.03413 123.87912,171.90327 123.24240,171.90327 L 121.34397,171.90327 M 121.34397,168.30561 L 121.34397,170.94233 L 123.09592,170.94233 C 123.67404,170.94233 124.10373,170.83491 124.38498,170.62006 C 124.67013,170.40132 124.81271,170.06929 124.81272,169.62397 C 124.81271,169.18257 124.67013,168.85249 124.38498,168.63374 C 124.10373,168.41499 123.67404,168.30562 123.09592,168.30561 L 121.34397,168.30561 M 120.16037,167.33295 L 123.18381,167.33295 C 124.08615,167.33296 124.78146,167.52046 125.26975,167.89545 C 125.75802,168.27046 126.00216,168.80366 126.00217,169.49506 C 126.00216,170.03023 125.87716,170.45601 125.62717,170.77241 C 125.37716,171.08882 125.00998,171.28608 124.52561,171.36420 C 125.10763,171.48921 125.55880,171.75093 125.87912,172.14936 C 126.20334,172.54390 126.36544,173.03804 126.36545,173.63178 C 126.36544,174.41303 126.09982,175.01655 125.56858,175.44233 C 125.03732,175.86811 124.28146,176.08100 123.30100,176.08100 L 120.16037,176.08100 L 120.16037,167.33295 M 141.03147,168.30561 L 141.03147,175.10834 L 142.46115,175.10834 C 143.66818,175.10835 144.55099,174.83491 145.10959,174.28803 C 145.67209,173.74116 145.95333,172.87788 145.95334,171.69819 C 145.95333,170.52632 145.67209,169.66890 145.10959,169.12592 C 144.55099,168.57906 143.66818,168.30562 142.46115,168.30561 L 141.03147,168.30561 M 139.84787,167.33295 L 142.27951,167.33295 C 143.97482,167.33296 145.21896,167.68648 146.01194,168.39350 C 146.80490,169.09663 147.20138,170.19819 147.20139,171.69819 C 147.20138,173.20600 146.80294,174.31342 146.00608,175.02045 C 145.20920,175.72749 143.96701,176.08100 142.27951,176.08100 L 139.84787,176.08100 L 139.84787,167.33295 M 160.54319,167.33295 L 165.57053,167.33295 L 165.57053,168.32905 L 161.72678,168.32905 L 161.72678,170.90717 L 165.19553,170.90717 L 165.19553,171.90327 L 161.72678,171.90327 L 161.72678,176.08100 L 160.54319,176.08100 L 160.54319,167.33295 M 122.79709,183.11225 C 122.18771,183.11226 121.72873,183.41304 121.42014,184.01459 C 121.11545,184.61226 120.96311,185.51265 120.96311,186.71577 C 120.96311,187.91499 121.11545,188.81538 121.42014,189.41694 C 121.72873,190.01460 122.18771,190.31342 122.79709,190.31342 C 123.41037,190.31342 123.86935,190.01460 124.17405,189.41694 C 124.48263,188.81538 124.63693,187.91499 124.63694,186.71577 C 124.63693,185.51265 124.48263,184.61226 124.17405,184.01459 C 123.86935,183.41304 123.41037,183.11226 122.79709,183.11225 M 122.79709,182.17475 C 123.77756,182.17476 124.52560,182.56343 125.04123,183.34077 C 125.56076,184.11421 125.82052,185.23921 125.82053,186.71577 C 125.82052,188.18843 125.56076,189.31342 125.04123,190.09077 C 124.52560,190.86420 123.77756,191.25092 122.79709,191.25092 C 121.81662,191.25092 121.06662,190.86420 120.54709,190.09077 C 120.03147,189.31342 119.77365,188.18843 119.77365,186.71577 C 119.77365,185.23921 120.03147,184.11421 120.54709,183.34077 C 121.06662,182.56343 121.81662,182.17476 122.79709,182.17475 M 141.89865,183.11225 C 141.28928,183.11226 140.83029,183.41304 140.52170,184.01459 C 140.21701,184.61226 140.06467,185.51265 140.06467,186.71577 C 140.06467,187.91499 140.21701,188.81538 140.52170,189.41694 C 140.83029,190.01460 141.28928,190.31342 141.89865,190.31342 C 142.51193,190.31342 142.97092,190.01460 143.27561,189.41694 C 143.58420,188.81538 143.73849,187.91499 143.73850,186.71577 C 143.73849,185.51265 143.58420,184.61226 143.27561,184.01459 C 142.97092,183.41304 142.51193,183.11226 141.89865,183.11225 M 141.89865,182.17475 C 142.87912,182.17476 143.62716,182.56343 144.14280,183.34077 C 144.66232,184.11421 144.92209,185.23921 144.92209,186.71577 C 144.92209,188.18843 144.66232,189.31342 144.14280,190.09077 C 143.62716,190.86420 142.87912,191.25092 141.89865,191.25092 C 140.91818,191.25092 140.16818,190.86420 139.64865,190.09077 C 139.13303,189.31342 138.87522,188.18843 138.87522,186.71577 C 138.87522,185.23921 139.13303,184.11421 139.64865,183.34077 C 140.16818,182.56343 140.91818,182.17476 141.89865,182.17475 M 158.67405,190.08491 L 160.60764,190.08491 L 160.60764,183.41108 L 158.50412,183.83295 L 158.50412,182.75483 L 160.59592,182.33295 L 161.77951,182.33295 L 161.77951,190.08491 L 163.71311,190.08491 L 163.71311,191.08100 L 158.67405,191.08100 L 158.67405,190.08491' />"
        "    <path"
        "       style='fill:none;fill-opacity:1.0000000;stroke:#00007d;stroke-width:1.2074658;stroke-miterlimit:4.0000000;stroke-dasharray:none;stroke-opacity:1.0000000'"
        "       d='M 161.98199,196.68088 L 172.99344,196.68088 L 172.99344,147.70445 L 161.98199,147.70445' />"
        "    <path"
        "       style='font-size:12.000000;font-style:normal;font-weight:normal;fill:#000000;fill-opacity:1.0000000;stroke:none;stroke-width:1.0000000px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1.0000000;font-family:BitstreamVeraSans'"
        "       d='M 99.545989,166.23838 L 107.05771,166.23838 L 107.05771,167.22276 L 99.545989,167.22276 L 99.545989,166.23838 M 99.545989,168.62901 L 107.05771,168.62901 L 107.05771,169.62510 L 99.545989,169.62510 L 99.545989,168.62901' />"
        "  </g>"
        "</svg>";

    transformImageIcon.showSvgFromMemory(svgData);
    _page_transform.table()
        .attach(transformImageIcon, 0, 3, 2, 3, Gtk::SHRINK | Gtk::FILL | Gtk::EXPAND,   Gtk::SHRINK | Gtk::FILL | Gtk::EXPAND);

}




/*########################################################################
# U P D A T E
########################################################################*/

void
Transformation::updateSelection(PageType page, Inkscape::Selection *selection)
{
    if (!selection || selection->isEmpty())
        return;

    switch (page) {
        case PAGE_MOVE: {
            updatePageMove(selection);
            break;
        }
        case PAGE_SCALE: {
            updatePageScale(selection);
            break;
        }
        case PAGE_ROTATE: {
            updatePageRotate(selection);
            break;
        }
        case PAGE_SKEW: {
            updatePageSkew(selection);
            break;
        }
        case PAGE_TRANSFORM: {
            updatePageTransform(selection);
            break;
        }
        case PAGE_QTY: {
            break;
        }
    }

    set_response_sensitive(Gtk::RESPONSE_APPLY,
                           selection && !selection->isEmpty());
}



void
Transformation::onSelectionChanged(Inkscape::NSApplication::Application *inkscape,
                                   Inkscape::Selection *selection)
{
    //TODO: replace with a Tranformation::getCurrentPage() function
    int page = _notebook.get_current_page();
    ++page;
    updateSelection((PageType)page, selection);
}

void
Transformation::onSelectionModified(Inkscape::NSApplication::Application *inkscape,
                                    Inkscape::Selection *selection,
                                    int unsigned flags)
{
    //TODO: replace with a Tranformation::getCurrentPage() function
    int page = _notebook.get_current_page();
    ++page;
    updateSelection((PageType)page, selection);
}

/* TODO:  Is this even needed?
void
Transformation::onSwitchPage(Gtk::Notebook *notebook,
                                   Gtk::Notebook::Page *page,
                                   guint pagenum)
{
    updateSelection(pagenum, _getSelection());
}
*/

void
Transformation::updatePageMove(Inkscape::Selection *selection)
{
    if (selection && !selection->isEmpty()) {
        if (_check_move_relative.get_active()) {
            NR::Rect bbox = selection->bounds();
            double x = bbox.min()[NR::X];
            double y = bbox.min()[NR::Y];

            _scalar_move_horizontal.setValue(x, "px");
            _scalar_move_vertical.setValue(y, "px");
        }
        _page_move.set_sensitive(true);
    } else {
        _page_move.set_sensitive(false);
    }
}

void
Transformation::updatePageScale(Inkscape::Selection *selection)
{
    if (selection && !selection->isEmpty()) {
          NR::Rect bbox = selection->bounds();
          double x = bbox.extent(NR::X);
          double y = bbox.extent(NR::Y);
        if (_units_scale.isAbsolute()) {
            _scalar_scale_horizontal.setValue(x, "px");
            _scalar_scale_vertical.setValue(y, "px");
        } else {
            /* TODO - Non-absolute units
            _scalar_scale_horizontal.setValue(x, 100.0);
            _scalar_scale_vertical.setValue(y, 100.0);
            */
        }
        _page_scale.set_sensitive(true);
    } else {
        _page_scale.set_sensitive(false);
    }
}

void
Transformation::updatePageRotate(Inkscape::Selection *selection)
{
    if (selection && !selection->isEmpty()) {
        _page_rotate.set_sensitive(true);
    } else {
        _page_rotate.set_sensitive(false);
    }
}

void
Transformation::updatePageSkew(Inkscape::Selection *selection)
{
    if (selection && !selection->isEmpty()) {
        _page_skew.set_sensitive(true);
    } else {
        _page_skew.set_sensitive(false);
    }
}

void
Transformation::updatePageTransform(Inkscape::Selection *selection)
{
    if (selection && !selection->isEmpty()) {
        if (_check_move_relative.get_active()) {
            NR::Rect bbox = selection->bounds();
            double x = bbox.min()[NR::X];
            double y = bbox.min()[NR::Y];

            _scalar_move_horizontal.setValue(x, "px");
            _scalar_move_vertical.setValue(y, "px");
        }
        _page_move.set_sensitive(true);
    } else {
        _page_move.set_sensitive(false);
    }
}





/*########################################################################
# A P P L Y
########################################################################*/



void
Transformation::_apply()
{
    Inkscape::Selection * const selection = _getSelection();
    if (!selection || selection->isEmpty())
        return;

    int const page = _notebook.get_current_page();

    switch (page) {
        case PAGE_MOVE: {
            applyPageMove(selection);
            break;
        }
        case PAGE_ROTATE: {
            applyPageRotate(selection);
            break;
        }
        case PAGE_SCALE: {
            applyPageScale(selection);
            break;
        }
        case PAGE_SKEW: {
            applyPageSkew(selection);
            break;
        }
        case PAGE_TRANSFORM: {
            applyPageTransform(selection);
            break;
        }
    }

    //Let's play with never turning this off
    //set_response_sensitive(Gtk::RESPONSE_APPLY, false);
}

void
Transformation::applyPageMove(Inkscape::Selection *selection)
{
    double x = _scalar_move_horizontal.getValue("px");
    double y = _scalar_move_vertical.getValue("px");

    if (_check_move_relative.get_active()) {

        sp_selection_move_relative(selection, x, y);

    } else {

        NR::Rect bbox = selection->bounds();
        sp_selection_move_relative(selection,
            x - bbox.min()[NR::X], y - bbox.min()[NR::Y]);

    }

    sp_document_done ( SP_DT_DOCUMENT (selection->desktop()) );

}

void
Transformation::applyPageScale(Inkscape::Selection *selection)
{
    double scaleX = _scalar_scale_horizontal.getValue("px");
    double scaleY = _scalar_scale_vertical.getValue("px");

    NR::Rect  bbox(selection->bounds());
    NR::Point center(bbox.midpoint());

    NR::scale scale(scaleX / 100.0, scaleY / 100.0);

    sp_selection_scale_relative(selection, center, scale);

    sp_document_done(SP_DT_DOCUMENT(selection->desktop()));
}

void
Transformation::applyPageRotate(Inkscape::Selection *selection)
{

    double angle = _scalar_rotate.getValue("deg");

    NR::Rect  bbox   = selection->bounds();
    NR::Point center = bbox.midpoint();
    sp_selection_rotate_relative(selection, center, angle);

    sp_document_done(SP_DT_DOCUMENT(selection->desktop()));

}

void
Transformation::applyPageSkew(Inkscape::Selection *selection)
{

    double skewX = _scalar_skew_horizontal.getValue("px");
    double skewY = _scalar_skew_vertical.getValue("px");

    NR::Rect bbox = selection->bounds();
    double width  = bbox.max()[NR::X] - bbox.min()[NR::X];
    double height = bbox.max()[NR::Y] - bbox.min()[NR::Y];

    NR::Point center = bbox.midpoint();

    sp_selection_skew_relative(selection, center,
                    skewX / width, skewY / height );

    sp_document_done(SP_DT_DOCUMENT(selection->desktop()));

}


void
Transformation::applyPageTransform(Inkscape::Selection *selection)
{
    double a = _scalar_transform_a.getValue();
    double b = _scalar_transform_b.getValue();
    double c = _scalar_transform_c.getValue();
    double d = _scalar_transform_d.getValue();
    double e = _scalar_transform_e.getValue();
    double f = _scalar_transform_f.getValue();

    NR::Matrix matrix(a, b, c, d, e, f);

    sp_selection_apply_affine(selection, matrix);

    sp_document_done(SP_DT_DOCUMENT(selection->desktop()));
}





/*########################################################################
# V A L U E - C H A N G E D    C A L L B A C K S
########################################################################*/

void
Transformation::onMoveValueChanged()
{
    /*
    double x = _scalar_move_horizontal.getValue("px");
    double y = _scalar_move_vertical.getValue("px");

    //g_message("onMoveValueChanged: %f, %f px\n", x, y);
    */

    set_response_sensitive(Gtk::RESPONSE_APPLY, true);

}

void
Transformation::onMoveRelativeToggled()
{
    Inkscape::Selection *selection = _getSelection();

    if (!selection || selection->isEmpty())
        return;

    double x = _scalar_move_horizontal.getValue("px");
    double y = _scalar_move_vertical.getValue("px");

    //g_message("onMoveRelativeToggled: %f, %f px\n", x, y);

    NR::Rect bbox = selection->bounds();

    if (_check_move_relative.get_active()) {
        // From absolute to relative
        _scalar_move_horizontal.setValue(x - bbox.min()[NR::X], "px");
        _scalar_move_vertical.setValue(  y - bbox.min()[NR::Y], "px");
    } else {
        // From relative to absolute
        _scalar_move_horizontal.setValue(bbox.min()[NR::X] + x, "px");
        _scalar_move_vertical.setValue(  bbox.min()[NR::Y] + y, "px");
    }


    set_response_sensitive(Gtk::RESPONSE_APPLY, true);
}

void
Transformation::onScaleValueChanged()
{
    /*
    double scalex = _scalar_scale_horizontal.getValue("px");
    double scaley = _scalar_scale_vertical.getValue("px");

    //g_message("onScaleValueChanged: %f, %f px\n", scalex, scaley);
    */

    set_response_sensitive(Gtk::RESPONSE_APPLY, true);
}

void
Transformation::onRotateValueChanged()
{
    /*
    double angle = _scalar_rotate.getValue("deg");

    //g_message("onRotateValueChanged: %f deg\n", angle);
    */

    set_response_sensitive(Gtk::RESPONSE_APPLY, true);
}



void
Transformation::onSkewValueChanged()
{
    /*
    double skewx = _scalar_skew_horizontal.getValue("px");
    double skewy = _scalar_skew_vertical.getValue("px");

    //g_message("onSkewValueChanged:  %f, %f px\n", skewx, skewy);
    */

    set_response_sensitive(Gtk::RESPONSE_APPLY, true);
}



void
Transformation::onTransformValueChanged()
{

    /*
    double a = _scalar_transform_a.getValue();
    double b = _scalar_transform_b.getValue();
    double c = _scalar_transform_c.getValue();
    double d = _scalar_transform_d.getValue();
    double e = _scalar_transform_e.getValue();
    double f = _scalar_transform_f.getValue();

    //g_message("onTransformValueChanged: (%f, %f, %f, %f, %f, %f)\n",
    //          a, b, c, d, e ,f);
    */

    set_response_sensitive(Gtk::RESPONSE_APPLY, true);
}




} // namespace Dialog
} // namespace UI
} // namespace Inkscape



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
