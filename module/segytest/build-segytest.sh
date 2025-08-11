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
  export GEODELITY_DIR=/s0/andrew/geo/geodelity
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

# Set OpenVDS related environment variables
if [[ -z ${Openvds_DIR+x} ]]; then
  export Openvds_DIR=${GEODELITY_DIR}/thirdparty/openvds
fi

if [[ -z ${Openvds_hue_bds_objects_DIR+x} ]]; then
  export Openvds_hue_bds_objects_DIR=${GEODELITY_DIR}/thirdparty/openvds/lib
fi

if [[ -z ${aws_crt_cpp_DIR+x} ]]; then
  export aws_crt_cpp_DIR=${GEODELITY_DIR}/thirdparty/aws-crt-cpp
fi

if [[ -z ${fmt_DIR+x} ]]; then
  export fmt_DIR=${GEODELITY_DIR}/thirdparty/fmt
fi

if [[ -z ${libuv_DIR+x} ]]; then
  export libuv_DIR=${GEODELITY_DIR}/thirdparty/libuv
fi

if [[ -z ${google_cloud_ovds_DIR+x} ]]; then
  export google_cloud_ovds_DIR=${GEODELITY_DIR}/thirdparty/google_cloud_ovds
fi

if [[ -z ${absl_DIR+x} ]]; then
  export absl_DIR=${GEODELITY_DIR}/thirdparty/absl
fi

if [[ -z ${curl_DIR+x} ]]; then
  export curl_DIR=${GEODELITY_DIR}/thirdparty/curl
fi

if [[ -z ${openssl_DIR+x} ]]; then
  export openssl_DIR=${GEODELITY_DIR}/thirdparty/openssl
fi

if [[ -z ${crc32c_DIR+x} ]]; then
  export crc32c_DIR=${GEODELITY_DIR}/thirdparty/crc32c
fi

if [[ -z ${AzureSdkForCpp_DIR+x} ]]; then
  export AzureSdkForCpp_DIR=${GEODELITY_DIR}/thirdparty/azure-sdk-for-cpp
fi

if [[ -z ${LibXml2_DIR+x} ]]; then
  export LibXml2_DIR=${GEODELITY_DIR}/thirdparty/libxml2
fi

if [[ -z ${zlib_DIR+x} ]]; then
  export zlib_DIR=${GEODELITY_DIR}/thirdparty/zlib
fi

cmake -GNinja \
  -DCMAKE_INSTALL_PREFIX=${GEODELITY_DIR}/module/segytest/$Version \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DUSE_SHARED_OPENVDS=OFF \
  ..

ninja -j1

# Copy the compilation database to the source
# code directory for YCM
cp compile_commands.json ../
popd