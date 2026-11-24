import assert from 'node:assert/strict';
import { writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';

export async function checkCameraCenters(page, origin, evidence) {
  const modes = ['2', '3', '5'], states = [];
  const cloud = 'ply\nformat ascii 1.0\nelement vertex 512\nproperty float x\nproperty float y\nproperty float z\nproperty uchar red\nproperty uchar green\nproperty uchar blue\nend_header\n'
    + Array.from({ length: 512 }, (_, i) => `${i % 8} ${Math.floor(i / 8) % 8} ${Math.floor(i / 64)} 128 128 128\n`).join('');
  const cameras = JSON.stringify({ camera_model: 'OPENCV', fl_x: 400, fl_y: 400,
    cx: 320, cy: 240, w: 640, h: 480, ply_file_path: 'seed-cloud.ply',
    frames: [-1, 1].map((x, i) => ({ file_path: `synthetic-pose-${i}.png`,
      transform_matrix: [[1,0,0,x],[0,1,0,0],[0,0,1,2],[0,0,0,1]] })) });
  async function drop(files) {
    const data = await page.evaluateHandle(files => {
      const transfer = new DataTransfer();
      for (const { name, content } of files) transfer.items.add(new File([content], name));
      return transfer;
    }, files);
    try { await page.locator('#viewport').dispatchEvent('drop', { dataTransfer: data }); }
    finally { await data.dispose(); }
    await page.waitForFunction(() => !/Loading|Scanning/.test(document.getElementById('status-text').textContent));
    assert.equal(await page.locator('#status-text').textContent(), 'Dataset loaded');
  }
  async function state(label) {
    const value = await page.evaluate(() => ({ model: window.__viewer.model,
      center: window.__viewer.opts.centerMode, selected: document.getElementById('center-mode').value,
      options: Array.from(document.getElementById('center-mode').options,
        option => ({ value: option.value, disabled: option.disabled, label: option.textContent })) }));
    states.push({ label, ...value });
    return value;
  }
  function availability(value, present) {
    assert.equal(value.model.type, 'dataset');
    for (const mode of modes) {
      assert.equal(value.options.find(option => option.value === mode).disabled, !present,
        'Camera centering must be available only when camera poses are present');
    }
  }
  try {
    await page.goto(`${origin}/viewer/`);
    await page.waitForFunction(() => document.getElementById('status-text').textContent === 'Ready — drop a model');
    await drop([{ name: 'seed-cloud.ply', content: cloud }]);
    const first = await state('point cloud without cameras');
    assert.equal(first.model.numCameras, 0);
    assert.equal(first.model.numPoints, 512);
    availability(first, false);
    assert.equal(first.selected, '1');
    await page.locator('#center-mode').selectOption('4');
    assert.equal((await state('point mean remains available')).center, 4);
    await drop([{ name: 'transforms.json', content: cameras }, { name: 'seed-cloud.ply', content: cloud }]);
    const withCameras = await state('two synthetic camera poses');
    assert.equal(withCameras.model.numCameras, 2);
    assert.equal(withCameras.model.numPoints, 512);
    availability(withCameras, true);
    await page.locator('#center-mode').selectOption('2');
    assert.equal((await state('camera median selected')).center, 2);
    await drop([{ name: 'seed-cloud.ply', content: cloud }]);
    const back = await state('camera modes withdrawn on point-only replacement');
    availability(back, false);
    assert.equal(back.selected, '1');
    assert.equal(back.model.numCameras, 0);
    assert.equal(back.model.numPoints, 512);
    const gl = await page.evaluate(() => {
      window.__viewer.snapshot();
      return document.getElementById('glcanvas').getContext('webgl2').getError();
    });
    assert.equal(gl, 0);
    return 'PASS: point-only centering, camera-bearing options and capability withdrawal on replacement';
  } finally {
    await writeFile(resolve(evidence, 'camera_centers.json'), JSON.stringify({ states }, null, 2) + '\n');
  }
}
