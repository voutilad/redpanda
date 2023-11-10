add_compile_options(-Wall)

add_executable (mqtt
  main.cc
  logger.h)
target_link_libraries(mqtt
  PRIVATE Seastar::seastar)
set_property(TARGET mqtt PROPERTY POSITION_INDEPENDENT_CODE ON)
