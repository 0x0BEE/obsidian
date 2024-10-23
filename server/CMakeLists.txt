add_executable(obsidian
        main.cpp)

set_target_properties(obsidian PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)

target_include_directories(obsidian
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
