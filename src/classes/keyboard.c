#include "keyboard.h"

#include <math.h>

#define DEFAULT_PADDING 0.1

enum {
	PROP_0,
	PROP_LAYOUT,
	PROP_XPADDING,
	PROP_YPADDING,
	PROP_N
};

enum {
	SIG_0,
	SIG_MASKCHANGE,
	SIG_N
};

struct GbdKeyboardPrivate {
	GbdLayout* layout;
	gdouble xpadding,ypadding;
	GPtrArray* cached_groups;
	guint mod;
	guint cached_width,cached_height;
};

struct DrawInfo {
	GbdKeyboard* self;
	GtkStyleContext* style;
	cairo_t* cr;
};

typedef void(* KeyOperation)( gdouble,gdouble,gdouble,gdouble,GbdKeyGroup*,gpointer );

static GParamSpec* props[ PROP_N ];
static guint sigs[ SIG_N ];

static void class_init( GbdKeyboardClass*,gpointer );
static void instance_init( GbdKeyboard* );
static void instance_finalize( GbdKeyboard* );
static void set_property( GObject*,guint,const GValue*,GParamSpec* );
static void get_property( GObject*,guint,GValue*,GParamSpec* );

static gboolean configure_event( GtkWidget*,GdkEventConfigure* );
static gboolean draw( GtkWidget*,cairo_t* );
static gboolean button_press_event( GtkWidget*,GdkEventButton* );
static gboolean button_release_event( GtkWidget*,GdkEventButton* );

static void mask_change( GbdKeyboard*,gpointer );
static void accumulate_regions( gdouble,gdouble,gdouble,gdouble,GbdKeyGroup*,cairo_region_t* );
static void draw_key( gdouble,gdouble,gdouble,gdouble,GbdKeyGroup*,struct DrawInfo* );
static gboolean foreach_key( GbdKeyboard*,KeyOperation,gpointer );

static void class_init( GbdKeyboardClass* klass,gpointer udata ) {
	GObjectClass* klass_go = G_OBJECT_CLASS( klass );
	GtkWidgetClass* klass_gw = GTK_WIDGET_CLASS( klass );

	g_type_class_add_private( klass,sizeof( GbdKeyboardPrivate ) );

	klass_go->get_property = get_property;
	klass_go->set_property = set_property;
	klass_go->finalize = (GObjectFinalizeFunc)instance_finalize;

	klass_gw->draw = draw;
	klass_gw->configure_event = configure_event;
	klass_gw->button_release_event = button_release_event;
	klass_gw->button_press_event = button_press_event;

	klass->mask_change = mask_change;

	props[ PROP_LAYOUT ]= g_param_spec_object( "layout","Layout","Layout of keyboard",GBD_TYPE_LAYOUT,G_PARAM_READWRITE );
	props[ PROP_XPADDING ]= g_param_spec_double( "xpadding","Padding X","Half of horizontal distance between keys",0,G_MAXDOUBLE,DEFAULT_PADDING,G_PARAM_READWRITE|G_PARAM_CONSTRUCT );
	props[ PROP_YPADDING ]= g_param_spec_double( "ypadding","Padding Y","Half of vertical distance between keys",0,G_MAXDOUBLE,DEFAULT_PADDING,G_PARAM_READWRITE|G_PARAM_CONSTRUCT );

	g_object_class_install_properties( klass_go,PROP_N,props );

	sigs[ SIG_MASKCHANGE ]= g_signal_new( "mask-change",GBD_TYPE_KEYBOARD,G_SIGNAL_RUN_FIRST,(void*)( &klass->mask_change )-(void*)klass,NULL,NULL,NULL,G_TYPE_NONE,0 );
}

static void instance_init( GbdKeyboard* self ) {
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self,GBD_TYPE_KEYBOARD,GbdKeyboardPrivate );
}

static void instance_finalize( GbdKeyboard* self ) {
	GbdKeyboardPrivate* const priv = self->priv;

	G_OBJECT_CLASS( g_type_class_peek( G_TYPE_OBJECT ) )->finalize( G_OBJECT( self ) );
}

static void set_property( GObject* _self,guint prop,const GValue* value,GParamSpec* pspec ) {
	GbdKeyboardPrivate* const priv = GBD_KEYBOARD( _self )->priv;
	switch( prop ) {
	case PROP_LAYOUT:
		if( priv->layout ) {
			g_object_unref( priv->layout );
			g_ptr_array_unref( priv->cached_groups );
		}
		priv->layout = g_value_dup_object( value );
		if( priv->layout )
			g_object_get( priv->layout,"groups",&priv->cached_groups,"width",&priv->cached_width,"height",&priv->cached_height,NULL );
		else
			priv->cached_groups = NULL;
		break;
	case PROP_XPADDING:
		priv->xpadding = g_value_get_double( value );
		break;
	case PROP_YPADDING:
		priv->ypadding = g_value_get_double( value );
		break;
	}
	
	g_signal_emit( _self,sigs[ SIG_MASKCHANGE ],0 );
}

static void get_property( GObject* _self,guint prop,GValue* value,GParamSpec* pspec ) {
	GbdKeyboardPrivate* const priv = GBD_KEYBOARD( _self )->priv;
	switch( prop ) {
	case PROP_LAYOUT:
		g_value_set_object( value,priv->layout );
		break;
	case PROP_XPADDING:
		g_value_set_double( value,priv->xpadding );
		break;
	case PROP_YPADDING:
		g_value_set_double( value,priv->ypadding );
		break;
	}
}

static gboolean configure_event( GtkWidget* _self,GdkEventConfigure* ev ) {
	g_signal_emit( _self,sigs[ SIG_MASKCHANGE ],0 );
}

static gboolean draw( GtkWidget* _self,cairo_t* cr ) {
	GbdKeyboardPrivate* const priv = GBD_KEYBOARD( _self )->priv;

	if( !priv->cached_groups )
		return TRUE;

	struct DrawInfo info = { GBD_KEYBOARD( _self ),gtk_widget_get_style_context( _self ),cr };

	foreach_key( GBD_KEYBOARD( _self ),(KeyOperation)draw_key,&info );
	cairo_fill( cr );

	return TRUE;
}

static gboolean button_press_event( GtkWidget* _self,GdkEventButton* ev ) {
	GbdKeyboardPrivate* const priv = GBD_KEYBOARD( _self )->priv;
	gint x = priv->cached_width*ev->x/gtk_widget_get_allocated_width( _self );
	gint y = priv->cached_height*ev->y/gtk_widget_get_allocated_height( _self );
	GbdKey* key = gbd_layout_at( priv->layout,x,y,priv->mod );
	if( key ) {
		g_print( "Pressed key '%s'\n",key->label );
		guint mod;
		if( !key->is_exec &&( mod = key->action.action.modifier.id ) ) {
			g_print( "Has modifier %i\n",mod );
		} else
			mod = 0;
		priv->mod = mod;
		gtk_widget_queue_draw( _self );
	}
}
static gboolean button_release_event( GtkWidget* _self,GdkEventButton* ev ) {
}

static void draw_key( gdouble x,gdouble y,gdouble w,gdouble h,GbdKeyGroup* keygrp,struct DrawInfo* info ) {
	GbdKeyboardPrivate* const priv = info->self->priv;
	gtk_style_context_add_class( info->style,GTK_STYLE_CLASS_DEFAULT );

	x = ceil( x );
	y = ceil( y );
	w = floor( w )-1;
	h = floor( h )-1;

	gtk_render_background( info->style,info->cr,x,y,w,h );
	gtk_render_frame( info->style,info->cr,x,y,w,h );
	gtk_render_focus( info->style,info->cr,x,y,w,h );

	guint i;
	GbdKey key = keygrp->keys[ 0 ];
	for( i = 0; i<keygrp->keycount; i++ ) {
		if( keygrp->keys[ i ].modifier.id==priv->mod ) {
			key = keygrp->keys[ i ];
			break;
		}
	}

	cairo_text_extents_t extents;
	cairo_text_extents( info->cr,key.label,&extents );

	const gdouble right = x+w/2-extents.width/2-extents.x_bearing;
	const gdouble down = y+h/2-extents.height/2-extents.y_bearing;

	cairo_move_to( info->cr,right,down );
	cairo_show_text( info->cr,key.label );
}

static void mask_change( GbdKeyboard* self,gpointer udata ) {
	gtk_widget_queue_draw( GTK_WIDGET( self ) );
}

static gboolean foreach_key( GbdKeyboard* self,KeyOperation op,gpointer udata ) {
	GbdKeyboardPrivate* const priv = self->priv;

	if( !priv->cached_groups || !gtk_widget_get_realized( GTK_WIDGET( self ) ) )
		return FALSE;

	const gint width = gtk_widget_get_allocated_width( GTK_WIDGET( self ) );
	const gint height = gtk_widget_get_allocated_height( GTK_WIDGET( self ) );
	const gdouble cellwidth = width/(gdouble)priv->cached_width;
	const gdouble cellheight = height/(gdouble)priv->cached_height;

	guint i;
	for( i = 0; i<priv->cached_groups->len; i++ ) {
		GbdKeyGroup* grp = g_ptr_array_index( priv->cached_groups,i );
	
		const gdouble x = grp->col*cellwidth+priv->xpadding*cellwidth;
		const gdouble y = grp->row*cellheight+priv->ypadding*cellheight;
		const gdouble w = cellwidth*( grp->colspan-2*priv->xpadding );
		const gdouble h = cellheight*( grp->rowspan-2*priv->ypadding );

		op( x,y,w,h,grp,udata );
	}

	return TRUE;
}

cairo_region_t* gbd_keyboard_regions( GbdKeyboard* self ) {
	cairo_region_t* regions = cairo_region_create( );
	if( foreach_key( self,(KeyOperation)accumulate_regions,regions ) )
		return regions;
	else {
		cairo_region_destroy( regions );
		return NULL;
	}
}

static void accumulate_regions( gdouble x,gdouble y,gdouble w,gdouble h,GbdKeyGroup* grp,cairo_region_t* regions ) {
	const cairo_rectangle_int_t rect = { ceil( x ),ceil( y ),floor( w )-1,floor( h )-1 };
	cairo_region_union_rectangle( regions,&rect );
}

GType gbd_keyboard_get_type( ) {
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
		type = g_type_register_static( GTK_TYPE_DRAWING_AREA,"GbdKeyboard",&info,0 );
	}
	return type;
}

GbdKeyboard* gbd_keyboard_new( GbdLayout* layout ) {
	return g_object_new( GBD_TYPE_KEYBOARD,NULL );
}
