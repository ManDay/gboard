#ifndef __GBD_X11EMITTER_H__
#define __GBD_X11EMITTER_H__

#include <glib-object.h>

/// Return GType; Return Class
#define GBD_TYPE_X11EMITTER (gbd_x11emitter_get_type( ))

/// Check whether instance is derived; Cast it
#define GBD_IS_X11EMITTER(obj) (G_TYPE_CHECK_INSTANCE_TYPE( (obj),GBD_TYPE_X11EMITTER ))
#define GBD_X11EMITTER(obj) (G_TYPE_CHECK_INSTANCE_CAST( (obj),GBD_TYPE_X11EMITTER,GbdX11emitter ))

/// Check whether class is derived; Cast it
#define GBD_IS_X11EMITTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE( (klass),GBD_TYPE_X11EMITTER ))
#define GBD_X11EMITTER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST( (klass),GBD_TYPE_X11EMITTER,GbdX11emitterClass ))

typedef struct GbdX11emitterPrivate GbdX11emitterPrivate;

typedef struct {
	GObject parent;

	GbdX11emitterPrivate* priv;
} GbdX11emitter;

typedef struct {
	GObjectClass parent;
} GbdX11emitterClass;

GType gbd_x11emitter_get_type( void );
GbdX11emitter* gbd_x11emitter_new( void );

#endif
