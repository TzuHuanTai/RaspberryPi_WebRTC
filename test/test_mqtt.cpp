#include "signaling/mqtt_service.h"
#include "args.h"

int main(int argc, char *argv[]) {
    Args args{.mqtt_username = "hakunamatata",
              .mqtt_password = "wonderful"};

    auto mqtt = MqttService::Create(args, nullptr);

    sleep(60);
}
