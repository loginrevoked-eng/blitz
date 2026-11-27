package picker

import (
	"container/heap"
	crand "crypto/rand"
	"encoding/binary"
	"errors"
	"math/rand"
	"net/netip"
	"sort"
	"sync"
	"time"
)

type bucket struct {
	start   uint32
	end     uint32
	seed    uint32
	current uint32
	expires time.Time
}

func (b *bucket) width() uint32 { return b.end - b.start + 1 }

type cooldownHeap []*bucket

func (h cooldownHeap) Len() int           { return len(h) }
func (h cooldownHeap) Less(i, j int) bool { return h[i].expires.Before(h[j].expires) }
func (h cooldownHeap) Swap(i, j int)      { h[i], h[j] = h[j], h[i] }
func (h *cooldownHeap) Push(value any)    { *h = append(*h, value.(*bucket)) }
func (h *cooldownHeap) Pop() any {
	old := *h
	n := len(old)
	value := old[n-1]
	old[n-1] = nil
	*h = old[:n-1]
	return value
}

type Picker struct {
	mu             sync.Mutex
	changed        *sync.Cond
	eligible       []*bucket
	cooldown       cooldownHeap
	rng            *rand.Rand
	cooldownPeriod time.Duration
	stopped        bool
}

func New(cidr string, bucketWidth uint32, cooldown time.Duration, excludePrivateReserved bool) (*Picker, error) {
	if bucketWidth == 0 {
		return nil, errors.New("bucket width must be positive")
	}
	if cooldown < 0 {
		return nil, errors.New("cooldown cannot be negative")
	}
	prefix, err := netip.ParsePrefix(cidr)
	if err != nil || !prefix.Addr().Is4() {
		return nil, errors.New("CIDR must be a valid IPv4 prefix")
	}
	prefix = prefix.Masked()

	var seedBytes [8]byte
	if _, err := crand.Read(seedBytes[:]); err != nil {
		return nil, err
	}
	p := &Picker{
		rng:            rand.New(rand.NewSource(int64(binary.LittleEndian.Uint64(seedBytes[:])))),
		cooldownPeriod: cooldown,
	}
	p.changed = sync.NewCond(&p.mu)

	start := ipv4Uint(prefix.Addr())
	end := start | uint32((uint64(1)<<uint(32-prefix.Bits()))-1)
	blocked := append([]CidrMask(nil), cloudflareRanges[:]...)
	if excludePrivateReserved {
		blocked = append(blocked, privateReservedRanges[:]...)
	}
	sort.Slice(blocked, func(i, j int) bool {
		return blocked[i].Network < blocked[j].Network
	})

	cursor, end64 := uint64(start), uint64(end)
	for _, excluded := range blocked {
		low := uint64(excluded.Network)
		high := low | uint64(^excluded.Mask)
		if high < cursor || low > end64 {
			continue
		}
		if low > cursor {
			p.appendBuckets(cursor, low-1, bucketWidth)
		}
		if high+1 > cursor {
			cursor = high + 1
		}
		if cursor > end64 {
			break
		}
	}
	if cursor <= end64 {
		p.appendBuckets(cursor, end64, bucketWidth)
	}
	return p, nil
}

func (p *Picker) appendBuckets(start, end uint64, width uint32) {
	for start <= end {
		bucketEnd := start + uint64(width) - 1
		if bucketEnd > end {
			bucketEnd = end
		}
		p.eligible = append(p.eligible, &bucket{
			start: uint32(start), end: uint32(bucketEnd), seed: p.rng.Uint32(),
		})
		start = bucketEnd + 1
	}
}

func (p *Picker) InitialExhaustionCount() int { return len(p.eligible) }

// Pick waits through cooldowns and returns false after Stop or exhaustion.
func (p *Picker) Pick() (netip.Addr, bool) {
	p.mu.Lock()
	defer p.mu.Unlock()

	for !p.stopped {
		now := time.Now()
		for len(p.cooldown) > 0 && !p.cooldown[0].expires.After(now) {
			b := heap.Pop(&p.cooldown).(*bucket)
			b.expires = time.Time{}
			p.eligible = append(p.eligible, b)
		}
		if len(p.eligible) > 0 {
			i := p.rng.Intn(len(p.eligible))
			b := p.eligible[i]
			p.eligible[i] = p.eligible[len(p.eligible)-1]
			p.eligible[len(p.eligible)-1] = nil
			p.eligible = p.eligible[:len(p.eligible)-1]

			address := b.start + permute(b.seed, b.current, b.width())
			b.current++
			if b.current < b.width() {
				b.expires = now.Add(p.cooldownPeriod)
				heap.Push(&p.cooldown, b)
				p.changed.Signal()
			}
			return uint32Addr(address), true
		}
		if len(p.cooldown) == 0 {
			return netip.Addr{}, false
		}

		wait := time.Until(p.cooldown[0].expires)
		if wait <= 0 {
			continue
		}
		timer := time.AfterFunc(wait, func() {
			p.mu.Lock()
			p.changed.Broadcast()
			p.mu.Unlock()
		})
		p.changed.Wait()
		timer.Stop()
	}
	return netip.Addr{}, false
}

func (p *Picker) Stop() {
	p.mu.Lock()
	p.stopped = true
	p.changed.Broadcast()
	p.mu.Unlock()
}

func ipv4Uint(addr netip.Addr) uint32 {
	b := addr.As4()
	return uint32(b[0])<<24 | uint32(b[1])<<16 | uint32(b[2])<<8 | uint32(b[3])
}

func uint32Addr(value uint32) netip.Addr {
	return netip.AddrFrom4([4]byte{
		byte(value >> 24), byte(value >> 16), byte(value >> 8), byte(value),
	})
}
