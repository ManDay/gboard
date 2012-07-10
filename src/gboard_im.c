#include <gtk/gtk.h>
#include <gtk/gtkimmodule.h>

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
}	GbdIM;

GtkIMContextInfo* clist[ ]= {
	&(GtkIMContextInfo){ "gboard_auto","GBoard (automatic detection)","gboard","","*" },
	&(GtkIMContextInfo){ "gboard_key","GBoard (keyboard)","gboard","","*" },
	&(GtkIMContextInfo){ "gboard_pen","GBoard (pen)","gboard","","*" }
};

static void class_init( GbdIMClass*,gpointer );

static GType gbd_im_get_type( ) {
	static GType type = 0;
	if( !type ) {
		type = g_type_from_name( "GbdIM" );
	}
	return type;
}

static void set_client_window( GtkIMContext* self,GdkWindow* win ) {
	g_print( "GbdIM %p: Client Window changed to %p\n",self,win );
}

static void focus_in( GtkIMContext* self ) {
	g_print( "GbdIM %p: Focus In\n",self );
}

static void focus_out( GtkIMContext* self ) {
	g_print( "GbdIM %p: Focus Out\n",self );
}

static void class_init( GbdIMClass* klass,gpointer udata ) {
	GObjectClass* klass_go = G_OBJECT_CLASS( klass );
	GtkIMContextClass* klass_ic = GTK_IM_CONTEXT_CLASS( klass );

	klass_ic->set_client_window = set_client_window;
	klass_ic->focus_in = focus_in;
	klass_ic->focus_out = focus_out;
}

void im_module_init( GTypeModule* module ) {
	const GTypeInfo info = {
		sizeof( GbdIMClass ),
		NULL,NULL,
		(GClassInitFunc)class_init,NULL,NULL,
		sizeof( GbdIM ),0,
		NULL,
		NULL
	};

	g_type_module_register_type( module,GTK_TYPE_IM_CONTEXT,"GbdIM",&info,0 );
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
