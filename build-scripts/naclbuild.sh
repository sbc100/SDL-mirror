#!/bin/bash

set -o errexit

if [ -z "$1" ] && [ -z "$NACL_SDK_ROOT" ]; then
    echo "Usage: ./naclbuild ~/nacl/pepper_35"
    echo "This will build SDL for Native Client, and testgles2.c as a demo"
    echo "You can set env vars CC, AR, LD and RANLIB to override the default PNaCl toolchain used"
    echo "You can set env var SOURCES to select a different source file than testgles2.c"
    exit 1
fi

if [ -n "$1" ]; then
    NACL_SDK_ROOT="$1"
fi

CC=""

if [ -n "$2" ]; then
    CC="$2"
fi

echo "Using SDK at $NACL_SDK_ROOT"

NCPUS="1"
case "$OSTYPE" in
    darwin*)
        NCPU=`sysctl -n hw.ncpu`
        ;; 
    linux*)
        if [ -n `which nproc` ]; then
            NCPUS=`nproc`
        fi  
        ;;
  *);;
esac

CURDIR=`pwd -P`
SDLPATH="$( cd "$(dirname "$0")/.." ; pwd -P )"
BUILDPATH="$SDLPATH/build/nacl"
TESTBUILDPATH="$BUILDPATH/test"
SDL2_STATIC="$BUILDPATH/build/.libs/libSDL2.a"
mkdir -p $BUILDPATH
mkdir -p $TESTBUILDPATH

if [ -z "$CC" ]; then
    CC="$NACL_SDK_ROOT/toolchain/linux_pnacl/bin/pnacl-clang"
    CPPFLAGS="-I$NACL_SDK_ROOT/include -I$NACL_SDK_ROOT/include/pnacl"
fi
if [ -z "$AR" ]; then
    AR="$NACL_SDK_ROOT/toolchain/linux_pnacl/bin/pnacl-ar"
fi
if [ -z "$LD" ]; then
    LD="$NACL_SDK_ROOT/toolchain/linux_pnacl/bin/pnacl-ar"
fi
if [ -z "$RANLIB" ]; then
    RANLIB="$NACL_SDK_ROOT/toolchain/linux_pnacl/bin/pnacl-ranlib"
fi

if [ -z "$SOURCES" ]; then
    SOURCES="$SDLPATH/test/testgles2.c"
fi

if [ ! -f "$CC" ]; then
    echo "Could not find compiler at $CC"
    exit 1
fi

export NACL_SDK_ROOT
export CC
export AR
export LD
export RANLIB
export CPPFLAGS

cd $BUILDPATH
CONFIGURE="$SDLPATH/configure --host=pnacl --prefix $TESTBUILDPATH"
echo ""
echo "Running configure: $CONFIGURE"
echo ""
$CONFIGURE
echo ""
echo "Running make:"
echo ""
make -j$NCPUS
make install

if [ ! -f "$SDL2_STATIC" ]; then
    echo "Build failed! $SDL2_STATIC"
    exit 1
fi

echo "Building test"
cp -f $SDLPATH/test/nacl/* $TESTBUILDPATH
# Some tests need these resource files
cp -f $SDLPATH/test/*.bmp $TESTBUILDPATH
cp -f $SDLPATH/test/*.wav $TESTBUILDPATH
cp -f $SDL2_STATIC $TESTBUILDPATH

# Copy user sources
cp $SOURCES $TESTBUILDPATH

cd $TESTBUILDPATH
export SOURCES
make -j$NCPUS CONFIG="Release" CFLAGS="-I$TESTBUILDPATH/include/SDL2 -I$SDLPATH/include"
make -j$NCPUS CONFIG="Debug" CFLAGS="-I$TESTBUILDPATH/include/SDL2 -I$SDLPATH/include"

echo
echo "Run the test with: "
echo "cd $TESTBUILDPATH && python -m SimpleHTTPServer"
echo "Then visit http://localhost:8000 with Chrome"
