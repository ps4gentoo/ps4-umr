# Copyright 2017 Edward O'Callaghan <funfunctor@folklore1984.net>

#where app .h file can be found
include_directories(inc)

if(UMR_GUI OR UMR_SERVER)
  include_directories("gui/parson")
  set (GUI_SOURCE gui/commands.c)
endif()

if(UMR_GUI)
  include_directories("gui/glad/include")
  include_directories("gui/imgui")
  set (GUI_SOURCE ${GUI_SOURCE}
                  umr_gui.cpp
                  gui/kernel_trace_event.cpp
                  gui/imgui/imgui.cpp gui/imgui/imgui_draw.cpp gui/imgui/imgui_impl_opengl3.cpp
                  gui/imgui/imgui_tables.cpp
                  gui/imgui/imgui_impl_sdl.cpp gui/imgui/imgui_widgets.cpp
                  gui/glad/src/glad.c
                  )
  add_compile_definitions(IMGUI_IMPL_OPENGL_LOADER_GLAD=1)
endif()

if(UMR_SERVER)
  set (GUI_SOURCE ${GUI_SOURCE} server.c)
endif()

#application objects
add_library(umrapp
  scriptware.c
  print.c
  print_config.c
  profile.c
  scan.c
  scan_log.c
  top.c
  umr_lookup.c
  set_bit.c
  set_reg.c
  print_waves.c
  enum.c
  power.c
  clock.c
  pp_table.c
  navi10_ppt.c
  read_metrics.c
  ring_stream_read.c
  vbios.c
  discovery.c
  print_cpg.c
  print_cpc.c
  print_sdma.c
  runlist.c
  ${GUI_SOURCE}
)
target_link_libraries(umrapp parson ${UMRAPP_DRM_LIBS})

add_executable(umr main.c)
target_link_libraries(umr umrapp umrlow umrcore umrlow)
if(UMR_GUI)
    # Used by glad
    target_link_libraries(umr dl)
endif()

target_link_libraries(umr ${REQUIRED_EXTERNAL_LIBS})

install(TARGETS umr DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS umrapp DESTINATION ${CMAKE_INSTALL_LIBDIR})

if(UMR_GUI)
	execute_process(COMMAND ln -sf umr umrgui
	    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	)
	install(FILES umrgui DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

find_package(bash-completion QUIET)
if(BASH_COMPLETION_FOUND AND CMAKE_INSTALL_DATAROOTDIR)
	set(BASH_COMPLETION_COMPLETIONSDIR "${CMAKE_INSTALL_DATAROOTDIR}/bash-completion/completions")
	set(BASH_COMPLETION_COMPLETIONSDIR "${CMAKE_INSTALL_DATAROOTDIR}/bash-completion/completions" CACHE PATH "Directory bash-completion is installed to")
	message(STATUS "Bash-completion moved to: ${BASH_COMPLETION_COMPLETIONSDIR}")
endif()
if(NOT BASH_COMPLETION_FOUND)
	set(BASH_COMPLETION_COMPLETIONSDIR "/usr/share/bash-completion/completions")
endif()
install(FILES ${PROJECT_SOURCE_DIR}/scripts/umr-completion.bash DESTINATION ${BASH_COMPLETION_COMPLETIONSDIR} RENAME umr)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/database/ DESTINATION ${CMAKE_INSTALL_DATADIR}/umr/database/)

if(UMR_INSTALL_DEV)
	install(FILES ${PROJECT_SOURCE_DIR}/src/umr.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/)
	install(FILES ${PROJECT_SOURCE_DIR}/src/umr_metrics.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/)
	install(FILES ${PROJECT_SOURCE_DIR}/src/umr_discovery.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/)
	install(FILES ${PROJECT_SOURCE_DIR}/src/umr_waves.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/)
	install(FILES ${PROJECT_SOURCE_DIR}/src/umr_mmio.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/)
	install(FILES ${PROJECT_SOURCE_DIR}/src/umr_packet.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/)
	install(FILES ${PROJECT_SOURCE_DIR}/src/umr_ih.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/)
	install(FILES ${PROJECT_SOURCE_DIR}/src/umr_vm.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/)
	install(FILES ${PROJECT_SOURCE_DIR}/src/umr_test_harness.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/)
	install(FILES ${PROJECT_SOURCE_DIR}/src/umr_clock.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/)
	install(FILES ${PROJECT_SOURCE_DIR}/src/umr_database_discovery.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/)
	install(FILES ${PROJECT_SOURCE_DIR}/src/umr_packet_hsa.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/)
	install(FILES ${PROJECT_SOURCE_DIR}/src/umr_packet_mes.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/)
	install(FILES ${PROJECT_SOURCE_DIR}/src/umr_packet_mqd.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/)
	install(FILES ${PROJECT_SOURCE_DIR}/src/umr_packet_pm4.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/)
	install(FILES ${PROJECT_SOURCE_DIR}/src/umr_packet_sdma.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/)
	install(FILES ${PROJECT_SOURCE_DIR}/src/umr_packet_umsch.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/)
	install(FILES ${PROJECT_SOURCE_DIR}/src/umr_packet_vcn.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/)
	install(FILES ${PROJECT_SOURCE_DIR}/src/umr_packet_vpe.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/)
	install(FILES ${PROJECT_SOURCE_DIR}/src/umrapp.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/)
	install(FILES ${PROJECT_SOURCE_DIR}/src/umr_rumr.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/)
	install(FILES ${PROJECT_BINARY_DIR}/src/lib/lowlevel/linux/libumrlow.a DESTINATION ${CMAKE_INSTALL_LIBDIR}/)
	install(FILES ${PROJECT_BINARY_DIR}/src/lib/libumrcore.a DESTINATION ${CMAKE_INSTALL_LIBDIR}/)
endif()
