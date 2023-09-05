# colors
BOLD=$'\e[0;1m'
RED=$'\e[0;31m'
GREEN=$'\e[0;32m'
CYAN=$'\e[0;36m'
YELLOW=$'\e[0;33m'
NC=$'\e[0m'

# find directory of this script
SOURCE=${BASH_SOURCE[0]}
while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
  DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )
  SOURCE=$(readlink "$SOURCE")
  [[ $SOURCE != /* ]] && SOURCE=$DIR/$SOURCE # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )

# get architecture (intel or apple silicon)
ARCH="$(uname -m)"

# make script directory CWD
pushd $DIR >/dev/null

# create output directory
if ! [[ -d "../out" ]]; then
    mkdir "../out"
fi

rm -f ../out/osx_metal_example

# preprocessor defines
PL_DEFINES="-DPL_METAL_BACKEND "

# includes directories
PL_INCLUDE_DIRECTORIES="-I../.. -I../../backends "

# compiler flags
PL_COMPILER_FLAGS="-std=c99 --debug -g -fmodules -ObjC "

# add flags for specific hardware
if [[ "$ARCH" == "arm64" ]]; then
    PL_COMPILER_FLAGS+="-arch arm64 "
else
    PL_COMPILER_FLAGS+="-arch x86_64 "
fi

# frameworks
PL_LINK_FRAMEWORKS="-framework Metal -framework MetalKit -framework Cocoa -framework IOKit -framework CoreVideo -framework QuartzCore "

# default compilation result
PL_RESULT=${BOLD}${GREEN}Successful.${NC}

# source files
PL_SOURCES="main.m ../../pl_ui_draw.c ../../pl_ui.c ../../pl_ui_widgets.c ../../pl_ui_demo.c ../../backends/pl_ui_metal.m "

# run compiler (and linker)
echo
echo ${YELLOW}~~~~~~~~~~~~~~~~~~~${NC}
echo ${CYAN}Compiling and Linking...${NC}
clang -fPIC $PL_SOURCES $PL_INCLUDE_DIRECTORIES $PL_DEFINES $PL_COMPILER_FLAGS $PL_INCLUDE_DIRECTORIES -o "../out/osx_metal_example"

# check build status
if [ $? -ne 0 ]
then
    PL_RESULT=${BOLD}${RED}Failed.${NC}
fi

# print results
echo ${CYAN}Results: ${NC} ${PL_RESULT}

# return CWD to previous CWD
popd >/dev/null