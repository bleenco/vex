all: build

build:
	@mkdir -p build
	@if [ ! -d statik ]; then make build_ui; fi
	@make build_client
	@make build_server

build_client:
	@mkdir -p build
	@go build -o build/vex cmd/vex/*.go

build_server:
	@mkdir -p build
	@go build -o build/vexd cmd/vexd/*.go

build_ui:
	@cd webui/ && npm run build
	@statik -src=./dist

clean:
	@rm -rf build statik dist

.PHONY: clean build
