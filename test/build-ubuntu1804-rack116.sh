#!/usr/bin/env bash

docker build \
       --build-arg TAG="ubuntu18.04-rack1.1.6" \
       --no-cache \
       -t vcv-robrichards-build:latest .
