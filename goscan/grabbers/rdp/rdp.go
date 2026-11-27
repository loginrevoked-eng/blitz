package rdp

import (
	"context"
	"encoding/binary"
	"fmt"
	"io"
	"net"
	"strconv"
	"time"

	"goscan/scanner"
	"goscan/utils"
)

// Grab sends an RDP X.224 connection request and reports the server's
// negotiation response. It does not authenticate or continue the RDP session.
func Grab(ctx context.Context, target scanner.Target) (bool, error) {
	address := net.JoinHostPort(target.IP.String(), strconv.Itoa(int(target.Port)))
	conn, err := (&net.Dialer{Timeout: 4 * time.Second}).DialContext(ctx, "tcp", address)
	if err != nil {
		return false, err
	}
	defer conn.Close()
	if err := conn.SetDeadline(time.Now().Add(4 * time.Second)); err != nil {
		return false, err
	}

	// TPKT + X.224 Connection Request + RDP_NEG_REQ (SSL and HYBRID).
	request := []byte{
		0x03, 0x00, 0x00, 0x13,
		0x0e, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x00, 0x08, 0x00, 0x03, 0x00, 0x00, 0x00,
	}
	if _, err := conn.Write(request); err != nil {
		return false, err
	}

	var header [4]byte
	if _, err := io.ReadFull(conn, header[:]); err != nil {
		return false, err
	}
	if header[0] != 0x03 {
		return false, nil
	}
	packetLen := int(binary.BigEndian.Uint16(header[2:4]))
	if packetLen < 7 || packetLen > 4096 {
		return false, nil
	}
	packet := make([]byte, packetLen)
	copy(packet, header[:])
	if _, err := io.ReadFull(conn, packet[4:]); err != nil {
		return false, err
	}

	// X.224 Connection Confirm is 0xd0; negotiation structures follow it.
	if packet[5] != 0xd0 {
		return false, nil
	}
	message := "RDP connection confirmed"
	if len(packet) >= 19 {
		switch packet[11] {
		case 0x02: // RDP_NEG_RSP
			protocol := binary.LittleEndian.Uint32(packet[15:19])
			message = fmt.Sprintf("RDP negotiated protocol 0x%08x", protocol)
		case 0x03: // RDP_NEG_FAILURE
			failure := binary.LittleEndian.Uint32(packet[15:19])
			message = fmt.Sprintf("RDP negotiation failure 0x%08x", failure)
		}
	}
	utils.Sprint("%s:%d -> %s", target.IP, target.Port, message)
	return true, nil
}
