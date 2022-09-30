#!/bin/bash

gcc \
    xlib_wheel.cpp \
    wheel.cpp \
    mesh_wheel.cpp \
    render_wheel.cpp \
    physics_wheel.cpp \
    scene_wheel.cpp \
    shape_wheel.cpp \
    files_wheel.cpp \
    -o app -lX11 -lXext -lrt -lm \
    -g \
    -Wall -Wno-unused-function
