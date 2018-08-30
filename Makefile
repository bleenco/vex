all: build

build:
	@mkdir -p build
	@go build -o build/vex cmd/vex/*.go
	@go build -o build/vexd cmd/vexd/*.go

clean:
	@rm -rf build

.PHONY: clean build
