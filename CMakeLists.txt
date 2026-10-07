cmake_minimum_required(VERSION 3.31)

find_package(cmake-bare REQUIRED PATHS node_modules/cmake-bare)
find_package(cmake-ports REQUIRED PATHS node_modules/cmake-ports)

project(bare_gpac C)

find_port(libgpac)

add_bare_module(bare_gpac)

target_sources(
  ${bare_gpac}
  PRIVATE
    binding.c
)

target_link_libraries(
  ${bare_gpac}
  PRIVATE
    gpac
)

if(APPLE)
  target_link_options(${bare_gpac}_module PRIVATE LINKER:-dead_strip)
elseif(UNIX)
  target_link_options(${bare_gpac}_module PRIVATE LINKER:--gc-sections)
endif()
