import React from "react";

const style = {
  border: "1px solid var(--border)",
  background: "var(--bg-card)",
  borderRadius: "var(--radius)",
  padding: "10px 14px",
  fontSize: "12px",
  fontFamily: "monospace",
  color: "var(--accent)",
  lineHeight: 1.5,
  wordBreak: "break-all",
  minHeight: "100px",
  overflowY: "auto",
};

export default function ErrorOverlay({ message }) {
  if (!message) return null;
  return <div className="selectable" style={style}>{message}</div>;
}
