#!/bin/bash
emcc --bind wasm/sort.cpp -O3 -ffast-math -g0 -flto -fno-exceptions -o src/wasm/sort.js \
    -s EXPORT_ES6=1 \
    -s MODULARIZE=1 \
    -s EXPORT_NAME=loadWasm \
    -s EXPORTED_FUNCTIONS="[_sort, _cleanUp, _malloc, _free]" \
    -s SINGLE_FILE=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s ENVIRONMENT=worker \
    -s ASSERTIONS=0 \
    -s INVOKE_RUN=0 \
    -s CHECK_NULL_WRITES=0 \
    -msimd128 \
    -msse4.2 \
    -msse4.1

emcc --bind wasm/data.cpp -Oz -o src/wasm/data.js \
    -s EXPORT_ES6=1 \
    -s MODULARIZE=1 \
    -s EXPORT_NAME=loadWasm \
    -s EXPORTED_FUNCTIONS="[_pack, _malloc, _free]" \
    -s SINGLE_FILE=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s ENVIRONMENT=worker

emcc --bind wasm/intersect.cpp -Oz -o src/wasm/intersect.js \
    -s EXPORT_ES6=1 \
    -s MODULARIZE=1 \
    -s EXPORT_NAME=loadWasm \
    -s EXPORTED_FUNCTIONS="[_evaluate, _malloc, _free]" \
    -s SINGLE_FILE=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s ENVIRONMENT=web
