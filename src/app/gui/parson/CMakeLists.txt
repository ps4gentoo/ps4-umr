add_library(parson OBJECT
    parson.c parson.h)
target_include_directories(parson PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
