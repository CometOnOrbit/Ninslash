if(NOT PREFER_BUNDLED_LIBS)
  set(CMAKE_MODULE_PATH ${ORIGINAL_CMAKE_MODULE_PATH})
  find_package(GLEW)
  set(CMAKE_MODULE_PATH ${OWN_CMAKE_MODULE_PATH})
  if(GLEW_FOUND)
    set(GLEW_BUNDLED OFF)
    unset(GLEW_DEP)
    if(NOT GLEW_LIBRARIES)
      set(GLEW_LIBRARIES GLEW::GLEW)
    endif()
  endif()
endif()

if(NOT GLEW_FOUND)
  set(GLEW_BUNDLED ON)
  set(GLEW_INCLUDEDIR ${PROJECT_SOURCE_DIR}/other/glew/include)
  set(GLEW_INCLUDE_DIRS ${GLEW_INCLUDEDIR})
  set(GLEW_COPY_FILES)

  set_extra_dirs_lib(GLEW glew)
  if(TARGET_OS STREQUAL "windows")
    set(GLEW_LIB_NAMES glew32s glew32)
  else()
    set(GLEW_LIB_NAMES GLEW glew)
  endif()

  find_library(GLEW_LIBRARY
    NAMES ${GLEW_LIB_NAMES}
    HINTS ${HINTS_GLEW_LIBDIR}
    PATHS ${PATHS_GLEW_LIBDIR}
    NO_DEFAULT_PATH
  )

  if(NOT GLEW_LIBRARY)
    find_library(GLEW_LIBRARY NAMES ${GLEW_LIB_NAMES})
  endif()

  if(GLEW_LIBRARY)
    # MSVC-built glew32*.lib does not link with MinGW; compile from source instead.
    if(MINGW)
      unset(GLEW_LIBRARY CACHE)
      set(GLEW_LIBRARY)
    endif()
  endif()

  if(GLEW_LIBRARY)
    set(GLEW_LIBRARIES ${GLEW_LIBRARY})
    unset(GLEW_DEP)
    is_bundled(GLEW_BUNDLED "${GLEW_LIBRARY}")
    if(GLEW_BUNDLED AND TARGET_OS STREQUAL "windows")
      get_filename_component(GLEW_LIBNAME "${GLEW_LIBRARY}" NAME)
      if(GLEW_LIBNAME STREQUAL "glew32.lib")
        set(GLEW_COPY_FILES "${EXTRA_GLEW_LIBDIR}/glew32.dll")
      endif()
    endif()
  else()
    set(GLEW_SRC_DIR ${PROJECT_SOURCE_DIR}/other/glew/src)
    set_src(GLEW_SRC GLOB ${GLEW_SRC_DIR} glew.c)
    set_src(GLEW_INCLUDES GLOB ${GLEW_INCLUDEDIR}/GL glew.h glxew.h wglew.h)
    add_library(glew EXCLUDE_FROM_ALL OBJECT ${GLEW_SRC} ${GLEW_INCLUDES})
    target_include_directories(glew PRIVATE ${GLEW_INCLUDEDIR})
    target_compile_definitions(glew PRIVATE GLEW_STATIC)

    set(GLEW_DEP $<TARGET_OBJECTS:glew>)
    set(GLEW_LIBRARIES)
    list(APPEND TARGETS_DEP glew)
  endif()

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(GLEW DEFAULT_MSG GLEW_INCLUDEDIR)
endif()
