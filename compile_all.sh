#!/bin/bash

if [ ! -d bin ]; then
    mkdir bin
fi

gcc src/read_config.c src/diskio_linux.c -o bin/read_config
gcc src/write_config.c src/diskio_linux.c -o bin/write_config
gcc src/card_enable.c src/diskio_linux.c -o bin/card_enable
gcc src/pcheck.c src/diskio_linux.c -o bin/pcheck
gcc src/sd_card_extract.c src/diskio_linux.c -o bin/sd_card_extract
