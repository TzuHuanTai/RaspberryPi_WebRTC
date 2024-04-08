project(capture)

aux_source_directory(${PROJECT_SOURCE_DIR} CAPTURE_FILES)

add_library(${PROJECT_NAME} ${CAPTURE_FILES})

target_link_libraries(${PROJECT_NAME} PUBLIC common ${WEBRTC_LIBRARY} pulse-simple pulse)
