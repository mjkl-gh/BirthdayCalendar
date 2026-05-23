FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    DEBIAN_FRONTEND=${DEBIAN_FRONTEND} apt-get upgrade -y && \
    DEBIAN_FRONTEND=${DEBIAN_FRONTEND} apt-get install -y --no-install-recommends \
    build-essential \
    clang \
    cmake \
    libcurl4-openssl-dev libssl-dev \
    pkg-config git curl \
    locales \
    sudo \
    python3-minimal \
    && rm -rf /var/lib/apt/lists/*

RUN echo "ubuntu ALL=(ALL) NOPASSWD:ALL" >/etc/sudoers.d/90-ubuntu-nopasswd \
    && chmod 0440 /etc/sudoers.d/90-ubuntu-nopasswd

ARG NODE_MAJOR=22
RUN curl -fsSL https://deb.nodesource.com/setup_${NODE_MAJOR}.x | bash - \
    && apt-get update \
    && DEBIAN_FRONTEND=${DEBIAN_FRONTEND} apt-get install -y --no-install-recommends nodejs npm \
    && rm -rf /var/lib/apt/lists/*

RUN locale-gen en_US.UTF-8

ENV LANG=en_US.UTF-8
ENV LANGUAGE=en_US:en
ENV LC_ALL=en_US.UTF-8
