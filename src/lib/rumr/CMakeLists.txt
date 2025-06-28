
add_library(rumr OBJECT
  buffer.c
  tcp_comm.c
  client.c
  umr_server.c
  rumr_serial_asic.c
)

target_link_libraries(rumr parson)
