FROM debian@sha256:67f3931ad8cb1967beec602d8c0506af1e37e8d73c2a0b38b181ec5d8560d395

# Metadata
LABEL maintainer="Simon Tollec <simon.tollec@cea.fr>"
LABEL co-maintainer="Damien Couroussé <damien.courousse@cea.fr>"

# Install required packages and clean up
RUN apt-get update \
    && apt-get install -y \
      g++ \
      make \
      cmake \
      git \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Create and switch to the non-root user
RUN useradd -m dockeruser
USER dockeruser

# Set working directory
VOLUME /artifact
WORKDIR /artifact
# COPY . /artifact

# Ensure submodules are initialized and build the `k-partitions` tool using Cmake
SHELL ["/bin/bash", "-c"]
CMD git submodule update --init --recursive src/cxxsat src/json \
    && rm -rf build \
    && cmake src -Bbuild && cmake --build build \
    && /bin/bash
