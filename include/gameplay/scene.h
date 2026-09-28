#ifndef GAMEPLAY_SCENE_H
#define GAMEPLAY_SCENE_H

#include "common.h"

#include "gameplay/enemy.h"

/// State the effect system shares with everything that draws effects.
///
/// `Gp_InitState1C` allocates it at room load and publishes it as `Gp_State1C`
/// together with the task that owns it. The effect, weapon and parasite-energy
/// overlays reach it through that pointer, so it carries what they cannot work
/// out for themselves: how much of the effect budget is left, and what the
/// player is doing.
///
/// `Gp_TickState1C` refreshes the player-state and pulse fields once a frame,
/// the current room publishes the ground and room-effect fields from its own
/// draw callback, and the effect tasks claim and release bits of
/// `screenFxFlags` and `peFxFlags` among themselves - which is how a later
/// cast cancels an earlier one.
typedef struct {
    s16 effectCount;    // Live effects; `Gp_SpawnEff` refuses to spawn once this reaches 0x81, and every release decrements it
    s16 rumbleCount;    // Live collapse rumbles; their sound plays while this is non-zero
    s16 eventState;     // Player's event state (0 free, 1-3 in an event, 4 or more aborts), with this frame's abort pulse folded in
    s16 groundTrace;    // (0 suppressed, 1 default) whether effects may raycast to the floor to mark it or to stop at it
    s16 groundShade;    // Ground-shadow shade row for the current view (-1 draws none)
    s16 roomEffectMode; // (0 nothing, 2 the room's effect set is live) which of the room's effects the current view draws
    s16 field_C;        // Written by the player's hit-sound path and read by nothing; role unproven
    s16 fadeState;      // `eventState` with the room fade and PE pulses folded in, as the parasite-energy overlays read it
    s16 screenFxFlags;  // (1 the full-screen fade quad is up, 0x80 the burst effect is up)
    s16 peFxFlags;      // (0x200 ring burst, 0x400 burst rings, 0x800 green burst) claimed by the PE attack effects
    s16 burstRequest;   // Set for one frame when a burst starts, so the running burst draws its opening flourish
    s16 battleState;    // Mirror of `Gp_StateF0`'s battle state (1 while a battle is engaged)
    s16 peFadeId;       // The PE whose fade wave is running; a wave for another PE drops its task
    u16 pendingPulses;  // (0x80 room fade, 0x100 event script) abort pulses raised since the last refresh
} GpState1C;
STATIC_ASSERT_SIZEOF(GpState1C, 0x1C);

/// The enemy a lock-on list entry belongs to: every `GpLinkNode` on that list
/// is the `node` member of an enemy.
#define GP_NODE_ENEMY(n) PARENT_OF(n, GpEnemy, node)

#endif // GAMEPLAY_SCENE_H
