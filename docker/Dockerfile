FROM ubuntu:20.10

# Prepare directory for tools
ARG TOOLS_PATH=/opt/arm
RUN mkdir -p ${TOOLS_PATH}
WORKDIR ${TOOLS_PATH}

RUN apt-get update \
    && apt-get install -y --no-install-recommends wget make cmake stlink-tools python3 python-is-python3 bzip2 \
                                                  libncurses5 libpython2-dev python2 openocd git \
    && rm -rf /var/lib/apt/lists/*

# Install STM32 toolchain
ARG TOOLCHAIN_TARBALL_URL="https://developer.arm.com/-/media/Files/downloads/gnu-rm/10-2020q4/gcc-arm-none-eabi-10-2020-q4-major-x86_64-linux.tar.bz2"
ARG TOOLCHAIN_PATH=${TOOLS_PATH}/toolchain
RUN wget --no-check-certificate ${TOOLCHAIN_TARBALL_URL} \
        && export TOOLCHAIN_TARBALL_FILENAME=$(basename "${TOOLCHAIN_TARBALL_URL}") \
        && tar -xvf ${TOOLCHAIN_TARBALL_FILENAME} \
        && mv $(dirname `tar -tf ${TOOLCHAIN_TARBALL_FILENAME} | head -1`) ${TOOLCHAIN_PATH} \
        && rm -rf ${TOOLCHAIN_PATH}/share/doc \
        && rm ${TOOLCHAIN_TARBALL_FILENAME}

ENV PATH="${TOOLCHAIN_PATH}/bin:${PATH}"

# Change workdir
WORKDIR /mnt
