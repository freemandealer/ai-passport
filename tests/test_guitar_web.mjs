// Browser acceptance of the shipped HTML. The test server uses the actual C
// parser; ESP HTTP/NVS integration is covered separately by test_guitar_service.
import assert from 'node:assert/strict';
import { readFileSync, mkdirSync } from 'node:fs';
import { createServer } from 'node:http';
import { spawnSync } from 'node:child_process';
import { chromium } from '../.tools/node_modules/playwright/index.mjs';

const html = readFileSync('main/guitar_editor.html');
let stored = '---\n绿光练习\nC\n80\nC C Am Am F F G G\n';
let failSave = false, saves = 0, failLoad = false;
let activities = 0;
const server = createServer(async (req, res) => {
    if (req.url === '/') { res.setHeader('Content-Type', 'text/html; charset=utf-8'); res.end(html); return; }
    res.setHeader('Content-Type', 'text/plain; charset=utf-8');
    if (req.method === 'POST' && req.url === '/api/activity') {
        assert.equal(req.headers['x-guitar-edit'], '1');
        ++activities; res.end('OK'); return;
    }
    if (req.method === 'GET' && req.url === '/api/score') {
        if (failLoad) { res.statusCode = 503; res.end('not ready'); } else res.end(stored);
        return;
    }
    if (req.method === 'POST' && ['/api/score', '/api/validate'].includes(req.url)) {
        assert.equal(req.headers['x-guitar-edit'], '1');
        const chunks = [];
        for await (const chunk of req) chunks.push(chunk);
        const body = Buffer.concat(chunks);
        const result = spawnSync('build/guitar-sim/guitar_score_cli', { input: body });
        assert.ifError(result.error);
        if (result.status !== 0) {
            res.statusCode = 422;
            res.end(`第 ${result.stdout.toString().trim().split(' ')[1]} 行：和弦或级数无效`);
        } else if (req.url === '/api/validate') res.end('校验通过');
        else if (failSave) { res.statusCode = 503; res.end('保存失败，原曲谱未替换'); }
        else { stored = body.toString(); saves++; res.end('已保存到设备'); }
        return;
    }
    res.statusCode = 404; res.end('not found');
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
let browser;
try {
    browser = await chromium.launch({ headless: true, executablePath: process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE });
    const page = await browser.newPage({ viewport: { width: 1280, height: 1050 } });
    const errors = [];
    page.on('pageerror', e => errors.push(e.message));
    const base = `http://127.0.0.1:${server.address().port}`;
    await page.goto(base);
    await page.waitForFunction(() => !document.getElementById('score').disabled);
    assert.equal(await page.locator('#score').inputValue(), stored);
    assert.ok(await page.locator('#retry').isHidden());
    assert.equal(activities, 0); /* Opening an idle page has no heartbeat. */
    assert.ok((await page.locator('#format').textContent()).includes('(注释内容)'));
    assert.ok((await page.locator('#format').textContent()).includes('长按下：调速'));
    mkdirSync('build/guitar-preview', { recursive: true });
    await page.screenshot({ path: 'build/guitar-preview/editor-desktop.png', fullPage: true });
    const old = stored;
    const wheelResponse = page.waitForResponse(r => r.url().endsWith('/api/activity') && r.status() === 200);
    await page.mouse.wheel(0, 10);
    await wheelResponse;
    const activityResponse = page.waitForResponse(r => r.url().endsWith('/api/activity') && r.status() === 200);
    await page.locator('#score').fill('---\n新歌\nAm\n95\n1(前奏) 4 (分解 轻弹) V7 1');
    await activityResponse;
    assert.ok(activities > 0);
    await page.locator('#validate').click();
    await page.waitForFunction(() => document.getElementById('status').textContent === '校验通过');
    assert.equal(stored, old); assert.equal(saves, 0);
    await page.locator('#save').click();
    await page.waitForFunction(() => document.getElementById('status').textContent === '已保存到设备');
    assert.equal(saves, 1); assert.ok(stored.includes('新歌'));
    assert.ok(stored.includes('1(前奏) 4 (分解 轻弹)'));
    await page.reload();
    await page.waitForFunction(() => !document.getElementById('score').disabled);
    assert.equal(await page.locator('#score').inputValue(), stored);
    await page.locator('#score').fill('---\nbad\nC\n80\nH');
    await page.locator('#save').click();
    await page.waitForFunction(() => document.getElementById('status').className === 'error');
    assert.ok((await page.locator('#status').textContent()).includes('第 5 行'));
    assert.equal(saves, 1); assert.ok((await page.locator('#score').inputValue()).endsWith('H'));
    await page.locator('#score').fill('---\n注释错误\nC\n80\nC(未闭合');
    await page.locator('#save').click();
    await page.waitForFunction(() => document.getElementById('status').className === 'error');
    assert.ok((await page.locator('#status').textContent()).includes('第 5 行'));
    assert.equal(saves, 1); assert.ok(stored.includes('1(前奏)'));
    await page.locator('#score').fill('中'.repeat(1400));
    assert.ok(await page.locator('#save').isDisabled());
    assert.ok((await page.locator('#size').textContent()).includes('4200'));
    const imported = '---\n导入歌曲\nF\n75\n1(前奏) 6 4(轻扫) 5';
    await page.locator('#file').setInputFiles({ name: 'test.txt', mimeType: 'text/plain', buffer: Buffer.from(imported) });
    await page.waitForFunction(value => document.getElementById('score').value === value, imported);
    failSave = true;
    await page.locator('#save').click();
    await page.waitForFunction(() => document.getElementById('status').className === 'error');
    assert.equal(await page.locator('#score').inputValue(), imported);
    assert.equal(saves, 1);
    failSave = false;
    await page.locator('#save').click();
    await page.waitForFunction(() => document.getElementById('status').textContent === '已保存到设备');
    assert.equal(stored, imported); assert.equal(saves, 2);
    const downloadPromise = page.waitForEvent('download');
    await page.locator('#download').click();
    const download = await downloadPromise;
    assert.equal(download.suggestedFilename(), 'guitar-scores.txt');
    assert.equal(readFileSync(await download.path(), 'utf8'), imported);
    await page.setViewportSize({ width: 390, height: 844 });
    await page.screenshot({ path: 'build/guitar-preview/editor-mobile.png', fullPage: true });
    assert.ok(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth));
    failLoad = true;
    await page.reload();
    await page.waitForFunction(() => !document.getElementById('retry').hidden);
    assert.ok(await page.locator('#save').isDisabled());
    failLoad = false;
    await page.locator('#retry').click();
    await page.waitForFunction(() => !document.getElementById('score').disabled);
    await page.waitForTimeout(3300); /* Let the final interaction notification drain. */
    const idleActivities = activities;
    await page.waitForTimeout(3300);
    assert.equal(activities, idleActivities); /* No recurring keep-awake traffic. */
    assert.deepEqual(errors, []);
    console.log('Browser editor: PASS (read/edit/validate/save/reload/import/export, comments, failures, UTF-8 byte limit, mobile layout)');
} finally {
    if (browser) await browser.close();
    await new Promise(resolve => server.close(resolve));
}
