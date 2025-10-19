# SX1255 RF Frontend Interface
# CMakeLists.txt for RF modules

# SX1255 Interface Library
add_library(m17_sx1255
    sx1255_interface.c
)

target_include_directories(m17_sx1255
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link math library for mathematical functions
target_link_libraries(m17_sx1255
    m
)

# Install library
install(TARGETS m17_sx1255
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Install headers
install(FILES
    sx1255_interface.h
    DESTINATION include/m17/rf
)
