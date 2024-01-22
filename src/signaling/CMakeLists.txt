project(signaling)

if(USE_MQTT_SIGNALING)
    set(SIGNALING_SOURCE ./mqtt_service.cpp)
    find_library(MOSQUITTO_LIBS NAMES mosquitto PATHS /usr/local/lib)
    if(NOT MOSQUITTO_LIBS)
        message(FATAL_ERROR "Mosquitto library not found")
    endif()
    set(SIGNALING_LIBS ${MOSQUITTO_LIBS})
elseif(USE_SIGNALR_SIGNALING)
    set(SIGNALING_SOURCE ./signalr_service.cpp)
    set(SIGNALING_LIBS 
        /usr/local/lib/libmicrosoft-signalr.a 
        /usr/local/lib/libcpprest.a 
        /usr/local/lib/libjsoncpp.a 
        -lz)
endif()

add_library(${PROJECT_NAME} 
    ./signaling_service.cpp
    ${SIGNALING_SOURCE})

target_link_libraries(${PROJECT_NAME} PUBLIC ${SIGNALING_LIBS})
