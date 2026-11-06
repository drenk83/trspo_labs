#!/bin/sh
cd "$(dirname "$0")"

./build/app-static  font.ttf out-static.png  TRSPO
./build/app-dynamic font.ttf out-dynamic.png TRSPO
./build/elfloader build/app-blob font.ttf out-blob.png TRSPO
