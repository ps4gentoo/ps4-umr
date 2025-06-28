# Copyright 2017 Edward O'Callaghan <funfunctor@folklore1984.net>

set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_DEB_COMPONENT_INSTALL ON)
cmake_minimum_required(VERSION 3.15)

# Add local repository for FindXXX.cmake modules.
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/" ${CMAKE_MODULE_PATH})
# Export compile_commands.json for IDEs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GitVersion)
get_version_from_tag("1.0.0")

project(umr
    VERSION "${VERSION_STRING}"
    HOMEPAGE_URL "https://gitlab.freedesktop.org/tomstdenis/umr")

include(GNUInstallDirs)
include(CheckSymbolExists)

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

set(CPACK_DEBIAN_PACKAGE_DEPENDS "libncurses6, libpciaccess-dev, libllvm14, zlib1g-dev, libsdl2-dev, libgbm-dev, libglx-dev, libglew-dev")

set(CPACK_RPM_PACKAGE_AUTOREQ 0)
set(CPACK_RPM_PACKAGE_AUTOPROV 0)
set(CPACK_RPM_PACKAGE_REQUIRES "ncurses-libs, libpciaccess, llvm-libs, zlib, SDL2-devel, libgbm-devel, nanomsg-devel")

include(CPack)

set(CMAKE_CXX_STANDARD 11 CACHE STRING "The C++ standard to use")
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(UMR_NO_DRM "Disable libdrm functions to read memory stats (disables server and GUI)" OFF)
option(UMR_NO_LLVM "Disable LLVM shader disasm functions, suggested for LLVM < 7" OFF)
option(UMR_STATIC_EXECUTABLE "Build a static executable" OFF)
option(UMR_STATIC_LLVM "Force using static LLVM libraries instead of preferring dynamic" OFF)
option(UMR_NO_GUI "Disable umr --gui option" OFF)
option(UMR_NO_SERVER "Disable umr --server option" ${UMR_NO_GUI})
option(UMR_INSTALL_DEV "Install the development headers and static library" OFF)
option(UMR_INSTALL_TEST "Install the umr test application" OFF)
# TODO: can server exist without GUI? assume not

# NOT UMR_NO_GUI is confusing. create a hidden option instead. ON by default
# https://cmake.org/cmake/help/latest/module/CMakeDependentOption.html
include(CMakeDependentOption)
cmake_dependent_option(UMR_GUI "Enable umr --gui option" ON "NOT UMR_NO_GUI" OFF)
cmake_dependent_option(UMR_SERVER "Enable umr --server option" ON "NOT UMR_NO_SERVER" OFF)

if(UMR_STATIC_EXECUTABLE)
  if(NOT UMR_NO_LLVM AND NOT UMR_STATIC_LLVM)
    message(STATUS "Static executable build sets UMR_STATIC_LLVM = ON")
    set(UMR_STATIC_LLVM ON)
  endif()
  if(UMR_GUI)
    message(WARNING "Static executable build disables GUI")
    set(UMR_GUI OFF)
  endif()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
endif()

if(UMR_NO_DRM)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUMR_NO_DRM")
  set(UMR_GUI OFF)
  set(UMR_SERVER OFF)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(DRM IMPORTED_TARGET REQUIRED libdrm)
  pkg_check_modules(DRM_AMDGPU IMPORTED_TARGET REQUIRED libdrm_amdgpu)
  include_directories(${DRM_INCLUDE_DIRS} ${DRM_AMDGPU_INCLUDE_DIRS})
endif()

if(UMR_NO_LLVM)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUMR_NO_LLVM")
else()
  find_package(LLVM REQUIRED COMPONENTS all)
  include_directories(${LLVM_INCLUDE_DIRS})
  if(LLVM_CMAKE_DIR)
    list(APPEND CMAKE_MODULE_PATH ${LLVM_CMAKE_DIR})
    include(AddLLVM)
    if((LLVM_LINK_LLVM_DYLIB) AND (NOT UMR_STATIC_LLVM))
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

if(UMR_GUI AND NOT UMR_SERVER)
  message(WARNING "You shouldn't build UMR_GUI without UMR_SERVER!")
endif()

if(UMR_GUI OR UMR_SERVER)
  if(NOT UMR_STATIC_EXECUTABLE)
    set(OpenGL_GL_PREFERENCE "LEGACY" CACHE STRING "Select OpenGL version [LEGACY, GLVND]")
    check_symbol_exists(SYS_pidfd_getfd sys/syscall.h HAVE_SYS_PIDFD_GETFD)
    check_symbol_exists(SYS_pidfd_open sys/syscall.h HAVE_SYS_PIDFD_OPEN)

    if (HAVE_SYS_PIDFD_GETFD AND HAVE_SYS_PIDFD_OPEN)
      find_package(OpenGL COMPONENTS EGL OpenGL)
      pkg_check_modules(GBM IMPORTED_TARGET gbm)
      add_definitions(-DCAN_IMPORT_BO=1)

      set(BO_IMPORT_LIBS OpenGL::EGL PkgConfig::DRM PkgConfig::GBM)
    else()
      # if(UMR_GUI) OpenGL isn't needed for server-only builds
        find_package(OpenGL COMPONENTS OpenGL)
      # endif()
      set(BO_IMPORT_LIBS "")
    endif()

    if (OpenGL_OpenGL_FOUND AND (NOT BO_IMPORT_LIBS OR (OpenGL_EGL_FOUND AND DRM_FOUND AND GBM_FOUND)))
      set(REQUIRED_EXTERNAL_LIBS_GUI ${REQUIRED_EXTERNAL_LIBS_GUI} OpenGL::GL ${BO_IMPORT_LIBS})
    else()
      message(WARNING "Required library (drm: ${DRM_FOUND}, gbm: ${GBM_FOUND}, OpenGL: ${OpenGL_OpenGL_FOUND}, EGL: ${OpenGL_EGL_FOUND}) not found. Disabling --gui")
      set (UMR_GUI OFF)
    endif()
  endif()

  set(REQUIRED_EXTERNAL_LIBS_GUI ${REQUIRED_EXTERNAL_LIBS_GUI} parson)
endif()

if(UMR_GUI)
  add_compile_definitions(UMR_GUI=1)
  find_package(SDL2 REQUIRED)
  include_directories(${SDL2_INCLUDE_DIRS})
  set(REQUIRED_EXTERNAL_LIBS_GUI ${REQUIRED_EXTERNAL_LIBS_GUI} ${SDL2_LIBRARIES})
endif()

if(UMR_SERVER)
  add_compile_definitions(UMR_SERVER=1)
  find_package(NanoMsg)
  if(NANOMSG_FOUND)
    add_compile_definitions(HAVE_NANOMSG=1)
    set(REQUIRED_EXTERNAL_LIBS_GUI ${REQUIRED_EXTERNAL_LIBS_GUI} ${NANOMSG_LIBRARIES} )
    include_directories(${NANOMSG_INCLUDE_DIR})
  else()
    message(FATAL_ERROR "nanomsg library not found. Install it or use -DUMR_NO_SERVER=ON.")
  endif()
endif()


find_library(RT_LIBS rt)

set(REQUIRED_EXTERNAL_LIBS
  ${NCURSES_LDFLAGS}
  ${PC_PCIACCESS_LDFLAGS}
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
