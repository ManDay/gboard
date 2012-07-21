#ifndef __GBD_KEYBOARD_H__
#define __GBD_KEYBOARD_H__

#include <gtk/gtk.h>

#include "../interfaces/emitter.h"
#include "layout.h"

/// Return GType; Return Class
#define GBD_TYPE_KEYBOARD (gbd_keyboard_get_type( ))
#define GBD_KEYBOARD_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS( (obj),GBD_TYPE_KEYBOARD,GbdKeyboardClass ))

/// Check whether instance is derived; Cast it
#define GBD_IS_KEYBOARD(obj) (G_TYPE_CHECK_INSTANCE_TYPE( (obj),GBD_TYPE_KEYBOARD ))
#define GBD_KEYBOARD(obj) (G_TYPE_CHECK_INSTANCE_CAST( (obj),GBD_TYPE_KEYBOARD,GbdKeyboard ))

/// Check whether class is derived; Cast it
#define GBD_IS_KEYBOARD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE( (klass),GBD_TYPE_KEYBOARD ))
#define GBD_KEYBOARD_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST( (klass),GBD_TYPE_KEYBOARD,GbdKeyboardClass ))

typedef struct GbdKeyboardPrivate GbdKeyboardPrivate;

typedef struct {
	GtkDrawingArea parent;

	GbdKeyboardPrivate* priv;
} GbdKeyboard;

typedef struct {
	GtkDrawingAreaClass parent;

	void(* mask_change)( GbdKeyboard*,gpointer );
} GbdKeyboardClass;

cairo_region_t* gbd_keyboard_regions( GbdKeyboard* );

GType gbd_keyboard_get_type( void );
GbdKeyboard* gbd_keyboard_new( GbdLayout* );

#endif
