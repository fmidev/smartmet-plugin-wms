/**
 * WeatherTimeline.js
 *
 * Maps a continuously-advancing wall-clock time to blended weather fields.
 *
 * Design goals:
 *   1. Particle micro-motion (rain falling, wind flowing) never resets or jumps.
 *      It is driven purely by performance.now() — the wall clock never stops.
 *   2. When the forecast timestep advances, the FIELDS the particles live in
 *      cross-fade smoothly over a configurable transition window.
 *   3. One timeline drives all effects (wind, rain, snow) — no per-effect animators.
 *   4. Stateless per frame: getBlended() is a pure function of the playhead position.
 *
 * Usage:
 *   const tl = new WeatherTimeline({ frames, frameIntervalMs: 2000, transitionMs: 600 });
 *   tl.play();
 *
 *   // In rAF loop:
 *   const blended = tl.getBlended();
 *   windSys.field = blended.makeWindField(gridInfo);
 *   rainSys.draw(ctx, vp, performance.now(), blended.precipRate);
 *
 * Frame format:
 *   {
 *     label:      string,              // e.g. "T+0h  2008-08-05 03:00"
 *     windU:      Float32Array,        // zonal wind, row-major, + = eastward (m/s)
 *     windV:      Float32Array,        // meridional wind, + = northward (m/s)
 *     precipRate: Float32Array,        // 0..1 (normalised), same grid
 *     precipType: Uint8Array,          // 0=none, 1=rain, 2=snow, 3=sleet
 *   }
 */

'use strict';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function smoothstep(t) {
  t = Math.max(0, Math.min(1, t));
  return t * t * (3 - 2 * t);
}

function lerpArray(a, b, alpha) {
  if (alpha <= 0) return a;
  if (alpha >= 1) return b;
  const out = new Float32Array(a.length);
  for (let i = 0; i < a.length; i++) out[i] = a[i] + (b[i] - a[i]) * alpha;
  return out;
}

/** Interpolate angles in degrees, choosing the shortest arc. */
function lerpAngles(a, b, alpha) {
  if (alpha <= 0) return a;
  if (alpha >= 1) return b;
  const out = new Float32Array(a.length);
  for (let i = 0; i < a.length; i++) {
    let diff = ((b[i] - a[i]) % 360 + 540) % 360 - 180;  // shortest arc
    out[i] = a[i] + diff * alpha;
  }
  return out;
}

// ---------------------------------------------------------------------------
// BlendedFields  — returned by WeatherTimeline.getBlended()
// ---------------------------------------------------------------------------
class BlendedFields {
  constructor(f0, f1, alpha, frameIndex, gridInfo) {
    this.windU      = lerpArray(f0.windU,      f1.windU,      alpha);
    this.windV      = lerpArray(f0.windV,      f1.windV,      alpha);
    this.precipRate = lerpArray(f0.precipRate, f1.precipRate, alpha);

    // Precipitation type: don't lerp the int — cross-fade weight towards f1
    this.precipTypeA     = f0.precipType;   // Uint8Array, current frame
    this.precipTypeB     = f1.precipType;   // Uint8Array, next frame
    this.precipTypeAlpha = alpha;           // 0 = all A, 1 = all B

    this.frameIndex = frameIndex;
    this.alpha      = alpha;
    this._gridInfo  = gridInfo;
  }

  /**
   * Returns the snow fraction at grid index i (0 = all rain, 1 = all snow).
   * Uses cross-fade between frame types.
   */
  snowFraction(i) {
    const a = this.precipTypeA[i] === 2 ? 1 : 0;
    const b = this.precipTypeB[i] === 2 ? 1 : 0;
    return a + (b - a) * this.precipTypeAlpha;
  }

  /**
   * Build a WindField object suitable for WindParticleSystem.
   * gridInfo: { cols, rows, worldWidth, worldHeight, worldX?, worldY? }
   */
  makeWindField(gridInfo) {
    const gi = gridInfo || this._gridInfo;
    return new WindField({
      cols: gi.cols, rows: gi.rows,
      u: this.windU, v: this.windV,
      worldX: gi.worldX || 0, worldY: gi.worldY || 0,
      worldWidth: gi.worldWidth, worldHeight: gi.worldHeight,
    });
  }
}

// ---------------------------------------------------------------------------
// WeatherTimeline
// ---------------------------------------------------------------------------
class WeatherTimeline {
  /**
   * @param {object} cfg
   * @param {Array}  cfg.frames            - Array of weather frame objects
   * @param {number} cfg.frameIntervalMs   - Wall-clock ms each frame is shown
   * @param {number} cfg.transitionMs      - Cross-fade window at end of each frame
   * @param {object} [cfg.gridInfo]        - Default grid info forwarded to BlendedFields
   * @param {number} [cfg.speedMultiplier] - Playback speed (default 1)
   */
  constructor({
    frames,
    frameIntervalMs   = 2000,
    transitionMs      = 600,
    gridInfo          = null,
    speedMultiplier   = 1,
  }) {
    this.frames           = frames;
    this.frameIntervalMs  = frameIntervalMs;
    this.transitionMs     = transitionMs;
    this.gridInfo         = gridInfo;
    this.speedMultiplier  = speedMultiplier;

    this._startT    = null;   // performance.now() when playback started (adjusted for pauses)
    this._pausedMs  = null;   // playhead ms at time of pause
    this._playing   = false;
  }

  get playing() { return this._playing; }

  play() {
    if (this._playing) return;
    const now = performance.now();
    this._startT   = now - (this._pausedMs ?? 0);
    this._pausedMs = null;
    this._playing  = true;
  }

  pause() {
    if (!this._playing) return;
    this._pausedMs = this._playheadMs();
    this._playing  = false;
  }

  toggle() { this._playing ? this.pause() : this.play(); }

  /** Step forward (+1) or backward (-1) by one frame. Keeps play state. */
  step(delta) {
    const total = this.frames.length * this.frameIntervalMs;
    const cur   = this._playheadMs();
    const next  = ((cur + delta * this.frameIntervalMs) % total + total) % total;
    this._pausedMs = next;
    if (this._playing) this._startT = performance.now() - next;
  }

  set speed(v) {
    // Preserve playhead position when changing speed
    const cur = this._playheadMs();
    this.speedMultiplier = v;
    this._pausedMs = cur;
    if (this._playing) this._startT = performance.now() - cur;
  }
  get speed() { return this.speedMultiplier; }

  /** Playhead in ms (unscaled, loops over total duration). */
  _playheadMs() {
    if (!this._playing && this._pausedMs === null) return 0;
    if (!this._playing) return this._pausedMs;
    const elapsed = (performance.now() - this._startT) * this.speedMultiplier;
    const total   = this.frames.length * this.frameIntervalMs;
    return elapsed % total;
  }

  /** Normalised playhead position [0,1). */
  get progress() {
    return this._playheadMs() / (this.frames.length * this.frameIntervalMs);
  }

  /**
   * Returns a BlendedFields snapshot for the current playhead position.
   * Call once per rAF, then use the result for all draw calls this frame.
   */
  getBlended() {
    const n     = this.frames.length;
    const total = n * this.frameIntervalMs;
    const ms    = this._playheadMs() % total;

    const frameF = ms / this.frameIntervalMs;
    const f0     = Math.floor(frameF) % n;
    const f1     = (f0 + 1) % n;

    // Transition happens only in the LAST `transitionMs` of each frame's dwell.
    const frameFrac     = frameF - Math.floor(frameF);            // 0..1 within frame
    const transStart    = 1 - this.transitionMs / this.frameIntervalMs;
    const rawAlpha      = Math.max(0, (frameFrac - transStart) / (this.transitionMs / this.frameIntervalMs));
    const alpha         = smoothstep(rawAlpha);

    return new BlendedFields(this.frames[f0], this.frames[f1], alpha, f0, this.gridInfo);
  }

  /** Current displayed frame index (0-based, updates mid-transition). */
  get currentFrame() {
    return this.getBlended().frameIndex;
  }

  get currentLabel() {
    return this.frames[this.currentFrame]?.label ?? '';
  }
}

// Export
if (typeof module !== 'undefined') {
  module.exports = { WeatherTimeline, BlendedFields, smoothstep, lerpArray, lerpAngles };
}
