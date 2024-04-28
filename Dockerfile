# Use an official Ubuntu runtime as a parent image
FROM ubuntu:latest

# Update the system and install AVR GCC toolchain, Make, Python3, and pip
RUN apt-get update && apt-get install -y \
    gcc-avr \
    binutils-avr \
    avr-libc \
    make \
    python3 \
    python3-pip

# Install Python packages using pip
RUN pip3 install intelhex

# Clean up APT when done.
RUN apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
