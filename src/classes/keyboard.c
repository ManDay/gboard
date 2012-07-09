#include "keyboard.h"

enum {
	PROP_0,
	PROP_N
};

enum {
	SIG_0,
	SIG_N
};

struct GbdKeyboardPrivate {
};

static GParamSpec* props[ PROP_N ];
static guint sigs[ SIG_N ];

static void class_init( GbdKeyboardClass*,gpointer );
static void instance_init( GbdKeyboard* );
static void dispose( GObject* );
static void set_property( GObject*,guint,const GValue*,GParamSpec* );
static void get_property( GObject*,guint,GValue*,GParamSpec* );

static void class_init( GbdKeyboardClass* klass,gpointer udata ) {
	g_type_class_add_private( klass,sizeof( GbdKeyboardPrivate ) );

	G_OBJECT_CLASS( klass )->get_property = get_property;
	G_OBJECT_CLASS( klass )->set_property = set_property;
	G_OBJECT_CLASS( klass )->dispose = dispose;

	g_object_class_install_properties( G_OBJECT_CLASS( klass ),PROP_N,props );
}

static void instance_init( GbdKeyboard* self ) {
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self,GBD_TYPE_KEYBOARD,GbdKeyboardPrivate );
}

static void dispose( GObject* _self ) {
	G_OBJECT_CLASS( g_type_class_peek( G_TYPE_OBJECT ) )->dispose( _self );
}

static void set_property( GObject* _self,guint prop,const GValue* value,GParamSpec* pspec ) {
	switch( prop ) {
	}
}

static void get_property( GObject* _self,guint prop,GValue* value,GParamSpec* pspec ) {
	switch( prop ) {
	}
}

GType hfc_workspace_get_type( ) {
	static GType type = 0;
	if( !type ) {
		const GTypeInfo info = {
			sizeof( GbdKeyboardClass ),
			NULL,NULL,
			(GClassInitFunc)class_init,NULL,NULL,
			sizeof( GbdKeyboard ),0,
			(GInstanceInitFunc)instance_init,
			NULL

		};
		type = g_type_register_static( G_TYPE_OBJECT,"GbdKeyboard",&info,0 );
	}
	return type;
}

GbdKeyboard* hfc_workspace_new( ) {
	return g_object_new( GBD_TYPE_KEYBOARD,NULL );
}
