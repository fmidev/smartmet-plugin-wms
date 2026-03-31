/**
 * test_particles.js
 *
 * Node.js unit tests for WeatherParticles.js + WeatherTimeline.js.
 *
 * Run:  node test/canvas/test_particles.js
 *
 * The two JS files were designed for browser <script> loading — they rely
 * on each other's globals (WeatherParticles.js defines WindField which
 * WeatherTimeline.js uses in BlendedFields.makeWindField).  We simulate
 * that here by requiring both files and merging their exports into global.
 */

'use strict';

// ── Load modules, expose as globals (mimics <script> loading order) ───────
Object.assign(global, require('./WeatherParticles.js'));
Object.assign(global, require('./WeatherTimeline.js'));

// ── Minimal test harness ──────────────────────────────────────────────────
let passed = 0, failed = 0;

function test(name, fn) {
  try {
    fn();
    console.log(`  ✓  ${name}`);
    passed++;
  } catch (e) {
    console.error(`  ✗  ${name}`);
    console.error(`     ${e.message}`);
    failed++;
  }
}

function assert(cond, msg) {
  if (!cond) throw new Error(msg || 'assertion failed');
}

function assertClose(a, b, tol = 1e-6, msg) {
  if (Math.abs(a - b) > tol)
    throw new Error(msg || `expected ${a} ≈ ${b} (tol ${tol})`);
}

function assertRange(v, lo, hi, msg) {
  if (v < lo || v > hi)
    throw new Error(msg || `expected ${v} in [${lo}, ${hi}]`);
}

// ═════════════════════════════════════════════════════════════════════════
// 1. PRNG / hash quality (module-internal, verified via particle distribution)
// ═════════════════════════════════════════════════════════════════════════
console.log('\n── RainSystem determinism & physics ─────────────────────────────');

test('particle position is deterministic (same i, same t → same result)', () => {
  const r = new RainSystem({ worldWidth: 512, worldHeight: 512 });
  const s1 = r._state(42, 12345);
  const s2 = r._state(42, 12345);
  assert(s1.x === s2.x && s1.y === s2.y, 'positions differ');
});

test('different particles have different positions at t=0', () => {
  const r = new RainSystem({ worldWidth: 512, worldHeight: 512 });
  const positions = new Set();
  for (let i = 0; i < 100; i++) {
    const s = r._state(i, 0);
    const key = `${s.x.toFixed(3)},${s.y.toFixed(3)}`;
    assert(!positions.has(key), `duplicate position at i=${i}`);
    positions.add(key);
  }
});

test('positions are within world bounds', () => {
  const W = 400, H = 300;
  const r = new RainSystem({ worldWidth: W, worldHeight: H });
  for (let i = 0; i < 200; i++) {
    const s = r._state(i, 99999);
    assertRange(s.x, 0, W, `x out of bounds at i=${i}, t=99999`);
    assertRange(s.y, 0, H, `y out of bounds at i=${i}, t=99999`);
  }
});

test('particle falls downward over time (fallSpeed > 0)', () => {
  const r = new RainSystem({ worldWidth: 512, worldHeight: 512, fallSpeed: 0.3, windU: 0 });
  // Pick particle 7; check y increases (mod worldH)
  const y0 = r._state(7, 0).y;
  const y1 = r._state(7, 1000).y;
  // After 1000ms at 0.3 px/ms * speed(~1.0) = ~300px advance (mod 512)
  const advance = ((y1 - y0) + 512) % 512;
  assertRange(advance, 200, 400, `expected ~300px fall in 1000ms, got ${advance.toFixed(1)}`);
});

test('wind drift: positive windU moves x rightward', () => {
  const r = new RainSystem({ worldWidth: 512, worldHeight: 512, windU: 0.1, fallSpeed: 0 });
  const x0 = r._state(3, 0).x;
  const x1 = r._state(3, 1000).x;
  const drift = ((x1 - x0) + 512) % 512;
  assertRange(drift, 60, 140, `expected ~100px drift in 1000ms, got ${drift.toFixed(1)}`);
});

test('particle alpha is in [0, 1]', () => {
  const r = new RainSystem({ worldWidth: 512, worldHeight: 512 });
  for (let i = 0; i < 50; i++) {
    const s = r._state(i, 0);
    assertRange(s.alpha, 0, 1, `alpha out of range at i=${i}`);
  }
});

// ═════════════════════════════════════════════════════════════════════════
// 2. TILE CONTINUITY — core guarantee of the world-space design
// ═════════════════════════════════════════════════════════════════════════
console.log('\n── Tile continuity ──────────────────────────────────────────────');

test('every particle falls in exactly one of the four tiles at any time', () => {
  const WW = 512, WH = 512, TW = 256, TH = 256;
  const r = new RainSystem({
    worldWidth: WW, worldHeight: WH,
    count: 600, fallSpeed: 0.3, windU: 0.08,
  });

  // Four tile viewports (2x2)
  const tiles = [
    { x: 0,  y: 0,  w: TW, h: TH },
    { x: TW, y: 0,  w: TW, h: TH },
    { x: 0,  y: TH, w: TW, h: TH },
    { x: TW, y: TH, w: TW, h: TH },
  ];

  // Test at several time points including across a wrap boundary
  const times = [0, 1234, 5678, 99991, 200003];

  for (const t of times) {
    for (let i = 0; i < r.count; i++) {
      const { x, y } = r._state(i, t);

      // Count how many tiles this particle's canonical position belongs to
      // (ignoring drawing margin — just the point itself)
      let count = 0;
      for (const tile of tiles) {
        if (x >= tile.x && x < tile.x + tile.w &&
            y >= tile.y && y < tile.y + tile.h) count++;
      }

      assert(count === 1,
        `particle ${i} at t=${t} pos=(${x.toFixed(1)},${y.toFixed(1)}) ` +
        `is in ${count} tiles (expected exactly 1)`);
    }
  }
});

test('world-space and naive approaches differ at tile boundaries', () => {
  // A particle spawning near x=256 (tile boundary) in world-space should
  // straddle the boundary smoothly. Naive per-tile particles have no such relation.
  const r = new RainSystem({ worldWidth: 512, worldHeight: 512, count: 1000, windU: 0.1 });
  let boundaryCount = 0;
  for (let i = 0; i < 1000; i++) {
    const { x } = r._state(i, 5000);
    if (x > 250 && x < 262) boundaryCount++;
  }
  // With 1000 particles over 512px, ~24 should be in a 12px band (1000*12/512 ≈ 23)
  assertRange(boundaryCount, 10, 40,
    `expected ~23 particles near boundary, got ${boundaryCount}`);
});

// ═════════════════════════════════════════════════════════════════════════
// 3. SNOW SYSTEM
// ═════════════════════════════════════════════════════════════════════════
console.log('\n── SnowSystem ───────────────────────────────────────────────────');

test('snowflake positions are deterministic', () => {
  const s = new SnowSystem({ worldWidth: 512, worldHeight: 512 });
  const a = s._state(17, 3000);
  const b = s._state(17, 3000);
  assert(a.x === b.x && a.y === b.y, 'snow positions differ');
});

test('sway produces lateral displacement over time', () => {
  const s = new SnowSystem({
    worldWidth: 512, worldHeight: 512,
    swayAmplitude: 20, swayPeriod: 2000,
    fallSpeed: 0, windU: 0,
  });
  // Particle 5 at t=0 vs t=500 (quarter period) — should have moved laterally
  const x0 = s._state(5, 0).x;
  const x1 = s._state(5, 500).x;   // quarter period
  // The sway term: A * sin(2π*t/T + phase). Difference at t=0 vs t=T/4.
  // With A=20 this should differ by up to 20px
  const drift = Math.abs(((x1 - x0) + 512) % 512);
  // Can't be exact due to phase, but shouldn't be zero
  assert(drift > 0, `expected non-zero sway drift, got ${drift.toFixed(3)}`);
});

test('snowflake radius in configured range', () => {
  const s = new SnowSystem({ worldWidth: 512, worldHeight: 512, minRadius: 2, maxRadius: 6 });
  for (let i = 0; i < 50; i++) {
    const st = s._state(i, 0);
    assertRange(st.radius, 2, 6, `radius ${st.radius} out of [2,6] at i=${i}`);
  }
});

// ═════════════════════════════════════════════════════════════════════════
// 4. WIND FIELD
// ═════════════════════════════════════════════════════════════════════════
console.log('\n── WindField ────────────────────────────────────────────────────');

test('sample at grid corners returns exact stored values', () => {
  const u = new Float32Array([1, 2, 3, 4]);
  const v = new Float32Array([5, 6, 7, 8]);
  const f = new WindField({ cols: 2, rows: 2, u, v,
                            worldWidth: 100, worldHeight: 100 });
  // Top-left (world 0,0) → grid (0,0) → u=1, v=5
  assertClose(f.sample(0, 0).u, 1, 0.01, 'top-left u');
  assertClose(f.sample(0, 0).v, 5, 0.01, 'top-left v');
  // Top-right (world 100,0) → grid (1,0) → u=2, v=6
  assertClose(f.sample(100, 0).u, 2, 0.01, 'top-right u');
  // Bottom-left (world 0,100) → grid (0,1) → u=3, v=7
  assertClose(f.sample(0, 100).u, 3, 0.01, 'bottom-left u');
  // Bottom-right → u=4, v=8
  assertClose(f.sample(100, 100).u, 4, 0.01, 'bottom-right u');
});

test('bilinear interpolation: center of 2x2 = average of corners', () => {
  const u = new Float32Array([0, 4, 8, 12]);  // corners: 0,4,8,12 → avg = 6
  const v = new Float32Array([1, 3, 5, 7]);   // avg = 4
  const f = new WindField({ cols: 2, rows: 2, u, v,
                            worldWidth: 100, worldHeight: 100 });
  assertClose(f.sample(50, 50).u, 6, 0.01, 'center u should be average of corners');
  assertClose(f.sample(50, 50).v, 4, 0.01, 'center v should be average of corners');
});

test('makeSynthetic produces non-zero vortex component', () => {
  const f = WindField.makeSynthetic({
    cols: 8, rows: 8, worldWidth: 512, worldHeight: 512,
    uniformU: 0, uniformV: 0, vortexStrength: 0.1,
  });
  // At the top of the vortex (cx, 0), tangential wind should be non-zero
  const s = f.sample(256, 0);
  assert(Math.abs(s.u) > 0 || Math.abs(s.v) > 0, 'expected non-zero vortex wind');
});

// ═════════════════════════════════════════════════════════════════════════
// 5. WEATHER TIMELINE helpers
// ═════════════════════════════════════════════════════════════════════════
console.log('\n── WeatherTimeline helpers ──────────────────────────────────────');

test('smoothstep: s(0)=0, s(0.5)=0.5, s(1)=1', () => {
  assertClose(smoothstep(0),   0,   1e-9, 's(0)');
  assertClose(smoothstep(0.5), 0.5, 1e-9, 's(0.5)');
  assertClose(smoothstep(1),   1,   1e-9, 's(1)');
});

test('smoothstep is monotonically increasing', () => {
  let prev = -1;
  for (let t = 0; t <= 1; t += 0.01) {
    const s = smoothstep(t);
    assert(s >= prev, `smoothstep not monotone at t=${t.toFixed(2)}`);
    prev = s;
  }
});

test('smoothstep clamps outside [0,1]', () => {
  assertClose(smoothstep(-1), 0, 1e-9, 'clamp below 0');
  assertClose(smoothstep(2),  1, 1e-9, 'clamp above 1');
});

test('lerpArray: correct interpolation at alpha=0, 0.5, 1', () => {
  const a = new Float32Array([0, 10, 20]);
  const b = new Float32Array([10, 30, 60]);
  const r0 = lerpArray(a, b, 0);
  const r5 = lerpArray(a, b, 0.5);
  const r1 = lerpArray(a, b, 1);
  assertClose(r0[1], 10, 1e-5, 'alpha=0 should return a');
  assertClose(r5[1], 20, 1e-5, 'alpha=0.5 mid-point');
  assertClose(r1[2], 60, 1e-5, 'alpha=1 should return b');
});

test('lerpAngles wraps correctly across 360°', () => {
  const a = new Float32Array([350]);  // near north, clockwise
  const b = new Float32Array([10]);   // just past north
  const mid = lerpAngles(a, b, 0.5);
  // Shortest arc: 350→10 = +20° → midpoint = 360 = 0°
  assertClose(((mid[0] % 360) + 360) % 360, 0, 1e-3,
    `expected 0°, got ${mid[0].toFixed(3)}`);
});

test('lerpAngles wraps correctly crossing 180° (SW wind example)', () => {
  const a = new Float32Array([170]);
  const b = new Float32Array([190]);
  const mid = lerpAngles(a, b, 0.5);
  assertClose(mid[0], 180, 1e-3, `expected 180°, got ${mid[0].toFixed(3)}`);
});

// ═════════════════════════════════════════════════════════════════════════
// 6. WEATHER TIMELINE — blending logic
// ═════════════════════════════════════════════════════════════════════════
console.log('\n── WeatherTimeline blending ─────────────────────────────────────');

/** Build a minimal 3-frame timeline for testing. */
function makeTestTimeline(frameIntervalMs = 1000, transitionMs = 200) {
  const frames = [0, 1, 2].map(i => ({
    label:      `T+${i}h`,
    windU:      new Float32Array([i * 10]),
    windV:      new Float32Array([0]),
    precipRate: new Float32Array([i * 0.5]),
    precipType: new Uint8Array([i === 2 ? 2 : 1]),  // frame 2 = snow
  }));

  const tl = new WeatherTimeline({
    frames, frameIntervalMs, transitionMs,
    gridInfo: { cols: 1, rows: 1, worldWidth: 100, worldHeight: 100 },
  });

  // We test by controlling the playhead directly.
  // Pause at a specific ms position so getBlended() is deterministic.
  tl._pausedMs = 0;
  tl._playing  = false;
  return { tl, frames };
}

test('alpha = 0 in the middle of a frame dwell', () => {
  const { tl } = makeTestTimeline(1000, 200);
  // Middle of frame 0 (t=500ms), well before transition window (800ms)
  tl._pausedMs = 500;
  const b = tl.getBlended();
  assertClose(b.alpha, 0, 1e-9, `expected alpha=0 at t=500ms, got ${b.alpha}`);
  assert(b.frameIndex === 0, `expected frame 0, got ${b.frameIndex}`);
});

test('alpha > 0 during transition window', () => {
  const { tl } = makeTestTimeline(1000, 200);
  // t=900ms: 100ms into the 200ms transition → raw_alpha=0.5
  tl._pausedMs = 900;
  const b = tl.getBlended();
  assert(b.alpha > 0 && b.alpha < 1,
    `expected 0 < alpha < 1 during transition, got ${b.alpha.toFixed(4)}`);
});

test('alpha = 0 at start of next frame', () => {
  const { tl } = makeTestTimeline(1000, 200);
  tl._pausedMs = 1000;  // exactly frame 1, dwell position 0
  const b = tl.getBlended();
  assertClose(b.alpha, 0, 1e-9, `expected alpha=0 at frame start`);
  assert(b.frameIndex === 1, `expected frame 1, got ${b.frameIndex}`);
});

test('windU is lerped between frames during transition', () => {
  const { tl, frames } = makeTestTimeline(1000, 200);
  // t=900ms: 50% through transition frame0→frame1 (raw_alpha=0.5, s(0.5)=0.5)
  tl._pausedMs = 900;
  const b = tl.getBlended();
  // windU: frame0=0, frame1=10 → lerp at alpha=smoothstep(0.5)=0.5 → 5
  assertClose(b.windU[0], 5, 0.1,
    `expected windU≈5 mid-transition, got ${b.windU[0].toFixed(3)}`);
});

test('precipRate is lerped during transition', () => {
  const { tl } = makeTestTimeline(1000, 200);
  tl._pausedMs = 900;  // 50% through frame0→frame1 transition
  const b = tl.getBlended();
  // precipRate: frame0=0, frame1=0.5 → lerp ≈ 0.25
  assertClose(b.precipRate[0], 0.25, 0.05,
    `expected precipRate≈0.25 mid-transition, got ${b.precipRate[0].toFixed(3)}`);
});

test('snowFraction: 0 during rain frame, 1 during snow frame', () => {
  const { tl } = makeTestTimeline(1000, 200);
  // Frame 0: rain (type=1), deep in dwell → alpha=0 → snowFraction should be 0
  tl._pausedMs = 500;
  assert(tl.getBlended().snowFraction(0) === 0, 'expected snow=0 in rain frame');

  // Frame 2: snow (type=2), deep in dwell
  tl._pausedMs = 2500;
  assert(tl.getBlended().snowFraction(0) === 1, 'expected snow=1 in snow frame');
});

test('timeline loops: t = totalMs wraps back to frame 0', () => {
  const { tl } = makeTestTimeline(1000, 200);
  const totalMs = 3 * 1000;
  tl._pausedMs = totalMs;  // exactly one full loop
  const b = tl.getBlended();
  assert(b.frameIndex === 0, `expected frame 0 after full loop, got ${b.frameIndex}`);
});

test('step(+1) advances playhead by one frame interval', () => {
  const { tl } = makeTestTimeline(1000, 200);
  tl._pausedMs = 0;
  tl.step(+1);
  assertClose(tl._pausedMs, 1000, 1,
    `expected playhead at 1000ms after step(+1), got ${tl._pausedMs}`);
});

test('step(-1) retreats playhead by one frame interval', () => {
  const { tl } = makeTestTimeline(1000, 200);
  tl._pausedMs = 2500;
  tl.step(-1);
  assertClose(tl._pausedMs, 1500, 1,
    `expected 1500ms after step(-1), got ${tl._pausedMs}`);
});

test('makeWindField returns a WindField with blended U/V', () => {
  const { tl } = makeTestTimeline(1000, 200);
  tl._pausedMs = 900;  // 50% transition frame0→frame1
  const b = tl.getBlended();
  const wf = b.makeWindField();
  assert(wf instanceof WindField, 'expected WindField instance');
  // Sample at center (world 50, 50) = grid only has 1 point → should get blended windU
  assertClose(wf.sample(50, 50).u, b.windU[0], 0.001, 'WindField u should match blended windU');
});

// ═════════════════════════════════════════════════════════════════════════
// 7. ARROW TEST DATA — verify synthetic frame generation
// ═════════════════════════════════════════════════════════════════════════
console.log('\n── Arrow test data (Finland/Baltic) ─────────────────────────────');

// Replicate the frame generation logic from arrow_test.html to test it in node
const GRID_COLS = 35, GRID_ROWS = 25;
const LON0 = 17, LAT0 = 59;

function makeArrowFrame({ bgU, bgV, vortex = [], rain = [], snow = [] }) {
  const GRID_N = GRID_COLS * GRID_ROWS;
  const windU = new Float32Array(GRID_N);
  const windV = new Float32Array(GRID_N);
  const precipRate = new Float32Array(GRID_N);
  const precipType = new Uint8Array(GRID_N);

  for (let r = 0; r < GRID_ROWS; r++) {
    for (let c = 0; c < GRID_COLS; c++) {
      const lon = LON0 + c * 0.5;
      const lat = LAT0 + r * 0.5;
      const idx = r * GRID_COLS + c;

      let u = bgU, v = bgV;
      for (const vor of vortex) {
        const dx = (lon - vor.lon) * 70;
        const dy = (lat - vor.lat) * 111;
        const dist = Math.sqrt(dx*dx + dy*dy) + 1;
        const str = vor.strength / (1 + (dist / vor.radius) ** 2);
        u += -dy / dist * str;
        v +=  dx / dist * str;
      }
      windU[idx] = u;
      windV[idx] = v;

      let rate = 0, type = 0;
      for (const blob of rain) {
        const dx = (lon - blob.lon) / blob.radLon;
        const dy = (lat - blob.lat) / blob.radLat;
        const r2 = dx*dx + dy*dy;
        const br = blob.intensity * Math.exp(-r2);
        if (br > rate) { rate = br; type = 1; }
      }
      for (const blob of snow) {
        const dx = (lon - blob.lon) / blob.radLon;
        const dy = (lat - blob.lat) / blob.radLat;
        const r2 = dx*dx + dy*dy;
        const br = blob.intensity * Math.exp(-r2);
        if (br > rate) { rate = br; type = 2; }
      }
      precipRate[idx] = Math.min(1, rate);
      precipType[idx] = type;
    }
  }
  return { windU, windV, precipRate, precipType };
}

test('synthetic frame has correct grid size', () => {
  const f = makeArrowFrame({ bgU: 4, bgV: 3 });
  assert(f.windU.length === GRID_COLS * GRID_ROWS,
    `expected ${GRID_COLS * GRID_ROWS}, got ${f.windU.length}`);
});

test('background wind present in vortex-free frame', () => {
  const f = makeArrowFrame({ bgU: 5, bgV: 2 });
  // Every point should have u=5, v=2 exactly when no vortex
  for (let i = 0; i < f.windU.length; i++) {
    assertClose(f.windU[i], 5, 0.001, `windU at ${i} should be 5`);
    assertClose(f.windV[i], 2, 0.001, `windV at ${i} should be 2`);
  }
});

test('vortex adds cyclonic flow around centre', () => {
  const f = makeArrowFrame({
    bgU: 0, bgV: 0,
    vortex: [{ lon: 25, lat: 65, strength: 20, radius: 200 }],
  });
  // Grid centre at lon≈25,lat≈65: grid col=(25-17)/0.5=16, row=(65-59)/0.5=12
  // Points north of centre (row=14 vs row=12): should have positive u (eastward)
  const idxNorth = 14 * GRID_COLS + 16;
  const idxSouth = 10 * GRID_COLS + 16;
  // Counterclockwise (cyclonic, N. Hemisphere): north of centre → westward (U<0),
  // south of centre → eastward (U>0).
  assert(f.windU[idxNorth] < 0, `expected westward flow north of low (CCW), got ${f.windU[idxNorth].toFixed(2)}`);
  assert(f.windU[idxSouth] > 0, `expected eastward flow south of low (CCW), got ${f.windU[idxSouth].toFixed(2)}`);
});

test('rain blob produces precipitation at its centre', () => {
  const f = makeArrowFrame({
    bgU: 0, bgV: 0,
    rain: [{ lon: 24, lat: 62, radLon: 2, radLat: 2, intensity: 0.8 }],
  });
  // col=(24-17)/0.5=14, row=(62-59)/0.5=6
  const idx = 6 * GRID_COLS + 14;
  assertRange(f.precipRate[idx], 0.6, 0.85,
    `expected ~0.8 precip at blob centre, got ${f.precipRate[idx].toFixed(3)}`);
  assert(f.precipType[idx] === 1, `expected rain type=1, got ${f.precipType[idx]}`);
});

test('snow blob produces snow-type precipitation', () => {
  const f = makeArrowFrame({
    bgU: 0, bgV: 0,
    snow: [{ lon: 27, lat: 68, radLon: 2, radLat: 2, intensity: 0.9 }],
  });
  const col = Math.round((27 - LON0) / 0.5);
  const row = Math.round((68 - LAT0) / 0.5);
  const idx = row * GRID_COLS + col;
  assert(f.precipType[idx] === 2, `expected snow type=2, got ${f.precipType[idx]}`);
});

test('met wind direction from U/V: westerly wind (U>0,V=0) → from west (270°)', () => {
  const u = 5, v = 0;
  let dir = Math.atan2(-u, -v) * 180 / Math.PI;
  if (dir < 0) dir += 360;
  assertClose(dir, 270, 0.1, `westerly wind should be from 270°, got ${dir.toFixed(1)}`);
});

test('met wind direction: southerly wind (U=0,V>0) → from south (180°)', () => {
  const u = 0, v = 5;
  let dir = Math.atan2(-u, -v) * 180 / Math.PI;
  if (dir < 0) dir += 360;
  assertClose(dir, 180, 0.1, `southerly wind should be from 180°, got ${dir.toFixed(1)}`);
});

test('met wind direction: northerly wind (U=0,V<0) → from north (0°)', () => {
  const u = 0, v = -5;
  let dir = Math.atan2(-u, -v) * 180 / Math.PI;
  if (dir < 0) dir += 360;
  assertClose(dir, 0, 0.1, `northerly wind should be from 0°, got ${dir.toFixed(1)}`);
});

// ═════════════════════════════════════════════════════════════════════════
// Summary
// ═════════════════════════════════════════════════════════════════════════
console.log(`\n${'─'.repeat(60)}`);
console.log(`  ${passed} passed, ${failed} failed`);
if (failed > 0) {
  console.error('\nSome tests failed.');
  process.exit(1);
} else {
  console.log('\nAll tests passed.');
}
