ARG TAG
FROM rhrhunter/vcv-build-env:${TAG}

WORKDIR /plugin
COPY . /plugin
RUN make
