# Copyright 2017 Edward O'Callaghan <funfunctor@folklore1984.net>

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/)

add_subdirectory(app/gui/parson)

#if(UMR_GUI AND UMR_SERVER)
    add_subdirectory(test)
#endif()

add_subdirectory(lib)
add_subdirectory(app)
