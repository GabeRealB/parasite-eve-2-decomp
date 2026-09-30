#ifndef GAMEPLAY_SCENE_H
#define GAMEPLAY_SCENE_H

#include "common.h"

#include "gameplay/enemy.h"

/// Control values shared by ordinary and parasite-energy effects.
///
/// Scene actor modes are augmented with cancellation flags. Consumers use the
/// running, hidden, and cancellation thresholds according to their own policy.
/// Pending cancellation bits are published for one update, then cleared.
enum {
    ROOM_EFFECT_CONTROL_RUNNING    = 0,
    ROOM_EFFECT_CONTROL_PAUSED     = 1,
    ROOM_EFFECT_CONTROL_HIDDEN     = 2,
    ROOM_EFFECT_CONTROL_CANCEL_MIN = 4,
    ROOM_EFFECT_CANCEL_PE          = 0x80,
    ROOM_EFFECT_CANCEL_ALL         = 0x100,
};

/// Ground-shadow, room-effect view, and battle settings.
enum {
    ROOM_EFFECT_GROUND_SHADOW_DISABLED    = -1,
    ROOM_EFFECT_GROUND_SHADOW_UNMODULATED = 0,
    ROOM_EFFECT_GROUND_SHADOW_MAX_SHADE   = 0xFF,
    ROOM_EFFECT_VIEW_DISABLED             = 0,
    ROOM_EFFECT_VIEW_ENABLED              = 2,
    ROOM_EFFECT_BATTLE_ENGAGED            = 1,
};

/// Full-screen effect state and PE visual-effect claims.
enum {
    ROOM_EFFECT_SCREEN_FADE_QUAD    = 1,
    ROOM_EFFECT_SCREEN_BURST_GUARD  = 0x80,
    ROOM_EFFECT_PE_ANTIBODY_AURA    = 0x200,
    ROOM_EFFECT_PE_ENERGY_SHOT_AURA = 0x400,
    ROOM_EFFECT_PE_STATUS_BURST     = 0x800,
};

/// Room-owned counts, drawing controls, and cancellation state for gameplay effects.
///
/// Allocated on the primary heap as the room effect controller's `Task::work`;
/// consumers borrow it through `gRoomEffectState` until that task is destroyed.
/// Each update snapshots the scene actor and battle modes and consumes pending
/// cancellations. Rooms supply the ground and view settings; effect tasks
/// maintain the counts and visual-effect claims. The two control fields include
/// all-effect cancellation, while only `peEffectControl` includes PE cancellation.
typedef struct {
    s16 effectCount;           // Live counted effect allocations; ordinary spawns stop at 129, forced spawns bypass the limit.
    s16 rumbleCount;           // Collapse effects sharing the rumble sound; clamped to zero after releases/cancellation.
    s16 effectControl;         // Scene actor mode (0 run, 1 pause, 2 hide) plus 0x100 cancellation; values >= 4 cancel.
    s16 groundTraceEnabled;    // Ground raycasts for effect placement/collision (0 disabled, 1 enabled).
    s16 groundShadowShade;     // Shadow vertex shade (-1 disabled, 0 unmodulated texture, 1..255 grayscale).
    s16 roomEffectMode;        // Current view's ambient-effect gate (2 enabled, other values disabled).
    s16 lastAnimationSoundCue; // Retained player animation sound cue (0 none, 1 cue 2, 2 cue 1); no known consumer.
    s16 peEffectControl;       // Scene actor mode plus 0x80/0x100 cancellation, published for PE effects.
    s16 screenFxFlags;         // Full-screen state (1 fade quad active, 0x80 burst guard queried/cleared; no known setter).
    s16 peFxFlags;             // Visual claims (0x200 antibody aura, 0x400 energy-shot aura, 0x800 PE status burst).
    s16 burstRequest;          // Opening-flourish request (0 none, 1 pending), latched until a consumer clears it.
    s16 battleState;           // Battle phase (0 idle, 1 engaged, 2 ended, 3 resumed after battle).
    s16 peFadeMask;            // PE status mask selecting the current fade wave (0 none; single bits 1 through 0x80).
    u16 pendingCancelFlags;    // Cancellations awaiting the next update (0x80 PE only, 0x100 all effects).
} RoomEffectState;
STATIC_ASSERT_SIZEOF(RoomEffectState, 0x1C);

/// The enemy a lock-on list entry belongs to: every `WorldTargetNode` on that list
/// is the `node` member of an enemy.
#define GP_NODE_ENEMY(n) PARENT_OF(n, Enemy, node)

#endif // GAMEPLAY_SCENE_H
