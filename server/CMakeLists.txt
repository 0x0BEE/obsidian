add_executable(obsidian
        "src/main.c"
        "src/server.c"
        "src/log.c"
        "src/memory/pool_allocator.c"
        "src/memory/ring_buffer.c"
        "src/minecraft/protocol.c"
        "include/obsidian/server.h"
        "include/obsidian/log.h"
        "include/obsidian/memory.h"
        "include/obsidian/minecraft/protocol.h")

set_target_properties(obsidian PROPERTIES
        # This project is written in C17
        C_STANDARD 17
        C_STANDARD_REQUIRED ON
        # We make use of GNU compiler extensions
        C_EXTENSIONS ON)

target_compile_definitions(obsidian
        PRIVATE _GNU_SOURCE)

target_include_directories(obsidian
        PRIVATE "include")

target_link_libraries(obsidian
        PRIVATE uring::uring)
