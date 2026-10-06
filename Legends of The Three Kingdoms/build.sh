#!/bin/sh
set -e
g++ -std=c++17 main.cpp src/*.cpp -Iinclude -o thks
echo "Build succeeded: ./thks"
