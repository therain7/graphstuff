FROM intel/oneapi-runtime

WORKDIR /build
COPY . .

RUN git config --global user.name "example"
RUN git config --global user.email "e@example.com"

RUN make vendor
RUN make build
