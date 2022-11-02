# Copyright 2017 Edward O'Callaghan <funfunctor@folklore1984.net>

add_subdirectory(database)
add_subdirectory(lowlevel)

add_library(umrcore STATIC
  add_ip_block.c
  apply_bank_address.c
  apply_callbacks.c
  bitfield_print.c
  close_asic.c
  create_mmio_accel.c
  decode_metrics.c
  discover_by_did.c
  discover_by_name.c
  discover_by_ip_table.c
  discovery_table.c
  dump_ib.c
  find_ip.c
  find_reg.c
  free_asic_blocks.c
  ih_decode_vectors.c
  get_ip_rev.c
  mmio.c
  pm4_decode_opcodes.c
  read_mes_stream.c
  read_pm4_stream.c
  read_sdma_stream.c
  read_vram.c
  ring_decode.c
  scan_config.c
  scan_waves.c
  sdma_decode_opcodes.c
  shader_disasm.c
  sq_cmd_halt_waves.c
  testing_harness.c
  version.c
  $<TARGET_OBJECTS:database>
)

target_link_libraries(umrcore ${REQUIRED_EXTERNAL_LIBS})

install(TARGETS umrcore DESTINATION ${CMAKE_INSTALL_LIBDIR})
