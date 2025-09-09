#!/usr/bin/env bash

Version=$1

if [[ ! -d mybuild ]]; then
  mkdir mybuild
else
rm -rf mybuild/CMake*
fi

source ${GEODELITY_DIR}/etc/env.sh

pushd mybuild

export ARROW_DIR=/usr/local/arrow

if [[ -z ${GEODELITY_DIR+x} ]]; then
  export GEODELITY_DIR=/home/taohe/Documents/geodelity
fi

if [[ -z ${MODULECONFIG_DIR+x} ]]; then
  export MODULECONFIG_DIR=${GEODELITY_DIR}/lib/moduleconfig
fi

if [[ -z ${ARROWSTORE_DIR+x} ]]; then
  export ARROWSTORE_DIR=${GEODELITY_DIR}/lib/arrowstore
fi

if [[ -z ${GEODATAFLOW_DIR+x} ]]; then
  export GEODATAFLOW_DIR=${GEODELITY_DIR}/lib/geodataflow
fi

if [[ -z ${nlohmann_json_DIR+x} ]]; then
  export nlohmann_json_DIR=${GEODELITY_DIR}/thirdparty/nlohmann
fi

if [[ -z ${VDSSTORE_DIR+x} ]]; then
  export VDSSTORE_DIR=${GEODELITY_DIR}/lib/vdsstore
fi

if [[ -z ${GDLOGGER_DIR+x} ]]; then
  export GDLOGGER_DIR=${GEODELITY_DIR}/lib/gdlogger
fi

if [[ -z ${quill_DIR+x} ]]; then
  export quill_DIR=${GEODELITY_DIR}/thirdparty/quill
fi

if [[ -z ${cpptrace_DIR+x} ]]; then
  export cpptrace_DIR=${GEODELITY_DIR}/thirdparty/cpptrace
fi

if [[ -z ${libdwarf_DIR+x} ]]; then
  export libdwarf_DIR=${GEODELITY_DIR}/thirdparty/cpptrace
fi

cmake -GNinja \
  -DCMAKE_INSTALL_PREFIX=${GEODELITY_DIR}/module/mute/$Version \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DUSE_SHARED_OPENVDS=OFF \
  ..

ninja -j1

# Copy the compilation database to the source
# code directory for YCM
cp compile_commands.json ../
popd