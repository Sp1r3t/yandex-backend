FROM ubuntu:22.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential \
    cmake \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Conan 1.x is required by this project (CMake uses conanbuildinfo.cmake).
# 1.52.0 is too old for the current boost/1.78.0 recipe, so use a newer 1.x.
RUN pip3 install conan==1.59.0

WORKDIR /app
COPY . .

RUN conan profile new default --detect --force
RUN conan profile update settings.compiler.libcxx=libstdc++11 default

RUN mkdir -p build
RUN conan install . -if build --build=missing -s build_type=Release -s compiler.libcxx=libstdc++11
RUN cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build -j

ENTRYPOINT ["./build/bin/game_server"]
CMD ["data/config.json", "static"]
