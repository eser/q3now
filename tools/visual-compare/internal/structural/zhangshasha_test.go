package structural

import (
	"testing"
)

func leaf(typ string, r Rect) *Node { return &Node{Type: typ, Rect: r} }

func TestIdentical(t *testing.T) {
	a := &Node{Type: "container", Rect: Rect{0, 0, 100, 100}, Children: []*Node{
		leaf("text", Rect{10, 10, 50, 20}),
		leaf("rect", Rect{0, 30, 100, 70}),
	}}
	d := EditDistance(a, a)
	if d > 0.001 {
		t.Fatalf("identical trees should be 0, got %f", d)
	}
}

func TestEntirelyDifferent(t *testing.T) {
	a := &Node{Type: "container", Rect: Rect{0, 0, 100, 100}}
	b := &Node{Type: "text", Rect: Rect{500, 500, 10, 10}, Text: "x"}
	// One node each, completely non-matching — relabel cost 1, normalized
	// over max(1,1) = 1.
	d := EditDistance(a, b)
	if d < 0.99 {
		t.Fatalf("entirely different trees should be ~1.0, got %f", d)
	}
}

func TestOneEmpty(t *testing.T) {
	a := &Node{Type: "container", Rect: Rect{0, 0, 10, 10}}
	d := EditDistance(a, nil)
	if d < 0.99 {
		t.Fatalf("one-empty should be 1.0, got %f", d)
	}
}

func TestNodeMatchByRectOverlap(t *testing.T) {
	a := &Node{Type: "rect", Rect: Rect{0, 0, 100, 100}, Style: Style{Color: "#ff0000"}}
	b := &Node{Type: "rect", Rect: Rect{5, 5, 100, 100}, Style: Style{Color: "#ff0102"}}
	d := EditDistance(a, b)
	if d > 0.01 {
		t.Fatalf("close rects + similar colors should match, got %f", d)
	}
}
