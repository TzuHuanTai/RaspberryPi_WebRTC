# RaspberryPi_WebRTC
 
Turn your Raspberry Pi into a home security camera using the v4l2 DMA hardware encoder and WebRTC.

Raspberry Pi 5 and other SBCs do not support v4l2 hardware encoding, please run this project in software encoding mode.

# How to use

* Download the latest binary file from [Releases](https://github.com/TzuHuanTai/RaspberryPi_WebRTC/releases).
* Install the [Pi Camera](https://github.com/TzuHuanTai/Pi-Camera) app and follow the instructions.

## Hardware Requirements

<img src="https://assets.raspberrypi.com/static/51035ec4c2f8f630b3d26c32e90c93f1/2b8d7/zero2-hero.webp" height="96">

* Raspberry Pi [Zero 2W](https://www.raspberrypi.com/products/raspberry-pi-zero-2-w/) (or better).
* CSI Camera Module.
* At least 4GB micro sd card.
* A USB disk and a Micro-USB Male to USB-A Female adaptor.

## Environment Setup

* Use the [Raspberry Pi Imager](https://www.raspberrypi.com/software/) to write the Lite OS (Bookworm 64-bit) to the micro SD card.
* Install essential libs
    ```bash
    sudo apt install libmosquitto1 pulseaudio libavformat59 libswscale6
    pulseaudio --start
    ```

* Enable Raspberry Pi Hardware by adding below in `/boot/firmware/config.txt`
    ```text
    camera_auto_detect=0
    start_x=1
    gpu_mem=16
    ```
    Set `camera_auto_detect=0` in order to read camera by v4l2.

* Mount USB disk [[ref]](https://wiki.gentoo.org/wiki/AutoFS)

    If the disk drive is detected, it will automatically mount to `/mnt/ext_disk`.
    ```bash
    sudo apt-get install autofs
    echo '/- /etc/auto.usb --timeout=5' | sudo tee -a /etc/auto.master > /dev/null
    echo '/mnt/ext_disk -fstype=auto,nofail,nodev,nosuid,noatime,umask=000 :/dev/sda1' | sudo tee -a /etc/auto.usb > /dev/null
    sudo systemctl restart autofs
    ```

## Running the Application

Running `pi_webrtc -h` will display all available options. To start the application, use the following command:

```bash
/path/to/pi_webrtc --device=/dev/video0 --fps=30 --width=1280 --height=960 --v4l2_format=h264 --hw_accel --mqtt_host=example.s1.eu.hivemq.cloud --mqtt_port=8883 --mqtt_username=hakunamatata --mqtt_password=Wonderful --uid=home-pi-zero2w --record_path=/mnt/ext_disk/video/
```
**For Pi 5**, remove the `--hw_accel` option and set `--v4l2_format` to `mjpeg`. Video encoding will be handled by [OpenH264](https://github.com/cisco/openh264).

### Run as Linux Service

In order to run `pi_webrtc` and ensure it starts automatically on reboot:
* Create a service file `/etc/systemd/system/pi-webrtc.service` with the following content:
    ```ini
    [Unit]
    Description= The p2p camera via webrtc.
    After=systemd-networkd.service

    [Service]
    Type=simple
    WorkingDirectory=/path/to
    ExecStart=/path/to/pi_webrtc --fps=30 --width=1280 --height=960 --v4l2_format=h264 --hw_accel --mqtt_host=example.s1.eu.hivemq.cloud --mqtt_port=8883 --mqtt_username=hakunamatata --mqtt_password=wonderful --record_path=/mnt/ext_disk/video/
    ExecStop=/bin/kill -s SIGTERM $MAINPID
    Restart=always
    RestartSec=10
      
    [Install]
    WantedBy=multi-user.target
    ```
* Enable and Start the Service
    ```bash
    sudo systemctl enable pi-webrtc.service
    sudo systemctl start pi-webrtc.service
    ```

## Advance

To enable two-way communication, a microphone and speaker need to be added to the Pi.

### Microphone

Please see this [link](https://learn.adafruit.com/adafruit-i2s-mems-microphone-breakout/raspberry-pi-wiring-test) for instructions on wiring and testing your Pi.

### Speaker

You can use the [link](https://learn.adafruit.com/adafruit-max98357-i2s-class-d-mono-amp/raspberry-pi-wiring) for instructions on setting up a speaker on your Pi.

# License

This project is licensed under the Apache License, Version 2.0. See the [LICENSE](LICENSE) file for details.

```
Copyright 2022 Tzu Huan Tai (Author)

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```
