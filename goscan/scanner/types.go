package scanner

import (
	"context"
	"net/netip"
)

type Target struct {
	IP   netip.Addr
	Port uint16
}

// Grabber reports whether it received a response. Network errors aren't fatal to a scan.
type Grabber func(context.Context, Target) (bool, error)
