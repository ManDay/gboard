#include "app.h"

#include <stdlib.h>
#include <gbd_config.h>
#include "../interfaces/emitter.h"
#include "x11emitter.h"
#include "keyboard.h"
#include "layout.h"

struct GbdAppPrivate {
	GtkStatusIcon* tray;
	GtkWindow* window;
	GbdEmitter* emitter;
	GbdKeyboard* keyboard;
	GData* layouts;
	gboolean visible,pen,docked,north;
	GSettings* settings;
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
static gint command_line( GApplication*,GApplicationCommandLine* );
static gboolean dbus_register( GApplication*,GDBusConnection*,const gchar*,GError** );
static void dbus_unregister( GApplication*,GDBusConnection*,const gchar* );

static GVariant* dbus_property_get( GDBusConnection*,gchar*,gchar*,gchar*,gchar*,GError*,gpointer );
static gboolean* dbus_property_set( GDBusConnection*,gchar*,gchar*,gchar*,gchar*,GVariant*,GError*,gpointer );

static gboolean load_layout( GbdApp*,GFile*,gboolean,GError** );
static void update_regions( GbdKeyboard*,GbdApp* );
static void toggle_board_hnd( GtkStatusIcon*,GbdApp* );
static void show_board_hnd( GSimpleAction*,GVariant*,gpointer );
static void hide_board_hnd( GSimpleAction*,GVariant*,gpointer );
static gboolean hide_board_hnd2( GtkWidget*,GdkEvent*,GbdApp* );

static void class_init( GbdAppClass* klass,gpointer udata ) {
	GApplicationClass* klass_ga = G_APPLICATION_CLASS( klass );
	GObjectClass* klass_go = G_OBJECT_CLASS( klass );

	g_type_class_add_private( klass,sizeof( GbdAppPrivate ) );

	klass_go->finalize = (GObjectFinalizeFunc)instance_finalize;

	klass_ga->startup = startup;
	klass_ga->activate = activate;
	klass_ga->command_line = command_line;

	// TODO : Comes with Glib-2.34
	//klass_ga->dbus_register = dbus_register;
	//klass_ga->dbus_unregister = dbus_unregister;
}

static void instance_init( GbdApp* self ) {
	GbdAppPrivate* const priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self,GBD_TYPE_APP,GbdAppPrivate );

	priv->settings = g_settings_new( GBD_NAME );

	GActionEntry actions[ ]= {
		{ "Show",show_board_hnd,"y" },
		{ "Hide",hide_board_hnd,"b" }
	};
	g_action_map_add_action_entries( G_ACTION_MAP( self ),actions,2,self );
}

static void instance_finalize( GbdApp* self ) {
	GbdAppPrivate* const priv = self->priv;
	g_object_unref( priv->settings );

	G_OBJECT_CLASS( g_type_class_peek( G_TYPE_OBJECT ) )->finalize( G_OBJECT( self ) );
}

static void startup( GApplication* _self ) {
	G_APPLICATION_CLASS( g_type_class_peek( GTK_TYPE_APPLICATION ) )->startup( _self );
	GbdApp* const self = GBD_APP( _self );
	GbdAppPrivate* const priv = self->priv;

	g_application_hold( _self );
	priv->tray = gtk_status_icon_new_from_stock( GTK_STOCK_EDIT );
	g_signal_connect( priv->tray,"activate",(GCallback)toggle_board_hnd,self );

	priv->window = GTK_WINDOW( gtk_window_new( GTK_WINDOW_TOPLEVEL ) );
	gtk_window_set_title( priv->window,"GBoard" );
	gtk_window_set_accept_focus( priv->window,FALSE );
	gtk_window_set_keep_above( priv->window,TRUE );
	gtk_window_stick( priv->window );
	gtk_window_set_skip_taskbar_hint( priv->window,TRUE );
	gtk_window_set_decorated( priv->window,FALSE );
	g_signal_connect( priv->window,"delete-event",(GCallback)hide_board_hnd2,self );

	priv->emitter = GBD_EMITTER( gbd_x11emitter_new( ) );
	g_datalist_init( &priv->layouts );
	priv->keyboard = gbd_keyboard_new( NULL );
	gtk_widget_set_events( GTK_WIDGET( priv->keyboard ),GDK_BUTTON_PRESS_MASK|GDK_EXPOSURE_MASK );

	gtk_container_add( GTK_CONTAINER( priv->window ),GTK_WIDGET( priv->keyboard ) );
	gtk_widget_show( GTK_WIDGET( priv->keyboard ) );

	g_signal_connect( priv->keyboard,"mask-change",(GCallback)update_regions,_self );
}

static void activate( GApplication* _self ) {
	G_APPLICATION_CLASS( g_type_class_peek( GTK_TYPE_APPLICATION ) )->activate( _self );

	// TODO - Do we need activation anywhere?
	g_print( "GBoard activated\n" );
}

static gint command_line( GApplication* _self,GApplicationCommandLine* cl ) {
	GbdAppPrivate* const priv = GBD_APP( _self )->priv;
	
	gint argc;
	gchar** argv = g_application_command_line_get_arguments( cl,&argc );

	GOptionContext* oc = g_option_context_new( "" );

	gchar** layouts = NULL;
	gboolean force = FALSE;

	GOptionEntry options[ ]= {
		{ "layout",'l',0,G_OPTION_ARG_FILENAME_ARRAY,&layouts,"Layout to use. Multiple layouts are preloaded, the last is activated","layout" },
		{ "force",'f',0,G_OPTION_ARG_NONE,&force,"Force reload of layout. Do not use cached layout",NULL },
		NULL
	};

	g_option_context_add_main_entries( oc,options,NULL );
	GError* err = NULL;
	if( g_option_context_parse( oc,&argc,&argv,&err ) ) {

		guint i = 0;
		GbdLayout* lastvalid = NULL;
		while( layouts && layouts[ i ] ) {
			GFile* file = g_file_new_for_commandline_arg( layouts[ i ] );
			if( !load_layout( GBD_APP( _self ),file,force,&err ) ) {
				g_critical( "GBoard could not parse layoutfile '%s': %s",layouts[ i ],err->message );
				g_error_free( err );
				err = NULL;
			}
			g_object_unref( file );
			i++;
		}
		g_strfreev( layouts );
	} else {
		g_error( "GBoard could not parse commandline: %s",err->message );
		g_error_free( err );
		err = NULL;
	}

	g_option_context_free( oc );

	return 0;
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
	GbdAppPrivate* const priv = GBD_APP( _self )->priv;

	if( !g_strcmp0( prop,"Visible" ) )
		return g_variant_new( "y",priv->visible?priv->pen?2:1:0 );

	return NULL;
}

static gboolean* dbus_property_set( GDBusConnection* conn,gchar* send,gchar* path,gchar* name,gchar* prop,GVariant* val,GError* err,gpointer _self ) {
	// Nothing to see here, move along
}

static gboolean load_layout( GbdApp* self,GFile* file,gboolean force,GError** err ) {
	GbdAppPrivate* const priv = self->priv;

	gchar* fileuri = g_file_get_uri( file );
	GbdLayout* layout;

	if( force || !(layout = g_datalist_get_data( &priv->layouts,fileuri ) ) ) {
		gchar* layoutstring;
		if( g_file_load_contents( file,NULL,&layoutstring,NULL,NULL,err ) ) {
			layout = gbd_layout_new( layoutstring,priv->emitter );
			g_datalist_set_data_full( &priv->layouts,fileuri,layout,g_object_unref );
			g_free( layoutstring );
		} else
			return FALSE;
	} else
		g_message( "Using cached version of '%s'",fileuri );

	g_free( fileuri );

	guint width,height;
	g_object_get( layout,"width",&width,"height",&height,NULL );

	gdouble aspect = width/(gdouble)height;

	GdkGeometry geom = { .min_aspect = aspect,.max_aspect = aspect };
	gtk_window_set_geometry_hints( priv->window,NULL,&geom,GDK_HINT_ASPECT );

	g_object_set( priv->keyboard,"layout",layout,NULL );

	return TRUE;
}

static void show_board( GbdApp* self ) {
	GbdAppPrivate* const priv = self->priv;
	priv->visible = TRUE;

	gtk_widget_show( GTK_WIDGET( priv->window ) );
}

static void hide_board( GbdApp* self ) {
	GbdAppPrivate* const priv = self->priv;
	priv->visible = FALSE;

	gtk_widget_hide( GTK_WIDGET( priv->window ) );
}

static void toggle_board_hnd( GtkStatusIcon* icon,GbdApp* self ) {
	GbdAppPrivate* const priv = self->priv;
	
	if( priv->visible )
		hide_board( self );
	else
		show_board( self );
}

/** TODO
 * Usually, GbdApp should be able to just set the window shape to the
 * children's (i.e. GbdKeyboard's) ones by means of
 * gdk_window_set_child_shapes. However, said function seems to invert
 * the mask which GbdKeyboard sets on itsself. Meaning that if
 * GbdKeyboard sets a mask to show all keys and hide the space between,
 * the Toplevel will decide to hide all keys and show the gaps, instead.
 * That's presumably a bug. However, by only setting the mask in the
 * Toplevel, we also forgo the unnecessary overhead of setting a mask
 * twice, which is a good thing.
 */
static void update_regions( GbdKeyboard* keyboard,GbdApp* self ) {
	cairo_region_t* regions = gbd_keyboard_regions( keyboard );
	GdkWindow* win;
	if( regions &&( win = gtk_widget_get_window( GTK_WIDGET( self->priv->window ) ) ) ) {
		gdk_window_shape_combine_region( win,regions,0,0 );
		cairo_region_destroy( regions );
	}
}

static void show_board_hnd( GSimpleAction* action,GVariant* parms,gpointer _self ) {
	show_board( GBD_APP( _self ) );
}

static void hide_board_hnd( GSimpleAction* action,GVariant* parms,gpointer _self ) {
	hide_board( GBD_APP( _self ) );
}

static gboolean hide_board_hnd2( GtkWidget* widget,GdkEvent* ev,GbdApp* self ) {
	hide_board( self );
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
	GbdApp* self = g_object_new( GBD_TYPE_APP,"application-id",GBD_NAME,"flags",G_APPLICATION_HANDLES_COMMAND_LINE,NULL );

	// TODO : Move into dbus_register with Glib-2.34
	dbus_register( G_APPLICATION( self ),g_bus_get_sync( G_BUS_TYPE_SESSION,NULL,NULL ),NULL,NULL );

	return self;
}
