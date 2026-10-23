#!/bin/sh
set -eu
cd "$HOME"
mkdir -p oslab-tools
cd oslab-tools
python3 -m venv qemu-env
qemu-env/bin/pip install -i https://pypi.tuna.tsinghua.edu.cn/simple ninja meson==1.2.3
curl -fL http://cn.archive.ubuntu.com/ubuntu/pool/main/q/qemu/qemu_8.2.2+ds.orig.tar.xz -o qemu-ubuntu.tar.xz
mkdir -p qemu-source
tar xf qemu-ubuntu.tar.xz -C qemu-source --strip-components=1
mkdir -p qemu-ubuntu-build
cd qemu-ubuntu-build
export PATH="$HOME/oslab-tools/qemu-env/bin:$PATH"
../qemu-source/configure --target-list=riscv64-softmmu --enable-debug --disable-docs --disable-werror --disable-pixman --disable-tools --disable-install-blobs
ninja -j4
