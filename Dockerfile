# Stage 1
FROM ubuntu:artful as base

WORKDIR /vex
COPY ./src /vex/src
COPY ./include /vex/include

RUN apt update && apt install -y build-essential \
    && mkdir -p build \
    && cc -Wall -I include -c src/utils.c -o build/utils.o \
    && cc -O2 -std=c99 -Wall -I include -lm -pthread -static build/utils.o -o build/vex-server src/server.c

# Stage 2
FROM alpine:3.6

ENV DOMAIN bleenco.space
ENV PORT 1234

LABEL maintainer="Jan Kuri <jan@bleenco.com>"

WORKDIR /vex

RUN apk --no-cache add tini

COPY --from=base /vex/build/vex-server /usr/bin/vex-server

EXPOSE 1234

ENTRYPOINT ["/sbin/tini", "--"]
CMD /usr/bin/vex-server -d $DOMAIN -p $PORT
