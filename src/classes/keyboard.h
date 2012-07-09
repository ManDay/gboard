#ifndef __GBD_KEYBOARD_H__
#define __GBD_KEYBOARD_H__

/// Return GType; Return Class
#define GBD_TYPE_KEYBOARD (gbd_keyboard_get_type( ))
#define GBD_KEYBOARD_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS( (obj),GBD_TYPE_KEYBOARD,GdbKeyboardClass ))

/// Check whether instance is derived; Cast it
#define GBD_IS_KEYBOARD(obj) (G_TYPE_CHECK_INSTANCE_TYPE( (obj),GBD_TYPE_KEYBOARD ))
#define GBD_KEYBOARD(obj) (G_TYPE_CHECK_INSTANCE_CAST( (obj),GBD_TYPE_KEYBOARD,GdbKeyboard ))

/// Check whether class is derived; Cast it
#define GBD_IS_KEYBOARD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE( (klass),GBD_TYPE_KEYBOARD ))
#define GBD_KEYBOARD_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST( (klass),GBD_TYPE_KEYBOARD,GdbKeyboardClass ))

typedef struct GdbKeyboardPrivate GdbKeyboardPrivate;

typedef struct {
	GObjectClass parent;
} GdbKeyboardClass;

typedef struct {
	GObject parent;

	GdbKeyboardPrivate* priv;
} GdbKeyboard;

GType gbd_keyboard_get_type( void );
GdbKeyboard* gbd_keyboard_new( void );

#endif
