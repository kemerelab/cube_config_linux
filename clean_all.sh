#!/bin/bash

if [ -d bin ]; then
    rm -rf bin
else
    echo Already clean
fi
