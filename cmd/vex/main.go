package main

import (
	"flag"
	"fmt"
	"io"
	"log"
	"math/rand"
	"net"
	"os"
	"os/signal"
	"time"

	"golang.org/x/crypto/ssh"
)

type Endpoint struct {
	Host string
	Port int
}

func (endpoint *Endpoint) String() string {
	return fmt.Sprintf("%s:%d", endpoint.Host, endpoint.Port)
}

var help = `
  Usage: vex [options]

  Options:

  -s, SSH server remote host (default: bleenco.space)

  -p, SSH server remote port (default: 2200)

  -ls, Local HTTP server host (default: localhost)

  -lp, Local HTTP server port (default: 7500)

  Read more:
    https://github.com/bleenco/vex
`

var (
	remoteServer = flag.String("s", "bleenco.space", "")
	remotePort   = flag.Int("p", 2200, "")
	localServer  = flag.String("ls", "localhost", "")
	localPort    = flag.Int("lp", 7500, "")
)

func main() {
	flag.Usage = func() {
		fmt.Print(help)
		os.Exit(1)
	}
	flag.Parse()

	// local service to be forwarded
	var localEndpoint = Endpoint{
		Host: *localServer,
		Port: *localPort,
	}

	// remote SSH server
	var serverEndpoint = Endpoint{
		Host: *remoteServer,
		Port: *remotePort,
	}

	// remote forwarding port (on remote SSH server network)
	var remoteEndpoint = Endpoint{
		Host: "localhost",
		Port: randomPort(11000, 65000),
	}

	sshConfig := &ssh.ClientConfig{
		HostKeyCallback: ssh.InsecureIgnoreHostKey(),
	}

	// Connect to SSH remote server using serverEndpoint
	serverConn, err := ssh.Dial("tcp", serverEndpoint.String(), sshConfig)
	if err != nil {
		log.Fatalln(fmt.Printf("Dial INTO remote server error: %s", err))
	}

	go func() {
		session, err := serverConn.NewSession()
		if err != nil {
			log.Fatalln(fmt.Printf("Cannot create session error: %s", err))
		}

		stdout, err := session.StdoutPipe()
		if err != nil {
			log.Fatalln(fmt.Printf("Unable to setup stdout for session: %s", err))
		}

		go io.Copy(os.Stdout, stdout)
	}()

	// Listen on remote server port
	listener, err := serverConn.Listen("tcp", remoteEndpoint.String())
	if err != nil {
		log.Fatalln(fmt.Printf("Listen open port ON remote server error: %s", err))
	}
	defer listener.Close()

	c := make(chan os.Signal, 1)
	signal.Notify(c, os.Interrupt)

	go func() {
		for {
			// Open a (local) connection to localEndpoint whose content will be forwarded so serverEndpoint
			local, err := net.Dial("tcp", localEndpoint.String())
			if err != nil {
				log.Fatalln(fmt.Printf("Dial INTO local service error: %s", err))
			}

			client, err := listener.Accept()
			if err != nil {
				log.Fatalln(err)
			}

			go handleClient(client, local)
		}
	}()

	<-c
}

func handleClient(client net.Conn, remote net.Conn) {
	defer client.Close()
	chDone := make(chan bool)

	go func() {
		_, err := io.Copy(client, remote)
		if err != nil {
			log.Println(fmt.Sprintf("error while copy remote->local: %s", err))
		}
		chDone <- true
	}()

	go func() {
		_, err := io.Copy(remote, client)
		if err != nil {
			log.Println(fmt.Sprintf("error while copy local->remote: %s", err))
		}
		chDone <- true
	}()

	<-chDone
}

func randomPort(min, max int) int {
	rand.Seed(time.Now().Unix())
	return rand.Intn(max-min) + min
}
