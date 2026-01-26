#!/bin/bash
set -e -x
OS=$(uname)

OPUS=opus-1.4
DAV1D=dav1d-1.5.2

# Download sources
curl -sL --retry 10 http://downloads.xiph.org/releases/opus/${OPUS}.tar.gz > ${OPUS}.tar.gz
curl -sL --retry 10 https://downloads.videolan.org/pub/videolan/dav1d/${DAV1D_VER}/${DAV1D}.tar.xz > ${DAV1D}.tar.xz

# Unzip
tar xzvf ${OPUS}.tar.gz
tar xvf ${DAV1D}.tar.xz

# Rename directories to the project names
mv ${OPUS} opus
mv ${DAV1D} dav1d

# Patching ./opus/silk/meson.build
if [ "$OS" == "Linux" ]; then
  echo "Patching ./opus/silk/meson.build on Linux"
  sed -i 's/ and have_arm_intrinsics_or_asm//g' ./opus/silk/meson.build
elif [ "$OS" == "Darwin" ]; then
  echo "Patching ./opus/silk/meson.build on macOS"
  sed -i '' 's/ and have_arm_intrinsics_or_asm//g' ./opus/silk/meson.build
else
  echo "Unknown OS: $OS"
fi
