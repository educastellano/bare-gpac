include_guard(GLOBAL)

set(lib libgpac_static.a)

set(args
  --static-build
  --enable-pic
  --isomedia-only
  --disable-all
  --enable-isoff
  --enable-isoff-write
  --enable-isoff-frag
  --enable-parsers
  --disable-rmtws
  --disable-directfb
  --disable-hid
  --disable-lzma
  --disable-vtb
)

foreach(package
  zlib opensvc openhevc platinum freetype ssl jpeg openjpeg png mad a52 xvid
  faad ffmpeg freenect vorbis theora nghttp2 ngtcp2 nghttp3 oss dvb4linux alsa
  pulseaudio jack directfb hid lzma tinygl vtb ogg sdl caption mpeghdec libcaca curl
)
  list(APPEND args --use-${package}=no)
endforeach()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  list(APPEND args --enable-debug)
endif()

if(APPLE)
  list(APPEND args --target-os=Darwin)
elseif(ANDROID)
  list(APPEND args --target-os=android)
elseif(WIN32)
  list(APPEND args --target-os=mingw32)
else()
  list(APPEND args --target-os=${CMAKE_SYSTEM_NAME})
endif()

set(arch "${CMAKE_SYSTEM_PROCESSOR}")
if(APPLE AND CMAKE_OSX_ARCHITECTURES)
  set(arch "${CMAKE_OSX_ARCHITECTURES}")
endif()
string(TOLOWER "${arch}" arch)
if(arch MATCHES "amd64|x64")
  set(arch x86_64)
elseif(arch MATCHES "armv7|armeabi")
  set(arch arm)
endif()
list(APPEND args --cpu=${arch})

set(env)
# Keep GPAC symbols private so the addon linker can discard unused code.
set(cflags "-DGPAC_STATIC_LIB -DGF_EXPORT= -ffunction-sections -fdata-sections")
set(ldflags)

if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  string(APPEND cflags " -g")
elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
  string(APPEND cflags " -Os")
endif()

foreach(tool CC AR RANLIB)
  if(tool STREQUAL "CC")
    set(program "${CMAKE_C_COMPILER}")
  else()
    set(program "${CMAKE_${tool}}")
  endif()
  if(NOT program)
    continue()
  endif()

  cmake_path(GET program PARENT_PATH path)
  cmake_path(GET program FILENAME name)
  if(WIN32)
    string(REPLACE "clang-cl" "clang" name "${name}")
    string(REPLACE "llvm-lib" "llvm-ar" name "${name}")
  endif()
  list(APPEND env "${tool}=${name}" --modify "PATH=path_list_prepend:${path}")
endforeach()

if(CMAKE_C_COMPILER_TARGET)
  string(APPEND cflags " --target=${CMAKE_C_COMPILER_TARGET}")
  string(APPEND ldflags " --target=${CMAKE_C_COMPILER_TARGET}")
endif()
if(APPLE)
  string(APPEND cflags " -isysroot ${CMAKE_OSX_SYSROOT}")
  string(APPEND ldflags " -isysroot ${CMAKE_OSX_SYSROOT}")
elseif(CMAKE_SYSROOT)
  string(APPEND cflags " --sysroot=${CMAKE_SYSROOT}")
  string(APPEND ldflags " --sysroot=${CMAKE_SYSROOT}")
endif()
if(IOS)
  string(APPEND cflags " -DGPAC_CONFIG_IOS")
elseif(ANDROID)
  string(APPEND cflags " -DGPAC_CONFIG_ANDROID")
elseif(WIN32)
  string(APPEND cflags " -DWIN32 -DWIN32_LEAN_AND_MEAN")
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    string(APPEND cflags " -fms-runtime-lib=static_dbg")
  else()
    string(APPEND cflags " -fms-runtime-lib=static")
  endif()
endif()
if(CMAKE_LINKER_TYPE MATCHES "LLD")
  string(APPEND ldflags " -fuse-ld=lld")
endif()
list(APPEND args "--extra-cflags=${cflags}" "--extra-ldflags=${ldflags}")

set(revision a07cbfff238a331233e11e916f9fb185d5da8604) # v26.07.0

declare_port(
  "github:gpac/gpac#${revision}"
  gpac
  AUTOTOOLS
  ENTRYPOINT <SOURCE_DIR>/configure
  SUBMODULES OFF
  BYPRODUCTS lib/${lib}
  ARGS ${args}
  ENV ${env}
  PATCHES
    patches/01-remove-item-references.patch
    patches/02-library-only.patch
    patches/03-windows-compat.patch
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
    GPAC_STATIC_LIB
    $<$<PLATFORM_ID:iOS>:GPAC_CONFIG_IOS>
    $<$<PLATFORM_ID:Android>:GPAC_CONFIG_ANDROID>
)

if(UNIX)
  target_link_libraries(gpac INTERFACE m ${CMAKE_DL_LIBS})
elseif(WIN32)
  target_link_libraries(gpac INTERFACE winmm shell32 advapi32)
endif()
