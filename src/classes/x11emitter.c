#include "x11emitter.h"

#include <X11/Xlib.h>

#include "../interfaces/emitter.h"

struct GbdX11emitterPrivate {
	guint dummy;
};

static void class_init( GbdX11emitterClass*,gpointer );
static void instance_init( GbdX11emitter* );
static void instance_finalize( GbdX11emitter* );
static void interface_init_emitter( GbdEmitterInterface*,gpointer* );

static guint64 get_code( GbdEmitter*,gchar* );
static void emit( GbdEmitter*,guint64 );
static void release( GbdEmitter*,guint64 );

static void class_init( GbdX11emitterClass* klass,gpointer udata ) {
	GObjectClass* klass_go = G_OBJECT_CLASS( klass );

	g_type_class_add_private( klass,sizeof( GbdX11emitterPrivate ) );

	klass_go->finalize = (GObjectFinalizeFunc)instance_finalize;
}

static void instance_init( GbdX11emitter* self ) {
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self,GBD_TYPE_X11EMITTER,GbdX11emitterPrivate );
}

static void interface_init_emitter( GbdEmitterInterface* iface,gpointer* udata ) {
	iface->emit = emit;
	iface->release = release;
	iface->get_code = get_code;
}

static void instance_finalize( GbdX11emitter* self ) {
	G_OBJECT_CLASS( g_type_class_peek( G_TYPE_OBJECT ) )->finalize( G_OBJECT( self ) );
}

static guint64 get_code( GbdEmitter* self,gchar* key ) {
	return 0;
}

static void emit( GbdEmitter* self,guint64 code ) {
	return;
}

static void release( GbdEmitter* self,guint64 code ) {
	return;
}

GType gbd_x11emitter_get_type( ) {
	static GType type = 0;
	if( !type ) {
		const GTypeInfo info = {
			sizeof( GbdX11emitterClass ),
			NULL,NULL,
			(GClassInitFunc)class_init,NULL,NULL,
			sizeof( GbdX11emitter ),0,
			(GInstanceInitFunc)instance_init,
			NULL
		};
		const GInterfaceInfo iinfo = {
			(GInterfaceInitFunc)(interface_init_emitter ),
			NULL,
			NULL
		};
		type = g_type_register_static( G_TYPE_OBJECT,"GbdX11emitter",&info,0 );
		g_type_add_interface_static( type,GBD_TYPE_EMITTER,&iinfo );
	}
	return type;
}

GbdX11emitter* gbd_x11emitter_new( ) {
	GbdX11emitter* self = g_object_new( GBD_TYPE_X11EMITTER,NULL );

	return self;
}
