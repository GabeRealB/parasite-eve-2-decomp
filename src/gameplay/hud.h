#ifndef GAMEPLAY_PRIVATE_HUD_H
#define GAMEPLAY_PRIVATE_HUD_H

#include "common.h"

struct Enemy;

/// Placement of the locked-on enemy's HP readout, kept between frames.
///
/// The readout is anchored at the right of the screen: near the top, or lower,
/// clear of the radar, when the player has one. A newly locked enemy's readout
/// is drawn at the anchor. While the same enemy stays locked, each frame moves
/// it one eighth of the remaining distance, rounded down, from the stored
/// position toward the anchor, so it slides when the anchor moves. A zero-filled
/// record is the valid initial state.
typedef struct {
    struct Enemy* enemy; // Enemy shown on the last drawn frame. Only compared, never dereferenced; `NULL` before the first
    s16           x;     // Horizontal position as last drawn, pixels from the screen center
    s16           y;     // Vertical position as last drawn, pixels from the screen center, increasing downward
} HudTargetHpReadout;
STATIC_ASSERT_SIZEOF(HudTargetHpReadout, 8);

/// Seven `u16` masks tested against `PlayerStatus.statusFlags` by the party HP/MP
/// HUD (`func_800A57B0`); each set bit draws one 14x14 status icon.
typedef struct GpHudStatusBits {
    u16 bits[7];
} GpHudStatusBits;
STATIC_ASSERT_SIZEOF(GpHudStatusBits, 0xE);

/// Displayed player HP and MP.
///
/// Signed widened copies of `PlayerStatus.hp` and `PlayerStatus.mp`, so a
/// negative hit-point total stays negative. While the in-game HUD is drawn,
/// each copy steps one point toward the live stat, either way. Menu drawings
/// of the HP/MP block step a copy upward only. The live stat is copied in
/// whole on a HUD reset, after equipment recalculates the maxima, when a
/// heal or boost panel or the menu HP/MP display opens, when an attachment
/// heal spends MP, when a Parasite Energy level fills MP, and when the heal
/// panel closes. HUD bars and the menu numbers and bars read the copies; the
/// HUD's numeric labels read the live stats.
typedef struct {
    s32 hp; // Displayed hit points, in the same points as `PlayerStatus.hp`
    s32 mp; // Displayed Parasite Energy, in the same points as `PlayerStatus.mp`
} HudHpMp;
STATIC_ASSERT_SIZEOF(HudHpMp, 0x8);

/// `HudState.battleStep`: how far a battle has got between its start and its results.
enum {
    HUD_BATTLE_STEP_START           = 0, // Requests the battle-start transition, which moves the step on itself
    HUD_BATTLE_STEP_FIGHT           = 1, // Fighting, until the end-of-battle delay has nearly run out
    HUD_BATTLE_STEP_END_ACTION      = 2, // Starts the player actor's end-of-battle action
    HUD_BATTLE_STEP_WAIT_END_ACTION = 3, // Waits for that action, then returns the player actor to normal control
    HUD_BATTLE_STEP_RESULTS         = 4, // Requests the results screen, which clears the step and `inBattle`
};

/// `HudState.suppression`: how much of the HUD one frame leaves out.
enum {
    HUD_SUPPRESS_NONE            = 0,    // Everything is drawn and updated
    HUD_SUPPRESS_PARASITE_ENERGY = 0x10, // HP/MP block and radar only: no target HP readout, no Parasite Energy update
    HUD_SUPPRESS_ALL             = 0x20, // Nothing is drawn or updated
};

/// `HudState.radarRangeIcon`: shape the radar overlays for the reach of the selected ability.
enum {
    HUD_RADAR_RANGE_NONE       = -1, // No overlay
    HUD_RADAR_RANGE_AROUND     = 2,  // Area centered on the player; also shown when every enemy is reached
    HUD_RADAR_RANGE_AHEAD      = 3,  // Area centered one radius ahead of the player
    HUD_RADAR_RANGE_PROJECTILE = 4,  // Projectile path
};

/// State the in-game HUD keeps from frame to frame.
///
/// One record exists while gameplay runs, inside the work block of the task
/// that drives the HUD, and it starts zero-filled apart from the radar overlay.
/// Besides what the HUD draws, it carries the battle sequence the HUD update
/// steps through: `inBattle` is raised when combat engages and `battleStep`
/// then walks `HUD_BATTLE_STEP_*`. The battle-start transition and the results
/// screen are separate tasks handed this record, and each moves the sequence
/// on when it starts.
///
/// `suppression` is rebuilt every frame: it is cleared, then raised by whatever
/// that frame should hide the HUD for, such as a menu request or a change of
/// battle step. `previewCastCost` and the radar overlay are likewise written
/// each frame by the Parasite Energy update, which runs after the drawing, so
/// the next frame's drawing shows them. The cost is in Parasite Energy points
/// and is drawn as a pending spend on the MP bar, or doubled on the HP bar
/// under Berserker.
///
/// `wheelTurn` animates the ability wheel: a step left or right starts it four
/// quarter slots away from rest, it moves one quarter back each frame, and the
/// wheel accepts a step or a confirmation only at rest.
typedef struct {
    s32                inBattle;        // 0 outside battle, 1 from the frame combat engages until the results screen opens
    s32                battleStep;      // `HUD_BATTLE_STEP_*`; meaningful while `inBattle` is 1
    s32                field_8;         // Cleared when the battle-start transition is requested and never read; role unproven
    byte               field_C;         // No access observed; role unproven
    s8                 suppression;     // `HUD_SUPPRESS_*` for the current frame
    s8                 field_E;         // Cleared by every Parasite Energy update and never read; role unproven
    s32                previewCastCost; // Cost of the ability under the wheel cursor; 0 while the wheel is closed
    u8                 queuedMenuMode;  // Menu request for `DisplayState.pendingMode`, handed over on the next frame; 0 none
    s8                 wheelTurn;       // Turn the ability wheel has left to make, in quarter slots, signed by direction; 0 at rest
    s8                 radarRangeIcon;  // `HUD_RADAR_RANGE_*`; reset to none once the radar has drawn it
    s16                radarRange;      // Reach shown with the icon, in world units; 0x3FFF when every enemy is reached
    HudTargetHpReadout targetHpReadout; // Placement of the locked-on enemy's HP readout
} HudState;
STATIC_ASSERT_SIZEOF(HudState, 0x24);

#endif // GAMEPLAY_PRIVATE_HUD_H
