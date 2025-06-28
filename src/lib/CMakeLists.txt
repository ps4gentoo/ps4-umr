# Copyright 2017 Edward O'Callaghan <funfunctor@folklore1984.net>

add_subdirectory(database)
add_subdirectory(lowlevel)
add_subdirectory(rumr)

add_library(umrcore
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
  find_ip.c
  find_reg.c
  free_asic_blocks.c
  ih_decode_vectors.c
  get_ip_rev.c
  mmio.c
  mqd_decode.c
  packet_stream.c
  pm4_decode_opcodes.c
  pm4_lite.c
  read_hsa_stream.c
  read_mes_stream.c
  read_pm4_stream.c
  read_sdma_stream.c
  read_umsch_stream.c
  read_vpe_stream.c
  read_vram.c
  ring_is_halted.c
  scan_config.c
  scan_waves.c
  sdma_decode_opcodes.c
  shader_disasm.c
  sq_cmd_halt_waves.c
  testing_harness.c
  vcn_dec_parser.c
  vcn_enc_opcodes.c
  read_vcn_enc_stream.c
  read_vcn_dec_stream.c
  version.c
  vpe_decode_opcodes.c
  $<TARGET_OBJECTS:database>
  $<TARGET_OBJECTS:rumr>
)

target_link_libraries(umrcore ${REQUIRED_EXTERNAL_LIBS})

install(TARGETS umrcore DESTINATION ${CMAKE_INSTALL_LIBDIR})
