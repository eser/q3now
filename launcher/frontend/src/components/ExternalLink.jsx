import React from "react";
import { BrowserOpenURL } from "../../wailsjs/runtime/runtime";

export default function ExternalLink({ href, children, style }) {
  return (
    <span
      style={{ cursor: "pointer", ...style }}
      onClick={(e) => {
        e.preventDefault();
        BrowserOpenURL(href);
      }}
    >
      {children}
    </span>
  );
}
