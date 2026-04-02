package pipeline

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"math"

	"gopkg.in/hraban/opus.v2"
)

func init() {
	RegisterConverter("wav", "opus", convertWAVtoOpus)
	RegisterConverterWithOpts("wav", "opus", convertWAVtoOpusWithOpts)
}

// convertWAVtoOpus converts WAV PCM audio to OGG Opus at the default bitrate (128 kbps).
func convertWAVtoOpus(src []byte) ([]byte, error) {
	return convertWAVtoOpusInternal(src, 128000)
}

// convertWAVtoOpusWithOpts converts WAV PCM audio to OGG Opus with configurable quality.
func convertWAVtoOpusWithOpts(src []byte, opts ConvertOptions) ([]byte, error) {
	bitrate := 128000
	switch opts.Quality {
	case "medium":
		bitrate = 96000
	case "low":
		bitrate = 64000
	case "high", "":
		bitrate = 128000
	}
	return convertWAVtoOpusInternal(src, bitrate)
}

// wavHeader holds the parsed fields of a WAV RIFF header.
type wavHeader struct {
	AudioFormat   uint16
	NumChannels   uint16
	SampleRate    uint32
	BitsPerSample uint16
	DataSize      uint32
	DataOffset    int // byte offset into src where PCM data begins
}

// parseWAVHeader parses a WAV RIFF file and returns the header information.
func parseWAVHeader(src []byte) (*wavHeader, error) {
	if len(src) < 12 {
		return nil, fmt.Errorf("wav: data too short for RIFF header: %d bytes", len(src))
	}

	// Check RIFF magic.
	if string(src[0:4]) != "RIFF" {
		return nil, fmt.Errorf("wav: missing RIFF magic, got %q", src[0:4])
	}
	if string(src[8:12]) != "WAVE" {
		return nil, fmt.Errorf("wav: missing WAVE format, got %q", src[8:12])
	}

	var hdr wavHeader
	pos := 12

	// Walk chunks to find "fmt " and "data".
	foundFmt := false
	foundData := false

	for pos+8 <= len(src) {
		chunkID := string(src[pos : pos+4])
		chunkSize := binary.LittleEndian.Uint32(src[pos+4 : pos+8])
		chunkDataStart := pos + 8

		switch chunkID {
		case "fmt ":
			if chunkSize < 16 {
				return nil, fmt.Errorf("wav: fmt chunk too small: %d bytes", chunkSize)
			}
			if chunkDataStart+16 > len(src) {
				return nil, fmt.Errorf("wav: fmt chunk extends beyond data")
			}
			r := bytes.NewReader(src[chunkDataStart : chunkDataStart+16])
			if err := binary.Read(r, binary.LittleEndian, &hdr.AudioFormat); err != nil {
				return nil, fmt.Errorf("wav: reading AudioFormat: %w", err)
			}
			if err := binary.Read(r, binary.LittleEndian, &hdr.NumChannels); err != nil {
				return nil, fmt.Errorf("wav: reading NumChannels: %w", err)
			}
			if err := binary.Read(r, binary.LittleEndian, &hdr.SampleRate); err != nil {
				return nil, fmt.Errorf("wav: reading SampleRate: %w", err)
			}
			var byteRate uint32
			if err := binary.Read(r, binary.LittleEndian, &byteRate); err != nil {
				return nil, fmt.Errorf("wav: reading ByteRate: %w", err)
			}
			var blockAlign uint16
			if err := binary.Read(r, binary.LittleEndian, &blockAlign); err != nil {
				return nil, fmt.Errorf("wav: reading BlockAlign: %w", err)
			}
			if err := binary.Read(r, binary.LittleEndian, &hdr.BitsPerSample); err != nil {
				return nil, fmt.Errorf("wav: reading BitsPerSample: %w", err)
			}
			foundFmt = true

		case "data":
			hdr.DataSize = chunkSize
			hdr.DataOffset = chunkDataStart
			foundData = true
		}

		if foundFmt && foundData {
			break
		}

		// Advance to next chunk (chunks are 2-byte aligned).
		advance := int(chunkSize)
		if advance%2 != 0 {
			advance++
		}
		pos = chunkDataStart + advance
	}

	if !foundFmt {
		return nil, fmt.Errorf("wav: fmt chunk not found")
	}
	if !foundData {
		return nil, fmt.Errorf("wav: data chunk not found")
	}
	if hdr.AudioFormat != 1 {
		return nil, fmt.Errorf("wav: unsupported audio format %d (only PCM=1 supported)", hdr.AudioFormat)
	}
	if hdr.NumChannels != 1 && hdr.NumChannels != 2 {
		return nil, fmt.Errorf("wav: unsupported channel count %d (only mono=1 or stereo=2 supported)", hdr.NumChannels)
	}
	if hdr.BitsPerSample != 8 && hdr.BitsPerSample != 16 {
		return nil, fmt.Errorf("wav: unsupported bits per sample %d (only 8 or 16 supported)", hdr.BitsPerSample)
	}

	return &hdr, nil
}

// extractPCM16 reads PCM data from the WAV and returns it as []int16 samples (interleaved for stereo).
func extractPCM16(src []byte, hdr *wavHeader) ([]int16, error) {
	dataEnd := hdr.DataOffset + int(hdr.DataSize)
	if dataEnd > len(src) {
		dataEnd = len(src)
	}
	rawData := src[hdr.DataOffset:dataEnd]

	var samples []int16

	switch hdr.BitsPerSample {
	case 16:
		// 16-bit signed little-endian samples.
		numSamples := len(rawData) / 2
		samples = make([]int16, numSamples)
		for i := 0; i < numSamples; i++ {
			samples[i] = int16(binary.LittleEndian.Uint16(rawData[i*2 : i*2+2]))
		}

	case 8:
		// 8-bit unsigned samples (0-255), convert to int16.
		samples = make([]int16, len(rawData))
		for i, b := range rawData {
			samples[i] = int16(int(b)-128) << 8
		}

	default:
		return nil, fmt.Errorf("wav: unsupported bits per sample: %d", hdr.BitsPerSample)
	}

	return samples, nil
}

// resampleLinear resamples interleaved PCM samples from srcRate to dstRate using linear interpolation.
func resampleLinear(samples []int16, channels int, srcRate, dstRate int) []int16 {
	if srcRate == dstRate {
		return samples
	}

	srcFrames := len(samples) / channels
	if srcFrames == 0 {
		return nil
	}

	ratio := float64(srcRate) / float64(dstRate)
	dstFrames := int(math.Ceil(float64(srcFrames) / ratio))
	out := make([]int16, dstFrames*channels)

	for i := 0; i < dstFrames; i++ {
		srcPos := float64(i) * ratio
		srcIdx := int(srcPos)
		frac := srcPos - float64(srcIdx)

		if srcIdx >= srcFrames-1 {
			// Last sample — no interpolation.
			for ch := 0; ch < channels; ch++ {
				out[i*channels+ch] = samples[(srcFrames-1)*channels+ch]
			}
			continue
		}

		for ch := 0; ch < channels; ch++ {
			s0 := float64(samples[srcIdx*channels+ch])
			s1 := float64(samples[(srcIdx+1)*channels+ch])
			val := s0 + frac*(s1-s0)
			// Clamp to int16 range.
			if val > 32767 {
				val = 32767
			} else if val < -32768 {
				val = -32768
			}
			out[i*channels+ch] = int16(val)
		}
	}

	return out
}

// convertWAVtoOpusInternal is the shared implementation for WAV-to-Opus conversion.
func convertWAVtoOpusInternal(src []byte, bitrate int) ([]byte, error) {
	hdr, err := parseWAVHeader(src)
	if err != nil {
		return nil, fmt.Errorf("wav decode: %w", err)
	}

	samples, err := extractPCM16(src, hdr)
	if err != nil {
		return nil, fmt.Errorf("wav pcm extract: %w", err)
	}

	channels := int(hdr.NumChannels)

	// Resample to 48000 Hz (required by Opus).
	const opusRate = 48000
	samples = resampleLinear(samples, channels, int(hdr.SampleRate), opusRate)

	if len(samples) == 0 {
		return nil, fmt.Errorf("wav: no audio samples after resampling")
	}

	// Create Opus encoder.
	enc, err := opus.NewEncoder(opusRate, channels, opus.AppAudio)
	if err != nil {
		return nil, fmt.Errorf("opus encoder create: %w", err)
	}
	if err := enc.SetBitrate(bitrate); err != nil {
		return nil, fmt.Errorf("opus set bitrate: %w", err)
	}

	// Encode in frames of 960 samples per channel (20ms at 48kHz).
	const frameSize = 960
	frameSamples := frameSize * channels

	// Collect encoded Opus packets.
	var packets [][]byte
	totalFrames := len(samples) / frameSamples
	remainder := len(samples) % frameSamples

	// Encode complete frames.
	maxPacketSize := 4000 // generous max for a single Opus packet
	for i := 0; i < totalFrames; i++ {
		frame := samples[i*frameSamples : (i+1)*frameSamples]
		buf := make([]byte, maxPacketSize)
		n, err := enc.Encode(frame, buf)
		if err != nil {
			return nil, fmt.Errorf("opus encode frame %d: %w", i, err)
		}
		packet := make([]byte, n)
		copy(packet, buf[:n])
		packets = append(packets, packet)
	}

	// Encode remaining samples (zero-padded to a full frame).
	if remainder > 0 {
		padded := make([]int16, frameSamples)
		copy(padded, samples[totalFrames*frameSamples:])
		buf := make([]byte, maxPacketSize)
		n, err := enc.Encode(padded, buf)
		if err != nil {
			return nil, fmt.Errorf("opus encode final frame: %w", err)
		}
		packet := make([]byte, n)
		copy(packet, buf[:n])
		packets = append(packets, packet)
	}

	if len(packets) == 0 {
		return nil, fmt.Errorf("opus: no packets encoded")
	}

	// Wrap in OGG container.
	oggData, err := wrapOggOpus(packets, channels, opusRate, int(hdr.SampleRate))
	if err != nil {
		return nil, fmt.Errorf("ogg wrap: %w", err)
	}

	return oggData, nil
}

// --- Minimal OGG Opus container writer ---

// oggCRCTable is the CRC lookup table for OGG pages (polynomial 0x04C11DB7).
var oggCRCTable [256]uint32

func init() {
	const poly = 0x04C11DB7
	for i := 0; i < 256; i++ {
		r := uint32(i) << 24
		for j := 0; j < 8; j++ {
			if r&0x80000000 != 0 {
				r = (r << 1) ^ poly
			} else {
				r <<= 1
			}
		}
		oggCRCTable[i] = r
	}
}

// oggCRC computes the OGG CRC32 for a byte slice.
func oggCRC(data []byte) uint32 {
	var crc uint32
	for _, b := range data {
		crc = (crc << 8) ^ oggCRCTable[((crc>>24)&0xFF)^uint32(b)]
	}
	return crc
}

// oggPage flags.
const (
	oggFlagContinued = 0x01
	oggFlagBOS       = 0x02
	oggFlagEOS       = 0x04
)

// writeOggPage writes a single OGG page to buf.
// segments is a list of segment data (each up to 255 bytes); for simplicity we write
// one segment per page for small packets, or split large packets into multiple segments.
func writeOggPage(buf *bytes.Buffer, serialNo uint32, pageSeqNo uint32, granulePos uint64, flags byte, data []byte) {
	// Build segment table: each segment is up to 255 bytes.
	// If data is exactly N*255, we need an additional 0-length segment to terminate.
	var segTable []byte
	remaining := len(data)
	for remaining >= 255 {
		segTable = append(segTable, 255)
		remaining -= 255
	}
	segTable = append(segTable, byte(remaining))

	numSegments := len(segTable)

	// Build the page header (27 bytes + segment table + data).
	headerSize := 27 + numSegments
	pageSize := headerSize + len(data)
	page := make([]byte, pageSize)

	// Capture pattern.
	copy(page[0:4], "OggS")
	page[4] = 0 // stream structure version
	page[5] = flags

	// Granule position (8 bytes, little-endian).
	binary.LittleEndian.PutUint64(page[6:14], granulePos)

	// Serial number.
	binary.LittleEndian.PutUint32(page[14:18], serialNo)

	// Page sequence number.
	binary.LittleEndian.PutUint32(page[18:22], pageSeqNo)

	// CRC placeholder (will fill after computing).
	// page[22:26] = 0 (already zero)

	// Number of segments.
	page[26] = byte(numSegments)

	// Segment table.
	copy(page[27:27+numSegments], segTable)

	// Page data.
	copy(page[headerSize:], data)

	// Compute CRC over the entire page (with CRC field set to 0).
	crc := oggCRC(page)
	binary.LittleEndian.PutUint32(page[22:26], crc)

	buf.Write(page)
}

// wrapOggOpus wraps Opus packets in a valid OGG Opus container.
func wrapOggOpus(packets [][]byte, channels int, opusRate int, inputRate int) ([]byte, error) {
	var buf bytes.Buffer
	var serialNo uint32 = 0x51334E57 // "Q3NW" — arbitrary but deterministic
	var pageSeq uint32

	// --- Page 0: OpusHead (BOS) ---
	opusHead := buildOpusHead(channels, inputRate)
	writeOggPage(&buf, serialNo, pageSeq, 0, oggFlagBOS, opusHead)
	pageSeq++

	// --- Page 1: OpusTags ---
	opusTags := buildOpusTags()
	writeOggPage(&buf, serialNo, pageSeq, 0, 0, opusTags)
	pageSeq++

	// --- Audio data pages ---
	// Pre-skip: Opus spec recommends 3840 samples (80ms) as encoder delay.
	// We use the standard 312 samples for simplicity (values < 960 are typical).
	const preSkip = 312
	const frameSize = 960

	var granulePos uint64 = preSkip
	for i, pkt := range packets {
		flags := byte(0)
		if i == len(packets)-1 {
			flags = oggFlagEOS
		}
		granulePos = uint64(preSkip) + uint64(i+1)*uint64(frameSize)
		writeOggPage(&buf, serialNo, pageSeq, granulePos, flags, pkt)
		pageSeq++
	}

	return buf.Bytes(), nil
}

// buildOpusHead builds the 19-byte OpusHead identification header.
// See RFC 7845 section 5.1.
func buildOpusHead(channels int, inputSampleRate int) []byte {
	head := make([]byte, 19)
	copy(head[0:8], "OpusHead")
	head[8] = 1                                                    // version
	head[9] = byte(channels)                                       // channel count
	binary.LittleEndian.PutUint16(head[10:12], 312)                // pre-skip
	binary.LittleEndian.PutUint32(head[12:16], uint32(inputSampleRate)) // input sample rate
	binary.LittleEndian.PutUint16(head[16:18], 0)                  // output gain
	head[18] = 0                                                   // mapping family
	return head
}

// buildOpusTags builds the OpusTags comment header.
// See RFC 7845 section 5.2.
func buildOpusTags() []byte {
	var buf bytes.Buffer
	buf.WriteString("OpusTags")

	vendor := "q3now"
	vendorLen := make([]byte, 4)
	binary.LittleEndian.PutUint32(vendorLen, uint32(len(vendor)))
	buf.Write(vendorLen)
	buf.WriteString(vendor)

	// 0 user comments.
	commentCount := make([]byte, 4)
	binary.LittleEndian.PutUint32(commentCount, 0)
	buf.Write(commentCount)

	return buf.Bytes()
}
