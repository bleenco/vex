package vexserver

import (
	"errors"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"net"
	"sync"
	"time"

	"golang.org/x/crypto/ssh"
)

type SSHServer struct {
	listener  net.Listener
	config    *ssh.ServerConfig
	running   chan error
	isRunning bool
	closer    sync.Once
	clients   map[string]Client
}

type Client struct {
	ID string
	net.Conn
	*ssh.ServerConn
	Listeners map[string]net.Listener
	Port      uint32
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
		clients: make(map[string]Client),
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

		client := &Client{"jan", tcpConn, sshConn, make(map[string]net.Listener), 0}
		log.Printf("New SSH connection from %s (%s)", sshConn.RemoteAddr(), sshConn.ClientVersion())

		go s.handleRequests(client, reqs)
		go s.handleChannels(client, chans)
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

func (s *SSHServer) handleRequests(client *Client, reqs <-chan *ssh.Request) {
	for req := range reqs {
		client.Conn.SetDeadline(time.Now().Add(5 * time.Minute))

		log.Printf("[%s] Out of band request: %v %v", client.ID, req.Type, req.WantReply)

		if req.Type == "tcpip-forward" {
			req.Reply(true, []byte{})

			listener, bindinfo, err := handleForward(client, req)
			if err != nil {
				fmt.Printf("[%s] Error, disconnecting ...\n", client.ID)
				client.Conn.Close()
			}

			client.Port = bindinfo.Port
			client.Listeners[bindinfo.Bound] = listener
			s.clients[client.ID] = *client

			go handleListener(client, bindinfo, listener)
			continue
			// client.Conn.Close()
		} else {
			req.Reply(false, []byte{})
		}
	}
}

func handleListener(client *Client, bindinfo *bindInfo, listener net.Listener) {
	// Start listening for connections
	for {
		lconn, err := listener.Accept()
		if err != nil {
			neterr := err.(net.Error)
			if neterr.Timeout() {
				log.Printf("[%s] Accept failed with timeout: %s", client.ID, err)
				continue
			}
			if neterr.Temporary() {
				log.Printf("[%s] Accept failed with temporary: %s", client.ID, err)
				continue
			}

			break
		}

		go handleForwardTCPIP(client, bindinfo, lconn)
	}
}

func handleForwardTCPIP(client *Client, bindinfo *bindInfo, lconn net.Conn) {
	remotetcpaddr := lconn.RemoteAddr().(*net.TCPAddr)
	raddr := remotetcpaddr.IP.String()
	rport := uint32(remotetcpaddr.Port)

	payload := forwardedTCPPayload{bindinfo.Addr, bindinfo.Port, raddr, uint32(rport)}
	mpayload := ssh.Marshal(&payload)

	// Open channel with client
	c, requests, err := client.ServerConn.OpenChannel("forwarded-tcpip", mpayload)
	if err != nil {
		log.Printf("[%s] Unable to get channel: %s. Hanging up requesting party!", client.ID, err)
		lconn.Close()
		return
	}
	log.Printf("[%s] Channel opened for client", client.ID)
	go ssh.DiscardRequests(requests)

	go io.Copy(c, lconn)
	go io.Copy(lconn, c)
}

func (s *SSHServer) handleChannels(client *Client, chans <-chan ssh.NewChannel) {
	for newChannel := range chans {
		go handleChannel(client, newChannel)
	}
}

func handleChannel(client *Client, newChannel ssh.NewChannel) {
	log.Printf("[%s] Channel type: %v", client.ID, newChannel.ChannelType())

	// if t := newChannel.ChannelType(); t != "session" {
	// 	newChannel.Reject(ssh.UnknownChannelType, fmt.Sprintf("unknown channel type: %s", t))
	// 	return
	// }
}

func handleForward(client *Client, req *ssh.Request) (net.Listener, *bindInfo, error) {
	var payload tcpIPForwardPayload
	if err := ssh.Unmarshal(req.Payload, &payload); err != nil {
		log.Printf("[%s] Unable to unmarshal payload", client.ID)
		req.Reply(false, []byte{})
		return nil, nil, fmt.Errorf("Unable to parse payload")
	}

	log.Printf("[%s] Request: %s %v %v", client.ID, req.Type, req.WantReply, payload)
	log.Printf("[%s] Request to listen on %s:%d", client.ID, "localhost", payload.Port)

	bind := fmt.Sprintf("%s:%d", "localhost", payload.Port)
	log.Printf(bind)
	ln, err := net.Listen("tcp", bind)
	if err != nil {
		log.Printf("[%s] Listen failed for %s", client.ID, bind)
		req.Reply(false, []byte{})
		return nil, nil, err
	}

	reply := tcpIPForwardPayloadReply{payload.Port}
	req.Reply(true, ssh.Marshal(&reply))

	return ln, &bindInfo{bind, payload.Port, payload.Addr}, nil
}
