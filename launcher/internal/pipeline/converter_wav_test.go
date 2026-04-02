package pipeline

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"math"
	"testing"

	"gopkg.in/hraban/opus.v2"
)

// buildWAV creates a minimal WAV file with the given parameters and PCM data.
func buildWAV(sampleRate uint32, numChannels uint16, bitsPerSample uint16, pcmData []byte) []byte {
	var buf bytes.Buffer

	dataSize := uint32(len(pcmData))
	fmtChunkSize := uint32(16)
	byteRate := sampleRate * uint32(numChannels) * uint32(bitsPerSample) / 8
	blockAlign := numChannels * bitsPerSample / 8

	// RIFF header.
	buf.WriteString("RIFF")
	riffSize := 4 + (8 + fmtChunkSize) + (8 + dataSize) // "WAVE" + fmt chunk + data chunk
	binary.Write(&buf, binary.LittleEndian, riffSize)
	buf.WriteString("WAVE")

	// fmt chunk.
	buf.WriteString("fmt ")
	binary.Write(&buf, binary.LittleEndian, fmtChunkSize)
	binary.Write(&buf, binary.LittleEndian, uint16(1)) // PCM
	binary.Write(&buf, binary.LittleEndian, numChannels)
	binary.Write(&buf, binary.LittleEndian, sampleRate)
	binary.Write(&buf, binary.LittleEndian, byteRate)
	binary.Write(&buf, binary.LittleEndian, blockAlign)
	binary.Write(&buf, binary.LittleEndian, bitsPerSample)

	// data chunk.
	buf.WriteString("data")
	binary.Write(&buf, binary.LittleEndian, dataSize)
	buf.Write(pcmData)

	return buf.Bytes()
}

// buildSilent16BitPCM creates silent (zero-valued) 16-bit PCM data for the given number of samples.
func buildSilent16BitPCM(numSamples int) []byte {
	return make([]byte, numSamples*2)
}

// buildTone16BitPCM creates a simple square wave in 16-bit PCM.
func buildTone16BitPCM(numSamples int, amplitude int16) []byte {
	data := make([]byte, numSamples*2)
	for i := 0; i < numSamples; i++ {
		val := amplitude
		if i%100 >= 50 {
			val = -amplitude
		}
		binary.LittleEndian.PutUint16(data[i*2:], uint16(val))
	}
	return data
}

func TestConvertWAVtoOpus_16BitMono(t *testing.T) {
	// 48000 Hz, mono, 16-bit, ~20ms of audio (960 samples = 1 Opus frame).
	pcm := buildTone16BitPCM(960, 10000)
	wav := buildWAV(48000, 1, 16, pcm)

	result, err := convertWAVtoOpus(wav)
	if err != nil {
		t.Fatalf("convertWAVtoOpus: %v", err)
	}

	// Verify OGG magic bytes.
	if len(result) < 4 || string(result[0:4]) != "OggS" {
		t.Fatal("output does not start with OggS magic")
	}

	// Verify OpusHead is present somewhere in the output.
	if !bytes.Contains(result, []byte("OpusHead")) {
		t.Error("output does not contain OpusHead header")
	}

	// Verify OpusTags is present.
	if !bytes.Contains(result, []byte("OpusTags")) {
		t.Error("output does not contain OpusTags header")
	}
}

func TestConvertWAVtoOpus_16BitStereo(t *testing.T) {
	// 44100 Hz stereo (needs resampling to 48000).
	pcm := buildTone16BitPCM(44100*2, 8000) // 1 second stereo
	wav := buildWAV(44100, 2, 16, pcm)

	result, err := convertWAVtoOpus(wav)
	if err != nil {
		t.Fatalf("convertWAVtoOpus: %v", err)
	}

	if len(result) < 4 || string(result[0:4]) != "OggS" {
		t.Fatal("output does not start with OggS magic")
	}
}

func TestConvertWAVtoOpus_8BitMono(t *testing.T) {
	// 22050 Hz, mono, 8-bit unsigned.
	pcm := make([]byte, 22050) // 1 second
	for i := range pcm {
		if i%100 < 50 {
			pcm[i] = 200 // positive
		} else {
			pcm[i] = 56 // negative
		}
	}
	wav := buildWAV(22050, 1, 8, pcm)

	result, err := convertWAVtoOpus(wav)
	if err != nil {
		t.Fatalf("convertWAVtoOpus: %v", err)
	}

	if len(result) < 4 || string(result[0:4]) != "OggS" {
		t.Fatal("output does not start with OggS magic")
	}
}

func TestConvertWAVtoOpus_8BitStereo(t *testing.T) {
	// 11025 Hz, stereo, 8-bit unsigned.
	pcm := make([]byte, 11025*2) // 1 second stereo
	for i := range pcm {
		pcm[i] = 128 // silence in 8-bit unsigned
	}
	wav := buildWAV(11025, 2, 8, pcm)

	result, err := convertWAVtoOpus(wav)
	if err != nil {
		t.Fatalf("convertWAVtoOpus: %v", err)
	}

	if len(result) < 4 || string(result[0:4]) != "OggS" {
		t.Fatal("output does not start with OggS magic")
	}
}

func TestConvertWAVtoOpusWithOpts_QualityPresets(t *testing.T) {
	pcm := buildSilent16BitPCM(48000) // 1 second mono silence
	wav := buildWAV(48000, 1, 16, pcm)

	tests := []struct {
		quality string
		desc    string
	}{
		{"", "default (128kbps)"},
		{"high", "high (128kbps)"},
		{"medium", "medium (96kbps)"},
		{"low", "low (64kbps)"},
	}

	for _, tt := range tests {
		t.Run(tt.desc, func(t *testing.T) {
			opts := ConvertOptions{Quality: tt.quality}
			result, err := convertWAVtoOpusWithOpts(wav, opts)
			if err != nil {
				t.Fatalf("convertWAVtoOpusWithOpts(%s): %v", tt.quality, err)
			}
			if len(result) < 4 || string(result[0:4]) != "OggS" {
				t.Fatal("output does not start with OggS magic")
			}
		})
	}
}

func TestConvertWAVtoOpus_ConverterRegistered(t *testing.T) {
	// Verify the simple converter was registered.
	fn, err := LookupConverter("wav", "opus")
	if err != nil {
		t.Fatalf("LookupConverter(wav, opus): %v", err)
	}
	if fn == nil {
		t.Fatal("LookupConverter(wav, opus): returned nil function")
	}
}

func TestConvertWAVtoOpus_OptsConverterRegistered(t *testing.T) {
	fn, ok := LookupConverterWithOpts("wav", "opus")
	if !ok {
		t.Fatal("LookupConverterWithOpts(wav, opus): not found")
	}
	if fn == nil {
		t.Fatal("LookupConverterWithOpts(wav, opus): returned nil function")
	}
}

func TestParseWAVHeader_InvalidRIFF(t *testing.T) {
	_, err := parseWAVHeader([]byte("NOT_RIFF_DATA_HERE"))
	if err == nil {
		t.Fatal("expected error for non-RIFF data")
	}
}

func TestParseWAVHeader_TooShort(t *testing.T) {
	_, err := parseWAVHeader([]byte{0x00, 0x01})
	if err == nil {
		t.Fatal("expected error for truncated data")
	}
}

func TestParseWAVHeader_NonPCM(t *testing.T) {
	// Build a WAV with audioFormat=3 (IEEE float) instead of PCM.
	var buf bytes.Buffer
	buf.WriteString("RIFF")
	binary.Write(&buf, binary.LittleEndian, uint32(36))
	buf.WriteString("WAVE")
	buf.WriteString("fmt ")
	binary.Write(&buf, binary.LittleEndian, uint32(16))
	binary.Write(&buf, binary.LittleEndian, uint16(3)) // float format
	binary.Write(&buf, binary.LittleEndian, uint16(1))
	binary.Write(&buf, binary.LittleEndian, uint32(48000))
	binary.Write(&buf, binary.LittleEndian, uint32(192000))
	binary.Write(&buf, binary.LittleEndian, uint16(4))
	binary.Write(&buf, binary.LittleEndian, uint16(32))
	buf.WriteString("data")
	binary.Write(&buf, binary.LittleEndian, uint32(0))

	_, err := parseWAVHeader(buf.Bytes())
	if err == nil {
		t.Fatal("expected error for non-PCM WAV")
	}
	if !containsSubstring(err.Error(), "only PCM=1 supported") {
		t.Errorf("unexpected error: %v", err)
	}
}

func TestResampleLinear_SameRate(t *testing.T) {
	samples := []int16{100, 200, 300, 400}
	result := resampleLinear(samples, 1, 48000, 48000)
	if len(result) != len(samples) {
		t.Fatalf("expected %d samples, got %d", len(samples), len(result))
	}
	for i, s := range samples {
		if result[i] != s {
			t.Errorf("sample %d: expected %d, got %d", i, s, result[i])
		}
	}
}

func TestResampleLinear_Upsample(t *testing.T) {
	// 2 samples at 24000 -> should produce ~4 samples at 48000.
	samples := []int16{0, 1000}
	result := resampleLinear(samples, 1, 24000, 48000)
	if len(result) < 3 {
		t.Fatalf("expected at least 3 output samples for 2x upsample, got %d", len(result))
	}
}

func TestResampleLinear_Empty(t *testing.T) {
	result := resampleLinear(nil, 1, 44100, 48000)
	if result != nil {
		t.Errorf("expected nil for empty input, got %v", result)
	}
}

func TestOggCRC_KnownValue(t *testing.T) {
	// Verify CRC function produces non-zero output for known input.
	data := []byte("OggS")
	crc := oggCRC(data)
	if crc == 0 {
		t.Error("expected non-zero CRC for 'OggS'")
	}
}

func TestQ3CopyProcessor_ModeConvert_WAV_WithOpts(t *testing.T) {
	// Build a WAV file for the conversion test.
	pcm := buildSilent16BitPCM(960) // 1 frame at 48kHz
	wavData := buildWAV(48000, 1, 16, pcm)

	proc := &Q3CopyProcessor{
		Entries: map[string]ProcessorEntry{
			"sound/test.wav": {
				Mode:       ModeConvert,
				TargetFmt:  "opus",
				TargetPath: "sound/test.opus",
				Quality:    "low",
			},
		},
	}

	entry := AssetEntry{Origin: "q3_base", Path: "sound/test.wav"}
	readFile := func() ([]byte, error) {
		return wavData, nil
	}

	decision, err := proc.Process(entry, readFile)
	if err != nil {
		t.Fatalf("Process: %v", err)
	}
	if decision.Action != Replace {
		t.Fatalf("expected Replace, got %d", decision.Action)
	}
	if decision.DestPath != "sound/test.opus" {
		t.Errorf("expected DestPath %q, got %q", "sound/test.opus", decision.DestPath)
	}
	if decision.Data == nil {
		t.Fatal("expected non-nil Data for Replace action")
	}
	// Verify output starts with OGG magic.
	if len(decision.Data) < 4 || string(decision.Data[0:4]) != "OggS" {
		t.Error("converted data does not start with OggS magic bytes")
	}
}

// buildSineWave16BitPCM creates a 16-bit mono sine wave at the given frequency and sample rate.
func buildSineWave16BitPCM(numSamples int, frequency float64, sampleRate float64, amplitude float64) ([]byte, []int16) {
	data := make([]byte, numSamples*2)
	samples := make([]int16, numSamples)
	for i := 0; i < numSamples; i++ {
		t := float64(i) / sampleRate
		val := amplitude * math.Sin(2.0*math.Pi*frequency*t)
		s := int16(val)
		samples[i] = s
		binary.LittleEndian.PutUint16(data[i*2:], uint16(s))
	}
	return data, samples
}

// parseOggPages parses raw OGG data into individual pages, returning each page's payload
// (the concatenated segment data) and its header type flag.
type oggPageInfo struct {
	headerType byte
	data       []byte
}

func parseOggPages(data []byte) ([]oggPageInfo, error) {
	var pages []oggPageInfo
	pos := 0
	for pos+27 <= len(data) {
		// Verify capture pattern.
		if string(data[pos:pos+4]) != "OggS" {
			return nil, fmt.Errorf("expected OggS at offset %d, got %q", pos, data[pos:pos+4])
		}
		headerType := data[pos+5]
		numSegments := int(data[pos+26])
		if pos+27+numSegments > len(data) {
			return nil, fmt.Errorf("segment table extends beyond data at offset %d", pos)
		}
		segTable := data[pos+27 : pos+27+numSegments]
		totalDataSize := 0
		for _, s := range segTable {
			totalDataSize += int(s)
		}
		dataStart := pos + 27 + numSegments
		if dataStart+totalDataSize > len(data) {
			return nil, fmt.Errorf("page data extends beyond input at offset %d", pos)
		}
		pageData := make([]byte, totalDataSize)
		copy(pageData, data[dataStart:dataStart+totalDataSize])

		pages = append(pages, oggPageInfo{headerType: headerType, data: pageData})
		pos = dataStart + totalDataSize
	}
	return pages, nil
}

// extractOpusPacketsFromOggPages extracts Opus audio packets from OGG pages,
// skipping the first two pages (OpusHead and OpusTags).
func extractOpusPacketsFromOggPages(pages []oggPageInfo) [][]byte {
	var packets [][]byte
	for i, page := range pages {
		if i < 2 {
			// Skip OpusHead (page 0) and OpusTags (page 1).
			continue
		}
		packets = append(packets, page.data)
	}
	return packets
}

func TestConvertWAVtoOpus_RoundTrip(t *testing.T) {
	// Create a synthetic 16-bit mono WAV at 48kHz: 440Hz sine wave, 0.1 seconds = 4800 samples.
	const (
		sampleRate = 48000
		channels   = 1
		frequency  = 440.0
		numSamples = 4800 // 0.1 seconds at 48kHz
		amplitude  = 16000.0
	)

	pcmBytes, originalSamples := buildSineWave16BitPCM(numSamples, frequency, float64(sampleRate), amplitude)
	wavData := buildWAV(sampleRate, uint16(channels), 16, pcmBytes)

	// --- Step 1: Convert WAV to OGG Opus ---
	oggData, err := convertWAVtoOpus(wavData)
	if err != nil {
		t.Fatalf("convertWAVtoOpus failed: %v", err)
	}

	// --- Step 2: Parse OGG pages ---
	pages, err := parseOggPages(oggData)
	if err != nil {
		t.Fatalf("parseOggPages failed: %v", err)
	}
	if len(pages) < 3 {
		t.Fatalf("expected at least 3 OGG pages (head + tags + audio), got %d", len(pages))
	}

	// Verify first page is BOS with OpusHead.
	if pages[0].headerType&oggFlagBOS == 0 {
		t.Error("first page is not BOS")
	}
	if !bytes.HasPrefix(pages[0].data, []byte("OpusHead")) {
		t.Error("first page does not contain OpusHead")
	}

	// Parse channel count from OpusHead to verify it matches.
	if len(pages[0].data) >= 10 {
		decodedChannels := int(pages[0].data[9])
		if decodedChannels != channels {
			t.Errorf("OpusHead channel count: expected %d, got %d", channels, decodedChannels)
		}
	}

	// Verify second page has OpusTags.
	if !bytes.HasPrefix(pages[1].data, []byte("OpusTags")) {
		t.Error("second page does not contain OpusTags")
	}

	// --- Step 3: Decode Opus packets back to PCM ---
	opusPackets := extractOpusPacketsFromOggPages(pages)
	if len(opusPackets) == 0 {
		t.Fatal("no Opus audio packets found in OGG stream")
	}

	decoder, err := opus.NewDecoder(sampleRate, channels)
	if err != nil {
		t.Fatalf("opus.NewDecoder failed: %v", err)
	}

	var decodedSamples []int16
	for i, pkt := range opusPackets {
		// Each Opus frame at 48kHz/20ms is 960 samples. Allocate enough for one frame.
		frameBuf := make([]int16, 960*channels)
		n, err := decoder.Decode(pkt, frameBuf)
		if err != nil {
			t.Fatalf("opus decode packet %d failed: %v", i, err)
		}
		decodedSamples = append(decodedSamples, frameBuf[:n*channels]...)
	}

	// --- Step 4: Verify decoded audio ---

	// ac-2: Verify approximate duration. The encoder produces frames of 960 samples.
	// For 4800 input samples, we expect exactly 5 frames = 4800 decoded samples.
	// Allow some tolerance for pre-skip and padding.
	expectedSamples := numSamples
	tolerance := 960 // allow up to 1 extra frame due to padding/pre-skip
	sampleDiff := len(decodedSamples) - expectedSamples
	if sampleDiff < -tolerance || sampleDiff > tolerance {
		t.Errorf("decoded sample count %d differs from expected %d by more than %d",
			len(decodedSamples), expectedSamples, tolerance)
	}
	t.Logf("Original samples: %d, Decoded samples: %d", expectedSamples, len(decodedSamples))

	// Compare the overlapping region. Trim decoded to match original length if it's longer.
	compareLen := len(originalSamples)
	if compareLen > len(decodedSamples) {
		compareLen = len(decodedSamples)
	}

	// The Opus encoder introduces a pre-skip delay of 312 samples.
	// This means decodedSamples[preSkip + k] corresponds to originalSamples[k].
	const preSkip = 312

	// Number of original samples we can compare (accounting for pre-skip shift in decoded).
	origCompareLen := len(originalSamples)
	decodedAvailable := len(decodedSamples) - preSkip
	if decodedAvailable < origCompareLen {
		origCompareLen = decodedAvailable
	}
	if origCompareLen <= 0 {
		t.Fatal("not enough decoded samples after pre-skip to compare")
	}

	// Compute mean absolute error over the comparison region.
	var totalAbsError float64
	count := origCompareLen
	for i := 0; i < origCompareLen; i++ {
		orig := float64(originalSamples[i])
		decoded := float64(decodedSamples[preSkip+i])
		totalAbsError += math.Abs(orig - decoded)
	}

	if count == 0 {
		t.Fatal("no samples to compare after pre-skip")
	}

	meanAbsError := totalAbsError / float64(count)
	// For lossy compression, mean absolute error should be well within 20% of max amplitude.
	maxTolerance := amplitude * 0.20
	t.Logf("Mean absolute error: %.2f (tolerance: %.2f, amplitude: %.0f)", meanAbsError, maxTolerance, amplitude)

	if meanAbsError > maxTolerance {
		t.Errorf("mean absolute error %.2f exceeds 20%% of amplitude (%.2f)", meanAbsError, maxTolerance)
	}

	// Additional check: verify the signal is not silence (i.e., decoder actually produced audio).
	var decodedEnergy float64
	for i := 0; i < origCompareLen; i++ {
		s := float64(decodedSamples[preSkip+i])
		decodedEnergy += s * s
	}
	decodedRMS := math.Sqrt(decodedEnergy / float64(count))
	// The original sine wave RMS should be amplitude/sqrt(2) ~ 11314.
	// The decoded should be at least 25% of that to confirm non-silence.
	expectedRMS := amplitude / math.Sqrt(2)
	minRMS := expectedRMS * 0.25
	t.Logf("Decoded RMS: %.2f (expected ~%.2f, minimum: %.2f)", decodedRMS, expectedRMS, minRMS)

	if decodedRMS < minRMS {
		t.Errorf("decoded RMS %.2f is below minimum %.2f — signal may be silence", decodedRMS, minRMS)
	}
}
