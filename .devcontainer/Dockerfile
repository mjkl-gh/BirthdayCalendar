FROM mcr.microsoft.com/devcontainers/base:bookworm

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    curl \
    git \
    libcurl4-openssl-dev \
    libssl-dev \
    ninja-build \
    pkg-config \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

ARG NODE_MAJOR=22
RUN curl -fsSL https://deb.nodesource.com/setup_${NODE_MAJOR}.x | bash - \
    && apt-get update \
    && apt-get install -y --no-install-recommends nodejs \
    && rm -rf /var/lib/apt/lists/*
