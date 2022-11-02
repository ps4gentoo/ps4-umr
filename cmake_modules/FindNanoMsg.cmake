# Try to find nanomsg
#
# Once done, this will define
#
# NANOMSG_FOUND
# NANOMSG_INCLUDE_DIR
# NANOMSG_LIBRARIES

find_package(PkgConfig)

pkg_check_modules(PC_NANOMSG QUIET nanomsg)

find_path(NANOMSG_INCLUDE_DIR NAMES nn.h
    HINTS
    ${PC_NANOMSG_INCLUDEDIR}
    ${PC_NANOMSG_INCLUDE_DIRS}
    /usr/include
    PATH_SUFFIXES nanomsg
)

find_library(NANOMSG_LIBRARY NAMES nanomsg
    HINTS
    ${PC_NANOMSG_LIBDIR}
    ${PC_NANOMSG_LIBRARY_DIRS}
    /usr/lib64
    /usr/lib
)

SET(NANOMSG_LIBRARIES ${NANOMSG_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NanoMsg DEFAULT_MSG
	NANOMSG_LIBRARIES NANOMSG_INCLUDE_DIR
)

mark_as_advanced(NANOMSG_INCLUDE_DIR NANOMSG_LIBRARIES)
