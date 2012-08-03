configure_file( de-latin1-mini.gboard de-latin1-mini.gboard )
configure_file( de-latin1-short.gboard de-latin1-short.gboard )

install( FILES "${CMAKE_BINARY_DIR}/layouts/de-latin1-mini.gboard" DESTINATION "share/gboard/layouts/" )
install( FILES "${CMAKE_BINARY_DIR}/layouts/de-latin1-short.gboard" DESTINATION "share/gboard/layouts/" )
