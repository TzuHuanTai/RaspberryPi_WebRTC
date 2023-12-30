project(signaling)

if(USE_MQTT_SIGNALING)
    set(SIGNALING_SOURCE ./mqtt_service.cpp)
elseif(USE_SIGNALR_SIGNALING)
    set(SIGNALING_SOURCE ./signalr_service.cpp)
endif()

add_library(${PROJECT_NAME} 
    ./signaling_service.cpp
    ${SIGNALING_SOURCE})

target_link_libraries(${PROJECT_NAME})
