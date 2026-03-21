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
};

export default function ProgressScreen({ onComplete, onError }) {
  const [progress, setProgress] = useState({
    step: "",
    current: 0,
    total: 1,
    message: "",
  });

  useEffect(() => {
    const unsubProgress = EventsOn("import:progress", (data) => {
      setProgress(data);
    });
    const unsubComplete = EventsOn("import:complete", () => {
      onComplete();
    });
    const unsubError = EventsOn("import:error", (errMsg) => {
      onError(errMsg);
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

  return (
    <div style={styles.container} className="screen-enter screen-enter-active">
      <div className="spinner" />

      <div style={styles.stepName}>{progress.step}</div>
      <div style={styles.message}>{progress.message}</div>

      <div style={styles.progressContainer}>
        <div className="progress-bar">
          <div
            className="progress-bar-fill"
            style={{ width: `${percent}%` }}
          />
        </div>
        <div style={styles.percent}>{percent}%</div>
      </div>

      <Button
        variant="secondary"
        onClick={() =>
          CancelImport()}
        style={{ marginTop: "8px" }}
      >
        Cancel
      </Button>
    </div>
  );
}
