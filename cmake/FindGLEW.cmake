if(NOT PREFER_BUNDLED_LIBS)
  set(CMAKE_MODULE_PATH ${ORIGINAL_CMAKE_MODULE_PATH})
  find_package(GLEW)
  set(CMAKE_MODULE_PATH ${OWN_CMAKE_MODULE_PATH})
  if(GLEW_FOUND)
    set(GLEW_BUNDLED OFF)
    set(GLEW_DEP)
  endif()
endif()

if(NOT GLEW_FOUND)
  set(GLEW_BUNDLED ON)
  set(GLEW_ROOT "${CMAKE_SOURCE_DIR}/other/glew")
  set(GLEW_INCLUDEDIR "${GLEW_ROOT}/include")
  set(GLEW_SRC_FILE "${GLEW_ROOT}/src/glew.c")

  if(NOT EXISTS "${GLEW_SRC_FILE}")
    message(FATAL_ERROR
      "Bundled GLEW source missing: ${GLEW_SRC_FILE}\n"
      "Unpack GLEW 2.2.0 src/glew.c there, or install system GLEW and use -DPREFER_BUNDLED_LIBS=OFF")
  endif()

  add_library(glew EXCLUDE_FROM_ALL OBJECT "${GLEW_SRC_FILE}")
  target_include_directories(glew PRIVATE ${GLEW_INCLUDEDIR})
  target_compile_definitions(glew PRIVATE GLEW_STATIC)

  set(GLEW_DEP $<TARGET_OBJECTS:glew>)
  set(GLEW_INCLUDE_DIRS ${GLEW_INCLUDEDIR})
  set(GLEW_LIBRARIES)

  list(APPEND TARGETS_DEP glew)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(GLEW DEFAULT_MSG GLEW_INCLUDEDIR GLEW_SRC_FILE)
endif()
