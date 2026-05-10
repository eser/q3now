import React, { useEffect, useState } from "react";
import {
  BuildGameCommand,
  GetSettings,
  LaunchGameWithArgs,
  SaveSettings,
} from "../../wailsjs/go/main/App";
import Button from "../components/Button";
import CopyButton from "../components/CopyButton";
import ErrorOverlay from "../components/ErrorOverlay";
import Logo from "../components/Logo";

const MAPS = ["arena1", "arena7", "arena17"];

const styles = {
  container: {
    flex: 1,
    display: "flex",
    flexDirection: "column",
    minHeight: 0,
  },
  header: {
    display: "flex",
    alignItems: "center",
    gap: "16px",
    padding: "16px 40px",
    background: "rgba(0,0,0,0.3)",
    borderBottom: "1px solid var(--border)",
  },
  title: { fontSize: "22px", fontWeight: 700 },
  subtitle: { fontSize: "13px", color: "var(--text-secondary)" },
  body: {
    flex: 1,
    minHeight: 0,
    display: "flex",
    flexDirection: "column",
    gap: "16px",
    padding: "24px 40px",
    overflowY: "auto",
  },
  section: {
    border: "1px solid var(--border)",
    borderRadius: "var(--radius)",
    padding: "16px",
  },
  sectionTitle: {
    fontSize: "11px",
    textTransform: "uppercase",
    letterSpacing: "2px",
    color: "var(--text-muted)",
    marginBottom: "12px",
  },
  row: {
    display: "flex",
    alignItems: "center",
    gap: "12px",
    marginBottom: "10px",
  },
  label: {
    width: "80px",
    fontSize: "13px",
    color: "var(--text-secondary)",
    flexShrink: 0,
  },
  input: {
    flex: 1,
    padding: "8px 12px",
    fontSize: "13px",
    fontFamily: "inherit",
    background: "var(--bg-card)",
    border: "1px solid var(--border)",
    borderRadius: "var(--radius)",
    color: "var(--text-primary)",
    outline: "none",
  },
  select: {
    flex: 1,
    padding: "8px 12px",
    fontSize: "13px",
    fontFamily: "inherit",
    background: "var(--bg-card)",
    border: "1px solid var(--border)",
    borderRadius: "var(--radius)",
    color: "var(--text-primary)",
    outline: "none",
  },
  radioGroup: { display: "flex", gap: "16px", flex: 1 },
  radioLabel: {
    fontSize: "13px",
    color: "var(--text-primary)",
    cursor: "pointer",
    display: "flex",
    alignItems: "center",
    gap: "6px",
  },
  separator: {
    textAlign: "center",
    color: "var(--text-muted)",
    fontSize: "12px",
    margin: "4px 0",
  },
  recentItem: {
    display: "flex",
    alignItems: "center",
    justifyContent: "space-between",
    padding: "6px 0",
    fontSize: "13px",
    color: "var(--text-secondary)",
  },
  recentBtn: {
    padding: "4px 10px",
    fontSize: "11px",
    background: "var(--bg-card)",
    border: "1px solid var(--border)",
    borderRadius: "4px",
    color: "var(--text-primary)",
    cursor: "pointer",
    fontFamily: "inherit",
  },
  commandBox: {
    display: "flex",
    gap: "8px",
    alignItems: "center",
  },
  commandText: {
    flex: 1,
    padding: "8px 12px",
    fontSize: "12px",
    fontFamily: "monospace",
    background: "var(--bg-card)",
    border: "1px solid var(--border)",
    borderRadius: "var(--radius)",
    color: "var(--text-muted)",
    whiteSpace: "nowrap",
    overflow: "hidden",
    textOverflow: "ellipsis",
  },
  copyBtn: {
    padding: "8px 12px",
    fontSize: "12px",
    background: "var(--bg-card)",
    border: "1px solid var(--border)",
    borderRadius: "var(--radius)",
    color: "var(--text-primary)",
    cursor: "pointer",
    fontFamily: "inherit",
    flexShrink: 0,
  },
  actions: {
    display: "flex",
    gap: "12px",
    padding: "16px 40px",
    borderTop: "1px solid var(--border)",
    justifyContent: "flex-end",
  },
};

export default function GameOptionsScreen({ assetsReady, onBack }) {
  const [mapName, setMapName] = useState("");
  const [connect, setConnect] = useState("");
  const [renderer, setRenderer] = useState("vulkan");
  const [playerName, setPlayerName] = useState("");
  const [customArgs, setCustomArgs] = useState("");
  const [recentServers, setRecentServers] = useState([]);
  const [command, setCommand] = useState("");
  const [error, setError] = useState(null);

  // Load saved settings.
  useEffect(() => {
    GetSettings().then((s) => {
      if (s.playerName) setPlayerName(s.playerName);
      if (s.renderer) setRenderer(s.renderer);
      if (s.customArgs) setCustomArgs(s.customArgs);
      if (s.recentServers) setRecentServers(s.recentServers || []);
    });
  }, []);

  // Update command preview when form changes.
  useEffect(() => {
    const opts = { renderer, playerName, customArgs };
    if (connect) {
      opts.connect = connect;
    } else if (mapName) {
      opts.map = mapName;
    }
    BuildGameCommand(opts).then(setCommand);
  }, [mapName, connect, renderer, playerName, customArgs]);

  const handleLaunch = async () => {
    // Save settings before launch.
    const s = { playerName, renderer, customArgs, recentServers };
    await SaveSettings(s);

    // Build args.
    const args = [];
    if (renderer) args.push("+set", "cl_renderer", renderer);
    if (playerName) args.push("+set", "name", playerName);
    if (connect) {
      args.push("+connect", connect);
    } else if (mapName) {
      args.push("+map", mapName);
    }
    if (customArgs) args.push(...customArgs.split(/\s+/).filter(Boolean));

    try {
      await LaunchGameWithArgs(args);
    } catch (err) {
      setError(err.toString());
    }
  };

  const timeAgo = (dateStr) => {
    if (!dateStr) return "";
    const diff = Date.now() - new Date(dateStr).getTime();
    const mins = Math.floor(diff / 60000);
    if (mins < 60) return `${mins}m ago`;
    const hrs = Math.floor(mins / 60);
    if (hrs < 24) return `${hrs}h ago`;
    return `${Math.floor(hrs / 24)}d ago`;
  };

  return (
    <div style={styles.container} className="screen-enter screen-enter-active">
      <div style={styles.header}>
        <Logo size="small" />
        <div>
          <h2 style={styles.title}>Launch Game</h2>
          <p style={styles.subtitle}>Configure launch options</p>
        </div>
      </div>

      <div style={styles.body}>
        <div style={styles.section}>
          <div style={styles.sectionTitle}>Play</div>
          <div style={styles.row}>
            <span style={styles.label}>Map</span>
            <div className="select-wrap">
              <select
                style={styles.select}
                value={mapName}
                onChange={(e) => {
                  setMapName(e.target.value);
                  setConnect("");
                }}
              >
                <option value="">— none (open game menu) —</option>
                {MAPS.map((m) => (
                  <option key={m} value={m}>
                    {m}
                  </option>
                ))}
              </select>
            </div>
          </div>
          <div style={styles.separator}>— or —</div>
          <div style={styles.row}>
            <span style={styles.label}>Connect</span>
            <input
              style={styles.input}
              value={connect}
              onChange={(e) => {
                setConnect(e.target.value);
                setMapName("");
              }}
              placeholder="192.168.1.5:27960"
            />
          </div>
        </div>

        <div style={styles.section}>
          <div style={styles.sectionTitle}>Settings</div>
          <div style={styles.row}>
            <span style={styles.label}>Renderer</span>
            <div style={styles.radioGroup}>
              <label style={styles.radioLabel}>
                <input
                  type="radio"
                  value="vulkan"
                  checked={renderer === "vulkan"}
                  onChange={() => setRenderer("vulkan")}
                />{" "}
                Vulkan
              </label>
              <label style={styles.radioLabel}>
                <input
                  type="radio"
                  value="opengl"
                  checked={renderer === "opengl"}
                  onChange={() => setRenderer("opengl")}
                />{" "}
                OpenGL
              </label>
            </div>
          </div>
          <div style={styles.row}>
            <span style={styles.label}>Player</span>
            <input
              style={styles.input}
              value={playerName}
              onChange={(e) => setPlayerName(e.target.value)}
              placeholder="UnnamedPlayer"
            />
          </div>
        </div>

        {recentServers.length > 0 && (
          <div style={styles.section}>
            <div style={styles.sectionTitle}>Recent Servers</div>
            {recentServers.map((s) => (
              <div key={s.address} style={styles.recentItem}>
                <span>{s.address}</span>
                <span style={{ color: "var(--text-muted)", fontSize: "11px" }}>
                  {timeAgo(s.lastUsed)}
                </span>
                <button
                  style={styles.recentBtn}
                  onClick={() => {
                    setConnect(s.address);
                    setMapName("");
                  }}
                >
                  Connect
                </button>
              </div>
            ))}
          </div>
        )}

        <div style={styles.row}>
          <span style={styles.label}>Custom</span>
          <input
            style={styles.input}
            value={customArgs}
            onChange={(e) => setCustomArgs(e.target.value)}
            placeholder="+set com_maxfps 250"
          />
        </div>

        <div>
          <div style={{ ...styles.sectionTitle, marginBottom: "8px" }}>
            Command
          </div>
          <div style={styles.commandBox}>
            <div style={styles.commandText} title={command}>
              {command}
            </div>
            <CopyButton text={command} />
          </div>
        </div>

        <ErrorOverlay message={error} />
      </div>

      <div style={styles.actions}>
        <Button
          variant="secondary"
          onClick={onBack}
          style={{ marginRight: "auto" }}
        >
          Back
        </Button>
        <Button disabled={!assetsReady} onClick={handleLaunch}>
          Launch
        </Button>
      </div>
    </div>
  );
}
