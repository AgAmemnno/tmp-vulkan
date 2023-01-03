#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later

# This script is part of the official build environment, see WIKI page for details.
# https://wiki.blender.org/wiki/Building_Blender/Other/CentOS7ReleaseEnvironment

set -e

if [ `id -u` -ne 0 ]; then
   echo "This script must be run as root"
   exit 1
fi

# Required by: config manager command below to enable powertools.
dnf install 'dnf-command(config-manager)'

# Packages `ninja-build` and `meson` are not available unless CBR or PowerTools repositories are enabled.
# See: https://wiki.rockylinux.org/rocky/repo/#notes-on-unlisted-repositories
dnf config-manager --set-enabled powertools

# Required by: epel-release has the patchelf and rubygem-asciidoctor packages
dnf install epel-release

# `yum-config-manager` does not come in the default minimal install,
# so make sure it is installed and available.
yum -y update
yum -y install yum-utils

# Install all the packages needed for a new tool-chain.
#
# NOTE: Keep this separate from the packages install, since otherwise
# older tool-chain will be installed.
yum -y update
yum -y install scl-utils
yum -y install scl-utils-build

# Currently this is defined by the VFX platform (CY2023), see: https://vfxplatform.com
yum -y install gcc-toolset-11

# Install packages needed for Blender's dependencies.
PACKAGES_FOR_LIBS=(
    # Used to checkout Blender's code.
    git
    # Used to checkout Blender's `../lib/` directory.
    subversion
    # Used to extract packages.
    bzip2
    # Used to extract packages.
    tar
    # Blender and some dependencies use `cmake`.
    cmake3
    # Apply patches from Blender's: `./build_files/build_environment/patches`
    patch
    # Use by `cmake` and `autoconf`.
    make

    # Required by: `external_nasm` which uses an `autoconf` build-system.
    autoconf
    automake
    libtool

    # TODO: why is this needed?
    patchelf

    # Builds generated by meson use Ninja for the actual build.
    ninja-build

    # Required by Blender build option: `WITH_GHOST_WAYLAND`.
    mesa-libEGL-devel
    # Required by: Blender & `external_opensubdiv` (probably others).
    mesa-libGL-devel
    mesa-libGLU-devel

    # Required by: `external_ispc`.
    zlib-devel
    # TODO: dependencies build without this, consider removal.
    rubygem-asciidoctor
    # TODO: dependencies build without this, consider removal.
    wget
    # Required by: `external_sqlite` as a build-time dependency (needed for the `tclsh` command).
    tcl
    # Required by: `external_aom`.
    # TODO: Blender is already building `external_nasm` which is listed as an alternative to `yasm`.
    # Why are both needed?
    yasm

    # NOTE(@campbellbarton): while `python39` is available, the default Python version is 3.6.
    # This is used for the `python3-mako` package for e.g.
    # So use the "default" system Python since it means it's most compatible with other packages.
    python3
    # Required by: `external_mesa`.
    python3-mako

    # Required by: `external_mesa`.
    expat-devel

    # Required by: `external_igc` & `external_osl` as a build-time dependency.
    bison
    # Required by: `external_osl` as a build-time dependency.
    flex

    # Required by: `external_ispc`.
    ncurses-devel
    # Required by: `external_ispc` (when building with CLANG).
    libstdc++-static
)

# Additional packages needed for building Blender.
PACKAGES_FOR_BLENDER=(
    # Required by Blender build option: `WITH_GHOST_WAYLAND`.
    libxkbcommon-devel

    # Required by Blender build option: `WITH_GHOST_X11`.
    libX11-devel
    libXcursor-devel
    libXi-devel
    libXinerama-devel
    libXrandr-devel
    libXt-devel
    libXxf86vm-devel
)

yum -y install -y ${PACKAGES_FOR_LIBS[@]} ${PACKAGES_FOR_BLENDER[@]}

# Dependencies for pip (needed for `buildbot-worker`), uses Python3.6.
yum -y install python3 python3-pip python3-devel

# Dependencies for asound.
yum -y install -y  \
    alsa-lib-devel pulseaudio-libs-devel
