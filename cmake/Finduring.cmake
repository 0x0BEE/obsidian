include(FindPackageHandleStandardArgs)

find_library(uring_LIBRARY NAMES uring)
find_path(uring_INCLUDE_DIR NAMES liburing.h)

find_package_handle_standard_args(uring
        REQUIRED_VARS uring_LIBRARY uring_INCLUDE_DIR)

if (uring_FOUND)
    mark_as_advanced(uring_LIBRARY)
    mark_as_advanced(uring_INCLUDE_DIR)
endif ()

if (uring_FOUND AND NOT TARGET uring::uring)
    add_library(uring::uring SHARED IMPORTED)
    set_property(TARGET uring::uring PROPERTY IMPORTED_LOCATION ${uring_LIBRARY})
    target_include_directories(uring::uring INTERFACE ${uring_INCLUDE_DIR})
endif ()
