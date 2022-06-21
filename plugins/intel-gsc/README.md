# Intel Graphics System Controller

## Introduction

This plugin is used to update the Intel graphics system controller via the Intel Management Engine.

## Firmware Format

There are two firmware formats in use:

* `$FPT` with children `FuIfwiFptFirmware`, where the `FW_DATA_IMAGE` is a `FuIfwiCpdFirmware`
* A linear array of `FuOpromFirmware` images, each with a `FuIfwiCpdFirmware`

This plugin supports the following protocol ID:

* com.intel.gsc

## GUID Generation

These devices use the standard PCI DeviceInstanceId values, e.g.

* `PCI\VID_8086&DEV_4905`

They also define custom per-part PCI IDs such as:

* `PCI\VID_8086&DEV_4905&PART_FWCODE`
* `PCI\VID_8086&DEV_4905&PART_FWDATA`
* `PCI\VID_8086&DEV_4905&PART_OPROMCODE`
* `PCI\VID_8086&DEV_4905&PART_OPROMDATA`

## Vendor ID Security

The vendor ID is set from the PCI vendor, in this instance set to `PCI:0x8086`

## External Interface Access

This plugin requires read/write access to `/dev/mei0`.
