package scanner

import (
	"context"
	"fmt"
	"sort"
	"sync"
	"time"

	"goscan/cli"
	"goscan/picker"
	"goscan/utils"
)

func Run(ctx context.Context, config cli.Config, registry map[string]Grabber) error {
	grabbers := make([]Grabber, 0, len(config.Runs))
	for _, name := range config.Runs {
		grabber, ok := registry[name]
		if !ok {
			return fmt.Errorf("unknown grabber %q (available: %s)", name, joinNames(registry))
		}
		grabbers = append(grabbers, grabber)
	}
	if len(grabbers) == 0 {
		return fmt.Errorf("select at least one grabber with --run")
	}

	addressPicker, err := picker.New(
		config.CIDR, config.BucketWidth, config.Cooldown, config.ExcludePrivate,
	)
	if err != nil {
		return err
	}
	utils.SetSprintNoop(!config.Verbose)

	scanCtx, cancel := context.WithCancel(ctx)
	defer cancel()
	go func() {
		<-scanCtx.Done()
		addressPicker.Stop()
	}()

	stats := &Stats{}
	jobs := make(chan Target, config.Threads*2)
	var workers sync.WaitGroup
	for i := 0; i < config.Threads; i++ {
		workers.Add(1)
		go worker(scanCtx, jobs, grabbers[i%len(grabbers)], stats, &workers)
	}

	stopStats := startStats(scanCtx, stats, config.Threads, config.Runs)
	produce(scanCtx, addressPicker, config.Ports, jobs, stats)
	close(jobs)
	workers.Wait()
	cancel()
	<-stopStats
	return nil
}

func produce(ctx context.Context, addresses *picker.Picker, ports []uint16, jobs chan<- Target, stats *Stats) {
	for ctx.Err() == nil {
		ip, ok := addresses.Pick()
		if !ok {
			return
		}
		for _, port := range ports {
			if ctx.Err() != nil {
				return
			}
			stats.QueuedInc()
			select {
			case jobs <- Target{IP: ip, Port: port}:
			case <-ctx.Done():
				stats.QueuedDec()
				return
			}
		}
	}
}

func worker(ctx context.Context, jobs <-chan Target, grabber Grabber, stats *Stats, workers *sync.WaitGroup) {
	defer workers.Done()
	for target := range jobs {
		stats.QueuedDec()
		if ctx.Err() != nil {
			continue
		}
		stats.ActiveInc()
		alive, _ := grabber(ctx, target)
		stats.ActiveDec()
		stats.ScannedInc()
		if alive {
			stats.AliveInc()
		}
	}
}

func startStats(ctx context.Context, stats *Stats, workers int, runs []string) <-chan struct{} {
	done := make(chan struct{})
	go func() {
		defer close(done)
		ticker := time.NewTicker(time.Second)
		defer ticker.Stop()
		for {
			select {
			case <-ticker.C:
				highlight(stats, workers, runs)
			case <-ctx.Done():
				highlight(stats, workers, runs)
				utils.SWrite1("\n")
				return
			}
		}
	}()
	return done
}

func highlight(stats *Stats, workers int, runs []string) {
	s := stats.Snapshot()
	utils.SWrite1("\rworkers=%d active=%d queued=%d scanned=%d alive=%d confirmed=%d grabbers=%s\x1b[K",
		workers, s.Active, s.Queued, s.Scanned, s.Alive, s.Confirmed, joinRun(runs))
}

func joinRun(runs []string) string {
	result := ""
	for i, name := range runs {
		if i > 0 {
			result += ","
		}
		result += name
	}
	return result
}

func joinNames(registry map[string]Grabber) string {
	names := make([]string, 0, len(registry))
	for name := range registry {
		names = append(names, name)
	}
	sort.Strings(names)
	return joinRun(names)
}
