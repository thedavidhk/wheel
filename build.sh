#!/bin/bash

gcc xlib_handmade.cpp handmade.cpp -o game -lX11 -lXext -lrt -lm -g -Wall
