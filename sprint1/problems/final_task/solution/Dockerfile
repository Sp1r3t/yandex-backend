FROM gcc:11.3 AS build

WORKDIR /app

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        cmake \
        python3-pip \
    && pip3 install --no-cache-dir "conan==1.*" \
    && rm -rf /var/lib/apt/lists/*

COPY . .

RUN mkdir -p build && \
    cd build && \
    conan install .. --build=missing && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . --config Release -j"$(nproc)"

FROM ubuntu:22.04

WORKDIR /app

RUN apt-get update && \
    apt-get install -y --no-install-recommends libstdc++6 && \
    rm -rf /var/lib/apt/lists/*

COPY --from=build /app/build/bin/game_server /app/game_server
COPY --from=build /app/data/config.json /app/data/config.json

EXPOSE 8080

ENTRYPOINT ["/app/game_server", "/app/data/config.json"]