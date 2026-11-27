package grabbers

import (
	"context"
	"fmt"
	"io"
	"net"
	"strconv"
	"time"

	"goscan/scanner"
	"goscan/utils"
)

// Banner passively reads a short unsolicited response from a TCP service.
func Banner(ctx context.Context, target scanner.Target) (bool, error) {
	address := net.JoinHostPort(target.IP.String(), strconv.Itoa(int(target.Port)))
	dialer := net.Dialer{Timeout: 4 * time.Second}
	conn, err := dialer.DialContext(ctx, "tcp", address)
	if err != nil {
		return false, err
	}
	defer conn.Close()

	if err := conn.SetReadDeadline(time.Now().Add(5 * time.Millisecond)); err != nil {
		return false, fmt.Errorf("set banner read deadline: %w", err)
	}
	buf := make([]byte, 4096)
	n, err := conn.Read(buf)
	if err != nil && err != io.EOF {
		if timeout, ok := err.(net.Error); ok && timeout.Timeout() {
			return false, nil
		}
		return false, err
	}
	if n > 0 {
		utils.Sprint("%s:%d -> %q", target.IP, target.Port, buf[:n])
	}
	return n > 0, nil
}
