# M17 ↔ AX.25 Protocol Bridge
# CMakeLists.txt for bridge modules

# Bridge Library
add_library(m17_bridge
    m17_ax25_bridge.c
)

target_include_directories(m17_bridge
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../tnc
)

# Link TNC libraries
target_link_libraries(m17_bridge
    m17_tnc
)

# Install library
install(TARGETS m17_bridge
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Install headers
install(FILES
    m17_ax25_bridge.h
    DESTINATION include/m17/bridge
)
