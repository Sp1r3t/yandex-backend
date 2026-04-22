FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV CONAN_NON_INTERACTIVE=1

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    python3-pip \
    gcc \
    g++ \
    && pip3 install --no-cache-dir conan==1.59.0 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

RUN conan profile new default --detect --force \
    && conan profile update settings.compiler.libcxx=libstdc++11 default \
    && conan profile update settings.compiler=gcc default \
    && conan profile update settings.compiler.version=11 default

COPY conanfile.* ./
RUN mkdir -p build \
    && conan install . -if build --build=missing \
        -s compiler=gcc \
        -s compiler.version=11 \
        -s compiler.libcxx=libstdc++11

COPY . .

RUN cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

RUN cmake --build build --verbose -j$(nproc --ignore=1)

ENTRYPOINT ["/app/build/game_server"]
CMD ["-c", "data/config.json", "-w", "static"]
