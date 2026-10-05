#ifndef MAIN_SESSION_TYPES_H
#define MAIN_SESSION_TYPES_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/animation_types.h"
#include "gameplay/world_collision_types.h"

#include "main/areas.h"
#include "main/coord.h"

struct CompanionWork;
struct AnimationRecord;
struct AnimationSet;
struct WorldTargetNode;
struct Task;

/// Location selector shared by the live session, saved state and world lookups.
///
/// The six unsigned bytes select parts of the location. `view` is
/// a room-local slot; a lookup maps it to the camera/image index. `variant`
/// selects the area's placement and resource layout and mirrors the saved
/// per-area variant after synchronization. Each layout supplies a placement
/// list and its associated resources.
///
/// Active stage, area, room, view and warp IDs are 1-based. Stage 0 means no
/// active stage; the other upper bounds depend on the selected loaded tables.
/// Each lookup requires only the components it reads to be initialized and in
/// range. The key is byte-aligned; packed word reads require a word-aligned
/// instance. Whole location-cell transfers use the eight-byte `GameLoc`.
typedef struct {
    u8 view;    // 1-based view slot within the room, mapped to a camera/image index
    u8 room;    // 1-based room within the area
    u8 area;    // 1-based area and CDF folder within the stage
    u8 stage;   // Stage ID (0 no active stage, 1..5 active stages)
    u8 warp;    // 1-based arrival record within the area's warp table
    u8 variant; // Placement/resource layout within the area (1 default)
} GameLocationKey;
STATIC_ASSERT_SIZEOF(GameLocationKey, 0x6);

/// Eight-byte location cell shared by the live session and saved state.
///
/// Whole-cell copies preserve the two bytes following the six-byte key;
/// their role is unproven. The cell is byte-aligned. Stream lookups borrow
/// `loc` and read only its leading view and room bytes;
/// a local copy can replace the view byte with a stream identifier.
typedef struct {
    GameLocationKey loc;          // Place key; leading view and room also select streams
    u8              unknown_6[2]; // Retained by whole-cell copies; role unproven
} GameLoc;
STATIC_ASSERT_SIZEOF(GameLoc, 8);

/// Music-loading and weapon-restoration options stored in `GameSession::flowFlags`.
enum {
    GAME_SESSION_FLOW_SKIP_ENDING_MUSIC      = 0x01,
    GAME_SESSION_FLOW_SKIP_AREA_MUSIC        = 0x02,
    GAME_SESSION_FLOW_LOAD_ENDING_MUSIC_ONLY = 0x04,
    GAME_SESSION_FLOW_LOAD_AREA_MUSIC_ONLY   = 0x08,
    GAME_SESSION_FLOW_HIDE_REEQUIPPED_WEAPON = 0x40,
    GAME_SESSION_FLOW_REEQUIP_WEAPON         = 0x80
};

/// Pad-script activity and permission bits stored in `GameSession::padScriptFlags`.
enum {
    GAME_SESSION_PAD_SCRIPT_HOLD_ACTIVE          = 0x01,
    GAME_SESSION_PAD_SCRIPT_LERP_ACTIVE          = 0x02,
    GAME_SESSION_PAD_SCRIPT_DURING_BATTLE_FREEZE = 0x80
};

/// Shared spawn-controller progress stored in `GameSession::spawnPhase`.
enum {
    GAME_SESSION_SPAWN_IDLE     = 0,
    GAME_SESSION_SPAWN_ARMED    = 1,
    GAME_SESSION_SPAWN_COMPLETE = 2
};

/// Incinerator descent progress retained across room and actor tasks.
enum {
    GAME_SESSION_INCINERATOR_DESCENT_WAITING  = 0,
    GAME_SESSION_INCINERATOR_DESCENT_MOVING   = 1,
    GAME_SESSION_INCINERATOR_DESCENT_LANDED   = 2,
    GAME_SESSION_INCINERATOR_DESCENT_COMPLETE = 3
};

/// Incinerator exit sequence selected by the warp or encounter controller.
enum {
    GAME_SESSION_INCINERATOR_EXIT_NONE      = 0,
    GAME_SESSION_INCINERATOR_EXIT_WARP      = 1,
    GAME_SESSION_INCINERATOR_EXIT_ENCOUNTER = 2
};

/// Sentinels for the session's resource cache and death presentation.
enum {
    GAME_SESSION_CHARACTER_NOT_LOADED = -1,
    GAME_SESSION_DEATH_SOUND_HOLD     = 0x7F,
    GAME_SESSION_DEATH_FADE_DEFAULT   = -0x80
};

/// Established restart paths stored in `GameSession::restartMode`.
///
/// Value 2 is also written by a scripted death; its distinct role is unproven.
enum {
    GAME_SESSION_RESTART_NORMAL           = 0,
    GAME_SESSION_RESTART_COMPANION_1_DOWN = 1,
    GAME_SESSION_RESTART_PRESERVE_DISPLAY = 3,
    GAME_SESSION_RESTART_COMPANION_3_DOWN = 4,
    GAME_SESSION_RESTART_ENDING           = 0xFF
};

/// Live world, input and script state retained for one play session.
///
/// The resident instance behind `gGameSession` supplies shared state to main
/// and the overlays. New-game, load and reset paths clear the entire object;
/// view and room transitions update its location and request deferred reloads.
/// Task slots borrow their tasks and are cleared when the task system resets.
/// Input masks contain remapped, suppressed buttons rather than raw pad state.
/// Unknown storage is retained with its observed extent and is not padding.
typedef struct {
    s8           deathVariant;            // Death presentation choice (0 inactive, 1/2 file and sound variant)
    s8           eventState;              // Script-controlled state (0 idle, nonzero blocks direction/scene handling)
    u8           uiOpen;                  // UI overlay active (0 closed, 1 open); enables d-pad repeat
    byte         unknown_3;               // Role unproven
    GameLoc      location;                // Live place key and the two bytes retained with whole-cell copies
    struct Task* ptrSlots[16];            // Borrowed task anchors, slots 0..15; NULL when empty
    u8           applySaveVariant;        // Restore the saved placement variant on the next area load (0/1)
    u8           viewReady;               // View transition state (0 loading, 1 ready for scene tasks)
    u16          field_4E;                // Set to 1 by model loads and reflection setup; role unproven
    byte         unknown_50[2];           // Role unproven
    s16          viewDirty;               // Nonzero requests deferred respawn using the saved view
    byte         unknown_54[4];           // Role unproven
    u16          padHeld;                 // Remapped and suppressed held buttons, including analog directions
    u16          padPressed;              // Remapped and suppressed press edges for this frame
    u16          padReleased;             // Remapped and suppressed release edges for this frame
    u8           field_5E;                // Set to 1 when the play-clock task starts; role unproven
    u8           evtSkipped;              // Forced script skip (0/1); releases script and caption waits
    byte         unknown_60[4];           // Role unproven
    u8           freezeRoomObjs;          // Nonzero suppresses room-object state dispatch
    u8           sceneUpdatesPaused;      // Scene/HUD gate (0 updating, nonzero held); 1 also hides the target cursor
    u8           cutsceneHold;            // Scripted player/menu hold (0/1), separate from eventState
    u8           areaSetupDone;           // First arrival setup completed (0/1); later arrivals apply warp SFX/flags
    u8           hideHud;                 // Suppress HUD, item prompts and target cursor (0/1)
    u8           flowFlags;               // GAME_SESSION_FLOW_* music-loading and weapon-restoration bits
    byte         unknown_6A[0xA];         // Role unproven
    u16          spriteVariant;           // 1-based display resource variant; command packets retain its low byte
    s16          roomObjsDirty;           // Nonzero requests deferred room-object relinking
    s16          loadedStage;             // Stage whose resources were last queued (0 before a stage load)
    byte         unknown_7A[2];           // Role unproven
    s16          field_7C;                // Cleared when actor buffer 0 is reused; nonzero meaning unproven
    s16          field_7E;                // Cleared when actor buffer 1 is reused; nonzero meaning unproven
    s16          field_80;                // Cleared when actor buffer 2 is reused; nonzero meaning unproven
    byte         unknown_82[0x9A];        // Role unproven
    s16          loadedCharacterId;       // Character whose resources were last queued; -1 forces reload
    s16          loadedConfigSet;         // Last queued animation/model/file selector, paired with loadedCharacterId
    s16          sceneClock;              // Script frame countdown; arithmetic retains 16-bit wraparound
    s16          waterY;                  // Water-surface height in world-coordinate units
    u8           companionType;           // Cached companion resource family (0 none, 1/2/3 family)
    u8           companionVariant;        // Cached resource variant within companion family 1
    u8           battleResetPending;      // Battle/result reset handshake (0 cleared, 1 requested)
    u8           suppressDeathChecks;     // Nonzero suppresses player and companion death handling
    u8           restartMode;             // 0 ordinary, 1 companion-1 down, 2 unproven, 3 preserve display, 4 companion-3 down, 255 ending
    u8           loadedSndId;             // Last queued sound-file ID; 0 invalidates the cache
    s16          bossPartsHpSum;          // Cached 16-bit sum of boss-part HP, reused for successor HP scaling
    u8           suppressViewTriggers;    // Nonzero suppresses the player's view-transition collision checks
    s8           deathSoundCountdown;     // Death-sound countdown in ticks; 127 holds, a negative value triggers playback
    s8           deathFadeFrames;         // Death-fade duration in ticks; <=0 selects the 32-tick default
    u8           deathRestartDelay;       // Play-clock ticks before death presentation and restart
    u8           spawnPhase[2];           // Per-controller progress (0 idle, 1 armed, 2 complete)
    u8           incineratorDescentPhase; // Descent progress (0 waiting, 1 moving, 2 landed, 3 complete)
    byte         incineratorRoomGroup;    // Incinerator room family (0 rooms below 4, 1 rooms 4 and above)
    u8           eventRoomIndex;          // Zero-based event-reply room; replies add 1 (observed 0..6)
    u8           incineratorExitPhase;    // Exit sequence (0 none, 1 warp exit, 2 encounter exit)
    u8           enemyCullZone;           // Enemy-axis limit selector (0 disabled, 1..16 table entry)
    u8           skipEventIntro;          // Incinerator controller skips introductory spawns (0/1)
    byte         unknown_138;             // Set to 1 before a scripted stage transition; role unproven
    s8           hudShakeY;               // Upward HUD offset in units of 3 pixels; <=0 ignored
    u8           dirActionBusy;           // Direction/caption action in flight (0/1); blocks HUD actions
    u8           padScriptFlags;          // GAME_SESSION_PAD_SCRIPT_* activity and battle-freeze permission bits
} GameSession;
STATIC_ASSERT_SIZEOF(GameSession, 0x13C);
STATIC_ASSERT(OFFSET_OF(GameSession, location) == 4, GameSession_location);
STATIC_ASSERT(OFFSET_OF(GameSession, location.loc) == 4, GameSession_location_loc);
STATIC_ASSERT(OFFSET_OF(GameSession, ptrSlots) == 0xC, GameSession_ptrSlots);

/// Collision-body indices in a player or companion's contiguous body array.
enum {
    GAME_ACTOR_BODY_ROOT   = 0,
    GAME_ACTOR_BODY_PART4  = 1,
    GAME_ACTOR_BODY_PART1  = 2,
    GAME_ACTOR_BODY_WEAPON = 3,
    GAME_ACTOR_BODY_AIM    = 4,
    GAME_ACTOR_BODY_COUNT  = 5
};

/// Active animation-slot counts; storage beyond the largest observed count is unproven.
enum {
    GAME_ACTOR_NORMAL_ANIMATION_SLOTS          = 19,
    GAME_ACTOR_ARMED_COMPANION_ANIMATION_SLOTS = 20
};

/// Top-level actor dispatch modes, retained in an unsigned halfword.
enum {
    GAME_ACTOR_MODE_NORMAL   = 0,
    GAME_ACTOR_MODE_DAMAGE   = 1,
    GAME_ACTOR_MODE_SCRIPTED = 2
};

/// Aim requests and tracking states use separate byte fields.
enum {
    GAME_ACTOR_AIM_REQUEST_ENTER    = 1,
    GAME_ACTOR_AIM_REQUEST_EXIT     = 2,
    GAME_ACTOR_AIM_REQUEST_SCRIPTED = 4,
    GAME_ACTOR_AIM_TRACKING_OFF     = 0,
    GAME_ACTOR_AIM_TRACKING_DECAY   = 1,
    GAME_ACTOR_AIM_TRACKING_TARGET  = 2
};

/// Collision update requests enable low bits or disable their counterparts three bits higher.
///
/// The update loops apply the first two bits to their selected body pair. Bit 2
/// is included in the all-pass requests but has no individual consumer here.
enum {
    GAME_ACTOR_COLLISION_REQUEST_MASK          = 7,
    GAME_ACTOR_COLLISION_FIRST_TWO_REQUESTS    = 3,
    GAME_ACTOR_COLLISION_DISABLE_REQUEST_SHIFT = 3
};

/// Proven pending-reaction IDs; 4..7 select separate presentation paths whose identities remain unproven.
enum {
    GAME_ACTOR_REACTION_ORDINARY  = 0,
    GAME_ACTOR_REACTION_DARKNESS  = 1,
    GAME_ACTOR_REACTION_PARALYSIS = 2,
    GAME_ACTOR_REACTION_POISON    = 3,
    GAME_ACTOR_REACTION_SILENCE   = 8,
    GAME_ACTOR_REACTION_STATUS20  = 9,
    GAME_ACTOR_REACTION_CONFUSION = 10,
    GAME_ACTOR_REACTION_BERSERKER = 11
};

/// Work owned by the player or companion task, including its embedded collision and animation state.
///
/// Player and companion spawns allocate and clear the complete block. The task
/// owns its optional companion work and attached tasks; collision bodies borrow
/// this block's contexts, shapes, contacts and transforms until they are unlinked.
/// Animation resources and model coordinates are borrowed and must remain loaded
/// while playback or a cached cue record uses them.
///
/// Positions and per-tick displacement use integer game-coordinate units in the
/// root coordinate's parent space. Euler angles use 4096 units per turn. The
/// normal actor walks 19 animation slots; the armed companion walks 20. Only
/// that minimum storage extent is established; the subsequent bytes remain
/// unknown. State arguments, timers and auxiliary values are reused by the
/// selected mode/state, rather than having one action-specific interpretation.
typedef struct {
    VECTOR3 velocity;                                                                                  // Root displacement per active movement tick
    byte    unknown_C[4];                                                                              // Unaccessed storage; role unproven
    VECTOR3 previousPosition;                                                                          // Accepted root translation used to reject excessive vertical movement
    byte    unknown_1C[4];                                                                             // Unaccessed storage; role unproven
    VECTOR3 destination;                                                                               // Scripted movement destination in the root's parent space
    byte    unknown_2C[4];                                                                             // Unaccessed storage; role unproven
    VECTOR  pushbackDirection;                                                                         // Normalized collision displacement (4096 per unit); temporarily the previous cached position
    VECTOR3 pendingDisplacement;                                                                       // Additional displacement applied and cleared by the player update
    byte    unknown_4C[4];                                                                             // Unaccessed storage; role unproven
    SVECTOR rotation;                                                                                  // Root Euler rotation; vy is the facing yaw, masked to 0..4095 when turned
    s16     part2Pitch;                                                                                // Additional X rotation of model part 2
    byte    unknown_5A[2];                                                                             // Unaccessed storage; role unproven
    s16     part2Roll;                                                                                 // Additional Z rotation of model part 2
    byte    unknown_5E[2];                                                                             // Unaccessed storage; role unproven
    s16     part3Pitch;                                                                                // Additional X rotation of model part 3
    byte    unknown_62[2];                                                                             // Unaccessed storage; role unproven
    s16     part3Roll;                                                                                 // Additional Z rotation of model part 3
    byte    unknown_66[2];                                                                             // Unaccessed storage; role unproven
    s16     field_68;                                                                                  // Cleared on scripted entry; role unproven
    s16     aimYaw;                                                                                    // Relative yaw applied to model part 4
    byte    unknown_6C[4];                                                                             // Unaccessed storage; role unproven
    s16     part6Pitch;                                                                                // Additional X rotation of model part 6
    byte    unknown_72[6];                                                                             // Unaccessed storage; role unproven
    s16     directAimPitch;                                                                            // Independently tracked pitch measured from the weapon origin
    byte    unknown_7A[6];                                                                             // Unaccessed storage; role unproven
    s16     jumpVariant;                                                                               // Scripted jump pitch selector (0 positive pitch, nonzero negative pitch)
    union {
        s16 targetYaw;                                                                                 // Scripted turn/approach heading, in 1/4096 turns
        s16 jumpSteps;                                                                                 // Remaining animation-cue movement steps in scripted jump state 3
        s16 surfaceIndexBase;                                                                          // Subtracted from a direction nibble in surface-table selection; domain unproven
    } scriptMotion;                                                                                    // Interpretation selected by the scripted action or surface lookup
    byte unknown_84[4];                                                                                // Unaccessed storage; role unproven

    WorldCollisionMotionContext collisionMotionContexts[3];                                            // Root, part-4 and part-1 spheres share collisionContacts
    WorldCollisionBody          collisionBodies[GAME_ACTOR_BODY_COUNT];                                // Three motion spheres, weapon capsule and aim capsule, in that order
    WorldCollisionCapsule       weaponShape;                                                           // Weapon reach/spread and its six-result contact table
    WorldCollisionCapsule       aimShape;                                                              // Companion aiming capsule with a single retained-body contact
    WorldCollisionContact       collisionContacts[18];                                                 // Shared motion-sphere contacts; LAST terminates the table
    WorldCollisionContact       weaponContacts[6];                                                     // Weapon capsule contacts; LAST terminates the table
    WorldCollisionContact       aimContacts[1];                                                        // Single contact for the aim capsule
    GfxCoord                    weaponCollisionCoord;                                                  // Copy of the weapon root transform, rotated for its collision capsules
    AnimationContext            animationContext;                                                      // Borrows this block's slots and encoded buffer, the model coordinates and set table

    AnimationSlot animationSlots[GAME_ACTOR_ARMED_COMPANION_ANIMATION_SLOTS];                          // Active prefix includes slot 0; child playback starts at 1
    byte          unknown_758[0x50];                                                                   // Bytes beyond the 20 established slots; role and further slot capacity unproven
    u8            poseBuffer[GAME_ACTOR_ARMED_COMPANION_ANIMATION_SLOTS][ANIMATION_POSE_BUFFER_BYTES]; // Word-aligned encoded blend poses, 16 bytes per slot
    byte          unknown_8E8[0x24];                                                                   // Bytes beyond the 20 established pose entries; role and further buffer capacity unproven

    struct WorldTargetNode*       targetNode;                                                          // Borrowed current lock target, or NULL; each actor slot can hold one
    struct CompanionWork*         companionWork;                                                       // Owned separate behavior/probe allocation; NULL identifies the player
    struct Task*                  weaponEffectTask;                                                    // Optional owned persistent effect for the equipped weapon
    struct Task*                  equipmentTasks[2];                                                   // Owned optional model tasks (0 creation/role unproven, 1 equipped weapon)
    struct Task*                  attachmentTasks[2];                                                  // Owned model anchors attached to the actor skeleton; the weapon is parented under slot 1
    struct AnimationSet**         animationSets;                                                       // Borrowed current set table; individual animation slots retain their own bindings
    const struct AnimationRecord* lastCueRecord;                                                       // Borrowed last processed record identity; suppresses duplicate cue effects/sounds

    s32 surfaceClass;                                                                                  // Room surface/resource index, from collision response or a scripted override
    s32 stateTimer;                                                                                    // State-owned tick counter/countdown, or scripted input-count threshold
    s16 animationSlotCount;                                                                            // Active prefix of animationSlots (19 normal, 20 armed companion)
    u16 animationBankIndex;                                                                            // Player/companion animation-table index; 0x7FFF selects a directly supplied set table
    u16 actionArgument;                                                                                // Initial/override animation ID, or the selected action's variant
    s16 actionValue;                                                                                   // State-owned argument/counter, including repetitions, cue counts and finish animation
    union {
        s16 cooldownTicks;                                                                             // Ticks before another weapon attack is allowed
        s16 targetVariant;                                                                             // Scripted companion attack animation/sound variant (0 no target, 1 target)
    } attackControl;                                                                                   // Player/armed-companion cooldown or scripted-companion variant
    s16 idleTicks;                                                                                     // Idle/decision elapsed ticks; noncombatant also uses it for its flinch interval
    union {
        s16 darknessTicks;                                                                             // Player: remaining active ticks of PLAYER_STATUS_DARKNESS
        s16 waterDripTicks;                                                                            // Armed companion: water-surface drip-effect countdown
    } effectTimer;                                                                                     // Interpretation selected by the actor identity
    s16 paralysisTicks;                                                                                // Remaining active ticks of PLAYER_STATUS_PARALYSIS
    s16 poisonTicks;                                                                                   // Remaining active ticks of PLAYER_STATUS_POISON
    s16 silenceTicks;                                                                                  // Remaining active ticks of PLAYER_STATUS_SILENCE
    s16 status20Ticks;                                                                                 // Remaining active ticks of status bit 0x20; effect identity unproven
    s16 confusionTicks;                                                                                // Remaining active ticks of PLAYER_STATUS_CONFUSION
    s16 berserkerTicks;                                                                                // Remaining eligible ticks of PLAYER_STATUS_BERSERKER
    s16 gunbladeSpinTicks;                                                                             // Remaining ticks of the Gunblade's yaw-spin/shake phase

    u16 mode;                                                                                          // Top-level dispatch (0 normal, 1 damage reaction, 2 scripted)
    u16 state;                                                                                         // Mode/companion-specific state-table index; valid domain depends on its dispatcher
    s16 movementMode;                                                                                  // Movement-speed/behavior table selector (0 stopped; player 1..4, companions also 5..7)
    u16 turnRateIndex;                                                                                 // Turn-rate table selector (0 disables turning)
    u16 animationState;                                                                                // Cue/end-driven animation controller selector (observed 0..10)
    u16 statePhase;                                                                                    // Progress within the selected state; cue/end handlers advance it
    u16 stateAux;                                                                                      // State-owned auxiliary value: saved state, reload slot, companion subphase or action variant
    u16 padHeld;                                                                                       // Current remapped/suppressed buttons, also altered by confusion
    u16 previousPadHeld;                                                                               // Previous captured padHeld
    u16 padPressed;                                                                                    // Press edges: padHeld & ~previousPadHeld
    u16 padReleased;                                                                                   // Release edges: previousPadHeld & ~padHeld
    u16 actionPadMask;                                                                                 // Buttons allowed to interrupt a completed attack after attackCancelTicks expires
    s16 hitRegion;                                                                                     // Pending collision reaction (0 none, 1 body index 1, 2 other body)
    s16 pendingDamage;                                                                                 // Damage selected from the pending contact, applied by the reaction state
    s16 confusionDirections;                                                                           // Synthetic d-pad bits ORed into held input during confusion

    u8   damageReaction;                                                                               // Pending GAME_ACTOR_REACTION_* code (observed 0..11); 4..7 presentation identities unproven
    s8   movementSign;                                                                                 // Forward/backward movement multiplier (-1 backward, 0 stopped, 1 forward)
    s8   previousMovementSign;                                                                         // Previous captured movementSign
    s8   turnSign;                                                                                     // Yaw or strafe multiplier (-1 negative, 0 none, 1 positive)
    s8   previousTurnSign;                                                                             // Previous captured turnSign
    s8   runButtonHeld;                                                                                // Captured logical run button (0 released, 1 held)
    s8   previousRunButtonHeld;                                                                        // Previous captured runButtonHeld
    s8   attackCancelTicks;                                                                            // Weapon action ticks before held actionPadMask buttons may cancel it
    u8   recoveryTicks;                                                                                // Recovery/cue suppression ticks; countdown/comparisons interpret the byte as s8
    s8   movementInputDisabled;                                                                        // Direction-input gate (0 derives movement/turn signs, nonzero clears both)
    s8   aimTransitionPending;                                                                         // Target assignment latch for the next completed aim transition (0/1)
    u8   aimControl;                                                                                   // Aim request bits (1 enter, 2 exit, 4 scripted weapon action)
    s8   aimTrackingState;                                                                             // Aim-angle state (0 inactive, 1 decay toward zero, 2 track the lock target)
    s8   attackButton;                                                                                 // Selected fire input (0 none, 1 primary, 2 secondary)
    byte unknown_980;                                                                                  // Unaccessed storage; role unproven
    s8   rumblePosted;                                                                                 // Current action's pad-event latch (0 not posted, 1 posted)
    s8   scriptedMotionPending;                                                                        // Scripted movement/turn/jump command in progress (0 complete, 1 pending)
    u8   pendingCollisionUpdates;                                                                      // Deferred pass changes: low bits enable; corresponding bits 3..5 disable
    u8   collisionEnableMask;                                                                          // Applied collision-pass enables (bits 0..2); bit 0 also gates grid response
    u8   animationRate;                                                                                // Per-tick animation rate copied into signed slot rates (16 normal speed)
    s8   usesPushbackDirection;                                                                        // Collision heading override (0 model forward axis times movementSign, 1 pushbackDirection)
    u8   textureSequenceA;                                                                             // First null-terminated texture sequence selector (0 inactive); read as s8
    u8   textureDelayA;                                                                                // First sequence countdown, reloaded to 4; tested as s8
    u8   textureFrameA;                                                                                // First sequence image index; read as s8
    u8   textureSequenceB;                                                                             // Second null-terminated texture sequence selector (0 inactive); read as s8
    u8   textureDelayB;                                                                                // Second sequence countdown, reloaded to 8; tested as s8
    u8   textureFrameB;                                                                                // Second sequence image index; read as s8
    u8   poisonDamageTicks;                                                                            // Ticks until the next poison HP loss; decrement/comparison uses s8
    u8   paralysisProgress;                                                                            // Ticks toward a paralysis episode, then press edges left to escape; tested as s8
    s8   reloadEffectSuppressed;                                                                       // Suppresses first reload effect (0 emit, 1 skip); cleared after the reload cue
    u8   confusionDirectionTicks;                                                                      // Ticks until confusion chooses new direction bits; comparison uses s8
    s8   restrictRunAndAim;                                                                            // Spawn-location restriction (0 ordinary run/aim rules, 1 suppress run and aim entry)
    u8   gridResponse;                                                                                 // Latest grid push/floor response result (0 none, nonzero correction; companions special-case 2)
    u8   hitBodyIndex;                                                                                 // Receiving motion-body index from contact flags (0..2); consumers explicitly read as s8
    byte unknown_994[4];                                                                               // Final bytes included by both full allocation clears; role unproven
} GameActor;
STATIC_ASSERT_SIZEOF(GameActor, 0x998);
STATIC_ASSERT(OFFSET_OF(GameActor, collisionBodies) == 0xAC, GameActor_collisionBodies);
STATIC_ASSERT(OFFSET_OF(GameActor, animationContext) == 0x424, GameActor_animationContext);
STATIC_ASSERT(OFFSET_OF(GameActor, poseBuffer) == 0x7A8, GameActor_poseBuffer);
STATIC_ASSERT(OFFSET_OF(GameActor, targetNode) == 0x90C, GameActor_targetNode);

#endif // MAIN_SESSION_TYPES_H
