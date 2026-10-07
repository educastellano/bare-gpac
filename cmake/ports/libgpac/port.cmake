include_guard(GLOBAL)

if(WIN32)
  set(lib gpac.lib)
else()
  set(lib libgpac.a)
endif()

# GPAC 26.07.0, pinned to the same source revision on every platform.
set(revision a07cbfff238a331233e11e916f9fb185d5da8604)

declare_port(
  "github:gpac/gpac#${revision}"
  gpac
  SUBMODULES OFF
  BYPRODUCTS lib/${lib}
  ARGS -DGPAC_GIT_REVISION=${revision}
  PATCHES
    patches/01-no-fragments.patch
    patches/02-minimal-guards.patch
    patches/03-remove-item-references.patch
    patches/04-modification-notices.patch
    patches/05-cmake.patch
)

add_library(gpac STATIC IMPORTED GLOBAL)

add_dependencies(gpac ${gpac})

set_target_properties(
  gpac
  PROPERTIES
  IMPORTED_LOCATION "${gpac_PREFIX}/lib/${lib}"
)

file(MAKE_DIRECTORY "${gpac_PREFIX}/include")

target_include_directories(
  gpac
  INTERFACE "${gpac_PREFIX}/include"
)

target_compile_definitions(
  gpac
  INTERFACE
    GPAC_HAVE_CONFIG_H
    GPAC_STATIC_LIB
)

if(UNIX)
  target_link_libraries(gpac INTERFACE m ${CMAKE_DL_LIBS})
elseif(WIN32)
  target_link_libraries(gpac INTERFACE winmm shell32 advapi32)
endif()
