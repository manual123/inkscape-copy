// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 * SVG <feSpecularLighting> implementation.
 *
 */
/*
 * Authors:
 *   hugo Rodrigues <haa.rodrigues@gmail.com>
 *   Jean-Rene Reinhard <jr@komite.net>
 *   Abhishek Sharma
 *
 * Copyright (C) 2006 Hugo Rodrigues
 *               2007 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

// Same directory
#include "specularlighting.h"
#include "distantlight.h"
#include "pointlight.h"
#include "spotlight.h"

#include "attributes.h"
#include "strneq.h"

#include "display/nr-filter.h"
#include "display/nr-filter-specularlighting.h"

#include "object/sp-object.h"

#include "svg/svg.h"
#include "svg/svg-color.h"
#include "svg/svg-icc-color.h"

#include "xml/repr.h"

/* FeSpecularLighting base class */
static void sp_feSpecularLighting_children_modified(SPFeSpecularLighting *sp_specularlighting);

SPFeSpecularLighting::SPFeSpecularLighting() : SPFilterPrimitive() {
    this->surfaceScale = 1;
    this->specularConstant = 1;
    this->specularExponent = 1;
    this->lighting_color = 0xffffffff;
    this->icc = nullptr;

    //TODO kernelUnit
    this->renderer = nullptr;
    
    this->surfaceScale_set = FALSE;
    this->specularConstant_set = FALSE;
    this->specularExponent_set = FALSE;
    this->lighting_color_set = FALSE;
}

SPFeSpecularLighting::~SPFeSpecularLighting() = default;

/**
 * Reads the Inkscape::XML::Node, and initializes SPFeSpecularLighting variables.  For this to get called,
 * our name must be associated with a repr via "sp_object_type_register".  Best done through
 * sp-object-repr.cpp's repr_name_entries array.
 */
void SPFeSpecularLighting::build(SPDocument *document, Inkscape::XML::Node *repr) {
	SPFilterPrimitive::build(document, repr);

	/*LOAD ATTRIBUTES FROM REPR HERE*/
	this->readAttr(SPAttr::SURFACESCALE);
	this->readAttr(SPAttr::SPECULARCONSTANT);
	this->readAttr(SPAttr::SPECULAREXPONENT);
	this->readAttr(SPAttr::KERNELUNITLENGTH);
	this->readAttr(SPAttr::LIGHTING_COLOR);
}

/**
 * Drops any allocated memory.
 */
void SPFeSpecularLighting::release() {
	SPFilterPrimitive::release();
}

/**
 * Sets a specific value in the SPFeSpecularLighting.
 */
void SPFeSpecularLighting::set(SPAttr key, gchar const *value) {
    gchar const *cend_ptr = nullptr;
    gchar *end_ptr = nullptr;

    switch(key) {
	/*DEAL WITH SETTING ATTRIBUTES HERE*/
//TODO test forbidden values
        case SPAttr::SURFACESCALE:
            end_ptr = nullptr;
            if (value) {
                this->surfaceScale = g_ascii_strtod(value, &end_ptr);
                if (end_ptr) {
                    this->surfaceScale_set = TRUE;
                } else {
                    g_warning("this: surfaceScale should be a number ... defaulting to 1");
                }

            }
            //if the attribute is not set or has an unreadable value
            if (!value || !end_ptr) {
                this->surfaceScale = 1;
                this->surfaceScale_set = FALSE;
            }
            if (this->renderer) {
                this->renderer->surfaceScale = this->surfaceScale;
            }
            this->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::SPECULARCONSTANT:
            end_ptr = nullptr;
            if (value) {
                this->specularConstant = g_ascii_strtod(value, &end_ptr);
                if (end_ptr && this->specularConstant >= 0) {
                    this->specularConstant_set = TRUE;
                } else {
                    end_ptr = nullptr;
                    g_warning("this: specularConstant should be a positive number ... defaulting to 1");
                }
            }
            if (!value || !end_ptr) {
                this->specularConstant = 1;
                this->specularConstant_set = FALSE;
            }
            if (this->renderer) {
                this->renderer->specularConstant = this->specularConstant;
            }
            this->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::SPECULAREXPONENT:
            end_ptr = nullptr;
            if (value) {
                this->specularExponent = g_ascii_strtod(value, &end_ptr);
                if (this->specularExponent >= 1 && this->specularExponent <= 128) {
                    this->specularExponent_set = TRUE;
                } else {
                    end_ptr = nullptr;
                    g_warning("this: specularExponent should be a number in range [1, 128] ... defaulting to 1");
                }
            } 
            if (!value || !end_ptr) {
                this->specularExponent = 1;
                this->specularExponent_set = FALSE;
            }
            if (this->renderer) {
                this->renderer->specularExponent = this->specularExponent;
            }
            this->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::KERNELUNITLENGTH:
            //TODO kernelUnit
            //this->kernelUnitLength.set(value);
            /*TODOif (feSpecularLighting->renderer) {
                feSpecularLighting->renderer->surfaceScale = feSpecularLighting->renderer;
            }
            */
            this->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::LIGHTING_COLOR:
            cend_ptr = nullptr;
            this->lighting_color = sp_svg_read_color(value, &cend_ptr, 0xffffffff);
            //if a value was read
            if (cend_ptr) {
                while (g_ascii_isspace(*cend_ptr)) {
                    ++cend_ptr;
                }
                if (strneq(cend_ptr, "icc-color(", 10)) {
                    if (!this->icc) this->icc = new SVGICCColor();
                    if ( ! sp_svg_read_icc_color( cend_ptr, this->icc ) ) {
                        delete this->icc;
                        this->icc = nullptr;
                    }
                }
                this->lighting_color_set = TRUE;
            } else {
                //lighting_color already contains the default value
                this->lighting_color_set = FALSE;
            }
            if (this->renderer) {
                this->renderer->lighting_color = this->lighting_color;
            }
            this->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        default:
        	SPFilterPrimitive::set(key, value);
            break;
    }
}

/**
 * Receives update notifications.
 */
void SPFeSpecularLighting::update(SPCtx *ctx, guint flags) {
    if (flags & (SP_OBJECT_MODIFIED_FLAG)) {
        this->readAttr(SPAttr::SURFACESCALE);
        this->readAttr(SPAttr::SPECULARCONSTANT);
        this->readAttr(SPAttr::SPECULAREXPONENT);
        this->readAttr(SPAttr::KERNELUNITLENGTH);
        this->readAttr(SPAttr::LIGHTING_COLOR);
    }

    SPFilterPrimitive::update(ctx, flags);
}

/**
 * Writes its settings to an incoming repr object, if any.
 */
Inkscape::XML::Node* SPFeSpecularLighting::write(Inkscape::XML::Document *doc, Inkscape::XML::Node *repr, guint flags) {
    /* TODO: Don't just clone, but create a new repr node and write all
     * relevant values _and children_ into it */
    if (!repr) {
        repr = this->getRepr()->duplicate(doc);
        //repr = doc->createElement("svg:feSpecularLighting");
    }

    if (this->surfaceScale_set) {
        repr->setAttributeCssDouble("surfaceScale", this->surfaceScale);
    }

    if (this->specularConstant_set) {
        repr->setAttributeCssDouble("specularConstant", this->specularConstant);
    }

    if (this->specularExponent_set) {
        repr->setAttributeCssDouble("specularExponent", this->specularExponent);
    }

    /*TODO kernelUnits */ 
    if (this->lighting_color_set) {
        gchar c[64];
        sp_svg_write_color(c, sizeof(c), this->lighting_color);
        repr->setAttribute("lighting-color", c);
    }
    
    SPFilterPrimitive::write(doc, repr, flags);

    return repr;
}

/**
 * Callback for child_added event.
 */
void SPFeSpecularLighting::child_added(Inkscape::XML::Node *child, Inkscape::XML::Node *ref) {
    SPFilterPrimitive::child_added(child, ref);

    sp_feSpecularLighting_children_modified(this);
    this->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

/**
 * Callback for remove_child event.
 */
void SPFeSpecularLighting::remove_child(Inkscape::XML::Node *child) {
    SPFilterPrimitive::remove_child(child);

    sp_feSpecularLighting_children_modified(this);
    this->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

void SPFeSpecularLighting::order_changed(Inkscape::XML::Node *child, Inkscape::XML::Node *old_ref, Inkscape::XML::Node *new_ref) {
    SPFilterPrimitive::order_changed(child, old_ref, new_ref);

    sp_feSpecularLighting_children_modified(this);
    this->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

static void sp_feSpecularLighting_children_modified(SPFeSpecularLighting *sp_specularlighting) {
	if (sp_specularlighting->renderer) {
        sp_specularlighting->renderer->light_type = Inkscape::Filters::NO_LIGHT;
        
        if (SP_IS_FEDISTANTLIGHT(sp_specularlighting->firstChild())) {
            sp_specularlighting->renderer->light_type = Inkscape::Filters::DISTANT_LIGHT;
            sp_specularlighting->renderer->light.distant = SP_FEDISTANTLIGHT(sp_specularlighting->firstChild());
        }
        
        if (SP_IS_FEPOINTLIGHT(sp_specularlighting->firstChild())) {
            sp_specularlighting->renderer->light_type = Inkscape::Filters::POINT_LIGHT;
            sp_specularlighting->renderer->light.point = SP_FEPOINTLIGHT(sp_specularlighting->firstChild());
        }
        
        if (SP_IS_FESPOTLIGHT(sp_specularlighting->firstChild())) {
            sp_specularlighting->renderer->light_type = Inkscape::Filters::SPOT_LIGHT;
            sp_specularlighting->renderer->light.spot = SP_FESPOTLIGHT(sp_specularlighting->firstChild());
        }
	}
}

void SPFeSpecularLighting::build_renderer(Inkscape::Filters::Filter* filter) {
    g_assert(filter != nullptr);

    int primitive_n = filter->add_primitive(Inkscape::Filters::NR_FILTER_SPECULARLIGHTING);
    Inkscape::Filters::FilterPrimitive *nr_primitive = filter->get_primitive(primitive_n);
    Inkscape::Filters::FilterSpecularLighting *nr_specularlighting = dynamic_cast<Inkscape::Filters::FilterSpecularLighting*>(nr_primitive);
    g_assert(nr_specularlighting != nullptr);

    this->renderer = nr_specularlighting;
    this->renderer_common(nr_primitive);

    nr_specularlighting->specularConstant = this->specularConstant;
    nr_specularlighting->specularExponent = this->specularExponent;
    nr_specularlighting->surfaceScale = this->surfaceScale;
    nr_specularlighting->lighting_color = this->lighting_color;
    nr_specularlighting->set_icc(this->icc);

    //We assume there is at most one child
    nr_specularlighting->light_type = Inkscape::Filters::NO_LIGHT;

    if (SP_IS_FEDISTANTLIGHT(this->firstChild())) {
        nr_specularlighting->light_type = Inkscape::Filters::DISTANT_LIGHT;
        nr_specularlighting->light.distant = SP_FEDISTANTLIGHT(this->firstChild());
    }

    if (SP_IS_FEPOINTLIGHT(this->firstChild())) {
        nr_specularlighting->light_type = Inkscape::Filters::POINT_LIGHT;
        nr_specularlighting->light.point = SP_FEPOINTLIGHT(this->firstChild());
    }

    if (SP_IS_FESPOTLIGHT(this->firstChild())) {
        nr_specularlighting->light_type = Inkscape::Filters::SPOT_LIGHT;
        nr_specularlighting->light.spot = SP_FESPOTLIGHT(this->firstChild());
    }
        
    //nr_offset->set_dx(sp_offset->dx);
    //nr_offset->set_dy(sp_offset->dy);
}


/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
