FROM ubuntu:22.04 AS build

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

COPY conanfile.txt ./
RUN mkdir -p build && cd /app/build \
    && conan install .. --build=missing \
        -s compiler=gcc \
        -s compiler.version=11 \
        -s compiler.libcxx=libstdc++11 \
        -s build_type=Release

COPY CMakeLists.txt ./
COPY src ./src
COPY tests ./tests

RUN cd /app/build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
    && cmake --build . -j$(nproc) \
    && cp /app/build/game_server /app/game_server

FROM ubuntu:22.04

WORKDIR /app

RUN apt-get update && apt-get install -y libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /app/game_server /app/game_server
COPY data /app/data
COPY static /app/static

ENTRYPOINT ["/app/game_server"]
CMD ["--config-file", "/app/data/config.json", "--www-root", "/app/static/"]
