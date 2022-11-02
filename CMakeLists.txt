# Copyright 2017 Edward O'Callaghan <funfunctor@folklore1984.net>

set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_DEB_COMPONENT_INSTALL ON)
cmake_minimum_required(VERSION 3.0.1)

# Add local repository for FindXXX.cmake modules.
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/" ${CMAKE_MODULE_PATH})

include(GitVersion)
get_version_from_tag("1.0.0")

project(umr
    VERSION "${VERSION_STRING}"
    HOMEPAGE_URL "https://gitlab.freedesktop.org/tomstdenis/umr")

include(GNUInstallDirs)

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "User mode register debugger for AMDGPU devices using IP cleared registers/definitions.")
set(CPACK_PACKAGE_VENDOR "AMD")
set(CPACK_PACKAGE_CONTACT "tom.stdenis@amd.com")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RPM_PACKAGE_GROUP "System/Monitoring")
set(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
set(CPACK_GENERATOR "RPM;DEB" CACHE STRING "Package binary using these generators")
set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}" CACHE STRING "Default packaging prefix." )
mark_as_advanced(CPACK_GENERATOR)

set(CPACK_DEBIAN_PACKAGE_DEPENDS "libncurses6, libpciaccess-dev, libllvm11, zlib1g-dev")

set(CPACK_RPM_PACKAGE_AUTOREQ 0)
set(CPACK_RPM_PACKAGE_AUTOPROV 0)
set(CPACK_RPM_PACKAGE_REQUIRES "ncurses-libs, libpciaccess, llvm11-libs, zlib")

include(CPack)

set(RELEASE_VERSION \"${VERSION_STRING}\")
add_definitions(-DUMR_BUILD_VER=${RELEASE_VERSION})
add_definitions(-DUMR_BUILD_REV=\"${VERSION_REV}\")
add_definitions(-DUMR_BUILD_BRANCH=\"${VERSION_BRANCH}\")
add_definitions(-DUMR_DB_DIR=\"${CMAKE_INSTALL_FULL_DATADIR}/umr/database/\")
add_definitions(-DUMR_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\")

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIRS})

find_package(PCIAccess REQUIRED)
include_directories(${PCIACCESS_INCLUDE_DIR})

option(UMR_NO_DRM "Disable libdrm functions to read memory stats" OFF)
option(UMR_NO_LLVM "Disable LLVM shader disasm functions, suggested for LLVM < 7" OFF)
option(UMR_NEED_RT "Link against RT library, needed for older glibc versions" OFF)
option(UMR_NO_GUI "Disable umr --server and --gui options" OFF)

if(UMR_NO_DRM)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUMR_NO_DRM")
else()
find_package(LibDRM REQUIRED)
include_directories(${LIBDRM_INCLUDE_DIR})
endif()

if(UMR_NO_LLVM)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUMR_NO_LLVM")
else()
find_package(LLVM REQUIRED COMPONENTS all)
include_directories(${LLVM_INCLUDE_DIRS})

if(LLVM_CMAKE_DIR)
  list(APPEND CMAKE_MODULE_PATH ${LLVM_CMAKE_DIR})
  include(AddLLVM)

  if (LLVM_LINK_LLVM_DYLIB)
    set(LLVM_LIBS LLVM)
  else()
    LLVM_MAP_COMPONENTS_TO_LIBNAMES(LLVM_LIBS
      AllTargetsDescs
      AllTargetsDisassemblers
      AllTargetsInfos
      MC
      Support
      MCDisassembler
    )
  endif()
endif()
endif()

if (NOT UMR_NO_GUI)
find_package(SDL2 REQUIRED)
find_package(NanoMsg)
include_directories(${SDL2_INCLUDE_DIRS})
  if (NANOMSG_FOUND)
    include_directories(${NANOMSG_INCLUDE_DIR})
    set(REQUIRED_EXTERNAL_LIBS_GUI ${NANOMSG_LIBRARIES} ${SDL2_LIBRARIES})
    set(UMR_GUI_REMOTE 1)
  else()
    message("nanomsg library not found. The remote GUI options won't be available.")
    set(REQUIRED_EXTERNAL_LIBS_GUI ${SDL2_LIBRARIES})
    set(UMR_GUI_REMOTE 0)
  endif()
endif()

if(UMR_NEED_RT)
set(RT_LIBS "-lrt")
else()
set(RT_LIBS "")
endif()

set(REQUIRED_EXTERNAL_LIBS
  ${CURSES_LIBRARIES}
  ${PCIACCESS_LIBRARIES}
  Threads::Threads
  ${LLVM_LIBS}
  ${RT_LIBS}
  ${REQUIRED_EXTERNAL_LIBS_GUI}
)

# Global setting: build everything position independent
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# CFLAGS += -Wall -W -O2 -g3 -Isrc/ -DPIC -fPIC
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -W -g3 -pedantic -Werror=format-security -Wunused-result")

add_subdirectory(src)
add_subdirectory(doc)
