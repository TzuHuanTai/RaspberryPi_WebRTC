project(scaler)

aux_source_directory(${CMAKE_SOURCE_DIR}/src/encoder ENCODER_SOURCES)
aux_source_directory(${PROJECT_SOURCE_DIR} SCALER_FILES)

add_library(${PROJECT_NAME} ${SCALER_FILES} ${ENCODER_SOURCES})

target_link_libraries(${PROJECT_NAME} PUBLIC common)
