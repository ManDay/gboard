configure_file( de-latin1-mini.gboard de-latin1-mini.gboard )

install( FILES "${CMAKE_BINARY_DIR}/layouts/de-latin1-mini.gboard" DESTINATION "share/gboard/layouts/" )
