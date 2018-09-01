all: build

build:
	@mkdir -p build
	@go build -o build/vex cmd/vex/*.go
	@go build -o build/vexd cmd/vexd/*.go

clean:
	@rm -rf build

certs:
	@mkdir -p build
	openssl req -x509 -nodes -newkey rsa:2048 -sha256 -keyout build/client.key -out build/client.crt
	openssl req -x509 -nodes -newkey rsa:2048 -sha256 -keyout build/server.key -out build/server.crt

.PHONY: clean build
