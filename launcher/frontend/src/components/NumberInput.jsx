import React from "react";

// Shares visual language with .select-wrap: 32px right zone, 1px left border.
const styles = {
  wrap: {
    display: "flex",
    alignItems: "stretch",
    border: "1px solid var(--border)",
    borderRadius: "var(--radius)",
    background: "var(--bg-card)",
    overflow: "hidden",
  },
  input: {
    flex: 1,
    padding: "8px 12px",
    fontSize: "13px",
    fontFamily: "inherit",
    background: "transparent",
    border: "none",
    color: "var(--text-primary)",
    outline: "none",
    textAlign: "center",
    MozAppearance: "textfield",
  },
  btns: {
    display: "flex",
    flexDirection: "column",
    width: "32px",
    flexShrink: 0,
    borderLeft: "1px solid var(--border)",
  },
  btn: {
    flex: 1,
    display: "flex",
    alignItems: "center",
    justifyContent: "center",
    background: "transparent",
    border: "none",
    color: "var(--text-secondary)",
    cursor: "pointer",
    fontSize: "8px",
    padding: 0,
    fontFamily: "inherit",
    lineHeight: 1,
  },
  btnUp: {
    borderBottom: "1px solid var(--border)",
  },
};

export default function NumberInput({ value, onChange, min, max, style }) {
  const handleUp = () => onChange(Math.min((value || 0) + 1, max || 999));
  const handleDown = () => onChange(Math.max((value || 0) - 1, min || 0));

  return (
    <div style={{ ...styles.wrap, ...style }}>
      <input
        type="number"
        style={styles.input}
        value={value}
        min={min}
        max={max}
        onChange={(e) => onChange(parseInt(e.target.value) || min || 0)}
      />
      <div style={styles.btns}>
        <button
          style={{ ...styles.btn, ...styles.btnUp }}
          onClick={handleUp}
          type="button"
        >
          ▲
        </button>
        <button style={styles.btn} onClick={handleDown} type="button">
          ▼
        </button>
      </div>
    </div>
  );
}
