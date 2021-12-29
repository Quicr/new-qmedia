FROM ubuntu:latest as builder

ENV TZ=America/Vancouver
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update
RUN apt-get upgrade -y
RUN apt-get install -y \
    apt-utils \
    build-essential \
    cmake \
    curl \
    emacs-nox \
    git \
    pkg-config \
    tar \
    tcsh \
    unzip \
    zip

RUN apt-get install -y \
    python3 \
    nasm 

RUN apt-get install -y libcurl4-openssl-dev


WORKDIR /tmp
RUN git clone https://github.com/microsoft/vcpkg
WORKDIR /tmp/vcpkg
RUN ./bootstrap-vcpkg.sh -disableMetrics
RUN ./vcpkg install opus
RUN ./vcpkg install protobuf
RUN ./vcpkg install openssl
RUN ./vcpkg install doctest
RUN ./vcpkg install dav1d
RUN ./vcpkg install libsamplerate
RUN ./vcpkg install portaudio
RUN ./vcpkg install openh264
#RUN ./vcpkg integrate install

WORKDIR /src/qmedia
COPY ./CMakeLists.txt	./CMakeLists.txt
COPY ./cmake ./cmake
COPY ./proto ./proto
COPY ./include ./include
COPY ./cmd ./cmd 
COPY ./test ./test
COPY ./src ./src 

RUN mkdir -p /tmp/build
WORKDIR /tmp/build

#CMD /bin/tcsh

RUN cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DCMAKE_TOOLCHAIN_FILE=/tmp/vcpkg/scripts/buildsystems/vcpkg.cmake /src/qmedia
RUN make VERBOSE=1 -j 8 echoSFU

RUN mkdir -p /src/bin

RUN cp cmd/echoSFU /src/bin/
RUN ldd cmd/echoSFU | grep "=> /" | awk '{print $3}' | xargs -I '{}' cp -v '{}' /src/bin

FROM ubuntu:latest

COPY --from=builder /src/bin/* /sfu/

ENV LD_LIBRARY_PATH /sfu

WORKDIR /sfu

EXPOSE 5004

CMD /sfu/echoSFU r
