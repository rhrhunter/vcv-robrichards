#!/usr/bin/env bash

docker build \
       --build-arg TAG="ubuntu18.10-rack1.1.6" \
       -t vcv-robrichards-build:latest .
