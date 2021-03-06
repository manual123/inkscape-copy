// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Actions for inkscape.
 *
 *
 * This file implements routines necessary to deal with verbs.  A verb
 * is a numeric identifier used to retrieve standard SPActions for particular
 * views.
 *//*
 * Authors:
 * see git history
 * Lauris Kaplinski <lauris@kaplinski.com>
 * Ted Gould <ted@gould.cx>
 * MenTaLguY <mental@rydia.net>
 * David Turner <novalis@gnu.org>
 * bulia byak <buliabyak@users.sf.net>
 * Jon A. Cruz <jon@joncruz.org>
 * Abhishek Sharma
 * 2006 Johan Engelen <johan@shouraizou.nl>
 * 2012 Kris De Gussem <Kris.DeGussem@gmail.com>
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "verbs.h"

#include <cstring>
#include <string>

// Note that gtkmm headers must be included before gtk+ C headers
// in all files.  The same applies for glibmm/glib etc.
// If this is not done, then errors will be generate relating to Glib::Threads being undefined

#include <gtkmm/filechooserdialog.h>
#include <gtkmm/messagedialog.h>

#include "desktop.h"

#include "document.h"
#include "file.h"
#include "gradient-drag.h"
#include "inkscape.h"
#include "inkscape-version.h"
#include "layer-manager.h"
#include "message-stack.h"
#include "page-manager.h"
#include "path-chemistry.h"
#include "selection-chemistry.h"
#include "seltrans.h"
#include "text-chemistry.h"

#include "actions/actions-tools.h"

#include "display/curve.h"

#include "extension/effect.h"

#include "helper/action.h"

#include "live_effects/effect.h"
#include "live_effects/lpe-powerclip.h"
#include "live_effects/lpe-powermask.h"

#include "object/sp-defs.h"
#include "object/sp-flowtext.h"
#include "object/sp-guide.h"
#include "object/sp-namedview.h"
#include "object/sp-object.h"

#include "path/path-offset.h"
#include "path/path-outline.h"
#include "path/path-simplify.h"

#include "ui/dialog/align-and-distribute.h"
#include "ui/dialog/clonetiler.h"
#include "ui/dialog/document-properties.h"
#include "ui/dialog/glyphs.h"
#include "ui/dialog/icon-preview.h"
#include "ui/dialog/inkscape-preferences.h"
#include "ui/dialog/layer-properties.h"
#include "ui/dialog/new-from-template.h"
#include "ui/dialog/object-properties.h"
#include "ui/dialog/save-template-dialog.h"
#include "ui/dialog/swatches.h"
#include "ui/dialog/symbols.h"
#include "ui/icon-names.h"
#include "ui/dialog/dialog-container.h"
#include "ui/interface.h"
#include "ui/shape-editor.h"
#include "ui/shortcuts.h"
#include "ui/tools/freehand-base.h"
#include "ui/tools/node-tool.h"
#include "ui/tools/pen-tool.h"
#include "ui/tools/pencil-tool.h"
#include "ui/tools/select-tool.h"
#include "ui/widget/canvas.h"  // Canvas area

using Inkscape::DocumentUndo;
using Inkscape::UI::Dialog::DialogContainer;

/**
 * Return the name without underscores and ellipsis, for use in dialog
 * titles, etc. Allocated memory must be freed by caller.
 */
gchar *sp_action_get_title(SPAction const *action)
{
    char const *src = action->name;
    size_t const len = strlen(src);
    gchar *ret = g_new(gchar, len + 1);
    unsigned ri = 0;

    for (unsigned si = 0 ; ; si++)  {
        int const c = src[si];
        // Ignore Unicode Character "???" (U+2026)
        if ( c == '\xE2' && si + 2 < len && src[si+1] == '\x80' && src[si+2] == '\xA6' ) {
            si += 2;
        } else if ( c != '_' && c != '.' ) {
            ret[ri] = c;
            ri++;
            if (c == '\0') {
                return ret;
            }
        }
    }

} // end of sp_action_get_title()

namespace Inkscape {

/**
 * A class to encompass all of the verbs which deal with file operations.
 */
class FileVerb : public Verb {
private:
    static void perform(SPAction *action, void *mydata);
protected:
    SPAction *make_action(Inkscape::ActionContext const & context) override;
public:
    /** Use the Verb initializer with the same parameters. */
    FileVerb(unsigned int const code,
             gchar const *id,
             gchar const *name,
             gchar const *tip,
             gchar const *image) :
        Verb(code, id, name, tip, image, _("File"))
    { }
}; // FileVerb class

/**
 * A class to encompass all of the verbs which deal with edit operations.
 */
class EditVerb : public Verb {
private:
    static void perform(SPAction *action, void *mydata);
protected:
    SPAction *make_action(Inkscape::ActionContext const & context) override;
public:
    /** Use the Verb initializer with the same parameters. */
    EditVerb(unsigned int const code,
             gchar const *id,
             gchar const *name,
             gchar const *tip,
             gchar const *image) :
        Verb(code, id, name, tip, image, _("Edit"))
    { }
}; // EditVerb class

/**
 * A class to encompass all of the verbs which deal with selection operations.
 */
class SelectionVerb : public Verb {
private:
    static void perform(SPAction *action, void *mydata);
protected:
    SPAction *make_action(Inkscape::ActionContext const & context) override;
public:
    /** Use the Verb initializer with the same parameters. */
    SelectionVerb(unsigned int const code,
                  gchar const *id,
                  gchar const *name,
                  gchar const *tip,
                  gchar const *image) :
        Verb(code, id, name, tip, image, _("Selection"))
    { }
}; // SelectionVerb class

/**
 * A class to encompass all of the verbs which deal with layer operations.
 */
class LayerVerb : public Verb {
private:
    static void perform(SPAction *action, void *mydata);
protected:
    SPAction *make_action(Inkscape::ActionContext const & context) override;
public:
    /** Use the Verb initializer with the same parameters. */
    LayerVerb(unsigned int const code,
              gchar const *id,
              gchar const *name,
              gchar const *tip,
              gchar const *image) :
        Verb(code, id, name, tip, image, _("Layer"))
    { }
}; // LayerVerb class

/**
 * A class to encompass all of the verbs which deal with operations related to objects.
 */
class ObjectVerb : public Verb {
private:
    static void perform(SPAction *action, void *mydata);
protected:
    SPAction *make_action(Inkscape::ActionContext const & context) override;
public:
    /** Use the Verb initializer with the same parameters. */
    ObjectVerb(unsigned int const code,
               gchar const *id,
               gchar const *name,
               gchar const *tip,
               gchar const *image) :
        Verb(code, id, name, tip, image, _("Object"))
    { }
}; // ObjectVerb class

/**
 * A class to encompass all of the verbs which deal with text operations.
 */
class TextVerb : public Verb {
private:
    static void perform(SPAction *action, void *mydata);
protected:
    SPAction *make_action(Inkscape::ActionContext const & context) override;
public:
    /** Use the Verb initializer with the same parameters. */
    TextVerb(unsigned int const code,
              gchar const *id,
              gchar const *name,
              gchar const *tip,
              gchar const *image) :
        Verb(code, id, name, tip, image, _("Text"))
    { }
}; //TextVerb : public Verb

Verb::VerbTable Verb::_verbs;
Verb::VerbIDTable Verb::_verb_ids;

/**
 * Create a verb without a code.
 *
 * This function calls the other constructor for all of the parameters,
 * but generates the code.  It is important to READ THE OTHER DOCUMENTATION
 * it has important details in it.  To generate the code a static is
 * used which starts at the last static value: \c SP_VERB_LAST.  For
 * each call it is incremented.  The list of allocated verbs is kept
 * in the \c _verbs hashtable which is indexed by the \c code.
 */
Verb::Verb(gchar const *id, gchar const *name, gchar const *tip, gchar const *image, gchar const *group) :
    _actions(nullptr),
    _id(id),
    _name(name),
    _tip(tip),
    _full_tip(nullptr),
    _image(image),
    _code(0),
    _group(group),
    _default_sensitive(false)
{
    static int count = SP_VERB_LAST;

    count++;
    _code = count;
    _verbs.insert(VerbTable::value_type(count, this));
    _verb_ids.insert(VerbIDTable::value_type(_id, this));
}

/**
 * Destroy a verb.
 *
 * The only allocated variable is the _actions variable.  If it has
 * been allocated it is deleted.
 */
Verb::~Verb()
{
    // Remove verbs from lists
    _verbs.erase(_code);
    _verb_ids.erase(_id);

    /// \todo all the actions need to be cleaned up first.
    delete _actions;

    if (_full_tip) {
        g_free(_full_tip);
        _full_tip = nullptr;
    }
}

/**
 * Verbs are no good without actions.  This is a place holder
 * for a function that every subclass should write.  Most
 * can be written using \c make_action_helper.
 *
 * @param  context  Which context the action should be created for.
 * @return NULL to represent error (this function shouldn't ever be called)
 */
SPAction *Verb::make_action(Inkscape::ActionContext const & /*context*/)
{
    //std::cout << "make_action" << std::endl;
    return nullptr;
}

/**
 * Create an action for a \c FileVerb.
 * Calls \c make_action_helper with the \c vector.
 *
 * @param  context  Which context the action should be created for.
 * @return The built action.
 */
SPAction *FileVerb::make_action(Inkscape::ActionContext const & context)
{
    //std::cout << "fileverb: make_action: " << &perform << std::endl;
    return make_action_helper(context, &perform);
}

/**
 * Create an action for a \c EditVerb.
 *
 * Calls \c make_action_helper with the \c vector.
 *
 * @param  context  Which context the action should be created for.
 * @return The built action.
 */
SPAction *EditVerb::make_action(Inkscape::ActionContext const & context)
{
    //std::cout << "editverb: make_action: " << &perform << std::endl;
    return make_action_helper(context, &perform);
}

/**
 * Create an action for a \c SelectionVerb.
 *
 * Calls \c make_action_helper with the \c vector.
 *
 * @param  context  Which context the action should be created for.
 * @return The built action.
 */
SPAction *SelectionVerb::make_action(Inkscape::ActionContext const & context)
{
    return make_action_helper(context, &perform);
}

/**
 * Create an action for a \c LayerVerb.
 *
 * Calls \c make_action_helper with the \c vector.
 *
 * @param  context  Which context the action should be created for.
 * @return The built action.
 */
SPAction *LayerVerb::make_action(Inkscape::ActionContext const & context)
{
    return make_action_helper(context, &perform);
}

/**
 * Create an action for a \c ObjectVerb.
 *
 * Calls \c make_action_helper with the \c vector.
 *
 * @param  context  Which context the action should be created for.
 * @return The built action.
 */
SPAction *ObjectVerb::make_action(Inkscape::ActionContext const & context)
{
    return make_action_helper(context, &perform);
}

/**
 * Create an action for a \c TextVerb.
 *
 * Calls \c make_action_helper with the \c vector.
 *
 * @param  context  Which context the action should be created for.
 * @return The built action.
 */
SPAction *TextVerb::make_action(Inkscape::ActionContext const & context)
{
    return make_action_helper(context, &perform);
}

/**
 * A quick little convenience function to make building actions
 * a little bit easier.
 *
 * This function does a couple of things.  The most obvious is that
 * it allocates and creates the action.  When it does this it
 * translates the \c _name and \c _tip variables.  This allows them
 * to be statically allocated easily, and get translated in the end.  Then,
 * if the action gets created, a listener is added to the action with
 * the vector that is passed in.
 *
 * @param  context Which context the action should be created for.
 * @param  vector  The function vector for the verb.
 * @return The created action.
 */
SPAction *Verb::make_action_helper(Inkscape::ActionContext const & context, void (*perform_fun)(SPAction *, void *), void *in_pntr)
{
    SPAction *action;
    //std::cout << "Adding action: " << _code << std::endl;
    action = sp_action_new(context, _id, _(_name),
                           _tip ? _(_tip) : nullptr, _image, this);

    if (action == nullptr) return nullptr;

    action->signal_perform.connect(
        sigc::bind(
            sigc::bind(
                sigc::ptr_fun(perform_fun),
                in_pntr ? in_pntr : reinterpret_cast<void*>(_code)),
            action));

    return action;
}

/**
 * A function to get an action if it exists, or otherwise to build it.
 *
 * This function will get the action for a given view for this verb.  It
 * will create the verb if it can't be found in the ActionTable.  Also,
 * if the \c ActionTable has not been created, it gets created by this
 * function.
 *
 * If the action is created, it's sensitivity must be determined.  The
 * default for a new action is that it is sensitive.  If the value in
 * \c _default_sensitive is \c false, then the sensitivity must be
 * removed.  Also, if the view being created is based on the same
 * document as a view already created, the sensitivity should be the
 * same as views on that document.  A view with the same document is
 * looked for, and the sensitivity is matched.  Unfortunately, this is
 * currently a linear search.
 *
 * @param  context  The action context which this action relates to.
 * @return The action, or NULL if there is an error.
 */
SPAction *Verb::get_action(Inkscape::ActionContext const & context)
{
    SPAction *action = nullptr;

    if ( _actions == nullptr ) {
        _actions = new ActionTable;
    }
    ActionTable::iterator action_found = _actions->find(context.getView());

    if (action_found != _actions->end()) {
        action = action_found->second;
    } else {
        action = this->make_action(context);

        if (action == nullptr) printf("Hmm, NULL in %s\n", _name);
        if (!_default_sensitive) {
            sp_action_set_sensitive(action, 0);
        } else {
            for (ActionTable::iterator cur_action = _actions->begin();
                 cur_action != _actions->end() && context.getView() != nullptr;
                 ++cur_action) {
                if (cur_action->first != nullptr && cur_action->first->doc() == context.getDocument()) {
                    sp_action_set_sensitive(action, cur_action->second->sensitive);
                    break;
                }
            }
        }

        _actions->insert(ActionTable::value_type(context.getView(), action));
    }

    return action;
}

/* static */
bool Verb::ensure_desktop_valid(SPAction *action)
{
    if (sp_action_get_desktop(action) != nullptr) {
        return true;
    }
    g_printerr("WARNING: ignoring verb %s - GUI required for this verb.\n", action->id);
    return false;
}

void Verb::sensitive(SPDocument *in_doc, bool in_sensitive)
{
    // printf("Setting sensitivity of \"%s\" to %d\n", _name, in_sensitive);
    if (_actions != nullptr) {
        for (auto & _action : *_actions) {
            if (in_doc == nullptr || (_action.first != nullptr && _action.first->doc() == in_doc)) {
                sp_action_set_sensitive(_action.second, in_sensitive ? 1 : 0);
            }
        }
    }

    if (in_doc == nullptr) {
        _default_sensitive = in_sensitive;
    }

    return;
}

/**
 * Accessor to get the tooltip for verb as localised string.
 */
gchar const *Verb::get_tip()
{
    gchar const *result = nullptr;
    if (_tip) {
        Gtk::AccelKey shortcut = Inkscape::Shortcuts::getInstance().get_shortcut_from_verb(this);
        if ( (shortcut.get_key() != _shortcut.get_key() && shortcut.get_mod() != shortcut.get_mod()) || !_full_tip) {
            if (_full_tip) {
                g_free(_full_tip);
                _full_tip = nullptr;
            }
            _shortcut = shortcut;
            Glib::ustring shortcutString = Inkscape::Shortcuts::get_label(shortcut);
            if (!shortcutString.empty()) {
                _full_tip = g_strdup_printf("%s (%s)", _(_tip), shortcutString.c_str());
            } else {
                _full_tip = g_strdup(_(_tip));
            }
        }
        result = _full_tip;
    }

    return result;
}

void
Verb::name(SPDocument *in_doc, Glib::ustring in_name)
{
    if (_actions != nullptr) {
        for (auto & _action : *_actions) {
            if (in_doc == nullptr || (_action.first != nullptr && _action.first->doc() == in_doc)) {
                sp_action_set_name(_action.second, in_name);
            }
        }
    }
}

/**
 * A function to remove the action associated with a view.
 *
 * This function looks for the action in \c _actions.  If it is
 * found then it is unreferenced and the entry in the action
 * table is erased.
 *
 * @param  view  Which view's actions should be removed.
 * @return None
 */
void Verb::delete_view(Inkscape::UI::View::View *view)
{
    if (_actions == nullptr) return;
    if (_actions->empty()) return;

#if 0
    static int count = 0;
    std::cout << count++ << std::endl;
#endif

    ActionTable::iterator action_found = _actions->find(view);

    if (action_found != _actions->end()) {
        SPAction *action = action_found->second;
        _actions->erase(action_found);
        g_object_unref(action);
    }

    return;
}

/**
 * A function to delete a view from all verbs.
 *
 * This function first looks through _base_verbs and deteles
 * the view from all of those views.  If \c _verbs is not empty
 * then all of the entries in that table have all of the views
 * deleted also.
 *
 * @param  view  Which view's actions should be removed.
 * @return None
 */
void Verb::delete_all_view(Inkscape::UI::View::View *view)
{
    for (int i = 0; i <= SP_VERB_LAST; i++) {
        if (_base_verbs[i])
          _base_verbs[i]->delete_view(view);
    }

    if (!_verbs.empty()) {
        for (auto & _verb : _verbs) {
            Inkscape::Verb *verbpntr = _verb.second;
            // std::cout << "Delete In Verb: " << verbpntr->_name << std::endl;
            verbpntr->delete_view(view);
        }
    }

    return;
}

/**
 * A function to turn a \c code into a Verb for dynamically created Verbs.
 *
 * This function basically just looks through the \c _verbs hash
 * table.  STL does all the work.
 *
 * @param  code  What code is being looked for.
 * @return The found Verb of NULL if none is found.
 */
Verb *Verb::get_search(unsigned int code)
{
    Verb *verb = nullptr;
    VerbTable::iterator verb_found = _verbs.find(code);

    if (verb_found != _verbs.end()) {
        verb = verb_found->second;
    }

    return verb;
}

/**
 * Find a Verb using it's ID.
 *
 * This function uses the \c _verb_ids has table to find the
 * verb by it's id.  Should be much faster than previous
 * implementations.
 *
 * @param  id  Which id to search for.
 */
Verb *Verb::getbyid(gchar const *id, bool verbose)
{
    Verb *verb = nullptr;
    VerbIDTable::iterator verb_found = _verb_ids.find(id);

    if (verb_found != _verb_ids.end()) {
        verb = verb_found->second;
    }

    if (verb == nullptr
#ifndef WITH_GSPELL
                && strcmp(id, "DialogSpellcheck") != 0
#endif
            ) {
        if (verbose)
            printf("Unable to find: %s\n", id);
    }

    return verb;
}

/**
 * Decode the verb code and take appropriate action.
 */
void FileVerb::perform(SPAction *action, void *data)
{
    // Convert verb impls to use this where possible, to reduce static cling
    // to macros like SP_ACTIVE_DOCUMENT, which end up enforcing GUI-mode operation
    SPDocument *doc = sp_action_get_document(action);

    // We can vacuum defs, or exit, without needing a desktop!
    bool handled = true;
    switch (reinterpret_cast<std::size_t>(data)) {
        case SP_VERB_FILE_VACUUM:
            sp_file_vacuum(doc);
            break;
        case SP_VERB_FILE_QUIT:
            sp_file_exit();
            break;
        default:
            handled = false;
            break;
    }
    if (handled) {
        return;
    }
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    g_return_if_fail(ensure_desktop_valid(action));
    SPDesktop *desktop = sp_action_get_desktop(action);

    Gtk::Window *parent = desktop->getToplevel();
    g_assert(parent != nullptr);

    switch (reinterpret_cast<std::size_t>(data)) {
        case SP_VERB_FILE_NEW:
            sp_file_new_default();
            break;
        case SP_VERB_FILE_OPEN:
            sp_file_open_dialog(*parent, nullptr, nullptr);
            break;
        case SP_VERB_FILE_REVERT:
            sp_file_revert_dialog();
            break;
        case SP_VERB_FILE_SAVE:
            sp_file_save(*parent, nullptr, nullptr);
            break;
        case SP_VERB_FILE_SAVE_AS:
            sp_file_save_as(*parent, nullptr, nullptr);
            break;
        case SP_VERB_FILE_SAVE_A_COPY:
            sp_file_save_a_copy(*parent, nullptr, nullptr);
            break;
        case SP_VERB_FILE_SAVE_TEMPLATE:
            Inkscape::UI::Dialog::SaveTemplate::save_document_as_template(*parent);
            break;
        case SP_VERB_FILE_PRINT:
            sp_file_print(*parent);
            break;
        case SP_VERB_FILE_IMPORT:
            prefs->setBool("/options/onimport",true);
            sp_file_import(*parent);
            prefs->setBool("/options/onimport",false);
            break;
//        case SP_VERB_FILE_EXPORT:
//            sp_file_export_dialog(*parent);
//            break;
        case SP_VERB_FILE_NEXT_DESKTOP:
            INKSCAPE.switch_desktops_next();
            break;
        case SP_VERB_FILE_PREV_DESKTOP:
            INKSCAPE.switch_desktops_prev();
            break;
        case SP_VERB_FILE_CLOSE_VIEW:
            sp_ui_close_view(nullptr);
            break;
        case SP_VERB_FILE_TEMPLATES:
            Inkscape::UI::NewFromTemplate::load_new_from_template();
            break;
        default:
            break;
    }


} // end of sp_verb_action_file_perform()

/**
 * Decode the verb code and take appropriate action.
 */
void EditVerb::perform(SPAction *action, void *data)
{
    // We can clear all without a desktop
    bool handled = true;
    switch (reinterpret_cast<std::size_t>(data)) {
        case SP_VERB_EDIT_CLEAR_ALL:
            sp_edit_clear_all(sp_action_get_selection(action));
            break;
        default:
            handled = false;
            break;
    }
    if (handled) {
        return;
    }

    g_return_if_fail(ensure_desktop_valid(action));
    SPDesktop *dt = sp_action_get_desktop(action);
    Glib::ustring switch_selector_to = get_active_tool(dt);
    switch (reinterpret_cast<std::size_t>(data)) {
        case SP_VERB_EDIT_UNDO:
            // this fix crashes on undo with knots forcing regenerate knots on undo
            if (switch_selector_to == "Node") {
                dt->selection->setBackup();
            }
            if (switch_selector_to != "Select") {
                set_active_tool(dt, "Select");
            }
            sp_undo(dt, dt->getDocument());
            if (switch_selector_to != "Select") {
                set_active_tool(dt, switch_selector_to);
            }
            if (switch_selector_to == "Node") {
                dt->selection->restoreBackup();
                dt->selection->emptyBackup();
            }
            break;
        case SP_VERB_EDIT_REDO:
            // this fix crashes on redo with knots forcing regenerate knots on undo
            if (switch_selector_to == "Node") {
                dt->selection->setBackup();
            }
            if (switch_selector_to != "Select") {
                set_active_tool(dt, "Select");
            }
            sp_redo(dt, dt->getDocument());
            if (switch_selector_to != "Select") {
                set_active_tool(dt, switch_selector_to);
            }
            if (switch_selector_to == "Node") {
                dt->selection->restoreBackup();
                dt->selection->emptyBackup();
            }
            break;
        case SP_VERB_EDIT_CUT:
            dt->selection->cut();
            break;
        case SP_VERB_EDIT_COPY:
            dt->selection->copy();
            break;
        case SP_VERB_EDIT_PASTE:
            sp_selection_paste(dt, false);
            break;
        case SP_VERB_EDIT_PASTE_STYLE:
            dt->selection->pasteStyle();
            break;
        case SP_VERB_EDIT_PASTE_SIZE:
            dt->selection->pasteSize(true,true);
            break;
        case SP_VERB_EDIT_PASTE_SIZE_X:
            dt->selection->pasteSize(true, false);
            break;
        case SP_VERB_EDIT_PASTE_SIZE_Y:
            dt->selection->pasteSize(false, true);
            break;
        case SP_VERB_EDIT_PASTE_SIZE_SEPARATELY:
            dt->selection->pasteSizeSeparately(true, true);
            break;
        case SP_VERB_EDIT_PASTE_SIZE_SEPARATELY_X:
            dt->selection->pasteSizeSeparately(true, false);
            break;
        case SP_VERB_EDIT_PASTE_SIZE_SEPARATELY_Y:
            dt->selection->pasteSizeSeparately(false, true);
            break;
        case SP_VERB_EDIT_PASTE_IN_PLACE:
            sp_selection_paste(dt, true);
            break;
        case SP_VERB_EDIT_PASTE_LIVEPATHEFFECT:
            dt->selection->pastePathEffect();
            break;
        case SP_VERB_EDIT_REMOVE_LIVEPATHEFFECT:
            dt->selection->removeLPE();
            break;
        case SP_VERB_EDIT_REMOVE_FILTER:
            dt->selection->removeFilter();
            break;
        case SP_VERB_EDIT_DELETE:
            dt->selection->deleteItems();
            break;
        case SP_VERB_EDIT_DUPLICATE:
            dt->selection->duplicate();
            break;
        case SP_VERB_EDIT_CLONE:
            dt->selection->clone();
            break;
        case SP_VERB_EDIT_UNLINK_CLONE:
            dt->selection->unlink();
            break;
        case SP_VERB_EDIT_UNLINK_CLONE_RECURSIVE:
            dt->selection->unlinkRecursive(false, true);
            break;
        case SP_VERB_EDIT_RELINK_CLONE:
            dt->selection->relink();
            break;
        case SP_VERB_EDIT_CLONE_SELECT_ORIGINAL:
            dt->selection->cloneOriginal();
            break;
        case SP_VERB_EDIT_CLONE_ORIGINAL_PATH_LPE:
            dt->selection->cloneOriginalPathLPE();
            break;
        case SP_VERB_EDIT_SELECTION_2_MARKER:
            dt->selection->toMarker();
            break;
        case SP_VERB_EDIT_SELECTION_2_GUIDES:
            dt->selection->toGuides();
            break;
        case SP_VERB_EDIT_TILE:
            dt->selection->tile();
            break;
        case SP_VERB_EDIT_UNTILE:
            dt->selection->untile();
            break;
        case SP_VERB_EDIT_SYMBOL:
            dt->selection->toSymbol();
            break;
        case SP_VERB_EDIT_UNSYMBOL:
            dt->selection->unSymbol();
            break;
        case SP_VERB_EDIT_SELECT_ALL:
            SelectionHelper::selectAll(dt);
            break;
        case SP_VERB_EDIT_SELECT_SAME_FILL_STROKE:
            SelectionHelper::selectSameFillStroke(dt);
            break;
        case SP_VERB_EDIT_SELECT_SAME_FILL_COLOR:
            SelectionHelper::selectSameFillColor(dt);
            break;
        case SP_VERB_EDIT_SELECT_SAME_STROKE_COLOR:
            SelectionHelper::selectSameStrokeColor(dt);
            break;
        case SP_VERB_EDIT_SELECT_SAME_STROKE_STYLE:
            SelectionHelper::selectSameStrokeStyle(dt);
            break;
        case SP_VERB_EDIT_SELECT_SAME_OBJECT_TYPE:
            SelectionHelper::selectSameObjectType(dt);
            break;
        case SP_VERB_EDIT_INVERT:
            SelectionHelper::invert(dt);
            break;
        case SP_VERB_EDIT_SELECT_ALL_IN_ALL_LAYERS:
            SelectionHelper::selectAllInAll(dt);
            break;
        case SP_VERB_EDIT_INVERT_IN_ALL_LAYERS:
            SelectionHelper::invertAllInAll(dt);
            break;
        case SP_VERB_EDIT_SELECT_NEXT:
            SelectionHelper::selectNext(dt);
            break;
        case SP_VERB_EDIT_SELECT_PREV:
            SelectionHelper::selectPrev(dt);
            break;
        case SP_VERB_EDIT_DESELECT:
            SelectionHelper::selectNone(dt);
            break;
        case SP_VERB_EDIT_DELETE_ALL_GUIDES:
            sp_guide_delete_all_guides(dt->getDocument());
            break;
        case SP_VERB_EDIT_GUIDES_TOGGLE_LOCK:
            dt->toggleGuidesLock();
            break;
        case SP_VERB_EDIT_GUIDES_AROUND_PAGE:
            sp_guide_create_guides_around_page(dt->getDocument());
            break;
        case SP_VERB_EDIT_NEXT_PATHEFFECT_PARAMETER:
            sp_selection_next_patheffect_param(dt);
            break;
        case SP_VERB_EDIT_SWAP_FILL_STROKE:
            dt->selection->swapFillStroke();
            break;
        case SP_VERB_EDIT_LINK_COLOR_PROFILE:
            break;
        case SP_VERB_EDIT_REMOVE_COLOR_PROFILE:
            break;
        default:
            break;
    }

} // end of sp_verb_action_edit_perform()

/**
 * Decode the verb code and take appropriate action.
 */
void SelectionVerb::perform(SPAction *action, void *data)
{
    Inkscape::Selection *selection = sp_action_get_selection(action);
    SPDesktop *dt = sp_action_get_desktop(action);
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    // Some of these operations have been modified so they work in command-line mode!
    // In this case, all we need is a selection
    if (!selection) {
        return;
    }

    bool handled = true;
    switch (reinterpret_cast<std::size_t>(data)) {
        case SP_VERB_SELECTION_UNION:
            selection->removeLPESRecursive(true);
            selection->unlinkRecursive(true);
            selection->pathUnion();
            break;
        case SP_VERB_SELECTION_INTERSECT:
            selection->removeLPESRecursive(true);
            selection->unlinkRecursive(true);
            selection->pathIntersect();
            break;
        case SP_VERB_SELECTION_DIFF:
            selection->removeLPESRecursive(true);
            selection->unlinkRecursive(true);
            selection->pathDiff();
            break;
        case SP_VERB_SELECTION_SYMDIFF:
            selection->removeLPESRecursive(true);
            selection->unlinkRecursive(true);
            selection->pathSymDiff();
            break;
        case SP_VERB_SELECTION_CUT:
            selection->removeLPESRecursive(true);
            selection->unlinkRecursive(true);
            selection->pathCut();
            break;
        case SP_VERB_SELECTION_SLICE:
            selection->removeLPESRecursive(true);
            selection->unlinkRecursive(true);
            selection->pathSlice();
            break;
        case SP_VERB_SELECTION_GROW:
        {
            // FIXME these and the other grow/shrink they should use gobble_key_events.
            // the problem is how to get access to which key, if any, to gobble.
            selection->scale(prefs->getDoubleLimited("/options/defaultscale/value", 2, 0, 1000));
            break;
        }
        case SP_VERB_SELECTION_GROW_SCREEN:
        {
            selection->scaleScreen(2);
            break;
        }
        case SP_VERB_SELECTION_GROW_DOUBLE:
        {
            selection->scaleTimes(2);
            break;
        }
        case SP_VERB_SELECTION_SHRINK:
        {
            selection->scale(-prefs->getDoubleLimited("/options/defaultscale/value", 2, 0, 1000));
            break;
        }
        case SP_VERB_SELECTION_SHRINK_SCREEN:
        {
            selection->scaleScreen(-2);
            break;
        }
        case SP_VERB_SELECTION_SHRINK_HALVE:
        {
            selection->scaleTimes(0.5);
            break;
        }
        case SP_VERB_SELECTION_TO_FRONT:
            selection->raiseToTop();
            break;
        case SP_VERB_SELECTION_TO_BACK:
            selection->lowerToBottom();
            break;
        case SP_VERB_SELECTION_RAISE:
            selection->raise();
            break;
        case SP_VERB_SELECTION_LOWER:
            selection->lower();
            break;
        case SP_VERB_SELECTION_STACK_UP:
            selection->stackUp();
            break;
        case SP_VERB_SELECTION_STACK_DOWN:
            selection->stackDown();
            break;
        case SP_VERB_SELECTION_GROUP:
            selection->group();
            break;
        case SP_VERB_SELECTION_UNGROUP:
            selection->ungroup();
            break;
        case SP_VERB_SELECTION_UNGROUP_POP_SELECTION:
            selection->popFromGroup();
            break;
        case SP_VERB_SELECTION_FILL_BETWEEN_MANY:
            selection->fillBetweenMany();
            break;
        default:
            handled = false;
            break;
    }

    if (handled) {
        return;
    }

    // The remaining operations require a desktop
    g_return_if_fail(ensure_desktop_valid(action));

    DialogContainer *container = dt->getContainer();

    switch (reinterpret_cast<std::size_t>(data)) {
        case SP_VERB_SELECTION_TEXTTOPATH:
            text_put_on_path();
            break;
        case SP_VERB_SELECTION_TEXTFROMPATH:
            text_remove_from_path();
            break;
        case SP_VERB_SELECTION_REMOVE_KERNS:
            text_remove_all_kerns();
            break;

        case SP_VERB_SELECTION_OFFSET:
            selection->removeLPESRecursive(true);
            selection->unlinkRecursive(true);
            sp_selected_path_offset(dt);
            break;
        case SP_VERB_SELECTION_OFFSET_SCREEN:
            selection->removeLPESRecursive(true);
            selection->unlinkRecursive(true);
            sp_selected_path_offset_screen(dt, 1);
            break;
        case SP_VERB_SELECTION_OFFSET_SCREEN_10:
            selection->removeLPESRecursive(true);
            selection->unlinkRecursive(true);
            sp_selected_path_offset_screen(dt, 10);
            break;
        case SP_VERB_SELECTION_INSET:
            selection->removeLPESRecursive(true);
            selection->unlinkRecursive(true);
            sp_selected_path_inset(dt);
            break;
        case SP_VERB_SELECTION_INSET_SCREEN:
            selection->removeLPESRecursive(true);
            selection->unlinkRecursive(true);
            sp_selected_path_inset_screen(dt, 1);
            break;
        case SP_VERB_SELECTION_INSET_SCREEN_10:
            selection->removeLPESRecursive(true);
            selection->unlinkRecursive(true);
            sp_selected_path_inset_screen(dt, 10);
            break;
        case SP_VERB_SELECTION_DYNAMIC_OFFSET:
            selection->removeLPESRecursive(true);
            selection->unlinkRecursive(true);
            sp_selected_path_create_offset_object_zero(dt);
            set_active_tool(dt, "Node");
            break;
        case SP_VERB_SELECTION_LINKED_OFFSET:
            selection->removeLPESRecursive(true);
            selection->unlinkRecursive(true);
            sp_selected_path_create_updating_offset_object_zero(dt);
            set_active_tool(dt, "Node");
            break;
        case SP_VERB_SELECTION_OUTLINE:
            selection->strokesToPaths();
            break;
        case SP_VERB_SELECTION_OUTLINE_LEGACY:
            selection->strokesToPaths(true);
            break;
        case SP_VERB_SELECTION_SIMPLIFY:
            selection->simplifyPaths();
            break;
        case SP_VERB_SELECTION_REVERSE:
            SelectionHelper::reverse(dt);
            break;
        case SP_VERB_SELECTION_TRACE:
            container->new_dialog("Trace");
            break;
        case SP_VERB_SELECTION_CREATE_BITMAP:
            dt->selection->createBitmapCopy();
            break;

        case SP_VERB_SELECTION_COMBINE:
            selection->unlinkRecursive(true);
            selection->combine();
            break;
        case SP_VERB_SELECTION_BREAK_APART:
            selection->breakApart();
            break;
        default:
            break;
    }

} // end of sp_verb_action_selection_perform()

/**
 * Decode the verb code and take appropriate action.
 */
void LayerVerb::perform(SPAction *action, void *data)
{
    g_return_if_fail(ensure_desktop_valid(action));
    SPDesktop *dt = sp_action_get_desktop(action);
    size_t verb = reinterpret_cast<std::size_t>(data);

    auto layer = dt->layerManager().currentLayer();
    auto root = dt->layerManager().currentRoot();
    if (!layer)
        return;

    switch (verb) {
        case SP_VERB_LAYER_NEW: {
            Inkscape::UI::Dialogs::LayerPropertiesDialog::showCreate(dt, layer);
            break;
        }
        case SP_VERB_LAYER_RENAME: {
            Inkscape::UI::Dialogs::LayerPropertiesDialog::showRename(dt, layer);
            break;
        }
        case SP_VERB_LAYER_NEXT: {
            SPObject *next=Inkscape::next_layer(root, layer);
            if (next) {
                dt->layerManager().setCurrentLayer(next);
                DocumentUndo::done(dt->getDocument(), _("Switch to next layer"), INKSCAPE_ICON("layer-previous")); // Icon backwards!
                dt->messageStack()->flash(Inkscape::NORMAL_MESSAGE, _("Switched to next layer."));
            } else {
                dt->messageStack()->flash(Inkscape::WARNING_MESSAGE, _("Cannot go past last layer."));
            }
            break;
        }
        case SP_VERB_LAYER_PREV: {
            SPObject *prev=Inkscape::previous_layer(root, layer);
            if (prev) {
                dt->layerManager().setCurrentLayer(prev);
                DocumentUndo::done(dt->getDocument(), _("Switch to previous layer"), INKSCAPE_ICON("layer-next")); // Icon backwards!
                dt->messageStack()->flash(Inkscape::NORMAL_MESSAGE, _("Switched to previous layer."));
            } else {
                dt->messageStack()->flash(Inkscape::WARNING_MESSAGE, _("Cannot go before first layer."));
            }
            break;
        }
        case SP_VERB_LAYER_MOVE_TO_NEXT: {
            dt->selection->toNextLayer();
            break;
        }
        case SP_VERB_LAYER_MOVE_TO_PREV: {
            dt->selection->toPrevLayer();
            break;
        }
        case SP_VERB_LAYER_MOVE_TO: {
            Inkscape::UI::Dialogs::LayerPropertiesDialog::showMove(dt, layer);
            break;
        }
        case SP_VERB_LAYER_TO_TOP: {
            if ( layer == root ) {
                dt->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
                return;
            }

            g_return_if_fail(layer != nullptr);
            SPObject *old_pos = layer->getNext();

            layer->raiseToTop();

            if ( layer->getNext() != old_pos ) {
                DocumentUndo::done(dt->getDocument(), _("Layer to top"), INKSCAPE_ICON("layer-top"));

                char const *message = g_strdup_printf(_("Raised layer <b>%s</b>."), layer->defaultLabel());
                if (message) {
                    dt->messageStack()->flash(Inkscape::NORMAL_MESSAGE, message);
                    g_free((void *) message);
                }
            } else {
                dt->messageStack()->flash(Inkscape::WARNING_MESSAGE, _("Cannot move layer any further."));
            }

            break;
        }
        case SP_VERB_LAYER_TO_BOTTOM: {
            if ( layer == root ) {
                dt->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
                return;
            }

            g_return_if_fail(layer != nullptr);
            SPObject *old_pos = layer->getNext();

            layer->lowerToBottom();

            if ( layer->getNext() != old_pos ) {
                DocumentUndo::done(dt->getDocument(), _("Layer to bottom"), INKSCAPE_ICON("layer-bottom"));

                char const *message = g_strdup_printf(_("Lowered layer <b>%s</b>."), layer->defaultLabel());
                if (message) {
                    dt->messageStack()->flash(Inkscape::NORMAL_MESSAGE, message);
                    g_free((void *) message);
                }
            } else {
                dt->messageStack()->flash(Inkscape::WARNING_MESSAGE, _("Cannot move layer any further."));
            }

            break;
        }
        case SP_VERB_LAYER_RAISE: {
            if ( layer == root ) {
                dt->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
                return;
            }

            g_return_if_fail(layer != nullptr);
            SPObject *old_pos = layer->getNext();

            layer->raiseOne();

            if ( layer->getNext() != old_pos ) {
                DocumentUndo::done(dt->getDocument(), _("Raise layer"), INKSCAPE_ICON("layer-raise"));

                char const *message = g_strdup_printf(_("Raised layer <b>%s</b>."), layer->defaultLabel());
                if (message) {
                    dt->messageStack()->flash(Inkscape::NORMAL_MESSAGE, message);
                    g_free((void *) message);
                }
            } else {
                dt->messageStack()->flash(Inkscape::WARNING_MESSAGE, _("Cannot move layer any further."));
            }

            break;
        }
        case SP_VERB_LAYER_LOWER: {
            if ( layer == root ) {
                dt->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
                return;
            }

            g_return_if_fail(layer != nullptr);
            SPObject *old_pos = layer->getNext();

            layer->lowerOne();

            if ( layer->getNext() != old_pos ) {
                DocumentUndo::done(dt->getDocument(), _("Lower layer"), INKSCAPE_ICON("layer-lower"));

                char const *message = g_strdup_printf(_("Lowered layer <b>%s</b>."), layer->defaultLabel());
                if (message) {
                    dt->messageStack()->flash(Inkscape::NORMAL_MESSAGE, message);
                    g_free((void *) message);
                }
            } else {
                dt->messageStack()->flash(Inkscape::WARNING_MESSAGE, _("Cannot move layer any further."));
            }

            break;
        }
        case SP_VERB_LAYER_DUPLICATE: {
            if ( layer != root ) {

                dt->selection->duplicate(true, true);

                DocumentUndo::done(dt->getDocument(), _("Duplicate layer"), INKSCAPE_ICON("layer-duplicate"));

                // TRANSLATORS: this means "The layer has been duplicated."
                dt->messageStack()->flash(Inkscape::NORMAL_MESSAGE, _("Duplicated layer."));
            } else {
                dt->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
            }
            break;
        }
        case SP_VERB_LAYER_DELETE: {
            if ( layer != root ) {
                dt->getSelection()->clear();
                auto old_layer = layer;
                SPObject *old_parent = old_layer->parent;
                SPObject *old_parent_parent = (old_parent != nullptr) ? old_parent->parent : nullptr;

                SPObject *survivor = Inkscape::previous_layer(root, old_layer);
                if (survivor != nullptr && survivor->parent == old_layer) {
                    while (survivor != nullptr &&
                           survivor->parent != old_parent &&
                           survivor->parent != old_parent_parent)
                    {
                        survivor = Inkscape::previous_layer(root, survivor);
                    }
                }

                if (survivor == nullptr || (survivor->parent != old_parent && survivor->parent != old_layer)) {
                    survivor = Inkscape::next_layer(root, old_layer);
                    while (survivor != nullptr &&
                           survivor != old_parent &&
                           survivor->parent != old_parent)
                    {
                        survivor = Inkscape::next_layer(root, survivor);
                    }
                }

                // Deleting the old layer before switching layers is a hack to trigger the
                // listeners of the deletion event (as happens when old_layer is deleted using the
                // xml editor).  See
                // http://sourceforge.net/tracker/index.php?func=detail&aid=1339397&group_id=93438&atid=604306
                //
                old_layer->deleteObject();

                if (survivor) {
                    dt->layerManager().setCurrentLayer(survivor);
                }

                DocumentUndo::done(dt->getDocument(), _("Delete layer"), INKSCAPE_ICON("layer-delete"));

                // TRANSLATORS: this means "The layer has been deleted."
                dt->messageStack()->flash(Inkscape::NORMAL_MESSAGE, _("Deleted layer."));
            } else {
                dt->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
            }
            break;
        }
        case SP_VERB_LAYER_SOLO: {
            if ( layer == root ) {
                dt->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
            } else {
                dt->layerManager().toggleLayerSolo( layer );
                DocumentUndo::done(dt->getDocument(), _("Toggle layer solo"), "");
            }
            break;
        }
        case SP_VERB_LAYER_SHOW_ALL: {
            dt->layerManager().toggleHideAllLayers( false );
            DocumentUndo::maybeDone(dt->getDocument(), "layer:showall", _("Show all layers"), "");
            break;
        }
        case SP_VERB_LAYER_HIDE_ALL: {
            dt->layerManager().toggleHideAllLayers( true );
            DocumentUndo::maybeDone(dt->getDocument(), "layer:hideall", _("Hide all layers"), "");
            break;
        }
        case SP_VERB_LAYER_LOCK_ALL: {
            dt->layerManager().toggleLockAllLayers( true );
            DocumentUndo::maybeDone(dt->getDocument(), "layer:lockall", _("Lock all layers"), "");
            break;
        }
        case SP_VERB_LAYER_LOCK_OTHERS: {
            if ( layer == root ) {
                dt->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
            } else {
                dt->layerManager().toggleLockOtherLayers( layer );
                DocumentUndo::done(dt->getDocument(), _("Lock other layers"), "");
            }
            break;
        }
        case SP_VERB_LAYER_UNLOCK_ALL: {
            dt->layerManager().toggleLockAllLayers( false );
            DocumentUndo::maybeDone(dt->getDocument(), "layer:unlockall", _("Unlock all layers"), "");
            break;
        }
        case SP_VERB_LAYER_TOGGLE_LOCK:
        case SP_VERB_LAYER_TOGGLE_HIDE: {
            if ( layer == root ) {
                dt->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("No current layer."));
            } else {
                if ( verb == SP_VERB_LAYER_TOGGLE_HIDE ){
                    layer->setHidden(!layer->isHidden());
                } else {
                    layer->setLocked(!layer->isLocked());
                }

            }
        }
    }

    return;
} // end of sp_verb_action_layer_perform()

/**
 * Decode the verb code and take appropriate action.
 */
void ObjectVerb::perform( SPAction *action, void *data)
{
    SPDesktop *dt = sp_action_get_desktop(action);
    Inkscape::Selection *sel = sp_action_get_selection(action);

    // We can perform some actions without a desktop
    bool handled = true;
    switch (reinterpret_cast<std::size_t>(data)) {
        case SP_VERB_OBJECT_TO_CURVE:
            sel->toCurves();
            break;
        default:
            handled = false;
            break;
    }
    if (handled) {
        return;
    }

    g_return_if_fail(ensure_desktop_valid(action));

    Inkscape::UI::Tools::ToolBase *ec = dt->event_context;

    if (sel->isEmpty())
        return;

    Geom::OptRect bbox = sel->visualBounds();
    if (!bbox) {
        return;
    }

    // If the rotation center of the selection is visible, choose it as reference point
    // for horizontal and vertical flips. Otherwise, take the center of the bounding box.
    Geom::Point center;
    if (dynamic_cast<Inkscape::UI::Tools::SelectTool *>(ec) &&
        sel->center()                                                      &&
        SP_SELECT_CONTEXT(ec)->_seltrans->centerIsVisible()                ) {
        center = *sel->center();
    } else {
        center = bbox->midpoint();
    }

    switch (reinterpret_cast<std::size_t>(data)) {
        case SP_VERB_OBJECT_ROTATE_90_CW:
            sel->rotate90(false);
            break;
        case SP_VERB_OBJECT_ROTATE_90_CCW:
            sel->rotate90(true);
            break;
        case SP_VERB_OBJECT_FLATTEN:
            sel->removeTransform();
            break;
        case SP_VERB_OBJECT_FLOW_TEXT:
            text_flow_into_shape();
            break;
        case SP_VERB_OBJECT_FLOW_SUBTRACT:
            text_flow_shape_subtract();
            break;
        case SP_VERB_OBJECT_UNFLOW_TEXT:
            text_unflow();
            break;
        case SP_VERB_OBJECT_FLOWTEXT_TO_TEXT:
            flowtext_to_text();
            break;
        case SP_VERB_OBJECT_FLIP_HORIZONTAL:
            sel->setScaleRelative(center, Geom::Scale(-1.0, 1.0));
            DocumentUndo::done(dt->getDocument(), _("Flip horizontally"), INKSCAPE_ICON("object-flip-horizontal"));
            break;
        case SP_VERB_OBJECT_FLIP_VERTICAL:
            sel->setScaleRelative(center, Geom::Scale(1.0, -1.0));
            DocumentUndo::done(dt->getDocument(), _("Flip vertically"), INKSCAPE_ICON("object-flip-vertical"));
            break;
        case SP_VERB_OBJECT_SET_MASK:
            sel->setMask(false, false);
            break;
        case SP_VERB_OBJECT_SET_INVERSE_MASK:
            sel->setMask(false, false);
            Inkscape::LivePathEffect::sp_inverse_powermask(sp_action_get_selection(action));
            DocumentUndo::done(dt->getDocument(), _("_Set Inverse (LPE)"), "");
            break;
        case SP_VERB_OBJECT_EDIT_MASK:
            sel->editMask(false);
            break;
        case SP_VERB_OBJECT_UNSET_MASK:
            Inkscape::LivePathEffect::sp_remove_powermask(sp_action_get_selection(action));
            sel->unsetMask(false);
            DocumentUndo::done(dt->getDocument(), _("Release mask"), "");
            break;
        case SP_VERB_OBJECT_SET_CLIPPATH:
            sel->setMask(true, false);
            break;
        case SP_VERB_OBJECT_SET_INVERSE_CLIPPATH:
            sel->setMask(true, false);
            Inkscape::LivePathEffect::sp_inverse_powerclip(sp_action_get_selection(action));
            DocumentUndo::done(dt->getDocument(), _("_Set Inverse (LPE)"), "");
            break;
        case SP_VERB_OBJECT_CREATE_CLIP_GROUP:
            sel->setClipGroup();
            break;
        case SP_VERB_OBJECT_EDIT_CLIPPATH:
            sel->editMask(true);
            break;
        case SP_VERB_OBJECT_UNSET_CLIPPATH:
            Inkscape::LivePathEffect::sp_remove_powerclip(sp_action_get_selection(action));
            sel->unsetMask(true);
            DocumentUndo::done(dt->getDocument(), _("Release clipping path"), "");

            break;
        default:
            break;
    }

} // end of sp_verb_action_object_perform()

/**
 * Decode the verb code and take appropriate action.
 */
void TextVerb::perform(SPAction *action, void */*data*/)
{
    g_return_if_fail(ensure_desktop_valid(action));
    SPDesktop *dt = sp_action_get_desktop(action);

    SPDocument *doc = dt->getDocument();
    (void)doc;
    Inkscape::XML::Node *repr = dt->namedview->getRepr();
    (void)repr;
}


// *********** Effect Last **********

/**
 * A class to represent the last effect issued.
 */
class EffectLastVerb : public Verb {
private:
    static void perform(SPAction *action, void *mydata);
protected:
    SPAction *make_action(Inkscape::ActionContext const & context) override;
public:
    /** Use the Verb initializer with the same parameters. */
    EffectLastVerb(unsigned int const code,
                   gchar const *id,
                   gchar const *name,
                   gchar const *tip,
                   gchar const *image) :
        Verb(code, id, name, tip, image, _("Extensions"))
    {
        set_default_sensitive(false);
    }
}; // EffectLastVerb class

/**
 * Create an action for a \c EffectLastVerb.
 *
 * Calls \c make_action_helper with the \c vector.
 *
 * @param  context  Which context the action should be created for.
 * @return The built action.
 */
SPAction *EffectLastVerb::make_action(Inkscape::ActionContext const & context)
{
    return make_action_helper(context, &perform);
}

/**
 * Decode the verb code and take appropriate action.
 */
void EffectLastVerb::perform(SPAction *action, void *data)
{
    g_return_if_fail(ensure_desktop_valid(action));
    Inkscape::UI::View::View *current_view = sp_action_get_view(action);

    Inkscape::Extension::Effect *effect = Inkscape::Extension::Effect::get_last_effect();

    if (effect == nullptr) return;

    switch (reinterpret_cast<std::size_t>(data)) {
        case SP_VERB_EFFECT_LAST_PREF:
            effect->prefs(current_view);
            break;
        case SP_VERB_EFFECT_LAST:
            effect->effect(current_view);
            break;
        default:
            return;
    }

    return;
}
// *********** End Effect Last **********

// *********** Fit Canvas **********

/**
 * A class to represent the canvas fitting verbs.
 */
class FitCanvasVerb : public Verb {
private:
    static void perform(SPAction *action, void *mydata);
protected:
    SPAction *make_action(Inkscape::ActionContext const & context) override;
public:
    /** Use the Verb initializer with the same parameters. */
    FitCanvasVerb(unsigned int const code,
                   gchar const *id,
                   gchar const *name,
                   gchar const *tip,
                   gchar const *image) :
        Verb(code, id, name, tip, image, _("View"))
    { }
}; // FitCanvasVerb class

/**
 * Create an action for a \c FitCanvasVerb.
 *
 * Calls \c make_action_helper with the \c vector.
 *
 * @param  context  Which context the action should be created for.
 * @return The built action.
 */
SPAction *FitCanvasVerb::make_action(Inkscape::ActionContext const & context)
{
    SPAction *action = make_action_helper(context, &perform);
    return action;
}

/**
 * Decode the verb code and take appropriate action.
 */
void FitCanvasVerb::perform(SPAction *action, void *data)
{
    g_return_if_fail(ensure_desktop_valid(action));
    SPDesktop *dt = sp_action_get_desktop(action);
    SPDocument *doc = dt->getDocument();
    if (!doc) return;

    switch (reinterpret_cast<std::size_t>(data)) {
        case SP_VERB_FIT_CANVAS_TO_SELECTION:
            dt->selection->fitCanvas(true);
            break;
        case SP_VERB_FIT_CANVAS_TO_DRAWING:
            fit_canvas_to_drawing(dt);
            break;
        default:
            return;
    }

    return;
}
// *********** End Fit Canvas **********


// *********** Lock'N'Hide **********

/**
 * A class to represent the object unlocking and unhiding verbs.
 */
class LockAndHideVerb : public Verb {
private:
    static void perform(SPAction *action, void *mydata);
protected:
    SPAction *make_action(Inkscape::ActionContext const & context) override;
public:
    /** Use the Verb initializer with the same parameters. */
    LockAndHideVerb(unsigned int const code,
                   gchar const *id,
                   gchar const *name,
                   gchar const *tip,
                   gchar const *image) :
        Verb(code, id, name, tip, image, _("Layer"))
    {
        set_default_sensitive(true);
    }
}; // LockAndHideVerb class

/**
 * Create an action for a \c LockAndHideVerb.
 *
 * Calls \c make_action_helper with the \c vector.
 *
 * @param  context  Which context the action should be created for.
 * @return The built action.
 */
SPAction *LockAndHideVerb::make_action(Inkscape::ActionContext const & context)
{
    SPAction *action = make_action_helper(context, &perform);
    return action;
}

/**
 * Decode the verb code and take appropriate action.
 */
void LockAndHideVerb::perform(SPAction *action, void *data)
{
    g_return_if_fail(ensure_desktop_valid(action));
    SPDesktop *dt = sp_action_get_desktop(action);
    SPDocument *doc = dt->getDocument();
    if (!doc) return;

    switch (reinterpret_cast<std::size_t>(data)) {
        case SP_VERB_UNLOCK_ALL:
            unlock_all(dt);
            DocumentUndo::done(doc, _("Unlock all objects in the current layer"), "");
            break;
        case SP_VERB_UNLOCK_ALL_IN_ALL_LAYERS:
            unlock_all_in_all_layers(dt);
            DocumentUndo::done(doc, _("Unlock all objects in all layers"), "");
            break;
        case SP_VERB_UNHIDE_ALL:
            unhide_all(dt);
            DocumentUndo::done(doc, _("Unhide all objects in the current layer"), "");
            break;
        case SP_VERB_UNHIDE_ALL_IN_ALL_LAYERS:
            unhide_all_in_all_layers(dt);
            DocumentUndo::done(doc, _("Unhide all objects in all layers"), "");
            break;
        default:
            return;
    }

    return;
}
// *********** End Lock'N'Hide **********


// these must be in the same order as the SP_VERB_* enum in "verbs.h"
Verb *Verb::_base_verbs[] = {
    // Header
    new Verb(SP_VERB_INVALID, nullptr, nullptr, nullptr, nullptr, nullptr),
    new Verb(SP_VERB_NONE, "None", NC_("Verb", "None"), N_("Does nothing"), nullptr, nullptr),

    // File
    new FileVerb(SP_VERB_FILE_NEW, "FileNew", N_("_New"), N_("Create new document from the default template"),
                 INKSCAPE_ICON("document-new")),
    new FileVerb(SP_VERB_FILE_OPEN, "FileOpen", N_("_Open..."), N_("Open an existing document"),
                 INKSCAPE_ICON("document-open")),
    new FileVerb(SP_VERB_FILE_REVERT, "FileRevert", N_("Re_vert"),
                 N_("Revert to the last saved version of document (changes will be lost)"),
                 INKSCAPE_ICON("document-revert")),
    new FileVerb(SP_VERB_FILE_SAVE, "FileSave", N_("_Save"), N_("Save document"), INKSCAPE_ICON("document-save")),
    new FileVerb(SP_VERB_FILE_SAVE_AS, "FileSaveAs", N_("Save _As..."), N_("Save document under a new name"),
                 INKSCAPE_ICON("document-save-as")),
    new FileVerb(SP_VERB_FILE_SAVE_A_COPY, "FileSaveACopy", N_("Save a Cop_y..."),
                 N_("Save a copy of the document under a new name"), nullptr),
    new FileVerb(SP_VERB_FILE_SAVE_TEMPLATE, "FileSaveTemplate", N_("Save Template..."),
                 N_("Save a copy of the document as template"), nullptr),
    new FileVerb(SP_VERB_FILE_PRINT, "FilePrint", N_("_Print..."), N_("Print document"),
                 INKSCAPE_ICON("document-print")),
    // TRANSLATORS: "Vacuum Defs" means "Clean up defs" (so as to remove unused definitions)
    new FileVerb(
        SP_VERB_FILE_VACUUM, "FileVacuum", N_("Clean _Up Document"),
        N_("Remove unused definitions (such as gradients or clipping paths) from the &lt;defs&gt; of the document"),
        INKSCAPE_ICON("document-cleanup")),
    new FileVerb(SP_VERB_FILE_IMPORT, "FileImport", N_("_Import..."),
                 N_("Import a bitmap or SVG image into this document"), INKSCAPE_ICON("document-import")),
    //    new FileVerb(SP_VERB_FILE_EXPORT, "FileExport", N_("_Export Bitmap..."), N_("Export this document or a
    //    selection as a bitmap image"), INKSCAPE_ICON("document-export")),
    new FileVerb(SP_VERB_FILE_NEXT_DESKTOP, "NextWindow", N_("N_ext Window"), N_("Switch to the next document window"),
                 INKSCAPE_ICON("window-next")),
    new FileVerb(SP_VERB_FILE_PREV_DESKTOP, "PrevWindow", N_("P_revious Window"),
                 N_("Switch to the previous document window"), INKSCAPE_ICON("window-previous")),
    new FileVerb(SP_VERB_FILE_CLOSE_VIEW, "FileClose", N_("_Close"), N_("Close this document window"),
                 INKSCAPE_ICON("window-close")),
    new FileVerb(SP_VERB_FILE_QUIT, "FileQuit", N_("_Quit"), N_("Quit Inkscape"), INKSCAPE_ICON("application-exit")),
    new FileVerb(SP_VERB_FILE_TEMPLATES, "FileTemplates", N_("New from _Template..."),
                 N_("Create new project from template"), INKSCAPE_ICON("dialog-templates")),

    // Edit
    new EditVerb(SP_VERB_EDIT_UNDO, "EditUndo", N_("_Undo"), N_("Undo last action"), INKSCAPE_ICON("edit-undo")),
    new EditVerb(SP_VERB_EDIT_REDO, "EditRedo", N_("_Redo"), N_("Do again the last undone action"),
                 INKSCAPE_ICON("edit-redo")),
    new EditVerb(SP_VERB_EDIT_CUT, "EditCut", N_("Cu_t"), N_("Cut selection to clipboard"), INKSCAPE_ICON("edit-cut")),
    new EditVerb(SP_VERB_EDIT_COPY, "EditCopy", N_("_Copy"), N_("Copy selection to clipboard"),
                 INKSCAPE_ICON("edit-copy")),
    new EditVerb(SP_VERB_EDIT_PASTE, "EditPaste", N_("_Paste"),
                 N_("Paste objects from clipboard to mouse point, or paste text"), INKSCAPE_ICON("edit-paste")),
    new EditVerb(SP_VERB_EDIT_PASTE_STYLE, "EditPasteStyle", N_("Paste _Style"),
                 N_("Apply the style of the copied object to selection"), INKSCAPE_ICON("edit-paste-style")),
    new EditVerb(SP_VERB_EDIT_PASTE_SIZE, "EditPasteSize", N_("Paste Si_ze"),
                 N_("Scale selection to match the size of the copied object"), INKSCAPE_ICON("edit-paste-size")),
    new EditVerb(SP_VERB_EDIT_PASTE_SIZE_X, "EditPasteWidth", N_("Paste _Width"),
                 N_("Scale selection horizontally to match the width of the copied object"),
                 INKSCAPE_ICON("edit-paste-width")),
    new EditVerb(SP_VERB_EDIT_PASTE_SIZE_Y, "EditPasteHeight", N_("Paste _Height"),
                 N_("Scale selection vertically to match the height of the copied object"),
                 INKSCAPE_ICON("edit-paste-height")),
    new EditVerb(SP_VERB_EDIT_PASTE_SIZE_SEPARATELY, "EditPasteSizeSeparately", N_("Paste Size Separately"),
                 N_("Scale each selected object to match the size of the copied object"),
                 INKSCAPE_ICON("edit-paste-size-separately")),
    new EditVerb(SP_VERB_EDIT_PASTE_SIZE_SEPARATELY_X, "EditPasteWidthSeparately", N_("Paste Width Separately"),
                 N_("Scale each selected object horizontally to match the width of the copied object"),
                 INKSCAPE_ICON("edit-paste-width-separately")),
    new EditVerb(SP_VERB_EDIT_PASTE_SIZE_SEPARATELY_Y, "EditPasteHeightSeparately", N_("Paste Height Separately"),
                 N_("Scale each selected object vertically to match the height of the copied object"),
                 INKSCAPE_ICON("edit-paste-height-separately")),
    new EditVerb(SP_VERB_EDIT_PASTE_IN_PLACE, "EditPasteInPlace", N_("Paste _In Place"),
                 N_("Paste objects from clipboard to the original location"), INKSCAPE_ICON("edit-paste-in-place")),
    new EditVerb(SP_VERB_EDIT_PASTE_LIVEPATHEFFECT, "PasteLivePathEffect", N_("Paste Path _Effect"),
                 N_("Apply the path effect of the copied object to selection"), nullptr),
    new EditVerb(SP_VERB_EDIT_REMOVE_LIVEPATHEFFECT, "RemoveLivePathEffect", N_("Remove Path _Effect"),
                 N_("Remove any path effects from selected objects"), nullptr),
    new EditVerb(SP_VERB_EDIT_REMOVE_FILTER, "RemoveFilter", N_("_Remove Filters"),
                 N_("Remove any filters from selected objects"), nullptr),
    new EditVerb(SP_VERB_EDIT_DELETE, "EditDelete", N_("_Delete"), N_("Delete selection"),
                 INKSCAPE_ICON("edit-delete")),
    new EditVerb(SP_VERB_EDIT_DUPLICATE, "EditDuplicate", N_("Duplic_ate"), N_("Duplicate Selected Objects"),
                 INKSCAPE_ICON("edit-duplicate")),
    new EditVerb(SP_VERB_EDIT_CLONE, "EditClone", N_("Create Clo_ne"),
                 N_("Create a clone (a copy linked to the original) of selected object"), INKSCAPE_ICON("edit-clone")),
    new EditVerb(SP_VERB_EDIT_UNLINK_CLONE, "EditUnlinkClone", N_("Unlin_k Clone"),
                 N_("Cut the selected clones' links to the originals, turning them into standalone objects"),
                 INKSCAPE_ICON("edit-clone-unlink")),
    new EditVerb(SP_VERB_EDIT_UNLINK_CLONE_RECURSIVE, "EditUnlinkCloneRecursive", N_("Unlink Clones _recursively"),
                 N_("Unlink all clones in the selection, even if they are in groups."),
                 INKSCAPE_ICON("edit-clone-unlink")),
    new EditVerb(SP_VERB_EDIT_RELINK_CLONE, "EditRelinkClone", N_("Relink to Copied"),
                 N_("Relink the selected clones to the object currently on the clipboard"),
                 INKSCAPE_ICON("edit-clone-link")),
    new EditVerb(SP_VERB_EDIT_CLONE_SELECT_ORIGINAL, "EditCloneSelectOriginal", N_("Select _Original"),
                 N_("Select the object to which the selected clone is linked"), INKSCAPE_ICON("edit-select-original")),
    new EditVerb(SP_VERB_EDIT_CLONE_ORIGINAL_PATH_LPE, "EditCloneOriginalPathLPE", N_("Clone original path (LPE)"),
                 N_("Creates a new path, applies the Clone original LPE, and refers it to the selected path"),
                 INKSCAPE_ICON("edit-clone-link-lpe")),
    new EditVerb(SP_VERB_EDIT_SELECTION_2_MARKER, "ObjectsToMarker", N_("Objects to _Marker"),
                 N_("Convert selection to a line marker"), nullptr),
    new EditVerb(SP_VERB_EDIT_SELECTION_2_GUIDES, "ObjectsToGuides", N_("Objects to Gu_ides"),
                 N_("Convert selected objects to a collection of guidelines aligned with their edges"), nullptr),
    new EditVerb(SP_VERB_EDIT_TILE, "ObjectsToPattern", N_("Objects to Patter_n"),
                 N_("Convert selection to a rectangle with tiled pattern fill"), nullptr),
    new EditVerb(SP_VERB_EDIT_UNTILE, "ObjectsFromPattern", N_("Pattern to _Objects"),
                 N_("Extract objects from a tiled pattern fill"), nullptr),
    new EditVerb(SP_VERB_EDIT_SYMBOL, "ObjectsToSymbol", N_("Group to Symbol"), N_("Convert group to a symbol"),
                 nullptr),
    new EditVerb(SP_VERB_EDIT_UNSYMBOL, "ObjectsFromSymbol", N_("Symbol to Group"), N_("Extract group from a symbol"),
                 nullptr),
    new EditVerb(SP_VERB_EDIT_CLEAR_ALL, "EditClearAll", N_("Clea_r All"), N_("Delete all objects from document"),
                 nullptr),
    new EditVerb(SP_VERB_EDIT_SELECT_ALL, "EditSelectAll", N_("Select Al_l"), N_("Select all objects or all nodes"),
                 INKSCAPE_ICON("edit-select-all")),
    new EditVerb(SP_VERB_EDIT_SELECT_ALL_IN_ALL_LAYERS, "EditSelectAllInAllLayers", N_("Select All in All La_yers"),
                 N_("Select all objects in all visible and unlocked layers"), INKSCAPE_ICON("edit-select-all-layers")),
    new EditVerb(SP_VERB_EDIT_SELECT_SAME_FILL_STROKE, "EditSelectSameFillStroke", N_("Fill _and Stroke"),
                 N_("Select all objects with the same fill and stroke as the selected objects"),
                 INKSCAPE_ICON("edit-select-same-fill-and-stroke")),
    new EditVerb(SP_VERB_EDIT_SELECT_SAME_FILL_COLOR, "EditSelectSameFillColor", N_("_Fill Color"),
                 N_("Select all objects with the same fill as the selected objects"),
                 INKSCAPE_ICON("edit-select-same-fill")),
    new EditVerb(SP_VERB_EDIT_SELECT_SAME_STROKE_COLOR, "EditSelectSameStrokeColor", N_("_Stroke Color"),
                 N_("Select all objects with the same stroke as the selected objects"),
                 INKSCAPE_ICON("edit-select-same-stroke-color")),
    new EditVerb(SP_VERB_EDIT_SELECT_SAME_STROKE_STYLE, "EditSelectSameStrokeStyle", N_("Stroke St_yle"),
                 N_("Select all objects with the same stroke style (width, dash, markers) as the selected objects"),
                 INKSCAPE_ICON("edit-select-same-stroke-style")),
    new EditVerb(
        SP_VERB_EDIT_SELECT_SAME_OBJECT_TYPE, "EditSelectSameObjectType", N_("_Object Type"),
        N_("Select all objects with the same object type (rect, arc, text, path, bitmap etc) as the selected objects"),
        INKSCAPE_ICON("edit-select-same-object-type")),
    new EditVerb(SP_VERB_EDIT_INVERT, "EditInvert", N_("In_vert Selection"),
                 N_("Invert selection (unselect what is selected and select everything else)"),
                 INKSCAPE_ICON("edit-select-invert")),
    new EditVerb(SP_VERB_EDIT_INVERT_IN_ALL_LAYERS, "EditInvertInAllLayers", N_("Invert in All Layers"),
                 N_("Invert selection in all visible and unlocked layers"), nullptr),
    new EditVerb(SP_VERB_EDIT_SELECT_NEXT, "EditSelectNext", N_("Select Next"), N_("Select next object or node"),
                 nullptr),
    new EditVerb(SP_VERB_EDIT_SELECT_PREV, "EditSelectPrev", N_("Select Previous"),
                 N_("Select previous object or node"), nullptr),
    new EditVerb(SP_VERB_EDIT_DESELECT, "EditDeselect", N_("D_eselect"), N_("Deselect any selected objects or nodes"),
                 INKSCAPE_ICON("edit-select-none")),
    new EditVerb(SP_VERB_EDIT_DELETE_ALL_GUIDES, "EditRemoveAllGuides", N_("Delete All Guides"),
                 N_("Delete all the guides in the document"), nullptr),
    new EditVerb(SP_VERB_EDIT_GUIDES_TOGGLE_LOCK, "EditGuidesToggleLock", N_("Lock All Guides"),
                 N_("Toggle lock of all guides in the document"), nullptr),
    new EditVerb(SP_VERB_EDIT_GUIDES_AROUND_PAGE, "EditGuidesAroundPage", N_("Create _Guides Around the Page"),
                 N_("Create four guides aligned with the page borders"), nullptr),
    new EditVerb(SP_VERB_EDIT_NEXT_PATHEFFECT_PARAMETER, "EditNextPathEffectParameter",
                 N_("Next path effect parameter"), N_("Show next editable path effect parameter"),
                 INKSCAPE_ICON("path-effect-parameter-next")),
    new EditVerb(SP_VERB_EDIT_SWAP_FILL_STROKE, "EditSwapFillStroke", N_("Swap fill and stroke"),
                 N_("Swap fill and stroke of an object"), nullptr),

    // Selection
    new SelectionVerb(SP_VERB_SELECTION_TO_FRONT, "SelectionToFront", N_("Raise to _Top"), N_("Raise selection to top"),
                      INKSCAPE_ICON("selection-top")),
    new SelectionVerb(SP_VERB_SELECTION_TO_BACK, "SelectionToBack", N_("Lower to _Bottom"),
                      N_("Lower selection to bottom"), INKSCAPE_ICON("selection-bottom")),
    new SelectionVerb(SP_VERB_SELECTION_RAISE, "SelectionRaise", N_("_Raise"), N_("Raise selection one step"),
                      INKSCAPE_ICON("selection-raise")),
    new SelectionVerb(SP_VERB_SELECTION_LOWER, "SelectionLower", N_("_Lower"), N_("Lower selection one step"),
                      INKSCAPE_ICON("selection-lower")),

    new SelectionVerb(SP_VERB_SELECTION_STACK_UP, "SelectionStackUp", N_("_Stack up"),
                      N_("Stack selection one step up"), INKSCAPE_ICON("layer-raise")),
    new SelectionVerb(SP_VERB_SELECTION_STACK_DOWN, "SelectionStackDown", N_("_Stack down"),
                      N_("Stack selection one step down"), INKSCAPE_ICON("layer-lower")),

    new SelectionVerb(SP_VERB_SELECTION_GROUP, "SelectionGroup", N_("_Group"), N_("Group selected objects"),
                      INKSCAPE_ICON("object-group")),
    new SelectionVerb(SP_VERB_SELECTION_UNGROUP, "SelectionUnGroup", N_("_Ungroup"), N_("Ungroup selected groups"),
                      INKSCAPE_ICON("object-ungroup")),
    new SelectionVerb(SP_VERB_SELECTION_UNGROUP_POP_SELECTION, "SelectionUnGroupPopSelection",
                      N_("_Pop Selected Objects out of Group"), N_("Pop selected objects out of group"),
                      INKSCAPE_ICON("object-ungroup-pop-selection")),

    new SelectionVerb(SP_VERB_SELECTION_TEXTTOPATH, "SelectionTextToPath", N_("_Put on Path"), N_("Put text on path"),
                      INKSCAPE_ICON("text-put-on-path")),
    new SelectionVerb(SP_VERB_SELECTION_TEXTFROMPATH, "SelectionTextFromPath", N_("_Remove from Path"),
                      N_("Remove text from path"), INKSCAPE_ICON("text-remove-from-path")),
    new SelectionVerb(SP_VERB_SELECTION_REMOVE_KERNS, "SelectionTextRemoveKerns", N_("Remove Manual _Kerns"),
                      // TRANSLATORS: "glyph": An image used in the visual representation of characters;
                      //  roughly speaking, how a character looks. A font is a set of glyphs.
                      N_("Remove all manual kerns and glyph rotations from a text object"),
                      INKSCAPE_ICON("text-unkern")),

    new SelectionVerb(SP_VERB_SELECTION_UNION, "SelectionUnion", N_("_Union"), N_("Create union of selected paths"),
                      INKSCAPE_ICON("path-union")),
    new SelectionVerb(SP_VERB_SELECTION_INTERSECT, "SelectionIntersect", N_("_Intersection"),
                      N_("Create intersection of selected paths"), INKSCAPE_ICON("path-intersection")),
    new SelectionVerb(SP_VERB_SELECTION_DIFF, "SelectionDiff", N_("_Difference"),
                      N_("Create difference of selected paths (bottom minus top)"), INKSCAPE_ICON("path-difference")),
    new SelectionVerb(SP_VERB_SELECTION_SYMDIFF, "SelectionSymDiff", N_("E_xclusion"),
                      N_("Create exclusive OR of selected paths (those parts that belong to only one path)"),
                      INKSCAPE_ICON("path-exclusion")),
    new SelectionVerb(SP_VERB_SELECTION_CUT, "SelectionDivide", N_("Di_vision"), N_("Cut the bottom path into pieces"),
                      INKSCAPE_ICON("path-division")),
    // TRANSLATORS: "to cut a path" is not the same as "to break a path apart" - see the
    // Advanced tutorial for more info
    new SelectionVerb(SP_VERB_SELECTION_SLICE, "SelectionCutPath", N_("Cut _Path"),
                      N_("Cut the bottom path's stroke into pieces, removing fill"), INKSCAPE_ICON("path-cut")),
    new SelectionVerb(SP_VERB_SELECTION_GROW, "SelectionGrow", N_("_Grow"), N_("Make selected objects bigger"),
                      INKSCAPE_ICON("selection-grow")),
    new SelectionVerb(SP_VERB_SELECTION_GROW_SCREEN, "SelectionGrowScreen", N_("_Grow on screen"),
                      N_("Make selected objects bigger relative to screen"), INKSCAPE_ICON("selection-grow-screen")),
    new SelectionVerb(SP_VERB_SELECTION_GROW_DOUBLE, "SelectionGrowDouble", N_("_Double size"),
                      N_("Double the size of selected objects"), INKSCAPE_ICON("selection-grow-double")),
    new SelectionVerb(SP_VERB_SELECTION_SHRINK, "SelectionShrink", N_("_Shrink"), N_("Make selected objects smaller"),
                      INKSCAPE_ICON("selection-shrink")),
    new SelectionVerb(SP_VERB_SELECTION_SHRINK_SCREEN, "SelectionShrinkScreen", N_("_Shrink on screen"),
                      N_("Make selected objects smaller relative to screen"), INKSCAPE_ICON("selection-shrink-screen")),
    new SelectionVerb(SP_VERB_SELECTION_SHRINK_HALVE, "SelectionShrinkHalve", N_("_Halve size"),
                      N_("Halve the size of selected objects"), INKSCAPE_ICON("selection-shrink-halve")),
    // TRANSLATORS: "outset": expand a shape by offsetting the object's path,
    // i.e. by displacing it perpendicular to the path in each point.
    // See also the Advanced Tutorial for explanation.
    new SelectionVerb(SP_VERB_SELECTION_OFFSET, "SelectionOffset", N_("Outs_et"), N_("Outset selected paths"),
                      INKSCAPE_ICON("path-outset")),
    new SelectionVerb(SP_VERB_SELECTION_OFFSET_SCREEN, "SelectionOffsetScreen", N_("O_utset Path by 1 px"),
                      N_("Outset selected paths by 1 px"), nullptr),
    new SelectionVerb(SP_VERB_SELECTION_OFFSET_SCREEN_10, "SelectionOffsetScreen10", N_("O_utset Path by 10 px"),
                      N_("Outset selected paths by 10 px"), nullptr),
    // TRANSLATORS: "inset": contract a shape by offsetting the object's path,
    // i.e. by displacing it perpendicular to the path in each point.
    // See also the Advanced Tutorial for explanation.
    new SelectionVerb(SP_VERB_SELECTION_INSET, "SelectionInset", N_("I_nset"), N_("Inset selected paths"),
                      INKSCAPE_ICON("path-inset")),
    new SelectionVerb(SP_VERB_SELECTION_INSET_SCREEN, "SelectionInsetScreen", N_("I_nset Path by 1 px"),
                      N_("Inset selected paths by 1 px"), nullptr),
    new SelectionVerb(SP_VERB_SELECTION_INSET_SCREEN_10, "SelectionInsetScreen10", N_("I_nset Path by 10 px"),
                      N_("Inset selected paths by 10 px"), nullptr),
    new SelectionVerb(SP_VERB_SELECTION_DYNAMIC_OFFSET, "SelectionDynOffset", N_("D_ynamic Offset"),
                      N_("Create a dynamic offset object"), INKSCAPE_ICON("path-offset-dynamic")),
    new SelectionVerb(SP_VERB_SELECTION_LINKED_OFFSET, "SelectionLinkedOffset", N_("_Linked Offset"),
                      N_("Create a dynamic offset object linked to the original path"),
                      INKSCAPE_ICON("path-offset-linked")),
    new SelectionVerb(SP_VERB_SELECTION_OUTLINE, "StrokeToPath", N_("_Stroke to Path"),
                      N_("Convert selected object's stroke to paths"), INKSCAPE_ICON("stroke-to-path")),
    new SelectionVerb(SP_VERB_SELECTION_OUTLINE_LEGACY, "StrokeToPathLegacy", N_("_Stroke to Path Legacy"),
                      N_("Convert selected object's stroke to paths legacy mode"), INKSCAPE_ICON("stroke-to-path")),
    new SelectionVerb(SP_VERB_SELECTION_SIMPLIFY, "SelectionSimplify", N_("Si_mplify"),
                      N_("Simplify selected paths (remove extra nodes)"), INKSCAPE_ICON("path-simplify")),
    new SelectionVerb(SP_VERB_SELECTION_REVERSE, "SelectionReverse", N_("_Reverse"),
                      N_("Reverse the direction of selected paths (useful for flipping markers)"),
                      INKSCAPE_ICON("path-reverse")),
    // TRANSLATORS: "to trace" means "to convert a bitmap to vector graphics" (to vectorize)
    new SelectionVerb(SP_VERB_SELECTION_TRACE, "SelectionTrace", N_("_Trace Bitmap..."),
                      N_("Create one or more paths from a bitmap by tracing it"), INKSCAPE_ICON("bitmap-trace")),
    new SelectionVerb(SP_VERB_SELECTION_CREATE_BITMAP, "SelectionCreateBitmap", N_("Make a _Bitmap Copy"),
                      N_("Export selection to a bitmap and insert it into document"),
                      INKSCAPE_ICON("selection-make-bitmap-copy")),
    new SelectionVerb(SP_VERB_SELECTION_COMBINE, "SelectionCombine", N_("_Combine"),
                      N_("Combine several paths into one"), INKSCAPE_ICON("path-combine")),
    // TRANSLATORS: "to cut a path" is not the same as "to break a path apart" - see the
    // Advanced tutorial for more info
    new SelectionVerb(SP_VERB_SELECTION_BREAK_APART, "SelectionBreakApart", N_("Break _Apart"),
                      N_("Break selected paths into subpaths"), INKSCAPE_ICON("path-break-apart")),
    new SelectionVerb(SP_VERB_SELECTION_FILL_BETWEEN_MANY, "SelectionFillBetweenMany", N_("Fill between paths"),
                      N_("Create a fill object using the selected paths"), nullptr),
    // Layer
    new LayerVerb(SP_VERB_LAYER_NEW, "LayerNew", N_("_Add Layer..."), N_("Create a new layer"),
                  INKSCAPE_ICON("layer-new")),
    new LayerVerb(SP_VERB_LAYER_RENAME, "LayerRename", N_("Re_name Layer..."), N_("Rename the current layer"),
                  INKSCAPE_ICON("layer-rename")),
    new LayerVerb(SP_VERB_LAYER_NEXT, "LayerNext", N_("Switch to Layer Abov_e"),
                  N_("Switch to the layer above the current"), INKSCAPE_ICON("layer-previous")),
    new LayerVerb(SP_VERB_LAYER_PREV, "LayerPrev", N_("Switch to Layer Belo_w"),
                  N_("Switch to the layer below the current"), INKSCAPE_ICON("layer-next")),
    new LayerVerb(SP_VERB_LAYER_MOVE_TO_NEXT, "LayerMoveToNext", N_("Move Selection to Layer Abo_ve"),
                  N_("Move selection to the layer above the current"), INKSCAPE_ICON("selection-move-to-layer-above")),
    new LayerVerb(SP_VERB_LAYER_MOVE_TO_PREV, "LayerMoveToPrev", N_("Move Selection to Layer Bel_ow"),
                  N_("Move selection to the layer below the current"), INKSCAPE_ICON("selection-move-to-layer-below")),
    new LayerVerb(SP_VERB_LAYER_MOVE_TO, "LayerMoveTo", N_("Move Selection to Layer..."), N_("Move selection to layer"),
                  INKSCAPE_ICON("selection-move-to-layer")),
    new LayerVerb(SP_VERB_LAYER_TO_TOP, "LayerToTop", N_("Layer to _Top"), N_("Raise the current layer to the top"),
                  INKSCAPE_ICON("layer-top")),
    new LayerVerb(SP_VERB_LAYER_TO_BOTTOM, "LayerToBottom", N_("Layer to _Bottom"),
                  N_("Lower the current layer to the bottom"), INKSCAPE_ICON("layer-bottom")),
    new LayerVerb(SP_VERB_LAYER_RAISE, "LayerRaise", N_("_Raise Layer"), N_("Raise the current layer"),
                  INKSCAPE_ICON("layer-raise")),
    new LayerVerb(SP_VERB_LAYER_LOWER, "LayerLower", N_("_Lower Layer"), N_("Lower the current layer"),
                  INKSCAPE_ICON("layer-lower")),
    new LayerVerb(SP_VERB_LAYER_DUPLICATE, "LayerDuplicate", N_("D_uplicate Current Layer"),
                  N_("Duplicate an existing layer"), INKSCAPE_ICON("layer-duplicate")),
    new LayerVerb(SP_VERB_LAYER_DELETE, "LayerDelete", N_("_Delete Current Layer"), N_("Delete the current layer"),
                  INKSCAPE_ICON("layer-delete")),
    new LayerVerb(SP_VERB_LAYER_SOLO, "LayerSolo", N_("_Show/hide other layers"), N_("Solo the current layer"),
                  nullptr),
    new LayerVerb(SP_VERB_LAYER_SHOW_ALL, "LayerShowAll", N_("_Show all layers"), N_("Show all the layers"), nullptr),
    new LayerVerb(SP_VERB_LAYER_HIDE_ALL, "LayerHideAll", N_("_Hide all layers"), N_("Hide all the layers"), nullptr),
    new LayerVerb(SP_VERB_LAYER_LOCK_ALL, "LayerLockAll", N_("_Lock all layers"), N_("Lock all the layers"), nullptr),
    new LayerVerb(SP_VERB_LAYER_LOCK_OTHERS, "LayerLockOthers", N_("Lock/Unlock _other layers"),
                  N_("Lock all the other layers"), nullptr),
    new LayerVerb(SP_VERB_LAYER_UNLOCK_ALL, "LayerUnlockAll", N_("_Unlock all layers"), N_("Unlock all the layers"),
                  nullptr),
    new LayerVerb(SP_VERB_LAYER_TOGGLE_LOCK, "LayerToggleLock", N_("_Lock/Unlock Current Layer"),
                  N_("Toggle lock on current layer"), nullptr),
    new LayerVerb(SP_VERB_LAYER_TOGGLE_HIDE, "LayerToggleHide", N_("_Show/Hide Current Layer"),
                  N_("Toggle visibility of current layer"), nullptr),

    // Object
    new ObjectVerb(SP_VERB_OBJECT_ROTATE_90_CW, "ObjectRotate90", N_("Rotate _90\xc2\xb0 CW"),
                   // This is shared between tooltips and statusbar, so they
                   // must use UTF-8, not HTML entities for special characters.
                   N_("Rotate selection 90\xc2\xb0 clockwise"), INKSCAPE_ICON("object-rotate-right")),
    new ObjectVerb(SP_VERB_OBJECT_ROTATE_90_CCW, "ObjectRotate90CCW", N_("Rotate 9_0\xc2\xb0 CCW"),
                   // This is shared between tooltips and statusbar, so they
                   // must use UTF-8, not HTML entities for special characters.
                   N_("Rotate selection 90\xc2\xb0 counter-clockwise"), INKSCAPE_ICON("object-rotate-left")),
    new ObjectVerb(SP_VERB_OBJECT_FLATTEN, "ObjectRemoveTransform", N_("Remove _Transformations"),
                   N_("Remove transformations from object"), nullptr),
    new ObjectVerb(SP_VERB_OBJECT_TO_CURVE, "ObjectToPath", N_("_Object to Path"),
                   N_("Convert selected object to path"), INKSCAPE_ICON("object-to-path")),
    new ObjectVerb(SP_VERB_OBJECT_FLOW_TEXT, "ObjectFlowText", N_("_Flow into Frame"),
                   N_("Put text into a frame (path or shape), creating a flowed text linked to the frame object"),
                   "text-flow-into-frame"),
    new ObjectVerb(SP_VERB_OBJECT_FLOW_SUBTRACT, "ObjectFlowSubtract", N_("Set _Subtraction Frames"),
                   N_("Flow text around a frame (path or shape), only available for SVG 2.0 Flow text."),
                   "text-flow-subtract-frame"),
    new ObjectVerb(SP_VERB_OBJECT_UNFLOW_TEXT, "ObjectUnFlowText", N_("_Unflow"),
                   N_("Remove text from frame (creates a single-line text object)"), INKSCAPE_ICON("text-unflow")),
    new ObjectVerb(SP_VERB_OBJECT_FLOWTEXT_TO_TEXT, "ObjectFlowtextToText", N_("_Convert to Text"),
                   N_("Convert flowed text to regular text object (preserves appearance)"),
                   INKSCAPE_ICON("text-convert-to-regular")),
    new ObjectVerb(SP_VERB_OBJECT_FLIP_HORIZONTAL, "ObjectFlipHorizontally", N_("Flip _Horizontal"),
                   N_("Flip selected objects horizontally"), INKSCAPE_ICON("object-flip-horizontal")),
    new ObjectVerb(SP_VERB_OBJECT_FLIP_VERTICAL, "ObjectFlipVertically", N_("Flip _Vertical"),
                   N_("Flip selected objects vertically"), INKSCAPE_ICON("object-flip-vertical")),
    new ObjectVerb(SP_VERB_OBJECT_SET_MASK, "ObjectSetMask", N_("_Set"),
                   N_("Apply mask to selection (using the topmost object as mask)"), nullptr),
    new ObjectVerb(SP_VERB_OBJECT_SET_INVERSE_MASK, "ObjectSetInverseMask", N_("_Set Inverse (LPE)"),
                   N_("Apply inverse mask to selection (using the topmost object as mask)"), nullptr),
    new ObjectVerb(SP_VERB_OBJECT_EDIT_MASK, "ObjectEditMask", N_("_Edit"), N_("Edit mask"),
                   INKSCAPE_ICON("path-mask-edit")),
    new ObjectVerb(SP_VERB_OBJECT_UNSET_MASK, "ObjectUnSetMask", N_("_Release"), N_("Remove mask from selection"),
                   nullptr),
    new ObjectVerb(SP_VERB_OBJECT_SET_CLIPPATH, "ObjectSetClipPath", N_("_Set"),
                   N_("Apply clipping path to selection (using the topmost object as clipping path)"), nullptr),
    new ObjectVerb(SP_VERB_OBJECT_SET_INVERSE_CLIPPATH, "ObjectSetInverseClipPath", N_("_Set Inverse (LPE)"),
                   N_("Apply inverse clipping path to selection (using the topmost object as clipping path)"), nullptr),
    new ObjectVerb(SP_VERB_OBJECT_CREATE_CLIP_GROUP, "ObjectCreateClipGroup", N_("Create Cl_ip Group"),
                   N_("Creates a clip group using the selected objects as a base"), nullptr),
    new ObjectVerb(SP_VERB_OBJECT_EDIT_CLIPPATH, "ObjectEditClipPath", N_("_Edit"), N_("Edit clipping path"),
                   INKSCAPE_ICON("path-clip-edit")),
    new ObjectVerb(SP_VERB_OBJECT_UNSET_CLIPPATH, "ObjectUnSetClipPath", N_("_Release"),
                   N_("Remove clipping path from selection"), nullptr),

    // Effect -- renamed Extension
    new EffectLastVerb(SP_VERB_EFFECT_LAST, "EffectLast", N_("Previous Exte_nsion"),
                       N_("Repeat the last extension with the same settings"), nullptr),
    new EffectLastVerb(SP_VERB_EFFECT_LAST_PREF, "EffectLastPref", N_("_Previous Extension Settings..."),
                       N_("Repeat the last extension with new settings"), nullptr),

    // Fit Page
    new FitCanvasVerb(SP_VERB_FIT_CANVAS_TO_SELECTION, "FitCanvasToSelection", N_("Fit Preview to Selection"),
                      N_("Fit the preview box to the current selection"), nullptr),
    new FitCanvasVerb(SP_VERB_FIT_CANVAS_TO_DRAWING, "FitCanvasToDrawing", N_("Fit Preview to Drawing"),
                      N_("Fit the preview box to the drawing"), nullptr),
    // LockAndHide
    new LockAndHideVerb(SP_VERB_UNLOCK_ALL, "UnlockAll", N_("Unlock All"),
                        N_("Unlock all objects in the current layer"), nullptr),
    new LockAndHideVerb(SP_VERB_UNLOCK_ALL_IN_ALL_LAYERS, "UnlockAllInAllLayers", N_("Unlock All in All Layers"),
                        N_("Unlock all objects in all layers"), nullptr),
    new LockAndHideVerb(SP_VERB_UNHIDE_ALL, "UnhideAll", N_("Unhide All"),
                        N_("Unhide all objects in the current layer"), nullptr),
    new LockAndHideVerb(SP_VERB_UNHIDE_ALL_IN_ALL_LAYERS, "UnhideAllInAllLayers", N_("Unhide All in All Layers"),
                        N_("Unhide all objects in all layers"), nullptr),
    // Color Management
    new EditVerb(SP_VERB_EDIT_LINK_COLOR_PROFILE, "LinkColorProfile", N_("Link Color Profile"),
                 N_("Link an ICC color profile"), nullptr),
    new EditVerb(SP_VERB_EDIT_REMOVE_COLOR_PROFILE, "RemoveColorProfile", N_("Remove Color Profile"),
                 N_("Remove a linked ICC color profile"), nullptr),
    // Footer
    new Verb(SP_VERB_LAST, " '\"invalid id", nullptr, nullptr, nullptr, nullptr)};

std::vector<Inkscape::Verb *>
Verb::getList () {

    std::vector<Verb *> verbs;
    // Go through the dynamic verb table
    for (auto & _verb : _verbs) {
        Verb * verb = _verb.second;
        if (verb->get_code() == SP_VERB_INVALID ||
                verb->get_code() == SP_VERB_NONE ||
                verb->get_code() == SP_VERB_LAST) {
            continue;
        }

        verbs.push_back(verb);
    }

    return verbs;
};

void
Verb::list () {
    // Go through the dynamic verb table
    for (auto & _verb : _verbs) {
        Verb * verb = _verb.second;
        if (verb->get_code() == SP_VERB_INVALID ||
                verb->get_code() == SP_VERB_NONE ||
                verb->get_code() == SP_VERB_LAST) {
            continue;
        }

        printf("%s: %s\n", verb->get_id(), verb->get_tip()? verb->get_tip() : verb->get_name());
    }

    return;
};

}  // namespace Inkscape

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
