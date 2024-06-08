
# ESPHome B2500 Config Generator

## Overview

This project is a React-based application that generates ESPHome configuration files for B2500 devices. It provides an intuitive form for users to fill out, which then creates a YAML configuration file. Users can download this configuration file or trigger a build process that generates a firmware image, which can be downloaded and uploaded to an ESP32 device.

## Features

- Generate ESPHome configuration files using a user-friendly form.
- Download the generated YAML configuration file.
- Trigger a build process to create a firmware image without having to install ESPHome.

## Usage

### Creating a Configuration File

1. Visit https://tomquist.github.io/esphome-b2500/
2. Fill out the form with your desired configuration settings. The default values are provided for convenience.
3. Once you have filled out the form, the configuration will be automatically generated and displayed.
4. You can download the YAML configuration file by clicking the "Download YAML" button.

### Triggering a Build

1. Click the "Build Firmware" button.
2. A modal will appear asking for a build identifier and a password.
   - The identifier is a unique name for your build, which helps in tracking it.
   - A password will be auto-generated for you, ensuring it meets the required length of at least 8 characters.
3. Optionally, you can change the auto-generated password.
4. Click the "Submit" button to trigger the build process. The submit button will display a loading indicator while the build is in progress.
5. A new tab will open, directing you to the GitHub Actions page where you can track the build progress.

### Downloading the Firmware Image

1. Once the build process is complete, go to the GitHub Actions page: [GitHub Actions](https://github.com/tomquist/esphome-b2500/actions/workflows/build-esphome.yml).
2. Find the build corresponding to your identifier.
3. Download the generated firmware image (ZIP file).

### Uploading the Firmware to an ESP32 using ESPHome Web Installer

1. Extract the firmware binary from the downloaded ZIP file.
2. Go to the ESPHome Web Installer: [ESPHome Web Installer](https://web.esphome.io/).
3. Click the "Install" button.
4. Select "Choose File" and upload the extracted firmware binary.
5. Follow the on-screen instructions to select the connected ESP32 device and flash the firmware.

## Acknowledgments

- Thanks to [@noone2k](https://github.com/noone2k) for reverse engineeing the bluetooth protocol and creating the [initial ESPHome config](https://github.com/noone2k/hm2500pub)
- Thanks to the [ESPHome](https://esphome.io/) community for their excellent documentation and tools.
- Thanks to [Material-UI](https://mui.com/) for providing the UI components used in this project.
