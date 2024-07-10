FROM debian@sha256:67f3931ad8cb1967beec602d8c0506af1e37e8d73c2a0b38b181ec5d8560d395

# Metadata
LABEL maintainer="Simon Tollec <simon.tollec@cea.fr>"
LABEL co-maintainer="Damien Couroussé <damien.courousse@cea.fr>"

# Install any required packages
RUN apt-get update \
    && apt-get install -y \
      g++ \
      make \
      cmake \
      git \
    && rm -rf /var/lib/apt/lists/*

# Switch to the non-root user
RUN useradd -m dockeruser
USER dockeruser

# Set working directory
VOLUME /artifact
WORKDIR /artifact
# COPY . /artifact

# Make sure the repository have been clone with cxxsat and json submodules
# and build the `k-partitions` tool using Cmake
CMD git submodule update --init --recursive src/cxxsat src/json \
    && cmake src -Bbuild && cmake --build build \
    && /bin/bash
