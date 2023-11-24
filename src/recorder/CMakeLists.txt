project(recorder)

aux_source_directory(${PROJECT_SOURCE_DIR} RECORDER_FILES)

add_library(${PROJECT_NAME} ${RECORDER_FILES})

target_link_libraries(${PROJECT_NAME} PUBLIC common capture)
