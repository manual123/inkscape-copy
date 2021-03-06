// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 * SVG <feDisplacementMap> implementation.
 *
 */
/*
 * Authors:
 *   hugo Rodrigues <haa.rodrigues@gmail.com>
 *   Abhishek Sharma
 *
 * Copyright (C) 2006 Hugo Rodrigues
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "displacementmap.h"

#include "attributes.h"
#include "helper-fns.h"

#include "display/nr-filter-displacement-map.h"
#include "display/nr-filter.h"

#include "object/sp-filter.h"

#include "svg/svg.h"

#include "xml/repr.h"

SPFeDisplacementMap::SPFeDisplacementMap() : SPFilterPrimitive() {
    this->scale=0;
    this->xChannelSelector = DISPLACEMENTMAP_CHANNEL_ALPHA;
    this->yChannelSelector = DISPLACEMENTMAP_CHANNEL_ALPHA;
    this->in2 = Inkscape::Filters::NR_FILTER_SLOT_NOT_SET;
}

SPFeDisplacementMap::~SPFeDisplacementMap() = default;

/**
 * Reads the Inkscape::XML::Node, and initializes SPFeDisplacementMap variables.  For this to get called,
 * our name must be associated with a repr via "sp_object_type_register".  Best done through
 * sp-object-repr.cpp's repr_name_entries array.
 */
void SPFeDisplacementMap::build(SPDocument *document, Inkscape::XML::Node *repr) {
	SPFilterPrimitive::build(document, repr);

	/*LOAD ATTRIBUTES FROM REPR HERE*/
	this->readAttr(SPAttr::SCALE);
	this->readAttr(SPAttr::IN2);
	this->readAttr(SPAttr::XCHANNELSELECTOR);
	this->readAttr(SPAttr::YCHANNELSELECTOR);

	/* Unlike normal in, in2 is required attribute. Make sure, we can call
	 * it by some name. */
	if (this->in2 == Inkscape::Filters::NR_FILTER_SLOT_NOT_SET ||
		this->in2 == Inkscape::Filters::NR_FILTER_UNNAMED_SLOT)
	{
		SPFilter *parent = SP_FILTER(this->parent);
		this->in2 = this->name_previous_out();
		repr->setAttribute("in2", parent->name_for_image(this->in2));
	}
}

/**
 * Drops any allocated memory.
 */
void SPFeDisplacementMap::release() {
	SPFilterPrimitive::release();
}

static FilterDisplacementMapChannelSelector sp_feDisplacementMap_readChannelSelector(gchar const *value)
{
    if (!value) return DISPLACEMENTMAP_CHANNEL_ALPHA;
    
    switch (value[0]) {
        case 'R':
            return DISPLACEMENTMAP_CHANNEL_RED;
            break;
        case 'G':
            return DISPLACEMENTMAP_CHANNEL_GREEN;
            break;
        case 'B':
            return DISPLACEMENTMAP_CHANNEL_BLUE;
            break;
        case 'A':
            return DISPLACEMENTMAP_CHANNEL_ALPHA;
            break;
        default:
            // error
            g_warning("Invalid attribute for Channel Selector. Valid modes are 'R', 'G', 'B' or 'A'");
            break;
    }
    
    return DISPLACEMENTMAP_CHANNEL_ALPHA; //default is Alpha Channel
}

/**
 * Sets a specific value in the SPFeDisplacementMap.
 */
void SPFeDisplacementMap::set(SPAttr key, gchar const *value) {
    int input;
    double read_num;
    FilterDisplacementMapChannelSelector read_selector;
    
    switch(key) {
	/*DEAL WITH SETTING ATTRIBUTES HERE*/
        case SPAttr::XCHANNELSELECTOR:
            read_selector = sp_feDisplacementMap_readChannelSelector(value);
            
            if (read_selector != this->xChannelSelector){
                this->xChannelSelector = read_selector;
                this->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        case SPAttr::YCHANNELSELECTOR:
            read_selector = sp_feDisplacementMap_readChannelSelector(value);
            
            if (read_selector != this->yChannelSelector){
                this->yChannelSelector = read_selector;
                this->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        case SPAttr::SCALE:
            read_num = value ? helperfns_read_number(value) : 0;
            
            if (read_num != this->scale) {
                this->scale = read_num;
                this->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        case SPAttr::IN2:
            input = this->read_in(value);
            
            if (input != this->in2) {
                this->in2 = input;
                this->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        default:
        	SPFilterPrimitive::set(key, value);
            break;
    }
}

/**
 * Receives update notifications.
 */
void SPFeDisplacementMap::update(SPCtx *ctx, guint flags) {
    if (flags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG |
                 SP_OBJECT_VIEWPORT_MODIFIED_FLAG)) {

        /* do something to trigger redisplay, updates? */

    }

    /* Unlike normal in, in2 is required attribute. Make sure, we can call
     * it by some name. */
    if (this->in2 == Inkscape::Filters::NR_FILTER_SLOT_NOT_SET ||
        this->in2 == Inkscape::Filters::NR_FILTER_UNNAMED_SLOT)
    {
        SPFilter *parent = SP_FILTER(this->parent);
        this->in2 = this->name_previous_out();

        //XML Tree being used directly here while it shouldn't be.
        this->setAttribute("in2", parent->name_for_image(this->in2));
    }

    SPFilterPrimitive::update(ctx, flags);
}

static char const * get_channelselector_name(FilterDisplacementMapChannelSelector selector) {
    switch(selector) {
        case DISPLACEMENTMAP_CHANNEL_RED:
            return "R";
        case DISPLACEMENTMAP_CHANNEL_GREEN:
            return "G";
        case DISPLACEMENTMAP_CHANNEL_BLUE:
            return "B";
        case DISPLACEMENTMAP_CHANNEL_ALPHA:
            return "A";
        default:
            return nullptr;
    }
}

/**
 * Writes its settings to an incoming repr object, if any.
 */
Inkscape::XML::Node* SPFeDisplacementMap::write(Inkscape::XML::Document *doc, Inkscape::XML::Node *repr, guint flags) {
    SPFilter *parent = SP_FILTER(this->parent);

    if (!repr) {
        repr = doc->createElement("svg:feDisplacementMap");
    }

    gchar const *in2_name = parent->name_for_image(this->in2);

    if( !in2_name ) {

        // This code is very similar to name_previous_out()
        SPObject *i = parent->firstChild();

        // Find previous filter primitive
        while (i && i->getNext() != this) {
        	i = i->getNext();
        }

        if( i ) {
            SPFilterPrimitive *i_prim = SP_FILTER_PRIMITIVE(i);
            in2_name = parent->name_for_image(i_prim->image_out);
        }
    }

    if (in2_name) {
        repr->setAttribute("in2", in2_name);
    } else {
        g_warning("Unable to set in2 for feDisplacementMap");
    }

    repr->setAttributeSvgDouble("scale", this->scale);
    repr->setAttribute("xChannelSelector",
                       get_channelselector_name(this->xChannelSelector));
    repr->setAttribute("yChannelSelector",
                       get_channelselector_name(this->yChannelSelector));

    SPFilterPrimitive::write(doc, repr, flags);

    return repr;
}

void SPFeDisplacementMap::build_renderer(Inkscape::Filters::Filter* filter) {
    g_assert(filter != nullptr);

    int primitive_n = filter->add_primitive(Inkscape::Filters::NR_FILTER_DISPLACEMENTMAP);
    Inkscape::Filters::FilterPrimitive *nr_primitive = filter->get_primitive(primitive_n);
    Inkscape::Filters::FilterDisplacementMap *nr_displacement_map = dynamic_cast<Inkscape::Filters::FilterDisplacementMap*>(nr_primitive);
    g_assert(nr_displacement_map != nullptr);

    this->renderer_common(nr_primitive);

    nr_displacement_map->set_input(1, this->in2);
    nr_displacement_map->set_scale(this->scale);
    nr_displacement_map->set_channel_selector(0, this->xChannelSelector);
    nr_displacement_map->set_channel_selector(1, this->yChannelSelector);
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
