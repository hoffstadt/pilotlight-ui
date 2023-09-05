#!/bin/bash

# colors
BOLD=$'\e[0;1m'
RED=$'\e[0;31m'
GREEN=$'\e[0;32m'
CYAN=$'\e[0;36m'
NC=$'\e[0m'

# find directory of this script
SOURCE=${BASH_SOURCE[0]}
while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
  DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )
  SOURCE=$(readlink "$SOURCE")
  [[ $SOURCE != /* ]] && SOURCE=$DIR/$SOURCE # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )

# make script directory CWD
pushd $DIR >/dev/null

# create output directory
if ! [[ -d "../out" ]]; then
    mkdir "../out"
fi

rm -f ../out/x11_vulkan_example

# preprocessor defines
PL_DEFINES="-D_USE_MATH_DEFINES "

# includes directories
PL_INCLUDE_DIRECTORIES="-I../.. -I../../backends -I$VULKAN_SDK/include -I/usr/include/vulkan "

# link directories
PL_LINK_DIRECTORIES="-L/usr/lib/x86_64-linux-gnu -L$VULKAN_SDK/lib "

# compiler flags
PL_COMPILER_FLAGS="-std=gnu99 --debug -g "

# linker flags
PL_LINKER_FLAGS="-ldl -lm "

# libraries
PL_LINK_LIBRARIES="-lxcb -lX11 -lX11-xcb -lxkbcommon -lxcb-cursor -lxcb-xfixes -lxcb-keysyms -lvulkan "

# default compilation result
PL_RESULT=${BOLD}${GREEN}Successful.${NC}

PL_SOURCES="main.c ../../pl_ui_draw.c ../../pl_ui.c ../../pl_ui_widgets.c ../../pl_ui_demo.c ../../backends/pl_ui_vulkan.c "

# run compiler (and linker)
echo
echo ${CYAN}Compiling and Linking...${NC}
gcc -fPIC $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES $PL_LINK_DIRECTORIES $PL_LINKER_FLAGS $PL_LINK_LIBRARIES -o "../out/x11_vulkan_example"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}

# return CWD to previous CWD
popd >/dev/null