// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Provides a class that can contain active TemporaryItem's on a desktop
 * Code inspired by message-stack.cpp
 *
 * Authors:
 *   Johan Engelen
 *
 * Copyright (C) Johan Engelen 2008 <j.b.c.engelen@utwente.nl>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "canvas-temporary-item.h"
#include "canvas-temporary-item-list.h"

namespace Inkscape {
namespace Display {

TemporaryItemList::TemporaryItemList(SPDesktop *desktop)
    : desktop(desktop)
{

}

TemporaryItemList::~TemporaryItemList()
{
    // delete all items in list so the timeouts are removed
    for (auto tempitem : itemlist) {
        delete tempitem;
    }
    itemlist.clear();
}

/* Note that TemporaryItem or TemporaryItemList is responsible for deletion and such, so this return pointer can safely be ignored. */
TemporaryItem *
TemporaryItemList::add_item(CanvasItem *item, unsigned int lifetime)
{
    // beware of strange things happening due to very short timeouts
    TemporaryItem * tempitem;
    if (lifetime == 0)
        tempitem = new TemporaryItem(item, 0, true);
    else {
        tempitem = new TemporaryItem(item, lifetime);
        tempitem->signal_timeout.connect( sigc::mem_fun(*this, &TemporaryItemList::_item_timeout) );
    }

    itemlist.push_back(tempitem);
    return tempitem;
}

void
TemporaryItemList::delete_item( TemporaryItem * tempitem )
{
    // check if the item is in the list, if so, delete it. (in other words, don't wait for the item to delete itself)
    bool in_list = false;
    for (auto & it : itemlist) {
        if ( it == tempitem ) {
            in_list = true;
            break;
        }
    }
    if (in_list) {
        itemlist.remove(tempitem);
        delete tempitem;
    }
}

void
TemporaryItemList::_item_timeout(TemporaryItem * tempitem)
{
    itemlist.remove(tempitem);
    // no need to delete the item, it does that itself after signal_timeout.emit() completes
}

} //namespace Display
} /* namespace Inkscape */

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
