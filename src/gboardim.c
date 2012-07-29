#include <gtk/gtk.h>
#include <gtk/gtkimmodule.h>

#include <gbd_config.h>

/// Return GType; Return Class
#define GBD_TYPE_IM (gbd_im_get_type( ))
#define GBD_IM_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS( (obj),GBD_TYPE_IM,GbdIMClass ))

/// Check whether instance is derived; Cast it
#define GBD_IS_IM(obj) (G_TYPE_CHECK_INSTANCE_TYPE( (obj),GBD_TYPE_IM ))
#define GBD_IM(obj) (G_TYPE_CHECK_INSTANCE_CAST( (obj),GBD_TYPE_IM,GbdIM ))

/// Check whether class is derived; Cast it
#define GBD_IS_IM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE( (klass),GBD_TYPE_IM ))
#define GBD_IM_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST( (klass),GBD_TYPE_IM,GbdIMClass ))

typedef enum {
	GBD_IM_AUTO,
	GBD_IM_KEY,
	GBD_IM_PEN
} GbdImMode;

typedef GtkIMContextClass GbdIMClass;

typedef struct {
	GtkIMContext parent;
	GDBusProxy* board;
	GdkWindow* client;
	GtkWidget* launcher;
	gboolean focused;
}	GbdIM;

static GtkIMContextInfo* clist[ ]= {
	&(GtkIMContextInfo){ "gboard","GBoard","gboard","","*" },
};

static GType type = 0;

static GdkPixbuf* pixbuf;

static void class_init( GbdIMClass*,gpointer );
static void instance_init( GbdIM* self );
static void instance_finalize( GbdIM* self );

static void set_client_window( GtkIMContext*,GdkWindow* );
static void focus_in( GtkIMContext* );
static void focus_out( GtkIMContext* );

static void proxy_aquired( GObject*,GAsyncResult*,GbdIM* );
static void show_launcher( GbdIM* );
static void launch_gboard( GtkButton*,GbdIM* );
static gboolean filter_keypress( GtkIMContext*,GdkEventKey* );

static GType gbd_im_get_type( ) {
	return type;
}

static void class_init( GbdIMClass* klass,gpointer udata ) {
	GObjectClass* klass_go = G_OBJECT_CLASS( klass );
	GtkIMContextClass* klass_ic = GTK_IM_CONTEXT_CLASS( klass );

	klass_go->finalize = (GObjectFinalizeFunc)instance_finalize;

	klass_ic->set_client_window = set_client_window;
	klass_ic->focus_in = focus_in;
	klass_ic->focus_out = focus_out;
	klass_ic->filter_keypress = filter_keypress;
}

static void instance_init( GbdIM* self ) {
	self->launcher = NULL;

	g_dbus_proxy_new_for_bus( G_BUS_TYPE_SESSION,G_DBUS_PROXY_FLAGS_NONE,NULL,GBD_NAME,GBD_PATH,GBD_NAME,NULL,(GAsyncReadyCallback)proxy_aquired,self );
}

static void instance_finalize( GbdIM* self ) {
	if( self->launcher )
		gtk_widget_destroy( self->launcher );
	g_object_unref( self->board );

	G_OBJECT_CLASS( g_type_class_peek( GTK_TYPE_IM_CONTEXT ) )->dispose( G_OBJECT( self ) );
}

static void set_client_window( GtkIMContext* _self,GdkWindow* win ) {
	GbdIM* self = GBD_IM( _self );
	self->client = win;
}

static void focus_in( GtkIMContext* _self ) {
	GbdIM* self = GBD_IM( _self );
	self->focused = TRUE;

	if( self->board ) {
		GVariant* visible = g_dbus_proxy_get_cached_property( self->board,"Visible" );
		if( visible ) {
			guchar visibility = g_variant_get_boolean( visible );
			g_variant_unref( visible );

			if( !visibility )
				show_launcher( self );
		} else
			g_message( "GBoard IM: GBoard could not be contacted (yet)\n" );
	}
}

static void focus_out( GtkIMContext* _self ) {
	GbdIM* self = GBD_IM( _self );
	self->focused = FALSE;

	if( self->launcher )
		gtk_widget_hide( self->launcher );
}

static void show_launcher( GbdIM* self ) {
	if( !self->launcher ) {
		self->launcher = gtk_window_new( GTK_WINDOW_POPUP );
		GtkWidget* button = gtk_button_new( );
		GtkWidget* image = gtk_image_new_from_pixbuf( pixbuf );
		gtk_container_add( GTK_CONTAINER( button ),image );
		gtk_container_add( GTK_CONTAINER( self->launcher ),button );

		g_signal_connect( button,"clicked",(GCallback)launch_gboard,self );

		gtk_widget_show_all( button );
	}
	
	gint x,y,w,h;
	gtk_window_get_size( GTK_WINDOW( self->launcher ),&w,&h );
	gdk_window_get_origin( self->client,&x,&y );

	cairo_region_t* reg = gdk_window_get_visible_region( self->client );
	cairo_rectangle_int_t rec;
	cairo_region_get_rectangle( reg,0,&rec );
	cairo_region_destroy( reg );

	gtk_window_move( GTK_WINDOW( self->launcher ),x,y+rec.height );
	gtk_widget_show( self->launcher );
}

static void hide_launcher( GbdIM* self ) {
	if( self->launcher )
		gtk_widget_hide( self->launcher );
}

static void launch_gboard( GtkButton* button,GbdIM* self ) {
	if( self->board )
		g_dbus_proxy_call( self->board,"Show",NULL,0,-1,NULL,NULL,NULL );
}

static void property_changed_hnd( GDBusProxy *proxy,GVariant* props,GStrv* invalidated,GbdIM* self ) {
	GVariantIter* iter;
	const gchar* name;
	GVariant* value;

	g_variant_get( props,"a{sv}",&iter );
	while( g_variant_iter_loop( iter,"{&sv}",&name,&value ) ) {
		if( !g_strcmp0( "Visible",name ) ) {
			gboolean visible = g_variant_get_boolean( value );
			if( visible )
				hide_launcher( self );
			else
				show_launcher( self );
		}
	}
	g_variant_iter_free( iter );
}

static void proxy_aquired( GObject* src,GAsyncResult* res,GbdIM* self ) {
	self->board = g_dbus_proxy_new_for_bus_finish( res,NULL );

	g_signal_connect( self->board,"g-properties-changed",(GCallback)property_changed_hnd,self );

	if( self->focused )
		focus_in( GTK_IM_CONTEXT( self ) );
}

static gboolean filter_keypress( GtkIMContext* _self,GdkEventKey* ev ) {
	return FALSE;
}

void im_module_init( GTypeModule* module ) {
	const GTypeInfo info = {
		sizeof( GbdIMClass ),
		NULL,NULL,
		(GClassInitFunc)class_init,NULL,NULL,
		sizeof( GbdIM ),0,
		(GInstanceInitFunc)instance_init,
		NULL
	};

	type = g_type_module_register_type( module,GTK_TYPE_IM_CONTEXT,"GbdIM",&info,0 );

	GSettings* settings = g_settings_new( GBD_NAME );
	gchar* statusicon = g_settings_get_string( settings,"statusicon" );
	pixbuf = gdk_pixbuf_new_from_file_at_scale( statusicon,32,32,TRUE,NULL );
	g_free( statusicon );
	g_object_unref( settings );

}

void im_module_exit( ) {
	g_object_unref( pixbuf );
}

void im_module_list( GtkIMContextInfo*** contexts,int* contextcount ) {
	*contexts = clist;
	*contextcount = 1;
}

GtkIMContext* im_module_create( const gchar* name ) {
	GbdIM* result = g_object_new( GBD_TYPE_IM,NULL );

	return GTK_IM_CONTEXT( result );
}
