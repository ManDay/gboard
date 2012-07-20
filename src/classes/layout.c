#include "layout.h"

#include <string.h>

enum {
	PROP_0,
	PROP_EMITTER,
	PROP_N
};

struct GbdLayoutPrivate {
	GbdEmitter* emitter;
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
				break;
			}
		x++;
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
/*
 * key ::= modname "{" keydata "}" | "{" keydata "}"
 * keydata ::= action | action " " label
 * action ::= codename | "{" codename " " modname "}" |
 * 	"{" modname "}" | "(" exec ")"
 * label ::= labeltext | "(" imageuri ")"
 */
	g_print( "KEY '%s' {\n",str );

	Key* result = g_malloc( sizeof( Key ) );
	result->modifier.id = 0;
	result->modifier.sticky = FALSE;
	result->is_image = FALSE;
	result->label = NULL;
	result->is_exec = FALSE;
	result->action.action.code = 0;
	result->action.action.modifier.id = 0;

	gchar* current_char = str;
	guint depth = 0;
	gboolean esc = FALSE;
	gboolean actiondone = FALSE;

	while( current_char[ 0 ] ) {
		if( !esc ) {
			gchar c = current_char[ 0 ];
			if( !actiondone &&( c==' ' || c=='}' && depth==1 && current_char-str>0 ) ) {
				gchar* codestr = g_strndup( str,current_char-str );
				result->action.action.code = gbd_emitter_get_code( emitter,codestr );
				g_free( codestr );
				if( depth==1 )
					actiondone = TRUE;
			}
			switch( c ) {
			case '{':
				if( depth==0 ) {
					gchar* modstr = g_strndup( str,current_char-str );
					result->modifier = spawn_modifier( modlist,modstr );
					g_free( modstr );
				}
				str = current_char+1;
				depth++;
				break;
			case '}':
				if( depth==2 ) {
					gchar* modstr = g_strndup( str,current_char-str );
					result->action.action.modifier = spawn_modifier( modlist,modstr );
					result->is_exec = FALSE;
					g_free( modstr );
					actiondone = TRUE;
				} else if( depth==1 ) {
					if( !result->label )
						result->label = g_strndup( str,current_char-str );
				}
				depth--;
				break;
			case ')':
				if( actiondone && !result->label ) {
					result->label = g_strndup( str,current_char-str );
					result->is_image = TRUE;
				} else if( !actiondone && !result->action.exec ) {
					result->action.exec = g_strndup( str,current_char-str );
					result->is_exec = TRUE;
				}
			case ' ':
			case '(':
				str = current_char+1;
				break;
			case '\\':
				esc = TRUE;
			}
		} else
			esc = FALSE;

		current_char = g_utf8_next_char( current_char );
	}

	g_print( "%i%c:\"%s\"{%i[%i%c]%s}\n",
		result->modifier.id,
		result->modifier.sticky?'S':'s',
		result->label,
		result->is_exec?-1:result->action.action.code,
		result->is_exec?-1:result->action.action.modifier.id,
		result->is_exec?' ':result->action.action.modifier.sticky?'S':'s',
		result->is_exec?result->action.exec:""
	);

	return result;
}

static KeyGroup* parse_keygroup( gchar* str,GPtrArray* modlist,GbdEmitter* emitter,GError** err ) {
	g_print( "GRP '%s' {\n",str );
	guint depth = 0;
	gchar* current_char = str,
		* current_key = str;
	gboolean esc = FALSE;
	
	KeyGroup* grp = g_malloc( sizeof( KeyGroup ) );
	grp->keys = NULL;
	grp->keycount = 0;
	grp->colspan = 1;
	grp->rowspan = 1;

	while( current_char[ 0 ] ) {
		gchar c = current_char[ 0 ];

		if( depth==1 && c!=' ' && !current_key || !esc && c=='{' && current_key==str ) {
			current_key = current_char;
		}

		if( !esc ) {
			if( depth==1 && c=='_' )
					grp->rowspan += grp->keys?2:1;

			if( c=='}' ) {
				if( depth<3 && current_key ) {
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
				} else if( depth==0 )
					grp->colspan += grp->keys?2:1;
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

	if( grp->keys ) {
		grp->colspan++;
		grp->rowspan++;
	}
	
	g_print( "}\n" );

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
		x = 0,
		y = 0,
		x_max = 0;

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
					gchar* keygroup = g_strndup( current_block,current_char-current_block );
					KeyGroup* grp = parse_keygroup( keygroup,modifier_list,priv->emitter,err );
					g_free( keygroup );
					if( grp ) {
						g_ptr_array_add( keygroup_list,grp );
	/* Keygroup successfully parsed. Position it. If it fits in, position it
	 * at the current (x,y) position, increasing x by the colspan of the group
	 * and updating the vstack in the range [x,x+colspan-1] to become
	 * y+rowspan-1. If it doesn't fit in, considering the vstack, find the
	 * next x for which vspan in the range of [x,x+colspan-1] is less than
	 * y, and position it there - updating the vstack equally.
	 */
						x = fit_keygroup( grp,vstack,x,y );
						if( vstack->len<=x+grp->colspan )
							g_array_set_size( vstack,x+grp->colspan+1 );
						guint i;
						for( i = x; i<x+grp->colspan; i++ )
							g_array_index( vstack,guint,i )= y+grp->rowspan-1;

						x += grp->colspan;
					}
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
				}
				finished_group = FALSE;
			}
		} else
			esc = FALSE;

		current_char = g_utf8_next_char( current_char );
	}

	g_ptr_array_unref( modifier_list );
	g_ptr_array_unref( keygroup_list );
	g_array_unref( vstack );
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
