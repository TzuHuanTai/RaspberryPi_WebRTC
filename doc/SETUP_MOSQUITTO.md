# Set up the Mosquitto broker

On the device which acts as the Mosquitto server.

* Install `mosquitto`
    ```bash
    sudo apt install mosquitto mosquitto-clients
    ```

* Configure the broker in `/etc/mosquitto/mosquitto.conf` likes
    ```bash
    pid_file /run/mosquitto/mosquitto.pid

    persistence true
    persistence_location /var/lib/mosquitto/

    log_dest file /var/log/mosquitto/mosquitto.log
    password_file /etc/mosquitto/p1.txt
    include_dir /etc/mosquitto/conf.d

    # this will listen for mqtt on tcp
    listener 1883
    allow_anonymous false

    # this will expect websockets connections
    listener 8083
    protocol websockets
    certfile /etc/letsencrypt/live/abc.com/cert.pem
    cafile /etc/letsencrypt/live/abc.com/chain.pem
    keyfile /etc/letsencrypt/live/abc.com/privkey.pem
    ```

* Restart the service by using the following command
    ```bash
    sudo systemctl restart mosquitto.service
    ```