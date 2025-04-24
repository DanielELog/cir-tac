FROM ubuntu:latest

ARG DEBIAN_FRONTEND=noninteractive

RUN apt update
RUN apt install -y --no-install-recommends \
  git python3 g++ cmake make ninja-build zlib1g-dev

COPY buildClangir.sh ./

RUN chmod +x buildClangir.sh
RUN ./buildClangir.sh
