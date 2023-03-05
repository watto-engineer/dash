Gitian building
================

*Setup instructions for a Gitian build of Wagerr Core using a Debian VM or physical system.*

Gitian is the deterministic build process that is used to build the Wagerr
Core executables. It provides a way to be reasonably sure that the
executables are really built from the source on GitHub. It also makes sure that
the same, tested dependencies are used and statically built into the executable.

Multiple developers build the source code by following a specific descriptor
("recipe"), cryptographically sign the result, and upload the resulting signature.
These results are compared and only if they match, the build is accepted and uploaded
to wagerr.org.

More independent Gitian builders are needed, which is why this guide exists.
It is preferred you follow these steps yourself instead of using someone else's
VM image to avoid 'contaminating' the build.

Table of Contents
------------------

- [Create a new VirtualBox VM](#create-a-new-virtualbox-vm)
- [Connecting to the VM](#connecting-to-the-vm)
- [Setting up Debian for Gitian building](#setting-up-debian-for-gitian-building)
- [Installing Gitian](#installing-gitian)
- [Setting up the Gitian image](#setting-up-the-gitian-image)
- [Getting and building the inputs](#getting-and-building-the-inputs)
- [Building Wagerr Core](#building-wagerr-core)
- [Building an alternative repository](#building-an-alternative-repository)
- [Signing externally](#signing-externally)
- [Uploading signatures](#uploading-signatures)

Preparing the Gitian builder host
---------------------------------

The first step is to prepare the host environment that will be used to perform the Gitian builds.
This guide explains how to set up the environment, and how to start the builds.

Debian Linux was chosen as the host distribution because it has a lightweight install (in contrast to Ubuntu) and is readily available.
Any kind of virtualization can be used, for example:
- [VirtualBox](https://www.virtualbox.org/) (covered by this guide)
- [KVM](http://www.linux-kvm.org/page/Main_Page)
- [Docker](https://docker.com/), see also [Gitian host docker container](https://github.com/gdm85/tenku/tree/master/docker/gitian-bitcoin-host/README.md).

You can also install Gitian on actual hardware instead of using virtualization.

Create a new VirtualBox VM
---------------------------

Get the [Debian 11.x net installer](https://cdimage.debian.org/mirror/cdimage/release/current/amd64/iso-cd/debian-11.6.0-amd64-netinst.iso) (a more recent minor version should also work, see also [Debian Network installation](https://www.debian.org/CD/netinst/)).
This DVD image can be [validated](https://www.debian.org/CD/verify) using a SHA256 hashing tool, for example on
Unixy OSes by entering the following in a terminal:

    echo "e482910626b30f9a7de9b0cc142c3d4a079fbfa96110083be1d0b473671ce08d  debian-11.6.0-amd64-netinst.iso" | sha256sum -c

    # (must return OK)
In the VirtualBox GUI click "New" and set the following parameters in the wizard:

![](gitian-building/create_new_vm.png)

- Type: Linux, Debian (64-bit)

- Make sure that `Skip Unattended Install` is checked.

![](gitian-building/create_vm_memsize.png)

- Memory Size: at least 4096MB, anything less and the build might not complete.
- Increase the number of processors to the number of cores on your machine if you want builds to be faster.

![](gitian-building/create_vm_hard_disk.png)

- Hard Disk: Create a virtual hard disk now

- File location and size: at least 120GB; as low as 20GB *may* be possible, but better to err on the safe side

- Hard Disk file type: Use the default, VDI (VirtualBox Disk Image)

- Storage on physical hard disk: Dynamically Allocated (the default)

- Click `Finish`

After creating the VM, we need to configure it.

- Click the `Settings` button, then go to `System` tab and `Network` Adapter 1 should be attached to `NAT`.

![](gitian-building/network_settings.png)

- Click `Advanced`, then `Port Forwarding`. We want to set up a port through which we can reach the VM to get files in and out.
- Create a new rule by clicking the plus icon.

![](gitian-building/port_forwarding_rules.png)

- Set up the new rule the following way:
  - Name: `SSH`
  - Protocol: `TCP`
  - Leave Host IP empty
  - Host Port: `22222`
  - Leave Guest IP empty
  - Guest Port: `22`

- Click `Ok` twice to save.


Installing Debian
------------------

This section will explain how to install Debian on the newly created VM.

- Start the VM

- Choose the non-graphical installer.  We do not need the graphical environment; it will only increase installation time and disk usage.

![](gitian-building/debian_install_1_boot_menu.png)

**Note**: Navigating in the Debian installer:
To keep a setting at the default and proceed, just press `Enter`.
To select a different button, press `Tab`.

- Choose locale and keyboard settings (doesn't matter, you can just go with the defaults or select your own information)

![](gitian-building/debian_install_2_select_a_language.png)
![](gitian-building/debian_install_3_select_location.png)
![](gitian-building/debian_install_4_configure_keyboard.png)

- The VM will detect network settings using DHCP, this should all proceed automatically
- Configure the network:
  - Hostname `debian`.
  - Leave domain name empty.

![](gitian-building/debian_install_5_configure_the_network.png)
![](gitian-building/debian_install_6_domain_name.png)

- Choose a root password and enter it twice (remember it for later)

![](gitian-building/debian_install_6a_set_up_root_password.png)

- Name the new user `gitianbuilder` (the full name doesn't matter, you can leave it empty)
- Set the account username as `gitianbuilder`

![](gitian-building/debian_install_7_set_up_user_fullname.png)
![](gitian-building/debian_install_8_set_up_username.png)

- Choose a user password and enter it twice (remember it for later)

![](gitian-building/debian_install_9_user_password.png)

- The installer will set up the clock using a time server; this process should be automatic
- Set up the clock: choose a time zone (depends on the locale settings that you picked earlier; specifics don't matter)  

![](gitian-building/debian_install_10_configure_clock.png)

- Disk setup
  - Partitioning method: Guided - Use the entire disk

![](gitian-building/debian_install_11_partition_disks.png)

  - Select disk to partition: SCSI1 (0,0,0)

![](gitian-building/debian_install_12_choose_disk.png)

  - Partition Disks -> *All files in one partition*

![](gitian-building/all_files_in_one_partition.png)

  - Finish partitioning and write changes to disk -> *Yes* (`Tab`, `Enter` to select the `Yes` button)

![](gitian-building/debian_install_14_finish.png)
![](gitian-building/debian_install_15_write_changes.png)

- The base system will be installed, this will take a minute or so
- Choose a mirror (any will do)

![](gitian-building/debian_install_16_choose_a_mirror.png)

- Enter proxy information (unless you are on an intranet, leave this empty)

![](gitian-building/debian_install_18_proxy_settings.png)

- Wait a bit while 'Select and install software' runs
- Participate in popularity contest -> *No*
- Choose software to install. We need just the base system.
- Make sure only 'SSH server' and 'Standard System Utilities' are checked
- Uncheck 'Debian Desktop Environment' and 'Print Server'

![](gitian-building/debian_install_19_software_selection.png)

- Install the GRUB boot loader to the master boot record? -> Yes

![](gitian-building/debian_install_20_install_grub.png)

- Device for boot loader installation -> ata-VBOX_HARDDISK

![](gitian-building/debian_install_21_install_grub_bootloader.png)

- Installation Complete -> *Continue*
- After installation, the VM will reboot and you will have a working Debian VM. Congratulations!

![](gitian-building/debian_install_22_finish_installation.png)


After Installation
-------------------
The next step in the guide involves logging in as root via SSH.
SSH login for root users is disabled by default, so we'll enable that now.

Login to the VM using username `root` and the root password you chose earlier.
You'll be presented with a screen similar to this.

![](gitian-building/debian_root_login.png)

Type:

```
systemctl ssh enable
sed -i 's/^PermitRootLogin.*/PermitRootLogin yes/' /etc/ssh/sshd_config
```
and press enter. Then,
```
systemctl ssh restart
```
and enter to restart SSH. Logout by typing 'logout' and pressing 'enter'.

Connecting to the VM
----------------------

After the VM has booted you can connect to it using SSH, and files can be copied from and to the VM using a SFTP utility.
Connect to `localhost`, port `22222` (or the port configured when installing the VM).
On Windows you can use [putty](http://www.chiark.greenend.org.uk/~sgtatham/putty/download.html) and [WinSCP](http://winscp.net/eng/index.php).

For example, to connect as `root` from a Linux command prompt use

    $ ssh root@localhost -p 22222
    The authenticity of host '[localhost]:22222 ([127.0.0.1]:22222)' can't be established.
    RSA key fingerprint is ae:f5:c8:9f:17:c6:c7:1b:c2:1b:12:31:1d:bb:d0:c7.
    Are you sure you want to continue connecting (yes/no)? yes
    Warning: Permanently added '[localhost]:22222' (RSA) to the list of known hosts.
    root@localhost's password: (enter root password configured during install)

    The programs included with the Debian GNU/Linux system are free software;
    the exact distribution terms for each program are described in the
    individual files in /usr/share/doc/*/copyright.

    Debian GNU/Linux comes with ABSOLUTELY NO WARRANTY, to the extent
    permitted by applicable law.
    root@gitianbuilder:~#

Replace `root` with `gitianbuilder` to log in as user.

Setting up Debian for Gitian building
--------------------------------------

In this section we will be setting up the Debian installation for Gitian building.

First we need to log in as `root` to set up dependencies and make sure that our
user can use the sudo command. Type/paste the following in the terminal:

```bash
apt-get install git ruby sudo apt-cacher-ng qemu-utils debootstrap docker docker.io parted kpartx bridge-utils make curl wget python2-minimal
usermod -aG sudo gitianbuilder
usermod -aG docker gitianbuilder
reboot
```
At the end the VM is rebooted to make sure that the changes take effect. The steps in this
section only need to be performed once.

Installing Gitian
------------------

Re-login as the user `gitianbuilder` that was created during installation.
The rest of the steps in this guide will be performed as that user.

There is no `python-vm-builder` package in Debian, so we need to install it from source ourselves,

```bash
wget http://archive.ubuntu.com/ubuntu/pool/universe/v/vm-builder/vm-builder_0.12.4+bzr494.orig.tar.gz
echo "76cbf8c52c391160b2641e7120dbade5afded713afaa6032f733a261f13e6a8e  vm-builder_0.12.4+bzr494.orig.tar.gz" | sha256sum -c
# (verification -- must return OK)
tar -zxvf vm-builder_0.12.4+bzr494.orig.tar.gz
cd vm-builder-0.12.4+bzr494
sudo python2 setup.py install
cd ..
```

**Note**: When sudo asks for a password, enter the password for the user *debian* not for *root*.

Clone the git repositories for Wagerr Core and Gitian.

```bash
git clone https://github.com/devrandom/gitian-builder.git
git clone https://github.com/wagerr/wagerr
git clone https://github.com/wagerr/gitian.sigs.git
```

Setting up the Gitian image
-------------------------

Gitian needs a virtual image of the operating system to build in.
Currently this is Ubuntu Trusty x86_64.
This image will be copied and used every time that a build is started to
make sure that the build is deterministic.
Creating the image will take a while, but only has to be done once.

Execute the following as user `gitianbuilder`:

```bash
cd gitian-builder
bin/make-base-vm --docker --arch amd64 --distro debian --suite bullseye --disksize 22287
```

There may be some warnings printed during the build of the image. These can be ignored.

**Note**: When sudo asks for a password, enter the password for the user *gitianbuilder* not for *root*.

**Note**: Repeat this step when you have upgraded to a newer version of Gitian.

```

Getting and building the inputs
--------------------------------

At this point you have two options, you can either use the automated script (found in [contrib/gitian-build.py](/contrib/gitian-build.py)) or you could manually do everything by following this guide. If you're using the automated script, then run it with the "--setup" command. Afterwards, run it with the "--build" command (example: "contrib/gitian-building.sh -b signer 0.13.0"). Otherwise ignore this.

Follow the instructions in [doc/release-process.md](release-process.md#fetch-and-create-inputs-first-time-or-when-dependency-versions-change)
in the Wagerr Core repository under 'Fetch and create inputs' to install sources which require
manual intervention. Also optionally follow the next step: 'Seed the Gitian sources cache
and offline git repositories' which will fetch the remaining files required for building
offline.

Building Wagerr Core
----------------

To build Wagerr Core (for Linux, macOS and Windows) just follow the steps under 'perform
Gitian builds' in [doc/release-process.md](release-process.md#setup-and-perform-gitian-builds) in the Wagerr Core repository.

This may take some time as it will build all the dependencies needed for each descriptor.
These dependencies will be cached after a successful build to avoid rebuilding them when possible.

At any time you can check the package installation and build progress with

```bash
tail -f var/install.log
tail -f var/build.log
```

Output from `gbuild` will look something like

```bash
    Initialized empty Git repository in /home/debian/gitian-builder/inputs/wagerr/.git/
    remote: Counting objects: 57959, done.
    remote: Total 57959 (delta 0), reused 0 (delta 0), pack-reused 57958
    Receiving objects: 100% (57959/57959), 53.76 MiB | 484.00 KiB/s, done.
    Resolving deltas: 100% (41590/41590), done.
    From https://github.com/wagerr/wagerr
    ... (new tags, new branch etc)
    --- Building for bullseye amd64 ---
    Stopping target if it is up
    Making a new image copy
    stdin: is not a tty
    Starting target
    Checking if target is up
    Preparing build environment
    Updating apt-get repository (log in var/install.log)
    Installing additional packages (log in var/install.log)
    Grabbing package manifest
    stdin: is not a tty
    Creating build script (var/build-script)
    lxc-start: Connection refused - inotify event with no name (mask 32768)
    Running build script (log in var/build.log)
```
Building an alternative repository
-----------------------------------

If you want to do a test build of a pull on GitHub it can be useful to point
the Gitian builder at an alternative repository, using the same descriptors
and inputs.

For example:
```bash
URL=https://github.com/crowning-/wagerr.git
COMMIT=b616fb8ef0d49a919b72b0388b091aaec5849b96
./bin/gbuild --commit wagerr=${COMMIT} --url wagerr=${URL} ../wagerr/contrib/gitian-descriptors/gitian-linux.yml
./bin/gbuild --commit wagerr=${COMMIT} --url wagerr=${URL} ../wagerr/contrib/gitian-descriptors/gitian-win.yml
./bin/gbuild --commit wagerr=${COMMIT} --url wagerr=${URL} ../wagerr/contrib/gitian-descriptors/gitian-osx.yml
```

Building fully offline
-----------------------

For building fully offline including attaching signatures to unsigned builds, the detached-sigs repository
and the wagerr git repository with the desired tag must both be available locally, and then gbuild must be
told where to find them. It also requires an apt-cacher-ng which is fully-populated but set to offline mode, or
manually disabling gitian-builder's use of apt-get to update the VM build environment.

To configure apt-cacher-ng as an offline cacher, you will need to first populate its cache with the relevant
files. You must additionally patch target-bin/bootstrap-fixup to set its apt sources to something other than
plain archive.ubuntu.com: us.archive.ubuntu.com works.

So, if you use Docker:

```bash
export PATH="$PATH":/path/to/gitian-builder/libexec
export USE_DOCKER=1
cd /path/to/gitian-builder
libexec/make-clean-vm --docker --arch amd64 --distro debian --suite bullseye --disksize 22287

DOCKER_ARCH=amd64 DOCKER_SUITE=bullseye on-target -u root apt-get update
DOCKER_ARCH=amd64 DOCKER_SUITE=bullseye on-target -u root \
  -e DEBIAN_FRONTEND=noninteractive apt-get --no-install-recommends -y install \
  $( sed -ne '/^packages:/,/[^-] .*/ {/^- .*/{s/"//g;s/- //;p}}' ../wagerr/contrib/gitian-descriptors/*|sort|uniq )
DOCKER_ARCH=amd64 DOCKER_SUITE=bullseye on-target -u root apt-get -q -y purge grub
DOCKER_ARCH=amd64 DOCKER_SUITE=bullseye on-target -u root -e DEBIAN_FRONTEND=noninteractive apt-get -y dist-upgrade
```

And then set offline mode for apt-cacher-ng:

```
/etc/apt-cacher-ng/acng.conf
[...]
Offlinemode: 1
[...]

service apt-cacher-ng restart
```

Then when building, override the remote URLs that gbuild would otherwise pull from the Gitian descriptors::
```bash

cd /some/root/path/
git clone https://github.com/wagerr/wagerr-detached-sigs.git

BTCPATH=/some/root/path/wagerr
SIGPATH=/some/root/path/wagerr-detached-sigs

./bin/gbuild --url wagerr=${BTCPATH},signature=${SIGPATH} ../wagerr/contrib/gitian-descriptors/gitian-win-signer.yml
```

Signing externally
-------------------

If you want to do the PGP signing on another device, that's also possible; just define `SIGNER` as mentioned
and follow the steps in the build process as normal.

    gpg: skipped "crowning-": secret key not available

When you execute `gsign` you will get an error from GPG, which can be ignored. Copy the resulting `.assert` files
in `gitian.sigs` to your signing machine and do

```bash
    gpg --detach-sign ${VERSION}-linux/${SIGNER}/wagerr-linux-build.assert
    gpg --detach-sign ${VERSION}-win/${SIGNER}/wagerr-win-build.assert
    gpg --detach-sign ${VERSION}-osx-unsigned/${SIGNER}/wagerr-osx-build.assert
```

This will create the `.sig` files that can be committed together with the `.assert` files to assert your
Gitian build.

Uploading signatures (not yet implemented)
---------------------

In the future it will be possible to push your signatures (both the `.assert` and `.assert.sig` files) to the
[wagerr/gitian.sigs](https://github.com/wagerr/gitian.sigs/) repository, or if that's not possible to create a pull
request.
There will be an official announcement when this repository is online.
