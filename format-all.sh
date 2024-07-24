#!/usr/bin/bash 
find DefinitelyNotMinecraft/ -iname '*.h' -o -iname '*.cpp' | xargs clang-format -i