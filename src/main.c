#include "classes/app.h"

int main( int argc,char** argv ) {
	g_type_init( );

	GbdApp* gboard = gbd_app_new( );
	g_application_run( G_APPLICATION( gboard ),argc,argv );

	return 0;
}
