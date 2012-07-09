#include "app.h"

#include <glib.h>
#include <stdlib.h>

#include "../config.h"

struct GbdAppPrivate {
	GtkStatusIcon* tray;
};

static void class_init( GbdAppClass*,gpointer );
static void instance_init( GbdApp* );

static void dispose( GObject* );

static void startup( GApplication* );
static void activate( GApplication* );
static gboolean dbus_register( GApplication*,GDBusConnection*,const gchar*,GError** );
static void dbus_unregister( GApplication*,GDBusConnection*,const gchar* );

static void class_init( GbdAppClass* klass,gpointer udata ) {
	GApplicationClass* klass_ga = G_APPLICATION_CLASS( klass );
	GObjectClass* klass_go = G_OBJECT_CLASS( klass );

	g_type_class_add_private( klass,sizeof( GbdAppPrivate ) );

	klass_go->dispose = dispose;

	klass_ga->startup = startup;
	klass_ga->activate = activate;

	// TODO : Comes with Glib-2.34
	//klass_ga->dbus_register = dbus_register;
	//klass_ga->dbus_unregister = dbus_unregister;
}

static void instance_init( GbdApp* self ) {
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self,GBD_TYPE_APP,GbdAppPrivate );
}

static void dispose( GObject* _self ) {
	G_OBJECT_CLASS( g_type_class_peek( G_TYPE_OBJECT ) )->dispose( _self );
}

static void startup( GApplication* _self ) {
	G_APPLICATION_CLASS( g_type_class_peek( GTK_TYPE_APPLICATION ) )->startup( _self );

	GbdAppPrivate* const priv = GBD_APP( _self )->priv;

	priv->tray = gtk_status_icon_new_from_stock( GTK_STOCK_EDIT );
}

static void activate( GApplication* _self ) {
	g_application_hold( _self );
}

static gboolean dbus_register( GApplication* _self,GDBusConnection* conn,const gchar* dbapi,GError** error ) {
}

static void dbus_unregister( GApplication* _self,GDBusConnection* conn,const gchar* dbapi ) {
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

	// TODO : Move into dbus_register with Glib-2.34
	GDBusConnection* conn = g_bus_get_sync( G_BUS_TYPE_SESSION,NULL,NULL );
	if( !conn ) {
		g_printerr( "Could not obtain connection on the bus\n" );
		exit( 1 );
	}
	// Register Objects
	GbdApp* self = g_object_new( GBD_TYPE_APP,"application-id",GBD_NAME,"flags",G_APPLICATION_IS_SERVICE,NULL );

	return self;
}
