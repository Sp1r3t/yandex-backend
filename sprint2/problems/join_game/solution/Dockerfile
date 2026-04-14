FROM gcc:13 AS builder

RUN apt-get update && apt-get install -y \
    cmake \
    python3 \
    python3-pip \
    git \
    make \
    ninja-build \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install --break-system-packages --no-cache-dir "conan>=2.0,<3.0"

WORKDIR /app

COPY . .

RUN conan profile detect --force
RUN conan install . --build=missing -s build_type=Release
RUN cmake --preset conan-release
RUN cmake --build --preset conan-release -j

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/build/Release/game_server /app/game_server

ENTRYPOINT ["/app/game_server"]
