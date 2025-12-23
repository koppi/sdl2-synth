FROM ubuntu:latest

RUN apt -qq update
RUN apt -qqqy -o Dpkg::Progress-Fancy="0" -o APT::Color="0" -o Dpkg::Use-Pty="0" install git cmake make ninja-build gcc g++ libopengl-dev libgl1-mesa-dev
RUN apt -qqqy -o Dpkg::Progress-Fancy="0" -o APT::Color="0" -o Dpkg::Use-Pty="0" install libxext-dev

WORKDIR /src

COPY . .

RUN cmake --preset release
RUN cmake --build --parallel $(($(nproc) + 1)) --preset synth-release

FROM scratch
COPY --from=0 [ \
  "/src/build/release/synth", \
  "." \
]
