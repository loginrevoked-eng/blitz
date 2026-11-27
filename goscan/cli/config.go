package cli

import (
	"errors"
	"flag"
	"fmt"
	"io"
	"strconv"
	"strings"
	"time"
)

type Config struct {
	CIDR           string
	Ports          []uint16
	Threads        int
	Runs           []string
	Verbose        bool
	BucketWidth    uint32
	Cooldown       time.Duration
	ExcludePrivate bool
}

func Parse(args []string) (Config, error) {
	if len(args) == 0 {
		return Config{}, errors.New("usage: goscan <CIDR> [flags]")
	}

	fs := flag.NewFlagSet("goscan", flag.ContinueOnError)
	fs.SetOutput(io.Discard)
	portsRaw := fs.String("ports", "80,443", "comma-separated ports")
	threads := fs.Int("threads", 10, "number of concurrent workers")
	runsRaw := fs.String("run", "banner", "comma-separated grabbers")
	verbose := fs.Bool("verbose", false, "print grabber output")
	bucketWidth := fs.Uint64("bucket-width", 2, "addresses per picker bucket")
	cooldownMS := fs.Int64("cooldown", 500, "bucket cooldown in milliseconds")
	includePrivate := fs.Bool("include-private", false, "include private and reserved IPv4 ranges")
	if err := fs.Parse(args[1:]); err != nil {
		return Config{}, err
	}
	if len(fs.Args()) != 0 {
		return Config{}, fmt.Errorf("unexpected arguments: %s", strings.Join(fs.Args(), " "))
	}
	if *threads < 1 {
		return Config{}, errors.New("--threads must be at least 1")
	}
	if *bucketWidth < 1 || *bucketWidth > uint64(^uint32(0)) {
		return Config{}, errors.New("--bucket-width must be between 1 and 2^32-1")
	}
	if *cooldownMS < 0 {
		return Config{}, errors.New("--cooldown cannot be negative")
	}

	ports, err := parsePorts(*portsRaw)
	if err != nil {
		return Config{}, err
	}
	runs := parseRuns(*runsRaw)
	if len(runs) == 0 {
		return Config{}, errors.New("--run requires at least one grabber name")
	}

	return Config{
		CIDR:           args[0],
		Ports:          ports,
		Threads:        *threads,
		Runs:           runs,
		Verbose:        *verbose,
		BucketWidth:    uint32(*bucketWidth),
		Cooldown:       time.Duration(*cooldownMS) * time.Millisecond,
		ExcludePrivate: !*includePrivate,
	}, nil
}

func parsePorts(raw string) ([]uint16, error) {
	parts := strings.Split(raw, ",")
	ports := make([]uint16, 0, len(parts))
	for _, part := range parts {
		n, err := strconv.Atoi(strings.TrimSpace(part))
		if err != nil || n < 1 || n > 65535 {
			return nil, fmt.Errorf("invalid port %q", part)
		}
		ports = append(ports, uint16(n))
	}
	return ports, nil
}

func parseRuns(raw string) []string {
	parts := strings.Split(raw, ",")
	runs := make([]string, 0, len(parts))
	for _, part := range parts {
		name := strings.TrimSpace(part)
		if name != "" {
			runs = append(runs, name)
		}
	}
	return runs
}
