FROM ubuntu:22.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential \
    cmake \
    python3-pip \
    && pip3 install conan==1.59.0 \
    && rm -rf /var/lib/apt/lists/* /root/.cache

WORKDIR /app
COPY . .

RUN conan profile new default --detect --force
RUN conan profile update settings.compiler.libcxx=libstdc++11 default

RUN mkdir -p build
RUN conan install . -if build --build=missing
RUN cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build --verbose -j2  # Использует два потока

ENTRYPOINT ["./build/bin/game_server"]
CMD ["-c", "data/config.json", "-w", "static"]