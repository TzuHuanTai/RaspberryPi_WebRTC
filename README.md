<h1 align="center">
    Raspberry Pi WebRTC
</h1>

<p align="center">
    <a href="https://chromium.googlesource.com/external/webrtc/+/branch-heads/5790"><img src="https://img.shields.io/badge/libwebrtc-m115.5790-red.svg" alt="WebRTC Version"></a>
    <img src="https://img.shields.io/github/downloads/TzuHuanTai/RaspberryPi_WebRTC/total.svg?color=yellow" alt="Download">
    <img src="https://img.shields.io/badge/C%2B%2B-20-brightgreen?logo=cplusplus">
    <img src="https://img.shields.io/github/v/release/TzuHuanTai/RaspberryPi_WebRTC?color=blue" alt="Release">
    <a href="https://opensource.org/licenses/Apache-2.0"><img src="https://img.shields.io/badge/License-Apache_2.0-purple.svg" alt="License Apache"></a>
</p>

<p align=center>
    <img src="doc/pi_4b_latency_demo.gif" alt="Pi 4b latency demo">
</p>

Turn your Raspberry Pi into a low-latency home security camera using the v4l2 DMA hardware encoder and WebRTC. [[demo video](https://www.youtube.com/watch?v=JZ5bcSAsXog)]

- It's designed as a pure P2P-based camera that allows video playback and download without needing a media server.
- Support [multiple users](doc/pi_4b_users_demo.gif) to watch the live stream simultaneously. 

- Raspberry Pi 5 or other SBCs do not support v4l2 hardware encoding, please run this project in software encoding mode.

# How to use

For the complete user manual, please refer to the [wiki page](https://github.com/TzuHuanTai/RaspberryPi_WebRTC/wiki). To set up the environment, please refer to the [tutorial video](https://youtu.be/t9aiqFlzkm4).

* Download and run the binary file from [Releases](https://github.com/TzuHuanTai/RaspberryPi_WebRTC/releases).
* Set up network configuration and the `uid` in client side UI.
    * [Pi Camera](https://github.com/TzuHuanTai/Pi-Camera) app (Android).
    * [Pi Camera Web](https://picamera.live)

## Hardware Requirements

<img src="https://assets.raspberrypi.com/static/51035ec4c2f8f630b3d26c32e90c93f1/2b8d7/zero2-hero.webp" height="96">

* Raspberry Pi (Zero 2W/3/3B+/4B/5).
* CSI or USB Camera Module.
* At least 4GB micro sd card.
* A USB disk and a Micro-USB Male to USB-A Female adaptor.

## Environment Setup

1. Use the [Raspberry Pi Imager](https://www.raspberrypi.com/software/) to write the Lite OS (Bookworm 64-bit) to the micro SD card.
2. Install essential libs
    ```bash
    sudo apt install libmosquitto1 pulseaudio libavformat59 libswscale6
    ```

3. Enable Camera on Raspberry Pi

    * If you are using a camera that requires libcamera (e.g., Camera Module 3), you can skip this step.

        By default, the system uses libcamera, and the official Camera Module 3 is only supported by libcamera. Please add `--use_libcamera` to the execution command.

    * If using the legacy v4l2 driver on the Raspberry Pi, please modify the camera settings in `/boot/firmware/config.txt`:
        ```ini
        # camera_auto_detect=1  # Default setting
        camera_auto_detect=0    # Read camera by v4l2
        start_x=1               # Enable hardware-accelerated
        gpu_mem=16              # It's depends on resolutions
        ```

    **Note**: For Raspberry Pi 4B, set gpu_mem=256 to ensure the camera device loads correctly. The required memory size may vary depending on the resolution you're capturing—higher resolutions will need more GPU memory. ([#182](https://github.com/TzuHuanTai/RaspberryPi_WebRTC/issues/182))

4. **[Optional]** Mount USB disk [[ref]](https://wiki.gentoo.org/wiki/AutoFS)

    * Skip this step if you don't want to record videos. Don't set the `record_path` flag while running.
    * The following commands will automatically mount to `/mnt/ext_disk` when the disk drive is detected.
        ```bash
        sudo apt-get install autofs
        echo '/- /etc/auto.usb --timeout=5' | sudo tee -a /etc/auto.master > /dev/null
        echo '/mnt/ext_disk -fstype=auto,nofail,nodev,nosuid,noatime,umask=000 :/dev/sda1' | sudo tee -a /etc/auto.usb > /dev/null
        sudo systemctl restart autofs
        ```

## Running the Application

MQTT is currently the only signaling mechanism used, so ensure that your MQTT server is ready before starting the application. If the application is only intended for use within a local area network (LAN), the MQTT server (such as [Mosquitto](doc/SETUP_MOSQUITTO.md)) can be installed on the same Pi. However, if remote access is required, it is recommended to use a cloud-based MQTT server. Free plans include, but are not limited to, [HiveMQ](https://www.hivemq.com) and [EXMQ](https://www.emqx.com/en). Because If you choose to self-host and need to access the signaling server remotely via mobile data, you may need to set up DDNS and port forwarding if your ISP provides a dynamic IP.

### Run
To start the application, use your network settings and apply them to the following example command. The SDP/ICE data will be transferred under the MQTT topic specified by your `uid` setting.
  * For legacy v4l2 driver
```bash
/path/to/pi_webrtc --device=/dev/video0 --v4l2_format=h264 --fps=30 --width=1280 --height=960 --hw_accel --no_audio --mqtt_host=your.mqtt.cloud --mqtt_port=8883 --mqtt_username=hakunamatata --mqtt_password=Wonderful --uid=your-custom-uid --record_path=/mnt/ext_disk/video/
```
  * For libcamera
```bash
/path/to/pi_webrtc --use_libcamera --fps=30 --width=1280 --height=960 --hw_accel --no_audio --mqtt_host=your.mqtt.cloud --mqtt_port=8883 --mqtt_username=hakunamatata --mqtt_password=Wonderful --uid=your-custom-uid --record_path=/mnt/ext_disk/video/
```

**Hint 1:** Since the Pi 5 does not support hardware encoding, please remove the `--hw_accel` flag and set `--v4l2_format` to `mjpeg`. Video encoding will be handled by built-in software codes.

**Hint 2:** I noticed that when I set `1920x1080` while using lagacy v4l2, the hardware decoder firmware changes it to `1920x1088`, but the isp/encoder does not adjust in the 6.6.31 kernel. This leads to memory being out of range. However, if I set `1920x1088`, all works fine.

### Run as Linux Service

#### 1. **[Optional]** Run `pulseaudio` as system-wide daemon [[ref]](https://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/User/SystemWide/):
* create a file `/etc/systemd/system/pulseaudio.service`
    ```ini
    [Unit]
    Description= Pulseaudio Daemon
    After=rtkit-daemon.service systemd-udevd.service dbus.service

    [Service]
    Type=simple
    ExecStart=/usr/bin/pulseaudio --system --disallow-exit --disallow-module-loading
    Restart=always
    RestartSec=10

    [Install]
    WantedBy=multi-user.target
    ```
* Run the cmd to add a `autospawn = no` in the client conf
    ```bash
    echo 'autospawn = no' | sudo tee -a /etc/pulse/client.conf > /dev/null
    ```
* Add root to pulse group
    ```bash
    sudo adduser root pulse-access
    ```
* Enable and Start the Service
    ```bash
    sudo systemctl daemon-reload
    sudo systemctl enable pulseaudio.service
    sudo systemctl start pulseaudio.service
    ```

#### 2. In order to run `pi_webrtc` and ensure it starts automatically on reboot:
* Create a service file `/etc/systemd/system/pi-webrtc.service` with the following content:
    ```ini
    [Unit]
    Description= The p2p camera via webrtc.
    After=network-online.target pulseaudio.service

    [Service]
    Type=simple
    WorkingDirectory=/path/to
    ExecStart=/path/to/pi_webrtc --device=/dev/video0 --fps=30 --width=1280 --height=960 --v4l2_format=h264 --hw_accel --mqtt_host=example.s1.eu.hivemq.cloud --mqtt_port=8883 --mqtt_username=hakunamatata --mqtt_password=wonderful --record_path=/mnt/ext_disk/video/
    Restart=always
    RestartSec=10

    [Install]
    WantedBy=multi-user.target
    ```
* Enable and Start the Service
    ```bash
    sudo systemctl daemon-reload
    sudo systemctl enable pi-webrtc.service
    sudo systemctl start pi-webrtc.service
    ```

## Advance

To enable two-way communication, a microphone and speaker need to be added to the Pi. It's easier to plug in a USB mic/speaker. If you want to use GPIO, please follow the link below.

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
