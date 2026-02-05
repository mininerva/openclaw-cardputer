# OpenClaw Cardputer - Avatar Integration Blockers

## Status: BLOCKED - Requires Refactoring

The procedural avatar system exists in `include/avatar/` and `src/avatar/` but cannot be enabled due to deep architectural incompatibilities with the reimplemented codebase.

---

## Blocker 1: Keyboard System Mismatch

**Files Affected:**
- `include/avatar/ancient_ritual.h`
- `include/keyboard_input.h` (old) vs `include/keyboard_handler.h` (new)

**Issue:**
The avatar code imports `keyboard_input.h` which defines:
```cpp
enum class SpecialKey : uint8_t {
    FUNCTION_1,  // Old naming
    FUNCTION_2,
    // ...
};
```

The new `keyboard_handler.h` defines:
```cpp
enum class SpecialKey : uint8_t {
    FN,      // New naming
    CTRL,
    OPT,
    // ...
};
```

**Impact:**
- `ancient_ritual.h` references `SpecialKey::FUNCTION_1` and `FUNCTION_2` for Konami code
- These don't exist in the new keyboard system
- Ancient mode trigger system won't compile

**Fix Required:**
Update `ancient_ritual.h` to use new SpecialKey names or add aliases.

---

## Blocker 2: Duplicate Type Definitions

**Files Affected:**
- `include/keyboard_input.h` (old avatar dependency)
- `include/keyboard_handler.h` (new system)

**Issue:**
Both files define in the `OpenClaw` namespace:
- `enum class SpecialKey`
- `struct KeyEvent`
- `struct InputBuffer`

When `ancient_ritual.h` includes `keyboard_input.h` and `main.cpp` includes `keyboard_handler.h`, we get:
```
error: multiple definition of 'enum class OpenClaw::SpecialKey'
error: redefinition of 'struct OpenClaw::KeyEvent'
```

**Fix Required:**
Either:
1. Remove `keyboard_input.h` entirely and update all avatar code to use `keyboard_handler.h`
2. Merge the two keyboard systems
3. Rename one set of types to avoid collision

---

## Blocker 3: Vec2 constexpr Constructor

**Files Affected:**
- `include/avatar/geometry.h`
- `include/avatar/procedural_avatar.h`

**Issue:**
`procedural_avatar.h` uses:
```cpp
constexpr Vec2 LEFT_EYE_POS(-28, -10);
```

But `Vec2` was defined without constexpr constructors:
```cpp
struct Vec2 {
    Vec2() : x(0), y(0) {}           // Not constexpr
    Vec2(float x, float y) : x(x), y(y) {}  // Not constexpr
};
```

**Status:** ✅ FIXED - Added constexpr to constructors

---

## Blocker 4: Missing ProceduralAvatar Methods

**Files Affected:**
- `include/avatar/procedural_avatar.h`
- `src/avatar/voice_synthesis.cpp`

**Issue:**
`voice_synthesis.cpp` calls:
```cpp
if (g_avatar.isReady()) { ... }
```

But `ProceduralAvatar` had no `isReady()` method.

**Status:** ✅ FIXED - Added `isReady() const { return initialized_; }`

---

## Blocker 5: AncientDialect Include

**Files Affected:**
- `src/avatar/voice_synthesis.cpp`

**Issue:**
Uses `AncientDialect::toAncientSpeak()` but doesn't include header.

**Status:** ✅ FIXED - Added `#include "avatar/ancient_ritual.h"`

---

## Remaining Work to Enable Avatar

### Phase 1: Keyboard System Unification (30 mins)
1. Decide: Merge systems or update avatar to use new handler
2. If merging: Port Konami code trigger from ancient_ritual.h to keyboard_handler
3. If updating: Change all `keyboard_input.h` includes in avatar to `keyboard_handler.h`
4. Update SpecialKey references (FUNCTION_1 → FN, etc.)

### Phase 2: Rendering Integration (20 mins)
1. Check how avatar renders to display
2. Old code likely used direct M5Canvas manipulation
3. New system uses `display_renderer.h` with canvas abstraction
4. May need to pass canvas pointer to avatar or use global

### Phase 3: Main Loop Integration (15 mins)
1. Add avatar update/render calls to main.cpp loop
2. Connect avatar mood to app state machine
3. Wire up voice synthesis to audio streamer events

### Phase 4: Testing (15 mins)
1. Verify all 9 moods render correctly
2. Test blink, breath, ruffle animations
3. Test ancient mode overlay
4. Test error/glitch effects

**Total Estimate:** 80 minutes

---

## Avatar Features Available (Once Enabled)

### Visual Components
- Procedural owl face with animated eyes
- Beak with syllable-based lip-sync
- Feather ruffling based on activity level
- Ear tufts with idle animation
- Eyebrows for expression

### Mood States (9 total)
1. IDLE - Default calm state
2. LISTENING - User is typing/speaking
3. THINKING - AI processing
4. SPEAKING - AI responding
5. EXCITED - High activity
6. JUDGING - Critical response
7. ERROR - Something went wrong
8. TOOL_USE - Using external tool
9. ANCIENT_MODE - Sepia + runes + amber glow

### Animations
- Blink controller (single, double, slow, flutter)
- Breath controller (subtle scale pulsing)
- Ruffle controller (feather noise movement)
- Beak controller (speaking lip-sync)
- Pupil tracking (follows input source)

### Easter Eggs
- Ancient mode activation (gestures, voice, konami code)
- Rune overlay animation
- Error glitch effects
- Haptic feedback patterns

---

## Recommendation

**Short term:** Disable avatar, ship with text + audio only
**Medium term:** Add avatar in v2.1 after keyboard refactoring

The avatar is ~800 lines of sophisticated procedural graphics code. It deserves proper integration time, not a rushed fix.

---

## Current Build Status

| Component | Status | Notes |
|-----------|--------|-------|
| Text/Keyboard | ✅ Working | Full implementation |
| Audio Capture | ✅ Working | I2S + VAD implemented |
| WebSocket | ✅ Working | Binary protocol |
| Avatar | ❌ Disabled | Blocked by keyboard system |
| Settings Menu | ✅ Working | On-device config |

**Firmware:** Builds successfully without avatar
**RAM:** 19.7% (64KB / 328KB)
**Flash:** 37.5% (1.18MB / 3.1MB)
