import React, { useCallback, useEffect, useRef, useState } from "react";
import {
  GetDedCommandPreview,
  GetDedStatus,
  GetLocalIP,
  StartDedicated,
  StopDedicated,
} from "../../wailsjs/go/main/App";
import { EventsOn } from "../../wailsjs/runtime/runtime";
import Button from "../components/Button";
import CopyButton from "../components/CopyButton";
import ErrorOverlay from "../components/ErrorOverlay";
import Logo from "../components/Logo";
import NumberInput from "../components/NumberInput";

const PRESETS = [
  {
    label: "FFA q3dm17",
    hostname: "q3now FFA",
    map: "q3dm17",
    gameType: "dm",
    maxClients: 8,
    addBots: true,
    botCount: 3,
  },
  {
    label: "CTF q3ctf1",
    hostname: "q3now CTF",
    map: "q3ctf1",
    gameType: "ctf",
    maxClients: 16,
    addBots: true,
    botCount: 3,
  },
  {
    label: "TDM q3dm6",
    hostname: "q3now TDM",
    map: "q3dm6",
    gameType: "tdm",
    maxClients: 12,
    addBots: true,
    botCount: 3,
  },
];

const GAME_TYPES = [
  { value: "dm", label: "DM" },
  { value: "tdm", label: "TDM" },
  { value: "ctf", label: "CTF" },
];

const MAPS = [
  "q3dm1",
  "q3dm2",
  "q3dm3",
  "q3dm4",
  "q3dm5",
  "q3dm6",
  "q3dm7",
  "q3dm8",
  "q3dm9",
  "q3dm10",
  "q3dm11",
  "q3dm12",
  "q3dm13",
  "q3dm14",
  "q3dm15",
  "q3dm16",
  "q3dm17",
  "q3dm18",
  "q3dm19",
  "q3tourney1",
  "q3tourney2",
  "q3tourney3",
  "q3tourney4",
  "q3tourney5",
  "q3tourney6",
  "q3ctf1",
  "q3ctf2",
  "q3ctf3",
  "q3ctf4",
];

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
  presets: { display: "flex", gap: "8px", flexWrap: "wrap" },
  presetBtn: {
    padding: "6px 14px",
    fontSize: "12px",
    background: "var(--bg-card)",
    border: "1px solid var(--border)",
    borderRadius: "var(--radius)",
    color: "var(--text-primary)",
    cursor: "pointer",
    fontFamily: "inherit",
  },
  row: {
    display: "flex",
    alignItems: "center",
    gap: "12px",
    marginBottom: "8px",
  },
  label: {
    width: "100px",
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
  checkLabel: {
    fontSize: "13px",
    color: "var(--text-primary)",
    cursor: "pointer",
    display: "flex",
    alignItems: "center",
    gap: "6px",
  },
  commandBox: { display: "flex", gap: "8px", alignItems: "center" },
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
  controls: { display: "flex", gap: "12px" },
  statusBar: {
    display: "flex",
    alignItems: "center",
    gap: "16px",
    padding: "8px 0",
    fontSize: "13px",
  },
  statusDot: {
    width: "8px",
    height: "8px",
    borderRadius: "50%",
    flexShrink: 0,
  },
  logArea: {
    flex: 1,
    minHeight: "100px",
    maxHeight: "200px",
    overflowY: "auto",
    padding: "10px 14px",
    fontSize: "12px",
    fontFamily: "monospace",
    background: "var(--bg-card)",
    border: "1px solid var(--border)",
    borderRadius: "var(--radius)",
    color: "var(--text-muted)",
    lineHeight: 1.5,
  },
  actions: {
    display: "flex",
    gap: "12px",
    padding: "16px 40px",
    borderTop: "1px solid var(--border)",
    justifyContent: "flex-end",
  },
};

export default function DedicatedScreen({ assetsReady, onBack }) {
  const [config, setConfig] = useState({
    hostname: "q3now server",
    map: "q3dm17",
    gameType: "dm",
    maxClients: 16,
    password: "",
    addBots: true,
    botCount: 3,
  });
  const [running, setRunning] = useState(false);
  const [logs, setLogs] = useState([]);
  const [command, setCommand] = useState("");
  const [localIP, setLocalIP] = useState("");
  const [error, setError] = useState(null);
  const logRef = useRef(null);

  // Check initial status + get local IP.
  useEffect(() => {
    GetDedStatus().then(setRunning);
    GetLocalIP().then(setLocalIP);
  }, []);

  // Subscribe to ded events.
  useEffect(() => {
    const unsubLog = EventsOn("ded:log", (line) => {
      setLogs((prev) => [...prev.slice(-500), line]);
    });
    const unsubStop = EventsOn("ded:stopped", () => {
      setRunning(false);
    });
    return () => {
      unsubLog();
      unsubStop();
    };
  }, []);

  // Auto-scroll log.
  useEffect(() => {
    if (logRef.current) {
      logRef.current.scrollTop = logRef.current.scrollHeight;
    }
  }, [logs]);

  // Update command preview.
  useEffect(() => {
    GetDedCommandPreview(config).then(setCommand);
  }, [config]);

  const updateConfig = useCallback((key, value) => {
    setConfig((prev) => ({ ...prev, [key]: value }));
  }, []);

  const applyPreset = (preset) => {
    setConfig((prev) => ({ ...prev, ...preset }));
  };

  const handleStart = async () => {
    setError(null);
    setLogs([]);
    try {
      await StartDedicated(config);
      setRunning(true);
    } catch (err) {
      setError(err.toString());
    }
  };

  const handleStop = async () => {
    try {
      await StopDedicated();
    } catch (err) {
      setError(err.toString());
    }
  };

  return (
    <div style={styles.container} className="screen-enter screen-enter-active">
      <div style={styles.header}>
        <Logo size="small" />
        <div>
          <h2 style={styles.title}>Dedicated Server</h2>
          <p style={styles.subtitle}>Host a game server</p>
        </div>
      </div>

      <div style={styles.body}>
        <div>
          <div
            style={{
              fontSize: "11px",
              textTransform: "uppercase",
              letterSpacing: "2px",
              color: "var(--text-muted)",
              marginBottom: "8px",
            }}
          >
            Quick Load Presets
          </div>
          <div style={styles.presets}>
            {PRESETS.map((p) => (
              <button
                key={p.label}
                style={styles.presetBtn}
                onClick={() => applyPreset(p)}
              >
                {p.label}
              </button>
            ))}
          </div>
        </div>

        <div>
          <div style={styles.row}>
            <span style={styles.label}>Server Name</span>
            <input
              style={styles.input}
              value={config.hostname}
              onChange={(e) =>
                updateConfig("hostname", e.target.value)}
            />
          </div>
          <div style={styles.row}>
            <span style={styles.label}>Map</span>
            <div className="select-wrap">
              <select
                style={styles.select}
                value={config.map}
                onChange={(e) =>
                  updateConfig("map", e.target.value)}
              >
                {MAPS.map((m) => <option key={m} value={m}>{m}</option>)}
              </select>
            </div>
          </div>
          <div style={styles.row}>
            <span style={styles.label}>Game Type</span>
            <div className="select-wrap">
              <select
                style={styles.select}
                value={config.gameType}
                onChange={(e) => updateConfig("gameType", e.target.value)}
              >
                {GAME_TYPES.map((gt) => (
                  <option key={gt.value} value={gt.value}>{gt.label}</option>
                ))}
              </select>
            </div>
            <span style={{ ...styles.label, width: "40px" }}>Max</span>
            <NumberInput
              style={{ maxWidth: "80px" }}
              value={config.maxClients}
              min={2}
              max={64}
              onChange={(v) => updateConfig("maxClients", v)}
            />
          </div>
          <div style={styles.row}>
            <span style={styles.label}>Password</span>
            <input
              style={styles.input}
              value={config.password}
              onChange={(e) => updateConfig("password", e.target.value)}
              placeholder="optional"
            />
          </div>
          <div style={styles.row}>
            <span style={styles.label}></span>
            <label style={styles.checkLabel}>
              <input
                type="checkbox"
                checked={config.addBots}
                onChange={(e) => updateConfig("addBots", e.target.checked)}
              />
              Add {config.botCount} random bots
            </label>
          </div>
        </div>

        <div>
          <div
            style={{
              fontSize: "11px",
              textTransform: "uppercase",
              letterSpacing: "2px",
              color: "var(--text-muted)",
              marginBottom: "8px",
            }}
          >
            Command
          </div>
          <div style={styles.commandBox}>
            <div style={styles.commandText} title={command}>{command}</div>
            <CopyButton text={command} />
          </div>
        </div>

        <div style={styles.controls}>
          <Button
            disabled={!assetsReady || running || !config.map}
            onClick={handleStart}
          >
            Start Server
          </Button>
          <Button variant="secondary" disabled={!running} onClick={handleStop}>
            Stop Server
          </Button>
        </div>

        <ErrorOverlay message={error} />

        <div style={styles.statusBar}>
          <div
            style={{
              ...styles.statusDot,
              background: running ? "var(--success)" : "var(--text-muted)",
            }}
          />
          <span>{running ? "Running" : "Stopped"}</span>
          {running && localIP && (
            <>
              <span style={{ color: "var(--border)" }}>|</span>
              <span style={{ fontFamily: "monospace", fontSize: "12px" }}>
                /connect {localIP}:27960
              </span>
              <CopyButton text={`/connect ${localIP}:27960`} />
            </>
          )}
        </div>

        <div ref={logRef} className="selectable" style={styles.logArea}>
          {logs.length === 0 && (
            <span style={{ color: "var(--text-muted)" }}>
              Server log will appear here...
            </span>
          )}
          {logs.map((line, i) => <div key={i}>{line}</div>)}
        </div>
      </div>

      <div style={styles.actions}>
        <Button
          variant="secondary"
          onClick={onBack}
          style={{ marginRight: "auto" }}
        >
          Back
        </Button>
      </div>
    </div>
  );
}
