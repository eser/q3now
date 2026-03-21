import React from "react";

const styles = {
  card: {
    padding: "14px 18px",
    background: "var(--bg-card)",
    border: "1px solid var(--border)",
    borderRadius: "var(--radius)",
    cursor: "pointer",
    transition: "all var(--transition)",
    display: "flex",
    alignItems: "center",
    gap: "12px",
  },
  cardSelected: {
    borderColor: "var(--accent)",
    background: "var(--bg-card-hover)",
  },
  radio: {
    width: "16px",
    height: "16px",
    borderRadius: "50%",
    border: "2px solid var(--border)",
    flexShrink: 0,
    display: "flex",
    alignItems: "center",
    justifyContent: "center",
    transition: "all var(--transition)",
  },
  radioSelected: {
    borderColor: "var(--accent)",
  },
  radioDot: {
    width: "8px",
    height: "8px",
    borderRadius: "50%",
    background: "var(--accent)",
  },
  info: {
    flex: 1,
    minWidth: 0,
  },
  type: {
    fontSize: "11px",
    textTransform: "uppercase",
    letterSpacing: "1px",
    color: "var(--text-secondary)",
    marginBottom: "2px",
  },
  path: {
    fontSize: "13px",
    color: "var(--text-muted)",
    overflow: "hidden",
    textOverflow: "ellipsis",
    whiteSpace: "nowrap",
  },
  badge: {
    fontSize: "10px",
    padding: "2px 6px",
    borderRadius: "4px",
    background: "var(--bg-secondary)",
    color: "var(--text-secondary)",
    flexShrink: 0,
  },
};

export default function DetectedPath({ installation, selected, onClick }) {
  const { path, sourceType, hasTA } = installation;

  return (
    <div
      style={{ ...styles.card, ...(selected ? styles.cardSelected : {}) }}
      onClick={onClick}
    >
      <div
        style={{ ...styles.radio, ...(selected ? styles.radioSelected : {}) }}
      >
        {selected && <div style={styles.radioDot} />}
      </div>
      <div style={styles.info}>
        <div style={styles.type}>{sourceType}</div>
        <div style={styles.path} title={path}>{path}</div>
      </div>
      {hasTA && <span style={styles.badge}>+ Team Arena</span>}
    </div>
  );
}
