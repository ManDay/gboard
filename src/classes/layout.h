#ifndef __GBD_LAYOUT_H__
#define __GBD_LAYOUT_H__

#include <glib-object.h>

#include "../interfaces/emitter.h"

#define GBD_LAYOUT_ERROR (gbd_layout_error_quark( ))

/// Return GType; Return Class
#define GBD_TYPE_LAYOUT (gbd_layout_get_type( ))

/// Check whether instance is derived; Cast it
#define GBD_IS_LAYOUT(obj) (G_TYPE_CHECK_INSTANCE_TYPE( (obj),GBD_TYPE_LAYOUT ))
#define GBD_LAYOUT(obj) (G_TYPE_CHECK_INSTANCE_CAST( (obj),GBD_TYPE_LAYOUT,GbdLayout ))

/// Check whether class is derived; Cast it
#define GBD_IS_LAYOUT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE( (klass),GBD_TYPE_LAYOUT ))
#define GBD_LAYOUT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST( (klass),GBD_TYPE_LAYOUT,GbdLayoutClass ))

typedef enum {
	GBD_LAYOUT_ERROR_CODEPAGE,
	GBD_LAYOUT_ERROR_KEY
} GbdLayoutError;

typedef struct GbdLayoutPrivate GbdLayoutPrivate;

typedef struct {
	GObject parent;

	GbdLayoutPrivate* priv;
} GbdLayout;

typedef struct {
	GObjectClass parent;
} GbdLayoutClass;

typedef struct {
	guint id;
	gboolean sticky;
} GbdKeyModifier;

typedef union {
	struct {
		guint64 code;
		GbdKeyModifier modifier;
	} action;
	gchar* exec;
} GbdKeyAction;

typedef struct {
	GbdKeyModifier filter;
	gboolean is_image;
	gchar* label;
	gboolean is_exec;
	GbdKeyAction action;
}	GbdKey;

typedef struct {
	GbdKey* keys;
	guint keycount,col,row,colspan,rowspan;
} GbdKeyGroup;

gboolean gbd_layout_parse( GbdLayout*,gchar*,GError** );
gboolean gbd_key_is_mod( const GbdKey* );
const GbdKeyGroup* gbd_layout_at( GbdLayout*,gint,gint );
const GbdKey* gbd_key_current( const GbdKeyGroup*,GQueue* );

GType gbd_layout_get_type( void );
GbdLayout* gbd_layout_new( gchar*,GbdEmitter* );

GQuark gbd_layout_error_quark( void ); 

#endif
