FROM emscripten/emsdk
LABEL Name=ALPACA Version=0.0.1
RUN sudo apt remove --purge --auto-remove cmake -y
RUN sudo apt update -y
RUN sudo apt upgrade -y
RUN sudo apt install libvorbis-dev libogg-dev wget python3 -y
RUN wget --quiet "https://cmake.org/files/v3.19/cmake-3.19.1-Linux-x86_64.sh"
RUN sudo mkdir /opt/cmake
RUN sudo sh cmake-3.19.1-Linux-x86_64.sh --prefix=/opt/cmake --skip-license
RUN sudo ln -s /opt/cmake/bin/cmake /usr/local/bin/cmake

# Run the following manuel to get a build
# RUN mkdir buildem
# RUN cd buildem
# RUN cp -r data buildem/data
# RUN emcmake cmake -DJNGL_VIDEO=0 -DCMAKE_BUILD_TYPE=Release .. && make -j8 pac
