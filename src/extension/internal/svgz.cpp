/*
 * Code to handle compressed SVG loading and saving. Almost identical to svg
 * routines, but separated for simpler extension maintenance.
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Ted Gould <ted@gould.cx>
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 2002-2005 Authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <string.h>
#include <glibmm/i18n.h>
#include "xml/repr.h"
#include "sp-object.h"
#include "document.h"
#include "dir-util.h"
#include "../implementation/implementation.h"
#include "svgz.h"
#include "file.h"
#include "extension/system.h"
#include "extension/output.h"

namespace Inkscape {
namespace Extension {
namespace Internal {

/**
    \return   None
    \brief    What would an SVG editor be without loading/saving SVG
              files.  This function sets that up.

    For each module there is a call to Inkscape::Extension::build_from_mem
    with a rather large XML file passed in.  This is a constant string
    that describes the module.  At the end of this call a module is
    returned that is basically filled out.  The one thing that it doesn't
    have is the key function for the operation.  And that is linked at
    the end of each call.
*/
void
Svgz::init(void)
{
    Inkscape::Extension::Extension * ext;

    /* SVGZ in */
    ext = Inkscape::Extension::build_from_mem(
        "<inkscape-extension>\n"
            "<name>SVGZ Input</name>\n"
            "<id>" SP_MODULE_KEY_INPUT_SVGZ "</id>\n"
            "<dependency type=\"extension\">" SP_MODULE_KEY_INPUT_SVG "</dependency>\n"
            "<input>\n"
                "<extension>.svgz</extension>\n"
                "<mimetype>image/x-svgz</mimetype>\n"
                "<filetypename>Compressed Inkscape SVG (*.svgz)</filetypename>\n"
                "<filetypetooltip>Inkscape's native file format compressed with GZip</filetypetooltip>\n"
                "<output_extension>" SP_MODULE_KEY_OUTPUT_SVGZ_INKSCAPE "</output_extension>\n"
            "</input>\n"
        "</inkscape-extension>", new Svgz());

    ext = Inkscape::Extension::build_from_mem(
        "<inkscape-extension>\n"
            "<name>SVGZ Output</name>\n"
            "<id>" SP_MODULE_KEY_OUTPUT_SVGZ_INKSCAPE "</id>\n"
            "<dependency type=\"extension\">" SP_MODULE_KEY_OUTPUT_SVGZ_INKSCAPE "</dependency>\n"
            "<output>\n"
                "<extension>.svgz</extension>\n"
                "<mimetype>image/x-svgz</mimetype>\n"
                "<filetypename>Compressed Inkscape SVG (*.svgz)</filetypename>\n"
                "<filetypetooltip>Inkscape's native file format compressed with GZip</filetypetooltip>\n"
                "<dataloss>FALSE</dataloss>\n"
            "</output>\n"
        "</inkscape-extension>\n", new Svgz());

    return;
}


} } }  // namespace inkscape, module, implementation

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
