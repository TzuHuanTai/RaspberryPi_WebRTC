project(signaling)

if(USE_MQTT_SIGNALING)
    set(SIGNALING_SOURCE ./mqtt_service.cpp)
    find_library(MOSQUITTO_LIBS NAMES mosquitto PATHS /usr/local/lib)
    if(NOT MOSQUITTO_LIBS)
        message(FATAL_ERROR "Mosquitto library not found")
    endif()
    set(SIGNALING_LIBS ${MOSQUITTO_LIBS})
endif()

add_library(${PROJECT_NAME} ${SIGNALING_SOURCE})
target_link_libraries(${PROJECT_NAME} PUBLIC ${SIGNALING_LIBS})
