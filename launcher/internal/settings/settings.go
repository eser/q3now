package settings

import (
	"encoding/json"
	"log/slog"
	"os"
	"path/filepath"
	"sort"
	"time"
)

const maxRecentServers = 10

// Settings holds user preferences persisted in settings.json.
type Settings struct {
	PlayerName     string         `json:"playerName"`
	Renderer       string         `json:"renderer"`
	CustomArgs     string         `json:"customArgs"`
	RecentServers  []RecentServer `json:"recentServers"`
	EulaAcceptedAt string         `json:"eulaAcceptedAt,omitempty"`
}

// RecentServer tracks a previously connected server address.
type RecentServer struct {
	Address  string `json:"address"`
	LastUsed string `json:"lastUsed"`
}

// Read loads settings from the given path. Returns defaults if file is
// missing or corrupt.
func Read(path string) *Settings {
	data, err := os.ReadFile(path)
	if err != nil {
		return defaults()
	}

	var s Settings
	if err := json.Unmarshal(data, &s); err != nil {
		slog.Warn("corrupt settings.json, using defaults", "error", err)
		return defaults()
	}

	return &s
}

// Write persists settings to the given path using atomic write.
//
// Per launcher convention (see docs/launcher.md "Writer self-bootstrap"):
// writers must os.MkdirAll their parent before writing. There is no
// centralized HomeDir bootstrap; each writer is independently responsible.
func Write(path string, s *Settings) error {
	data, err := json.MarshalIndent(s, "", "  ")
	if err != nil {
		return err
	}

	dir := filepath.Dir(path)
	if err := os.MkdirAll(dir, 0755); err != nil {
		slog.Error("failed to create settings directory", "dir", dir, "error", err)
		return err
	}

	tmp := path + ".tmp"
	if err := os.WriteFile(tmp, data, 0644); err != nil {
		slog.Error("failed to write settings", "path", tmp, "error", err)
		return err
	}
	return os.Rename(tmp, path)
}

// AddRecentServer adds or updates a server in the recent list. Newest first,
// capped at maxRecentServers.
func (s *Settings) AddRecentServer(address string) {
	now := time.Now().UTC().Format(time.RFC3339)

	// Update existing entry if present.
	for i := range s.RecentServers {
		if s.RecentServers[i].Address == address {
			s.RecentServers[i].LastUsed = now
			s.sortRecent()
			return
		}
	}

	// Add new entry.
	s.RecentServers = append(s.RecentServers, RecentServer{
		Address:  address,
		LastUsed: now,
	})

	s.sortRecent()

	// Evict oldest if over limit.
	if len(s.RecentServers) > maxRecentServers {
		s.RecentServers = s.RecentServers[:maxRecentServers]
	}
}

func (s *Settings) sortRecent() {
	sort.Slice(s.RecentServers, func(i, j int) bool {
		return s.RecentServers[i].LastUsed > s.RecentServers[j].LastUsed
	})
}

func defaults() *Settings {
	return &Settings{
		PlayerName:    "",
		Renderer:      "vulkan",
		CustomArgs:    "",
		RecentServers: nil,
	}
}
