# Kernel Driver Experiment

## Overview
This experiment demonstrates how to interact with a simple character device driver (scull0) by writing data to it. The driver consumes system memory dynamically, which provides an opportunity to observe how it impacts RAM usage on your device.

## Usage
To execute the experiment, run the following command:
`dd if=/dev/zero of=/dev/scull0`

This command writes an infinite stream of zeros from /dev/zero to the device file /dev/scull0. As a result, the driver will allocate and consume system memory. Ensure that you actively monitor your system's RAM usage, as the driver will continue consuming available memory until the device runs out of resources, potentially leading to system instability.

Important Notes:
 - Use caution when running this command on systems with limited memory, as it may cause the system to become unresponsive or crash.
 - Always have a monitoring tool (such as htop or free) running to observe memory consumption.
