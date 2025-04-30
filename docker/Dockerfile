FROM ubuntu:latest

ARG DEBIAN_FRONTEND=noninteractive

RUN apt update
RUN apt install -y --no-install-recommends \
  git python3 g++ cmake make ninja-build zlib1g-dev

COPY docker/buildClangir.sh ./

ENV C_COMPILER=gcc
ENV CXX_COMPILER=g++

RUN chmod +x buildClangir.sh
RUN ./buildClangir.sh
