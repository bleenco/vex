package main

import (
	"github.com/bleenco/vex/server"
)

func main() {
	config := &vexserver.Config{
		PrivateKeyFile: "./id_rsa",
		SSHListenAddr:  "0.0.0.0:2200",
		HTTPListenAddr: "0.0.0.0:8080",
	}

	server := vexserver.NewServer(config)
	server.Run()
}
