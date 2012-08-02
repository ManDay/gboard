#include "keyboard.h"

#include <math.h>

enum {
	PROP_0,
	PROP_LAYOUT,
	PROP_XPADDING,
	PROP_YPADDING,
	PROP_FONTSIZE,
	PROP_RELATIVE,
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
	GbdEmitter* emitter;
	gdouble fontsize;
	gboolean relative;

	GQueue* modstack;
/**< Release Stack
 *
 * With every modifier in the modifier stack is associated a
 * boolean-type release state. For sticky modifiers, that state is TRUE
 * if and only if the sticky modifier was pressed while it was active,
 * indicating that on the following release, the modifier is expected to
 * be dismissed from the stack. For non-sticky modifiers, the release
 * state is TRUE if the modifier was pressed while it was active,
 * indicating that on the following release, it's expected to dismissed,
 * or if an ordinary key was pressed while it was pressed, due to
 * multi-pointer.
 * Generally, the release stack indicates whether a modifier should be
 * dimissed when it's released.
 */

	GPtrArray* pressed;
	GPtrArray* keycache;

	GPtrArray* cached_groups;
	guint cached_width,cached_height;
};

typedef struct {
	GbdKeyModifier mod;
	gboolean release;
	const GbdKey* key;
} ModElement;

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

static void remove_active_mod( GbdKeyboard*,const GbdKeyModifier );
static void release_all_modifiers( GbdKeyboard* );
static void release_all_keys( GbdKeyboard* );
static ModElement* is_active_mod( GbdKeyboard*,GbdKeyModifier,gboolean );
static const generate_cache( GbdKeyboard* );
static const GbdKey* key_at( GbdKeyboard*,const guint,const guint );
static GbdKeyGroup* get_pressed_key( GbdKeyboard*,guint );
static void set_pressed_key( GbdKeyboard*,guint,const GbdKeyGroup* );
static gboolean is_pressed_key( GbdKeyboard*,const GbdKeyGroup* );
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
	props[ PROP_XPADDING ]= g_param_spec_double( "xpadding","Padding X","Half of horizontal distance between keys",0,G_MAXDOUBLE,1,G_PARAM_READWRITE|G_PARAM_CONSTRUCT );
	props[ PROP_YPADDING ]= g_param_spec_double( "ypadding","Padding Y","Half of vertical distance between keys",0,G_MAXDOUBLE,1,G_PARAM_READWRITE|G_PARAM_CONSTRUCT );
	props[ PROP_FONTSIZE ]= g_param_spec_double( "fontsize","Fontsize","Fontsize on keys",0,G_MAXDOUBLE,0.5,G_PARAM_READWRITE|G_PARAM_CONSTRUCT );
	props[ PROP_RELATIVE ]= g_param_spec_boolean( "relative","Relative fontsize","Wether the size should be interpreted as a fraction of keysize",TRUE,G_PARAM_READWRITE|G_PARAM_CONSTRUCT );

	g_object_class_install_properties( klass_go,PROP_N,props );

	sigs[ SIG_MASKCHANGE ]= g_signal_new( "mask-change",GBD_TYPE_KEYBOARD,G_SIGNAL_RUN_FIRST,(void*)( &klass->mask_change )-(void*)klass,NULL,NULL,NULL,G_TYPE_NONE,0 );
}

static void instance_init( GbdKeyboard* self ) {
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self,GBD_TYPE_KEYBOARD,GbdKeyboardPrivate );

	self->priv->pressed = g_ptr_array_new( );
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
			release_all_keys( GBD_KEYBOARD( _self ) );
			release_all_modifiers( GBD_KEYBOARD( _self ) );

			g_ptr_array_unref( priv->pressed );
			g_object_unref( priv->layout );
			g_ptr_array_unref( priv->cached_groups );
			g_ptr_array_unref( priv->keycache );

			g_queue_free_full( priv->modstack,g_free );
		}
		priv->layout = g_value_dup_object( value );
		if( priv->layout ) {
			g_object_get( priv->layout,"groups",&priv->cached_groups,"width",&priv->cached_width,"height",&priv->cached_height,"emitter",&priv->emitter,NULL );
			priv->keycache = g_ptr_array_new( );
			g_ptr_array_set_size( priv->keycache,priv->cached_width*priv->cached_height );
			priv->pressed = g_ptr_array_new( );
			priv->modstack = g_queue_new( );
			g_queue_push_tail( priv->modstack,NULL );
			generate_cache( GBD_KEYBOARD( _self ) );
		} else
			priv->cached_groups = NULL;
		break;
	case PROP_XPADDING:
		priv->xpadding = g_value_get_double( value );
		break;
	case PROP_YPADDING:
		priv->ypadding = g_value_get_double( value );
		break;
	case PROP_FONTSIZE:
		priv->fontsize = g_value_get_double( value );
		break;
	case PROP_RELATIVE:
		priv->relative = g_value_get_boolean( value );
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
	case PROP_FONTSIZE:
		g_value_set_double( value,priv->fontsize );
		break;
	case PROP_RELATIVE:
		g_value_set_boolean( value,priv->relative );
		break;
	}
}

static gboolean configure_event( GtkWidget* _self,GdkEventConfigure* ev ) {
	g_signal_emit( _self,sigs[ SIG_MASKCHANGE ],0 );
}

static gboolean draw( GtkWidget* _self,cairo_t* cr ) {
	GbdKeyboard* const self = GBD_KEYBOARD( _self );
	GbdKeyboardPrivate* const priv = self->priv;

	if( !priv->cached_groups )
		return TRUE;

	const gint width = gtk_widget_get_allocated_width( _self  );
	const gint height = gtk_widget_get_allocated_height( _self );
	const gdouble cellwidth = width/(gdouble)priv->cached_width;
	const gdouble cellheight = height/(gdouble)priv->cached_height;
	guint i;

	cairo_set_font_size( cr,priv->relative?cellheight*priv->fontsize:priv->fontsize );
	cairo_select_font_face( cr,"sans-serif",CAIRO_FONT_SLANT_NORMAL,CAIRO_FONT_WEIGHT_BOLD );

	struct DrawInfo info = { GBD_KEYBOARD( _self ),gtk_widget_get_style_context( _self ),cr };
	foreach_key( GBD_KEYBOARD( _self ),(KeyOperation)draw_key,&info );

	for( i = 0; i<priv->pressed->len; i++ ) {
		const GbdKeyGroup* grp = get_pressed_key( self,i );
		if( grp ) {
			const GbdKey* key = key_at( self,grp->col,grp->row );
			if( !gbd_key_is_mod( key ) ) {
				const gdouble x = ceil( grp->col*cellwidth+priv->xpadding*cellwidth );
				const gdouble y = ceil( grp->row*cellheight+priv->ypadding*cellheight );
				const gdouble w = floor( cellwidth*( grp->colspan-2*priv->xpadding ) )-1;
				const gdouble h = floor( cellheight*( grp->rowspan-2*priv->ypadding ) )-1;

				gtk_render_focus( gtk_widget_get_style_context( _self ),cr,x,y,w,h );
			}
		}
	}		

	return TRUE;
}

static const generate_cache( GbdKeyboard* self ) {
	GbdKeyboardPrivate* const priv = self->priv;
	guint i;
	const GbdKey** keycache = (const GbdKey**)priv->keycache->pdata;
	for( i = 0; i<priv->cached_groups->len; i++ ) {
		const GbdKeyGroup* const grp = g_ptr_array_index( priv->cached_groups,i );
		guint row;
		for( row = 0; row<grp->rowspan; row++ ) {
			guint col;
			for( col = 0; col<grp->colspan; col++ )
				keycache[ ( grp->row+row )*priv->cached_width+grp->col+col ]= gbd_key_current( grp,priv->modstack );
		}
	}
}

static const GbdKey* key_at( GbdKeyboard* self,const guint x,const guint y ) {
	GbdKeyboardPrivate* const priv = self->priv;
	if( x<priv->cached_width && y<priv->cached_height )
		return g_ptr_array_index( priv->keycache,y*priv->cached_width+x );
	else
		return NULL;
}

static void release_all_modifiers( GbdKeyboard* self ) {
	GbdKeyboardPrivate* const priv = self->priv;

	guint i;
	const guint imax = g_queue_get_length( priv->modstack );
	for( i = 1; i<imax; i++ ) {
		ModElement* const popmod = g_queue_pop_tail( priv->modstack );
		gbd_emitter_release( priv->emitter,popmod->key->action.action.code );
		g_free( popmod );
	}
}

static void release_all_keys( GbdKeyboard* self ) {
	GbdKeyboardPrivate* const priv = self->priv;
	guint i;
	for( i = 0; i<priv->pressed->len; i++ ) {
		const GbdKeyGroup* grp;
		if( grp = g_ptr_array_index( priv->pressed,i ) ) {
			const GbdKey* key = key_at( self,grp->col,grp->row );
			if( !gbd_key_is_mod( key ) ) {
				if( !key->is_exec )
					gbd_emitter_release( priv->emitter,key->action.action.code );
				g_ptr_array_index( priv->pressed,i )= NULL;
			}
		}
	}
}

static gboolean button_press_event( GtkWidget* _self,GdkEventButton* ev ) {
	GbdKeyboard* const self = GBD_KEYBOARD( _self );
	GbdKeyboardPrivate* const priv = self->priv;
/* TODO : Multi-Pointer and/or -Device support.
 * Assign a number to each pointer/device, currently hardcoded to 0.
 * Each pointer is given its own "Keypress"-Slot, so with n pointers,
 * n keys may be pressed in parallel. */
	guint pointer = 0;

	if( ev->type==GDK_2BUTTON_PRESS )
		return TRUE;

	// TODO : Consistency check may be removed
	const GbdKeyGroup* grp;

	gint x = priv->cached_width*ev->x/gtk_widget_get_allocated_width( _self );
	gint y = priv->cached_height*ev->y/gtk_widget_get_allocated_height( _self );
	grp = gbd_layout_at( priv->layout,x,y );
	const GbdKey* const key = key_at( self,x,y );

/* In the following, "active" refers to a key being held down, either
 * actively by the user or through automatic hold (as with modifiers)
 * whereas "pressed" refers to the key actually being kept track of as
 * "pressed by a particular pointer". Therefore, all pressed keys are
 * active, but not all active keys are pressed (e.g. modifiers).
 * 
 * Policy:
 *
 * If no key is hit, drop out immediately. Otherwise, register the press
 * in the list of pointers and their associated key presses. Drop out
 * then, if it's already pressed. Otherwise, go on.
 *
 * If the key is an exec, nothing happens. All exec operations are
 * performed upon release.
 *
 * If the key is ordinary, schedule all non-sticky modifiers for
 * releasem, emit it and we're done. If the key is a modifier, different
 * things may happen depending on the current modstack and sticky
 * states:
 *
 * If the modifier is already active with the same sticky state as it
 * was pressed, emit it and schedule its release if it's sticky.
 *
 * Otherwise, if the modifier is already active with the sticky
 * state, emit it, and add it to the modifier stack.
 *
 * Otherwise, emit it and add it to the modifier stack.
 */
	if( grp ) {
		gboolean pressed = is_pressed_key( self,grp );
		set_pressed_key( self,pointer,grp );
		if( !pressed && !key->is_exec ) {
			if( gbd_key_is_mod( key ) ) {
				ModElement* mod;
				if( mod = is_active_mod( self,key->action.action.modifier,TRUE ) ) {
					mod->release = TRUE;
					if( !key->action.action.modifier.sticky )
						return;
				} else {
					release_all_keys( self );
/* TODO : See the according commit which introduced this comment on
 * August 2nd 2012 for details. The following part may be removed in the
 * future: Release all non stickies, if a sticky is pressed. Begin of
 * block. */
					if( key->action.action.modifier.sticky ) {
						guint i;
						for( i = 1; i<g_queue_get_length( priv->modstack ); i++ ) {
							ModElement* const el = g_queue_peek_nth( priv->modstack,i );
							if( !el->mod.sticky )
								remove_active_mod( self,el->mod );
						}
					}
/* End of block. */
					ModElement el = { key->action.action.modifier,FALSE,key };
					g_queue_push_tail( priv->modstack,g_memdup( &el,sizeof( ModElement ) ) );
				}
				generate_cache( self );
			}
			gbd_emitter_emit( priv->emitter,key->action.action.code );
		}
		gtk_widget_queue_draw( _self );
	}
}

static gboolean button_release_event( GtkWidget* _self,GdkEventButton* ev ) {
	GbdKeyboard* const self = GBD_KEYBOARD( _self );
	GbdKeyboardPrivate* const priv = self->priv;
	guint pointer = 0;
/* Key release: The release is only emitted, if no other pointer holds a
 * press on the specific key. If any other pointer does, the current
 * releases its press but nothing else happens. Either way, the
 * pointer's press is released.
 *
 * If the key is an exec key, the action is executed.
 *
 * If the key is an ordinary key, the release of the code is emitted. If
 * there are any active non-sticky modifiers, they are purged, provided
 * no other key, modifier or ordinary, is currently pressed.
 *
 * If the key is a sticky modifier, the release of the code is emitted.
 *
 * If the key is a modifier and it's scheduled for deactivation as by
 * the release-stack, it's dismissed and the release is emitted. If it's
 * sticky, the release-stack is evaluated for compatibility under
 * removal.
 */
	const GbdKeyGroup* const grp = get_pressed_key( self,pointer );
	set_pressed_key( self,pointer,NULL );
	if( grp && !is_pressed_key( self,grp ) ) {
		const GbdKey* const key = key_at( self,grp->col,grp->row );
		if( gbd_key_is_mod( key ) ) {
			ModElement* model = is_active_mod( self,key->action.action.modifier,TRUE );
			if( !model )
				g_warning( "Detected release of modifier which is not in modifier stack" );
			else {
				if( key->action.action.modifier.sticky )
					gbd_emitter_release( priv->emitter,key->action.action.code );
				if( model->release )
					remove_active_mod( self,key->action.action.modifier );
			}
			generate_cache( self );
		} else {
			if( key->is_exec ) {
				gchar** argv = g_strsplit( key->action.exec," ",0 );
				GError* err = NULL;
				if( !g_spawn_async( NULL,argv,NULL,G_SPAWN_SEARCH_PATH,NULL,NULL,NULL,&err ) ) {
					g_warning( "Could not spawn process '%s': %s\n",key->action.exec,err->message );
					g_error_free( err );
				}
				g_strfreev( argv );
			} else
				gbd_emitter_release( priv->emitter,key->action.action.code );
			if( !is_pressed_key( self,NULL ) ) {
				guint i = 1;
				guint imax = g_queue_get_length( priv->modstack );
				while( i<imax ) {
					const ModElement* const el = g_queue_peek_nth( priv->modstack,i );
					if( !el->mod.sticky ) {
						gbd_emitter_release( priv->emitter,el->key->action.action.code );
						g_free( g_queue_pop_nth( priv->modstack,i ) );
						imax--;
					} else
						i++;
				}
				generate_cache( self );
			}
		}
		gtk_widget_queue_draw( _self );
	}
}

static ModElement* is_active_mod( GbdKeyboard* self,const GbdKeyModifier mod,gboolean strict ) {
	GbdKeyboardPrivate* const priv = self->priv;
	guint i;
	const guint imax = g_queue_get_length( priv->modstack );
	for( i = 1; i<imax; i++ ) {
		ModElement* testmod = g_queue_peek_nth( priv->modstack,i );
		if( testmod->mod.id==mod.id &&( !strict || testmod->mod.sticky==mod.sticky ) )
			return testmod;
	}
	return NULL;
}

/** Removes a modifier from the stack
 *
 * Removes a modifier and all modifiers which at least partially
 * depended on it from the modifier stack. Partial dependence of a
 * modifier B on a modifier A is defined by modifier A providing at
 * least one key which provides modifier B. This operation is of course
 * recursive, so that if there are dependend modifiers being removed,
 * their dependences are relinquished aswell.
 *
 * @param Self
 * @param Modifier to remove from the stack. If no such modifier is
 * found, nothing happens
 */
static void remove_active_mod( GbdKeyboard* self,const GbdKeyModifier mod ) {
	GbdKeyboardPrivate* const priv = self->priv;

	guint i;
	const guint imax = g_queue_get_length( priv->modstack );
	for( i = 1; i<imax; i++ ) {
		const ModElement* const testmod = g_queue_peek_nth( priv->modstack,i );
		if( testmod->mod.id==mod.id && testmod->mod.sticky==mod.sticky )
			break;
	}

/* Popping the queue here for subsequent calls to gbd_key_current, but
 * not yet updating the cache, so that key_at may still obtain the
 * currently visible keys. */
	ModElement* const popmod = g_queue_pop_nth( priv->modstack,i );

	guint row;
	for( row = 0; row<priv->cached_height; row++ ) {
		guint col;
		for( col = 0; col<priv->cached_width; col++ ) {
			const GbdKey* key = key_at( self,col,row );
/* Is the key a modifier which depends on the currently release
* modifier? If yes, release that modifier, too. The necessary condition
* for a key to immediately depend on a modifier is to match the
* modifier by filter. The sufficient condition, however, is that it
* actually becomes invisible when the modifier is released. And that is
* only the case if it's not sustained by a sticky/non-sticky
* counterpart of the modifier, which may remain in the stack. In order
* to not to dive into a lengthy analysis under which exact
* circumstances (s.a. the complicated mechanism of gbd_key_current) and
* in particular leave the details to gbd_key_current to decide, we
* simply check the necessary condition and if it holds, we compare the
* key as it's displayed after release (at this point the modstack is
* already popped, s.a.) with the one obtained from before the release.
*/
			if( key && key->filter.id==mod.id && gbd_key_is_mod( key ) ) {
				const guint jmax = g_queue_get_length( priv->modstack );
				guint j = i;
				while( j<jmax ) {
					const ModElement* const testmod = g_queue_peek_nth( priv->modstack,i );
					if( testmod->mod.id==key->action.action.modifier.id && testmod->mod.sticky==key->action.action.modifier.sticky )
						break;
					j++;
				}

				if( j<jmax ) {
					const GbdKey* const newkey = gbd_key_current( gbd_layout_at( priv->layout,col,row ),priv->modstack );
					if( newkey!=key )
						remove_active_mod( self,key->action.action.modifier );
				}
			}
		}
	}

	gbd_emitter_release( priv->emitter,popmod->key->action.action.code );
	g_free( popmod );

/* Consistify cache in topmost stackframe, because otherwise we'd
 * generate the cache in the middle of the recursion. */
}

static GbdKeyGroup* get_pressed_key( GbdKeyboard* self,guint pointer ) {
	if( self->priv->pressed->len>pointer )
		return g_ptr_array_index( self->priv->pressed,pointer );
	else
		return NULL;
}

static gboolean is_pressed_key( GbdKeyboard* self,const GbdKeyGroup* grp ) {
	GbdKeyboardPrivate* const priv = self->priv;
	guint i;
	for( i = 0; i<priv->pressed->len; i++ )
		if( grp!=NULL && grp==g_ptr_array_index( priv->pressed,i )|| grp==NULL && g_ptr_array_index( priv->pressed,i )!=NULL )
			return TRUE;
	return FALSE;
}

static void set_pressed_key( GbdKeyboard* self,guint pointer,const GbdKeyGroup* grp ) {
	GbdKeyboardPrivate* const priv = self->priv;
	if( priv->pressed->len<=pointer )
		g_ptr_array_set_size( priv->pressed,pointer+1 );

/* Not very beautiful, but priv->pressed is practically const to us and
 * just couldn't be declared as such - the cast serves to silence the
 * compiler. */
	( (const GbdKeyGroup**)priv->pressed->pdata )[ pointer ]= grp;
}

static void draw_key( gdouble x,gdouble y,gdouble w,gdouble h,GbdKeyGroup* keygrp,struct DrawInfo* info ) {
	GbdKeyboardPrivate* const priv = info->self->priv;
	gtk_style_context_add_class( info->style,GTK_STYLE_CLASS_BUTTON );

	x = ceil( x );
	y = ceil( y );
	w = floor( w )-1;
	h = floor( h )-1;

	gtk_render_background( info->style,info->cr,x,y,w,h );
	gtk_render_frame( info->style,info->cr,x,y,w,h );

	const GbdKey* key = key_at( info->self,keygrp->col,keygrp->row );

	if( gbd_key_is_mod( key )&& is_active_mod( info->self,key->action.action.modifier,TRUE ) )
		gtk_render_focus( info->style,info->cr,x,y,w,h );

	if( key->is_image ) {
		if( key->label.image ) {
			gint imagew = gdk_pixbuf_get_width( key->label.image );
			gint imageh = gdk_pixbuf_get_height( key->label.image );

			if( imageh>h*0.8 ) {
				imagew *= 0.8*h/(gdouble)imageh;
				imageh = h*0.8;
			}

			if( imagew>w*0.8 ) {
				imageh *= 0.8*w/(gdouble)imagew;
				imagew = w*0.8;
			}

			GdkPixbuf* scaled = gdk_pixbuf_scale_simple( key->label.image,imagew,imageh,GDK_INTERP_BILINEAR );
			gtk_render_icon( info->style,info->cr,scaled,x+( w-imagew )/2.0,y+( h-imageh )/2.0 );
			g_object_unref( scaled );
		}
	} else {
		cairo_text_extents_t extents;
		cairo_text_extents( info->cr,key->label.text,&extents );

		const gdouble right = x+w/2-extents.width/2-extents.x_bearing;
		const gdouble down = y+h/2-extents.height/2-extents.y_bearing;

		cairo_move_to( info->cr,right,down );
		cairo_show_text( info->cr,key->label.text );
	}
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
