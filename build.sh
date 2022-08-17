#!/bin/bash

gcc xlib_wheel.cpp wheel.cpp -o wheel -lX11 -lXext -lrt -lm -g -Wall -Wno-unused-function
