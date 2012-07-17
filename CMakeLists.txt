cmake_minimum_required( VERSION 2.8 )

project( GBoard C )

include( FindPkgConfig )

pkg_check_modules( GTK REQUIRED gtk+-3.0 )

include_directories( ${GTK_INCLUDE_DIRS} )
link_directories( ${GTK_LIBRARY_DIRS} )

add_executable( gboard src/main.c src/classes/app.c src/classes/x11emitter.c src/interfaces/emitter.c src/classes/layout.c )
target_link_libraries( gboard ${GTK_LDFLAGS} )

add_library( gboard_im MODULE src/gboard_im.c )
target_link_libraries( gboard ${GTK_LDFLAGS} )
