cmake_minimum_required(VERSION 3.22)
project(lemus808_voice_engine LANGUAGES CXX)
find_package(Threads REQUIRED)

set(board "${CMAKE_CURRENT_SOURCE_DIR}/machines/tr808")
set(generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(header "${generated}/tr808/netlists.h")
set(generator "${board}/tools/cmake/netlists.cmake")
file(GLOB netlists CONFIGURE_DEPENDS "${board}/core/netlists/*.cir")
foreach(netlist IN LISTS netlists)
  get_filename_component(name "${netlist}" NAME_WE)
  list(APPEND names "${name}")
endforeach()
list(JOIN names "," names_arg)
add_custom_command(
  OUTPUT "${header}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${generated}/tr808"
  COMMAND "${CMAKE_COMMAND}" "-DDIR=${board}/core/netlists" "-DOUT=${header}"
          "-DNAMES=${names_arg}" -P "${generator}"
  DEPENDS ${netlists} "${generator}"
  VERBATIM
)

add_library(tr808_voice_engine STATIC
  "${board}/core/circuit.cpp"
  "${board}/core/voiceboard.cpp"
  "${board}/core/parallel.cpp"
  "${board}/core/oversampler.cpp"
  "${header}"
)
target_compile_features(tr808_voice_engine PUBLIC cxx_std_17)
target_include_directories(tr808_voice_engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/machines" PRIVATE "${generated}")
target_link_libraries(tr808_voice_engine PUBLIC Threads::Threads)
set_target_properties(tr808_voice_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set_source_files_properties("${board}/core/circuit.cpp" PROPERTIES
    COMPILE_OPTIONS "-fno-builtin-memcpy;-fno-builtin-memmove")
endif()

option(TR808_ENGINE_TOOLS "Build voicerun and circuitrun" ON)
if(TR808_ENGINE_TOOLS)
  foreach(tool IN ITEMS voicerun circuitrun)
    add_executable(${tool} "${board}/tools/${tool}/main.cpp")
    target_link_libraries(${tool} PRIVATE tr808_voice_engine)
  endforeach()
endif()
