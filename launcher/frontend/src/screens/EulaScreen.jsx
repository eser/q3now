import React, { useEffect, useRef, useState } from "react";
import { AcceptEula, GetEulaText } from "../../wailsjs/go/main/App";
import Button from "../components/Button";
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
    padding: "24px 40px",
    gap: "16px",
  },
  eulaBox: {
    flex: 1,
    minHeight: 0,
    padding: "16px",
    background: "var(--bg-card)",
    border: "1px solid var(--border)",
    borderRadius: "var(--radius)",
    overflowY: "auto",
    fontSize: "12px",
    lineHeight: 1.7,
    color: "var(--text-secondary)",
    fontFamily: "monospace",
    whiteSpace: "pre-wrap",
    WebkitUserSelect: "text",
    userSelect: "text",
  },
  scrollHint: {
    fontSize: "12px",
    color: "var(--text-muted)",
    textAlign: "center",
  },
  actions: {
    display: "flex",
    gap: "12px",
    justifyContent: "flex-end",
    padding: "16px 40px",
    borderTop: "1px solid var(--border)",
  },
};

export default function EulaScreen({ onAccept, onDecline }) {
  const [eulaText, setEulaText] = useState("");
  const [scrolledToBottom, setScrolledToBottom] = useState(false);
  const boxRef = useRef(null);

  useEffect(() => {
    GetEulaText().then(setEulaText);
  }, []);

  const handleScroll = () => {
    const el = boxRef.current;
    if (!el) return;
    // Consider "scrolled to bottom" when within 20px of the end.
    const atBottom = el.scrollHeight - el.scrollTop - el.clientHeight < 20;
    if (atBottom && !scrolledToBottom) {
      setScrolledToBottom(true);
    }
  };

  const handleAccept = async () => {
    await AcceptEula();
    onAccept();
  };

  return (
    <div style={styles.container} className="screen-enter screen-enter-active">
      <div style={styles.header}>
        <Logo size="small" />
        <div style={styles.headerText}>
          <h2 style={styles.title}>License Agreement</h2>
          <p style={styles.subtitle}>
            Please read and accept id Software's EULA before downloading game
            assets.
          </p>
        </div>
      </div>

      <div style={styles.body}>
        <div
          ref={boxRef}
          style={styles.eulaBox}
          className="selectable"
          onScroll={handleScroll}
        >
          {eulaText || "Loading..."}
        </div>
        {!scrolledToBottom && (
          <div style={styles.scrollHint}>
            Scroll to the bottom to enable Accept
          </div>
        )}
      </div>

      <div style={styles.actions}>
        <Button
          variant="secondary"
          onClick={onDecline}
          style={{ marginRight: "auto" }}
        >
          Decline
        </Button>
        <Button disabled={!scrolledToBottom} onClick={handleAccept}>
          Accept
        </Button>
      </div>
    </div>
  );
}
