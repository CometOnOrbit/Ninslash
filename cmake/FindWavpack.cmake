#
# Bundled WavPack in src/engine/external/wavpack is the Teeworlds-era fork: sound.cpp passes a
# read_stream callback to WavpackOpenFileInput(read_stream infile, char *error).
# Upstream distro libwavpack uses the same symbol name but a path-based ABI
# (open by filename plus flags); linking it silently breaks every WV load ("can't open file").
# Bam always compiles these sources; CMake must do the same unless explicitly overridden.
#
option(NINSLASH_USE_SYSTEM_WAVPACK
  "Link system libwavpack (upstream API incompatible with current sound loader; not recommended)"
  OFF)
mark_as_advanced(NINSLASH_USE_SYSTEM_WAVPACK)

if(NINSLASH_USE_SYSTEM_WAVPACK AND NOT PREFER_BUNDLED_LIBS)
  if(NOT CMAKE_CROSSCOMPILING)
    find_package(PkgConfig QUIET)
    pkg_check_modules(PC_WAVPACK wavpack)
  endif()

  find_library(WAVPACK_LIBRARY
    NAMES wavpack
    HINTS ${PC_WAVPACK_LIBDIR} ${PC_WAVPACK_LIBRARY_DIRS}
    ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
  )
  find_path(WAVPACK_INCLUDEDIR
    NAMES wavpack.h
    PATH_SUFFIXES wavpack
    HINTS ${PC_WAVPACK_INCLUDEDIR} ${PC_WAVPACK_INCLUDE_DIRS}
    ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
  )

  mark_as_advanced(WAVPACK_LIBRARY WAVPACK_INCLUDEDIR)

  if(WAVPACK_LIBRARY AND WAVPACK_INCLUDEDIR)
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(Wavpack DEFAULT_MSG WAVPACK_LIBRARY WAVPACK_INCLUDEDIR)

    set(WAVPACK_LIBRARIES ${WAVPACK_LIBRARY})
    set(WAVPACK_INCLUDE_DIRS ${WAVPACK_INCLUDEDIR})
    set(WAVPACK_BUNDLED OFF)
  endif()
endif()

if(NOT WAVPACK_FOUND)
  set(WAVPACK_SRC_DIR src/engine/external/wavpack)
  set_src(WAVPACK_SRC GLOB ${WAVPACK_SRC_DIR}
    bits.c
    float.c
    metadata.c
    unpack.c
    wavpack.h
    words.c
    wputils.c
  )
  add_library(wavpack EXCLUDE_FROM_ALL OBJECT ${WAVPACK_SRC})
  set(WAVPACK_DEP $<TARGET_OBJECTS:wavpack>)
  set(WAVPACK_INCLUDEDIR ${WAVPACK_SRC_DIR})
  set(WAVPACK_INCLUDE_DIRS ${WAVPACK_INCLUDEDIR})

  list(APPEND TARGETS_DEP wavpack)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(Wavpack DEFAULT_MSG WAVPACK_INCLUDEDIR)
  set(WAVPACK_BUNDLED ON)
endif()
