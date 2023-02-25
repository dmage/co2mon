# Software for CO2 Monitor

## Compatible Devices

This software supports compact USB-powered CO2 meters that identify as:

```
  idVendor           0x04d9 Holtek Semiconductor, Inc.
  idProduct          0xa052 USB-zyTemp
```

A of 2023, there are two revisions of these product on the
market, that can be distinguished by its serial and release
numbers:

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
                    old        new
――――――――――――――――――――――――――――――――――
serial_number      1.40       2.00
release_number   0x0100     0x0200
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

(where serial_number == iSerial and release_number == bcdDevice)

The co2mon software tries to auto-detect them, but its also possible to
override the detection (cf. the `-N` and `-n` options).

Note that these devices are rebranded by vendors such as TFA and
thus are available online under different product names (e.g.
sold by Amazon, as of 2023). Even if one user reported success
with a certain product, the vendor might have silently switched
to a completely different and unsupported hardware without
changing the product name, at any time.

List of user reported devices that worked in the past:

- TFA AIRCO2NTROL MINI CO2 Monitor (EAN 4009816027351), via
  Amazon.de, 2023


## Installation

### Arch Linux
[There is PKGBUILD in AUR](https://aur.archlinux.org/packages/co2mon-git/). The simplest way to [install](https://wiki.archlinux.org/index.php/Arch_User_Repository#Installing_packages) is using yaourt:

`yaourt -S co2mon-git`

### Fedora GNU/Linux and RHEL/CentOS/Scientific Linux
co2mon packages can be installed from official repo:

`dnf install co2mon`

### From Source

    # macOS
    brew install cmake pkg-config hidapi

    # Ubuntu
    apt-get install cmake g++ pkg-config libhidapi-dev

    mkdir build
    cd build
    cmake ..
    make
    ./co2mond/co2mond

## See also

  * [ZyAura ZG01C Module Manual](http://www.zyaura.com/support/manual/pdf/ZyAura_CO2_Monitor_ZG01C_Module_ApplicationNote_141120.pdf)
  * [RevSpace CO2 Meter Hacking](https://revspace.nl/CO2MeterHacking)
  * [Photos of the device and the circuit board](http://habrahabr.ru/company/masterkit/blog/248403/)
