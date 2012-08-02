#include "x11emitter.h"

#include <gio/gio.h>
#include <X11/Xlib.h>
#include <string.h>
#include <gbd_config.h>
#include <stdio.h>
#include <gdk/gdkx.h>

#include "../interfaces/emitter.h"

#define KEYDEF_PREFIX "#define XK_"

struct GbdX11emitterPrivate {
	GFileInputStream* srcstream;
	GDataInputStream* srcdata;
	GData* mapcache;
	Display* dpy;
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

	g_datalist_init( &self->priv->mapcache );
	self->priv->dpy = GDK_SCREEN_XDISPLAY( gdk_screen_get_default( ) );
}

static void interface_init_emitter( GbdEmitterInterface* iface,gpointer* udata ) {
	iface->emit = emit;
	iface->release = release;
	iface->get_code = get_code;
}

static void instance_finalize( GbdX11emitter* self ) {
	GbdX11emitterPrivate* const priv = self->priv;
	g_clear_object( &priv->srcdata );
	g_clear_object( &priv->srcstream );

	g_datalist_clear( &priv->mapcache );

	G_OBJECT_CLASS( g_type_class_peek( G_TYPE_OBJECT ) )->finalize( G_OBJECT( self ) );
}

static gboolean load_file( GbdX11emitter* self,GFile* src,GError** err ) {
	GbdX11emitterPrivate* const priv = self->priv;

	g_clear_object( &priv->srcstream );
	g_clear_object( &priv->srcdata );

	GFileInputStream* is = g_file_read( src,NULL,err );
	if( !is )
		return FALSE;

	priv->srcstream = is;
	priv->srcdata = g_data_input_stream_new( G_INPUT_STREAM( priv->srcstream ) );
	return TRUE;
}

static guint64 get_code( GbdEmitter* _self,gchar* key ) {
	GbdX11emitter* const self = GBD_X11EMITTER( _self );
	GbdX11emitterPrivate* const priv = self->priv;

	if( !priv->srcdata ) {
		GFile* file = g_file_new_for_path( GBD_X11EMITTER_SRC );
		if( !load_file( self,file,NULL ) ) {
			g_critical( "GBoard X11-Emitter could not parse X11 keysymdef.h at " GBD_X11EMITTER_SRC );
			return 0;
		}
		g_object_unref( file );
	}

	guint64* code,symval = 0;
	if( ( code = g_datalist_get_data( &priv->mapcache,key ) ) )
		return *code;
	else
		code = g_malloc( sizeof( guint64 ) );

	g_seekable_seek( G_SEEKABLE( priv->srcstream ),0,G_SEEK_SET,NULL,NULL );
	GDataInputStream* dis = priv->srcdata;

	gchar* line;
	gsize len = strlen( key );
	gchar* keytoken = g_malloc( sizeof( gchar )*len+sizeof( KEYDEF_PREFIX )+1 );
	sprintf( keytoken,KEYDEF_PREFIX "%s",key );

	len = strlen( keytoken );

	GError* err = NULL;
	while( line = g_data_input_stream_read_line( dis,NULL,NULL,&err ) ) {
		if( !strncmp( keytoken,line,len )&& g_ascii_isspace( line[ len ] ) ) {
			if( sscanf( line+len," %" G_GINT64_MODIFIER "x ",&symval ) ) {
				g_free( line );
				break;
			}
		}
		g_free( line );
	}
	if( err ) {
		g_error( "%s",err->message );
		g_error_unref( &err );
	}

	if( symval ) {
		guint64 codeval = XKeysymToKeycode( priv->dpy,symval );
		*code = codeval;
		g_datalist_set_data_full( &priv->mapcache,key,code,g_free );
		if( !codeval )
			g_warning( "Could not find code for key '%s', Keysym-Value " G_GINT64_FORMAT,key,symval );
		return codeval;
	}

	return 0;
}

static void emit( GbdEmitter* self,guint64 _code ) {
	unsigned int code = _code;
	if( code )
		XTestFakeKeyEvent( GBD_X11EMITTER( self )->priv->dpy,code,1,1 );
}

static void release( GbdEmitter* self,guint64 _code ) {
	unsigned int code = _code;
	if( code )
		XTestFakeKeyEvent( GBD_X11EMITTER( self )->priv->dpy,code,0,1 );
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
