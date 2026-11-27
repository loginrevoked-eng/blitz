package picker

import "net/netip"

type CidrMask struct {
	Network uint32
	Mask    uint32
}

var cloudflareRanges = [...]CidrMask{
	{0xADF53000, 0xFFFFF000}, // 173.245.48.0/20
	{0x6715F400, 0xFFFFFC00}, // 103.21.244.0/22
	{0x6716C800, 0xFFFFFC00}, // 103.22.200.0/22
	{0x671F0400, 0xFFFFFC00}, // 103.31.4.0/22
	{0x8D654000, 0xFFFFC000}, // 141.101.64.0/18
	{0x6CA2C000, 0xFFFFC000}, // 108.162.192.0/18
	{0xBE5DF000, 0xFFFFF000}, // 190.93.240.0/20
	{0xBC726000, 0xFFFFF000}, // 188.114.96.0/20
	{0xC5EAF000, 0xFFFFFC00}, // 197.234.240.0/22
	{0xC6298000, 0xFFFF8000}, // 198.41.128.0/17
	{0xA29E0000, 0xFFFE0000}, // 162.158.0.0/15
	{0x68100000, 0xFFF80000}, // 104.16.0.0/13
	{0x68180000, 0xFFFC0000}, // 104.24.0.0/14
	{0xAC400000, 0xFFF80000}, // 172.64.0.0/13
	{0x83004800, 0xFFFFFC00}, // 131.0.72.0/22
}

var privateReservedRanges = [...]CidrMask{
	{0x00000000, 0xFF000000}, // 0.0.0.0/8
	{0x0A000000, 0xFF000000}, // 10.0.0.0/8
	{0x64400000, 0xFFC00000}, // 100.64.0.0/10
	{0x7F000000, 0xFF000000}, // 127.0.0.0/8
	{0xA9FE0000, 0xFFFF0000}, // 169.254.0.0/16
	{0xAC100000, 0xFFF00000}, // 172.16.0.0/12
	{0xC0000000, 0xFFFFFF00}, // 192.0.0.0/24
	{0xC0000200, 0xFFFFFF00}, // 192.0.2.0/24
	{0xC0A80000, 0xFFFF0000}, // 192.168.0.0/16
	{0xC6120000, 0xFFFE0000}, // 198.18.0.0/15
	{0xC6336400, 0xFFFFFF00}, // 198.51.100.0/24
	{0xCB007100, 0xFFFFFF00}, // 203.0.113.0/24
	{0xE0000000, 0xF0000000}, // 224.0.0.0/4
	{0xF0000000, 0xF0000000}, // 240.0.0.0/4
}

// Exclude reports whether ip is in a Cloudflare range or, when requested, a
// private or reserved range.
func Exclude(ip netip.Addr, excludePrivateReserved bool) bool {
	if !ip.Is4() {
		return true
	}
	address := ipv4Uint(ip)
	for _, cidr := range cloudflareRanges {
		if address&cidr.Mask == cidr.Network {
			return true
		}
	}
	if excludePrivateReserved {
		for _, cidr := range privateReservedRanges {
			if address&cidr.Mask == cidr.Network {
				return true
			}
		}
	}
	return false
}
