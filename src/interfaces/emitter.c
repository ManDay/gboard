#include "emitter.h"

#define GBD_EMITTER_GET_INTERFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE(obj,GBD_TYPE_EMITTER,GbdEmitterInterface))

guint gbd_emitter_get_code( GbdEmitter* self,gchar* key ) {
	GBD_EMITTER_GET_INTERFACE( self )->get_code( self,key );
}

void gbd_emitter_emit( GbdEmitter* self,guint code) {
	GBD_EMITTER_GET_INTERFACE( self )->emit( self,code );
}

GType gbd_emitter_get_type( ) {
	static GType type = 0;
	if( !type ) {
		const GTypeInfo info = {
			sizeof( GbdEmitterInterface ),
			NULL,NULL,
			NULL,NULL,NULL,
			0,0,
			NULL,
			NULL
		};
		type = g_type_register_static( G_TYPE_INTERFACE,"GbdEmitter",&info,0 );
	}
	return type;
}
