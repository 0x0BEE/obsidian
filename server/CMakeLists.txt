add_executable(obsidian
        main.cpp
        io/file_descriptor.cpp
        io/file_descriptor.hpp)

set_target_properties(obsidian PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)

target_include_directories(obsidian
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
