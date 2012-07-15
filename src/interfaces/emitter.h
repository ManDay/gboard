#ifndef __GBD_EMITTER_H__
#define __GBD_EMITTER_H__

#include <glib-object.h>

/// Return GType
#define GBD_TYPE_EMITTER (gbd_emitter_get_type( ))

/// Check whether instance has interface; Cast to it
#define GBD_IS_EMITTER(obj) (G_TYPE_CHECK_INSTANCE_TYPE( (obj),GBD_TYPE_EMITTER ))
#define GBD_EMITTER(obj) (G_TYPE_CHECK_INSTANCE_CAST( (obj),GBD_TYPE_EMITTER,GbdEmitter ))

typedef struct GbdEmitter GbdEmitter;

typedef struct {
	GTypeInterface parent;

	guint(* get_code)( GbdEmitter*,gchar* );
	void(* emit)( GbdEmitter*,guint );
} GbdEmitterInterface;

guint gbd_emitter_get_code( GbdEmitter*,gchar* );
void gbd_emitter_emit( GbdEmitter*,guint );

GType gbd_emitter_get_type( void );

#endif
