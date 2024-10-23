cmake_minimum_required(VERSION 3.30)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(obsidian VERSION 0.1.0 LANGUAGES CXX
        DESCRIPTION "A fast Minecraft server"
        HOMEPAGE_URL "https://github.com/0x0BEE/obsidian")

add_subdirectory("server")
