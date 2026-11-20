import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { readFile, mkdir, mkdtemp, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { dirname, extname, resolve, sep } from 'node:path';
import { fileURLToPath } from 'node:url';
import { chromium } from 'playwright';
import { checkImportControls } from './import_controls.mjs';
import { checkCameraCenters } from './camera_centers.mjs';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const evidence = process.env.SS_VIEWER_EVIDENCE || await mkdtemp(resolve(tmpdir(), 'spirula-viewer-test-'));
await mkdir(evidence, { recursive: true });
const mime = { '.html':'text/html', '.js':'application/javascript', '.wasm':'application/wasm', '.css':'text/css', '.png':'image/png' };
const server = createServer(async (req, res) => {
  const route = decodeURIComponent(new URL(req.url, 'http://localhost').pathname);
  const path = resolve(root, '.' + route);
  if ((!route.startsWith('/viewer/') && route !== '/assets/icon_128.png') || !path.startsWith(root + sep)) {
    res.writeHead(404).end();
    return;
  }
  try {
    const body = await readFile(path.endsWith(sep) || route.endsWith('/') ? resolve(path, 'index.html') : path);
    res.writeHead(200, {'Content-Type':mime[extname(route)] || 'text/html', 'Cache-Control':'no-store'}).end(body);
  } catch {
    res.writeHead(404).end();
  }
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const origin = `http://127.0.0.1:${server.address().port}`;
const results = [], external = [];
let browser;
try {
  browser = await chromium.launch(process.env.SS_VIEWER_CHROME === '1'
    ? {channel:'chrome', headless:true}
    : {args:['--use-angle=swiftshader', '--enable-unsafe-swiftshader']});
  for (const name of ['shader_smoke', 'import_recovery', 'import_controls', 'camera_centers']) {
    const page = await browser.newPage({viewport:{width:1280,height:850}});
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.route('**/*', async route => {
      const url = route.request().url();
      if (/^https:\/\/harry7557558\.github\.io\/spirula-studio\/assets\/icon(?:_128\.png|\.ico)$/.test(url)) {
        await route.fulfill({path:resolve(root,'assets/icon_128.png'), contentType:'image/png'});
      } else if (url.startsWith(origin + '/')) {
        await route.continue();
      } else {
        external.push(url);
        await route.abort();
      }
    });
    let text;
    try {
      if (name === 'import_controls') text = await checkImportControls(page, origin, evidence);
      else if (name === 'camera_centers') text = await checkCameraCenters(page, origin, evidence);
      else {
        await page.goto(`${origin}/viewer/tests/${name}.html`);
        await page.waitForFunction(() => /^(PASS|FAIL|ERROR)/.test(document.getElementById('result').textContent));
        text = await page.locator('#result').textContent();
      }
    } catch (error) { text = 'FAIL: ' + error.message; }
    await page.screenshot({path:resolve(evidence,`${name}.png`),fullPage:true});
    results.push({test:name,result:text,errors});
    assert.match(text,/^PASS/);
    assert.deepEqual(errors,[]);
    await page.close();
  }
  assert.deepEqual(external,[]);
  console.log(JSON.stringify({browser:browser.version(),results,external,evidence}));
} finally {
  await writeFile(resolve(evidence,'result.json'),JSON.stringify({results,external},null,2)+'\n');
  await browser?.close();
  await new Promise(resolve => server.close(resolve));
}
