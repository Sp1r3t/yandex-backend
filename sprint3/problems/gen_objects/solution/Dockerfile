FROM gcc:11.3 as build

RUN apt-get update && \
    apt-get install -y \
      python3-pip \
      cmake \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install conan==1.59.0

# Install dependencies first (cached layer)
COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan profile new default --detect --force && \
    conan profile update settings.compiler.libcxx=libstdc++11 default && \
    conan install .. --build=missing -s build_type=Release

# Copy source
COPY . /app

# Build
RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . -j

CMD ["/app/build/game_server", "/app/data/config.json", "/app/static"]
