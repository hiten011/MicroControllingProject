#!/usr/bin/env node
/* Render docs/amu_fsm.drawio to docs/amu_fsm_preview.png (or a .svg) with the real draw.io viewer.
 *
 * Usage:  node docs/amu_fsm_render.js [in.drawio] [out.png|out.svg] [scale]
 *         (defaults: docs/amu_fsm.drawio, docs/amu_fsm_preview.png, scale 2 = 200%)
 *
 * Needs the `playwright` npm package with a Chromium, and network access once to download
 * viewer-static.min.js from viewer.diagrams.net (cached in the OS temp directory, or point
 * AMU_VIEWER_JS at a local copy). The PNG has a 20 px border, like draw.io's own export.
 */
const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');
const { chromium } = require('playwright');

const here = __dirname;
const inp = process.argv[2] || path.join(here, 'amu_fsm.drawio');
const out = process.argv[3] || path.join(here, 'amu_fsm_preview.png');
const scale = parseFloat(process.argv[4] || '2');
const PAD = 20;

function viewerJs() {
  const file = process.env.AMU_VIEWER_JS || path.join(os.tmpdir(), 'drawio-viewer-static.min.js');
  if (!fs.existsSync(file) || fs.statSync(file).size < 1e6) {
    execFileSync('curl', ['-fsS', '-o', file, 'https://viewer.diagrams.net/js/viewer-static.min.js']);
  }
  return fs.readFileSync(file, 'utf8');
}


/* Runs in the page. draw.io's own SVG export puts every label in a <foreignObject> (HTML), which Word,
 * PowerPoint, Inkscape, LaTeX and most converters cannot show, and adds dark-mode light-dark() styles.
 * This turns each label into plain <text>/<tspan> (positions measured from the browser's own layout, so
 * bold / font sizes / line breaks match the PNG) and drops the light-dark() styles: a self-contained,
 * light-theme SVG that imports anywhere and never pixelates. */
function exportSvg(pad) {
  const graph = window.__amuViewer.graph;
  const svg = graph.getSvg('#ffffff', 1, pad, true, null, true, null, null, null, null);
  const NS = 'http://www.w3.org/2000/svg';
  const host = document.createElement('div');
  host.style.cssText = 'position:absolute;left:0;top:0;visibility:hidden';
  host.appendChild(svg);
  document.body.appendChild(host);
  const ctx = document.createElement('canvas').getContext('2d');

  for (const sw of Array.from(svg.querySelectorAll('switch'))) {
    const fo = sw.querySelector('foreignObject');
    const fb = sw.querySelector('text');
    if (!fo || !fb) continue;
    const anchor = fb.getAttribute('text-anchor') || 'start';
    const frags = [];                       // one entry per visual line fragment
    const walker = document.createTreeWalker(fo, NodeFilter.SHOW_TEXT);
    for (let n = walker.nextNode(); n; n = walker.nextNode()) {
      if (!n.nodeValue.trim()) continue;
      const cs = getComputedStyle(n.parentElement);
      ctx.font = cs.fontStyle + ' ' + cs.fontWeight + ' ' + cs.fontSize + ' ' + cs.fontFamily;
      const asc = ctx.measureText('Hg').fontBoundingBoxAscent;
      let cur = null;
      const r = document.createRange();
      for (let i = 0; i < n.nodeValue.length; i++) {
        r.setStart(n, i); r.setEnd(n, i + 1);
        const q = r.getClientRects()[0];
        if (!q || (!q.width && n.nodeValue[i] === ' ')) continue;
        const base = q.top + asc;
        if (!cur || Math.abs(cur.base - base) > 2) {
          cur = { base, left: q.left, right: q.right, text: '', cs };
          frags.push(cur);
        }
        cur.text += n.nodeValue[i];
        cur.right = q.right;
      }
    }
    const g = document.createElementNS(NS, 'g');
    const lines = [];                       // group fragments that share a baseline
    for (const f of frags.sort((a, b) => a.base - b.base || a.left - b.left)) {
      const l = lines.find((x) => Math.abs(x.base - f.base) <= 2);
      if (l) l.parts.push(f); else lines.push({ base: f.base, parts: [f] });
    }
    const o = svg.getBoundingClientRect();
    const mk = (x, y, anc, p, text) => {
      const t = document.createElementNS(NS, 'text');
      t.setAttribute('x', x.toFixed(2));
      t.setAttribute('y', y.toFixed(2));
      t.setAttribute('text-anchor', anc);
      t.setAttribute('xml:space', 'preserve');
      t.setAttribute('font-family', p.cs.fontFamily.replace(/"/g, ''));
      t.setAttribute('font-size', p.cs.fontSize.replace('px', ''));
      t.setAttribute('fill', p.cs.color);
      if (parseInt(p.cs.fontWeight, 10) >= 600) t.setAttribute('font-weight', 'bold');
      if (p.cs.fontStyle === 'italic') t.setAttribute('font-style', 'italic');
      t.textContent = text;
      g.appendChild(t);
    };
    for (const l of lines) {
      const y = l.base - o.top;
      if (l.parts.length === 1) {           // plain line: keep the label's own alignment
        const p = l.parts[0];
        const x = (anchor === 'middle' ? (p.left + p.right) / 2 : anchor === 'end' ? p.right : p.left) - o.left;
        mk(x, y, anchor, p, p.text.trim());
      } else {                              // mixed bold / mono / italic runs: one <text> per run, left-anchored
        l.parts.forEach((p, i) => {
          mk(p.left - o.left, y, 'start', p, p.text.replace(/\s+$/, ''));
        });
      }
    }
    sw.replaceWith(g);
  }

  svg.querySelectorAll('a').forEach((e) => { if (/Text is not SVG/.test(e.textContent)) e.remove(); });
  svg.querySelectorAll('style').forEach((e) => e.remove());
  for (const e of Array.from(svg.querySelectorAll('[style]')).concat([svg])) {
    const keep = (e.getAttribute('style') || '').split(';').filter((d) => d.trim() && !/light-dark|var\(/.test(d));
    keep.length ? e.setAttribute('style', keep.join(';')) : e.removeAttribute('style');
  }
  svg.removeAttribute('id');
  svg.querySelectorAll('[data-cell-id]').forEach((e) => e.removeAttribute('data-cell-id'));
  host.remove();
  return '<?xml version="1.0" encoding="UTF-8"?>\n' + new XMLSerializer().serializeToString(svg) + '\n';
}

(async () => {
  const xml = fs.readFileSync(inp, 'utf8');
  const browser = await chromium.launch();
  const page = await browser.newPage({ deviceScaleFactor: scale, viewport: { width: 3400, height: 3400 } });
  await page.setContent('<html><body style="margin:0;background:#fff"><div id="g" class="mxgraph" ' +
    'style="width:3300px;height:3300px;position:absolute;left:0;top:0"></div></body></html>');
  await page.addScriptTag({ content: viewerJs() });
  const b = await page.evaluate((xml) => new Promise((resolve) => {
    const el = document.getElementById('g');
    el.setAttribute('data-mxgraph', JSON.stringify({ xml, nav: false, resize: false, toolbar: null, border: 0,
      'auto-fit': false, 'auto-origin': false, center: false }));
    window.GraphViewer.createViewerForElement(el, (v) => { window.__amuViewer = v; resolve(v.graph.getGraphBounds()); });
  }), xml);
  await page.waitForTimeout(1000);
  if (out.toLowerCase().endsWith('.svg')) {
    fs.writeFileSync(out, await page.evaluate(exportSvg, PAD));
  } else {
    await page.screenshot({ path: out, clip: { x: Math.max(0, b.x - PAD), y: Math.max(0, b.y - PAD),
      width: b.width + 2 * PAD, height: b.height + 2 * PAD } });
  }
  await browser.close();
  console.log('wrote', out);
})();
