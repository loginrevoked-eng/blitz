package scanner

import "sync"

type Stats struct {
	mu        sync.Mutex
	queued    int64
	active    int64
	scanned   int64
	alive     int64
	confirmed int64
}

type StatsSnapshot struct {
	Queued    int64
	Active    int64
	Scanned   int64
	Alive     int64
	Confirmed int64
}

func (s *Stats) QueuedInc()    { s.mu.Lock(); s.queued++; s.mu.Unlock() }
func (s *Stats) QueuedDec()    { s.mu.Lock(); s.queued--; s.mu.Unlock() }
func (s *Stats) ActiveInc()    { s.mu.Lock(); s.active++; s.mu.Unlock() }
func (s *Stats) ActiveDec()    { s.mu.Lock(); s.active--; s.mu.Unlock() }
func (s *Stats) ScannedInc()   { s.mu.Lock(); s.scanned++; s.mu.Unlock() }
func (s *Stats) ScannedDec()   { s.mu.Lock(); s.scanned--; s.mu.Unlock() }
func (s *Stats) AliveInc()     { s.mu.Lock(); s.alive++; s.mu.Unlock() }
func (s *Stats) AliveDec()     { s.mu.Lock(); s.alive--; s.mu.Unlock() }
func (s *Stats) ConfirmedInc() { s.mu.Lock(); s.confirmed++; s.mu.Unlock() }
func (s *Stats) ConfirmedDec() { s.mu.Lock(); s.confirmed--; s.mu.Unlock() }

func (s *Stats) Snapshot() StatsSnapshot {
	s.mu.Lock()
	defer s.mu.Unlock()
	return StatsSnapshot{
		Queued: s.queued, Active: s.active, Scanned: s.scanned,
		Alive: s.alive, Confirmed: s.confirmed,
	}
}
