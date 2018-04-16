package main

import (
	"flag"
	"fmt"
	"os"

	"github.com/bleenco/vex/server"
)

var help = `
  Usage: vexd [options]

  Options:

  -d, Domain name that HTTP server is hosted on. It is
  used for generating subdomain IDs (defaults to the
  environment variable VEX_DOMAIN and falls back to local.net)

  -k, Path to file of a ECDSA private key. All SSH communication
  will be secured using this key (defaults to the VEX_KEY environment
  variable falls back to id_rsa)

  -s, SSH server listen address (defaults to VEX_SSH_SERVER and
  falls back to 0.0.0.0:2200)

  -http, HTTP server listen address (defaults to VEX_HTTP_SERVER and
  falls back to 0.0.0.0:2000)

  Read more:
	  https://github.com/bleenco/vex
`

var (
	domain         = flag.String("d", "", "Domain name")
	privatekeyfile = flag.String("k", "", "Private Key File")
	sshlistenaddr  = flag.String("s", "", "SSH server listen address")
	httplistenaddr = flag.String("http", "", "HTTP server listen address")
)

func main() {
	flag.Usage = func() {
		fmt.Print(help)
		os.Exit(1)
	}
	flag.Parse()

	if *domain == "" {
		*domain = os.Getenv("VEX_DOMAIN")
	}
	if *domain == "" {
		*domain = "local.net"
	}
	if *privatekeyfile == "" {
		*privatekeyfile = os.Getenv("VEX_KEY")
	}
	if *privatekeyfile == "" {
		*privatekeyfile = "id_rsa"
	}
	if *sshlistenaddr == "" {
		*sshlistenaddr = os.Getenv("VEX_SSH_SERVER")
	}
	if *sshlistenaddr == "" {
		*sshlistenaddr = "0.0.0.0:2200"
	}
	if *httplistenaddr == "" {
		*httplistenaddr = os.Getenv("VEX_HTTP_SERVER")
	}
	if *httplistenaddr == "" {
		*httplistenaddr = "0.0.0.0:2000"
	}

	config := &vexserver.Config{
		Domain:         *domain,
		PrivateKeyFile: *privatekeyfile,
		SSHListenAddr:  *sshlistenaddr,
		HTTPListenAddr: *httplistenaddr,
	}

	server := vexserver.NewServer(config)
	server.Run()
}
