FROM gcc:11.3 as build

RUN apt-get update && \
    apt-get install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.59.0

COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing -s compiler.libcxx=libstdc++11 -s build_type=Release

COPY ./src /app/src
COPY ./data /app/data
COPY CMakeLists.txt /app/
RUN mkdir -p /app/static

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . && \
    cp /app/build/game_server /app/game_server

CMD ["/app/game_server", "--config-file", "/app/data/config.json", "--www-root", "/app/static/"]
