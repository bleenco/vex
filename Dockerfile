# stage 1 build
FROM golang:1.10-alpine AS build

WORKDIR /go/src/github.com/bleenco/vex

RUN apk --no-cache add git

COPY . /go/src/github.com/bleenco/vex/

RUN go get -u github.com/golang/dep/cmd/dep && dep ensure
RUN CGO_ENABLED=0 go build -o build/vexd cmd/vexd/*.go

# stage 2 image
FROM scratch

COPY --from=build /go/src/github.com/bleenco/vex/build/vexd /usr/bin/vexd

ENTRYPOINT [ "/usr/bin/vexd" ]

EXPOSE 5223 80 443
