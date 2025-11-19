FROM ubuntu:20.04
ENV DEBIAN_FRONTEND=noninteractive


RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    gdb \
    git \
    clang-12 \
    llvm-12 \
    llvm-12-dev \
    lldb-12 \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# تنظیمات پیش‌فرض
RUN update-alternatives --install /usr/bin/cc cc /usr/bin/clang-12 100
RUN update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++-12 100
# لینک کردن ابزارها برای استفاده راحت‌تر
RUN ln -s /usr/bin/clang-12 /usr/bin/clang && \
    ln -s /usr/bin/llc-12 /usr/bin/llc
