# Dual-Mode Radio Controller
# CMakeLists.txt for controller modules

# Controller Library
add_library(m17_controller
    dual_mode_controller.c
)

target_include_directories(m17_controller
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../bridge
)

# Link required libraries
target_link_libraries(m17_controller
    m17_bridge
    m17_tnc
)

# Install library
install(TARGETS m17_controller
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Install headers
install(FILES
    dual_mode_controller.h
    DESTINATION include/m17/controller
)
