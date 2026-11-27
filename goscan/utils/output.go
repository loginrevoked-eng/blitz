package utils

import (
	"fmt"
	"os"
	"strings"
	"sync"
)

var (
	mu         sync.Mutex
	sprintNoop bool
	statusLine bool
)

func SetSprintNoop(noop bool) {
	mu.Lock()
	sprintNoop = noop
	mu.Unlock()
}

func Sprint(format string, args ...any) {
	mu.Lock()
	defer mu.Unlock()
	if sprintNoop {
		return
	}
	if statusLine {
		fmt.Fprint(os.Stdout, "\r\x1b[K")
		statusLine = false
	}
	fmt.Fprintf(os.Stdout, format+"\n", args...)
}

// SWrite1 writes to stdout without adding a newline.
func SWrite1(format string, args ...any) {
	mu.Lock()
	fmt.Fprintf(os.Stdout, format, args...)
	statusLine = !strings.HasSuffix(format, "\n")
	mu.Unlock()
}
