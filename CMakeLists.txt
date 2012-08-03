cmake_minimum_required( VERSION 2.8 )

project( GBoard C )

set( GBD_BUILD_GTK_IM TRUE CACHE BOOL "Whether to build the GTK IM-Module" )
set( GBD_INSTALL_LAYOUTS TRUE CACHE BOOL "Whether to install layouts" )
set( GBD_BUILD_X11 TRUE CACHE BOOL "Whether to build the X11-Emitter" )
set( GBD_NAME "org.gitorious.gboard" CACHE STRING "Unique dot separated name for GBoard" )
set( GBD_PATH "/org/gitorious/gboard" CACHE STRING "Unique slash separated path for GBoard - must not end with a slash" )
set( GBD_SCHEMAS_PATH "share/glib-2.0/schemas" CACHE PATH "Install path for schema definition" )
set( GBD_GTK_IM_PATH "lib/gtk-3.0/3.0.0" CACHE PATH "Install path for IM Module and associated cache" )
set( GBD_SERVICE_PATH "share/dbus-1/services" CACHE PATH "Install path for DBUS Service defintion" )

mark_as_advanced( GBD_NAME GBD_PATH )

if( IS_ABSOLUTE "${GBD_SCHEMAS_PATH}" )
	set( GBD_SCHEMAS_TARGET "${GBD_SCHEMAS_PATH}" )
else( )
	set( GBD_SCHEMAS_TARGET "${CMAKE_INSTALL_PREFIX}/${GBD_SCHEMAS_PATH}" )
endif( )

if( IS_ABSOLUTE "${GBD_GTK_IM_PATH}" )
	set( GBD_GTK_IM_TARGET "${GBD_GTK_IM_PATH}" )
else( )
	set( GBD_GTK_IM_TARGET "${CMAKE_INSTALL_PREFIX}/${GBD_GTK_IM_PATH}" )
endif( )

include( FindPkgConfig )

pkg_check_modules( GTK REQUIRED gtk+-3.0 )
include_directories( ${GTK_INCLUDE_DIRS} )
link_directories( ${GTK_LIBRARY_DIRS} )

if( GBD_BUILD_X11 )
	find_package( X11 REQUIRED )
	include_directories( ${X11_XTest_INCLUDE_PATH} )
	set( GBD_X11_INCLUDE "${X11_X11_INCLUDE_PATH}" )
	message( STATUS "Reading GBoard X11-Emitter key definitions from ${X11_X11_INCLUDE_PATH}" )
endif( )

set( GBD_PATH_TERMED "${GBD_PATH}/" )
configure_file( src/org.gitorious.gboard.gschema.xml org.gitorious.gboard.gschema.xml )
configure_file( src/gbd_config.h src/gbd_config.h )
configure_file( gboard.service gboard.service )

include_directories( "${CMAKE_BINARY_DIR}/src" BEFORE )

add_executable( gboard src/main.c src/classes/app.c src/classes/x11emitter.c src/interfaces/emitter.c src/classes/layout.c src/classes/keyboard.c )
target_link_libraries( gboard ${GTK_LDFLAGS} )
if( GBD_BUILD_X11 )
	target_link_libraries( gboard ${X11_XTest_LIB} )
endif( )

if( GBD_BUILD_GTK_IM )
	add_library( gboardim MODULE src/gboardim.c )
	target_link_libraries( gboardim ${GTK_LDFLAGS} )

	set_property( TARGET gboardim PROPERTY PREFIX "im-" )
	if( ${CMAKE_HOST_UNIX} )
		set_property( TARGET gboardim PROPERTY SUFFIX ".so" )
		set( GBD_IMLIBNAME "im-gboardim.so" )
	else( )
		set_property( TARGET gboardim PROPERTY SUFFIX ".dll" )
		set( GBD_IMLIBNAME "im-gboardim.dll" )
	endif( )

	install( TARGETS gboardim DESTINATION "${GBD_GTK_IM_TARGET}/immodules" )
	install( CODE "set( ENV{GTK_IM_MODULE_FILE} \"${GBD_GTK_IM_TARGET}/immodules.cache\" )\nexecute_process( COMMAND gtk-query-immodules-3.0 --update-cache \"${GBD_GTK_IM_TARGET}/immodules/${GBD_IMLIBNAME}\" )" )
endif( )

if( GBD_INSTALL_LAYOUTS )
# Install all layouts individually from subdirectory
#	install( DIRECTORY layouts DESTINATION share/gboard/ )
	add_subdirectory( layouts )
endif( )

install( TARGETS gboard DESTINATION bin )
install( FILES "${CMAKE_BINARY_DIR}/org.gitorious.gboard.gschema.xml" DESTINATION "${GBD_SCHEMAS_TARGET}" )
install( FILES "images/gboard-icon.png" "src/preferences.builder" DESTINATION share/gboard/ )
install( FILES "${CMAKE_BINARY_DIR}/gboard.service" DESTINATION "${GBD_SERVICE_PATH}" )
install( CODE "execute_process( COMMAND glib-compile-schemas \"${GBD_SCHEMAS_TARGET}\" )" )
