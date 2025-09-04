cmake_minimum_required(VERSION 3.10)

set(CMAKE_USE_RELATIVE_PATHS ON)

project(CoD2x LANGUAGES C CXX)


# -----------------------------------
# Function that collects all files with given extensions from a base directory (recursively)
# -----------------------------------
function(find_files_by_extension OUT_LIST BASE_DIR)
  set(full_base "${CMAKE_SOURCE_DIR}/${BASE_DIR}")
  if (EXISTS "${full_base}")
    set(local "")
    foreach(ext IN LISTS ARGN)
      file(GLOB_RECURSE found "${full_base}/*.${ext}")
      foreach(f ${found})
        file(RELATIVE_PATH relf "${CMAKE_SOURCE_DIR}" "${f}")
        list(APPEND local "${relf}")
      endforeach()
    endforeach()
    set(${OUT_LIST} ${local} PARENT_SCOPE)
  else()
    message(WARNING "Directory not found: ${full_base}")
    set(${OUT_LIST} "" PARENT_SCOPE)
  endif()
endfunction()

# Shared sources (C/C++)
find_files_by_extension(SHARED_SRC "src/shared" c cpp)

# -----------------------------------
# Windows: mss32.dll (C/C++ + NASM + RC)
# -----------------------------------
if (WIN32)
  enable_language(ASM_NASM)
  enable_language(RC)

  find_files_by_extension(MSS32_CC  "src/mss32" c cpp asm rc def)

  add_library(mss32 SHARED
    ${MSS32_CC}
    ${SHARED_SRC}
  )

  set_target_properties(mss32 PROPERTIES
    PREFIX ""
    OUTPUT_NAME "mss32.build"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/windows"
  )

  # Per-language compile options, with DEBUG=1 defined in Debug mode
  target_compile_options(mss32 PRIVATE
    $<$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>:
        -Wall -Wextra -Wno-unused-parameter -Wno-address-of-packed-member
        -g -m32 -msse -O0 -DCJSON_CDECL=__cdecl
        -fdiagnostics-color=always
        $<$<CONFIG:Debug>:-DDEBUG=1>
    >
    $<$<COMPILE_LANGUAGE:ASM_NASM>:
        -f win32
    >
  )

  target_link_libraries(mss32 PRIVATE
    kernel32 wininet dbghelp ole32 oleaut32 uuid gdi32
    -static-libgcc -static-libstdc++ stdc++
  )

  # Optional strip after link (for release packaging)
  if (CMAKE_BUILD_TYPE STREQUAL "Release")
    if (NOT CMAKE_STRIP)
      find_program(CMAKE_STRIP NAMES strip)
    endif()
    if (CMAKE_STRIP)
      add_custom_command(TARGET mss32 POST_BUILD
        COMMAND echo Stripping mss32.dll...
        COMMAND "${CMAKE_STRIP}" "$<TARGET_FILE:mss32>"
        COMMENT "Stripping mss32.dll"
        VERBATIM)
    else()
      message(WARNING "STRIP_MSS32=ON but 'strip' not found in PATH.")
    endif()
  endif()

endif()

# -----------------------------------
# Linux: linux.so
# -----------------------------------
if (UNIX AND NOT WIN32)

  find_files_by_extension(LNX_CC "src/linux" c cpp)

  add_library(linux SHARED
    ${LNX_CC}
    ${SHARED_SRC}
  )

  message(STATUS "Found Linux source files: ${LNX_CC}")

  set_target_properties(linux PROPERTIES
    PREFIX ""
    OUTPUT_NAME "libCoD2x"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/linux"
  )

  target_include_directories(linux PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/shared"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/linux"
  )

  target_compile_options(linux PRIVATE
    -Wall -Wextra -Wno-unused-parameter
    -g -m32 -msse -O0 -fPIC
    -fdiagnostics-color=always
    $<$<CONFIG:Debug>:-DDEBUG=1>
  )

  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m32 -shared -static-libgcc -static-libstdc++")

  target_link_libraries(linux PRIVATE
    -ldl -pthread
  )

endif()