FROM ubuntu:latest

ARG DEBIAN_FRONTEND=noninteractive

RUN apt update
RUN apt install -y --no-install-recommends \
  git python3 g++ cmake make ninja-build zlib1g-dev

COPY docker/buildClangir.sh ./

ARG CLANGIR_REPOSITORY=$CLANGIR_REPOSITORY
ARG CLANGIR_VERSION=$CLANGIR_VERSION

RUN chmod +x buildClangir.sh
RUN ./buildClangir.sh
