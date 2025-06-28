# Copyright 2017 Edward O'Callaghan <funfunctor@folklore1984.net>

project(umrtestapp)

set(TEST_SRC test_framework.c
  test_framework.c
  main.c
  test_mmio.c
  test_vm.c
)

if(UMR_GUI OR UMR_SERVER)
  add_compile_definitions(COMMANDS_TEST=1)
  set(TEST_SRC ${TEST_SRC} test_server.c ../app/gui/commands.c)
endif()

add_executable(umrtest ${TEST_SRC})

target_link_libraries(umrtest umrcore)
target_link_libraries(umrtest umrlow)
target_link_libraries(umrtest umrcore) #circular dependency umrcode->umrlow->umrcore

target_link_libraries(umrtest ${REQUIRED_EXTERNAL_LIBS})

if(UMR_INSTALL_TEST)
	install(TARGETS umrtest DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
