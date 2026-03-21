import React, { useCallback, useEffect, useRef } from "react";
import eserPhoto from "../assets/images/eser.png";
import Button from "../components/Button";
import ExternalLink from "../components/ExternalLink";

const BASE_SPEED = 0.8; // px per frame (~48px/s at 60fps)

const styles = {
  container: {
    flex: 1,
    position: "relative",
    overflow: "hidden",
    background: "var(--bg-primary)",
  },
  scrollWrap: {
    position: "absolute",
    left: 0,
    right: 0,
    display: "flex",
    flexDirection: "column",
    alignItems: "center",
    paddingBottom: "200px",
  },
  section: {
    width: "100%",
    maxWidth: "480px",
    textAlign: "center",
    marginBottom: "48px",
  },
  sectionTitle: {
    fontSize: "11px",
    textTransform: "uppercase",
    letterSpacing: "4px",
    color: "var(--accent)",
    marginBottom: "20px",
  },
  logo: {
    fontSize: "56px",
    fontWeight: 800,
    letterSpacing: "4px",
    lineHeight: 1,
    marginBottom: "8px",
  },
  photo: {
    width: "120px",
    height: "120px",
    borderRadius: "50%",
    filter: "grayscale(1)",
    border: "2px solid var(--border)",
    marginBottom: "16px",
  },
  name: {
    fontSize: "18px",
    fontWeight: 600,
    color: "var(--text-primary)",
    marginBottom: "8px",
  },
  bio: {
    fontSize: "13px",
    color: "var(--text-secondary)",
    lineHeight: 1.7,
    marginBottom: "12px",
  },
  link: {
    fontSize: "13px",
    color: "var(--text-primary)",
    textDecoration: "underline",
  },
  credit: {
    fontSize: "14px",
    color: "var(--text-secondary)",
    lineHeight: 2,
  },
  role: {
    color: "var(--text-muted)",
    fontSize: "12px",
  },
  quote: {
    fontSize: "15px",
    fontStyle: "italic",
    color: "var(--text-muted)",
    marginTop: "8px",
  },
  backArea: {
    position: "absolute",
    bottom: "16px",
    left: "16px",
    zIndex: 10,
  },
  // Fade top and bottom edges.
  fadeTop: {
    position: "absolute",
    top: 0,
    left: 0,
    right: 0,
    height: "80px",
    background: "linear-gradient(var(--bg-primary), transparent)",
    zIndex: 5,
    pointerEvents: "none",
  },
  fadeBottom: {
    position: "absolute",
    bottom: 0,
    left: 0,
    right: 0,
    height: "120px",
    background: "linear-gradient(transparent, var(--bg-primary))",
    zIndex: 5,
    pointerEvents: "none",
  },
};

export default function CreditsScreen({ onBack }) {
  const scrollRef = useRef(null);
  const offsetRef = useRef(0);
  const animRef = useRef(null);
  const pausedUntilRef = useRef(0);

  // Start from below the viewport.
  useEffect(() => {
    if (scrollRef.current) {
      const containerH = scrollRef.current.parentElement.clientHeight;
      offsetRef.current = containerH;
    }
  }, []);

  // Animation loop.
  useEffect(() => {
    const animate = () => {
      if (!scrollRef.current) return;

      // Only auto-scroll if not paused.
      if (Date.now() >= pausedUntilRef.current) {
        offsetRef.current -= BASE_SPEED;
      }

      // Loop: when scrolled past all content, reset to below viewport.
      const contentH = scrollRef.current.scrollHeight;
      const containerH = scrollRef.current.parentElement.clientHeight;
      if (offsetRef.current < -contentH) {
        offsetRef.current = containerH;
      }

      scrollRef.current.style.transform = `translateY(${offsetRef.current}px)`;
      animRef.current = requestAnimationFrame(animate);
    };
    animRef.current = requestAnimationFrame(animate);
    return () => cancelAnimationFrame(animRef.current);
  }, []);

  // Wheel: nudge position + pause auto-scroll for 5 seconds.
  const handleWheel = useCallback((e) => {
    e.preventDefault();
    if (e.deltaY > 0) {
      offsetRef.current -= 3;
    } else {
      offsetRef.current += 6;
    }
    pausedUntilRef.current = Date.now() + 5000;
  }, []);

  return (
    <div style={styles.container} onWheel={handleWheel}>
      <div style={styles.fadeTop} />
      <div style={styles.fadeBottom} />

      <div ref={scrollRef} style={styles.scrollWrap}>
        {/* Logo */}
        <div style={{ ...styles.section, marginTop: "40px" }}>
          <div style={styles.logo}>
            <span style={{ color: "var(--accent)" }}>q3</span>
            <span style={{ color: "var(--text-primary)" }}>now</span>
          </div>
        </div>

        {/* Creator */}
        <div style={styles.section}>
          <div style={styles.sectionTitle}>Creator</div>
          <img src={eserPhoto} alt="Eser Ozvataf" style={styles.photo} />
          <div style={styles.name}>Eser "Laroux.PoS" Ozvataf</div>
          <div style={styles.bio}>
            A long time Doomer and Quake fan. Learned C by reading John
            Carmack's Quake 1 sources to make a Quake modification. Worked on
            idTech 1 through 3. In 2026, Quake 3 Arena is still the favorite
            game.
          </div>
          <ExternalLink href="https://eser.live" style={styles.link}>
            https://eser.live
          </ExternalLink>
        </div>

        {/* q3now Team */}
        <div style={styles.section}>
          <div style={styles.sectionTitle}>q3now Team</div>
          <div style={styles.credit}>Eser "Laroux.PoS" Ozvataf</div>
        </div>

        {/* Quake 3 Arena Team */}
        <div style={styles.section}>
          <div style={styles.sectionTitle}>Quake 3 Arena Team</div>
          {[
            ["John Carmack", "Programming"],
            ["John Cash", "Programming"],
            ["Brian Hook", "Programming"],
            ["Graeme Devine", "Game Design"],
            ["Adrian Carmack", "Art Direction"],
            ["Kevin Cloud", "Art"],
            ["Paul Jaquays", "Level Design"],
            ["Tim Willits", "Level Design"],
            ["Christian Antkow", "Level Design"],
            ["Paul Steed", "Modelling"],
            ["Todd Hollenshead", "Producer"],
          ].map(([name, role]) => (
            <div key={name} style={styles.credit}>
              {name} <span style={styles.role}>— {role}</span>
            </div>
          ))}
        </div>

        {/* Quake Modding Community */}
        <div style={styles.section}>
          <div style={styles.sectionTitle}>Quake Modding Community</div>
          {[
            ["ec- (Eugene)", "Quake3e engine maintainer"],
            ["ioQuake3 Team", null],
            ["OpenArena Team", null],
            ["OSP Team", null],
            ["CPMA Team", null],
            ["flexible-hud", "Widescreen HUD, FOV"],
            ["Spearmint", "atmospheric, tracemap"],
            ["UIE12", "Enhanced UI/menu system"],
            ["Unlagged", null],
            ["JUHOX Bright Arena", null],
          ].map(([name, desc]) => (
            <div key={name} style={styles.credit}>
              {name}
              {desc && <span style={styles.role}>— {desc}</span>}
            </div>
          ))}
        </div>

        {/* Special Thanks */}
        <div style={styles.section}>
          <div style={styles.sectionTitle}>Special Thanks</div>
          <div style={styles.credit}>The Quake community</div>
          <div style={styles.quote}>Quake 3 Arena — 1999–forever</div>
        </div>
      </div>

      <div style={styles.backArea}>
        <Button variant="secondary" onClick={onBack}>
          Back
        </Button>
      </div>
    </div>
  );
}
