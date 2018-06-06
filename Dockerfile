# Stage 1 - build program
FROM golang:1.10-alpine3.7 as program

RUN apk --no-cache add curl git && curl https://raw.githubusercontent.com/golang/dep/master/install.sh | sh
RUN mkdir -p $GOPATH/src/github.com/bleenco/vex

COPY . /go/src/github.com/bleenco/vex

WORKDIR /go/src/github.com/bleenco/vex/

RUN dep ensure && CGO_ENABLED=0 go build -o vexd cmd/vexd/main.go

# Stage 2 - image
FROM scratch

ENV VEX_DOMAIN bleenco.space

COPY --from=program /go/src/github.com/bleenco/vex/vexd /vexd

EXPOSE 2200 2000

CMD [ "/vexd" ]
