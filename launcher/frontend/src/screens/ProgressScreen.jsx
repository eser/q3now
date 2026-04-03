import React, { useEffect, useState } from "react";
import { EventsOn } from "../../wailsjs/runtime/runtime";
import { CancelImport } from "../../wailsjs/go/main/App";
import Button from "../components/Button";

const styles = {
  container: {
    flex: 1,
    display: "flex",
    flexDirection: "column",
    alignItems: "center",
    justifyContent: "center",
    padding: "0 40px 32px",
    gap: "24px",
  },
  stepName: {
    fontSize: "12px",
    textTransform: "uppercase",
    letterSpacing: "2px",
    color: "var(--text-secondary)",
  },
  message: {
    fontSize: "14px",
    color: "var(--text-primary)",
  },
  progressContainer: {
    width: "100%",
    maxWidth: "400px",
    display: "flex",
    flexDirection: "column",
    gap: "8px",
  },
  percent: {
    fontSize: "13px",
    color: "var(--text-muted)",
    textAlign: "right",
  },
  checkmark: {
    fontSize: "48px",
    color: "var(--success, #4ade80)",
    lineHeight: 1,
  },
  errorIcon: {
    fontSize: "48px",
    color: "var(--accent, #ef4444)",
    lineHeight: 1,
  },
  errorMessage: {
    fontSize: "13px",
    color: "var(--accent, #ef4444)",
    textAlign: "center",
    maxWidth: "400px",
    lineHeight: 1.5,
    wordBreak: "break-word",
  },
  errorContainer: {
    flex: 1,
    minHeight: 0,
    display: "flex",
    flexDirection: "column",
    padding: "24px 40px 0",
    gap: "16px",
  },
  errorHeader: {
    display: "flex",
    flexDirection: "column",
    alignItems: "center",
    gap: "12px",
    flexShrink: 0,
  },
  errorDetailBox: {
    flex: 1,
    minHeight: 0,
    padding: "12px 16px",
    background: "var(--bg-card)",
    border: "1px solid var(--border)",
    borderRadius: "var(--radius)",
    overflowY: "auto",
    fontSize: "12px",
    lineHeight: 1.6,
    color: "var(--accent, #ef4444)",
    fontFamily: "monospace",
    whiteSpace: "pre-wrap",
    WebkitUserSelect: "text",
    userSelect: "text",
  },
  errorActions: {
    display: "flex",
    justifyContent: "flex-end",
    padding: "16px 40px",
    borderTop: "1px solid var(--border)",
    flexShrink: 0,
  },
};

export default function ProgressScreen({ onComplete, onError }) {
  const [progress, setProgress] = useState({
    step: "",
    current: 0,
    total: 1,
    message: "",
  });
  const [error, setError] = useState(null);

  useEffect(() => {
    const unsubProgress = EventsOn("import:progress", (data) => {
      setProgress(data);
    });
    const unsubComplete = EventsOn("import:complete", () => {
      onComplete();
    });
    const unsubError = EventsOn("import:error", (errMsg) => {
      setError(errMsg);
    });

    return () => {
      unsubProgress();
      unsubComplete();
      unsubError();
    };
  }, [onComplete, onError]);

  const percent = progress.total > 0
    ? Math.round((progress.current / progress.total) * 100)
    : 0;

  const isComplete = progress.step === "complete";

  if (error) {
    // Split into summary (first line) and details (rest).
    const lines = error.split("\n");
    const summary = lines[0];
    const details = lines.slice(1).join("\n").trim();

    return (
      <div style={{ flex: 1, minHeight: 0, display: "flex", flexDirection: "column" }} className="screen-enter screen-enter-active">
        <div style={styles.errorContainer}>
          <div style={styles.errorHeader}>
            <div style={styles.errorIcon}>&#10007;</div>
            <div style={styles.stepName}>Import Failed</div>
            <div style={styles.errorMessage}>{summary}</div>
          </div>
          {details && (
            <div style={styles.errorDetailBox}>{details}</div>
          )}
        </div>
        <div style={styles.errorActions}>
          <Button variant="secondary" onClick={() => onError(error)}>
            Back
          </Button>
        </div>
      </div>
    );
  }

  return (
    <div style={styles.container} className="screen-enter screen-enter-active">
      {isComplete
        ? <div style={styles.checkmark}>&#10003;</div>
        : <div className="spinner" />}

      <div style={styles.stepName}>{progress.step}</div>
      <div style={styles.message}>{progress.message}</div>

      {!isComplete && (
        <div style={styles.progressContainer}>
          <div className="progress-bar">
            <div
              className="progress-bar-fill"
              style={{ width: `${percent}%` }}
            />
          </div>
          <div style={styles.percent}>{percent}%</div>
        </div>
      )}

      {!isComplete && (
        <Button
          variant="secondary"
          onClick={() =>
            CancelImport()}
          style={{ marginTop: "8px" }}
        >
          Cancel
        </Button>
      )}
    </div>
  );
}
