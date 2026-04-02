package pipeline

import (
	"encoding/binary"
	"image/color"
	"image/png"
	"bytes"
	"testing"
)

// buildTGAHeader returns an 18-byte TGA header.
func buildTGAHeader(imageType uint8, w, h uint16, bpp uint8, descriptor uint8) []byte {
	hdr := tgaHeader{
		IDLength:        0,
		ColorMapType:    0,
		ImageType:       imageType,
		ColorMapOrigin:  0,
		ColorMapLength:  0,
		ColorMapDepth:   0,
		XOrigin:         0,
		YOrigin:         0,
		Width:           w,
		Height:          h,
		BitsPerPixel:    bpp,
		ImageDescriptor: descriptor,
	}
	var buf bytes.Buffer
	binary.Write(&buf, binary.LittleEndian, &hdr)
	return buf.Bytes()
}

// bgraPixel returns 4 bytes in BGRA order.
func bgraPixel(r, g, b, a uint8) []byte {
	return []byte{b, g, r, a}
}

func TestConvertTGAtoPNG_UncompressedType2_AlphaFidelity(t *testing.T) {
	// 4x4 32-bit BGRA uncompressed TGA, top-to-bottom (descriptor bit 5 set + 8 alpha bits).
	const w, h = 4, 4
	descriptor := uint8(0x28) // top-to-bottom + 8 alpha bits

	header := buildTGAHeader(2, w, h, 32, descriptor)

	// Build 16 pixels with varying RGBA and alpha values.
	type pixel struct {
		r, g, b, a uint8
	}
	pixels := [w * h]pixel{
		// Row 0: varying alpha
		{255, 0, 0, 0},     // red, fully transparent
		{0, 255, 0, 85},    // green, ~33% alpha
		{0, 0, 255, 170},   // blue, ~67% alpha
		{255, 255, 0, 255}, // yellow, fully opaque
		// Row 1: varying colors, same alpha
		{128, 64, 32, 100},
		{10, 20, 30, 100},
		{200, 150, 100, 100},
		{1, 2, 3, 100},
		// Row 2: all zero
		{0, 0, 0, 0},
		{0, 0, 0, 0},
		{0, 0, 0, 0},
		{0, 0, 0, 0},
		// Row 3: all max
		{255, 255, 255, 255},
		{255, 255, 255, 255},
		{255, 255, 255, 255},
		{255, 255, 255, 255},
	}

	var pixelData []byte
	for _, p := range pixels {
		pixelData = append(pixelData, bgraPixel(p.r, p.g, p.b, p.a)...)
	}

	tgaBytes := append(header, pixelData...)
	pngBytes, err := convertTGAtoPNG(tgaBytes)
	if err != nil {
		t.Fatalf("convertTGAtoPNG: %v", err)
	}

	img, err := png.Decode(bytes.NewReader(pngBytes))
	if err != nil {
		t.Fatalf("png.Decode: %v", err)
	}

	bounds := img.Bounds()
	if bounds.Dx() != w || bounds.Dy() != h {
		t.Fatalf("expected %dx%d, got %dx%d", w, h, bounds.Dx(), bounds.Dy())
	}

	// Verify every pixel matches exactly.
	for y := 0; y < h; y++ {
		for x := 0; x < w; x++ {
			idx := y*w + x
			expected := pixels[idx]
			r, g, b, a := img.At(x, y).RGBA()
			// RGBA() returns premultiplied 16-bit values; convert back.
			gotR := uint8(r >> 8)
			gotG := uint8(g >> 8)
			gotB := uint8(b >> 8)
			gotA := uint8(a >> 8)

			// For NRGBA images, use the color.NRGBAModel for exact comparison.
			nrgba := color.NRGBAModel.Convert(img.At(x, y)).(color.NRGBA)
			gotR = nrgba.R
			gotG = nrgba.G
			gotB = nrgba.B
			gotA = nrgba.A

			if gotR != expected.r || gotG != expected.g || gotB != expected.b || gotA != expected.a {
				t.Errorf("pixel(%d,%d): expected RGBA(%d,%d,%d,%d), got RGBA(%d,%d,%d,%d)",
					x, y, expected.r, expected.g, expected.b, expected.a,
					gotR, gotG, gotB, gotA)
			}
		}
	}
}

func TestConvertTGAtoPNG_RLEType10_AlphaFidelity(t *testing.T) {
	// 5x1 image: 3 identical pixels (RLE run) + 2 different pixels (raw run).
	const w, h = 5, 1
	descriptor := uint8(0x28) // top-to-bottom + 8 alpha bits

	header := buildTGAHeader(10, w, h, 32, descriptor)

	type pixel struct {
		r, g, b, a uint8
	}

	// RLE run: 3 identical pixels (red, alpha=128)
	rlePixel := pixel{255, 0, 0, 128}
	// Raw run: 2 different pixels
	rawPixel0 := pixel{0, 255, 0, 64}
	rawPixel1 := pixel{0, 0, 255, 255}

	expected := []pixel{rlePixel, rlePixel, rlePixel, rawPixel0, rawPixel1}

	var pixelData []byte

	// RLE packet: header byte = 0x82 (bit 7 set = RLE, count = 2+1 = 3)
	pixelData = append(pixelData, 0x82)
	pixelData = append(pixelData, bgraPixel(rlePixel.r, rlePixel.g, rlePixel.b, rlePixel.a)...)

	// Raw packet: header byte = 0x01 (bit 7 clear = raw, count = 1+1 = 2)
	pixelData = append(pixelData, 0x01)
	pixelData = append(pixelData, bgraPixel(rawPixel0.r, rawPixel0.g, rawPixel0.b, rawPixel0.a)...)
	pixelData = append(pixelData, bgraPixel(rawPixel1.r, rawPixel1.g, rawPixel1.b, rawPixel1.a)...)

	tgaBytes := append(header, pixelData...)
	pngBytes, err := convertTGAtoPNG(tgaBytes)
	if err != nil {
		t.Fatalf("convertTGAtoPNG: %v", err)
	}

	img, err := png.Decode(bytes.NewReader(pngBytes))
	if err != nil {
		t.Fatalf("png.Decode: %v", err)
	}

	bounds := img.Bounds()
	if bounds.Dx() != w || bounds.Dy() != h {
		t.Fatalf("expected %dx%d, got %dx%d", w, h, bounds.Dx(), bounds.Dy())
	}

	for x := 0; x < w; x++ {
		nrgba := color.NRGBAModel.Convert(img.At(x, 0)).(color.NRGBA)
		exp := expected[x]
		if nrgba.R != exp.r || nrgba.G != exp.g || nrgba.B != exp.b || nrgba.A != exp.a {
			t.Errorf("pixel(%d,0): expected RGBA(%d,%d,%d,%d), got RGBA(%d,%d,%d,%d)",
				x, exp.r, exp.g, exp.b, exp.a,
				nrgba.R, nrgba.G, nrgba.B, nrgba.A)
		}
	}
}

func TestConvertTGAtoPNG_BottomToTopFlip(t *testing.T) {
	// 2x2 image, bottom-to-top (descriptor bit 5 = 0).
	const w, h = 2, 2
	descriptor := uint8(0x08) // bottom-to-top + 8 alpha bits (bit 5 = 0)

	header := buildTGAHeader(2, w, h, 32, descriptor)

	// In bottom-to-top TGA, the first row of pixel data is the BOTTOM row of the image.
	// Pixel data order (as stored in file):
	//   Row 0 in file → bottom row of image (y=1 in output)
	//   Row 1 in file → top row of image    (y=0 in output)

	type pixel struct {
		r, g, b, a uint8
	}

	bottomRow := []pixel{{255, 0, 0, 255}, {0, 255, 0, 255}} // file row 0 → image bottom
	topRow := []pixel{{0, 0, 255, 255}, {255, 255, 0, 255}}   // file row 1 → image top

	var pixelData []byte
	for _, p := range bottomRow {
		pixelData = append(pixelData, bgraPixel(p.r, p.g, p.b, p.a)...)
	}
	for _, p := range topRow {
		pixelData = append(pixelData, bgraPixel(p.r, p.g, p.b, p.a)...)
	}

	tgaBytes := append(header, pixelData...)
	pngBytes, err := convertTGAtoPNG(tgaBytes)
	if err != nil {
		t.Fatalf("convertTGAtoPNG: %v", err)
	}

	img, err := png.Decode(bytes.NewReader(pngBytes))
	if err != nil {
		t.Fatalf("png.Decode: %v", err)
	}

	// After flip, top row of PNG should be topRow, bottom row should be bottomRow.
	for x := 0; x < w; x++ {
		// Top row (y=0) should be topRow
		nrgba := color.NRGBAModel.Convert(img.At(x, 0)).(color.NRGBA)
		exp := topRow[x]
		if nrgba.R != exp.r || nrgba.G != exp.g || nrgba.B != exp.b || nrgba.A != exp.a {
			t.Errorf("top pixel(%d,0): expected RGBA(%d,%d,%d,%d), got RGBA(%d,%d,%d,%d)",
				x, exp.r, exp.g, exp.b, exp.a,
				nrgba.R, nrgba.G, nrgba.B, nrgba.A)
		}

		// Bottom row (y=1) should be bottomRow
		nrgba = color.NRGBAModel.Convert(img.At(x, 1)).(color.NRGBA)
		exp = bottomRow[x]
		if nrgba.R != exp.r || nrgba.G != exp.g || nrgba.B != exp.b || nrgba.A != exp.a {
			t.Errorf("bottom pixel(%d,1): expected RGBA(%d,%d,%d,%d), got RGBA(%d,%d,%d,%d)",
				x, exp.r, exp.g, exp.b, exp.a,
				nrgba.R, nrgba.G, nrgba.B, nrgba.A)
		}
	}
}

func TestConvertTGAtoPNG_Error_8BitBPP(t *testing.T) {
	header := buildTGAHeader(2, 4, 4, 8, 0x20)
	pixelData := make([]byte, 4*4) // 8-bit = 1 byte per pixel
	tgaBytes := append(header, pixelData...)

	_, err := convertTGAtoPNG(tgaBytes)
	if err == nil {
		t.Fatal("expected error for 8-bit TGA")
	}
	if got := err.Error(); !containsSubstring(got, "only 24 and 32 supported") {
		t.Errorf("expected error containing %q, got: %s", "only 24 and 32 supported", got)
	}
}

func TestConvertTGAtoPNG_Error_GrayscaleType3(t *testing.T) {
	header := buildTGAHeader(3, 4, 4, 32, 0x28)
	pixelData := make([]byte, 4*4*4) // enough data
	tgaBytes := append(header, pixelData...)

	_, err := convertTGAtoPNG(tgaBytes)
	if err == nil {
		t.Fatal("expected error for grayscale TGA type 3 with 32bpp")
	}
	if got := err.Error(); !containsSubstring(got, "only 8 supported") {
		t.Errorf("expected error containing %q, got: %s", "only 8 supported", got)
	}
}

func TestConvertTGAtoPNG_Error_TruncatedData(t *testing.T) {
	// Only partial header.
	_, err := convertTGAtoPNG([]byte{0x00, 0x00, 0x02})
	if err == nil {
		t.Fatal("expected error for truncated TGA data")
	}
}

func TestConvertTGAtoPNG_GrayscaleType3(t *testing.T) {
	// 4x4 8-bit grayscale uncompressed TGA (type 3), top-to-bottom.
	const w, h = 4, 4
	descriptor := uint8(0x20) // top-to-bottom, no alpha bits

	header := buildTGAHeader(3, w, h, 8, descriptor)

	// Build 16 grayscale pixel values: 0, 17, 34, ..., 255
	var pixelData []byte
	for i := 0; i < w*h; i++ {
		pixelData = append(pixelData, uint8(i*17))
	}

	tgaBytes := append(header, pixelData...)
	pngBytes, err := convertTGAtoPNG(tgaBytes)
	if err != nil {
		t.Fatalf("convertTGAtoPNG: %v", err)
	}

	img, err := png.Decode(bytes.NewReader(pngBytes))
	if err != nil {
		t.Fatalf("png.Decode: %v", err)
	}

	bounds := img.Bounds()
	if bounds.Dx() != w || bounds.Dy() != h {
		t.Fatalf("expected %dx%d, got %dx%d", w, h, bounds.Dx(), bounds.Dy())
	}

	// Verify every pixel has R=G=B=original_value, A=255.
	for y := 0; y < h; y++ {
		for x := 0; x < w; x++ {
			idx := y*w + x
			expected := uint8(idx * 17)
			nrgba := color.NRGBAModel.Convert(img.At(x, y)).(color.NRGBA)
			if nrgba.R != expected || nrgba.G != expected || nrgba.B != expected || nrgba.A != 255 {
				t.Errorf("pixel(%d,%d): expected RGBA(%d,%d,%d,255), got RGBA(%d,%d,%d,%d)",
					x, y, expected, expected, expected,
					nrgba.R, nrgba.G, nrgba.B, nrgba.A)
			}
		}
	}
}

func TestConvertTGAtoPNG_GrayscaleRLEType11(t *testing.T) {
	// 4x1 8-bit grayscale RLE TGA (type 11), top-to-bottom.
	// One RLE packet (2 pixels of value 128) + one raw packet (2 pixels of values 64, 192).
	const w, h = 4, 1
	descriptor := uint8(0x20) // top-to-bottom, no alpha bits

	header := buildTGAHeader(11, w, h, 8, descriptor)

	var pixelData []byte

	// RLE packet: header byte = 0x81 (bit 7 set = RLE, count = 1+1 = 2), pixel = 128
	pixelData = append(pixelData, 0x81)
	pixelData = append(pixelData, 128)

	// Raw packet: header byte = 0x01 (bit 7 clear = raw, count = 1+1 = 2), pixels = 64, 192
	pixelData = append(pixelData, 0x01)
	pixelData = append(pixelData, 64)
	pixelData = append(pixelData, 192)

	tgaBytes := append(header, pixelData...)
	pngBytes, err := convertTGAtoPNG(tgaBytes)
	if err != nil {
		t.Fatalf("convertTGAtoPNG: %v", err)
	}

	img, err := png.Decode(bytes.NewReader(pngBytes))
	if err != nil {
		t.Fatalf("png.Decode: %v", err)
	}

	bounds := img.Bounds()
	if bounds.Dx() != w || bounds.Dy() != h {
		t.Fatalf("expected %dx%d, got %dx%d", w, h, bounds.Dx(), bounds.Dy())
	}

	expected := []uint8{128, 128, 64, 192}
	for x := 0; x < w; x++ {
		nrgba := color.NRGBAModel.Convert(img.At(x, 0)).(color.NRGBA)
		exp := expected[x]
		if nrgba.R != exp || nrgba.G != exp || nrgba.B != exp || nrgba.A != 255 {
			t.Errorf("pixel(%d,0): expected RGBA(%d,%d,%d,255), got RGBA(%d,%d,%d,%d)",
				x, exp, exp, exp,
				nrgba.R, nrgba.G, nrgba.B, nrgba.A)
		}
	}
}

func TestConvertTGAtoPNG_Error_GrayscaleType3_16bit(t *testing.T) {
	header := buildTGAHeader(3, 4, 4, 16, 0x20)
	pixelData := make([]byte, 4*4*2) // 16-bit = 2 bytes per pixel
	tgaBytes := append(header, pixelData...)

	_, err := convertTGAtoPNG(tgaBytes)
	if err == nil {
		t.Fatal("expected error for 16-bit grayscale TGA type 3")
	}
	if got := err.Error(); !containsSubstring(got, "only 8 supported") {
		t.Errorf("expected error containing %q, got: %s", "only 8 supported", got)
	}
}

// containsSubstring checks if s contains substr.
func containsSubstring(s, substr string) bool {
	return bytes.Contains([]byte(s), []byte(substr))
}
