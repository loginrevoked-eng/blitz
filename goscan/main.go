package main

import (
	"context"
	"fmt"
	"os"
	"os/signal"
	"syscall"

	"goscan/cli"
	"goscan/grabbers"
	httpgrabber "goscan/grabbers/http"
	rdpgrabber "goscan/grabbers/rdp"
	"goscan/scanner"
)

func main() {
	config, err := cli.Parse(os.Args[1:])
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(2)
	}

	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()
	registry := map[string]scanner.Grabber{
		"banner": grabbers.Banner,
		"http":   httpgrabber.Root,
		"robots": httpgrabber.Robots,
		"rdp":    rdpgrabber.Grab,
	}
	if err := scanner.Run(ctx, config, registry); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}
