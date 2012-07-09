#ifndef __GBD_APP_H__
#define __GBD_APP_H__

#include <gtk/gtk.h>

/// Return GType; Return Class
#define GBD_TYPE_APP (gbd_app_get_type( ))
#define GBD_APP_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS( (obj),GBD_TYPE_APP,GbdAppClass ))

/// Check whether instance is derived; Cast it
#define GBD_IS_APP(obj) (G_TYPE_CHECK_INSTANCE_TYPE( (obj),GBD_TYPE_APP ))
#define GBD_APP(obj) (G_TYPE_CHECK_INSTANCE_CAST( (obj),GBD_TYPE_APP,GbdApp ))

/// Check whether class is derived; Cast it
#define GBD_IS_APP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE( (klass),GBD_TYPE_APP ))
#define GBD_APP_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST( (klass),GBD_TYPE_APP,GbdAppClass ))

typedef struct GbdAppPrivate GbdAppPrivate;

typedef struct {
	GtkApplicationClass parent;
} GbdAppClass;

typedef struct {
	GtkApplication parent;

	GbdAppPrivate* priv;
} GbdApp;

GType gbd_app_get_type( void );
GbdApp* gbd_app_new( void );

#endif
