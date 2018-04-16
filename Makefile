OUTPUT_DIR = build
OS = "darwin freebsd linux windows"
ARCH = "amd64 arm"
OSARCH = "!darwin/arm !windows/arm"

build:
	mkdir -p ${OUTPUT_DIR}
	GOARM=5 gox -os=${OS} -arch=${ARCH} -osarch=${OSARCH} -output "${OUTPUT_DIR}/vex-{{.Dir}}-{{.OS}}_{{.Arch}}" ./cmd/server

clean:
	rm -rf ${OUTPUT_DIR}
