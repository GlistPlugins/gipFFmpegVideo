#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "OpenAL::OpenAL" for configuration "Release"
set_property(TARGET OpenAL::OpenAL APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(OpenAL::OpenAL PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/libopenal.dll.a"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/libopenal-1.dll"
  )

list(APPEND _IMPORT_CHECK_TARGETS OpenAL::OpenAL )
list(APPEND _IMPORT_CHECK_FILES_FOR_OpenAL::OpenAL "${_IMPORT_PREFIX}/lib/libopenal.dll.a" "${_IMPORT_PREFIX}/bin/libopenal-1.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
