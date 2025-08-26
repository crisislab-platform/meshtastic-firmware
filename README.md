# Meshtastic Firmware - CRISiSLab Fork

This is CRISiSLab's fork of the Meshtastic firmware for our Meshtastic monitoring system.

## How to Set Up a Node

### Flashing the Firmware

1. Clone this repo:

    ```
    git clone https://github.com/crisislab-platform/meshtastic-firmware.git
    cd meshtastic-firmware
    ```

2. Install PlatformIO if you haven't already. [See installation instructions here](https://platformio.org/install/cli).

3. Connect the LoRa device to your computer via USB.

4. Build and upload the firmware. Depending on the type of this node (gateway or normal) you will need a different flag before the command:

    For normal nodes (no WiFi):

    ```bash
    PLATFORMIO_BUILD_FLAGS="-DMESHTASTIC_CRISISLAB_NORMAL=1" pio run -t upload
    ```

    For gateway nodes (with WiFi):

    ```bash
    PLATFORMIO_BUILD_FLAGS="-DMESHTASTIC_CRISISLAB_GATEWAY=1" pio run -t upload
    ```

### Configuration

All nodes need to be set on the ANZ region, on the Short Fast modem preset, and have a common channel.

Gateway nodes also require other settings to be configured:

- Network: enable it and set the SSID and password of your WiFi network.
- MQTT:
    - Enable it
    - Set the address of the server the broker is running on (also specify port if necessary, default is 1883)
    - Set the username and password
    - Disable encryption (for now)

We have links which will open the Meshtastic app and set the appropriate settings for you, so use them or contact us.
