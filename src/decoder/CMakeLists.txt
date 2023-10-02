project(decoder)

aux_source_directory(${CMAKE_SOURCE_DIR}/src/encoder ENCODER_SOURCES)
aux_source_directory(${PROJECT_SOURCE_DIR} DECODER_FILES)

add_library(${PROJECT_NAME} ${DECODER_FILES} ${ENCODER_SOURCES})

target_link_libraries(${PROJECT_NAME} PUBLIC common)
