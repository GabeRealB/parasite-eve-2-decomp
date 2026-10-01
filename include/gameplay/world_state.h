#ifndef GAMEPLAY_WORLD_STATE_H
#define GAMEPLAY_WORLD_STATE_H

#include "common.h"

/// Scene battle phases, stored in one byte.
enum {
    SCENE_COMBAT_BATTLE_IDLE      = 0,
    SCENE_COMBAT_BATTLE_ENGAGED   = 1,
    SCENE_COMBAT_BATTLE_FINISHED  = 2,
    SCENE_COMBAT_BATTLE_RESUMED   = 3,
    SCENE_COMBAT_END_DELAY_FRAMES = 60
};

/// Actor update and visibility control shared with room effects.
enum {
    SCENE_COMBAT_ACTORS_RUNNING = 0,
    SCENE_COMBAT_ACTORS_PAUSED  = 1,
    SCENE_COMBAT_ACTORS_HIDDEN  = 2
};

/// Action stimuli accumulated until the next player update.
///
/// Cast bit 8 covers PE codes 300..600; bit 4 covers the other codes. Packed
/// masks keep word-wide loads: NOISE_OR_OTHER_CAST tests action bits 1/4,
/// CAST_FOOTSTEP_OR_ALERT tests action bits 4/16 and the entire enemy-alert byte.
enum {
    SCENE_COMBAT_ACTION_NOISE                  = 0x01,
    SCENE_COMBAT_ACTION_PE_ACTIVE              = 0x02,
    SCENE_COMBAT_ACTION_PE_CAST_OTHER          = 0x04,
    SCENE_COMBAT_ACTION_PE_CAST_300_TO_600     = 0x08,
    SCENE_COMBAT_ACTION_PE_CAST_MASK           = 0x0C,
    SCENE_COMBAT_ACTION_FOOTSTEP               = 0x10,
    SCENE_COMBAT_ACTION_ATTACK_MASK            = 0x0D,
    SCENE_COMBAT_SIGNAL_ATTACK_MASK            = 0x000D0000,
    SCENE_COMBAT_SIGNAL_NOISE_OR_OTHER_CAST    = 0x00050000,
    SCENE_COMBAT_SIGNAL_CAST_FOOTSTEP_OR_ALERT = 0xFF140000U
};

/// Shared ownership of a Mad Chaser alert: low nibble is the enemy placement index.
enum {
    SCENE_COMBAT_MAD_CHASER_OWNER_MASK    = 0x0F,
    SCENE_COMBAT_MAD_CHASER_ALERT_CLAIMED = 0x80
};

/// Actor 03700 group alert and player-release signals.
enum {
    SCENE_COMBAT_ACTOR03700_ALERT          = 0x01,
    SCENE_COMBAT_ACTOR03700_PLAYER_RELEASE = 0x02
};

/// Handshake between actor 105100 and actor 205200.
enum {
    SCENE_COMBAT_PAIRED_CHARGE_REQUEST = 0x01,
    SCENE_COMBAT_PAIRED_HEAL_REQUEST   = 0x02,
    SCENE_COMBAT_PAIRED_HEAL_READY     = 0x04,
    SCENE_COMBAT_PAIRED_RESET_REQUEST  = 0x08
};

/// Room-script phases for the shrine enemy entrance.
enum {
    SCENE_COMBAT_SHRINE_HIDDEN   = 0,
    SCENE_COMBAT_SHRINE_REVEALED = 1,
    SCENE_COMBAT_SHRINE_RELEASED = 2
};

/// Group activation phases of the Zebra Stalkers (actor_400600).
enum {
    SCENE_COMBAT_ZEBRA_STALKER_WAITING = 0,
    SCENE_COMBAT_ZEBRA_STALKER_DELAYED = 1,
    SCENE_COMBAT_ZEBRA_STALKER_ACTIVE  = 2
};

/// Difficulty rows distinguish a first normal run from a replay.
enum {
    SCENE_COMBAT_DIFFICULTY_NORMAL = 0,
    SCENE_COMBAT_DIFFICULTY_REPLAY = 4
};

/// Scene combat coordination, actor controls and pending battle rewards.
///
/// Scene loading resets this record. Enemy tasks hold `battleRefs` until they
/// die or retire; the last release finishes the battle and starts the end delay.
/// Room scripts and actor packages also exchange group-specific signals here.
/// `signals` preserves byte access and combined little-endian word tests.
typedef struct {
    union {
        struct {
            u8 battlePhase;             // Battle phase (0 idle, 1 engaged, 2 finished, 3 resumed).
            u8 endDelayFrames;          // Remaining battle-end hold frames; pauses with actor updates.
            u8 actionFlags;             // Stimuli (1 noise, 2 PE active, 4/8 PE cast, 16 running footstep).
            u8 enemyAlert;              // Enemy stimulus (0 none, 1/2 alerts with per-kind reactions).
        } bytes;
        u32 packed;                     // Combined view of the four signal bytes.
    } signals;
    u8  actorControl;                   // Actor control (0 update/draw, 1 pause/redraw, 2 hide).
    u8  peTargetCount;                  // Contact claims for the current PE cast; Life Drain's damage divisor.
    u16 battleRefs;                     // Outstanding enemy and encounter holds, including pending spawns.
    s32 expReward;                      // Experience accumulated for the battle result.
    s32 bpReward;                       // Battle points accumulated for the battle result.
    s32 mpReward;                       // MP accumulated for the battle result.
    s32 lifeDrainHp;                    // Drain damage capped per enemy at its remaining HP; paid to the player.
    s8  actor00700DeathAlert;           // Group alert after an actor 00700/300700 enemy starts dying (0/1).
    u8  actor03700Flags;                // Group signals (1 alert latched, 2 release the held player).
    s8  actor03700Wave;                 // Scripted entrance/wave stage (0 initial, 1 begin, 2+ wave thresholds).
    s8  actor02400Alert;                // Group awakening latched when an actor 02400 projectile is spawned (0/1).
    s8  actor01600Wave;                 // Scripted activation (0 hold, 1 entrance, 2 engage, 3+ wave thresholds).
    u8  pairedEnemySignals;             // Actor 105100/205200 handshake (1 charge, 2 heal request, 4 heal ready, 8 reset).
    s8  maggotCaterpillarEntranceReady; // Releases the Maggot/Caterpillar scripted entrance (0 hold, 1 release).
    u8  madChaserAlertOwner;            // Mad Chaser alert claim: bit 7 held, low nibble enemy placement index.
    s8  shrineEnemyPhase;               // Shrine entrance (0 hidden, 1 reveal/reset delay, 2 run delay).
    s8  actor02500EntranceReady;        // Latched group entrance trigger for actor 02500 (0/1).
    s8  maggotCaterpillarAmbushReady;   // Latched Maggot/Caterpillar group ambush trigger (0/1).
    s8  actor00400HideRequested;        // Hides the actor 00400 group and cuts short its death fade (0/1).
    s8  zebraStalkerGroupPhase;         // Zebra Stalker group activation (0 wait, 1 delayed, 2 active).
    s8  enemySoundBankQueued;           // The scene's shared enemy sound-bank load has been queued (0/1).
    s8  generatorDeathStarted;          // A Generator has begun its death sequence (0/1).
    s8  zebraStalkerDeathAlert;         // A Zebra Stalker has reached its death cleanup; alerts the group (0/1).
    s8  actor00300AttackAlert;          // Group attack trigger, cleared by actor 00300 patrols (0/1).
    s8  golemPawnRookDeathAlert;        // Alerts the surviving Pawn/Rook GOLEMs when one starts dying (0/1).
    u8  field_2A;                       // Initialized to zero; role unproven.
    u8  difficulty;                     // Damage/threshold row (saved modes 0..3, 4 normal replay).
} SceneCombatState;
STATIC_ASSERT_SIZEOF(SceneCombatState, 0x2C);

#endif // GAMEPLAY_WORLD_STATE_H
