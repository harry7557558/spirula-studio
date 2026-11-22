import assert from 'node:assert/strict';
import { mkdtemp, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { resolve } from 'node:path';
import { fixture } from './fixtures.js';

export async function checkImportControls(page, origin, evidence) {
  const directory = await mkdtemp(resolve(tmpdir(), 'spirula-picker-'));
  const original = resolve(directory, 'original.ply');
  const ply = new Uint8Array(await fixture('original.ply', true).arrayBuffer());
  await writeFile(original, ply);
  const states = [];
  async function state(label) {
    const value = await page.evaluate(() => {
      const v = window.__viewer;
      return { name: document.getElementById('st-file').textContent, model: v.model,
        status: document.getElementById('status-text').textContent,
        changes: window.pickerChanges, primitive: v.opts.primitive, shDegree: v.opts.shDegree };
    });
    states.push({ label, ...value });
    return value;
  }
  async function pick(keyboard = false) {
    const button = page.getByRole('button', { name: 'Browse files', exact: true });
    await button.waitFor({ state: 'visible' });
    const [chooser] = await Promise.all([
      page.waitForEvent('filechooser'),
      keyboard ? button.press('Enter') : button.click(),
    ]);
    await chooser.setFiles(original);
  }
  async function drop(bytes, name) {
    const data = await page.evaluateHandle(({ bytes, name }) => {
      const transfer = new DataTransfer();
      transfer.items.add(new File([new Uint8Array(bytes)], name));
      return transfer;
    }, { bytes: Array.from(bytes), name });
    try { await page.locator('#viewport').dispatchEvent('drop', { dataTransfer: data }); }
    finally { await data.dispose(); }
    await page.waitForFunction(() => document.getElementById('status-text').textContent.startsWith('Load error:'));
    assert.equal((await state(`refused ${name}`)).model, null);
    assert.equal(await page.locator('#st-file').textContent(), '—');
    assert.equal(await page.locator('#sec-splat').isVisible(), false);
    assert.equal(await page.getByRole('button', { name: 'Browse files', exact: true }).isVisible(), true);
  }
  async function loaded(label) {
    await page.waitForFunction(() => document.getElementById('status-text').textContent === 'Splat model loaded');
    const value = await state(label);
    assert.equal(value.name, 'original.ply');
    assert.equal(value.model.count, 512);
    assert.equal(value.model.shDegree, 3);
    assert.equal(value.model.mip, true);
    assert.equal(value.primitive, 1);
    assert.equal(await page.locator('#sec-splat').isVisible(), true);
    assert.equal(await page.locator('#st-splat-stats').isVisible(), true);
    assert.equal(await page.locator('#shdeg').getAttribute('max'), '3');
    assert.equal(await page.locator('#toast').isVisible(), false);
    assert.equal(await page.getByRole('button', { name: 'Browse files', exact: true }).isVisible(), false);
    assert.ok(await page.evaluate(() => window.__viewer.snapshot().nonBlack > 0));
    return value;
  }
  try {
    await page.goto(`${origin}/viewer/`);
    await page.waitForFunction(() => document.getElementById('status-text').textContent === 'Ready — drop a model');
    await page.evaluate(() => {
      window.pickerChanges = 0;
      document.getElementById('file-input').addEventListener('change', () => window.pickerChanges++);
    });
    await pick();
    const initial = await loaded('initial pointer import');
    await drop(ply.slice(0, 2300), 'truncated.ply');
    await page.screenshot({ path: resolve(evidence, 'import_controls_refused.png') });
    await pick(true);
    assert.equal((await state('same-file keyboard selection')).changes, initial.changes + 1,
      'The restored picker must import the same file after a failed drop');
    const recovered = await loaded('keyboard recovery');
    await drop(new TextEncoder().encode('synthetic unsupported input'), 'unsupported.spz');
    await pick();
    assert.equal((await state('same-file pointer selection')).changes, recovered.changes + 1);
    await loaded('pointer recovery');
    await page.locator('#primitive').selectOption('0');
    assert.equal((await state('restored primitive control')).primitive, 0);
    await page.locator('#shdeg').press('Home');
    await page.locator('#shdeg').press('ArrowRight');
    assert.equal((await state('restored SH control')).shDegree, 1);
    await page.locator('#shdeg').press('End');
    await page.locator('#primitive').selectOption('auto');
    await page.getByRole('button', { name: 'Reset Camera', exact: true }).click();
    await loaded('restored controls and identity');
    return 'PASS: pointer and keyboard browsing, same-file recovery, imported identity and restored controls';
  } finally {
    await writeFile(resolve(evidence, 'import_controls.json'), JSON.stringify({ states }, null, 2) + '\n');
  }
}
