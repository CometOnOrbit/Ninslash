if(NOT PREFER_BUNDLED_LIBS)
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

    # The decoder embedded by this engine uses WavPack 4's read_stream API.
    # Modern system WavPack libraries export a different function with the
    # same C symbol name, so merely finding and linking the library succeeds
    # but every sound fails at runtime due to an ABI mismatch.
    include(CheckCSourceCompiles)
    set(_WAVPACK_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES})
    set(_WAVPACK_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
    set(CMAKE_REQUIRED_INCLUDES ${WAVPACK_INCLUDEDIR})
    set(CMAKE_REQUIRED_LIBRARIES ${WAVPACK_LIBRARY})
    check_c_source_compiles("\
#include <wavpack.h>\n\
static int read_callback(void *buffer, int size) { (void)buffer; return size; }\n\
int main(void) { char error[80]; return WavpackOpenFileInput(read_callback, error) == 0; }\n"
      WAVPACK_HAS_LEGACY_STREAM_API)
    set(CMAKE_REQUIRED_INCLUDES ${_WAVPACK_REQUIRED_INCLUDES})
    set(CMAKE_REQUIRED_LIBRARIES ${_WAVPACK_REQUIRED_LIBRARIES})
    if(WAVPACK_HAS_LEGACY_STREAM_API)
      set(WAVPACK_LIBRARIES ${WAVPACK_LIBRARY})
      set(WAVPACK_INCLUDE_DIRS ${WAVPACK_INCLUDEDIR})
      set(WAVPACK_BUNDLED OFF)
    else()
      message(STATUS "System WavPack API is incompatible with the engine decoder; using bundled version")
      set(WAVPACK_FOUND FALSE)
      set(Wavpack_FOUND FALSE)
      unset(WAVPACK_LIBRARIES)
    endif()
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
