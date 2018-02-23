# Stage 1
FROM ubuntu:artful as base

WORKDIR /vex
COPY ./src /vex/src
COPY ./include /vex/include

RUN apt update && apt install -y clang \
    && mkdir -p build \
    && clang -Wall -I include -c src/http.c -o build/http.o \
    && clang -Wall -I include -c src/utils.c -o build/utils.o \
    && clang -O2 -std=c99 -Wall -I include -lm -pthread build/utils.o build/http.o -o build/vex-server src/server.c

# Stage 2
FROM ubuntu:artful

ENV DOMAIN bleenco.space
ENV PORT 1234

ENV TINI_VERSION v0.16.1

LABEL maintainer="Jan Kuri <jan@bleenco.com>"

WORKDIR /vex

ADD https://github.com/krallin/tini/releases/download/${TINI_VERSION}/tini /sbin/tini
RUN chmod +x /sbin/tini

COPY --from=base /vex/build/vex-server /usr/bin/vex-server

EXPOSE 1234

ENTRYPOINT ["/sbin/tini", "--"]
CMD /usr/bin/vex-server -d $DOMAIN -p $PORT
