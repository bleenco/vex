package vexserver

import (
	"errors"
	"log"
	"net"
	"net/http"
	"sync"
)

// HTTPServer extends net/http server and
// adds graceful shutdowns
type HTTPServer struct {
	*http.Server
	listener  net.Listener
	running   chan error
	isRunning bool
	closer    sync.Once
}

// NewHTTPServer creates a new HTTPServer instance
func NewHTTPServer() *HTTPServer {
	return &HTTPServer{
		Server:   &http.Server{},
		listener: nil,
		running:  make(chan error, 1),
	}
}

// GoListenAndServe starts HTTPServer instance and listens on
// specified addr
func (h *HTTPServer) GoListenAndServe(addr string, handler http.Handler) error {
	l, err := net.Listen("tcp", addr)
	if err != nil {
		return err
	}
	h.isRunning = true
	h.Handler = handler
	h.listener = l

	log.Printf("HTTP server listening on %s...\n", addr)

	go func() {
		h.closeWith(h.Serve(l))
	}()
	return nil
}

func (h *HTTPServer) closeWith(err error) {
	if !h.isRunning {
		return
	}
	h.isRunning = false
	h.running <- err
}

// Close closes the HTTPServer instance
func (h *HTTPServer) Close() error {
	h.closeWith(nil)
	return h.listener.Close()
}

// Wait waits for server to be stopped
func (h *HTTPServer) Wait() error {
	if !h.isRunning {
		return errors.New("already closed")
	}
	return <-h.running
}
