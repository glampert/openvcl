#!/bin/bash
# Builds the environment run_vcl.py needs to run SCE vcl 1.4beta7 (an i386
# Linux binary) on macOS under Qiling: a Python venv and an i386 rootfs.
#
#   SCEVCL_HOME  where to build it (default ~/.cache/openvcl-scevcl)
#   TYRA         a Tyra checkout, for assets/vcl (default ~/Repos/GitHub/tyra)
#   OPENVCL      an openvcl binary, for the instruction table (default ../../openvcl)
set -euo pipefail

HOME_DIR=${SCEVCL_HOME:-$HOME/.cache/openvcl-scevcl}
TYRA=${TYRA:-$HOME/Repos/GitHub/tyra}
HERE=$(cd "$(dirname "$0")" && pwd)
OPENVCL=${OPENVCL:-$HERE/../../openvcl}
VCL_SHA256=83bee75205b5b0f37dbefc2c2769eba2c353befa532472c49aedb2ebaf168274

mkdir -p "$HOME_DIR"
cd "$HOME_DIR"

# Python 3.14 has no distutils, which keystone imports; and keystone's native
# library does not load here. Qiling only needs keystone to assemble, which
# running vcl never does, so it is replaced by a stub of its constants.
if [ ! -d venv ]; then
    python3 -m venv venv
    ./venv/bin/pip install -q --upgrade pip
    ./venv/bin/pip install -q qiling setuptools
fi
K=$(echo venv/lib/python3*/site-packages/keystone)
if ! grep -q "keystone stub" "$K/__init__.py"; then
    cp "$K/__init__.py" "$K/__init__.py.orig"
    cat > "$K/__init__.py" <<'EOF'
# keystone stub: the constants Qiling imports; assembling raises.
from .keystone_const import *
from .x86_const import *
from .arm_const import *
from .arm64_const import *
from .mips_const import *
from .ppc_const import *

__version__ = '0.0-stub'

class KsError(Exception):
    pass

class Ks(object):
    def __init__(self, arch, mode):
        self.arch, self.mode = arch, mode
    def asm(self, *args, **kwargs):
        raise KsError('keystone stub: assembling is not available')

def ks_version():
    return (0, 0)

def ks_arch_supported(arch):
    return False

def version_bind():
    return (0, 0)

def debug():
    return 'stub'
EOF
fi

# The i386 libraries vcl links against, from Ubuntu 20.04 (what Tyra's
# Dockerfile runs vcl on).
mkdir -p debs rootfs/bin rootfs/work rootfs/tmp
while read -r path sha; do
    deb=debs/$(basename "$path")
    [ -f "$deb" ] || curl -s -f -o "$deb" "http://archive.ubuntu.com/ubuntu/$path"
    echo "$sha  $deb" | shasum -a 256 -c - > /dev/null
    x=debs/x-$(basename "$path" .deb)
    mkdir -p "$x"
    (cd "$x" && ar x "../$(basename "$path")" && tar -xf data.tar.* -C ../../rootfs)
done <<'EOF'
pool/main/g/glibc/libc6_2.31-0ubuntu9_i386.deb f3508da9bcd7b052df8e30d6c82902655809b68f8210edbc907b3fdc99d9b0ab
pool/universe/g/gcc-3.3/libstdc++5_3.3.6-30ubuntu2_i386.deb cfb0e2db802c863026ec32dcac2edcbe76a509853c9a91cae500ad1f7af1dda0
pool/main/g/gcc-10/libgcc-s1_10-20200411-0ubuntu1_i386.deb c2b7a0c246b4bb41d2a3fe00f440737c6fd78764fb19daadbbbfdaf6967d0ba0
EOF
# ld.so stops at Qiling's EPERM for a missing file in /lib before it reaches
# /usr/lib, so libstdc++ goes next to the other libraries.
cp rootfs/usr/lib/i386-linux-gnu/libstdc++.so.5.0.7 rootfs/lib/i386-linux-gnu/
ln -sf libstdc++.so.5.0.7 rootfs/lib/i386-linux-gnu/libstdc++.so.5

cp "$TYRA/assets/vcl" rootfs/bin/vcl
echo "$VCL_SHA256  rootfs/bin/vcl" | shasum -a 256 -c - > /dev/null
chmod +x rootfs/bin/vcl

"$OPENVCL" --dump-instruction-info-json > info.json

echo "ready: $HOME_DIR"
