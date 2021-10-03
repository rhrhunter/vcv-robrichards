#!/usr/bin/env bash

docker build \
       --build-arg TAG="ubuntu18.04-rack2" \
       -t vcv-robrichards-build:latest .
