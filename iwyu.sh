#!/usr/bin/env sh

echo $*

PATH=${HOME}/repos/include-what-you-use/build/bin:${PATH} \
    ${HOME}/repos/include-what-you-use/iwyu_tool.py -p . $*
