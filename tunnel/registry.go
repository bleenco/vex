package tunnel

import (
	"fmt"
	"net"
	"sync"

	"github.com/bleenco/vex/id"
	"github.com/bleenco/vex/logger"
)

// RegistryItem holds information about hosts and listeners associated with a client.
type RegistryItem struct {
	Hosts     []*HostAuth
	Listeners []net.Listener
}

// HostAuth holds host and authentication info.
type HostAuth struct {
	Host string
	Auth *Auth
}

type hostInfo struct {
	identifier id.ID
	auth       *Auth
}

type registry struct {
	items  map[id.ID]*RegistryItem
	hosts  map[string]*hostInfo
	mu     sync.RWMutex
	logger *logger.Logger
}

func newRegistry(log *logger.Logger) *registry {
	if log == nil {
		log = logger.NewLogger(false)
	}

	return &registry{
		items:  make(map[id.ID]*RegistryItem),
		hosts:  make(map[string]*hostInfo),
		logger: log,
	}
}

var voidRegistryItem = &RegistryItem{}

// Subscribe allows to connect client with a given identifier.
func (r *registry) Subscribe(identifier id.ID) {
	r.mu.Lock()
	defer r.mu.Unlock()

	if _, ok := r.items[identifier]; ok {
		return
	}

	r.logger.Infof("subscribe, identifier: %s", identifier)

	r.items[identifier] = voidRegistryItem
}

// IsSubscribed returns true if the client is subscribed.
func (r *registry) IsSubscribed(identifier id.ID) bool {
	r.mu.RLock()
	defer r.mu.RUnlock()
	_, ok := r.items[identifier]
	return ok
}

// Subscriber returns client identifier assigned to given host.
func (r *registry) Subscriber(hostPort string) (id.ID, *Auth, bool) {
	r.mu.RLock()
	defer r.mu.RUnlock()

	h, ok := r.hosts[trimPort(hostPort)]
	if !ok {
		return id.ID{}, nil, false
	}

	return h.identifier, h.auth, ok
}

// Unsubscribe removes client from registry and returns it's RegistryItem
func (r *registry) Unsubscribe(identifier id.ID) *RegistryItem {
	r.mu.Lock()
	defer r.mu.Unlock()

	i, ok := r.items[identifier]
	if !ok {
		return nil
	}

	r.logger.Infof("unsubscribe, identifier: %s", identifier)

	if i.Hosts != nil {
		for _, h := range i.Hosts {
			delete(r.hosts, h.Host)
		}
	}

	delete(r.items, identifier)

	return i
}

func (r *registry) set(i *RegistryItem, identifier id.ID) error {
	r.logger.Debugf("set registry item, identifier: %s", identifier)

	r.mu.Lock()
	defer r.mu.Unlock()

	j, ok := r.items[identifier]
	if !ok {
		return errClientNotSubscribed
	}
	if j != voidRegistryItem {
		return fmt.Errorf("attempt to overwrite registry item")
	}

	if i.Hosts != nil {
		for _, h := range i.Hosts {
			if h.Auth != nil && h.Auth.User == "" {
				return fmt.Errorf("missing auth user")
			}
			if _, ok := r.hosts[trimPort(h.Host)]; ok {
				return fmt.Errorf("host %q is occupied", h.Host)
			}
		}

		for _, h := range i.Hosts {
			r.hosts[trimPort(h.Host)] = &hostInfo{
				identifier: identifier,
				auth:       h.Auth,
			}
		}
	}

	r.items[identifier] = i

	return nil
}

func (r *registry) clear(identifier id.ID) *RegistryItem {
	r.logger.Debugf("clear registry item, identifier: %s", identifier)

	r.mu.Lock()
	defer r.mu.Unlock()

	i, ok := r.items[identifier]
	if !ok || i == voidRegistryItem {
		return nil
	}

	if i.Hosts != nil {
		for _, h := range i.Hosts {
			delete(r.hosts, trimPort(h.Host))
		}
	}

	r.items[identifier] = voidRegistryItem

	return i
}

func trimPort(hostPort string) (host string) {
	host, _, _ = net.SplitHostPort(hostPort)
	if host == "" {
		host = hostPort
	}
	return
}