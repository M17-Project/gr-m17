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

# FX.25 Protocol Library
add_library(m17_fx25
    fx25_protocol.c
)

target_include_directories(m17_fx25
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# IL2P Protocol Library
add_library(m17_il2p
    il2p_protocol.c
)

target_include_directories(m17_il2p
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Combined TNC Library
add_library(m17_tnc
    kiss_protocol.c
    ax25_protocol.c
    fx25_protocol.c
    il2p_protocol.c
)

target_include_directories(m17_tnc
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(m17_tnc
    m17_kiss
    m17_ax25
    m17_fx25
    m17_il2p
)

# Install libraries
install(TARGETS m17_kiss m17_ax25 m17_fx25 m17_il2p m17_tnc
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Install headers
install(FILES
    kiss_protocol.h
    ax25_protocol.h
    fx25_protocol.h
    il2p_protocol.h
    DESTINATION include/m17/tnc
)
