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
	GtkDialog* prefwindow;
	GbdEmitter* emitter;
	GbdKeyboard* keyboard;
	GData* layouts;
	gboolean visible;
	GSettings* settings;
	GdkPixbuf* icon;
	GDBusConnection* connection;
	GtkMenu* menu;

	/// Cached settings
	gboolean shaped,hardhide,reset,north;
};

static GDBusSignalInfo* sinfo[ ] = {
	&(GDBusSignalInfo){ -1,"Submit",(GDBusArgInfo*[ ]){ &(GDBusArgInfo){ -1,"Text","s" },NULL } },
	NULL
};

static GDBusPropertyInfo* pinfo[ ] = {
	&(GDBusPropertyInfo){ -1,"Visible","b",G_DBUS_PROPERTY_INFO_FLAGS_READABLE },
	NULL
};

static GDBusMethodInfo* minfo[ ] = {
	&(GDBusMethodInfo){ -1,"Show",NULL,NULL,NULL },
	&(GDBusMethodInfo){ -1,"Hide",NULL,NULL,NULL },
	NULL
};

static GDBusInterfaceInfo iinfo = { -1,GBD_NAME,minfo,sinfo,pinfo,NULL };

static GdkPixbuf* iconpixbuf;

static void class_init( GbdAppClass*,gpointer );
static void instance_init( GbdApp* );
static void instance_finalize( GbdApp* );

static void startup( GApplication* );
static gint command_line( GApplication*,GApplicationCommandLine* );
static gboolean dbus_register( GApplication*,GDBusConnection*,const gchar*,GError** );
static void dbus_unregister( GApplication*,GDBusConnection*,const gchar* );

static void dbus_method_call( GDBusConnection*,gchar*,gchar*,gchar*,gchar*,GVariant*,GDBusMethodInvocation*,GbdApp* );
static GVariant* dbus_property_get( GDBusConnection*,gchar*,gchar*,gchar*,gchar*,GError*,gpointer );
static gboolean* dbus_property_set( GDBusConnection*,gchar*,gchar*,gchar*,gchar*,GVariant*,GError*,gpointer );

static void show_board( GbdApp* );
static void hide_board( GbdApp* );
static void configure_window( GbdApp* );
static void prefs_hnd( GSettings*,gchar*,GbdApp* );
static void file_set_hnd( GtkFileChooserButton*,GbdApp* );
static gboolean popup_hnd( GtkStatusIcon*,guint,guint,GbdApp* );
static void screen_hnd( GdkScreen*,GbdApp* );
static gboolean close_pref_hnd( GtkButton*,GdkEvent*,GbdApp* );
static void connect_popup_signals( GtkBuilder*,GObject*,const gchar*,const gchar*,GObject*,GConnectFlags,GbdApp* );
static void close_pref_hnd2( GtkButton*,GbdApp* );
static void confirm_pref_hnd( GtkButton*,GbdApp* );
static gboolean load_layout( GbdApp*,GFile*,gboolean,GError** );
static void activate_layout( GbdApp*,GbdLayout* );
static void show_prefs( GbdApp* );
static void update_regions( GbdKeyboard*,GbdApp* );
static void toggle_board_hnd( GtkStatusIcon*,GbdApp* );
static gboolean hide_board_hnd( GtkWidget*,GdkEvent*,GbdApp* );

static void class_init( GbdAppClass* klass,gpointer udata ) {
	GApplicationClass* klass_ga = G_APPLICATION_CLASS( klass );
	GObjectClass* klass_go = G_OBJECT_CLASS( klass );

	g_type_class_add_private( klass,sizeof( GbdAppPrivate ) );

	klass_go->finalize = (GObjectFinalizeFunc)instance_finalize;

	klass_ga->startup = startup;
	klass_ga->command_line = command_line;

	// TODO : Comes with Glib-2.34
	//klass_ga->dbus_register = dbus_register;
	//klass_ga->dbus_unregister = dbus_unregister;
}

static void instance_init( GbdApp* self ) {
	GbdAppPrivate* const priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self,GBD_TYPE_APP,GbdAppPrivate );
}

static void instance_finalize( GbdApp* self ) {
	GbdAppPrivate* const priv = self->priv;

/// Settings being set indicates this is the primary instance
	if( priv->settings ) {
		g_object_unref( priv->settings );
		g_object_unref( priv->tray );
		if( priv->icon )
			g_object_unref( priv->icon );
	}

	G_OBJECT_CLASS( g_type_class_peek( G_TYPE_OBJECT ) )->finalize( G_OBJECT( self ) );
}

static void startup( GApplication* _self ) {
	G_APPLICATION_CLASS( g_type_class_peek( GTK_TYPE_APPLICATION ) )->startup( _self );
	GbdApp* const self = GBD_APP( _self );
	GbdAppPrivate* const priv = self->priv;

	priv->settings = g_settings_new( GBD_NAME );

	gchar* statusicon = g_settings_get_string( priv->settings,"statusicon" );
	priv->icon = gdk_pixbuf_new_from_file( statusicon,NULL );
	g_free( statusicon );

	priv->tray = gtk_status_icon_new_from_pixbuf( priv->icon );

	g_signal_connect( priv->tray,"activate",(GCallback)toggle_board_hnd,self );
	g_signal_connect( priv->tray,"popup-menu",(GCallback)popup_hnd,self );

	GtkBuilder* builder = gtk_builder_new( );
	GError* err = NULL;
	if( !gtk_builder_add_from_file( builder,GBD_POPUPDEF,&err ) ) {
		g_error( "Could not load GtkBuilder definitions: %s'",err->message );
		g_error_free( err );
		return;
	}
	priv->menu = g_object_ref( GTK_MENU( gtk_builder_get_object( builder,"menu" ) ) );
	gtk_builder_connect_signals_full( builder,(GtkBuilderConnectFunc)connect_popup_signals,self );
	gtk_widget_show_all( GTK_WIDGET( priv->menu ) );

	g_object_unref( builder );

	priv->window = GTK_WINDOW( gtk_window_new( GTK_WINDOW_TOPLEVEL ) );
	gtk_window_set_title( priv->window,"GBoard" );
	gtk_window_set_skip_taskbar_hint( priv->window,TRUE );
	gtk_window_set_decorated( priv->window,FALSE );

	g_signal_connect( priv->window,"delete-event",(GCallback)hide_board_hnd,self );
	g_signal_connect( gdk_screen_get_default( ),"size-changed",(GCallback)screen_hnd,self );

	priv->emitter = GBD_EMITTER( gbd_x11emitter_new( ) );
	g_datalist_init( &priv->layouts );
	priv->keyboard = gbd_keyboard_new( NULL );
	gtk_widget_set_events( GTK_WIDGET( priv->keyboard ),GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK|GDK_EXPOSURE_MASK );

	gtk_container_add( GTK_CONTAINER( priv->window ),GTK_WIDGET( priv->keyboard ) );
	gtk_widget_show( GTK_WIDGET( priv->keyboard ) );

	g_settings_bind( priv->settings,"xpadding",priv->keyboard,"xpadding",G_SETTINGS_BIND_DEFAULT );
	g_settings_bind( priv->settings,"ypadding",priv->keyboard,"ypadding",G_SETTINGS_BIND_DEFAULT );
	g_settings_bind( priv->settings,"fontsize",priv->keyboard,"fontsize",G_SETTINGS_BIND_DEFAULT );
	g_settings_bind( priv->settings,"relative",priv->keyboard,"relative",G_SETTINGS_BIND_DEFAULT );

	priv->reset = g_settings_get_boolean( priv->settings,"reset" );
	priv->north = g_settings_get_boolean( priv->settings,"north" );
	priv->shaped = g_settings_get_boolean( priv->settings,"shaped" );
	priv->hardhide = g_settings_get_boolean( priv->settings,"hardhide" );
	g_signal_connect( priv->settings,"changed",(GCallback)prefs_hnd,self );

	g_signal_connect( priv->keyboard,"mask-change",(GCallback)update_regions,_self );

	gchar* defaultlayout = g_settings_get_string( priv->settings,"layout" );
	GFile* file = g_file_new_for_uri( defaultlayout );
	load_layout( self,file,FALSE,NULL );
	g_object_unref( file );
	g_free( defaultlayout );

	gtk_application_add_window( GTK_APPLICATION( _self ),priv->window );

	if( !priv->hardhide ) {
		gtk_window_iconify( priv->window );
		gtk_widget_show( GTK_WIDGET( priv->window ) );
	}
}

static void popup_menu_hnd( GtkMenuItem* item,GbdApp* self ) {
	gchar* label;
	g_object_get( item,"label",&label,NULL );

	if( !g_strcmp0( label,"gtk-preferences" ) )
		show_prefs( self );
	else if( !g_strcmp0( label,"gtk-about" ) ) {
		const gchar* authors[ ]= { "Cedric Sodhi <manday@gmx.net>",NULL };

		gtk_show_about_dialog( NULL,
			"program-name","GBoard",
			"license-type",GTK_LICENSE_GPL_2_0,
			"logo",self->priv->icon,
			"version",GBD_VERSION,
			"website","http://github.com/ManDay/gboard",
			"website-label","Hosted on Github",
			"wrap-license",TRUE,
			"authors",authors,
			"comments","Lightweight, simple, and flexible on-screen-keyboard written in GTK.",
			"copyright","As by GPL-2 License",NULL );
	} else if( !g_strcmp0( label,"gtk-quit" ) )
		g_application_quit( G_APPLICATION( self ) );
	else
		if( self->priv->visible )
			hide_board( self );
		else
			show_board( self );
	
	g_free( label );
}

static void connect_popup_signals( GtkBuilder* build,GObject* obj,const gchar* sig,const gchar* handler,GObject* target,GConnectFlags flags,GbdApp* self ) {
	g_signal_connect( obj,sig,(GCallback)popup_menu_hnd,self );
}

static void change_visibility( GbdApp* self,gboolean visibility ) {
	GbdAppPrivate* const priv = self->priv;

	priv->visible = visibility;

	GVariantBuilder* builder = g_variant_builder_new( G_VARIANT_TYPE_ARRAY ); 
	g_variant_builder_add( builder,"{sv}","Visible",g_variant_new_boolean( visibility ) );
	GVariant* parms = g_variant_new( "(sa{sv}as)",GBD_NAME,builder,NULL );
	g_variant_builder_unref( builder );

	GError* err = NULL;
	if( !g_dbus_connection_emit_signal( priv->connection,NULL,GBD_PATH,"org.freedesktop.DBus.Properties","PropertiesChanged",parms,&err ) ) {
		g_error( "Could not emit signal: %s",err->message );
		g_error_free( err );
	}
}

static gint command_line( GApplication* _self,GApplicationCommandLine* cl ) {
	GbdApp* self = GBD_APP( _self );
	GbdAppPrivate* const priv = self->priv;
	
	gint argc;
	gchar** argv = g_application_command_line_get_arguments( cl,&argc );

	GOptionContext* oc = g_option_context_new( "" );

	gchar** layouts = NULL;
	gboolean force = FALSE,
		prefs = FALSE,
		hide = FALSE,
		show = FALSE,
		toggle = FALSE;

	GOptionEntry options[ ]= {
		{ "force",'f',0,G_OPTION_ARG_NONE,&force,"Force reload of layout. Do not use cached layout.",NULL },
		{ "toggle",'t',0,G_OPTION_ARG_NONE,&toggle,"Toggle GBoard.",NULL },
		{ "hide",'h',0,G_OPTION_ARG_NONE,&hide,"Hide GBoard.",NULL },
		{ "show",'s',0,G_OPTION_ARG_NONE,&show,"Show GBoard. Use together with -h to redock, if docking is enabled.",NULL },
		{ "preferences",'p',0,G_OPTION_ARG_NONE,&prefs,"Display Preferences dialog.",NULL },
		NULL
	};

	g_option_context_add_main_entries( oc,options,NULL );
	GError* err = NULL;
	if( g_option_context_parse( oc,&argc,&argv,&err ) ) {

		guint i;
		GbdLayout* lastvalid = NULL;
		for( i = 1; i<argc; i++ ) {
			GFile* file = g_file_new_for_commandline_arg( argv[ i ] );
			if( !load_layout( self,file,force,&err ) ) {
				g_critical( "GBoard could not parse layoutfile '%s': %s",argv[ i ],err->message );
				g_error_free( err );
				err = NULL;
			}
			g_object_unref( file );
		}
		g_strfreev( layouts );

		if( prefs )
			show_prefs( self );
		if( toggle )
			if( priv->visible )
				hide_board( self );
			else
				show_board( self );
		else {
			if( hide )
				hide_board( self );
			if( show )
				show_board( self );
		}
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
		(GDBusInterfaceMethodCallFunc)dbus_method_call,
		(GDBusInterfaceGetPropertyFunc)dbus_property_get,
		(GDBusInterfaceSetPropertyFunc)dbus_property_set
	};

	g_dbus_connection_register_object( conn,GBD_PATH,&iinfo,&vtable,_self,NULL,NULL );
	GBD_APP( _self )->priv->connection = conn;
}

static void dbus_unregister( GApplication* _self,GDBusConnection* conn,const gchar* dbapi ) {
}

static void dbus_method_call( GDBusConnection* conn,gchar* send,gchar* path,gchar* ifname,gchar* mname,GVariant* parms,GDBusMethodInvocation* minv,GbdApp* self ) {
	if( !g_strcmp0( mname,"Show" ) )
		show_board( self );
	else if( !g_strcmp0( mname,"Hide" ) )
		hide_board( self );

	g_dbus_method_invocation_return_value( minv,NULL );
}

static GVariant* dbus_property_get( GDBusConnection* conn,gchar* send,gchar* path,gchar* name,gchar* prop,GError* err,gpointer _self ) {
	GbdAppPrivate* const priv = GBD_APP( _self )->priv;

	if( !g_strcmp0( prop,"Visible" ) )
		return g_variant_new( "b",priv->visible );

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

	activate_layout( self,layout );

	return TRUE;
}

static void activate_layout( GbdApp* self,GbdLayout* layout ) {
	GbdAppPrivate* const priv = self->priv;

	g_object_set( priv->keyboard,"layout",layout,NULL );
	configure_window( self );
}

static gboolean popup_hnd( GtkStatusIcon* icon,guint button,guint t,GbdApp* self ) {
	gtk_menu_popup( self->priv->menu,NULL,NULL,gtk_status_icon_position_menu,icon,button,t );
}

static void show_board( GbdApp* self ) {
	GbdAppPrivate* const priv = self->priv;
	change_visibility( self,TRUE );

	if( priv->reset )
		configure_window( self );

	if( !priv->hardhide )
		gtk_window_deiconify( priv->window );
	else
		gtk_widget_show( GTK_WIDGET( priv->window ) );
}

static void hide_board( GbdApp* self ) {
	GbdAppPrivate* const priv = self->priv;
	change_visibility( self,FALSE );

	if( priv->hardhide )
		gtk_widget_hide( GTK_WIDGET( priv->window ) );
	else
		gtk_window_iconify( priv->window );
}

static void show_prefs( GbdApp* self ) {
	GbdAppPrivate* const priv = self->priv;

	if( !priv->prefwindow ) {
		GtkBuilder* builder = gtk_builder_new( );
		GError* err = NULL;
		if( !gtk_builder_add_from_file( builder,GBD_GUIDEF,&err ) ) {
			g_error( "Could not load GtkBuilder definitions: %s'",err->message );
			g_error_free( err );
			return;
		}
		priv->prefwindow = GTK_DIALOG( gtk_builder_get_object( builder,"window" ) );

		gtk_window_set_icon( GTK_WINDOW( priv->prefwindow ),priv->icon );

		g_signal_connect( priv->prefwindow,"delete-event",(GCallback)close_pref_hnd,self );
		g_signal_connect( gtk_builder_get_object( builder,"cancel" ),"clicked",(GCallback)close_pref_hnd2,self );
		g_signal_connect( gtk_builder_get_object( builder,"ok" ),"clicked",(GCallback)confirm_pref_hnd,self );


		GtkWidget* reset = GTK_WIDGET( gtk_builder_get_object( builder,"reset" ) );
		GtkWidget* north = GTK_WIDGET( gtk_builder_get_object( builder,"north" ) );
		GtkFileChooser* fc = GTK_FILE_CHOOSER( gtk_builder_get_object( builder,"layout" ) );

		gchar* const uri = g_settings_get_string( priv->settings,"layout" );
		gtk_file_chooser_set_uri( fc,uri );
		g_free( uri );
		g_signal_connect( fc,"file-set",(GCallback)file_set_hnd,self );


		g_object_bind_property( reset,"active",north,"sensitive",G_BINDING_DEFAULT|G_BINDING_SYNC_CREATE );

		g_settings_delay( priv->settings );
		g_settings_bind( priv->settings,"reset",reset,"active",G_SETTINGS_BIND_DEFAULT );
		g_settings_bind( priv->settings,"north",north,"active",G_SETTINGS_BIND_DEFAULT|G_SETTINGS_BIND_NO_SENSITIVITY );
		g_settings_bind( priv->settings,"xpadding",gtk_builder_get_object( builder,"xpadding" ),"value",G_SETTINGS_BIND_DEFAULT );
		g_settings_bind( priv->settings,"ypadding",gtk_builder_get_object( builder,"ypadding" ),"value",G_SETTINGS_BIND_DEFAULT );
		g_settings_bind( priv->settings,"keywidth",gtk_builder_get_object( builder,"keywidth" ),"value",G_SETTINGS_BIND_DEFAULT );
		g_settings_bind( priv->settings,"keyheight",gtk_builder_get_object( builder,"keyheight" ),"value",G_SETTINGS_BIND_DEFAULT );
		g_settings_bind( priv->settings,"fontsize",gtk_builder_get_object( builder,"fontsize" ),"value",G_SETTINGS_BIND_DEFAULT );
		g_settings_bind( priv->settings,"maxheight",gtk_builder_get_object( builder,"maxheight" ),"value",G_SETTINGS_BIND_DEFAULT );
		g_settings_bind( priv->settings,"relative",gtk_builder_get_object( builder,"relative" ),"active",G_SETTINGS_BIND_DEFAULT );
		g_settings_bind( priv->settings,"shaped",gtk_builder_get_object( builder,"shaped" ),"active",G_SETTINGS_BIND_DEFAULT );
		g_settings_bind( priv->settings,"hardhide",gtk_builder_get_object( builder,"hardhide" ),"active",G_SETTINGS_BIND_DEFAULT );

		gtk_widget_show_all( GTK_WIDGET( priv->prefwindow ) );

		g_object_unref( builder );
	}
}

static void prefs_hnd( GSettings* settings,gchar* key,GbdApp* self ) {
	GbdAppPrivate* const priv = self->priv;

	if( !g_strcmp0( key,"shaped" ) ) {
		priv->shaped = g_settings_get_boolean( priv->settings,"shaped" );
		GdkWindow* win;
		if( priv->shaped )
			update_regions( priv->keyboard,self );
		else if( win = gtk_widget_get_window( GTK_WIDGET( priv->window ) ) )
			gdk_window_shape_combine_region( win,NULL,0,0 );
	} else if( !g_strcmp0( key,"keywidth" )|| !g_strcmp0( key,"keyheight" ) )
		configure_window( self );
	else if( !g_strcmp0( key,"hardhide" ) ) {
		priv->hardhide = g_settings_get_boolean( priv->settings,"hardhide" );
		if( !priv->visible )
			if( priv->hardhide )
				gtk_widget_hide( GTK_WIDGET( priv->window ) );
			else {
				gtk_window_iconify( priv->window );
				gtk_widget_show( GTK_WIDGET( priv->window ) );
			}
	} else if( !g_strcmp0( key,"reset" ) )
		priv->reset = g_settings_get_boolean( priv->settings,"reset" );
	else if( !g_strcmp0( key,"north" ) )
		priv->north = g_settings_get_boolean( priv->settings,"north" );
}

static void file_set_hnd( GtkFileChooserButton* widget,GbdApp* self ) {
	gchar* uri = gtk_file_chooser_get_uri( GTK_FILE_CHOOSER( widget ) );
	g_settings_set_string( self->priv->settings,"layout",uri );
	g_free( uri );
}

static gboolean close_pref_hnd( GtkButton* button,GdkEvent* ev,GbdApp* self ) {
	g_settings_revert( self->priv->settings );
	self->priv->prefwindow = NULL;

	return FALSE;
}

static void close_pref_hnd2( GtkButton* button,GbdApp* self ) {
	gtk_widget_destroy( GTK_WIDGET( self->priv->prefwindow ) );

	g_settings_revert( self->priv->settings );
	self->priv->prefwindow = NULL;
}

static void confirm_pref_hnd( GtkButton* button,GbdApp* self ) {
	gtk_widget_destroy( GTK_WIDGET( self->priv->prefwindow ) );

	g_settings_apply( self->priv->settings );
	self->priv->prefwindow = NULL;
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
	if( self->priv->shaped ) {
		cairo_region_t* regions = gbd_keyboard_regions( keyboard );
		GdkWindow* win;
		if( regions &&( win = gtk_widget_get_window( GTK_WIDGET( self->priv->window ) ) ) ) {
			gdk_window_shape_combine_region( win,regions,0,0 );
			cairo_region_destroy( regions );
		}
	}
}

static gboolean hide_board_hnd( GtkWidget* widget,GdkEvent* ev,GbdApp* self ) {
	hide_board( self );
}

static void configure_window( GbdApp* self ) {
	GbdAppPrivate* const priv = self->priv;

	gtk_window_set_accept_focus( priv->window,FALSE );
	gtk_window_set_keep_above( priv->window,TRUE );
	gtk_window_stick( priv->window );

	GbdLayout* layout;
	guint cols,rows;

	g_object_get( priv->keyboard,"layout",&layout,NULL );
	g_object_get( layout,"width",&cols,"height",&rows,NULL );

	const guint keywidth = g_settings_get_int( priv->settings,"keywidth" );
	const guint keyheight = g_settings_get_int( priv->settings,"keyheight" );
	const gdouble maxheight = g_settings_get_double( priv->settings,"maxheight" )*gdk_screen_height( );

	gdouble scale = 1;
	if( rows*keyheight>maxheight )
		scale = maxheight/( rows*keyheight );
	if( cols*keywidth*scale>gdk_screen_width( ) )
		scale = gdk_screen_width( )/(gdouble)( cols*keywidth );
	
	const gint height = rows*keyheight*scale;
	const gint width = cols*keywidth*scale;

/* One would think that setting the window's gravity to the edge on
 * which it docks makes sense. In fact, trying to do that will cause the
 * resize & move to fail. I cannot make sense of this, but it seems to
 * work without gravity, so I'll just go with that. */
	if( priv->north )
		gtk_window_move( priv->window,gdk_screen_width( )/2-width/2,0 );
	else
		gtk_window_move( priv->window,gdk_screen_width( )/2-width/2,gdk_screen_height( )-height );
	gtk_window_resize( priv->window,width,height );
}

static void screen_hnd( GdkScreen* screen,GbdApp* self ) {
	configure_window( self );
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
