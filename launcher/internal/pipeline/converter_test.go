package pipeline

import "testing"

func TestRegisterAndLookupConverter(t *testing.T) {
	// Register a test converter for a unique pair so it doesn't conflict with init().
	called := false
	RegisterConverter("bmp", "jpg", func(src []byte) ([]byte, error) {
		called = true
		return []byte("converted"), nil
	})

	fn, err := LookupConverter("bmp", "jpg")
	if err != nil {
		t.Fatalf("LookupConverter(bmp, jpg): unexpected error: %v", err)
	}
	if fn == nil {
		t.Fatal("LookupConverter(bmp, jpg): returned nil function")
	}

	result, err := fn([]byte("input"))
	if err != nil {
		t.Fatalf("converter call: %v", err)
	}
	if !called {
		t.Error("expected converter function to be called")
	}
	if string(result) != "converted" {
		t.Errorf("expected %q, got %q", "converted", string(result))
	}
}

func TestLookupConverter_Unregistered(t *testing.T) {
	_, err := LookupConverter("wav", "mp3")
	if err == nil {
		t.Fatal("expected error for unregistered converter pair")
	}
	if got := err.Error(); got != "pipeline: no converter registered for wav->mp3" {
		t.Errorf("unexpected error message: %s", got)
	}
}

func TestTGAtoPNGConverterRegistered(t *testing.T) {
	// The init() in converter_tga.go should have registered tga->png.
	fn, err := LookupConverter("tga", "png")
	if err != nil {
		t.Fatalf("LookupConverter(tga, png): %v", err)
	}
	if fn == nil {
		t.Fatal("LookupConverter(tga, png): returned nil function")
	}
}
