# KISS Protocol and AX.25 TNC Implementation
# CMakeLists.txt for TNC modules

# KISS Protocol Library
add_library(m17_kiss
    kiss_protocol.c
)

target_include_directories(m17_kiss
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# AX.25 Protocol Library
add_library(m17_ax25
    ax25_protocol.c
)

target_include_directories(m17_ax25
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Combined TNC Library
add_library(m17_tnc
    kiss_protocol.c
    ax25_protocol.c
)

target_include_directories(m17_tnc
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(m17_tnc
    m17_kiss
    m17_ax25
)

# Install libraries
install(TARGETS m17_kiss m17_ax25 m17_tnc
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Install headers
install(FILES
    kiss_protocol.h
    ax25_protocol.h
    DESTINATION include/m17/tnc
)
