#!/usr/bin/env node
/* Render docs/amu_fsm.drawio to docs/amu_fsm_preview.png with the real draw.io viewer.
 *
 * Usage:  node docs/amu_fsm_render.js [in.drawio] [out.png] [scale]
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

(async () => {
  const xml = fs.readFileSync(inp, 'utf8');
  const browser = await chromium.launch();
  const page = await browser.newPage({ deviceScaleFactor: scale, viewport: { width: 2400, height: 2200 } });
  await page.setContent('<html><body style="margin:0;background:#fff"><div id="g" class="mxgraph" ' +
    'style="width:2300px;height:2100px;position:absolute;left:0;top:0"></div></body></html>');
  await page.addScriptTag({ content: viewerJs() });
  const b = await page.evaluate((xml) => new Promise((resolve) => {
    const el = document.getElementById('g');
    el.setAttribute('data-mxgraph', JSON.stringify({ xml, nav: false, resize: false, toolbar: null, border: 0,
      'auto-fit': false, 'auto-origin': false, center: false }));
    window.GraphViewer.createViewerForElement(el, (v) => resolve(v.graph.getGraphBounds()));
  }), xml);
  await page.waitForTimeout(1000);
  await page.screenshot({ path: out, clip: { x: Math.max(0, b.x - PAD), y: Math.max(0, b.y - PAD),
    width: b.width + 2 * PAD, height: b.height + 2 * PAD } });
  await browser.close();
  console.log('wrote', out);
})();
