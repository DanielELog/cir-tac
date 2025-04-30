#!/usr/bin/env bash

set -euo pipefail

CLANGIR_REPOSITORY="${CLANGIR_REPOSITORY:=https://github.com/explyt/clangir.git}"
CLANGIR_VERSION="${CLANGIR_VERSION:=21.03.2025-rc}"

function checkEnvVar() {
  if [ -z ${!1+x} ]; then
    >&2 echo "$1 is unset. Aborting...";
    exit 1
  fi
  echo "$1 is set to ${!1}"
}

function gitCloneAndCheckout() {
  GIT_ERROR_CODE=0
  GIT_OUTPUT=$(git clone -c http.sslVerify=false "$1" --branch "$2" 2>&1) || GIT_ERROR_CODE=$?
  if [ "$GIT_ERROR_CODE" -ne 0 ]; then
    GIT_ERROR_CODE=0
    GIT_OUTPUT=$(git clone -c http.sslVerify=false  "$1" 2>&1) || GIT_ERROR_CODE=$?
  fi

  if [ "$GIT_ERROR_CODE" -eq 0 ]; then
    DESTINATION_DIR=$(echo "$GIT_OUTPUT" | sed -nr "s/Cloning into '([^\.']*)'\.\.\./\1/p")
  else
    DESTINATION_DIR=$(echo "$GIT_OUTPUT" | sed -nr "s/fatal: destination path '([^\.']*)' already exists and is not an empty directory\./\1/p")
  fi

  if [ -z "${DESTINATION_DIR}" ]; then
    >&2 echo "Directory for repository $1 can not be determined. Aborting...";
    exit 1
  fi

  # -> 'repository'
  pushd >/dev/null "$DESTINATION_DIR" || exit 2

  git checkout "$2" --quiet
  GIT_SSL_NO_VERIFY=1 git submodule update --init --recursive --quiet

  # <- 'repository'
  popd >/dev/null || exit 2
  echo "$DESTINATION_DIR"
}

function buildClangir() {
  echo "Compiling clangir..."
  CLANGIR_SOURCES_PATH=$(gitCloneAndCheckout "$CLANGIR_REPOSITORY" "$CLANGIR_VERSION")
  export CLANGIR_SOURCES_PATH
  echo "Successfully cloned clangir!"

  # -> clangir/llvm/build
  mkdir -p "$CLANGIR_SOURCES_PATH"/llvm/build
  pushd >/dev/null "$CLANGIR_SOURCES_PATH"/llvm/build || exit 2

  cmake -DLLVM_ENABLE_PROJECTS="clang;mlir" \
        -DCLANG_ENABLE_CIR=ON \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=$C_COMPILER \
        -DCMAKE_CXX_COMPILER=$CXX_COMPILER \
        -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON \
        -DCMAKE_SYSTEM_NAME=Darwin \
        -DCMAKE_OSX_SYSROOT="/osxcross/SDK/MacOSX14.5.sdk" \
        -DCMAKE_OSX_ARCHITECTURES="arm64" \
        -GNinja ..
  ninja -j16

  # <- clangir/llvm/build
  popd >/dev/null || exit 2

  CLANG_BUILD_DIR=$(realpath "$CLANGIR_SOURCES_PATH"/llvm/build)
  export CLANG_BUILD_DIR
}

# Check existence of environment variables
checkEnvVar CLANGIR_REPOSITORY
checkEnvVar CLANGIR_VERSION
checkEnvVar C_COMPILER
checkEnvVar CXX_COMPILER

# Install clangir
if [ -z ${CLANG_BUILD_DIR+x} ]; then
    buildClangir
fi

# <- compilersSources
popd >/dev/null || exit 2

rm -rf $CLANGIR_SOURCES_PATH
