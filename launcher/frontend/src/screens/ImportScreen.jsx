import React, { useEffect, useState } from "react";
import {
  BrowseForDirectory,
  CheckDownloadStatus,
  DetectQ3Installations,
  DownloadFreeResources,
  HasAcceptedEula,
  StartImport,
} from "../../wailsjs/go/main/App";
import { EventsOn, EventsOff } from "../../wailsjs/runtime/runtime";
import Button from "../components/Button";
import ExternalLink from "../components/ExternalLink";
import Logo from "../components/Logo";

const styles = {
  container: {
    flex: 1,
    minHeight: 0,
    display: "flex",
    flexDirection: "column",
  },
  header: {
    display: "flex",
    alignItems: "center",
    gap: "16px",
    padding: "16px 40px",
    background: "rgba(0, 0, 0, 0.3)",
    borderBottom: "1px solid var(--border)",
  },
  headerText: {
    display: "flex",
    flexDirection: "column",
    gap: "4px",
  },
  title: {
    fontSize: "22px",
    fontWeight: 700,
  },
  subtitle: {
    fontSize: "13px",
    color: "var(--text-secondary)",
    lineHeight: 1.5,
  },
  body: {
    flex: 1,
    minHeight: 0,
    display: "flex",
    flexDirection: "column",
    gap: "24px",
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
  label: {
    fontSize: "13px",
    fontWeight: 500,
    color: "var(--text-secondary)",
    marginBottom: "8px",
  },
  inputRow: {
    display: "flex",
    gap: "8px",
  },
  input: {
    flex: 1,
    padding: "10px 14px",
    fontSize: "13px",
    fontFamily: "monospace",
    background: "var(--bg-card)",
    border: "1px solid var(--border)",
    borderRadius: "var(--radius)",
    color: "var(--text-primary)",
    outline: "none",
  },
  browseBtn: {
    padding: "10px 20px",
    fontSize: "13px",
    fontWeight: 500,
    background: "var(--bg-card)",
    border: "1px solid var(--border)",
    borderRadius: "var(--radius)",
    color: "var(--text-primary)",
    cursor: "pointer",
    transition: "all var(--transition)",
    fontFamily: "inherit",
    flexShrink: 0,
  },
  helpText: {
    fontSize: "12px",
    color: "var(--text-muted)",
    lineHeight: 1.6,
    marginTop: "8px",
  },
  actions: {
    display: "flex",
    gap: "12px",
    justifyContent: "flex-end",
    padding: "16px 40px",
    borderTop: "1px solid var(--border)",
  },
  error: {
    color: "var(--accent)",
    fontSize: "13px",
  },
  downloadStatus: {
    display: "flex",
    alignItems: "center",
    gap: "12px",
    marginTop: "8px",
  },
  downloadReady: {
    fontSize: "13px",
    color: "var(--text-secondary)",
    fontWeight: 500,
  },
  downloadMessage: {
    fontSize: "12px",
    color: "var(--text-muted)",
    marginTop: "4px",
  },
};

export default function ImportScreen({ onImportComplete, onBack, onEula, importError, autoDownload }) {
  const [q3aInstallations, setQ3aInstallations] = useState([]);
  const [q3aPath, setQ3aPath] = useState("");
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [downloadStatus, setDownloadStatus] = useState(null);
  const [downloading, setDownloading] = useState(false);
  const [downloadMessage, setDownloadMessage] = useState("");

  useEffect(() => {
    Promise.all([DetectQ3Installations(), CheckDownloadStatus()])
      .then(([q3aFound, dlStatus]) => {
        setQ3aInstallations(q3aFound || []);
        setDownloadStatus(dlStatus);
        if (q3aFound && q3aFound.length > 0) {
          setQ3aPath(q3aFound[0].path);
        }
        setLoading(false);
      })
      .catch((err) => {
        setError(err);
        setLoading(false);
      });
  }, []);

  // Listen for download events
  useEffect(() => {
    const offProgress = EventsOn("download:progress", (data) => {
      setDownloadMessage(data.message || "");
    });
    const offComplete = EventsOn("download:complete", () => {
      setDownloading(false);
      setDownloadMessage("");
      CheckDownloadStatus().then(setDownloadStatus);
    });
    const offError = EventsOn("download:error", (msg) => {
      setDownloading(false);
      setDownloadMessage("");
      setError(msg);
    });
    return () => {
      offProgress();
      offComplete();
      offError();
    };
  }, []);

  const startDownload = async () => {
    setError(null);
    try {
      setDownloading(true);
      await DownloadFreeResources();
    } catch (err) {
      setDownloading(false);
      setError(err.toString());
    }
  };

  const handleDownload = async () => {
    const accepted = await HasAcceptedEula();
    if (!accepted) {
      onEula();
      return;
    }
    startDownload();
  };

  // Auto-trigger download after EULA acceptance.
  useEffect(() => {
    if (autoDownload && !loading && !downloading) {
      startDownload();
    }
  }, [autoDownload, loading]); // eslint-disable-line react-hooks/exhaustive-deps

  const handleBrowseQ3A = async () => {
    try {
      const dir = await BrowseForDirectory();
      if (dir) setQ3aPath(dir);
    } catch (err) {
      setError(err.toString());
    }
  };

  const handleAutoDetect = () => {
    setLoading(true);
    DetectQ3Installations().then((q3aFound) => {
      setQ3aInstallations(q3aFound || []);
      if (q3aFound && q3aFound.length > 0) {
        setQ3aPath(q3aFound[0].path);
      }
      setLoading(false);
    });
  };

  const handleImport = async () => {
    if (!downloadStatus?.ready) return;
    setError(null);
    try {
      await StartImport(q3aPath);
      onImportComplete();
    } catch (err) {
      setError(err.toString());
    }
  };

  if (loading) {
    return (
      <div
        style={{
          ...styles.container,
          alignItems: "center",
          justifyContent: "center",
        }}
      >
        <div className="spinner" />
      </div>
    );
  }

  const isDownloaded = downloadStatus?.ready;

  return (
    <div style={styles.container} className="screen-enter screen-enter-active">
      <div style={styles.header}>
        <Logo size="small" />
        <div style={styles.headerText}>
          <h2 style={styles.title}>Import Assets</h2>
          <p style={styles.subtitle}>
            q3now requires original game files. These are not included due to
            licensing.
          </p>
        </div>
      </div>

      <div style={styles.body}>
        {/* Download Free Resources */}
        <div style={styles.section}>
          <div style={styles.sectionTitle}>
            Download Free Resources
          </div>
          <div style={{ ...styles.helpText, marginTop: 0, marginBottom: "12px" }}>
            Download free demo files to get started without purchasing the full
            game.
          </div>
          <div style={styles.downloadStatus}>
            {isDownloaded ? (
              <span style={styles.downloadReady}>
                &#10003; Downloaded
              </span>
            ) : (
              <Button
                variant="secondary"
                disabled={downloading}
                onClick={handleDownload}
              >
                {downloading ? "Downloading..." : "Download"}
              </Button>
            )}
          </div>
          {downloadMessage && (
            <div style={styles.downloadMessage}>{downloadMessage}</div>
          )}
        </div>

        {/* Q3A Installation Path (optional) */}
        <div style={styles.section}>
          <div style={styles.sectionTitle}>
            Quake 3 Arena Installation Path (optional)
          </div>
          <div style={styles.inputRow}>
            <input
              type="text"
              style={styles.input}
              value={q3aPath}
              onChange={(e) =>
                setQ3aPath(e.target.value)}
              placeholder="/path/to/Quake 3 Arena"
            />
            <button style={styles.browseBtn} onClick={handleBrowseQ3A}>
              Browse
            </button>
          </div>
          <div style={styles.helpText}>
            {q3aInstallations.length === 0 && (
              <span style={{ color: "var(--accent)" }}>
                Not detected.{" "}
              </span>
            )}
            Providing the full game adds extra maps, textures, and models.
            Purchase on{" "}
            <ExternalLink href="https://store.steampowered.com/app/2200/Quake_III_Arena/">
              Steam
            </ExternalLink>{" "}
            or{" "}
            <ExternalLink href="https://www.gog.com/game/quake_iii_arena">
              GoG
            </ExternalLink>
            .
          </div>
        </div>

        {importError && <div className="selectable" style={styles.error}>{importError}</div>}
        {error && <div className="selectable" style={styles.error}>{error}</div>}
      </div>

      <div style={styles.actions}>
        {onBack && (
          <Button
            variant="secondary"
            onClick={onBack}
            style={{ marginRight: "auto" }}
          >
            Back
          </Button>
        )}
        <Button variant="secondary" onClick={handleAutoDetect}>
          Auto-Detect
        </Button>
        <Button disabled={!isDownloaded || downloading} onClick={handleImport}>
          Import
        </Button>
      </div>
    </div>
  );
}
