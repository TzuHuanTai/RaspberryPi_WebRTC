project(capture)

find_package(PkgConfig)
pkg_check_modules(LIBCAMERA REQUIRED IMPORTED_TARGET libcamera)
message(STATUS "libcamera library found:")
message(STATUS "    version: ${LIBCAMERA_VERSION}")
message(STATUS "    libraries: ${LIBCAMERA_LINK_LIBRARIES}")
message(STATUS "    include path: ${LIBCAMERA_INCLUDE_DIRS}")

aux_source_directory(${PROJECT_SOURCE_DIR} CAPTURE_FILES)

add_library(${PROJECT_NAME} ${CAPTURE_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC ${LIBCAMERA_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC common ${WEBRTC_LIBRARY} pulse-simple pulse ${LIBCAMERA_LINK_LIBRARIES})
