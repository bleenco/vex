package vexserver

import (
	"errors"
	"fmt"
	"io/ioutil"
	"log"
	"net"
	"sync"

	"golang.org/x/crypto/ssh"
)

type SSHServer struct {
	listener  net.Listener
	config    *ssh.ServerConfig
	running   chan error
	isRunning bool
	closer    sync.Once
}

func NewSSHServer() *SSHServer {
	return &SSHServer{
		listener: nil,
		config: &ssh.ServerConfig{
			PasswordCallback: func(c ssh.ConnMetadata, pass []byte) (*ssh.Permissions, error) {
				if c.User() == "foo" && string(pass) == "bar" {
					return nil, nil
				}
				return nil, fmt.Errorf("password rejected for %q", c.User())
			},
		},
		running: make(chan error, 1),
	}
}

func (s *SSHServer) Start(addr string, privateKey string) error {
	privateBytes, err := ioutil.ReadFile(privateKey)
	if err != nil {
		log.Fatalf("Failed to load private key %s\n", privateKey)
	}

	private, err := ssh.ParsePrivateKey(privateBytes)
	if err != nil {
		log.Fatalf("Failed to parse private key")
	}

	s.config.AddHostKey(private)

	go func() {
		s.closeWith(s.listen(addr))
	}()
	return nil
}

func (s *SSHServer) listen(addr string) error {
	listener, err := net.Listen("tcp", addr)
	if err != nil {
		log.Fatalf("Failed to listen on %s (%s)", addr, err)
	}
	s.listener = listener

	log.Printf("SSH server listening on %s...\n", addr)

	for {
		tcpConn, err := s.listener.Accept()
		if err != nil {
			log.Printf("Failed to accept incoming connection (%s)\n", err)
			continue
		}

		sshConn, chans, reqs, err := ssh.NewServerConn(tcpConn, s.config)
		if err != nil {
			log.Printf("Failed to handshake (%s)\n", err)
			continue
		}

		log.Printf("New SSH connection from %s (%s)", sshConn.RemoteAddr(), sshConn.ClientVersion())
		go ssh.DiscardRequests(reqs)
		go handleChannels(chans)
	}
}

func (s *SSHServer) closeWith(err error) {
	if !s.isRunning {
		return
	}
	s.isRunning = false
	s.running <- err
}

func (s *SSHServer) Close() error {
	s.closeWith(nil)
	return s.listener.Close()
}

func (s *SSHServer) Wait() error {
	if !s.isRunning {
		return errors.New("already closed")
	}
	return <-s.running
}

func handleChannels(chans <-chan ssh.NewChannel) {
	for newChannel := range chans {
		go handleChannel(newChannel)
	}
}

func handleChannel(newChannel ssh.NewChannel) {
	if t := newChannel.ChannelType(); t != "session" {
		newChannel.Reject(ssh.UnknownChannelType, fmt.Sprintf("unknown channel type: %s", t))
		return
	}

	return
}
