import React from "react";

const styles = {
  button: {
    padding: "14px 48px",
    fontSize: "16px",
    fontWeight: 600,
    letterSpacing: "1px",
    textTransform: "uppercase",
    border: "none",
    borderRadius: "var(--radius)",
    cursor: "pointer",
    transition: "all var(--transition)",
    fontFamily: "inherit",
  },
  primary: {
    background: "var(--accent)",
    color: "#fff",
  },
  secondary: {
    background: "transparent",
    color: "var(--text-secondary)",
    border: "1px solid var(--border)",
  },
  disabled: {
    opacity: 0.5,
    cursor: "not-allowed",
  },
};

export default function Button(
  { children, variant = "primary", disabled, glow, onClick, style },
) {
  const variantStyle = variant === "primary"
    ? styles.primary
    : styles.secondary;

  return (
    <button
      className={glow && !disabled ? "btn-glow" : ""}
      style={{
        ...styles.button,
        ...variantStyle,
        ...(disabled ? styles.disabled : {}),
        ...style,
      }}
      disabled={disabled}
      onClick={onClick}
    >
      {children}
    </button>
  );
}
