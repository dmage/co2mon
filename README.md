# Software for CO2 Monitor

## Installation

### Arch Linux
[There is PKGBUILD in AUR](https://aur.archlinux.org/packages/co2mon-git/). The simplest way to [install](https://wiki.archlinux.org/index.php/Arch_User_Repository#Installing_packages) is using yaourt:

`yaourt -S co2mon-git`

### Fedora GNU/Linux and RHEL/CentOS/Scientific Linux
co2mon packages can be installed from russianfedora-free repo or directly from [koji](http://koji.russianfedora.pro/koji/packageinfo?packageID=174)

Install repo:

`su -c 'dnf install --nogpgcheck http://mirror.yandex.ru/fedora/russianfedora/russianfedora/free/fedora/russianfedora-free-release-stable.noarch.rpm http://mirror.yandex.ru/fedora/russianfedora/russianfedora/nonfree/fedora/russianfedora-nonfree-release-stable.noarch.rpm'`

Install co2mon:

`dnf install co2mon`

### From sources

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
