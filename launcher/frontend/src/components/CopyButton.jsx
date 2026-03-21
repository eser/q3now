import React, { useState } from "react";
import { ClipboardSetText } from "../../wailsjs/runtime/runtime";

const style = {
  padding: "8px 12px",
  fontSize: "12px",
  background: "var(--bg-card)",
  border: "1px solid var(--border)",
  borderRadius: "var(--radius)",
  color: "var(--text-primary)",
  cursor: "pointer",
  fontFamily: "inherit",
  flexShrink: 0,
  transition: "all var(--transition)",
  minWidth: "60px",
  textAlign: "center",
};

const copiedStyle = {
  ...style,
  borderColor: "var(--success)",
  color: "var(--success)",
};

export default function CopyButton({ text }) {
  const [copied, setCopied] = useState(false);

  const handleCopy = () => {
    ClipboardSetText(text);
    setCopied(true);
    setTimeout(() => setCopied(false), 1500);
  };

  return (
    <button style={copied ? copiedStyle : style} onClick={handleCopy}>
      {copied ? "Copied!" : "Copy"}
    </button>
  );
}
