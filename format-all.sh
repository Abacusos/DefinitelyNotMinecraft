#!/usr/bin/bash 
find DefinitelyNotMinecraft/ -iname '*.hpp' -o -iname '*.cpp' | xargs clang-format --style=file -i --verbose