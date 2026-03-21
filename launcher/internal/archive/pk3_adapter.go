package archive

import "github.com/eser/q3now/launcher/internal/pk3"

// pk3Reader wraps pk3.Reader to implement the archive.Reader interface.
type pk3Reader struct {
	inner   *pk3.Reader
	entries []Entry
}

func openPK3(path string) (Reader, error) {
	r, err := pk3.Open(path)
	if err != nil {
		return nil, err
	}

	// Map pk3.Entry → archive.Entry once upfront.
	pk3Entries := r.Entries()
	entries := make([]Entry, len(pk3Entries))
	for i, e := range pk3Entries {
		entries[i] = Entry{
			Path:             e.Path,
			CompressedSize:   e.CompressedSize,
			UncompressedSize: e.UncompressedSize,
			CRC32:            e.CRC32,
		}
	}

	return &pk3Reader{inner: r, entries: entries}, nil
}

func (r *pk3Reader) Entries() []Entry    { return r.entries }
func (r *pk3Reader) Close() error        { return r.inner.Close() }

func (r *pk3Reader) ReadFile(idx int) ([]byte, error) {
	return r.inner.ReadFile(idx)
}
