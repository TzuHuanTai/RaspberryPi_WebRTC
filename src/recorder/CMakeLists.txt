project(recorder)

set(FFMPEG_LINK_LIBS avformat avcodec swscale avutil)

aux_source_directory(${PROJECT_SOURCE_DIR} RECORDER_FILES)

add_library(${PROJECT_NAME} ${RECORDER_FILES})

target_link_libraries(${PROJECT_NAME} PUBLIC capture v4l2_codecs ${FFMPEG_LINK_LIBS})
