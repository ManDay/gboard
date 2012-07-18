#include "layout.h"

#include <string.h>

struct GbdLayoutPrivate {
	guint dummy;
};

typedef struct {
	guchar id;
	gboolean sticky;
} KeyModifier;

typedef union {
	struct {
		guint code;
		KeyModifier modifier;
	};
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

static void class_init( GbdLayoutClass*,gpointer );
static void instance_init( GbdLayout* );
static void instance_finalize( GbdLayout* );

static void delete_keygroup( KeyGroup* );

static void class_init( GbdLayoutClass* klass,gpointer udata ) {
	GObjectClass* klass_go = G_OBJECT_CLASS( klass );

	g_type_class_add_private( klass,sizeof( GbdLayoutPrivate ) );

	klass_go->finalize = (GObjectFinalizeFunc)instance_finalize;
}

static void instance_init( GbdLayout* self ) {
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE( self,GBD_TYPE_LAYOUT,GbdLayoutPrivate );
}

static void instance_finalize( GbdLayout* self ) {
	G_OBJECT_CLASS( g_type_class_peek( G_TYPE_OBJECT ) )->finalize( G_OBJECT( self ) );
}

static Key* parse_key( gchar* str,GPtrArray* modlist,GError** err ) {
	g_print( "KEY '%s'\n",str );
	return NULL;
}

static KeyGroup* parse_keygroup( gchar* str,GPtrArray* modlist,GError** err ) {
	g_print( "GRP {\n" );
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

		if( !esc ) {
			gchar c = current_char[ 0 ];

			if( depth==1 ) {
				if( c!=' ' ) {
					if( c=='_' )
						grp->rowspan += grp->keys?2:1;
					if( !current_key || c=='{' && current_key==str )
						current_key = current_char;
				}
			}

			if( c=='}' ) {
				if( depth<3 && current_key && current_char-current_key>1) {
					gchar* keystr = g_strndup( current_key,current_char-current_key+1 );
					Key* key = parse_key( keystr,modlist,err );
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
					current_key = NULL;
				} else if( depth=0 )
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

	return NULL;
}

static void delete_keygroup( KeyGroup* grp ) {
	guint i;
	for( i = 0; i<grp->keycount; i++ )
		g_free( grp->keys[ i ].label );
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

gboolean gbd_layout_parse( GbdLayout* self,gchar* str,GError** err ) {
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
					KeyGroup* grp = parse_keygroup( keygroup,modifier_list,err );
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

GbdLayout* gbd_layout_new( gchar* str ) {
	GbdLayout* self = g_object_new( GBD_TYPE_LAYOUT,NULL );
	if( str )
		gbd_layout_parse( self,str,NULL );

	return self;
}

GQuark gbd_layout_error_quark( ) {
	return g_quark_from_static_string( "gbd-layout-error-quark" );
}
