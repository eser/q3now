import React, { useEffect, useState } from "react";
import {
  BrowseForDirectory,
  DetectQ3Installations,
  DetectQ1Installations,
  StartImport,
} from "../../wailsjs/go/main/App";
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
};

export default function ImportScreen({ onImportComplete, onBack }) {
  const [q3aInstallations, setQ3aInstallations] = useState([]);
  const [q1Installations, setQ1Installations] = useState([]);
  const [q3aPath, setQ3aPath] = useState("");
  const [q1Path, setQ1Path] = useState("");
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  useEffect(() => {
    Promise.all([DetectQ3Installations(), DetectQ1Installations()])
      .then(([q3aFound, q1Found]) => {
        setQ3aInstallations(q3aFound || []);
        setQ1Installations(q1Found || []);
        if (q3aFound && q3aFound.length > 0) {
          setQ3aPath(q3aFound[0].path);
        }
        if (q1Found && q1Found.length > 0) {
          setQ1Path(q1Found[0].path);
        }
        setLoading(false);
      })
      .catch((err) => {
        setError(err);
        setLoading(false);
      });
  }, []);

  const handleBrowseQ3A = async () => {
    try {
      const dir = await BrowseForDirectory();
      if (dir) setQ3aPath(dir);
    } catch (err) {
      setError(err.toString());
    }
  };

  const handleBrowseQ1 = async () => {
    try {
      const dir = await BrowseForDirectory();
      if (dir) setQ1Path(dir);
    } catch (err) {
      setError(err.toString());
    }
  };

  const handleAutoDetect = () => {
    setLoading(true);
    Promise.all([DetectQ3Installations(), DetectQ1Installations()])
      .then(([q3aFound, q1Found]) => {
        setQ3aInstallations(q3aFound || []);
        setQ1Installations(q1Found || []);
        if (q3aFound && q3aFound.length > 0) {
          setQ3aPath(q3aFound[0].path);
        }
        if (q1Found && q1Found.length > 0) {
          setQ1Path(q1Found[0].path);
        }
        setLoading(false);
      });
  };

  const handleImport = async () => {
    if (!q3aPath) return;
    setError(null);
    try {
      await StartImport(q3aPath, q1Path);
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
        <div style={styles.section}>
          <div style={styles.sectionTitle}>
            Quake 3 Arena / Quake Live Installation Path (required)
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
            Don't own Quake 3 Arena? Purchase on{" "}
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

        {/* Q1 import hidden for this release — will be re-enabled next release
        <div style={styles.section}>
          <div style={styles.sectionTitle}>
            Quake 1 Installation Path (optional)
          </div>
          <div style={styles.inputRow}>
            <input
              type="text"
              style={styles.input}
              value={q1Path}
              onChange={(e) =>
                setQ1Path(e.target.value)}
              placeholder="/path/to/Quake"
            />
            <button style={styles.browseBtn} onClick={handleBrowseQ1}>
              Browse
            </button>
          </div>
          <div style={styles.helpText}>
            {q1Installations.length === 0 && (
              <span style={{ color: "var(--accent)" }}>
                Not detected.{" "}
              </span>
            )}
            Don't own Quake? Purchase on{" "}
            <ExternalLink href="https://store.steampowered.com/app/2310/Quake/">
              Steam
            </ExternalLink>{" "}
            or{" "}
            <ExternalLink href="https://www.gog.com/game/quake_the_offering">
              GoG
            </ExternalLink>
            .
          </div>
        </div>
        */}

        {error && <div style={styles.error}>{error}</div>}
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
        <Button disabled={!q3aPath} onClick={handleImport}>
          Import
        </Button>
      </div>
    </div>
  );
}
