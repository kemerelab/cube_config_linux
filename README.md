# cube_config_linux # 

This repository contains source code for Linux-compatible SD card utilities using SpikeGadgets 
wireless recording hardware. 

## Building from source ##
The executables are already provided, but you can always build from source if you'd like. Since 
these are small programs, it's quick and easy to call gcc directly.

All of the utilities use the functions defined in diskio.c so be sure to include this as 
an input file for gcc if you're building from source.

Example:

```gcc write_config.c diskio.c -o write_config```