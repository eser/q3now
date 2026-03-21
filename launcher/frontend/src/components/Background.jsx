import React from "react";

// Atmospheric background — dark with subtle geometric grid pattern
// and a radial accent glow. Pure CSS, no images.
const styles = {
  wrapper: {
    position: "fixed",
    inset: 0,
    zIndex: -1,
    overflow: "hidden",
    background: "#121218",
  },
  // Subtle grid pattern using repeating gradients.
  grid: {
    position: "absolute",
    inset: 0,
    backgroundImage: `
      linear-gradient(rgba(255,255,255,0.06) 1px, transparent 1px),
      linear-gradient(90deg, rgba(255,255,255,0.06) 1px, transparent 1px)
    `,
    backgroundSize: "40px 40px",
  },
  // Diagonal accent lines.
  diagonals: {
    position: "absolute",
    inset: 0,
    backgroundImage: `
      repeating-linear-gradient(
        45deg,
        transparent,
        transparent 80px,
        rgba(200, 64, 48, 0.06) 80px,
        rgba(200, 64, 48, 0.06) 81px
      )
    `,
  },
  // Central radial glow.
  glow: {
    position: "absolute",
    top: "30%",
    left: "50%",
    transform: "translate(-50%, -50%)",
    width: "600px",
    height: "600px",
    background:
      "radial-gradient(circle, rgba(200, 64, 48, 0.15) 0%, transparent 70%)",
    pointerEvents: "none",
  },
  // Bottom fade for depth.
  bottomFade: {
    position: "absolute",
    bottom: 0,
    left: 0,
    right: 0,
    height: "200px",
    background: "linear-gradient(transparent, rgba(18, 18, 24, 0.8))",
    pointerEvents: "none",
  },
  // Top vignette.
  vignette: {
    position: "absolute",
    inset: 0,
    background:
      "radial-gradient(ellipse at center, transparent 50%, rgba(0,0,0,0.4) 100%)",
    pointerEvents: "none",
  },
};

export default function Background() {
  return (
    <div style={styles.wrapper}>
      <div style={styles.grid} />
      <div style={styles.diagonals} />
      <div style={styles.glow} />
      <div style={styles.bottomFade} />
      <div style={styles.vignette} />
    </div>
  );
}
