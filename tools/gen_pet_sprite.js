#!/usr/bin/env node
/**
 * 生成 Petdex 像素宠物精灵图 sprite.png
 * 规格 (对应 pet_anim.c / pet_ui.c):
 *   帧大小   96x104 (24x26 格, 每格 4px)
 *   行数     10 (Row 0..9 对应 PET_ANIM_* 状态)
 *   列数     12 (最多帧数, Deadloop 12 帧)
 *   输出     1152 x 1040 RGBA PNG
 */
'use strict';
const zlib = require('zlib');
const fs = require('fs');
const path = require('path');

const GW = 24, GH = 26, SCALE = 4;
const FW = GW * SCALE, FH = GH * SCALE;   // 96 x 104
const COLS = 12, ROWS = 10;

const PAL = {
  o: [0xFF, 0x8C, 0x00, 255],
  l: [0xFF, 0xA8, 0x40, 255],
  w: [0xFF, 0xFF, 0xFF, 255],
  e: [0x08, 0x08, 0x10, 255],
  k: [0x1A, 0x1A, 0x22, 255],
  c: [0xFF, 0xE0, 0xB0, 255],
  y: [0xFF, 0xE6, 0x6D, 255],
  r: [0xFF, 0x30, 0x30, 255],
  b: [0x64, 0xB4, 0xFF, 255],
  n: [0, 0, 0, 0],
};

function newGrid() { return Array.from({ length: GH }, () => Array(GW).fill('n')); }

function inRoundRect(x, y, x0, y0, x1, y1, rad) {
  if (x < x0 || x > x1 || y < y0 || y > y1) return false;
  const cx = x < x0 + rad ? x0 + rad : (x > x1 - rad ? x1 - rad : x);
  const cy = y < y0 + rad ? y0 + rad : (y > y1 - rad ? y1 - rad : y);
  const dx = x - cx, dy = y - cy;
  return dx * dx + dy * dy <= rad * rad;
}

function roundRect(g, x0, y0, x1, y1, rad, fill, outline) {
  for (let y = Math.max(0, y0); y <= Math.min(GH - 1, y1); y++)
    for (let x = Math.max(0, x0); x <= Math.min(GW - 1, x1); x++) {
      const inOuter = inRoundRect(x, y, x0, y0, x1, y1, rad);
      const inInner = inRoundRect(x, y, x0 + 1, y0 + 1, x1 - 1, y1 - 1, rad - 1);
      if (inOuter) g[y][x] = inInner ? fill : outline;
    }
}

function fillRect(g, x, y, w, h, c) {
  for (let yy = Math.max(0, y); yy < Math.min(GH, y + h); yy++)
    for (let xx = Math.max(0, x); xx < Math.min(GW, x + w); xx++)
      g[yy][xx] = c;
}

function fillEllipse(g, cx, cy, rx, ry, c) {
  for (let y = 0; y < GH; y++)
    for (let x = 0; x < GW; x++)
      if (((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1) g[y][x] = c;
}

function set(g, x, y, c) { if (x >= 0 && x < GW && y >= 0 && y < GH) g[y][x] = c; }

function mirrorGrid(g) {
  const out = newGrid();
  for (let y = 0; y < GH; y++)
    for (let x = 0; x < GW; x++) out[y][x] = g[y][GW - 1 - x];
  return out;
}

/**
 * 构建一帧宠物
 * @param {object} o  {bodyY, lean, squash, eye, mouth, armL, armR, foot, color, sweat, stars, sparks}
 */
function buildPet(o) {
  const g = newGrid();
  const bodyY = o.bodyY || 0, lean = o.lean || 0, sq = o.squash || 0;
  const bx0 = 3 + lean - sq, bx1 = 20 + lean + sq;
  const by0 = 4 + bodyY + sq, by1 = 21 + bodyY - sq;
  const bodyC = o.color === 'r' ? 'r' : 'o';
  const armL = o.armL || 'down', armR = o.armR || 'down';

  if (armL === 'down') fillRect(g, 1, 11 + bodyY, 2, 4, bodyC);
  if (armL === 'pumpUp') fillRect(g, 1, 8 + bodyY, 2, 4, bodyC);
  if (armL === 'wave') fillRect(g, 1, 6 + bodyY, 2, 3, bodyC);
  if (armL === 'chin') fillRect(g, 1, 12 + bodyY, 2, 4, bodyC);
  if (armR === 'down') fillRect(g, 21, 11 + bodyY, 2, 4, bodyC);
  if (armR === 'pumpUp') fillRect(g, 21, 8 + bodyY, 2, 4, bodyC);
  if (armR === 'wave') fillRect(g, 21, 6 + bodyY, 2, 3, bodyC);
  if (armR === 'chin') fillRect(g, 20, 9 + bodyY, 2, 3, bodyC);

  roundRect(g, bx0, by0, bx1, by1, 6, bodyC, 'k');
  fillRect(g, bx0 + 2, by0 + 1, 4, 1, 'l');
  fillRect(g, bx0 + 1, by0 + 2, 2, 2, 'l');
  fillEllipse(g, 12 + lean, 17 + bodyY, 5, 4, 'c');

  for (const ax of [7, 16]) {
    set(g, ax + lean, by0 - 1, bodyC);
    set(g, ax + lean, by0 - 2, bodyC);
    fillEllipse(g, ax + lean, by0 - 3, 1, 1, 'y');
  }

  const ey = 11 + bodyY, lx = 9 + lean, rx = 15 + lean;
  const eye = o.eye || 'open';
  if (eye === 'open') {
    fillEllipse(g, lx, ey, 2, 2, 'w'); fillEllipse(g, lx, ey + 1, 1, 1, 'e');
    fillEllipse(g, rx, ey, 2, 2, 'w'); fillEllipse(g, rx, ey + 1, 1, 1, 'e');
  } else if (eye === 'blink' || eye === 'closed' || eye === 'happy') {
    fillEllipse(g, lx, ey, 2, 1, 'e'); fillEllipse(g, rx, ey, 2, 1, 'e');
  } else if (eye === 'x') {
    fillEllipse(g, lx, ey, 2, 2, 'w'); fillEllipse(g, rx, ey, 2, 2, 'w');
    for (const cx of [lx, rx]) {
      set(g, cx - 1, ey - 1, 'e'); set(g, cx, ey - 1, 'e'); set(g, cx + 1, ey - 1, 'e');
      set(g, cx - 1, ey, 'e'); set(g, cx + 1, ey, 'e');
      set(g, cx - 1, ey + 1, 'e'); set(g, cx, ey + 1, 'e'); set(g, cx + 1, ey + 1, 'e');
    }
  } else if (eye === 'dizzy') {
    fillEllipse(g, lx, ey, 2, 2, 'w'); fillEllipse(g, rx, ey, 2, 2, 'w');
    for (const cx of [lx, rx]) {
      set(g, cx - 1, ey - 1, 'e'); set(g, cx, ey, 'e'); set(g, cx + 1, ey + 1, 'e');
      set(g, cx + 1, ey - 1, 'e'); set(g, cx - 1, ey + 1, 'e');
    }
  } else if (eye === 'angry') {
    fillEllipse(g, lx, ey, 2, 2, 'w'); fillEllipse(g, lx, ey + 1, 1, 1, 'e');
    fillEllipse(g, rx, ey, 2, 2, 'w'); fillEllipse(g, rx, ey + 1, 1, 1, 'e');
    fillRect(g, lx - 2, ey - 3, 3, 1, 'k'); set(g, lx - 2, ey - 2, 'k'); set(g, lx - 3, ey - 2, 'k');
    fillRect(g, rx, ey - 3, 3, 1, 'k'); set(g, rx + 2, ey - 2, 'k'); set(g, rx + 3, ey - 2, 'k');
  }

  const my = 15 + bodyY, mx = 12 + lean;
  const mouth = o.mouth || 'smile';
  if (mouth === 'smile') {
    set(g, mx - 2, my, 'e'); set(g, mx - 1, my, 'e'); set(g, mx, my, 'e'); set(g, mx + 1, my, 'e');
    set(g, mx - 2, my + 1, 'e'); set(g, mx + 1, my + 1, 'e');
  } else if (mouth === 'open') {
    fillEllipse(g, mx, my + 1, 2, 2, 'e');
  } else if (mouth === 'wow') {
    fillEllipse(g, mx, my + 1, 1, 1, 'e');
  } else if (mouth === 'frown') {
    set(g, mx - 2, my + 1, 'e'); set(g, mx - 1, my, 'e'); set(g, mx, my, 'e'); set(g, mx + 1, my, 'e'); set(g, mx + 1, my + 1, 'e');
  } else if (mouth === 'flat') {
    set(g, mx - 1, my, 'e'); set(g, mx, my, 'e');
  } else if (mouth === 'grr') {
    set(g, mx - 2, my, 'e'); set(g, mx - 1, my, 'e'); set(g, mx, my, 'e'); set(g, mx + 1, my, 'e');
    set(g, mx - 2, my + 1, 'e'); set(g, mx + 1, my + 1, 'e'); set(g, mx, my + 1, 'e');
  }

  const foot = o.foot || [0, 0, 0, 0, 0, 0];
  fillRect(g, 6 + lean + foot[0] + foot[1], 22 + bodyY + foot[2], 4, 2, bodyC);
  fillRect(g, 14 + lean + foot[3] + foot[4], 22 + bodyY + foot[5], 4, 2, bodyC);

  if (o.sweat) { fillEllipse(g, 20 + lean, 4 + bodyY, 1, 1, 'b'); set(g, 20 + lean, 3 + bodyY, 'b'); }

  if (o.stars) for (const [sx, sy] of o.stars) {
    set(g, sx, sy, 'y'); set(g, sx - 1, sy, 'y'); set(g, sx + 1, sy, 'y');
    set(g, sx, sy - 1, 'y'); set(g, sx, sy + 1, 'y');
  }
  if (o.sparks) for (const [sx, sy, c] of o.sparks) set(g, sx, sy, c);

  return g;
}

/* ================= 各状态动画帧 ================= */

const IDLE_FRAMES = [0, 1, 1, 2, 1, 0].map((bodyY, i) =>
  buildPet({ bodyY, eye: (i === 2 || i === 5) ? 'blink' : 'open', mouth: 'smile' }));

function runFrames(forward) {
  const out = [];
  const phases = [0, 1, 2, 3, 0, 1, 2, 3];
  const bounces = [1, 0, 2, 0, 1, 0, 2, 0];
  for (let i = 0; i < 8; i++) {
    const p = phases[i];
    let g = buildPet({
      bodyY: bounces[i], lean: forward ? 1 : -1, eye: 'open', mouth: 'smile',
      armL: (p === 1 || p === 3) ? 'pumpUp' : 'down',
      armR: (p === 1 || p === 3) ? 'down' : 'pumpUp',
      foot: [0, p === 1 ? -1 : p === 3 ? 1 : 0, p === 2 ? -1 : 0,
             0, p === 1 ? 1 : p === 3 ? -1 : 0, p === 0 ? -1 : 0],
    });
    if (!forward) g = mirrorGrid(g);
    out.push(g);
  }
  return out;
}
const RUN_RIGHT_FRAMES = runFrames(true);
const RUN_LEFT_FRAMES = runFrames(false);

const WAVING_FRAMES = [0, 1, 2, 1].map(i =>
  buildPet({ bodyY: i % 2, eye: 'happy', mouth: 'smile', armL: 'down',
             armR: (i === 0 || i === 2) ? 'wave' : 'pumpUp' }));

const JUMPING_FRAMES = [
  buildPet({ bodyY: 0, squash: 1, eye: 'open', mouth: 'smile' }),
  buildPet({ bodyY: -2, eye: 'open', mouth: 'wow' }),
  buildPet({ bodyY: -5, eye: 'open', mouth: 'wow', foot: [0, 0, -1, 0, 0, -1] }),
  buildPet({ bodyY: -3, eye: 'open', mouth: 'wow' }),
  buildPet({ bodyY: 0, squash: 1, eye: 'open', mouth: 'smile' }),
];

const FAILED_STARS = [
  [[2, 3], [21, 3], [2, 18], [21, 18]],
  [[3, 1], [20, 1], [3, 20], [20, 20]],
];
const FAILED_FRAMES = [0, 1, 2, 3, 4, 5, 6, 7].map(i =>
  buildPet({
    bodyY: 0, lean: i % 2,
    eye: i < 4 ? 'dizzy' : 'x',
    mouth: i < 4 ? 'frown' : 'open',
    color: i >= 6 ? 'r' : 'o',
    stars: FAILED_STARS[i % 2],
  }));

const WAITING_FRAMES = [0, 1, 0, 1, 0, 1].map(bodyY =>
  buildPet({ bodyY, eye: 'happy', mouth: 'smile' }));

const RUNNING_FRAMES = [0, 1, 2, 3, 4, 5].map(i =>
  buildPet({
    bodyY: i % 2 === 0 ? 0 : 2, lean: 2, eye: 'open', mouth: 'open',
    armL: i % 2 === 0 ? 'pumpUp' : 'down',
    armR: i % 2 === 0 ? 'down' : 'pumpUp',
    foot: [0, i % 2 === 0 ? -1 : 1, 0, 0, i % 2 === 0 ? 1 : -1, 0],
    sweat: i === 2 || i === 5,
  }));

const REVIEW_FRAMES = [0, 1, 2, 3, 4, 5].map(i =>
  buildPet({
    bodyY: i < 3 ? 0 : 1, lean: i < 3 ? 0 : 1,
    eye: i % 2 === 0 ? 'blink' : 'open',
    mouth: (i === 2 || i === 5) ? 'flat' : 'wow',
    armL: 'down', armR: i < 3 ? 'chin' : 'down',
  }));

const DEADLOOP_SPARKS = [
  [[4, 4, 'y'], [19, 4, 'w'], [4, 20, 'w'], [19, 20, 'y']],
  [[3, 5, 'w'], [20, 5, 'y'], [3, 19, 'y'], [20, 19, 'w']],
  [[5, 3, 'y'], [18, 3, 'w'], [5, 21, 'w'], [18, 21, 'y']],
  [[2, 6, 'w'], [21, 6, 'y'], [2, 18, 'y'], [21, 18, 'w']],
];
const DEADLOOP_FRAMES = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11].map(i =>
  buildPet({
    bodyY: 0, lean: i < 4 ? (i % 2 === 0 ? -2 : 2) : 0,
    eye: i < 4 ? 'angry' : (i < 9 ? 'dizzy' : 'x'),
    mouth: i < 4 ? 'grr' : 'wow',
    color: 'r',
    sparks: DEADLOOP_SPARKS[i % 4],
  }));

/* 行顺序必须与 PET_ANIM_* 枚举一致 */
const SHEET_ROWS = [
  IDLE_FRAMES, RUN_RIGHT_FRAMES, RUN_LEFT_FRAMES, WAVING_FRAMES, JUMPING_FRAMES,
  FAILED_FRAMES, WAITING_FRAMES, RUNNING_FRAMES, REVIEW_FRAMES, DEADLOOP_FRAMES,
];

/* ================= 拼图 + PNG 编码 ================= */

const CRC_TABLE = (() => {
  const t = new Int32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1);
    t[n] = c;
  }
  return t;
})();

function crc32(buf) {
  let c = 0xFFFFFFFF;
  for (let i = 0; i < buf.length; i++) c = CRC_TABLE[(c ^ buf[i]) & 0xFF] ^ (c >>> 8);
  return (c ^ 0xFFFFFFFF) >>> 0;
}

function chunk(type, data) {
  const len = Buffer.alloc(4);
  len.writeUInt32BE(data.length, 0);
  const body = Buffer.concat([Buffer.from(type, 'ascii'), data]);
  const crc = Buffer.alloc(4);
  crc.writeUInt32BE(crc32(body), 0);
  return Buffer.concat([len, body, crc]);
}

function encodePNG(w, h, rgba) {
  const raw = Buffer.alloc(h * (1 + w * 4));
  let o = 0;
  for (let y = 0; y < h; y++) {
    raw[o++] = 0;
    for (let x = 0; x < w * 4; x++) raw[o++] = rgba[y * w * 4 + x];
  }
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
  return Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A]),
    chunk('IHDR', ihdr),
    chunk('IDAT', zlib.deflateSync(raw, { level: 9 })),
    chunk('IEND', Buffer.alloc(0)),
  ]);
}

const W = FW * COLS, H = FH * ROWS;

/* 渲染一帧网格到 RGBA 缓冲 */
function renderFrame(grid, w, h) {
  const buf = Buffer.alloc(w * h * 4);
  for (let gy = 0; gy < GH; gy++)
    for (let gx = 0; gx < GW; gx++) {
      const [r, g0, b, a] = PAL[grid[gy][gx]];
      const px = gx * SCALE, py = gy * SCALE;
      for (let dy = 0; dy < SCALE; dy++)
        for (let dx = 0; dx < SCALE; dx++) {
          const idx = ((py + dy) * w + (px + dx)) * 4;
          buf[idx] = r; buf[idx + 1] = g0; buf[idx + 2] = b; buf[idx + 3] = a;
        }
    }
  return buf;
}

/* 拼精灵图 */
const sheetRgba = Buffer.alloc(W * H * 4);
for (let row = 0; row < ROWS; row++) {
  const frames = SHEET_ROWS[row];
  if (!frames) throw new Error(`Row ${row} has no frames`);
  if (frames.length > COLS) throw new Error(`Row ${row} exceeds ${COLS} cols`);
  frames.forEach((grid, col) => {
    const frame = renderFrame(grid, FW, FH);
    for (let y = 0; y < FH; y++) {
      const dst = (row * FH + y) * W * 4 + col * FW * 4;
      frame.copy(sheetRgba, dst, y * FW * 4, (y + 1) * FW * 4);
    }
  });
}

const outPng = encodePNG(W, H, sheetRgba);
const outPath = path.join(__dirname, '..', 'sprite.png');
fs.writeFileSync(outPath, outPng);

/* 自校验 */
if (outPng[0] !== 0x89 || outPng.toString('ascii', 1, 4) !== 'PNG')
  throw new Error('PNG 签名错误');
const iw = outPng.readUInt32BE(16), ih = outPng.readUInt32BE(20);
if (iw !== W || ih !== H) throw new Error(`尺寸错误 ${iw}x${ih}`);

/* ============ 输出每帧小 PNG (序列帧模式, 对应 pet_anim.c dir_name) ============ */
const DIR_NAMES = [
  "idle", "run_right", "run_left", "waving", "jumping",
  "failed", "waiting", "action", "inspect", "deadloop",
];
if (DIR_NAMES.length !== SHEET_ROWS.length)
  throw new Error("DIR_NAMES 与 SHEET_ROWS 行数不一致");

const framesRoot = path.join(__dirname, '..', 'sprites');
SHEET_ROWS.forEach((frames, row) => {
  const dir = path.join(framesRoot, DIR_NAMES[row]);
  fs.mkdirSync(dir, { recursive: true });
  frames.forEach((grid, i) => {
    const num = String(i + 1).padStart(3, '0');
    const png = encodePNG(FW, FH, renderFrame(grid, FW, FH));
    fs.writeFileSync(path.join(dir, `frame_${num}.png`), png);
  });
});

const frameCounts = SHEET_ROWS.map(f => f.length);
console.log(`OK: ${outPath}  ${W}x${H}  帧数/行: ${frameCounts.join(', ')}  (${outPng.length} bytes)`);
console.log(`OK: 序列帧目录: ${framesRoot}/<dir_name>/frame_XXX.png`);
