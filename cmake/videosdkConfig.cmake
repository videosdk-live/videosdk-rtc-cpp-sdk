# find_package(videosdk REQUIRED)
# target_link_libraries(my_app PRIVATE videosdk::cpp)

get_filename_component(_videosdk_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

set(VIDEOSDK_INCLUDE_DIR "${_videosdk_prefix}/include")
set(VIDEOSDK_LIB_DIR     "${_videosdk_prefix}/lib")

find_library(VIDEOSDK_LIBRARY NAMES videosdk PATHS "${VIDEOSDK_LIB_DIR}" NO_DEFAULT_PATH)

if(NOT VIDEOSDK_LIBRARY)
    message(FATAL_ERROR "videosdk: libvideosdk.so not found in ${VIDEOSDK_LIB_DIR}")
endif()

add_library(videosdk::cpp SHARED IMPORTED)
set_target_properties(videosdk::cpp PROPERTIES
    IMPORTED_LOCATION "${VIDEOSDK_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${VIDEOSDK_INCLUDE_DIR}")

set(videosdk_FOUND TRUE)
