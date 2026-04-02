FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install conan==1.59.0

WORKDIR /app

COPY . .

RUN rm -rf build bin && \
    mkdir build && \
    cd build && \
    conan install .. --build=missing && \
    cmake .. && \
    cmake --build .

EXPOSE 8080

CMD ["/app/build/bin/game_server", "data/config.json", "static"]
