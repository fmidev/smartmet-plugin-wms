/**
 * WeatherParticles.js
 *
 * World-space deterministic particle system for weather animation.
 *
 * CORE PRINCIPLE: Every particle's position is a pure function of (id, time_ms).
 * No shared mutable state. This means any tile canvas can independently compute
 * positions for all particles and render only those within its viewport bounds —
 * yielding seamless particle crossing at tile boundaries without any communication
 * between tiles.
 *
 * Usage (single canvas):
 *   const rain = new RainSystem({ worldWidth: 800, worldHeight: 600, windU: 0.08 });
 *   function loop(t) {
 *     ctx.clearRect(0, 0, 800, 600);
 *     rain.draw(ctx, { x: 0, y: 0, width: 800, height: 600 }, t);
 *     requestAnimationFrame(loop);
 *   }
 *   requestAnimationFrame(loop);
 *
 * Usage (tiled):
 *   // All tiles share ONE RainSystem instance (or identical configs).
 *   // Each tile passes its own viewport (in world coordinates).
 *   // Time base: performance.now() — same JS clock across all tiles on the page.
 *   const rain = new RainSystem({ worldWidth: 1024, worldHeight: 1024 });
 *   // Tile (0,0): rain.draw(ctx, { x: 0,   y: 0,   width: 512, height: 512 }, t);
 *   // Tile (1,0): rain.draw(ctx, { x: 512, y: 0,   width: 512, height: 512 }, t);
 *   // etc.
 */

'use strict';

// ---------------------------------------------------------------------------
// Deterministic PRNG  (Murmur3 integer finalizer)
// Returns float in [0, 1) given a non-negative integer seed.
// ---------------------------------------------------------------------------
function ihash(n) {
  n = Math.imul(n ^ (n >>> 16), 0x45d9f3b);
  n = Math.imul(n ^ (n >>> 16), 0x45d9f3b);
  return (n ^ (n >>> 16)) >>> 0;
}
function fhash(n) {
  return ihash(n) / 0x100000000;
}

// ---------------------------------------------------------------------------
// RainSystem
// ---------------------------------------------------------------------------
class RainSystem {
  /**
   * @param {object} cfg
   * @param {number} cfg.worldWidth   - World space width in px
   * @param {number} cfg.worldHeight  - World space height in px
   * @param {number} cfg.count        - Total particle count in world
   * @param {number} cfg.fallSpeed    - Base fall speed (world px / ms)
   * @param {number} cfg.windU        - Horizontal wind drift (world px / ms, + = right)
   * @param {number} cfg.minLength    - Min streak length in screen px
   * @param {number} cfg.maxLength    - Max streak length in screen px
   * @param {string} cfg.color        - CSS r,g,b string e.g. "170,200,255"
   */
  constructor({
    worldWidth  = 512,
    worldHeight = 512,
    count       = 600,
    fallSpeed   = 0.28,
    windU       = 0.06,
    minLength   = 7,
    maxLength   = 16,
    color       = '160,195,255',
  } = {}) {
    Object.assign(this, { worldWidth, worldHeight, count, fallSpeed, windU,
                          minLength, maxLength, color });
  }

  /** Compute particle state at time t (ms). Pure function of (i, t). */
  _state(i, t) {
    const x0    = fhash(i * 4 + 0) * this.worldWidth;
    const y0    = fhash(i * 4 + 1) * this.worldHeight;
    const speed = 0.75 + fhash(i * 4 + 2) * 0.5;   // 0.75 .. 1.25
    const alpha = 0.30 + fhash(i * 4 + 3) * 0.50;

    const wx = this.worldWidth;
    const wh = this.worldHeight;
    const x  = ((x0 + this.windU * t * speed) % wx + wx) % wx;
    const y  = ((y0 + this.fallSpeed * t * speed) % wh + wh) % wh;
    const len = this.minLength + fhash(i * 13) * (this.maxLength - this.minLength);

    return { x, y, alpha, len, speed };
  }

  /**
   * Draw rain particles that fall within viewport.
   * @param {CanvasRenderingContext2D} ctx
   * @param {{x,y,width,height}} vp  - Viewport in world coordinates
   * @param {number} t               - Time in ms (use performance.now())
   * @param {WindField} [precipField] - Optional: spatially-varying precip rate field
   *   (same world-space coords as vp). Each sample returns a value 0..1 that scales
   *   the particle's alpha. When the timeline blends precip rate between timesteps,
   *   passing the blended field here gives continuous fade-in/fade-out per region
   *   without any per-particle state. Particles are always "alive" — they simply
   *   become invisible where precipitation is absent.
   */
  draw(ctx, vp, t, precipField = null) {
    const { x: vx, y: vy, width: vw, height: vh } = vp;
    const margin = this.maxLength + 4;
    const angle  = Math.atan2(this.windU, this.fallSpeed);
    const dx     = Math.sin(angle);
    const dy     = Math.cos(angle);

    ctx.save();
    ctx.lineWidth = 1.3;

    for (let i = 0; i < this.count; i++) {
      const s = this._state(i, t);

      // Handle world-wrap: check particle at its canonical position
      // and also one period away in each axis so edge tiles work correctly.
      for (let ox = -1; ox <= 1; ox++) {
        for (let oy = -1; oy <= 1; oy++) {
          const px = s.x + ox * this.worldWidth;
          const py = s.y + oy * this.worldHeight;
          if (px < vx - margin || px > vx + vw + margin) continue;
          if (py < vy - margin || py > vy + vh + margin) continue;

          const sx = px - vx;
          const sy = py - vy;

          // Modulate by local precipitation rate (0..1) if a field is provided.
          // This is the "timeout" mechanism: when the timeline cross-fades the
          // precip rate to zero in a region, particles there fade out smoothly
          // without any per-particle state. No particle is ever "killed" or
          // "spawned" — they simply become transparent.
          let fieldAlpha = 1;
          if (precipField) {
            const rate = precipField.sample(px, py);
            if (rate <= 0) continue;
            fieldAlpha = Math.min(1, rate);
          }

          ctx.strokeStyle = `rgba(${this.color},${s.alpha * fieldAlpha})`;
          ctx.beginPath();
          ctx.moveTo(sx, sy);
          ctx.lineTo(sx - dx * s.len * s.speed, sy - dy * s.len * s.speed);
          ctx.stroke();
        }
      }
    }
    ctx.restore();
  }
}

// ---------------------------------------------------------------------------
// SnowSystem
// ---------------------------------------------------------------------------
class SnowSystem {
  /**
   * @param {object} cfg
   * @param {number} cfg.worldWidth
   * @param {number} cfg.worldHeight
   * @param {number} cfg.count
   * @param {number} cfg.fallSpeed   - Base fall speed (world px / ms)
   * @param {number} cfg.windU       - Mean horizontal drift (world px / ms)
   * @param {number} cfg.swayAmplitude - Sinusoidal sway amplitude (world px)
   * @param {number} cfg.swayPeriod  - Sway period (ms)
   * @param {number} cfg.minRadius   - Min flake radius (screen px)
   * @param {number} cfg.maxRadius   - Max flake radius (screen px)
   */
  constructor({
    worldWidth    = 512,
    worldHeight   = 512,
    count         = 250,
    fallSpeed     = 0.08,
    windU         = 0.02,
    swayAmplitude = 18,
    swayPeriod    = 4000,
    minRadius     = 2,
    maxRadius     = 5,
  } = {}) {
    Object.assign(this, { worldWidth, worldHeight, count, fallSpeed, windU,
                          swayAmplitude, swayPeriod, minRadius, maxRadius });
  }

  _state(i, t) {
    const x0       = fhash(i * 5 + 0) * this.worldWidth;
    const y0       = fhash(i * 5 + 1) * this.worldHeight;
    const speed    = 0.6 + fhash(i * 5 + 2) * 0.8;
    const phase    = fhash(i * 5 + 3) * Math.PI * 2;   // sway phase offset
    const radius   = this.minRadius + fhash(i * 5 + 4) * (this.maxRadius - this.minRadius);
    const rotation = fhash(i * 17) * Math.PI / 3;  // 0..60° rotation of flake arms

    const wx = this.worldWidth;
    const wh = this.worldHeight;
    const sway = this.swayAmplitude * Math.sin(2 * Math.PI * t / this.swayPeriod + phase);

    const x = ((x0 + this.windU * t * speed + sway) % wx + wx) % wx;
    const y = ((y0 + this.fallSpeed * t * speed)       % wh + wh) % wh;

    return { x, y, radius, rotation, alpha: 0.5 + fhash(i * 31) * 0.4, speed };
  }

  /** Draw a 6-armed snowflake centered at (cx, cy) with given radius */
  _drawFlake(ctx, cx, cy, r, rotation) {
    ctx.save();
    ctx.translate(cx, cy);
    ctx.rotate(rotation);
    ctx.beginPath();
    for (let arm = 0; arm < 6; arm++) {
      const a = arm * Math.PI / 3;
      ctx.moveTo(0, 0);
      ctx.lineTo(Math.cos(a) * r, Math.sin(a) * r);
      // Small side branches
      const bx = Math.cos(a) * r * 0.5;
      const by = Math.sin(a) * r * 0.5;
      const bLen = r * 0.3;
      const ba1 = a + Math.PI / 4;
      const ba2 = a - Math.PI / 4;
      ctx.moveTo(bx, by);
      ctx.lineTo(bx + Math.cos(ba1) * bLen, by + Math.sin(ba1) * bLen);
      ctx.moveTo(bx, by);
      ctx.lineTo(bx + Math.cos(ba2) * bLen, by + Math.sin(ba2) * bLen);
    }
    ctx.stroke();
    ctx.restore();
  }

  /** @param {WindField} [precipField] - Optional spatially-varying snow rate 0..1 */
  draw(ctx, vp, t, precipField = null) {
    const { x: vx, y: vy, width: vw, height: vh } = vp;
    const margin = this.maxRadius + 4;

    ctx.save();
    ctx.lineWidth = 1.0;

    for (let i = 0; i < this.count; i++) {
      const s = this._state(i, t);

      for (let ox = -1; ox <= 1; ox++) {
        for (let oy = -1; oy <= 1; oy++) {
          const px = s.x + ox * this.worldWidth;
          const py = s.y + oy * this.worldHeight;
          if (px < vx - margin || px > vx + vw + margin) continue;
          if (py < vy - margin || py > vy + vh + margin) continue;

          let fieldAlpha = 1;
          if (precipField) {
            const rate = precipField.sample(px, py);
            if (rate <= 0) continue;
            fieldAlpha = Math.min(1, rate);
          }

          ctx.strokeStyle = `rgba(220,235,255,${s.alpha * fieldAlpha})`;
          this._drawFlake(ctx, px - vx, py - vy, s.radius, s.rotation);
        }
      }
    }
    ctx.restore();
  }
}

// ---------------------------------------------------------------------------
// WindField  — a 2D bilinearly-sampled velocity field
// ---------------------------------------------------------------------------
class WindField {
  /**
   * @param {object} cfg
   * @param {number}       cfg.cols        - Grid columns
   * @param {number}       cfg.rows        - Grid rows
   * @param {Float32Array} cfg.u           - Zonal wind, size cols*rows (row-major)
   * @param {Float32Array} cfg.v           - Meridional wind, same size
   * @param {number}       cfg.worldX      - World x of left edge
   * @param {number}       cfg.worldY      - World y of top edge
   * @param {number}       cfg.worldWidth
   * @param {number}       cfg.worldHeight
   */
  constructor({ cols, rows, u, v, worldX = 0, worldY = 0, worldWidth, worldHeight }) {
    Object.assign(this, { cols, rows, u, v, worldX, worldY, worldWidth, worldHeight });
  }

  /** Sample wind at world position (wx, wy). Returns {u, v}. */
  sample(wx, wy) {
    const fx = (wx - this.worldX) / this.worldWidth  * (this.cols - 1);
    const fy = (wy - this.worldY) / this.worldHeight * (this.rows - 1);
    const x0 = Math.max(0, Math.min(this.cols - 2, Math.floor(fx)));
    const y0 = Math.max(0, Math.min(this.rows - 2, Math.floor(fy)));
    const tx = fx - x0;
    const ty = fy - y0;

    const idx = (r, c) => r * this.cols + c;
    const u = (1-tx)*(1-ty)*this.u[idx(y0,x0)] + tx*(1-ty)*this.u[idx(y0,x0+1)]
            + (1-tx)*   ty *this.u[idx(y0+1,x0)] + tx*   ty *this.u[idx(y0+1,x0+1)];
    const v = (1-tx)*(1-ty)*this.v[idx(y0,x0)] + tx*(1-ty)*this.v[idx(y0,x0+1)]
            + (1-tx)*   ty *this.v[idx(y0+1,x0)] + tx*   ty *this.v[idx(y0+1,x0+1)];
    return { u, v };
  }

  /** Factory: synthetic vortex + uniform wind field for demos */
  static makeSynthetic({ cols = 32, rows = 32, worldWidth = 512, worldHeight = 512,
                          uniformU = 0.04, uniformV = 0.01, vortexStrength = 0.05 } = {}) {
    const n = cols * rows;
    const u = new Float32Array(n);
    const v = new Float32Array(n);
    const cx = cols / 2, cy = rows / 2;

    for (let r = 0; r < rows; r++) {
      for (let c = 0; c < cols; c++) {
        const dx = (c - cx) / cols;
        const dy = (r - cy) / rows;
        u[r * cols + c] = uniformU + (-dy) * vortexStrength;
        v[r * cols + c] = uniformV + ( dx) * vortexStrength;
      }
    }
    return new WindField({ cols, rows, u, v, worldWidth, worldHeight });
  }
}

// ---------------------------------------------------------------------------
// WindParticleSystem
// ---------------------------------------------------------------------------
class WindParticleSystem {
  /**
   * @param {object} cfg
   * @param {WindField} cfg.field        - Velocity field
   * @param {number}    cfg.worldWidth
   * @param {number}    cfg.worldHeight
   * @param {number}    cfg.count        - Number of particles
   * @param {number}    cfg.lifetime     - Particle lifetime (ms)
   * @param {number}    cfg.trailSteps   - Number of trail integration steps stored
   * @param {number}    cfg.stepMs       - Integration step size (ms)
   * @param {number}    cfg.speedScale   - Multiplier to convert field units to px/step
   */
  constructor({
    field,
    worldWidth  = 512,
    worldHeight = 512,
    count       = 200,
    lifetime    = 3000,
    trailSteps  = 20,
    stepMs      = 16,
    speedScale  = 1.0,
  }) {
    this.field       = field;
    this.worldWidth  = worldWidth;
    this.worldHeight = worldHeight;
    this.count       = count;
    this.lifetime    = lifetime;
    this.trailSteps  = trailSteps;
    this.stepMs      = stepMs;
    this.speedScale  = speedScale;
  }

  /**
   * Compute the complete trail for particle i at epoch time t.
   * Returns array of {x, y} in world coordinates, newest position first.
   *
   * Determinism: birth time is derived from particle id and repeats each `lifetime`.
   * Within its active window the particle integrates the wind field from its spawn point.
   */
  _trail(i, t) {
    // Phase each particle so spawns are spread across time
    const phaseOffset = fhash(i * 3 + 0) * this.lifetime;
    const age = ((t - phaseOffset) % this.lifetime + this.lifetime) % this.lifetime;

    // Spawn position (world space)
    const spawnX = fhash(i * 3 + 1) * this.worldWidth;
    const spawnY = fhash(i * 3 + 2) * this.worldHeight;

    // Integrate forward from spawn by `age` ms
    let x = spawnX;
    let y = spawnY;
    const steps = Math.ceil(age / this.stepMs);
    const dt    = this.stepMs * this.speedScale;

    const trail = [];
    for (let s = 0; s < steps; s++) {
      if (s >= steps - this.trailSteps) trail.unshift({ x, y });
      const { u, v } = this.field.sample(x, y);
      x = ((x + u * dt) % this.worldWidth  + this.worldWidth)  % this.worldWidth;
      y = ((y + v * dt) % this.worldHeight + this.worldHeight) % this.worldHeight;
    }
    trail.unshift({ x, y }); // head

    return trail;
  }

  /** Speed at head of particle trail, for colorization. */
  _speed(trail) {
    if (trail.length < 2) return 0;
    const dx = trail[0].x - trail[1].x;
    const dy = trail[0].y - trail[1].y;
    return Math.sqrt(dx*dx + dy*dy) / this.stepMs;
  }

  /** Map speed to colour hue (blue=slow, yellow=fast). */
  _color(speed, maxSpeed = 0.08) {
    const t = Math.min(1, speed / maxSpeed);
    // Interpolate: slow=blue(220°) → medium=cyan(180°) → fast=yellow(60°)
    const hue = 220 - t * 160;
    return `hsl(${hue.toFixed(0)},80%,65%)`;
  }

  draw(ctx, vp, t) {
    const { x: vx, y: vy, width: vw, height: vh } = vp;
    const margin = 4;

    ctx.save();
    ctx.lineWidth = 1.5;

    for (let i = 0; i < this.count; i++) {
      const trail = this._trail(i, t);
      if (trail.length < 2) continue;

      // Check if head is within viewport
      const { x: hx, y: hy } = trail[0];
      if (hx < vx - margin || hx > vx + vw + margin) continue;
      if (hy < vy - margin || hy > vy + vh + margin) continue;

      const spd   = this._speed(trail);
      const color = this._color(spd);

      for (let s = 1; s < trail.length; s++) {
        const alpha = (1 - s / trail.length) * 0.8;
        ctx.globalAlpha = alpha;
        ctx.strokeStyle = color;
        ctx.beginPath();
        ctx.moveTo(trail[s-1].x - vx, trail[s-1].y - vy);
        ctx.lineTo(trail[s].x   - vx, trail[s].y   - vy);
        ctx.stroke();
      }

      // Arrow head at trail[0]
      ctx.globalAlpha = 0.9;
      ctx.fillStyle = color;
      ctx.beginPath();
      ctx.arc(trail[0].x - vx, trail[0].y - vy, 2, 0, Math.PI * 2);
      ctx.fill();
    }

    ctx.globalAlpha = 1;
    ctx.restore();
  }
}

// Export for use as ES module or via <script> tag
if (typeof module !== 'undefined') {
  module.exports = { RainSystem, SnowSystem, WindField, WindParticleSystem };
}
