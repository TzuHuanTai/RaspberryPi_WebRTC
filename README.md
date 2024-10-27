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

Turn your Raspberry Pi into a low-latency home security camera using the V4L2 DMA hardware encoder and WebRTC. [[demo video](https://www.youtube.com/watch?v=JZ5bcSAsXog)]

- Pure P2P-based camera allows video playback and download without a media server.
- Support [multiple users](doc/pi_4b_users_demo.gif) for simultaneous live streaming.

# How to use

To set up the environment, please check out the [tutorial video](https://youtu.be/g5Npb6DsO-0) or the steps below.

* Download and run the binary file from [Releases](https://github.com/TzuHuanTai/RaspberryPi_WebRTC/releases).
* Set up the network configuration and create a new client using one of the following options:
    * [Pi Camera](https://github.com/TzuHuanTai/Pi-Camera) app (Android).
    * [Pi Camera Web](https://picamera.live).

## Hardware Requirements

<img src="https://assets.raspberrypi.com/static/51035ec4c2f8f630b3d26c32e90c93f1/2b8d7/zero2-hero.webp" height="96">

* Raspberry Pi (Zero 2W/3/3B+/4B/5).
* CSI or USB Camera Module.

## Environment Setup

1. Install Raspberry Pi OS

    Use the [Raspberry Pi Imager](https://www.raspberrypi.com/software/) to install Raspberry Pi Lite OS on your microSD card.
> [!TIP]
> **Can I use a regular Raspberry Pi OS, or does it have to be Lite?**<br/>
> You can use either the Lite or full Raspberry Pi OS (the official recommended versions), but Lite OS is generally more efficient.

2. Install essential libraries
    ```bash
    sudo apt install libmosquitto1 pulseaudio libavformat59 libswscale6
    ```
3. Download and unzip the binary file
    ```bash
    wget https://github.com/TzuHuanTai/RaspberryPi_WebRTC/releases/download/v1.0.2/pi_webrtc-1.0.2_pi-os-bookworm.tar.gz
    tar -xzf pi_webrtc-1.0.2_pi-os-bookworm.tar.gz
    ```

## Running the Application

### MQTT Setup

An MQTT server is required for communication between devices. For remote access, free cloud options include [HiveMQ](https://www.hivemq.com) and [EMQX](https://www.emqx.com/en).

> [!TIP]
> **Is MQTT registration necessary, and why is MQTT needed?**<br/>
> Yes, MQTT is required for signaling P2P connection info between your camera and the client UI.
If you choose to self-host an MQTT server (e.g., [Mosquitto](doc/SETUP_MOSQUITTO.md)) and need to access the signaling server remotely via mobile data, you may need to set up DDNS, port forwarding, and SSL/TLS.

### Start the Application

* Set up the MQTT settings on your [Pi Camera App](https://github.com/TzuHuanTai/Pi-Camera) or [Pi Camera Web](https://picamera.live), and create a new device in the settings to get a `UID`. 
* Run the command based on your network settings and `UID`:
    ```bash
    ./pi_webrtc --use_libcamera --fps=30 --width=1280 --height=960 --hw_accel --no_audio --mqtt_host=your.mqtt.cloud --mqtt_port=8883 --mqtt_username=hakunamatata --mqtt_password=Wonderful --uid=your-custom-uid
    ```

> [!IMPORTANT]
> For Raspberry Pi 5 or other SBCs without hardware encoding support, run this command in software encoding mode by removing the `--hw_accel` flag.
* Go to the Live page to enjoy real-time streaming!

<p align=center>
    <img src="doc/web_live_demo.jpg" alt="Pi 5 live demo on web">
</p>

# Advance

For the complete user manual, please refer to the [wiki page](https://github.com/TzuHuanTai/RaspberryPi_WebRTC/wiki).

## Using the Legacy V4L2 Driver

* For Libcamera users (e.g., Camera Module 3), skip this step and add `--use_libcamera` to your command.
* For V4L2 users, modify `/boot/firmware/config.txt` to enable the legacy driver:
    ```ini
    # camera_auto_detect=1  # Default setting
    camera_auto_detect=0    # Read camera by v4l2
    start_x=1               # Enable hardware-accelerated
    gpu_mem=128             # Adjust based on resolution; use 256MB for 1080p and higher.
    ```

> [!TIP]
> **How do I know if I should choose V4L2 or Libcamera for my camera?**<br/>
> V4L2 is typically used with older cameras that don’t require specific drivers. Libcamera supports newer official Raspberry Pi Camera Modules, like Camera Module 3. If you are unsure, start with **Libcamera**.

Here is a example command for V4L2 camera:
```bash
./pi_webrtc --device=/dev/video0 --v4l2_format=h264 --fps=30 --width=1280 --height=960 --hw_accel --no_audio --mqtt_host=your.mqtt.cloud --mqtt_port=8883 --mqtt_username=hakunamatata --mqtt_password=Wonderful --uid=your-custom-uid
```

> [!CAUTION]
> When setting 1920x1080 with the legacy V4L2 driver, the hardware decoder firmware may adjust it to 1920x1088, while the ISP/encoder remains at 1920x1080 on the 6.6.31 kernel. This may cause memory out-of-range issues. Setting 1920x1088 resolves this issue.

## Run as Linux Service

### 1. Run `pulseaudio` as system-wide daemon [[ref]](https://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/User/SystemWide/):

* Create a service file `/etc/systemd/system/pulseaudio.service`
    ```bash
    sudo nano /etc/systemd/system/pulseaudio.service
    ```
* Copy the following content:
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
* Run the command to add a `autospawn = no` in the client config
    ```bash
    echo 'autospawn = no' | sudo tee -a /etc/pulse/client.conf > /dev/null
    ```
* Add root to the pulse group
    ```bash
    sudo adduser root pulse-access
    ```
* Enable and start the Service
    ```bash
    sudo systemctl daemon-reload
    sudo systemctl enable pulseaudio.service
    sudo systemctl start pulseaudio.service
    ```

### 2. In order to run `pi_webrtc` and ensure it starts automatically on reboot:

* Create a service file `/etc/systemd/system/pi-webrtc.service`
    ```bash
    sudo nano /etc/systemd/system/pi-webrtc.service
    ```
* Modify `WorkingDirectory` and `ExecStart` to your settings:
    ```ini
    [Unit]
    Description= The p2p camera via webrtc.
    After=network-online.target pulseaudio.service

    [Service]
    Type=simple
    WorkingDirectory=/path/to
    ExecStart=/path/to/pi_webrtc --use_libcamera --fps=30 --width=1280 --height=960 --hw_accel --mqtt_host=example.s1.eu.hivemq.cloud --mqtt_port=8883 --mqtt_username=hakunamatata --mqtt_password=wonderful
    Restart=always
    RestartSec=10

    [Install]
    WantedBy=multi-user.target
    ```
* Enable and start the Service
    ```bash
    sudo systemctl daemon-reload
    sudo systemctl enable pi-webrtc.service
    sudo systemctl start pi-webrtc.service
    ```

## Recording

* To mount a USB disk at `/mnt/ext_disk` when detected [[ref]](https://wiki.gentoo.org/wiki/AutoFS):
    ```bash
    sudo apt-get install autofs
    echo '/- /etc/auto.usb --timeout=5' | sudo tee -a /etc/auto.master > /dev/null
    echo '/mnt/ext_disk -fstype=auto,nofail,nodev,nosuid,noatime,umask=000 :/dev/sda1' | sudo tee -a /etc/auto.usb > /dev/null
    sudo systemctl restart autofs
    ```
* Add `--record_path` with the path under the disk behind the command.
    ```bash
    /path/to/pi_webrtc ... --record_path=/mnt/ext_disk/video
    ```

## Two-way communication

a microphone and speaker need to be added to the Pi. It's easier to plug in a USB mic/speaker. If you want to use GPIO, please follow the link below.

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
