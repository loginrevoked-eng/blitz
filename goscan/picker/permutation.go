package picker

func splitMix64(state *uint64) uint64 {
	*state += 0x9E3779B97F4A7C15
	z := *state
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9
	z = (z ^ (z >> 27)) * 0x94D049BB133111EB
	return z ^ (z >> 31)
}

func fmix32(h uint32) uint32 {
	h ^= h >> 16
	h *= 0x85EBCA6B
	h ^= h >> 13
	h *= 0xC2B2AE35
	h ^= h >> 16
	return h
}

// permute matches the six-round Feistel permutation used by the C picker.
func permute(seed, n, width uint32) uint32 {
	if width == 1 {
		return 0
	}
	bits := uint(2)
	for bits < 32 && (uint64(1)<<bits) < uint64(width) {
		bits += 2
	}
	halfBits := bits / 2
	mask := uint32((uint64(1) << halfBits) - 1)

	state := uint64(seed)
	var keys [6]uint32
	for i := range keys {
		keys[i] = uint32(splitMix64(&state))
	}

	x := n
	for {
		left, right := x>>halfBits, x&mask
		for _, key := range keys {
			left, right = right, left^(fmix32(right^key)&mask)
		}
		x = (left << halfBits) | right
		if x < width {
			return x
		}
	}
}
