include_directories(include)

set(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/lib)
add_library(oscpack STATIC src/oscpack/oscpack.c)