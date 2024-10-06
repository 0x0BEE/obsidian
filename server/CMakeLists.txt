add_executable(obsidian
        "src/main.c")

set_target_properties(obsidian PROPERTIES
        # This project is written in C17
        C_STANDARD 17
        C_STANDARD_REQUIRED ON
        # We make use of GNU compiler extensions
        C_EXTENSIONS ON)

target_include_directories(obsidian
        PRIVATE "include")
