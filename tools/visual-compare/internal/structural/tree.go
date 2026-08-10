// Package structural compares Clay-emitted layout trees against DOM-extracted
// trees via Zhang-Shasha tree edit distance, normalized to [0,1] by max tree
// size. Both sides share a single JSON schema (see tests/visual/README.md).
package structural

import (
	"encoding/json"
	"math"
	"os"
)

// Rect mirrors {x,y,w,h} from both Clay and DOM dumps.
type Rect struct {
	X float64 `json:"x"`
	Y float64 `json:"y"`
	W float64 `json:"w"`
	H float64 `json:"h"`
}

// Style mirrors the foreground/background color + border + padding info.
// Colors are hex strings (e.g. "#a4f0c8") when present, empty when absent.
type Style struct {
	Color   string `json:"color,omitempty"`
	Bg      string `json:"bg,omitempty"`
	Border  int    `json:"border,omitempty"`
	Padding [4]int `json:"padding,omitempty"`
}

// Node is one entry in the layout tree — either an internal container or a
// leaf (text/image/rect).  Children are ordered (sibling order matters for
// Zhang-Shasha).
type Node struct {
	ID       string  `json:"node_id,omitempty"`
	Type     string  `json:"type"`
	Rect     Rect    `json:"rect"`
	Style    Style   `json:"style,omitempty"`
	Text     string  `json:"text,omitempty"`
	Children []*Node `json:"children,omitempty"`
}

// Load reads a Clay/DOM JSON dump from disk.
func Load(path string) (*Node, error) {
	b, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	var n Node
	if err := json.Unmarshal(b, &n); err != nil {
		return nil, err
	}
	return &n, nil
}

// Size returns the total node count of the tree.
func Size(n *Node) int {
	if n == nil {
		return 0
	}
	s := 1
	for _, c := range n.Children {
		s += Size(c)
	}
	return s
}

// Filter returns a deep copy of the tree containing only nodes whose Rect
// intersects the given region rect.  An internal node is kept if it or any of
// its descendants intersects.  Used for per-region structural diff.
func Filter(n *Node, rx, ry, rw, rh float64) *Node {
	if n == nil {
		return nil
	}
	var kept []*Node
	for _, c := range n.Children {
		if f := Filter(c, rx, ry, rw, rh); f != nil {
			kept = append(kept, f)
		}
	}
	selfHit := rectsOverlap(n.Rect, Rect{X: rx, Y: ry, W: rw, H: rh})
	if !selfHit && len(kept) == 0 {
		return nil
	}
	cp := *n
	cp.Children = kept
	return &cp
}

func rectsOverlap(a, b Rect) bool {
	return !(a.X+a.W <= b.X || b.X+b.W <= a.X || a.Y+a.H <= b.Y || b.Y+b.H <= a.Y)
}

// rectOverlapFraction returns |A∩B| / min(|A|, |B|).  Used by the node-match
// criterion ("rect overlap > 50%").
func rectOverlapFraction(a, b Rect) float64 {
	ix := math.Max(a.X, b.X)
	iy := math.Max(a.Y, b.Y)
	ax := math.Min(a.X+a.W, b.X+b.W)
	ay := math.Min(a.Y+a.H, b.Y+b.H)
	iw := ax - ix
	ih := ay - iy
	if iw <= 0 || ih <= 0 {
		return 0
	}
	inter := iw * ih
	aArea := a.W * a.H
	bArea := b.W * b.H
	if aArea <= 0 || bArea <= 0 {
		return 0
	}
	minArea := math.Min(aArea, bArea)
	if minArea == 0 {
		return 0
	}
	return inter / minArea
}
