package main

import (
	"flag"
	"fmt"
	"os"
)

const usage1 string = `Usage: vexd [OPTIONS]
options:
`

const usage2 string = `
Example:
	vexd
	vexd -clients YMBKT3V-ESUTZ2Z-7MRILIJ-T35FHGO-D2DHO7D-FXMGSSR-V4LBSZX-BNDONQ4
	vexd -httpAddr :8080 -httpsAddr ""
`

func init() {
	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, usage1)
		flag.PrintDefaults()
		fmt.Fprintf(os.Stderr, usage2)
	}
}

// options specify arguments read command line arguments.
type options struct {
	domain     string
	httpAddr   string
	httpsAddr  string
	tunnelAddr string
	tlsCrt     string
	tlsKey     string
	rootCA     string
	clients    string
	logLevel   int
	version    bool
}

func parseArgs() *options {
	domain := flag.String("domain", "local.dev", "Root domain name")
	httpAddr := flag.String("httpAddr", ":80", "Public address for HTTP connections, empty string to disable")
	httpsAddr := flag.String("httpsAddr", ":443", "Public address listening for HTTPS connections, emptry string to disable")
	tunnelAddr := flag.String("tunnelAddr", ":5223", "Public address listening for tunnel client")
	tlsCrt := flag.String("tlsCrt", "server.crt", "Path to a TLS certificate file")
	tlsKey := flag.String("tlsKey", "server.key", "Path to a TLS key file")
	rootCA := flag.String("rootCA", "", "Path to the trusted certificate chian used for client certificate authentication, if empty any client certificate is accepted")
	clients := flag.String("clients", "", "Comma-separated list of tunnel client ids, if empty accept all clients")
	logLevel := flag.Int("log-level", 1, "Level of messages to log, 0-3")
	version := flag.Bool("version", false, "Prints vexd version")
	flag.Parse()

	return &options{
		domain:     *domain,
		httpAddr:   *httpAddr,
		httpsAddr:  *httpsAddr,
		tunnelAddr: *tunnelAddr,
		tlsCrt:     *tlsCrt,
		tlsKey:     *tlsKey,
		rootCA:     *rootCA,
		clients:    *clients,
		logLevel:   *logLevel,
		version:    *version,
	}
}
