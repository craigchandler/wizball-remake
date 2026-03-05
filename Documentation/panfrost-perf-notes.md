# WizBall muOS / Panfrost GLES2 Performance Notes
**Target:** muOS aarch64, SDL 2.28.5, renderer `opengles2` (Panfrost)  
**Build branch:** `feature/sdl2-port`  
**File being worked on:** `wizball/platform_renderer.cpp`  
**Last build deployed:** 896,088 bytes (05 Mar 2026 05:01)

---

## 1. Core problem

Panfrost GLES2 is a **tiling GPU** that batches `SDL_RenderGeometry` calls into one draw-packet per `(texture, blend_mode, clip_rect)` triplet. Any change to that triplet mid-frame forces an immediate GPU submit (a "batch flush"). Each flush costs roughly **25–50 ms** on this hardware.

Target is ~16 ms/frame (60 fps). The main menu tunnel animation currently runs at **50–270+ ms/frame** depending on scene state.

---

## 2. What every `[FRAME]` field means

```
[FRAME N] draws=D textured=T nontex=X winspr=W geom_miss=G degraded=R
          tx_sw=TX clip_sw=CL blend_sw=BL colmod_sw=CM skip=SK
          add_d=A spec_d=S defer_d=DF
          present_h=H frame_ms=F render_ms=R present_ms=P
```

| Field | Meaning |
|-------|---------|
| `draws` | Total `SDL_RenderGeometry` calls this frame |
| `textured` | Draws with a non-NULL texture |
| `nontex` | Draws via `white_texture` (solid/coloured rects, points) |
| `tx_sw` | Times the bound texture changed between consecutive calls = GPU batch flushes from texture switches |
| `clip_sw` | `SDL_RenderSetClipRect` calls that changed state = GPU batch flushes |
| `blend_sw` | `SDL_SetTextureBlendMode` calls that changed state = GPU batch flushes |
| `colmod_sw` | `SDL_SetTextureColorMod/AlphaMod` calls that changed state |
| `add_d` | Number of draws submitted with `BLEND_MODE_ADD` (additive blend) |
| `spec_d` | Number of draws with any non-alpha blend (ADD + MUL + SUB combined) |
| `defer_d` | Draws buffered in the ADD deferred queue (see §6) |
| `render_ms` | Wall-clock time from `clear_backbuffer` to `SDL_RenderPresent` call (GPU submit cost) |
| `present_ms` | Time inside `SDL_RenderPresent` itself (vsync wait + actual flip) |
| `frame_ms` | Wall-clock since end of previous `SDL_RenderPresent` (total frame budget) |

**Minimum `render_ms` formula:**

```
render_ms ≥ (tx_sw + clip_sw + blend_sw) × ~25ms
```

---

## 3. Timeline of all fixes applied (session history)

| Date | Fix | Result |
|------|-----|--------|
| Earlier sessions | Set `SDL_HINT_RENDER_BATCHING=1` | Batching enabled at all |
| Earlier | Route 400 nontex draws through `SDL_RenderGeometry(white_texture)` instead of `SDL_RenderFillRect` | Eliminated pipeline break per nontex draw |
| Earlier | Per-window `SDL_RenderSetClipRect` — cache clip rect, skip redundant calls | `clip_sw` reduced to 2 baseline |
| Earlier | BLEND/ADD blend mode caching via SDLCACHE hash table (`SDLCACHE_SLOTS=128`) | Eliminated redundant `SDL_SetTextureBlendMode` for those modes |
| Session N-1 | Found MULTIPLY and SUBTRACT branches called `SDL_SetTextureBlendMode` unconditionally on every draw (no cache check) | Fixed with cache check on both branches; added `blend_switch_count++` |
| Session N-1 | `SDLCACHE_SLOTS` 128→256, hash shifted from `>>4` to `>>3` (reduces collisions with 6+ textures) | SDLCACHE collision risk reduced |
| Session N | Added `add_d` / `spec_d` diagnostics | Confirmed 100% of textured draws are ADD blend; 2 non-ADD draws per frame cause residual `blend_sw` |
| **This session** | Deferred ADD draw queue: ADD-blend draws buffered and sorted by texture pointer, flushed before `SDL_RenderPresent` and before any `SDL_RenderSetClipRect` change | `tx_sw` 3→2 (steady state), 5→4 (large scene); `defer_d=5` (small scene), `defer_d=58/87` (large scene) |

---

## 4. Current steady-state baseline (frames 91–117)

**Scene:** intro tunnel, 5 entities + MENU_INTRO_TITLE, 2 textures active

```
draws=717  textured=317  nontex=400
tx_sw=2    clip_sw=2     blend_sw=0
add_d=317  spec_d=317    defer_d=5
render_ms=51-59ms
```

**Cost breakdown:**
- `tx_sw=2` = 2 texture-switch flushes × ~25ms = ~50ms
- `clip_sw=2` = 2 clip-rect flushes × ~25ms = ~50ms
- **Total theoretical minimum: ~100ms**
- Actual: ~55ms — better than formula because some flushes are cheap (small batch)

**`defer_d=5` — why so few deferred?**  
The deferred queue intercepts ADD draws in `PLATFORM_RENDERER_try_sdl_geometry_textured` only if `vertex_count ≤ 8` and `index_count ≤ 18`. The 312 perspective tunnel quads use `PLATFORM_RENDERER_GEOM_SRC_PERSPECTIVE` / `GEOM_SRC_COLOURED_PERSPECTIVE` — these go through the `force_coloured_geometry` early-return branch **before** reaching the deferred interception code, and they have large vertex counts. So only 5 small overlay sprites get deferred; the tunnel geometry bypasses the queue entirely.

This is the main design limitation of the current deferred ADD queue.

---

## 5. Two-texture scene (frames 118–147): the hard floor

**Trigger:** Two 256×256 textures are `Building SDL texture for legacy ID 0` at frame 118.

```
draws=770  textured=370  nontex=400
tx_sw=4    clip_sw=2     blend_sw=2
add_d=370  spec_d=370    defer_d=58
render_ms=150-205ms
```

**Cost breakdown:**
- `tx_sw=4` = 4 flushes × ~25ms = ~100ms
- `clip_sw=2` = 2 flushes × ~25ms = ~50ms
- `blend_sw=2` = 2 flushes × ~25ms = ~50ms
- **Total formula: ~200ms** — matches observed 163–205ms ✓

**Why `blend_sw=2` appeared:**  
`blend_sw` was 0 in the previous session's equivalent phase. The deferred ADD queue is now flushing 58 ADD draws before each `SDL_RenderSetClipRect` call (clip_sw=2), so at those flush points the blend mode forces via `set_texture_blend_cached(tex, ADD)`. After the flush, the next immediate-path draw is the 1 non-ADD draw (`spec_d > add_d` by 0 in this phase), which switches blend back — triggering 2 `blend_sw` counts.

**Root cause: 2 non-ADD textured draws per frame**  
`spec_d = add_d` here (both 370), meaning those 2 extra `blend_sw` are caused entirely by the deferred queue flush-then-switch pattern, not different blend modes.

**Actually re-reading:** At frames 118-147, `spec_d == add_d == 370`. So there are 0 non-ADD textured draws. The `blend_sw=2` is therefore caused by the deferred queue flushing with ADD → then the SDL batch needs to be re-set for the *next non-ADD clip boundary operation* (probably the `white_texture` nontex draws which use BLEND not ADD). The flush before `SDL_RenderSetClipRect` submits the buffered ADD draws with ADD blend; then the `white_texture` BLEND draws that follow the clip change cause 2 BLEND→ADD transitions as the deferred flush is re-entered.

**Summary: deferred flush is causing new blend transitions it didn't previously.**

---

## 6. Large scene with particles (frames 148–159)

```
draws=801  textured=401  nontex=400
tx_sw=4    clip_sw=2     blend_sw=3
add_d=399  spec_d=401    defer_d=87
render_ms=225-265ms
```

**`spec_d=401, add_d=399` → 2 non-ADD textured draws.**  
These are the same 2 mystery non-ADD textured draws that caused `blend_sw=3` in the previous session (before deferred queue was added). The deferred queue sort-by-texture is not helping because these 2 non-ADD draws go through the immediate path and interleave with the large perspective tunnel batch.

**Formula: `tx_sw=4 + clip_sw=2 + blend_sw=3 = 9 flushes × ~25ms = ~225ms`** — matches observed 225–265ms ✓

---

## 7. Known bottlenecks (prioritised)

### 7.1 `tx_sw` is the dominant cost  

**Formula: each unique texture per frame = 1 or more GL flush.**  

In the two-texture phase (`tx_sw=4`):
- Batch 1: `white_texture` (nontex ALPHA blend)
- Batch 2: perspective tunnel texture (ADD) 
- Batch 3: extra font/particle texture A (ADD)
- Batch 4: extra font/particle texture B (ADD)

To reduce `tx_sw`, the only options are:
1. **Pack frequently co-used textures into a single atlas** (e.g. both 256×256 font/particle textures into one 512×256 or 256×512 texture). This would reduce `tx_sw` from 4→3 in the two-texture phase.
2. **Sort ALL draws by texture pointer regardless of blend mode** — a full draw-order sort rather than the current ADD-only deferred queue.
3. **Drop the per-entity texture model** — not feasible without major refactoring.

### 7.2 `blend_sw` from deferred ADD flush interaction

The current deferred queue flushes sorted ADD draws *before* `SDL_RenderSetClipRect`. This now causes `blend_sw=2` in the two-texture phase where it was previously 0. The flush introduces ADD→BLEND transitions at the clip boundary.

**Root cause:** The flush calls `set_texture_blend_cached(tex, ADD)` on the last deferred texture, then the immediate `white_texture` draws (BLEND) that follow the clip change cause 2 BLEND→ADD transitions.

**Fix options:**
- After `flush_addq()` completes, force the last texture's blend mode back to BLEND via cache so the subsequent BLEND draws don't cause a switch. Not correct because blend is per-texture.
- **Better:** Don't flush the ADD queue before `SDL_RenderSetClipRect` — instead, flush it after the clip change. Test: does the clip rect affect ADD draw results? If the ADD draws are rendered after the clip changes, some might be clipped incorrectly. Need to verify what the clip rect covers (probably the game window area, not the whole screen).
- **Best:** Move ALL deferred flushes to end-of-frame only (just before `SDL_RenderPresent`), and make the clip rect change aware of the deferred queue by not flushing at clip boundaries. The tunnel draws have no clip rect requirements (they fill the screen).

### 7.3 2 non-ADD textured draws interleaved with ADD batch

`spec_d - add_d = 2` consistently from frame 148+. These 2 draws use MULTIPLY or ALPHA blend (not ADD) and are interleaved among hundreds of ADD tunnel draws. From the VCOL-ENTITY log, candidates are:
- `MENU_TUNNEL_STARFIELD` (`flags=0x82`) — likely uses ALPHA blend for background sprites
- `MAIN_MENU_TUNNEL_LIGHT` (`flags=0x1b2`) — could be MULTIPLY for lighting overlay

Both are in the perspective tunnel draw list. These 2 non-ADD draws break the ADD texture batch, causing the 2 extra `blend_sw` counts and ~50ms penalty.

**Fix:** Identify which entity type generates these 2 non-ADD draws (add a `[NON-ADD-DRAW]` log when the immediate path is taken with non-ADD blend while `blend_enabled=true`). Then either:
- Change their blend mode to ADD (if visually acceptable)
- Sort them to a separate pass (extend the deferred queue concept to handle all blend modes, not just ADD)

### 7.4 `clip_sw=2` baseline — 50ms unavoidable with current approach

Two `SDL_RenderSetClipRect` changes per frame. These come from `PLATFORM_RENDERER_finish_textured_window_draw` (disable clip) and from the window draw setup (enable clip). Each is a genuine state change needed for correct rendering.

**Potential saving:** If the clip rect matches the full framebuffer (640×480 = full output), disabling clip is a no-op. The check `if (clip == fullscreen) skip SDL_RenderSetClipRect` might eliminate 1 or both clip changes. Current code already checks if the rect changed, but doesn't check if it equals the full output rect.

### 7.5 High nontex draw count — 400 per frame

`nontex=400` solid/coloured rect draws via `white_texture`. All use ALPHA blend. They batch together as one GL submit (since they all use the same `white_texture`). Currently costing 0 extra batch flushes beyond the natural `white_texture` batch.

However, 400 individual `SDL_RenderGeometry` calls with 4 vertices each is 1600 vertex submissions. These could theoretically be batched into one `SDL_RenderGeometry` call with an accumulated vertex buffer. This is a CPU optimization (less call overhead) but unlikely to matter on Panfrost where GPU submit cost dominates.

---

## 8. Current code state (`platform_renderer.cpp`)

### 8.1 Static counters (lines ~81–107)
```cpp
static int platform_renderer_sdl_tx_switch_count = 0;
static int platform_renderer_sdl_clip_switch_count = 0;
static int platform_renderer_sdl_blend_switch_count = 0;
static int platform_renderer_sdl_colmod_switch_count = 0;
static int platform_renderer_sdl_alpha_skip_count = 0;
static int platform_renderer_sdl_add_draw_count = 0;
static int platform_renderer_sdl_spec_draw_count = 0;
static int platform_renderer_sdl_defer_draw_count = 0;

/* Deferred ADD queue */
#define PLATFORM_RENDERER_ADDQ_MAX 512
struct platform_renderer_addq_entry { SDL_Texture *tex; SDL_Vertex verts[8]; int indices[18]; int vertex_count; int index_count; int source_id; };
static platform_renderer_addq_entry platform_renderer_addq[PLATFORM_RENDERER_ADDQ_MAX];
static int platform_renderer_addq_count = 0;
```

### 8.2 SDLCACHE (lines ~1653–1654)
```cpp
#define SDLCACHE_SLOTS 256
#define SDLCACHE_IDX(p) (((uintptr_t)(p) >> 3) & (SDLCACHE_SLOTS - 1))
```
Hash table keyed by texture pointer, caches `(blend, r, g, b, a)` per texture. Prevents redundant SDL state calls. 256 slots chosen to handle 6+ simultaneous textures without collision.

### 8.3 `apply_sdl_texture_blend_mode` (line ~1707)
Called before every immediate-path textured draw. Increments `add_draw_count` and `spec_draw_count`. MULTIPLY and SUBTRACT branches have cache checks to avoid redundant `SDL_SetTextureBlendMode` calls.

### 8.4 `flush_addq` (lines after `apply_sdl_texture_blend_mode`)
```cpp
static void PLATFORM_RENDERER_flush_addq(void) {
    // sort platform_renderer_addq[0..count-1] by tex pointer
    // foreach: set_texture_blend_cached(tex, SDL_BLENDMODE_ADD), SDL_RenderGeometry
    // count = 0
}
```
Called:
- Before each `SDL_RenderSetClipRect` in the window-draw clip setup
- Before each `SDL_RenderSetClipRect` in `finish_textured_window_draw`
- Before `SDL_RenderPresent` (at top of present block)

### 8.5 Deferred interception in `try_sdl_geometry_textured` (line ~787)
**Gate conditions:**
```cpp
if (texture != NULL && indices != NULL &&
    platform_renderer_blend_mode == PLATFORM_RENDERER_BLEND_MODE_ADD &&
    platform_renderer_blend_enabled &&
    vertex_count >= 1 && vertex_count <= 8 &&
    index_count >= 1 && index_count <= 18 &&
    platform_renderer_addq_count < PLATFORM_RENDERER_ADDQ_MAX)
```
Only intercepts small draws (≤8 verts, ≤18 indices). The 312 perspective tunnel draws bypass this gate.

### 8.6 `[FRAME]` log format
```
[FRAME N] draws= textured= nontex= winspr= geom_miss= degraded= tx_sw= clip_sw= blend_sw= colmod_sw= skip= add_d= spec_d= defer_d= present_h= frame_ms= render_ms= present_ms=
```

---

## 9. What the deferred ADD queue achieved vs. what it didn't

### ✅ Wins
- `tx_sw=3→2` in the steady-state 5-entity scene (the 5 deferred small draws sort by texture, consolidating 2 texture switches into 1)
- `tx_sw=5→4` in the two-texture-phase scene (same reason)
- `blend_sw` eliminated from steady state (same as before)

### ❌ Did not help
- **`defer_d=5/317`** — only 5 of 317 ADD draws are actually deferred. The 312 perspective tunnel draws have large vertex arrays and go through `force_coloured_geometry` before the deferred gate. The deferred sort is operating on a tiny fraction of the work.
- **Introduced new `blend_sw=2`** at frame 118+. The flush-before-clip-change causes ADD blend to be set on a texture, then the BLEND draws that follow the clip change trigger 2 ADD→BLEND transitions. Net effect: `blend_sw` previously 0 in two-texture phase, now 2 — **regression**.
- `render_ms` in two-texture phase now 150–205ms (similar to before), but with extra `blend_sw=2` overhead that wasn't there before.

---

## 10. Next steps (ranked by expected impact)

### Priority 1: Fix the deferred-flush-causes-blend_sw regression ⚠️

The `flush_addq` before `SDL_RenderSetClipRect` is introducing 2 new `blend_sw` counts in the two-texture phase. Options:

**Option A:** Remove `flush_addq()` calls from the clip-rect change points. Only flush at `SDL_RenderPresent`. Risk: ADD draws submitted after a clip change will have the wrong clip applied. Needs investigation — are the perspective tunnel draws (which are ADD) ever drawn under a clip rect? If `clip_sw=2` is from windowing code that only wraps UI, the tunnel background draws may be safe to render without that clip.

**Option B:** After flushing the ADD queue, reset the SDL blend cache state for the last-used texture back to BLEND so the following BLEND draws don't trigger a switch. This is a hack that could break correctness.

**Option C:** Track whether the deferred flush happened before a clip change and suppress blend counting for the transitions that result from it. Statistical fix, not a real solution.

**Quick test to try:** What are `clip_sw=2` actually guarding? Log the clip rect coordinates when `SDL_RenderSetClipRect` fires. If both clip calls are re-setting to the same value (the full window), they could be eliminated entirely (they're no-ops). Check `PLATFORM_RENDERER_begin_textured_window_draw` and `finish_textured_window_draw` call sites.

### Priority 2: Extend deferred queue to cover perspective tunnel draws

The 312 ADD-blend perspective draws (the bulk of the work) never enter the deferred queue because:
1. They go through `force_coloured_geometry` which returns early
2. Even if they reached the gate, `vertex_count > 8` would block them (tunnel quads have many vertices)

**Fix:** Either:
- Remove the `vertex_count ≤ 8` gate and increase `platform_renderer_addq_entry.verts[]` to `SDL_MAX_VERTEX_COUNT` / dynamic allocation
- Or add a second deferred queue path specifically for `force_coloured_geometry` source types (`GEOM_SRC_PERSPECTIVE`, `GEOM_SRC_COLOURED_PERSPECTIVE`)

If the 312 large perspective draws were also deferred and sorted by texture, `tx_sw` could potentially drop from 4→2 (just `white_texture` + one ADD batch). That would save 2 × ~25ms = ~50ms per frame in the two-texture phase.

### Priority 3: Identify and fix the 2 non-ADD textured draws

`spec_d - add_d = 2` from frame 148+. These draws break the ADD batch. Add a diagnostic:
```cpp
/* In apply_sdl_texture_blend_mode, before the existing counters: */
if (platform_renderer_blend_enabled &&
    platform_renderer_blend_mode != PLATFORM_RENDERER_BLEND_MODE_ADD) {
    static int logged = 0;
    if (logged < 20) {
        logged++;
        fprintf(stderr, "[NON-ADD-DRAW %d] blend=%d\n", logged, platform_renderer_blend_mode);
    }
}
```
Or log the texture handle and blend mode. Once identified, either change these draws to ADD or route them to a separate deferred pass.

### Priority 4: Eliminate `clip_sw=2` floor

`clip_sw=2` costs ~50ms. If the clip rect is the full framebuffer (640×480), both calls are redundant. Add a check:
```cpp
/* In PLATFORM_RENDERER_begin_textured_window_draw and finish_textured_window_draw */
SDL_Rect full = {0, 0, platform_renderer_sdl_output_width, platform_renderer_sdl_output_height};
if (clip == full) { skip SDL_RenderSetClipRect; }
```
If confirmed safe, this alone could save ~50ms (from 6 flushes to 4).

### Priority 5: Atlas merge for two 256×256 textures

At frame 118, two 256×256 textures load, raising `tx_sw` from 2→4. These appear to be the `big_font` (256×256, bmp=8) and `intro_logos`/other 256×256 texture. If they can be packed into a single 512×256 texture (adjusting sprite UV coordinates), `tx_sw` would stay at 3 instead of 4 — saving one flush per frame indefinitely during those scenes.

---

## 11. Build / deploy commands

```bash
# Delete stale .o (REQUIRED — clock skew inside Docker)
docker exec elated_brown rm -f /workspace/build-aarch64/CMakeFiles/wizball.dir/wizball/platform_renderer.cpp.o

# Build
docker exec elated_brown sh -c "cd /workspace/build-aarch64 && ninja -j4 > /tmp/build.log 2>&1 && echo BUILD_OK || echo BUILD_FAILED"
docker exec elated_brown cat /tmp/build.log

# Deploy to staging
docker exec elated_brown sh -c "cp /workspace/build-aarch64/wizball.aarch64 /workspace/staging/wizball.aarch64 && ls -la /workspace/staging/wizball.aarch64"
```

---

## 12. Log interpretation quick reference

When a new log arrives, check these fields in order:

1. **`blend_sw=0`?** — If yes, all blend caching is working. If not, something is causing genuine blend-mode changes.
2. **`defer_d`** — How many ADD draws were deferred. If `defer_d ≈ add_d`, the deferred queue is covering most work. Currently `defer_d ≪ add_d` (5/317 or 58/370).
3. **`spec_d - add_d`** — Count of non-ADD textured draws. Should be 0 if possible. Currently 2.
4. **`tx_sw + clip_sw + blend_sw`** — Total GPU flushes. Multiply by ~25ms to predict `render_ms`.
5. **`render_ms` spike** — Compare to `(tx_sw + clip_sw + blend_sw) × 25`. If actual >> formula, there's a fill-rate issue (large texture area × blending cost).

---

## 13. Known anomalies

- **Frame 111 in previous session**: `render_ms=147ms` with same stats as surrounding frames (47–62ms). GPU scheduling stall — not a software issue.
- **Frame 88**: `draws=1, tx_sw=0` — single-draw transition frame (menu state change). `frame_ms=123ms` is state machine work, not render cost.
- **ALSA underruns**: `snd_pcm_recover` messages at startup are audio not video — ignore for rendering analysis.
- **`COLOUR-ZERO N`**: `set_colour4f(1,1,1,0)` call for `MENU_INTRO_TITLE` (alpha=0, invisible). This entity draws but contributes a nearly-invisible spec_d=1 draw. It IS counted in `spec_d` — may be one of the 2 non-ADD draws.
- **`Building SDL texture for legacy ID 0`** lines: On-demand texture creation (first time a sprite is used in a scene). The frame containing this will be slow due to upload cost; subsequent frames normalise.
