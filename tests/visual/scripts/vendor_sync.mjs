#!/usr/bin/env node
// Populates tests/visual/vendor/ so the artboard harnesses render offline.
//
// WHY THIS EXISTS
// The harnesses used to load their JSX from ../../../docs/wui-claude-design-
// canonical-v2/project/, a directory that was never committed to this repo
// (`git log --all --diff-filter=A` over that path is empty). They also pulled
// React/Babel from unpkg and fonts from Google Fonts, so the gate could not run
// offline or in CI. Result: `#root` stayed empty and every render was a single
// flat colour.
//
// The design JSX lives in a SEPARATE PRIVATE repo (eser/wiki) while q3now is
// PUBLIC, so the JSX is deliberately NOT committed here. This script copies it
// into tests/visual/vendor/, which is gitignored. Everything under vendor/ is a
// reproducible local build artifact, never a commit.
//
//   node tests/visual/scripts/vendor_sync.mjs [--design-src <dir>]
//
// Source directory resolution order:
//   1. --design-src <dir>
//   2. $QW_DESIGN_SRC
//   3. the default sibling-wiki path below
//
// JSX is precompiled to plain JS here, at sync time, using the @babel/core
// already vendored under launcher/frontend/node_modules. That removes the
// @babel/standalone CDN script from the harness entirely: the browser then
// loads ordinary <script> files, which also sidesteps the file:// CORS failure
// that `<script type="text/babel" src=...>` hits (it fetches over XHR).

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { createRequire } from 'node:module';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const VISUAL = path.resolve(HERE, '..');          // tests/visual
const REPO = path.resolve(VISUAL, '../..');       // repo root
const VENDOR = path.join(VISUAL, 'vendor');

const DEFAULT_DESIGN_SRC =
  '/Users/eser/projects/eser/wiki/raw/articles/quakewired-handoff-v2/quakewired/project';

const argv = process.argv.slice(2);
const flagIdx = argv.indexOf('--design-src');
const designSrc =
  (flagIdx >= 0 ? argv[flagIdx + 1] : null) ||
  process.env.QW_DESIGN_SRC ||
  DEFAULT_DESIGN_SRC;

const JSX_FILES = ['qw-shared.jsx', 'qw-screens.jsx', 'qw-variants.jsx'];

// Repo-local TTFs, so the harness never needs Google Fonts. The JSX only ever
// names Oxanium and JetBrains Mono; Share Tech Mono is included because the
// wider design system references it and the TTF is already here.
const FONTS = [
  ['assets/fonts/Oxanium-Light.ttf', 'Oxanium-Light.ttf'],
  ['assets/fonts/Oxanium-Regular.ttf', 'Oxanium-Regular.ttf'],
  ['assets/fonts/Oxanium-Medium.ttf', 'Oxanium-Medium.ttf'],
  ['assets/fonts/Oxanium-SemiBold.ttf', 'Oxanium-SemiBold.ttf'],
  ['assets/fonts/Oxanium-Bold.ttf', 'Oxanium-Bold.ttf'],
  ['assets/fonts/Oxanium-ExtraBold.ttf', 'Oxanium-ExtraBold.ttf'],
  ['assets/fonts/ShareTechMono-Regular.ttf', 'ShareTechMono-Regular.ttf'],
  ['tools/msdf/fonts/JetBrainsMono-Regular.ttf', 'JetBrainsMono-Regular.ttf'],
];

const LIBS = [
  ['launcher/frontend/node_modules/react/umd/react.development.js', 'react.development.js'],
  ['launcher/frontend/node_modules/react-dom/umd/react-dom.development.js', 'react-dom.development.js'],
];

function ensureDir(d) { fs.mkdirSync(d, { recursive: true }); }
function copy(from, to, label) {
  if (!fs.existsSync(from)) { console.error(`  MISSING ${label}: ${from}`); return false; }
  ensureDir(path.dirname(to));
  fs.copyFileSync(from, to);
  console.error(`  ok ${label}: ${path.relative(VISUAL, to)} (${fs.statSync(to).size} bytes)`);
  return true;
}

// ---- JSX -> React.createElement -------------------------------------------
// Uses @babel/core + @babel/types from launcher/frontend/node_modules. That
// tree has the JSX *parser* but no JSX *transform* plugin (no @babel/preset-
// react, no @babel/standalone), so the ~50-line visitor below supplies it
// rather than pulling a new package into the repo.
function makeJsxPlugin(t) {
  const elementName = (n) => {
    if (t.isJSXIdentifier(n)) {
      // Lowercase => intrinsic tag (string); capitalised => component binding.
      return /^[a-z]/.test(n.name) ? t.stringLiteral(n.name) : t.identifier(n.name);
    }
    if (t.isJSXMemberExpression(n)) {
      return t.memberExpression(elementName(n.object), t.identifier(n.property.name));
    }
    if (t.isJSXNamespacedName(n)) {
      return t.stringLiteral(`${n.namespace.name}:${n.name.name}`);
    }
    throw new Error('unhandled JSX name node: ' + n.type);
  };

  const childNodes = (children) => {
    const out = [];
    for (const c of children) {
      if (t.isJSXText(c)) {
        // Match Babel's JSX whitespace handling: drop lines that are pure
        // indentation, collapse the rest.
        const lines = c.value.split('\n');
        let text = '';
        for (let i = 0; i < lines.length; i++) {
          let line = lines[i];
          const firstNonSpace = line.match(/[^ \t]/);
          if (i !== 0) line = line.replace(/^[ \t]*/, '');
          if (i !== lines.length - 1) line = line.replace(/[ \t]*$/, '');
          if (!line) continue;
          if (firstNonSpace || i === 0) {
            if (text) text += ' ';
            text += line;
          }
        }
        if (text) out.push(t.stringLiteral(text));
      } else if (t.isJSXExpressionContainer(c)) {
        if (!t.isJSXEmptyExpression(c.expression)) out.push(c.expression);
      } else if (t.isJSXSpreadChild(c)) {
        out.push(t.spreadElement(c.expression));
      } else {
        out.push(c);
      }
    }
    return out;
  };

  const propsObject = (attrs) => {
    if (!attrs.length) return t.nullLiteral();
    const props = [];
    for (const a of attrs) {
      if (t.isJSXSpreadAttribute(a)) { props.push(t.spreadElement(a.argument)); continue; }
      let v = a.value;
      if (v == null) v = t.booleanLiteral(true);                 // <input disabled />
      else if (t.isJSXExpressionContainer(v)) v = v.expression;  // attr={expr}
      const raw = t.isJSXNamespacedName(a.name)
        ? `${a.name.namespace.name}:${a.name.name.name}`
        : a.name.name;
      // data-*/aria-*/namespaced names are not valid identifiers -> quote them.
      const key = /^[A-Za-z_$][A-Za-z0-9_$]*$/.test(raw)
        ? t.identifier(raw)
        : t.stringLiteral(raw);
      props.push(t.objectProperty(key, v));
    }
    return t.objectExpression(props);
  };

  const createElement = (name, props, children) =>
    t.callExpression(
      t.memberExpression(t.identifier('React'), t.identifier('createElement')),
      [name, props, ...children]
    );

  return {
    visitor: {
      // `exit` so inner JSX is already rewritten when the parent is built.
      JSXElement: {
        exit(p) {
          const o = p.node.openingElement;
          p.replaceWith(createElement(
            elementName(o.name), propsObject(o.attributes), childNodes(p.node.children)
          ));
        },
      },
      JSXFragment: {
        exit(p) {
          p.replaceWith(createElement(
            t.memberExpression(t.identifier('React'), t.identifier('Fragment')),
            t.nullLiteral(), childNodes(p.node.children)
          ));
        },
      },
    },
  };
}

async function main() {
  console.error(`vendor_sync: repo=${REPO}`);
  console.error(`vendor_sync: design-src=${designSrc}`);
  ensureDir(path.join(VENDOR, 'lib'));
  ensureDir(path.join(VENDOR, 'jsx'));
  ensureDir(path.join(VENDOR, 'fonts'));

  let failed = 0;

  console.error('React UMD (from launcher/frontend/node_modules):');
  for (const [rel, name] of LIBS) {
    if (!copy(path.join(REPO, rel), path.join(VENDOR, 'lib', name), name)) failed++;
  }

  console.error('Fonts (repo-local TTF, replaces Google Fonts):');
  for (const [rel, name] of FONTS) {
    if (!copy(path.join(REPO, rel), path.join(VENDOR, 'fonts', name), name)) failed++;
  }

  console.error('Design JSX -> precompiled JS:');
  if (!fs.existsSync(designSrc)) {
    console.error(`  FATAL: design source not found: ${designSrc}`);
    console.error('  Pass --design-src <dir> or set QW_DESIGN_SRC.');
    process.exit(2);
  }

  const require_ = createRequire(pathToFileURL(path.join(REPO, 'launcher/frontend/package.json')));
  let babel, types;
  try {
    babel = require_('@babel/core');
    types = require_('@babel/types');
  } catch (e) {
    console.error('  FATAL: @babel/core not available under launcher/frontend/node_modules');
    console.error('  ' + e.message);
    process.exit(3);
  }
  const plugin = makeJsxPlugin(types);

  for (const f of JSX_FILES) {
    const from = path.join(designSrc, f);
    if (!fs.existsSync(from)) { console.error(`  MISSING jsx: ${from}`); failed++; continue; }
    const src = fs.readFileSync(from, 'utf8');
    let out;
    try {
      out = babel.transformSync(src, {
        parserOpts: { plugins: ['jsx'] },
        plugins: [plugin],
        configFile: false, babelrc: false, compact: false,
      });
    } catch (e) {
      console.error(`  FATAL transform ${f}: ${e.message}`);
      process.exit(4);
    }
    const to = path.join(VENDOR, 'jsx', f.replace(/\.jsx$/, '.js'));
    fs.writeFileSync(to, `// GENERATED by vendor_sync.mjs from ${f} — do not edit.\n` + out.code + '\n');
    console.error(`  ok ${f} -> ${path.relative(VISUAL, to)} (${fs.statSync(to).size} bytes)`);
  }

  if (failed) {
    console.error(`vendor_sync: ${failed} asset(s) missing`);
    process.exit(1);
  }
  console.error('vendor_sync: complete');
}

main();
