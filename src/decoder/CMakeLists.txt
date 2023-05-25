project(decoder)

aux_source_directory(${PROJECT_SOURCE_DIR} DECODER_FILES)

add_library(${PROJECT_NAME} ${DECODER_FILES})

target_link_libraries(${PROJECT_NAME} PUBLIC common)
