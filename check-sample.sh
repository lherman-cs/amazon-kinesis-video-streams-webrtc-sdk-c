#!/bin/bash

cd build
./kvsWebrtcClientMaster SampleChannel &

sleep 2
./kvsWebrtcClientViewer SampleChannel &

wait "$!"
