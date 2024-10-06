cmake_minimum_required(VERSION 3.20)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(obsidian VERSION 0.1.0
        DESCRIPTION "Obsidian is a fast Minecraft alpha server"
        HOMEPAGE_URL "https://github.com/0x0BEE/obsidian"
        LANGUAGES C)

add_subdirectory("server")
