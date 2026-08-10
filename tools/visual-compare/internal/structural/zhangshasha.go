package structural

import (
	"strings"

	"github.com/eser/q3now/tools/visual-compare/internal/deltae"
)

// Cost model:
//
//   insert / delete  = 1.0
//   relabel          = 0.0  if nodeMatch(a, b)
//                    = 1.0  otherwise
//
// nodeMatch: type matches exactly AND rect overlap > 50% AND
// style.color CIEDE2000 < 10 AND text matches (for text nodes only).

const (
	insertCost = 1.0
	deleteCost = 1.0
	colorDeltaEThreshold = 10.0
	rectOverlapThreshold = 0.5
)

func nodeMatch(a, b *Node) bool {
	if a == nil || b == nil {
		return a == b
	}
	if a.Type != b.Type {
		return false
	}
	if rectOverlapFraction(a.Rect, b.Rect) < rectOverlapThreshold {
		return false
	}
	if a.Style.Color != "" && b.Style.Color != "" {
		if colorDeltaE(a.Style.Color, b.Style.Color) >= colorDeltaEThreshold {
			return false
		}
	}
	if a.Type == "text" && strings.TrimSpace(a.Text) != strings.TrimSpace(b.Text) {
		return false
	}
	return true
}

func relabelCost(a, b *Node) float64 {
	if nodeMatch(a, b) {
		return 0
	}
	return 1
}

func colorDeltaE(hexA, hexB string) float64 {
	ar, ag, ab, ok := parseHex(hexA)
	if !ok {
		return 100
	}
	br, bg, bb, ok := parseHex(hexB)
	if !ok {
		return 100
	}
	L1, a1, b1 := deltae.SRGBToLab(ar, ag, ab)
	L2, a2, b2 := deltae.SRGBToLab(br, bg, bb)
	return deltae.CIEDE2000(L1, a1, b1, L2, a2, b2)
}

func parseHex(s string) (r, g, b uint8, ok bool) {
	s = strings.TrimPrefix(strings.TrimSpace(s), "#")
	if len(s) == 3 {
		s = string([]byte{s[0], s[0], s[1], s[1], s[2], s[2]})
	}
	if len(s) != 6 {
		return 0, 0, 0, false
	}
	dec := func(c byte) (uint8, bool) {
		switch {
		case c >= '0' && c <= '9':
			return c - '0', true
		case c >= 'a' && c <= 'f':
			return c - 'a' + 10, true
		case c >= 'A' && c <= 'F':
			return c - 'A' + 10, true
		}
		return 0, false
	}
	h := [6]uint8{}
	for i := 0; i < 6; i++ {
		v, k := dec(s[i])
		if !k {
			return 0, 0, 0, false
		}
		h[i] = v
	}
	return h[0]<<4 | h[1], h[2]<<4 | h[3], h[4]<<4 | h[5], true
}

// flatten returns the post-order traversal of n's children-first nodes.  Used
// by Zhang-Shasha's leftmost-leaf / keyroot indexing.
type flatTree struct {
	nodes     []*Node
	parent    []int
	leftmost  []int
	keyroots  []int
}

func flatten(n *Node) flatTree {
	var ft flatTree
	if n == nil {
		return ft
	}
	postIdx := map[*Node]int{}
	var walk func(node *Node) int
	walk = func(node *Node) int {
		var leftLeaf int
		leftSet := false
		for i, c := range node.Children {
			ci := walk(c)
			if i == 0 {
				leftLeaf = ft.leftmost[ci]
				leftSet = true
			}
		}
		idx := len(ft.nodes)
		ft.nodes = append(ft.nodes, node)
		ft.parent = append(ft.parent, -1)
		if !leftSet {
			ft.leftmost = append(ft.leftmost, idx)
		} else {
			ft.leftmost = append(ft.leftmost, leftLeaf)
		}
		postIdx[node] = idx
		for _, c := range node.Children {
			ft.parent[postIdx[c]] = idx
		}
		return idx
	}
	walk(n)
	seenLeftmost := map[int]bool{}
	for i := len(ft.nodes) - 1; i >= 0; i-- {
		l := ft.leftmost[i]
		if !seenLeftmost[l] {
			ft.keyroots = append([]int{i}, ft.keyroots...)
			seenLeftmost[l] = true
		}
	}
	return ft
}

// EditDistance computes Zhang-Shasha tree edit distance between two trees and
// returns the normalized ratio in [0, 1] (0 = identical, 1 = entirely
// different).  Empty trees collapse to ratio 0 (both empty) or 1 (one empty).
func EditDistance(a, b *Node) float64 {
	fa := flatten(a)
	fb := flatten(b)
	if len(fa.nodes) == 0 && len(fb.nodes) == 0 {
		return 0
	}
	if len(fa.nodes) == 0 || len(fb.nodes) == 0 {
		return 1
	}
	treeDist := make([][]float64, len(fa.nodes))
	for i := range treeDist {
		treeDist[i] = make([]float64, len(fb.nodes))
	}
	for _, ki := range fa.keyroots {
		for _, kj := range fb.keyroots {
			computeForestDist(fa, fb, ki, kj, treeDist)
		}
	}
	d := treeDist[len(fa.nodes)-1][len(fb.nodes)-1]
	maxSize := float64(len(fa.nodes))
	if float64(len(fb.nodes)) > maxSize {
		maxSize = float64(len(fb.nodes))
	}
	if maxSize == 0 {
		return 0
	}
	ratio := d / maxSize
	if ratio > 1 {
		return 1
	}
	if ratio < 0 {
		return 0
	}
	return ratio
}

func computeForestDist(fa, fb flatTree, i, j int, treeDist [][]float64) {
	li := fa.leftmost[i]
	lj := fb.leftmost[j]
	m := i - li + 2
	n := j - lj + 2
	fd := make([][]float64, m)
	for k := range fd {
		fd[k] = make([]float64, n)
	}
	fd[0][0] = 0
	for di := 1; di < m; di++ {
		fd[di][0] = fd[di-1][0] + deleteCost
	}
	for dj := 1; dj < n; dj++ {
		fd[0][dj] = fd[0][dj-1] + insertCost
	}
	for di := 1; di < m; di++ {
		for dj := 1; dj < n; dj++ {
			ai := li + di - 1
			bj := lj + dj - 1
			if fa.leftmost[ai] == li && fb.leftmost[bj] == lj {
				cost := relabelCost(fa.nodes[ai], fb.nodes[bj])
				fd[di][dj] = minF(
					fd[di-1][dj]+deleteCost,
					fd[di][dj-1]+insertCost,
					fd[di-1][dj-1]+cost,
				)
				treeDist[ai][bj] = fd[di][dj]
			} else {
				lai := fa.leftmost[ai] - li
				lbj := fb.leftmost[bj] - lj
				if lai < 0 {
					lai = 0
				}
				if lbj < 0 {
					lbj = 0
				}
				fd[di][dj] = minF(
					fd[di-1][dj]+deleteCost,
					fd[di][dj-1]+insertCost,
					fd[lai][lbj]+treeDist[ai][bj],
				)
			}
		}
	}
}

func minF(a, b, c float64) float64 {
	m := a
	if b < m {
		m = b
	}
	if c < m {
		m = c
	}
	return m
}
