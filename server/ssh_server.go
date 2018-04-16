package vexserver

import (
	"crypto/rand"
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
	domain    string
}

type Client struct {
	ID string
	net.Conn
	*ssh.ServerConn
	Listeners   map[string]net.Listener
	Addr        string
	Port        uint32
	ListenMutex *sync.Mutex
}

func NewSSHServer() *SSHServer {
	return &SSHServer{
		listener: nil,
		config: &ssh.ServerConfig{
			NoClientAuth: true,
		},
		running: make(chan error, 1),
		clients: make(map[string]Client),
	}
}

func (s *SSHServer) Start(addr string, privateKey string, domain string) error {
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
		s.closeWith(s.listen(addr, domain))
	}()
	return nil
}

func (s *SSHServer) listen(addr string, domain string) error {
	listener, err := net.Listen("tcp", addr)
	if err != nil {
		log.Fatalf("Failed to listen on %s (%s)", addr, err)
	}
	s.listener = listener
	s.domain = domain

	log.Printf("SSH server listening on %s, generating urls on *.%s ...\n", addr, domain)

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

		client := &Client{randID(), tcpConn, sshConn, make(map[string]net.Listener), "", 0, &sync.Mutex{}}
		log.Printf("New SSH connection from %s (%s)", sshConn.RemoteAddr(), sshConn.ClientVersion())

		go func() {
			err := client.ServerConn.Wait()
			client.ListenMutex.Lock()
			log.Printf("[%s] SSH connection closed: %s", client.ID, err)

			for bind, listener := range client.Listeners {
				log.Printf("[%s] Closing listener bound to %s", client.ID, bind)
				listener.Close()
			}
		}()

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

func (s *SSHServer) handleChannels(client *Client, chans <-chan ssh.NewChannel) {
	for newChannel := range chans {
		channelConn, _, err := newChannel.Accept()
		if err != nil {
			log.Printf("Could not accept channel (%s)", err)
			return
		}

		generatedURL := "http://" + client.ID + "." + s.domain
		io.WriteString(channelConn, "[vexd] Generated URL: "+generatedURL)
	}
}

func (s *SSHServer) handleRequests(client *Client, reqs <-chan *ssh.Request) {
	for req := range reqs {
		client.Conn.SetDeadline(time.Now().Add(2 * time.Minute))

		log.Printf("[%s] Out of band request: %v %v", client.ID, req.Type, req.WantReply)

		if req.Type == "tcpip-forward" {
			client.ListenMutex.Lock()

			listener, bindinfo, err := handleForward(client, req)
			if err != nil {
				fmt.Printf("[%s] Error, disconnecting ...\n", client.ID)
				client.ListenMutex.Unlock()
				client.Conn.Close()
			}

			client.Addr = bindinfo.Addr
			client.Port = bindinfo.Port
			client.Listeners[bindinfo.Bound] = listener
			s.clients[client.ID] = *client
			client.ListenMutex.Unlock()

			go handleListener(client, bindinfo, listener)
			continue
			// client.Conn.Close()
		} else {
			req.Reply(false, []byte{})
		}
	}
}

func handleListener(client *Client, bindinfo *bindInfo, listener net.Listener) {
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

	go handleForwardTCPIPTransfer(c, lconn)
}

func handleForwardTCPIPTransfer(c ssh.Channel, lconn net.Conn) {
	defer lconn.Close()
	done := make(chan bool)

	go func() {
		io.Copy(c, lconn)
		done <- true
	}()

	go func() {
		io.Copy(lconn, c)
		done <- true
	}()

	<-done
}

func handleForward(client *Client, req *ssh.Request) (net.Listener, *bindInfo, error) {
	var payload tcpIPForwardPayload
	if err := ssh.Unmarshal(req.Payload, &payload); err != nil {
		log.Printf("[%s] Unable to unmarshal payload", client.ID)
		req.Reply(false, []byte{})
		return nil, nil, fmt.Errorf("Unable to parse payload")
	}

	log.Printf("[%s] Request: %s %v %v", client.ID, req.Type, req.WantReply, payload)

	bind := fmt.Sprintf("%s:%d", payload.Addr, payload.Port)
	ln, err := net.Listen("tcp", bind)
	if err != nil {
		log.Printf("[%s] Listen failed for %s", client.ID, bind)
		req.Reply(false, []byte{})
		return nil, nil, err
	}

	log.Printf("[%s] Listening on %s:%d", client.ID, payload.Addr, payload.Port)

	reply := tcpIPForwardPayloadReply{payload.Port}
	req.Reply(true, ssh.Marshal(&reply))

	return ln, &bindInfo{bind, payload.Port, payload.Addr}, nil
}

func randID() string {
	b := make([]byte, 4)
	rand.Read(b)
	return fmt.Sprintf("%x", b)
}
