import React from "react";

// q3now text logo — clean, bold, game-styled.
export default function Logo({ size = "default" }) {
  const fontSize = size === "small" ? "20px" : "28px";

  return (
    <div
      style={{
        fontWeight: 800,
        fontSize,
        letterSpacing: "2px",
        textTransform: "lowercase",
        lineHeight: 1,
        userSelect: "none",
      }}
    >
      <span style={{ color: "var(--accent)" }}>q3</span>
      <span style={{ color: "var(--text-primary)" }}>now</span>
    </div>
  );
}
