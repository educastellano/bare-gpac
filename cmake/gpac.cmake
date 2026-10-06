include(FetchContent)

# GPAC 26.07.0. Keep the archive pinned and verified for reproducible builds.
FetchContent_Declare(
  gpac
  URL https://codeload.github.com/gpac/gpac/tar.gz/a07cbfff238a331233e11e916f9fb185d5da8604
  URL_HASH SHA256=92ee747c2b0057ec615a289198cf484c82ba317b5eabf8c52491e15b12a40bff
  SOURCE_SUBDIR bare-isoff
)

FetchContent_MakeAvailable(gpac)

find_package(Git REQUIRED)

file(GLOB gpac_patches CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/patches/*.patch")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${gpac_patches})
foreach(patch IN LISTS gpac_patches)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} apply --reverse --check "${patch}"
    WORKING_DIRECTORY "${gpac_SOURCE_DIR}"
    RESULT_VARIABLE applied
    OUTPUT_QUIET ERROR_QUIET
  )
  if(NOT applied EQUAL 0)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} apply "${patch}"
      WORKING_DIRECTORY "${gpac_SOURCE_DIR}"
      COMMAND_ERROR_IS_FATAL ANY
    )
  endif()
endforeach()

configure_file(
  ${CMAKE_CURRENT_LIST_DIR}/config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/gpac/config.h
)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/gpac/gpac/revision.h" "#define GPAC_GIT_REVISION \"a07cbfff238a331233e11e916f9fb185d5da8604\"\n")

add_library(gpac_isoff STATIC)

set_target_properties(
  gpac_isoff
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden
)

target_include_directories(
  gpac_isoff
  PUBLIC
    ${gpac_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/gpac
)

target_compile_definitions(
  gpac_isoff
  PUBLIC
    GPAC_HAVE_CONFIG_H
    GPAC_STATIC_LIB
  PRIVATE
    GF_EXPORT=
    GPAC_DISABLE_REMOTERY
)

target_sources(
  gpac_isoff
  PRIVATE
    ${gpac_SOURCE_DIR}/src/isomedia/avc_ext.c
    ${gpac_SOURCE_DIR}/src/isomedia/box_code_3gpp.c
    ${gpac_SOURCE_DIR}/src/isomedia/box_code_apple.c
    ${gpac_SOURCE_DIR}/src/isomedia/box_code_base.c
    ${gpac_SOURCE_DIR}/src/isomedia/box_code_drm.c
    ${gpac_SOURCE_DIR}/src/isomedia/box_code_meta.c
    ${gpac_SOURCE_DIR}/src/isomedia/box_funcs.c
    ${gpac_SOURCE_DIR}/src/isomedia/data_map.c
    ${gpac_SOURCE_DIR}/src/isomedia/drm_sample.c
    ${gpac_SOURCE_DIR}/src/isomedia/isom_intern.c
    ${gpac_SOURCE_DIR}/src/isomedia/isom_read.c
    ${gpac_SOURCE_DIR}/src/isomedia/isom_store.c
    ${gpac_SOURCE_DIR}/src/isomedia/isom_write.c
    ${gpac_SOURCE_DIR}/src/isomedia/media.c
    ${gpac_SOURCE_DIR}/src/isomedia/media_odf.c
    ${gpac_SOURCE_DIR}/src/isomedia/meta.c
    ${gpac_SOURCE_DIR}/src/isomedia/sample_descs.c
    ${gpac_SOURCE_DIR}/src/isomedia/stbl_read.c
    ${gpac_SOURCE_DIR}/src/isomedia/stbl_write.c
    ${gpac_SOURCE_DIR}/src/isomedia/track.c
    ${gpac_SOURCE_DIR}/src/isomedia/tx3g.c
    ${gpac_SOURCE_DIR}/src/isomedia/iff.c
    ${gpac_SOURCE_DIR}/src/odf/desc_private.c
    ${gpac_SOURCE_DIR}/src/odf/descriptors.c
    ${gpac_SOURCE_DIR}/src/odf/odf_code.c
    ${gpac_SOURCE_DIR}/src/odf/odf_codec.c
    ${gpac_SOURCE_DIR}/src/odf/odf_command.c
    ${gpac_SOURCE_DIR}/src/odf/slc.c
    ${gpac_SOURCE_DIR}/src/utils/os_divers.c
    ${gpac_SOURCE_DIR}/src/utils/os_file.c
    ${gpac_SOURCE_DIR}/src/utils/list.c
    ${gpac_SOURCE_DIR}/src/utils/bitstream.c
    ${gpac_SOURCE_DIR}/src/utils/constants.c
    ${gpac_SOURCE_DIR}/src/utils/error.c
    ${gpac_SOURCE_DIR}/src/utils/alloc.c
    ${gpac_SOURCE_DIR}/src/utils/url.c
    ${gpac_SOURCE_DIR}/src/utils/configfile.c
    ${gpac_SOURCE_DIR}/src/utils/gzio.c
    ${gpac_SOURCE_DIR}/src/utils/os_thread.c
    ${gpac_SOURCE_DIR}/src/utils/os_config_init.c
    ${gpac_SOURCE_DIR}/src/utils/utf.c
    ${gpac_SOURCE_DIR}/src/utils/token.c
    ${gpac_SOURCE_DIR}/src/utils/base_encoding.c
    ${gpac_SOURCE_DIR}/src/media_tools/av_parsers.c
)

if(UNIX)
  target_compile_options(gpac_isoff PRIVATE -ffunction-sections -fdata-sections -Wno-pointer-sign)
  target_link_libraries(gpac_isoff PRIVATE m ${CMAKE_DL_LIBS})
endif()

if(WIN32)
  target_link_libraries(gpac_isoff PRIVATE winmm shell32 advapi32)
endif()
