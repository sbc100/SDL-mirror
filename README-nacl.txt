==============================================================================
Simple DirectMedia Layer for Native Client (Chrome)
==============================================================================

Requirements:
 - NaCl SDK (version 35 or above)
 - Chrome (version 35 or above)
 - naclports checkout:
   - https://code.google.com/p/naclports/


==============================================================================
Building
==============================================================================

- export NACL_SDK_ROOT=<path_to_pepper_dir>
- export PATH=$PATH:<naclports_src>/bin
- CONFIG_SUB=build-scripts/config.sub naclconfigure ./configure
- make

==============================================================================
Building Tests
==============================================================================

- CONFIG_SUB=../build-scripts/config.sub naclconfigure ./configure \
  --with-sdl-prefix=$NACL_SDK_ROOT/toolchain/linux_x86_newlib/x86_64-nacl/usr
- make
