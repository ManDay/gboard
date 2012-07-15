#include "layout.h"

#include <gio.h>

struct GbdLayoutPrivate {
	guint dummy;
};

static void class_init( GbdLayoutClass*,gpointer );
static void instance_init( GbdLayout* );
static void instance_finalize( GbdLayout* );

static void class_init( GbdLayoutClass* klass,gpointer udata ) {
	GObjectClass* klass_go = G_OBJECT_CLASS( klass );

	g_type_class_add_private( klass,sizeof( GbdLayoutPrivate ) );

	klass_go->finalize = (GObjectFinalizeFunc)instance_finalize;
}

static void instance_init( GbdLayout* self ) {
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self,GBD_TYPE_LAYOUT,GbdLayoutPrivate );
}

static void instance_finalize( GbdLayout* self ) {
	G_OBJECT_CLASS( g_type_class_peek( G_TYPE_OBJECT ) )->finalize( G_OBJECT( self ) );
}

gboolean gbd_layout_parse( GbdLayout* self,gchar* str ) {
}

GType gbd_layout_get_type( ) {
	static GType type = 0;
	if( !type ) {
		const GTypeInfo info = {
			sizeof( GbdLayoutClass ),
			NULL,NULL,
			(GClassInitFunc)class_init,NULL,NULL,
			sizeof( GbdLayout ),0,
			(GInstanceInitFunc)instance_init,
			NULL
		};
		type = g_type_register_static( G_TYPE_OBJECT,"GbdLayout",&info,0 );
	}
	return type;
}

GbdLayout* gbd_layout_new( gchar* str ) {
	GbdLayout* self = g_object_new( GBD_TYPE_LAYOUT );
	if( str )
		gbd_layout_parse( self,str )

	return self;
}
