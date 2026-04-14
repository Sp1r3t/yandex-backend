FROM gcc:13 AS builder

RUN apt-get update && apt-get install -y \
    cmake \
    python3 \
    python3-pip \
    git \
    make \
    ninja-build \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install --no-cache-dir "conan>=2.0,<3.0"

WORKDIR /app

COPY . .

RUN conan profile detect --force
RUN conan install . --output-folder=build --build=missing -s build_type=Release
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake
RUN cmake --build build --config Release -j

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/build/game_server /app/game_server

ENTRYPOINT ["/app/game_server"]
