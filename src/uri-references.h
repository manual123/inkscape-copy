#ifndef __SP_URI_REFERENCES_H__
#define __SP_URI_REFERENCES_H__

/*
 * Helper methods for resolving URI References
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * Copyright (C) 2001-2002 Lauris Kaplinski
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <glib.h>
#include <sigc++/sigc++.h>
#include <exception>
#include "forward.h"
#include "document.h"

namespace Inkscape {

class UnsupportedURIException {
public:
	const char *what() const { return "Unsupported or malformed URI"; }
};

/**
 * A class encapsulating a reference to a particular URI; observers can
 * be notified when the URI comes to reference a different SPObject.
 *
 * The URIReference increments and decrements the SPObject's hrefcount
 * automatically.
 */
class URIReference : public SigC::Object {
public:
	/**
	 * Constructs a reference object given an SPDocument *
	 * and a uri which is resolved relative to that document.
	 *
	 * Throws an UnsupportedURIException if the uri is unsupported
	 * or malformed.
	 *
	 * @param rel_document document to resolve references relative to
	 * @param uri a CSS url() specification
	 *
	 * @see SPObject
	 * @see sp_object_href
	 * @see sp_object_hunref
	 */
	URIReference(SPDocument *rel_document, const gchar *uri);

	/**
	 * Destructor.  Calls shutdown() if the reference has not been
	 * shut down yet.
	 */
	virtual ~URIReference();

	/**
	 * Shuts down the reference, reporting the current object
	 * as NULL.  No further changes will be reported.
	 */
	void shutdown();

	/**
	 * Accessor for the reference's change notification signal.
	 * The signal has two parameters: the formerly referenced
	 * SPObject and the newly refrenced SPObject.
	 * @returns a signal
	 */
	SigC::Signal2<void, SPObject *, SPObject *> changedSignal();

	/**
	 * Returns a pointer to the SPObject the reference currently
	 * refers to (if any).
	 * @return a pointer to the referenced SPObject or NULL
	 */
	SPObject *getObject();

private:
	SigC::Connection _connection;
	SPObject *_obj;

	SigC::Signal2<void, SPObject *, SPObject *> _changed_signal;

	void _setObject(SPObject *object);
	static void _release(SPObject *object, URIReference *reference);
};

inline SigC::Signal2<void, SPObject *, SPObject *> URIReference::changedSignal(){
	return _changed_signal;
}

inline SPObject *URIReference::getObject() {
	return _obj;
}

}

SPObject *sp_uri_reference_resolve (SPDocument *document, const gchar *uri);

#endif
