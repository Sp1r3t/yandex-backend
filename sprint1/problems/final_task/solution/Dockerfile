FROM gcc:11.3 AS build

WORKDIR /app

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        cmake \
        python3-pip \
    && pip3 install --no-cache-dir "conan==1.*" \
    && rm -rf /var/lib/apt/lists/*

COPY conanfile.txt CMakeLists.txt ./
COPY *.cpp *.h ./
COPY config.json ./

RUN mkdir build && cd build && \
    conan install .. --build=missing && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . --config Release -j$(nproc)

FROM ubuntu:22.04 AS run

WORKDIR /app

RUN apt-get update && \
    apt-get install -y --no-install-recommends libstdc++6 && \
    rm -rf /var/lib/apt/lists/* && \
    groupadd -r www && useradd -r -g www www

COPY --from=build /app/build/bin/game_server /app/game_server
COPY --from=build /app/config.json /app/config.json

EXPOSE 8080
USER www

ENTRYPOINT ["/app/game_server", "/app/config.json"]