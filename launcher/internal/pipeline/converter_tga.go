package pipeline

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"image"
	"image/png"
)

func init() {
	RegisterConverter("tga", "png", convertTGAtoPNG)
}

// convertTGAtoPNG decodes a 24-bit or 32-bit TGA (uncompressed or RLE) and encodes as PNG.
// 32-bit preserves alpha losslessly; 24-bit gets alpha=255.
func convertTGAtoPNG(src []byte) ([]byte, error) {
	img, err := decodeTGA(src)
	if err != nil {
		return nil, fmt.Errorf("tga decode: %w", err)
	}

	var buf bytes.Buffer
	if err := png.Encode(&buf, img); err != nil {
		return nil, fmt.Errorf("png encode: %w", err)
	}
	return buf.Bytes(), nil
}

// tgaHeader represents the 18-byte TGA file header.
type tgaHeader struct {
	IDLength        uint8
	ColorMapType    uint8
	ImageType       uint8
	ColorMapOrigin  uint16
	ColorMapLength  uint16
	ColorMapDepth   uint8
	XOrigin         uint16
	YOrigin         uint16
	Width           uint16
	Height          uint16
	BitsPerPixel    uint8
	ImageDescriptor uint8
}

const tgaHeaderSize = 18

// decodeTGA decodes a TGA image: type 2 (uncompressed RGB), type 3 (uncompressed grayscale),
// type 10 (RLE RGB), or type 11 (RLE grayscale).
// Returns an NRGBA image with non-premultiplied alpha. 24-bit and 8-bit pixels get alpha=255.
func decodeTGA(data []byte) (*image.NRGBA, error) {
	if len(data) < tgaHeaderSize {
		return nil, fmt.Errorf("data too short for TGA header: %d bytes", len(data))
	}

	var hdr tgaHeader
	if err := binary.Read(bytes.NewReader(data[:tgaHeaderSize]), binary.LittleEndian, &hdr); err != nil {
		return nil, fmt.Errorf("reading TGA header: %w", err)
	}

	// Validate: no color map, supported image type and bit depth.
	if hdr.ColorMapType != 0 {
		return nil, fmt.Errorf("unsupported TGA color map type: %d (only 0 supported)", hdr.ColorMapType)
	}
	switch hdr.ImageType {
	case 2, 10: // RGB (uncompressed / RLE)
		if hdr.BitsPerPixel != 24 && hdr.BitsPerPixel != 32 {
			return nil, fmt.Errorf("unsupported TGA bits per pixel: %d for RGB type (only 24 and 32 supported)", hdr.BitsPerPixel)
		}
	case 3, 11: // Grayscale (uncompressed / RLE)
		if hdr.BitsPerPixel != 8 {
			return nil, fmt.Errorf("unsupported TGA bits per pixel: %d for grayscale type (only 8 supported)", hdr.BitsPerPixel)
		}
	default:
		return nil, fmt.Errorf("unsupported TGA image type: %d (only 2, 3, 10, 11 supported)", hdr.ImageType)
	}

	w := int(hdr.Width)
	h := int(hdr.Height)
	bpp := int(hdr.BitsPerPixel) / 8 // 3 or 4
	if w <= 0 || h <= 0 {
		return nil, fmt.Errorf("invalid TGA dimensions: %dx%d", w, h)
	}

	// Skip past the header and the ID field.
	pixelOffset := tgaHeaderSize + int(hdr.IDLength)
	if pixelOffset > len(data) {
		return nil, fmt.Errorf("TGA ID field extends beyond data")
	}
	pixelData := data[pixelOffset:]

	totalPixels := w * h
	// Decoded pixels stored as flat RGBA slice (always 4 bytes per pixel).
	rgba := make([]byte, totalPixels*4)

	switch hdr.ImageType {
	case 2, 3:
		if err := decodeTGAUncompressed(pixelData, rgba, totalPixels, bpp); err != nil {
			return nil, err
		}
	case 10, 11:
		if err := decodeTGARLE(pixelData, rgba, totalPixels, bpp); err != nil {
			return nil, err
		}
	}

	// Build the NRGBA image.
	img := image.NewNRGBA(image.Rect(0, 0, w, h))

	// Bit 5 of image descriptor: 0 = bottom-to-top (default TGA), 1 = top-to-bottom.
	topToBottom := (hdr.ImageDescriptor & 0x20) != 0

	for y := 0; y < h; y++ {
		srcRow := y
		if !topToBottom {
			// Flip: the first row in the pixel data is the bottom row of the image.
			srcRow = h - 1 - y
		}
		srcStart := srcRow * w * 4
		dstStart := y * img.Stride
		copy(img.Pix[dstStart:dstStart+w*4], rgba[srcStart:srcStart+w*4])
	}

	return img, nil
}

// decodeTGAUncompressed reads raw grayscale (8-bit), BGR (24-bit), or BGRA (32-bit) pixels and converts to RGBA.
func decodeTGAUncompressed(src []byte, dst []byte, totalPixels int, bpp int) error {
	needed := totalPixels * bpp
	if len(src) < needed {
		return fmt.Errorf("TGA uncompressed: expected %d bytes of pixel data, got %d", needed, len(src))
	}
	for i := 0; i < totalPixels; i++ {
		sOff := i * bpp
		dOff := i * 4
		if bpp == 1 {
			// Grayscale: R=G=B=pixel, A=255
			dst[dOff+0] = src[sOff]
			dst[dOff+1] = src[sOff]
			dst[dOff+2] = src[sOff]
			dst[dOff+3] = 255
		} else {
			dst[dOff+0] = src[sOff+2] // R
			dst[dOff+1] = src[sOff+1] // G
			dst[dOff+2] = src[sOff+0] // B
			if bpp == 4 {
				dst[dOff+3] = src[sOff+3]
			} else {
				dst[dOff+3] = 255
			}
		}
	}
	return nil
}

// decodeTGARLE decodes RLE-compressed grayscale (8-bit), BGR (24-bit), or BGRA (32-bit) pixels and converts to RGBA.
func decodeTGARLE(src []byte, dst []byte, totalPixels int, bpp int) error {
	srcPos := 0
	dstPixel := 0

	for dstPixel < totalPixels {
		if srcPos >= len(src) {
			return fmt.Errorf("TGA RLE: unexpected end of data at pixel %d of %d", dstPixel, totalPixels)
		}
		packetHeader := src[srcPos]
		srcPos++
		count := int(packetHeader&0x7F) + 1

		if dstPixel+count > totalPixels {
			return fmt.Errorf("TGA RLE: packet would exceed image size (pixel %d + count %d > %d)", dstPixel, count, totalPixels)
		}

		if (packetHeader & 0x80) != 0 {
			// RLE packet: one pixel repeated count times.
			if srcPos+bpp > len(src) {
				return fmt.Errorf("TGA RLE: unexpected end of data reading RLE pixel")
			}
			if bpp == 1 {
				// Grayscale
				g := src[srcPos]
				srcPos++
				for i := 0; i < count; i++ {
					off := (dstPixel + i) * 4
					dst[off+0] = g
					dst[off+1] = g
					dst[off+2] = g
					dst[off+3] = 255
				}
			} else {
				b := src[srcPos+0]
				g := src[srcPos+1]
				r := src[srcPos+2]
				a := uint8(255)
				if bpp == 4 {
					a = src[srcPos+3]
				}
				srcPos += bpp

				for i := 0; i < count; i++ {
					off := (dstPixel + i) * 4
					dst[off+0] = r
					dst[off+1] = g
					dst[off+2] = b
					dst[off+3] = a
				}
			}
		} else {
			// Raw packet: count pixels.
			needed := count * bpp
			if srcPos+needed > len(src) {
				return fmt.Errorf("TGA RLE: unexpected end of data reading %d raw pixels", count)
			}
			if bpp == 1 {
				// Grayscale
				for i := 0; i < count; i++ {
					g := src[srcPos+i]
					off := (dstPixel + i) * 4
					dst[off+0] = g
					dst[off+1] = g
					dst[off+2] = g
					dst[off+3] = 255
				}
			} else {
				for i := 0; i < count; i++ {
					sOff := srcPos + i*bpp
					dOff := (dstPixel + i) * 4
					dst[dOff+0] = src[sOff+2] // R
					dst[dOff+1] = src[sOff+1] // G
					dst[dOff+2] = src[sOff+0] // B
					if bpp == 4 {
						dst[dOff+3] = src[sOff+3]
					} else {
						dst[dOff+3] = 255
					}
				}
			}
			srcPos += needed
		}

		dstPixel += count
	}

	return nil
}
