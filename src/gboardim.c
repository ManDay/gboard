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
	GbdImMode mode;
	GDBusProxy* board;
	GdkWindow* client;
	GtkWidget* launcher;
	GDBusActionGroup* boardactions;
	gboolean actions_ready;

// TODO : Part of FIXME TODO for updating GDBusActionGroup correctly
	guint update_actions_watcher;
}	GbdIM;

GtkIMContextInfo* clist[ ]= {
	&(GtkIMContextInfo){ "gboard_auto","GBoard (automatic detection)","gboard","","*" },
	&(GtkIMContextInfo){ "gboard_key","GBoard (keyboard)","gboard","","*" },
	&(GtkIMContextInfo){ "gboard_pen","GBoard (pen)","gboard","","*" }
};

GType type = 0;

static void class_init( GbdIMClass*,gpointer );
static void instance_init( GbdIM* self );
static void instance_finalize( GbdIM* self );

static void set_client_window( GtkIMContext*,GdkWindow* );
static void focus_in( GtkIMContext* );
static void focus_out( GtkIMContext* );

static void proxy_aquired( GObject*,GAsyncResult*,GbdIM* );
static void show_launcher( GbdIM* );
static void launch_gboard( GtkButton*,GbdIM* );
static void boardactions_verify( GActionGroup*,gchar*,GbdIM* );

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
}

static void instance_init( GbdIM* self ) {
	g_print( "GbdIM %p: Initialized, aquiring proxy\n",self );

	self->launcher = NULL;
	self->actions_ready = FALSE;

	g_dbus_proxy_new_for_bus( G_BUS_TYPE_SESSION,G_DBUS_PROXY_FLAGS_NONE,NULL,GBD_NAME,GBD_PATH,GBD_NAME,NULL,(GAsyncReadyCallback)proxy_aquired,self );
}

static void instance_finalize( GbdIM* self ) {
	g_print( "GbdIM %p: Finalized\n",self );

	if( self->launcher )
		gtk_widget_destroy( self->launcher );
	g_object_unref( self->board );
	g_object_unref( self->boardactions );

// TODO : Part of FIXME TODO for updating GDBusActionGroup correctly
	if( self->update_actions_watcher )
		g_bus_unwatch_name( self->update_actions_watcher );

	G_OBJECT_CLASS( g_type_class_peek( GTK_TYPE_IM_CONTEXT ) )->dispose( G_OBJECT( self ) );
}

static void set_client_window( GtkIMContext* _self,GdkWindow* win ) {
	GbdIM* self = GBD_IM( _self );
	g_print( "GbdIM %p: Client Window changed to %p\n",self,win );

	self->client = win;

}

static void focus_in( GtkIMContext* _self ) {
	GbdIM* self = GBD_IM( _self );

	if( self->board ) {
	GVariant* visible = g_dbus_proxy_get_cached_property( self->board,"Visible" );
		if( visible ) {
			if( self->actions_ready ) {
				guchar visibility = g_variant_get_byte( visible );
				g_variant_unref( visible );

				if( !visibility )
					show_launcher( self );
			} else
				g_printerr( "GBoard IM: GBoard did not report the required actions (yet):\n" );
		} else
			g_printerr( "GBoard IM: GBoard could not be contacted (yet)\n" );
	} else
		g_printerr( "GBoard IM: Proxy to GBoard not available (yet)\n" );
}

static void focus_out( GtkIMContext* _self ) {
	GbdIM* self = GBD_IM( _self );

	if( self->launcher )
		gtk_widget_hide( self->launcher );
}

static void show_launcher( GbdIM* self ) {
	if( !self->launcher ) {
		self->launcher = gtk_window_new( GTK_WINDOW_POPUP );
		GtkWidget* button = gtk_button_new_with_label( "GBoard" );

		gtk_container_add( GTK_CONTAINER( self->launcher ),button );
		g_signal_connect( button,"clicked",(GCallback)launch_gboard,self );

		gtk_widget_show( button );
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

static void launch_gboard( GtkButton* button,GbdIM* self ) {
	g_action_group_activate_action( G_ACTION_GROUP( self->boardactions ),"Show",g_variant_new( "y",1 ) );
}

// TODO : Part of FIXME TODO for updating GDBusActionGroup correctly
static void update_actions( GDBusConnection* conn,const gchar *name,const gchar *unique,gpointer _self ) {
	GbdIM* self = GBD_IM( _self );
	g_print( "GBoard appeared on the Bus - updating actions\n" );
	g_object_unref( self->boardactions );
	self->boardactions = g_dbus_action_group_get( conn,GBD_NAME,GBD_PATH );
	g_signal_connect( self->boardactions,"action-added",(GCallback)boardactions_verify,self );
	boardactions_verify( G_ACTION_GROUP( self->boardactions ),NULL,self );
	g_strfreev( g_action_group_list_actions( G_ACTION_GROUP( self->boardactions ) ) );
}

static void proxy_aquired( GObject* src,GAsyncResult* res,GbdIM* self ) {
	self->board = g_dbus_proxy_new_for_bus_finish( res,NULL );

	GDBusConnection* conn = g_dbus_proxy_get_connection( self->board );
	self->boardactions = g_dbus_action_group_get( conn,GBD_NAME,GBD_PATH );

	g_signal_connect( self->boardactions,"action-added",(GCallback)boardactions_verify,self );

	// According to Spec, ActionGroup may already be populated
	boardactions_verify( G_ACTION_GROUP( self->boardactions ),NULL,self );
	
/* FIXME TODO : GDBusActionGroup has some *very* bizarre and seemingly
 * highly defective policy for updating its knowledge about the
 * remote's capabilities. E.g.: It does not attempt to update its
 * "cache" by itsself, but only if list_actions or query_actions is
 * inquired. Even further: query_actions refetches the information if a
 * certain action is not found in its cache, yet returns FALSE, as if
 * the action ultimately did not exist.
 * For the time being, we make the GDBusActioGroup inquire the remote by
 * asking for a list of actions (which initially returns as empty but
 * subsequently will cause the proper update).
 */
	g_strfreev( g_action_group_list_actions( G_ACTION_GROUP( self->boardactions ) ) );
/* FIXME TODO : Same here. It should not be the user's code's
 * responsibility to look out for changed on the bus relevant for the
 * GDBusActionGroup.
 */
	self->update_actions_watcher = g_bus_watch_name_on_connection( conn,GBD_NAME,G_BUS_NAME_WATCHER_FLAGS_NONE,update_actions,NULL,self,NULL );
}

static void boardactions_verify( GActionGroup* grp,gchar* act,GbdIM* self ) {
	self->actions_ready = g_action_group_has_action( grp,"Show" )&& g_action_group_has_action( grp,"Hide" );
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
}

void im_module_exit( ) {
	g_print( "Terminating GBoard IM Module\n" );
}

void im_module_list( GtkIMContextInfo*** contexts,int* contextcount ) {
	*contexts = clist;
	*contextcount = 3;
}

GtkIMContext* im_module_create( const gchar* name ) {
	GbdIM* result = g_object_new( GBD_TYPE_IM,NULL );

	if( !g_strcmp0( name,clist[ 0 ]->context_id ) )
		result->mode = GBD_IM_AUTO;
	else if( !g_strcmp0( name,clist[ 1 ]->context_id ) )
		result->mode = GBD_IM_KEY;
	else
		result->mode = GBD_IM_PEN;

	return GTK_IM_CONTEXT( result );
}
