package tunnel

import "time"

var (
	// DefaultTimeout specifies a general purpose timeout.
	DefaultTimeout = 10 * time.Second
	// DefaultPingTimeout specifies a ping timeout.
	DefaultPingTimeout = 500 * time.Millisecond
)
