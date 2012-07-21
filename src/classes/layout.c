#include "layout.h"

#include <string.h>

#define REGEX_F "(?'F'\\w+)"
#define REGEX_E "\\((?'E'[^)]+)\\)"
#define REGEX_C "(?'C'\\w+)"
#define REGEX_CM "\\{(?:" REGEX_C " )?(?'M'\\w+)\\}"
#define REGEX_L "(?'L'.*?(?=\\}))"
#define REGEX_I "\\((?'I'[^}]+)\\)"

#define REGEX_ALL "^" REGEX_F "?s*\\{\\s*(?:" REGEX_E "|" REGEX_C "|" REGEX_CM ")\\s*(?:" REGEX_I "|" REGEX_L ")?\\}$"

enum {
	PROP_0,
	PROP_EMITTER,
	PROP_N
};

struct GbdLayoutPrivate {
	GbdEmitter* emitter;
	guint width,height;
	GPtrArray* groups;
	guint* map;
};

typedef struct {
	guint id;
	gboolean sticky;
} KeyModifier;

typedef union {
	struct {
		guint code;
		KeyModifier modifier;
	} action;
	gchar* exec;
} KeyAction;

typedef struct {
	KeyModifier modifier;
	gboolean is_image;
	gchar* label;
	gboolean is_exec;
	KeyAction action;
}	Key;

typedef struct {
	Key* keys;
	guint keycount,col,row,colspan,rowspan;
} KeyGroup;

static GParamSpec* props[ PROP_N ];

static void class_init( GbdLayoutClass*,gpointer );
static void instance_init( GbdLayout* );
static void instance_finalize( GbdLayout* );

static void set_property( GObject*,guint,const GValue*,GParamSpec* );
static void get_property( GObject*,guint,GValue*,GParamSpec* );

static void delete_keygroup( KeyGroup* );

static void class_init( GbdLayoutClass* klass,gpointer udata ) {
	GObjectClass* klass_go = G_OBJECT_CLASS( klass );

	g_type_class_add_private( klass,sizeof( GbdLayoutPrivate ) );

	klass_go->finalize = (GObjectFinalizeFunc)instance_finalize;
	klass_go->get_property = get_property;
	klass_go->set_property = set_property;

	props[ PROP_EMITTER ]= g_param_spec_object( "emitter","Emitter","Emitter Object",G_TYPE_OBJECT,G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY );

	g_object_class_install_properties( klass_go,PROP_N,props );
}

static void instance_init( GbdLayout* self ) {
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self,GBD_TYPE_LAYOUT,GbdLayoutPrivate );
}

static void instance_finalize( GbdLayout* self ) {
	G_OBJECT_CLASS( g_type_class_peek( G_TYPE_OBJECT ) )->finalize( G_OBJECT( self ) );
}

static void set_property( GObject* _self,guint param,const GValue* val,GParamSpec* pspec ) {
	GbdLayoutPrivate* const priv = GBD_LAYOUT( _self )->priv;

	priv->emitter = GBD_EMITTER( g_value_get_object( val ) );
}

static void get_property( GObject* _self,guint param,GValue* val,GParamSpec* pspec ) {
	GbdLayoutPrivate* const priv = GBD_LAYOUT( _self )->priv;

	g_value_set_object( val,priv->emitter );
}

static void delete_keygroup( KeyGroup* grp ) {
	guint i;
	for( i = 0; i<grp->keycount; i++ ) {
		Key key = grp->keys[ i ];
		if( key.label )
			g_free( key.label );
		if( key.is_exec && key.action.exec )
			g_free( key.action.exec );
	}
	g_free( grp->keys );
	g_free( grp );
}

static guint fit_keygroup( KeyGroup* grp,GArray* vstack,guint x,guint y ) {
	gboolean obtruded;
	do {
		guint i;
		obtruded = FALSE;
		for( i = x; i<x+grp->colspan; i++ )
			if( vstack->len>i && g_array_index( vstack,guint,i )>=y ) {
				obtruded = TRUE;
				x = i+1;
				break;
			}
	} while( obtruded );

	return x;
}

static KeyModifier spawn_modifier( GPtrArray* modlist,gchar* str ) {
	guint i;
	KeyModifier result = { .sticky = g_ascii_isupper( str[ 0 ] ) };
	if( str[ 0 ]=='\0' ) {
		result.id = 0;
		return result;
	}
	for( i = 0; i<modlist->len; i++ )
		if( !g_ascii_strcasecmp( str,g_ptr_array_index( modlist,i ) ) ) {
			result.id = i + 1;
			return result;
		}
	gchar* name = g_strdup( str );
	result.id = i + 1;
	g_ptr_array_add( modlist,name );
	return result;
}

static Key* parse_key( gchar* str,GPtrArray* modlist,GbdEmitter* emitter,GError** err ) {
	static GRegex* matcher = NULL;

	if( !matcher )
		matcher = g_regex_new( REGEX_ALL,G_REGEX_DUPNAMES|G_REGEX_OPTIMIZE,0,NULL );
	GMatchInfo* minfo = NULL;
	g_regex_match( matcher,str,0,&minfo );

	gchar* filter = g_match_info_fetch_named( minfo,"F" );
	gchar* exec = g_match_info_fetch_named( minfo,"E" );
	gchar* code = g_match_info_fetch_named( minfo,"C" );
	gchar* mod = g_match_info_fetch_named( minfo,"M" );
	gchar* label = g_match_info_fetch_named( minfo,"L" );
	gchar* image = g_match_info_fetch_named( minfo,"I" );

	if( !filter ) {
		g_set_error( err,GBD_LAYOUT_ERROR,GBD_LAYOUT_ERROR_KEY,"Could not parse key definition '%s'",str );
		return NULL;
	}

	Key* result = g_malloc( sizeof( Key ) );
	result->is_image = FALSE;
	result->label = NULL;
	result->action.action.code = 0;
	result->action.action.modifier.id = 0;

	result->modifier = spawn_modifier( modlist,filter );
	g_free( filter );

	result->is_exec = exec[ 0 ]!='\0';
	if( result->is_exec )
		result->action.exec = exec;
	else {
		result->action.action.code = gbd_emitter_get_code( emitter,code );
		result->action.action.modifier = spawn_modifier( modlist,mod );
		g_free( exec );
	}
	g_free( code );
	g_free( mod );

	result->is_image = image[ 0 ]!='\0';
	if( result->is_image ) {
		result->label = image;
		g_free( label );
	} else {
		result->label = label;
		g_free( image );
	}

	return result;
}

static KeyGroup* parse_keygroup( gchar* str,GPtrArray* modlist,GbdEmitter* emitter,GError** err ) {
	gint depth = 0;
	gchar* current_char = str,
		* current_key = str;
	gboolean esc = FALSE;
	
	KeyGroup* grp = g_malloc( sizeof( KeyGroup ) );
	grp->keys = NULL;
	grp->keycount = 0;
	grp->rowspan = 1;

	while( current_char[ 0 ] ) {
		gchar c = current_char[ 0 ];

		if( depth==1 && c!=' ' && !current_key || !esc && c=='{' && current_key==str )
			current_key = current_char;

		if( !esc ) {
			if( depth==1 )
				if( c=='_' )
					grp->rowspan++;
				else if( c!='}' )
					grp->rowspan = 1;

			if( c=='}' ) {
				if( depth<3 &&( depth>1 || current_key==str )&& current_key ) {
					if( current_char-current_key>1 ) {
						gchar* keystr = g_strndup( current_key,current_char-current_key+1 );
						Key* key = parse_key( keystr,modlist,emitter,err );
						if( err ) {
							delete_keygroup( grp );
							return NULL;
						}
						if( key ) {
							grp->keys = g_realloc_n( grp->keys,++grp->keycount,sizeof( Key ) );
							memcpy( grp->keys+grp->keycount-1,key,sizeof( Key ) );
							g_free( key );
						}
						g_free( keystr );
					}
					current_key = NULL;
				}
				depth--;
			} else if( c=='{' ) {
				if( depth==1 )
					grp->rowspan = 1;
				depth++;
			} else if( c=='\\' )
				esc = TRUE;
		} else
			esc = FALSE;

		current_char = g_utf8_next_char( current_char );
	}

	if( depth<0 )
		grp->colspan = 1-depth;
	else
		grp->colspan = 1;

	if( grp->keys>0 ) {
		grp->colspan++;
		grp->rowspan++;
	}

	return grp;
}

gboolean gbd_layout_parse( GbdLayout* self,gchar* str,GError** err ) {
	GbdLayoutPrivate* const priv = self->priv;
/* Operation proceeds as follows: Iterating line-by-line,
 * utf8-character-by-character, trying to make sense of the keygroup.
 * Every successfully parse keygroup is added, with information about
 * its row- and col-span and position, to a list, while the information
 * is also used to find the size of the key matrix.
 * Every modifier which is mentioned - either as an alternative for a
 * key or as the modifier itsself, is added to the list of modifiers. If
 * the modifier was mentioned as an alternative for a key, that key is
 * added to a list of keys for the respective modifier.
 * After all keygroups were read, the list is again iterated over,
 * caching the position of keys into a rectangular memory region for
 * fast position-key-lookup.
 */
	if( !g_utf8_validate( str,-1,NULL ) ) {
		g_set_error( err,GBD_LAYOUT_ERROR,GBD_LAYOUT_ERROR_CODEPAGE,"Invalid Layout Definition File: Contains non-UTF-8 characters" );
		return FALSE;
	}

	GPtrArray* modifier_list = g_ptr_array_new_with_free_func( g_free );
	GPtrArray* keygroup_list = g_ptr_array_new_with_free_func( (GDestroyNotify)delete_keygroup );
	GArray* vstack = g_array_new( FALSE,FALSE,sizeof( guint ) );

	gchar* current_char = str,
		* current_block = str;

	gboolean esc = FALSE,
		finished_group = FALSE;

	guint depth = 0,
		width = 0,x = 0,
		height = 0,y = 0;

	while( current_char[ 0 ] ) {
		gchar c = current_char[ 0 ];

		if( !esc ) {
			if( c=='}' ) {
				if( depth>1 ) {
					depth--;
				} else {
					if( depth==1 )
						finished_group = TRUE;
					depth = 0;
				}
			} else {
				if( depth==0 && finished_group ) {
					gchar* groupstr = g_strndup( current_block,current_char-current_block );
					KeyGroup* grp = parse_keygroup( groupstr,modifier_list,priv->emitter,err );
					if( grp ) {
						guint i;
/* Keygroup successfully parsed. Position it. If it fits in, position it
 * at the current (x,y) position, increasing x by the colspan of the group
 * and updating the vstack in the range [x,x+colspan-1] to become
 * y+rowspan-1. If it doesn't fit in, considering the vstack, find the
 * next x for which vspan in the range of [x,x+colspan-1] is less than
 * y, and position it there - updating the vstack equally.
 */
						grp->col = fit_keygroup( grp,vstack,x,y );
						grp->row = y;

						if( vstack->len<x+grp->colspan )
							g_array_set_size( vstack,x+grp->colspan );
						for( i = x; i<x+grp->colspan; i++ )
							g_array_index( vstack,guint,i )= y+grp->rowspan-1;

//						g_print( "GRP '%s' {\n\tKeys:\t%i\n\tRowspan:\t%i\n\tColspan:\t%i\n}\n",groupstr,grp->keycount,grp->rowspan,grp->colspan );

/* Not strictly necessary. Fitting the keygroup will increment X
 * accordingly, anyway, just saves a few iterations. */
						x += grp->colspan;

						if( grp->col+grp->colspan>width )
							width = grp->col+grp->colspan;
						if( grp->row+grp->rowspan>height )
							height = grp->row+grp->rowspan;
						if( grp->keycount>0 )
							g_ptr_array_add( keygroup_list,grp );
						else
							delete_keygroup( grp );
					}
					g_free( groupstr );
				}
				if( c=='\\' )
					esc = TRUE;
				if( c=='{' ) {
					if( depth==0 )
						current_block = current_char;
					depth++;
				} else if( c=='\n' ) {
					depth = 0;
					y++;
					x = 0;
				}
				finished_group = FALSE;
			}
		} else
			esc = FALSE;

		current_char = g_utf8_next_char( current_char );
	}

	g_ptr_array_unref( modifier_list );
	g_array_unref( vstack );
	
	guint* keymap = g_malloc0_n( height*width,sizeof( guint ) );
	guint i;
	for( i = 0; i<keygroup_list->len; i++ ) {
		KeyGroup* grp = g_ptr_array_index( keygroup_list,i );
		guint y;
		for( y = grp->row; y<grp->row+grp->rowspan; y++ ) {
			guint x;
			for( x = grp->col; x<grp->col+grp->colspan; x++ )
				keymap[ x+( width )*y ]= i+1;
		}
	}

	if( priv->groups ) {
		g_ptr_array_unref( priv->groups );
		g_free( priv->map );
	}
	priv->groups = keygroup_list;
	priv->map = keymap;
	priv->width = width;
	priv->height = height;

	return TRUE;
}

GType gbd_layout_get_type( ) {
	static GType type = 0;
	if( !type ) {
		const GTypeInfo info = {
			sizeof( GbdLayoutClass ),
			NULL,NULL,
			(GClassInitFunc)class_init,NULL,NULL,
			sizeof( GbdLayout ),0,
			(GInstanceInitFunc)instance_init,
			NULL
		};
		type = g_type_register_static( G_TYPE_OBJECT,"GbdLayout",&info,0 );
	}
	return type;
}

GbdLayout* gbd_layout_new( gchar* str,GbdEmitter* emitter ) {
	GbdLayout* self = g_object_new( GBD_TYPE_LAYOUT,"emitter",emitter,NULL );
	if( str )
		gbd_layout_parse( self,str,NULL );

	return self;
}

GQuark gbd_layout_error_quark( ) {
	return g_quark_from_static_string( "gbd-layout-error-quark" );
}
