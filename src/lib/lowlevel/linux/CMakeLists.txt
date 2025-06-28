add_library(umrlow
  build_discovery_entry_table.c
  discover.c
  enumerate_devices.c
  mmio.c
  query_drm.c
  read_gprwave.c
  read_sensor.c
  mem.c
  umr_free_asic.c
  umr_read_ring_data.c
  umr_shader_disasm.c
  umr_clock.c
  gfxoff.c
)

target_link_libraries(umrlow ${REQUIRED_EXTERNAL_LIBS})

install(TARGETS umrlow DESTINATION ${CMAKE_INSTALL_LIBDIR})
