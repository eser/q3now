package pipeline

import "fmt"

// ConverterFunc converts source file bytes to a target format.
// Returns the converted bytes or an error.
type ConverterFunc func(src []byte) ([]byte, error)

// converters is the package-level registry keyed by "srcFmt->dstFmt" (e.g., "tga->png").
var converters = map[string]ConverterFunc{}

// RegisterConverter registers a converter for a source->target format pair.
// Panics if a converter is already registered for the same pair (catch at init time).
func RegisterConverter(srcFmt, dstFmt string, fn ConverterFunc) {
	key := srcFmt + "->" + dstFmt
	if _, exists := converters[key]; exists {
		panic(fmt.Sprintf("pipeline: duplicate converter registered for %s", key))
	}
	converters[key] = fn
}

// LookupConverter returns the converter for a source->target format pair.
// Returns an error if no converter is registered.
func LookupConverter(srcFmt, dstFmt string) (ConverterFunc, error) {
	key := srcFmt + "->" + dstFmt
	fn, ok := converters[key]
	if !ok {
		return nil, fmt.Errorf("pipeline: no converter registered for %s", key)
	}
	return fn, nil
}

// ConvertOptions holds optional parameters for converters that support quality/preprocessing.
type ConvertOptions struct {
	Quality    string // "high", "medium", "low", or "" (default = high)
	Preprocess bool   // if true, pre-resample audio before encoding
}

// ConverterWithOptsFunc converts source file bytes to a target format with quality options.
type ConverterWithOptsFunc func(src []byte, opts ConvertOptions) ([]byte, error)

// convertersWithOpts is the package-level registry for options-aware converters.
var convertersWithOpts = map[string]ConverterWithOptsFunc{}

// RegisterConverterWithOpts registers an options-aware converter for a source->target format pair.
// Panics if a converter is already registered for the same pair (catch at init time).
func RegisterConverterWithOpts(srcFmt, dstFmt string, fn ConverterWithOptsFunc) {
	key := srcFmt + "->" + dstFmt
	if _, exists := convertersWithOpts[key]; exists {
		panic(fmt.Sprintf("pipeline: duplicate options-aware converter registered for %s", key))
	}
	convertersWithOpts[key] = fn
}

// LookupConverterWithOpts returns the options-aware converter for a source->target format pair.
func LookupConverterWithOpts(srcFmt, dstFmt string) (ConverterWithOptsFunc, bool) {
	key := srcFmt + "->" + dstFmt
	fn, ok := convertersWithOpts[key]
	return fn, ok
}
