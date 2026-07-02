FROM ethereum/cpp-build-env

COPY ./scripts/install_cmake.sh /

RUN bash /install_cmake.sh

ENV PATH="/root/.local/bin:${PATH}"

USER root

RUN curl https://dl.google.com/go/go1.12.5.linux-amd64.tar.gz > go.tar.gz \
	&& tar -C /usr/local -xvf go.tar.gz

ENV PATH="/usr/local/go/bin:${PATH}"

WORKDIR /

# install go-ethereum
RUN git clone https://github.com/Kenun99/go-ethereum.git \
	&& cd /go-ethereum  \
	&& make -j8  \
    && cp /go-ethereum/build/bin/geth /root/ 

# install hera

COPY ./latesthera /hera
# RUN git clone https://github.com/Kenun99/hera.git \
RUN cd hera \
	# && git checkout 4f169bc \
	&& git submodule update --init \
	&& cd cmake/cable  \
	&& git reset --hard 311c0599183fbc46b402f40d6e6e5033ace2f686 \
	&& cd ../../evmc \
	&& git reset --hard 354ba6f540655649e081fb455e21998186bf5576 \
	&& cd ../evm2wasm \
	&& git reset --hard 926dab7793dcf5716c8e1295172d48143b9d41a1 \
	&& cd .. \
	&& mkdir build \
	&& cd build \
	&& cmake -DBUILD_SHARED_LIBS=ON -DHERA_BINARYEN=ON .. \
	&& make -j8 \
	&& cp /hera/build/src/libhera.so /root/
	# && rm -rf /hera

RUN mkdir /LOGS
#RUN ln -s /LOGD /LOGS

ENTRYPOINT ["/root/geth"]
# ENTRYPOINT ["/bin/bash"]
