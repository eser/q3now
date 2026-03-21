import React, { useEffect, useState } from "react";
import { GetDedStatus, LaunchGame } from "../../wailsjs/go/main/App";
import { EventsOn, Quit } from "../../wailsjs/runtime/runtime";
import CoverAnimation from "../components/CoverAnimation";
import ErrorOverlay from "../components/ErrorOverlay";
import ExternalLink from "../components/ExternalLink";
import Logo from "../components/Logo";

const styles = {
  container: {
    flex: 1,
    display: "flex",
    flexDirection: "row",
    overflow: "hidden",
  },
  sidebar: {
    width: "260px",
    flexShrink: 0,
    display: "flex",
    flexDirection: "column",
    padding: "24px",
    gap: "8px",
    background: "rgba(0,0,0,0.3)",
    borderRight: "1px solid var(--border)",
  },
  logoArea: {
    paddingBottom: "20px",
    borderBottom: "1px solid var(--border)",
    marginBottom: "8px",
  },
  menuButton: {
    width: "100%",
    padding: "12px 16px",
    fontSize: "14px",
    fontWeight: 500,
    textAlign: "left",
    background: "var(--bg-card)",
    color: "var(--text-primary)",
    border: "1px solid var(--border)",
    borderRadius: "var(--radius)",
    cursor: "pointer",
    transition: "all var(--transition)",
    fontFamily: "inherit",
    display: "flex",
    alignItems: "center",
    gap: "8px",
  },
  menuButtonPrimary: {
    background: "var(--accent)",
    borderColor: "var(--accent)",
    color: "#fff",
    fontWeight: 600,
  },
  menuButtonDisabled: {
    opacity: 0.4,
    cursor: "not-allowed",
  },
  shortcut: {
    marginLeft: "auto",
    fontSize: "11px",
    opacity: 0.4,
    fontFamily: "monospace",
  },
  statusDot: {
    width: "8px",
    height: "8px",
    borderRadius: "50%",
    flexShrink: 0,
  },
  spacer: { flex: 1 },
  creditsButton: {
    width: "100%",
    padding: "10px 16px",
    fontSize: "13px",
    fontWeight: 500,
    textAlign: "left",
    background: "var(--bg-card)",
    color: "var(--text-primary)",
    border: "1px solid var(--border)",
    borderRadius: "var(--radius)",
    cursor: "pointer",
    transition: "all var(--transition)",
    fontFamily: "inherit",
    display: "flex",
    alignItems: "center",
    gap: "8px",
  },
  quitButton: {
    width: "100%",
    padding: "10px 16px",
    fontSize: "13px",
    textAlign: "left",
    background: "transparent",
    color: "var(--text-secondary)",
    border: "1px solid var(--border)",
    borderRadius: "var(--radius)",
    cursor: "pointer",
    transition: "all var(--transition)",
    fontFamily: "inherit",
    display: "flex",
    alignItems: "center",
  },
  coverArea: {
    flex: 1,
    display: "flex",
    flexDirection: "column",
    alignItems: "center",
    justifyContent: "center",
    position: "relative",
    overflow: "hidden",
  },
  coverLogo: {
    fontSize: "64px",
    fontWeight: 800,
    letterSpacing: "4px",
    lineHeight: 1,
    userSelect: "none",
  },
  meta: {
    fontSize: "11px",
    color: "var(--text-muted)",
    textAlign: "center",
    marginTop: "16px",
  },
  githubLink: {
    position: "absolute",
    bottom: "24px",
    fontSize: "12px",
    color: "var(--text-secondary)",
    textDecoration: "none",
    transition: "color var(--transition)",
  },
  errorPosition: {
    position: "absolute",
    bottom: "52px",
    left: "16px",
    right: "16px",
    zIndex: 10,
  },
};

export default function LaunchScreen({
  appState,
  onGameOptions,
  onDedicated,
  onImportAssets,
  onCredits,
}) {
  const [error, setError] = useState(null);
  const [dedRunning, setDedRunning] = useState(false);
  const assetsReady = appState.assetsReady;

  useEffect(() => {
    GetDedStatus().then(setDedRunning);
    const unsub = EventsOn("ded:stopped", () => setDedRunning(false));
    return unsub;
  }, []);

  // Keyboard shortcuts.
  useEffect(() => {
    const handler = (e) => {
      if (e.target.tagName === "INPUT" || e.target.tagName === "TEXTAREA") {
        return;
      }
      switch (e.key.toLowerCase()) {
        case "l":
          if (assetsReady) handleLaunch();
          break;
        case "o":
          onGameOptions();
          break;
        case "s":
          onDedicated();
          break;
        case "i":
          onImportAssets();
          break;
        case "t":
          onCredits();
          break;
        case "q":
          Quit();
          break;
      }
    };
    window.addEventListener("keydown", handler);
    return () => window.removeEventListener("keydown", handler);
  }, [assetsReady, onGameOptions, onDedicated, onImportAssets, onCredits]);

  const handleLaunch = async () => {
    if (!assetsReady) return;
    try {
      await LaunchGame();
    } catch (err) {
      setError(err.toString());
    }
  };

  const formattedDate = appState.importedAt
    ? new Date(appState.importedAt).toLocaleDateString(undefined, {
      year: "numeric",
      month: "short",
      day: "numeric",
    })
    : null;

  return (
    <div style={styles.container} className="screen-enter screen-enter-active">
      <div style={styles.sidebar}>
        <div style={styles.logoArea}>
          <Logo />
        </div>

        <button
          style={{
            ...styles.menuButton,
            ...styles.menuButtonPrimary,
            ...(!assetsReady ? styles.menuButtonDisabled : {}),
          }}
          disabled={!assetsReady}
          onClick={handleLaunch}
        >
          Quick Launch Game
          <span style={styles.shortcut}>L</span>
        </button>

        <button
          style={styles.menuButton}
          onClick={onGameOptions}
        >
          Launch with Options
          <span style={styles.shortcut}>O</span>
        </button>

        <button style={styles.menuButton} onClick={onDedicated}>
          {dedRunning && (
            <div
              style={{ ...styles.statusDot, background: "var(--success)" }}
            />
          )}
          Host Dedicated Server
          <span style={styles.shortcut}>S</span>
        </button>

        <button style={styles.menuButton} onClick={onImportAssets}>
          Import Assets
          <span style={styles.shortcut}>I</span>
        </button>

        <div style={styles.spacer} />

        <button style={styles.creditsButton} onClick={onCredits}>
          Credits
          <span style={styles.shortcut}>T</span>
        </button>

        <button
          style={styles.quitButton}
          onClick={() =>
            Quit()}
        >
          Quit
          <span style={styles.shortcut}>Q</span>
        </button>
      </div>

      <div style={styles.coverArea}>
        <CoverAnimation />
        <div style={{ textAlign: "center", zIndex: 1 }}>
          <div style={styles.coverLogo}>
            <span style={{ color: "var(--accent)" }}>q3</span>
            <span style={{ color: "var(--text-primary)", opacity: 0.3 }}>
              now
            </span>
          </div>
          <div style={styles.meta}>
            {appState.version && <div>v{appState.version}</div>}
            {formattedDate && <div>Assets imported {formattedDate}</div>}
            {!assetsReady && (
              <div
                style={{
                  color: "var(--text-secondary)",
                  marginTop: "12px",
                  fontSize: "13px",
                  lineHeight: 1.6,
                  maxWidth: "360px",
                }}
              >
                q3now requires original game files to run. Navigate to{" "}
                <span
                  style={{
                    color: "var(--text-primary)",
                    cursor: "pointer",
                    textDecoration: "underline",
                  }}
                  onClick={onImportAssets}
                >
                  Import Assets
                </span>{" "}
                and start the importing process.
              </div>
            )}
          </div>
        </div>
        <div style={styles.errorPosition}>
          <ErrorOverlay message={error} />
        </div>
        <ExternalLink
          href="https://github.com/eser/q3now"
          style={styles.githubLink}
        >
          ★ Star our GitHub Repository
        </ExternalLink>
      </div>
    </div>
  );
}
