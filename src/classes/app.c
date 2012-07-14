#include "app.h"

#include <glib.h>
#include <stdlib.h>

#include "../config.h"

struct GbdAppPrivate {
	GtkStatusIcon* tray;
	guchar visibility;
};

GDBusSignalInfo* sinfo[ ] = {
	&(GDBusSignalInfo){ -1,"Submit",(GDBusArgInfo*[ ]){ &(GDBusArgInfo){ -1,"Text","s" },NULL } },
	NULL
};

GDBusPropertyInfo* pinfo[ ] = {
	&(GDBusPropertyInfo){ -1,"Visible","y",G_DBUS_PROPERTY_INFO_FLAGS_READABLE },
	NULL
};

GDBusInterfaceInfo iinfo = { -1,GBD_NAME,NULL,sinfo,pinfo,NULL };

static void class_init( GbdAppClass*,gpointer );
static void instance_init( GbdApp* );
static void instance_finalize( GbdApp* );

static void startup( GApplication* );
static void activate( GApplication* );
static gboolean dbus_register( GApplication*,GDBusConnection*,const gchar*,GError** );
static void dbus_unregister( GApplication*,GDBusConnection*,const gchar* );

static GVariant* dbus_property_get( GDBusConnection*,gchar*,gchar*,gchar*,gchar*,GError*,gpointer );
static gboolean* dbus_property_set( GDBusConnection*,gchar*,gchar*,gchar*,gchar*,GVariant*,GError*,gpointer );

static void show_board( GSimpleAction*,GVariant*,gpointer );
static void hide_board( GSimpleAction*,GVariant*,gpointer );

static void class_init( GbdAppClass* klass,gpointer udata ) {
	GApplicationClass* klass_ga = G_APPLICATION_CLASS( klass );
	GObjectClass* klass_go = G_OBJECT_CLASS( klass );

	g_type_class_add_private( klass,sizeof( GbdAppPrivate ) );

	klass_go->finalize = (GObjectFinalizeFunc)instance_finalize;

	klass_ga->startup = startup;
	klass_ga->activate = activate;

	// TODO : Comes with Glib-2.34
	//klass_ga->dbus_register = dbus_register;
	//klass_ga->dbus_unregister = dbus_unregister;
}

static void instance_init( GbdApp* self ) {
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self,GBD_TYPE_APP,GbdAppPrivate );

	self->priv->visibility = 0;

	GActionEntry actions[ ]= {
		{ "Show",show_board,"y" },
		{ "Hide",hide_board,"b" }
	};
	g_action_map_add_action_entries( G_ACTION_MAP( self ),actions,2,self );
}

static void instance_finalize( GbdApp* self ) {
	G_OBJECT_CLASS( g_type_class_peek( G_TYPE_OBJECT ) )->finalize( G_OBJECT( self ) );
}

static void startup( GApplication* _self ) {
	G_APPLICATION_CLASS( g_type_class_peek( GTK_TYPE_APPLICATION ) )->startup( _self );

	GbdAppPrivate* const priv = GBD_APP( _self )->priv;

	g_print( "Activating GBoard\n" );
	g_application_hold( _self );
	priv->tray = gtk_status_icon_new_from_stock( GTK_STOCK_EDIT );
}

static void activate( GApplication* _self ) {
	G_APPLICATION_CLASS( g_type_class_peek( GTK_TYPE_APPLICATION ) )->activate( _self );
}

static gboolean dbus_register( GApplication* _self,GDBusConnection* conn,const gchar* dbapi,GError** error ) {
	g_assert( conn );

	GDBusInterfaceVTable vtable = { 
		NULL,
		(GDBusInterfaceGetPropertyFunc)dbus_property_get,
		(GDBusInterfaceSetPropertyFunc)dbus_property_set
	};

	g_dbus_connection_register_object( conn,GBD_PATH,&iinfo,&vtable,_self,NULL,NULL );
}

static void dbus_unregister( GApplication* _self,GDBusConnection* conn,const gchar* dbapi ) {
}

static GVariant* dbus_property_get( GDBusConnection* conn,gchar* send,gchar* path,gchar* name,gchar* prop,GError* err,gpointer _self ) {
	GbdAppPrivate* priv = GBD_APP( _self )->priv;

	if( !g_strcmp0( prop,"Visible" ) )
		return g_variant_new( "y",priv->visibility );

	return NULL;
}

static gboolean* dbus_property_set( GDBusConnection* conn,gchar* send,gchar* path,gchar* name,gchar* prop,GVariant* val,GError* err,gpointer _self ) {
	// Nothing to see here, move along
}

static void show_board( GSimpleAction* action,GVariant* parms,gpointer _self ) {
	g_print( "Showing\n" );
}

static void hide_board( GSimpleAction* action,GVariant* parms,gpointer _self ) {
	g_print( "Hiding\n" );
}

GType gbd_app_get_type( ) {
	static GType type = 0;
	if( !type ) {
		const GTypeInfo info = {
			sizeof( GbdAppClass ),
			NULL,NULL,
			(GClassInitFunc)class_init,NULL,NULL,
			sizeof( GbdApp ),0,
			(GInstanceInitFunc)instance_init,
			NULL
		};
		type = g_type_register_static( GTK_TYPE_APPLICATION,"GbdApp",&info,0 );
	}
	return type;
}

GbdApp* gbd_app_new( ) {
	GbdApp* self = g_object_new( GBD_TYPE_APP,"application-id",GBD_NAME,"flags",G_APPLICATION_IS_SERVICE,NULL );

	// TODO : Move into dbus_register with Glib-2.34
	dbus_register( G_APPLICATION( self ),g_bus_get_sync( G_BUS_TYPE_SESSION,NULL,NULL ),NULL,NULL );

	return self;
}
