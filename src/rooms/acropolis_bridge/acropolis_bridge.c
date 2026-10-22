#include "rooms/acropolis_bridge.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/memory.h>
#include <psyq/stdio.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/display.h"
#include "gameplay/action_prompt.h"
#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/item_menu.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/pad_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "assets/rooms/akropolis/bridge/declarations.inc"

/// Selects the private hotspot test used by the bridge keypad.
///
/// Bind a function identifier with signature
/// `s32(ActionPromptHotspot* hotspots, s16 cursorX, s16 cursorY)`; coordinates
/// are screen-centred pixels. The static prototype establishes local linkage
/// before the shared header. Keep the binding through the hit-test fragment,
/// then undefine it. This alias captures no arguments, evaluates nothing and
/// uses neither stringification nor token pasting.
#define ACTION_PROMPT_HIT_TEST _actionPromptHitTestDefault
static s32 ACTION_PROMPT_HIT_TEST(ActionPromptHotspot* hotspots, s16 cursorX, s16 cursorY);
#include "../../shared/room/action_prompt.h"
#include "../../shared/effects/glow_draw.h"

/// Selects the bridge billboard's `u16` frame and `s16` size arguments.
///
/// Presence-only configuration for the first inclusion of `effect_sprite.h`.
/// The shared debris task calls this room's billboard implementation; the
/// chip drawer retains the common signature. Undefine after the header include.
#define EFFECT_SPRITE_BILLBOARD_HALFWORD_ARGUMENTS
/// Binds the shared debris task to the bridge's exported `void (Task*)` callback.
///
/// Supply this function identifier before `effect_sprite.h` and keep it defined
/// through the debris fragment, which clears it. No arguments or tokens are built.
#define EFFECT_SPRITE_DEBRIS_TASK acropolisBridgeEffectSpriteDebrisTask
#include "../../shared/effects/effect_sprite.h"
#undef EFFECT_SPRITE_BILLBOARD_HALFWORD_ARGUMENTS

#include "../../shared/creatures/boss_stranger/boss_stranger.h"
#include "../../shared/room/bridge_model.h"
#include "../../shared/effects/acropolis_glows.h"

/// Keypad hotspot ID that clears all entered digits.
///
/// The ten digit hotspots use IDs 0..9. ID 10 selects clearing the packed
/// code to three blank nibbles and resetting the entered-digit count.
#define ACROPOLIS_BRIDGE_KEYPAD_KEY_CLEAR 0xA

/// Digits in a complete bridge code; the keypad checks the entry once this
/// many have been typed.
#define ACROPOLIS_BRIDGE_KEYPAD_CODE_DIGITS 3

/// `_AcropolisBridgeKeypadWork::code` with nothing entered. A nibble above 9
/// draws as an empty digit, so this value blanks all three.
#define ACROPOLIS_BRIDGE_KEYPAD_CODE_BLANK 0xFFF

/// Accepted bridge code 561, packed as hundreds/tens/ones in bits 8..11/4..7/0..3.
#define ACROPOLIS_BRIDGE_KEYPAD_CODE_CORRECT 0x561

/// Waiting updates before a complete entry is checked on the following update.
///
/// Entry starts the timer at zero; ten callbacks increment it and return.
#define ACROPOLIS_BRIDGE_KEYPAD_CHECK_DELAY_FRAMES 10

/// Display updates in each visible/blank phase of a keypad result blink.
///
/// Timer values 0..9 select the first phase and 10..19 the second. The next
/// update resets the timer and counts the blink without changing the display;
/// the full cycle therefore takes 21 updates.
#define ACROPOLIS_BRIDGE_KEYPAD_BLINK_PHASE_FRAMES 10

/// Counted blink cycles at which the keypad finishes displaying its result.
///
/// Both result states compare for equality after stepping the retained counter;
/// entry to the accepted state does not reset a previous rejection's count.
#define ACROPOLIS_BRIDGE_KEYPAD_BLINK_COUNT 3

/// Completed rejection displays after which the keypad closes with cancellation.
///
/// The byte counter increments after the error blink finishes, not when a
/// wrong code is first detected. Counts below three permit another entry.
#define ACROPOLIS_BRIDGE_KEYPAD_REJECTED_LIMIT 3

/// Dispatch indices of the keypad task; stored in `Task::state`.
enum {
    ACROPOLIS_BRIDGE_KEYPAD_STATE_OPEN         = 0,
    ACROPOLIS_BRIDGE_KEYPAD_STATE_ARM_CURSOR   = 1,
    ACROPOLIS_BRIDGE_KEYPAD_STATE_ENTER_CODE   = 2,
    ACROPOLIS_BRIDGE_KEYPAD_STATE_OPEN_EXAMINE = 3,
    ACROPOLIS_BRIDGE_KEYPAD_STATE_WAIT_EXAMINE = 4,
    ACROPOLIS_BRIDGE_KEYPAD_STATE_CHECK_CODE   = 5,
    ACROPOLIS_BRIDGE_KEYPAD_STATE_ACCEPTED     = 6,
    ACROPOLIS_BRIDGE_KEYPAD_STATE_REJECTED     = 7,
    ACROPOLIS_BRIDGE_KEYPAD_STATE_CLOSE        = 8
};

/// CAP command and variants for the keypad caption.
enum {
    ACROPOLIS_BRIDGE_KEYPAD_COMMAND                  = 7,
    ACROPOLIS_BRIDGE_KEYPAD_CAPTION_VARIANT          = 1,
    ACROPOLIS_BRIDGE_KEYPAD_COMPLETE_CAPTION_VARIANT = 2,
    ACROPOLIS_BRIDGE_KEYPAD_CAPTION_PLAYBACK_MODE    = 1
};

/// Key-item-use message answered by this room; the first payload is an item ID.
enum { ACROPOLIS_BRIDGE_MESSAGE_USE_KEY_ITEM = 0x13F1 };

/// Message that seeds the arrival scene's dust plane on the room-effect task.
enum { ACROPOLIS_BRIDGE_EFFECT_MESSAGE_ARRIVAL_DUST = 3104 };

/// Arrival-scene sprite selection, applied to the argument's low byte.
/// Each frame selects one three-source batch in the tenth view.
enum {
    ACROPOLIS_BRIDGE_ARRIVAL_FRAME_FIRST  = 0,
    ACROPOLIS_BRIDGE_ARRIVAL_FRAME_SECOND = 1,
    ACROPOLIS_BRIDGE_ARRIVAL_FRAME_THIRD  = 2,
    ACROPOLIS_BRIDGE_ARRIVAL_FRAME_HIDDEN = 3,
    ACROPOLIS_BRIDGE_ARRIVAL_VIEW_INDEX   = 9,
    ACROPOLIS_BRIDGE_ARRIVAL_BATCH_FIRST  = 1,
    ACROPOLIS_BRIDGE_ARRIVAL_BATCH_SECOND = 2,
    ACROPOLIS_BRIDGE_ARRIVAL_BATCH_THIRD  = 3
};

/// Batch indices in the eighth view's three-digit keypad display.
enum {
    ACROPOLIS_BRIDGE_KEYPAD_VIEW_INDEX           = 7,
    ACROPOLIS_BRIDGE_KEYPAD_DIGIT_BATCH_COUNT    = 10,
    ACROPOLIS_BRIDGE_KEYPAD_ONES_FIRST_BATCH     = 1,
    ACROPOLIS_BRIDGE_KEYPAD_TENS_FIRST_BATCH     = 11,
    ACROPOLIS_BRIDGE_KEYPAD_HUNDREDS_FIRST_BATCH = 21,
    ACROPOLIS_BRIDGE_KEYPAD_ONES_BLANK_BATCH     = 31,
    ACROPOLIS_BRIDGE_KEYPAD_TENS_BLANK_BATCH     = 32,
    ACROPOLIS_BRIDGE_KEYPAD_HUNDREDS_BLANK_BATCH = 33,
    ACROPOLIS_BRIDGE_KEYPAD_ERROR_BATCH          = 34,
    ACROPOLIS_BRIDGE_KEYPAD_PROMPT_BATCH         = 35
};

/// Cutscene views, keypad result and post-movie bridge placement variant.
enum {
    ACROPOLIS_BRIDGE_VIEW_OVERVIEW                  = 6,
    ACROPOLIS_BRIDGE_VIEW_KEYPAD                    = 8,
    ACROPOLIS_BRIDGE_VIEW_KEYPAD_MOVIE              = 9,
    ACROPOLIS_BRIDGE_KEYPAD_CANCELLED               = 0,
    ACROPOLIS_BRIDGE_KEYPAD_ACCEPTED                = 1,
    ACROPOLIS_BRIDGE_ROOM_AFTER_KEYPAD_MOVIE        = 2,
    ACROPOLIS_BRIDGE_PROGRESS_KEYPAD_MOVIE_COMPLETE = 3
};

/// Actor commands for revealing the enemies and starting their movie presentation.
///
/// The reveal command uses the promenade context; keypad acceptance uses the bridge.
enum {
    ACROPOLIS_BRIDGE_COMMAND_CONTEXT_MOVIE      = GAME_STAGE_AKROPOLIS | (GAME_AREA_AKROPOLIS_PROMENADE << 8),
    ACROPOLIS_BRIDGE_COMMAND_CONTEXT_KEYPAD     = GAME_STAGE_AKROPOLIS | (GAME_AREA_AKROPOLIS_BRIDGE << 8),
    ACROPOLIS_BRIDGE_COMMAND_SHOW_MOVIE_ENEMIES = 1,
    ACROPOLIS_BRIDGE_COMMAND_KEYPAD_ACCEPTED    = 2
};

/// Enemy behavior indices used by this room's state dispatcher.
enum {
    ACROPOLIS_BRIDGE_ENEMY_STATE_INACTIVE             = 0,
    ACROPOLIS_BRIDGE_ENEMY_STATE_PATROL_SUNK          = 1,
    ACROPOLIS_BRIDGE_ENEMY_STATE_CHASE                = 2,
    ACROPOLIS_BRIDGE_ENEMY_STATE_RETREAT              = 3,
    ACROPOLIS_BRIDGE_ENEMY_STATE_FOLLOW_MOVIE         = 4,
    ACROPOLIS_BRIDGE_ENEMY_STATE_DEFEATED_BOB         = 5,
    ACROPOLIS_BRIDGE_ENEMY_STATE_COLLAPSE             = 6,
    ACROPOLIS_BRIDGE_ENEMY_STATE_DEATH_EFFECT         = 7,
    ACROPOLIS_BRIDGE_ENEMY_STATE_ANIMATE_AT_PLACEMENT = 8
};

/// Enemy animation requests and clips; playback rates use `ANIMATION_RATE_ONE`.
enum {
    ACROPOLIS_BRIDGE_ENEMY_ANIM_REQUEST_BLEND   = 1,
    ACROPOLIS_BRIDGE_ENEMY_ANIM_REQUEST_RESET   = 2,
    ACROPOLIS_BRIDGE_ENEMY_ANIM_REQUEST_PLAYING = 3,
    ACROPOLIS_BRIDGE_ENEMY_ANIM_PATROL          = 1,
    ACROPOLIS_BRIDGE_ENEMY_ANIM_CHASE           = 2,
    ACROPOLIS_BRIDGE_ENEMY_ANIM_BOB             = 3,
    ACROPOLIS_BRIDGE_ENEMY_ANIM_REACTION        = 4,
    ACROPOLIS_BRIDGE_ENEMY_ANIM_YAW_REROLL_TICK = 5,
    ACROPOLIS_BRIDGE_ENEMY_ANIM_YAW_REROLL_WORD = (ACROPOLIS_BRIDGE_ENEMY_ANIM_YAW_REROLL_TICK << 16) |
                                                  ACROPOLIS_BRIDGE_ENEMY_ANIM_REACTION
};

/// Root Y thresholds in game-coordinate units; the two chase gates retain their different values.
enum {
    ACROPOLIS_BRIDGE_ENEMY_CHASE_START_Y = 701,
    ACROPOLIS_BRIDGE_ENEMY_CHASE_KEEP_Y  = 801,
    ACROPOLIS_BRIDGE_ENEMY_SINK_LIMIT    = 1800
};

/// Presentation root Y gates in coordinate units and restart delay in updates.
enum {
    ACROPOLIS_BRIDGE_ENEMY_PRESENTATION_PATH_Y_OFFSET = 200,
    ACROPOLIS_BRIDGE_ENEMY_PRESENTATION_SCALE_START_Y = 500,
    ACROPOLIS_BRIDGE_ENEMY_PRESENTATION_RETRY_MIN_Y   = -989,
    ACROPOLIS_BRIDGE_ENEMY_PRESENTATION_POSE_MAX_Y    = 800,
    ACROPOLIS_BRIDGE_ENEMY_PRESENTATION_RETRY_TICKS   = 8
};

/// Work block of the task that runs the bridge's code keypad screen, allocated
/// by its first state and kept at `Task::work`.
///
/// The keypad's entry state acts on the key the player confirms. Until the
/// player has accepted the Examine command on the keypad, a confirmed key is
/// latched and opens that command at the cursor; afterwards a digit key shifts
/// its digit into `code` and the clear key blanks it. A complete entry is held
/// for a moment and checked: the right code blinks on the display and ends the
/// task, a wrong one blinks the error display and returns to entry, and the
/// keypad closes once the limit of wrong codes is reached.
typedef struct {
    s32 field_0;        // Set to 0x14 when the task starts and never read; role unproven
    s16 code;           // Digits entered so far, one per nibble with the newest lowest; ACROPOLIS_BRIDGE_KEYPAD_CODE_BLANK when none
    u8  digitCount;     // Digits typed into `code` since it was last blank
    u8  rejectedCount;  // Wrong codes entered since the keypad opened
    s16 blinkCount;     // Blink cycles the result display has completed
    s16 timer;          // Frames the current timed step has run: the hold before a complete entry is checked, then the position inside a blink cycle
    s16 selectedKey;    // `ActionPromptHotspot::id` of the key confirmed before the keypad was examined; stored and never read
    s8  promptKind;     // `ActionPromptHotspot::promptKind` of that key, forwarded when its command prompt opens
    s8  keypadExamined; // Whether the Examine command was accepted on the keypad (0 a key confirm offers Examine, 1 it types the key)
} _AcropolisBridgeKeypadWork;
STATIC_ASSERT_SIZEOF(_AcropolisBridgeKeypadWork, 0x10);

/// Scratch-stack workspace for the four-corner quad one of the bridge's effect
/// tasks projects straight into its packet.
///
/// `vertices` stages each corner in the effect's own frame and then holds the
/// world position that corner is projected from. One perspective transform
/// projects corner 0 and a triple transform projects corners 1..3; the screen
/// positions are stored directly in the primitive, so the block keeps none of
/// them. `otz` receives the depth after the triple transform; with the
/// drawer's bias added it decides whether the quad is drawn and selects its
/// ordering-table entry.
///
/// The block is one word longer than `OverlayFlaggedQuadScratch`, which the
/// room's other packet-projected quad uses and whose word after the depth is
/// the GTE flag word. This drawer stores no flag word and touches nothing
/// between `otz` and `vertices`, so what those eight bytes are for is unproven.
///
/// Reserve the complete block and release it in scratch-stack order after
/// drawing; no pointer into it survives release.
typedef struct {
    s32     otz;         // Last projected corner's SZ3 / 4, then with the drawer's ordering bias added
    u8      field_4[8];  // Reserved with the block but never read or written; role unproven
    SVECTOR vertices[4]; // Local corner workspace, then the world positions supplied to the projection
} _AcropolisBridgeQuadScratch;
STATIC_ASSERT_SIZEOF(_AcropolisBridgeQuadScratch, 0x2C);

/// Scratch-stack workspace for one upright debris billboard.
///
/// `_effectSpriteDrawBillboard` reserves one block and copies the piece's
/// world translation into `worldPoint`, narrowed to 16 bits. One perspective
/// transform through `GsWSMATRIX` supplies the screen centre, the GTE flag
/// word and SZ3 / 4. A negative flag word drops the piece. Otherwise `depth`
/// is incremented by one and used both as the divisor for `screenExtent` and
/// as the ordering-table depth.
///
/// `screenX` and `screenY` keep the raw 16-bit encodings of the signed GTE
/// pixel coordinates. They are adjacent so one screen-XY store fills both.
/// `screenExtent` is the quad's half-side in pixels: the caller's size times
/// 55, divided by `depth`. The quad is a square of twice that side, shifted
/// up so the projected point sits three quarters of the way down it. The
/// piece stays axis-aligned, so one extent takes the place of the
/// corner-offset pair in `EffectBillboardScratch`. `EffectCentreScratch`
/// holds the same words in a different order.
///
/// Reserve one complete block and release it before any pointer into it is
/// used again.
typedef struct {
    s32     depth;           // SZ3 / 4 plus one; divisor for the half-side and ordering-table depth
    s32     screenExtent;    // Half-side of the upright quad, in pixels
    s32     projectionFlags; // GTE FLAG word; a negative value rejects the projection
    SVECTOR worldPoint;      // World position at projection, each component narrowed to 16 bits
    u16     screenX;         // Raw projected centre X; first half of the GTE screen-position word
    u16     screenY;         // Raw projected centre Y; second half of the same GTE word
} _AcropolisBridgeBillboardScratch;
STATIC_ASSERT_SIZEOF(_AcropolisBridgeBillboardScratch, 0x18);

/// Exit result shared by the bridge keypad's singleton state callbacks.
///
/// Opening initializes CANCELLED (0); a completed accepted blink stores
/// ACCEPTED (1). Closing forwards the value to `taskRequestKill`.
static s32 _gAcropolisBridgeKeypadExitResult;

/// Next frame-local GPU image-move packet for the bridge's framebuffer streaks.
///
/// The ambient task resets this borrowed cursor before streak tasks draw. Its
/// two 0x7000-byte regions start at byte 0xA000 in `gFsActorBuffer2Base`, selected
/// by OT buffer 0/1. Each streak draw consumes one 24-byte `DR_MOVE`; the normal
/// counted-effect limit of 129 fits either region. This is byte-addressed file
/// workspace, not an array owned by the cursor. Keep it reserved until queued
/// GPU work completes, and reset the cursor before reusing the frame region.
static DR_MOVE* _gAcropolisBridgeStreakMoveCursor;

static void _actionPromptResetDefault(Task* task);
static void _acropolisBridgeWaitForKeypadResult(Task* task);

/// Keypad-movie position used to synchronize the bridge enemy's height.
///
/// The movie task snapshots the decoder's one-based frame number; zero is the
/// initial value before its first update. Height lookup selects frame + 1 in
/// the room's 190-point path, requiring movieFrame <= 188. The next two stored
/// bytes have no established interpretation or containing-object relationship.
typedef struct {
    u16  movieFrame;   // One-based encoded movie position (0 before the first movie-task update)
    byte unknown_2[2]; // Retained bytes; format, role and object boundary unproven
} _AcropolisBridgeMovieFrameStorage;
STATIC_ASSERT_SIZEOF(_AcropolisBridgeMovieFrameStorage, 4);

static void _acropolisBridgeRoomSetup(Task* task);
static void _acropolisBridgeRoomIdle(Task* task);
static void _acropolisBridgeUpdateModelVisibility(Task* task);
static void _acropolisBridgeStartKeypadCaption(Task* task);
static void _acropolisBridgeWaitForKeypadCaption(Task* task);
static void _acropolisBridgeStartKeypad(Task* task);
static void _acropolisBridgeResumeAfterKeypad(Task* task);
static void _acropolisBridgeAdvanceCutsceneState(Task* task);
static void _acropolisBridgeStartKeypadMovie(Task* sequenceTask);
static void _acropolisBridgeFinishKeypadMovie(Task* task);
static void _acropolisBridgeRestoreAfterKeypadMovie(Task* task);
static void _acropolisBridgeKeypadOpen(Task* task);
static void _acropolisBridgeKeypadEnterCode(Task* task);
static void _acropolisBridgeKeypadBlinkAcceptedCode(Task* task);
static void _acropolisBridgeKeypadBlinkError(Task* task);
static void _acropolisBridgeShowKeypadError(void);
static void _acropolisBridgeSetArrivalSpritesHidden(s32 arrivalSceneSeen);
static void _acropolisBridgeKeypadArmCursor(Task* task);
static void _acropolisBridgeKeypadOpenExaminePrompt(Task* task);
static void _acropolisBridgeKeypadCollectExamineAnswer(Task* task);
static void _acropolisBridgeKeypadCheckCode(Task* task);
static void _acropolisBridgeKeypadClose(Task* task);
static void _acropolisBridgeDrawWaterRipple(const GfxCoord* coord, s32 halfSize, s16 brightness);
static void _acropolisBridgeEnemySetup(Enemy* enemy, Task* task);
static void _acropolisBridgeEnemyFrame(Enemy* enemy, Task* task);

/// Work block of the enemy that lurks below the bridge, allocated into `Task::work`.
///
/// `state` indexes `_gAcropolisBridgeEnemyStateHandlers`; each handler runs its entry
/// setup while `stateEntered` is set. The model sits `sinkDepth` below the root
/// height it spawned at, so the handlers sink and raise it by easing that one
/// offset. `body` takes the enemy's incoming contacts (attacks and the surface
/// it lands on) and `attack` is the box that strikes the player while it
/// chases; the handlers toggle `WORLD_COLLISION_BODY_PAIR_ENABLED` on each.
typedef struct {
    s16                   state;             // handler index (0 inactive, 1 sunk patrolling, 2 risen and chasing, 3 sinking after a strike, 4/8 falling, 5 out of HP and bobbing, 6 out of HP mid-fall, 7 landed from a fall)
    s16                   prevState;         // `state` as of the previous tick; -1 before the first
    s16                   stateEntered;      // 1 on the first tick of a state, else 0
    byte                  unknown_6[0x2];    // No direct accesses; role unproven
    s16                   yaw;               // model root yaw while falling, in 1/4096 turns
    byte                  unknown_A[0x2];    // No direct accesses; role unproven
    ActorAnimRig4         rig;               // playback storage of the model's parts; slots 1..3 are driven
    s16                   animRequest;       // (1 restart `animId` blended from `prevAnimId`, 2 restart without blend, 3 playing)
    s16                   prevAnimId;        // animation last started; row of the blend-length table
    s16                   animId;            // animation requested or playing
    s16                   animFrame;         // ticks since `animId` started
    s16                   animRate;          // playback rate copied into each driven slot (`ANIMATION_RATE_ONE` is normal speed)
    byte                  unknown_10A[0x2];  // No direct accesses; role unproven
    s16                   hp;                // hit points; mirrored into `Enemy::hp` after each hit
    s16                   field_10E;         // set to 1 beside `hp` at setup and never read; role unproven
    WorldCollisionBody    body;              // sphere on list 2 receiving attacks and surface contacts
    WorldCollisionContact bodyContacts[3];   // `body`'s contacts, also the walker's avoidance contacts
    WorldCollisionBody    attack;            // sphere on list 3 that strikes the player, enabled while chasing
    WorldCollisionContact attackContacts[1]; // `attack`'s contact; occupied once it has struck
    MATRIX                lightMtx;          // the model's light matrix
    MATRIX                colorMtx;          // the model's colour matrix; loaded with a fixed blend while falling
    EffectSpawnArg        effectArg;         // placement of the hit and death effects
    s16                   sinkDepth;         // distance the model root sits below `baseHeight` (0 fully risen)
    s16                   baseHeight;        // model root height at spawn
    BossStrangerWalker    walker;            // patrol/chase movement
    u16                   deathFrame;        // ticks into the collapse or death sequence, stops at 101
    u16                   syncedView;        // camera view index the mesh visibility was last synced to
} _AcropolisBridgeEnemyWork;
STATIC_ASSERT_SIZEOF(_AcropolisBridgeEnemyWork, 0x294);

/// Scratch-stack block holding the attack contact the enemy's per-frame tick
/// found on its body.
///
/// The tick reserves one before scanning `_AcropolisBridgeEnemyWork::bodyContacts`
/// and releases it when the frame's state handler has run. Nothing reads the
/// block back: the tick passes the key on to the damage step by value.
typedef struct {
    SVECTOR point; // the contact's world position; only vx/vy/vz are written, and only when an attack contact was found
    s32     key;   // the contact's packed key, handed to the damage step as the attack id (0 no attack contact this frame)
} _AcropolisBridgeHitScratch;
STATIC_ASSERT_SIZEOF(_AcropolisBridgeHitScratch, 0xC);

/// Ticks the walker task: steps its patrol route and drives its animation.

/// Returns the patrol node nearest the given actor's coordinate, by squared
/// distance in the XZ plane.
/// Returns the patrol node nearest the walker, by squared distance in the
/// XZ plane between the node table and the walker's coordinate translation.

static void _acropolisBridgeShowKeypadCode(s32 code, s16 hidePrompt);

static void _acropolisBridgeKeypadMovieTask(Task* movieTask);
static void _acropolisBridgeKeypadCursorTask(Task* task);
static s32  _acropolisBridgeSpawnArrivalDust(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);

static void _acropolisBridgePrepareArrivalScene(void);
static void _acropolisBridgeSelectArrivalSpriteFrame(s32 frame);

static s32  _acropolisBridgeResolveRoomTransition(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _acropolisBridgeRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
static s32  _acropolisBridgeHandleKeypadCommand(Task* task, s32 messageId, s32 commandIndex, s32 unusedArg);
static s32  _acropolisBridgeIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg);
static s32  _acropolisBridgeIgnoreSoundCommand(Task* task, s32 messageId, s32 soundId, s32 unusedArg);
static void _acropolisBridgeModelTask(Task* task);
static void _acropolisBridgeKeypadSequenceTask(Task* task);

/// Storage of the one-entry task descriptor table the bridge enemy spawns from.
///
/// The room's area-resource table names `desc` as the descriptor table of its
/// enemy entry, with spawn index 0: a TMD-bodied task on the enemy model this
/// room embeds, running the enemy's task callback. No other entry exists and
/// nothing indexes past it. The eight bytes after the descriptor sit between it
/// and the room's own task pointers; they are zero in the image and have no
/// established access, so whether they belong to the descriptor's object or are
/// separate unreferenced variables is unproven. They stay in this allocation
/// only to keep the data that follows at its address.
typedef struct {
    TaskDesc desc;         // The enemy's spawn recipe; the table's only entry
    u8       unknown_C[8]; // Zero in the image; no access established and role unproven
} _AcropolisBridgeEnemyTaskDescStorage;
STATIC_ASSERT_SIZEOF(_AcropolisBridgeEnemyTaskDescStorage, 0x14);

static _AcropolisBridgeEnemyTaskDescStorage _gAcropolisBridgeEnemyTaskDescStorage;

static s32  _acropolisBridgeEnemyApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);
static s32  _acropolisBridgeEnemySetModelDraw(Task* task, s32 messageId, s32 drawRequest, s32 unusedArg);
static void _acropolisBridgeEnemyPatrolSunk(Task* task);
static void _acropolisBridgeEnemyChase(Task* task);
static void _acropolisBridgeEnemyRetreat(Task* task);
static void _acropolisBridgeEnemyFollowMovie(Task* task);
static void _acropolisBridgeEnemyAnimateAtPlacement(Task* task);
static void _acropolisBridgeEnemyDefeatedBob(Task* task);
static void _acropolisBridgeEnemyCollapse(Task* task);
static void _acropolisBridgeEnemyDeathEffect(Task* task);
static void _acropolisBridgeEnemyInactive(Task* task);
static void _acropolisBridgeEnemyTask(Task* task);

/// Initial identity transform of the translucent plane's only part.
/// Vertex count of the translucent plane's single part.
/// Translucent plane vertices in integer part-local coordinates, all at Y zero.
/// Semi-transparent textured quads; model creation resolves the draw slots in place.
/// Number of directly transformed FT4 packets in each plane primitive-buffer half.
enum { ACROPOLIS_BRIDGE_TRANSLUCENT_PLANE_QUAD_COUNT = 137 };
/// Fixed horizontal translucent plane drawn by the bridge room's model task.
///
/// The single part is placed at world (-9200, 300, -2800) and hidden in mapped
/// views 8..10. Each buffer half holds 137 directly transformed `POLY_FT4`
/// packets. Instances borrow this source and its writable stream until teardown;
/// keep the bridge overlay loaded. The stream never reads normals; the unused
/// pointer retains the image's one-past-vertices address.
#include "assets/rooms/akropolis/bridge/acropolis_bridge_model_0AD9C.inc"

/// Room-transition, keypad, action, item-use and sound message bindings.
///
/// The room task borrows this read-only table while the overlay is loaded.
/// Transition payloads are a request and writable reply; keypad/item/sound
/// payloads are integer words and room actions borrow a direction request.
/// The final end marker makes unsupported messages return zero.
static TaskMessageEntry _gAcropolisBridgeRoomMessageTable[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _acropolisBridgeResolveRoomTransition },
    { ROOM_MESSAGE_COMMAND, _acropolisBridgeHandleKeypadCommand },
    { DIRECTION_MESSAGE_ROOM_ACTION, _acropolisBridgeIgnoreRoomAction },
    { ACROPOLIS_BRIDGE_MESSAGE_USE_KEY_ITEM, _acropolisBridgeRejectKeyItemUse },
    { ROOM_MESSAGE_SOUND, _acropolisBridgeIgnoreSoundCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Indices of the two room-owned task recipes and their scheduler priority.
enum {
    ACROPOLIS_BRIDGE_ROOM_TASK_MODEL           = 0,
    ACROPOLIS_BRIDGE_ROOM_TASK_KEYPAD_SEQUENCE = 1,
    ACROPOLIS_BRIDGE_CONTROL_TASK_PRIORITY     = 192
};

/// Bridge-model and keypad-sequence task recipes, followed by the bank end marker.
///
/// Model setup borrows the embedded plane source; the sequence has no body.
/// Keep the overlay loaded while either task or its callbacks are live.
static TaskDesc _gAcropolisBridgeRoomTaskDescriptors[3] = {
    { { { TASK_BODY_TMD, ACROPOLIS_BRIDGE_CONTROL_TASK_PRIORITY } }, _acropolisBridgeModelTask, { .model = &_gAkropolisBridgeAcropolisBridgeModel0AD9C } },
    { { { TASK_BODY_NONE, ACROPOLIS_BRIDGE_CONTROL_TASK_PRIORITY } }, _acropolisBridgeKeypadSequenceTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Arrival CAP slot, pixel shake amplitude and interpreter-update delays.
enum {
    ACROPOLIS_BRIDGE_ARRIVAL_CAP_SLOT                    = 1,
    ACROPOLIS_BRIDGE_ARRIVAL_SHAKE_AMPLITUDE_PIXELS      = 4,
    ACROPOLIS_BRIDGE_ARRIVAL_SHAKE_HALF_DURATION_UPDATES = 7,
    ACROPOLIS_BRIDGE_ARRIVAL_FINISH_DELAY_UPDATES        = 3
};

/// Interpreter-update delays bracketing the arrival skip's fade and view restoration.
enum {
    ACROPOLIS_BRIDGE_ARRIVAL_SKIP_FADE_WAIT_UPDATES = 8,
    ACROPOLIS_BRIDGE_ARRIVAL_SKIP_VIEW_WAIT_UPDATES = 1
};

#include "assets/rooms/akropolis/bridge/events.inc"
/// Bodyless accepted-code movie recipe at descriptor index zero.
///
/// Spawning copies this recipe synchronously; keep its room-owned callback loaded
/// until movie teardown. The movie starts with state and kill countdown zero.
static TaskDesc _gAcropolisBridgeKeypadMovieTaskDesc = {
    { { TASK_BODY_NONE, ACROPOLIS_BRIDGE_CONTROL_TASK_PRIORITY } }, _acropolisBridgeKeypadMovieTask, { .value = 0 }
};

/// Raw movie samples supplying the bridge enemies' presentation height.
///
/// Read Y at captured STR frame + 1, in signed game-coordinate units; the
/// presentation subtracts its own root-height offset. The index must be below
/// 190. X, Z and the fourth halfword remain in the asset but have no consumers.
/// Keep the samples loaded while the movie and its enemy presentation run.
static SVECTOR _gAcropolisBridgeMovieEnemyHeightPath[190] = {
#include "assets/acropolis_bridge_path_0BC80.inc"
};

/// Bodyless keypad cursor recipe, borrowed until the keypad closes its cursor.
static TaskDesc _gAcropolisBridgeKeypadCursorTaskDesc = {
    { { TASK_BODY_NONE, ACROPOLIS_BRIDGE_CONTROL_TASK_PRIORITY } }, _acropolisBridgeKeypadCursorTask, { .value = 0 }
};

/// Cursor hit regions for the bridge keypad's ten digit keys and clear key.
///
/// Rectangles use pixels relative to the view centre. IDs 0..9 are the entered
/// digit, and CLEAR (10) blanks the entry; END terminates the list. Hit testing
/// mutates `hit`, so this singleton table stays writable while the keypad runs.
static ActionPromptHotspot _gAcropolisBridgeKeypadHotspots[] = {
    { 20, -41, 20, 21, 1, 0, 0 },
    { 41, -41, 20, 21, 2, 0, 0 },
    { 62, -41, 20, 21, 3, 0, 0 },
    { 20, -20, 20, 21, 4, 0, 0 },
    { 41, -20, 20, 21, 5, 0, 0 },
    { 62, -20, 20, 21, 6, 0, 0 },
    { 20, 1, 20, 21, 7, 0, 0 },
    { 41, 1, 20, 21, 8, 0, 0 },
    { 62, 1, 20, 21, 9, 0, 0 },
    { 20, 23, 20, 21, 0, 0, 0 },
    { 42, 23, 40, 21, ACROPOLIS_BRIDGE_KEYPAD_KEY_CLEAR, 0, 0 },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
};

/// Nibble-to-sprite-batch lookup for the keypad's hundreds, tens and ones rows.
///
/// Each 16-entry row maps 0..9 to that digit's batch in mapped view 8. Nibbles
/// 10..15 select the row's blank batch (33, 32 or 31), not a list terminator.
static u8 _gAcropolisBridgeKeypadDigitBatches[ACROPOLIS_BRIDGE_KEYPAD_CODE_DIGITS][16] = {
    { ACROPOLIS_BRIDGE_KEYPAD_HUNDREDS_FIRST_BATCH, ACROPOLIS_BRIDGE_KEYPAD_HUNDREDS_FIRST_BATCH + 1,
      ACROPOLIS_BRIDGE_KEYPAD_HUNDREDS_FIRST_BATCH + 2, ACROPOLIS_BRIDGE_KEYPAD_HUNDREDS_FIRST_BATCH + 3,
      ACROPOLIS_BRIDGE_KEYPAD_HUNDREDS_FIRST_BATCH + 4, ACROPOLIS_BRIDGE_KEYPAD_HUNDREDS_FIRST_BATCH + 5,
      ACROPOLIS_BRIDGE_KEYPAD_HUNDREDS_FIRST_BATCH + 6, ACROPOLIS_BRIDGE_KEYPAD_HUNDREDS_FIRST_BATCH + 7,
      ACROPOLIS_BRIDGE_KEYPAD_HUNDREDS_FIRST_BATCH + 8, ACROPOLIS_BRIDGE_KEYPAD_HUNDREDS_FIRST_BATCH + 9,
      ACROPOLIS_BRIDGE_KEYPAD_HUNDREDS_BLANK_BATCH, ACROPOLIS_BRIDGE_KEYPAD_HUNDREDS_BLANK_BATCH,
      ACROPOLIS_BRIDGE_KEYPAD_HUNDREDS_BLANK_BATCH, ACROPOLIS_BRIDGE_KEYPAD_HUNDREDS_BLANK_BATCH,
      ACROPOLIS_BRIDGE_KEYPAD_HUNDREDS_BLANK_BATCH, ACROPOLIS_BRIDGE_KEYPAD_HUNDREDS_BLANK_BATCH },
    { ACROPOLIS_BRIDGE_KEYPAD_TENS_FIRST_BATCH, ACROPOLIS_BRIDGE_KEYPAD_TENS_FIRST_BATCH + 1,
      ACROPOLIS_BRIDGE_KEYPAD_TENS_FIRST_BATCH + 2, ACROPOLIS_BRIDGE_KEYPAD_TENS_FIRST_BATCH + 3,
      ACROPOLIS_BRIDGE_KEYPAD_TENS_FIRST_BATCH + 4, ACROPOLIS_BRIDGE_KEYPAD_TENS_FIRST_BATCH + 5,
      ACROPOLIS_BRIDGE_KEYPAD_TENS_FIRST_BATCH + 6, ACROPOLIS_BRIDGE_KEYPAD_TENS_FIRST_BATCH + 7,
      ACROPOLIS_BRIDGE_KEYPAD_TENS_FIRST_BATCH + 8, ACROPOLIS_BRIDGE_KEYPAD_TENS_FIRST_BATCH + 9,
      ACROPOLIS_BRIDGE_KEYPAD_TENS_BLANK_BATCH, ACROPOLIS_BRIDGE_KEYPAD_TENS_BLANK_BATCH,
      ACROPOLIS_BRIDGE_KEYPAD_TENS_BLANK_BATCH, ACROPOLIS_BRIDGE_KEYPAD_TENS_BLANK_BATCH,
      ACROPOLIS_BRIDGE_KEYPAD_TENS_BLANK_BATCH, ACROPOLIS_BRIDGE_KEYPAD_TENS_BLANK_BATCH },
    { ACROPOLIS_BRIDGE_KEYPAD_ONES_FIRST_BATCH, ACROPOLIS_BRIDGE_KEYPAD_ONES_FIRST_BATCH + 1,
      ACROPOLIS_BRIDGE_KEYPAD_ONES_FIRST_BATCH + 2, ACROPOLIS_BRIDGE_KEYPAD_ONES_FIRST_BATCH + 3,
      ACROPOLIS_BRIDGE_KEYPAD_ONES_FIRST_BATCH + 4, ACROPOLIS_BRIDGE_KEYPAD_ONES_FIRST_BATCH + 5,
      ACROPOLIS_BRIDGE_KEYPAD_ONES_FIRST_BATCH + 6, ACROPOLIS_BRIDGE_KEYPAD_ONES_FIRST_BATCH + 7,
      ACROPOLIS_BRIDGE_KEYPAD_ONES_FIRST_BATCH + 8, ACROPOLIS_BRIDGE_KEYPAD_ONES_FIRST_BATCH + 9,
      ACROPOLIS_BRIDGE_KEYPAD_ONES_BLANK_BATCH, ACROPOLIS_BRIDGE_KEYPAD_ONES_BLANK_BATCH,
      ACROPOLIS_BRIDGE_KEYPAD_ONES_BLANK_BATCH, ACROPOLIS_BRIDGE_KEYPAD_ONES_BLANK_BATCH,
      ACROPOLIS_BRIDGE_KEYPAD_ONES_BLANK_BATCH, ACROPOLIS_BRIDGE_KEYPAD_ONES_BLANK_BATCH },
};

/// Arrival-dust message receiver for the room's ambient effect task.
///
/// Payload words are ignored and the handler returns zero. The end marker
/// rejects other messages; the live task borrows this table from the overlay.
static TaskMessageEntry _gAcropolisBridgeAmbientMessageTable[2] = {
    { ACROPOLIS_BRIDGE_EFFECT_MESSAGE_ARRIVAL_DUST, _acropolisBridgeSpawnArrivalDust },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// X/Z unit signs of the horizontal ground-glow quad, in GPU strip order.
///
/// The drawer scales these four corners in local game coordinates before its
/// retained origin replacement. Storage is read-only and lives with this overlay.
static EffectUnitQuadCorner _gAcropolisBridgeGroundGlowUnitCorners[4] = {
    { -1, 1 },
    { 1, 1 },
    { -1, -1 },
    { 1, -1 },
};

/// Bits for mapped camera views in the room's glow-emission masks.
enum {
    ACROPOLIS_BRIDGE_GLOW_VIEW_2  = 1 << (2 - 1),
    ACROPOLIS_BRIDGE_GLOW_VIEW_3  = 1 << (3 - 1),
    ACROPOLIS_BRIDGE_GLOW_VIEW_4  = 1 << (4 - 1),
    ACROPOLIS_BRIDGE_GLOW_VIEW_5  = 1 << (5 - 1),
    ACROPOLIS_BRIDGE_GLOW_VIEW_6  = 1 << (6 - 1),
    ACROPOLIS_BRIDGE_GLOW_VIEW_7  = 1 << (7 - 1),
    ACROPOLIS_BRIDGE_GLOW_VIEW_9  = 1 << (9 - 1),
    ACROPOLIS_BRIDGE_GLOW_VIEW_10 = 1 << (10 - 1)
};

/// Framebuffer-streak families selected by the ambient task's mapped camera.
enum {
    ACROPOLIS_BRIDGE_STREAK_NONE      = 0,
    ACROPOLIS_BRIDGE_STREAK_FALLING   = 1,
    ACROPOLIS_BRIDGE_STREAK_MID_DUST  = 2,
    ACROPOLIS_BRIDGE_STREAK_LOW_DUST  = 3,
    ACROPOLIS_BRIDGE_STREAK_TALL_DUST = 5,
    ACROPOLIS_BRIDGE_STREAK_PARTICLE  = 6
};

#define ROOM_PART _gAcropolisBridgeStarGlowOffsets
#include "assets/rooms/akropolis/bridge/markers.inc"
#define ROOM_PART _gAcropolisBridgeStarGlowViewMasks
#include "assets/rooms/akropolis/bridge/view_masks.inc"

/// Maps a one-based camera view to its framebuffer-streak family.
///
/// Indexed by mapped view - 1; the room maps views only into 1..10. The six
/// trailing stored zeroes are outside that domain. Selectors remain u16;
/// NONE disables emission and the other values select the named families.
static u16 _gAcropolisBridgeViewStreakStyles[16] = {
    ACROPOLIS_BRIDGE_STREAK_NONE,
    ACROPOLIS_BRIDGE_STREAK_LOW_DUST,
    ACROPOLIS_BRIDGE_STREAK_NONE,
    ACROPOLIS_BRIDGE_STREAK_TALL_DUST,
    ACROPOLIS_BRIDGE_STREAK_PARTICLE,
    ACROPOLIS_BRIDGE_STREAK_FALLING,
    ACROPOLIS_BRIDGE_STREAK_MID_DUST,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

#define ROOM_PART _gAcropolisBridgeLampGlowViewMasks
#include "assets/rooms/akropolis/bridge/view_masks.inc"
#define ROOM_PART _gAcropolisBridgePlayerWaterPartPositions
#include "assets/rooms/akropolis/bridge/markers.inc"
#define ROOM_PART gAcropolisBridgeRoomCollisionResources
#include "assets/rooms/akropolis/bridge/header.inc"
u8 gAcropolisBridgeStairSurfaceRows[2][12] = {
    { 2, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
};
/// Cue-2 bases in the loaded type-1 bank; cue 1 uses the following entry.
enum {
    ACROPOLIS_BRIDGE_SURFACE_2_WALK_CUE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x25,
    ACROPOLIS_BRIDGE_SURFACE_2_RUN_CUE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x27,
    ACROPOLIS_BRIDGE_SURFACE_4_WALK_CUE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x15,
    ACROPOLIS_BRIDGE_SURFACE_4_RUN_CUE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x17,
    ACROPOLIS_BRIDGE_SURFACE_4_JUMP_CUE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x19,
};

#define ROOM_PART gAcropolisBridgeWarpEntries
#include "assets/rooms/akropolis/bridge/header.inc"
#include "assets/rooms/akropolis/bridge/geometry.inc"
#include "assets/rooms/akropolis/bridge/view_boundaries.inc"
#include "assets/rooms/akropolis/bridge/action_triggers.inc"
#include "assets/rooms/akropolis/bridge/sprites.inc"
#include "assets/rooms/akropolis/bridge/placements.inc"
#include "assets/rooms/akropolis/bridge/lights.inc"
#include "assets/rooms/akropolis/bridge/cameras.inc"
#include "assets/rooms/akropolis/bridge/vibrations.inc"
#define ROOM_PART _gAcropolisBridgeSurface2FootstepSounds
#include "assets/rooms/akropolis/bridge/surfaces.inc"

/// Three retained type-1 sound-request words with no established consumer or role.
///
/// Their values match the surface-2 walk/run/walk cues, but no pointer or
/// relocation identifies them as a footstep record. Preserve this separate storage.
static s32 D_acropolis_bridge_80190C00[3] = {
    ACROPOLIS_BRIDGE_SURFACE_2_WALK_CUE,
    ACROPOLIS_BRIDGE_SURFACE_2_RUN_CUE,
    ACROPOLIS_BRIDGE_SURFACE_2_WALK_CUE,
};

/// Map-marker nibble IDs attached to the bridge's two warp endpoints.
enum {
    ACROPOLIS_BRIDGE_PROMENADE_WARP_MAP_FLAG   = 491,
    ACROPOLIS_BRIDGE_FIRE_ESCAPE_WARP_MAP_FLAG = 490
};

/// Packed five-step stair selectors and the descent bit in the trigger control.
///
/// Before the movie, selector 0x85 uses indexed surface flight 0; afterwards
/// selector 0x45 applies surface class 4. Facing bytes store yaw / 16.
enum {
    ACROPOLIS_BRIDGE_STAIR_DESCEND        = 0x100,
    ACROPOLIS_BRIDGE_STAIR_ROOM1_SELECTOR = 0x80 | 5,
    ACROPOLIS_BRIDGE_STAIR_ROOM2_SELECTOR = (4 << 4) | 5
};

/// Encoded 8-bit texture pages, palette rows and modulation for bridge sprites.
enum {
    ACROPOLIS_BRIDGE_SPRITE_TEXTURE_PAGE_0 = getTPage(1, GPU_BLEND_AVERAGE, 768, 0),
    ACROPOLIS_BRIDGE_SPRITE_TEXTURE_PAGE_1 = getTPage(1, GPU_BLEND_AVERAGE, 832, 0),
    ACROPOLIS_BRIDGE_SPRITE_TEXTURE_PAGE_2 = getTPage(1, GPU_BLEND_AVERAGE, 896, 0),
    ACROPOLIS_BRIDGE_SPRITE_TEXTURE_PAGE_3 = getTPage(1, GPU_BLEND_AVERAGE, 960, 0),
    ACROPOLIS_BRIDGE_SPRITE_CLUT_0         = getClut(0, 255),
    ACROPOLIS_BRIDGE_SPRITE_CLUT_1         = getClut(0, 256),
    ACROPOLIS_BRIDGE_SPRITE_NEUTRAL_RGB    = 128,
    ACROPOLIS_BRIDGE_SPRITE_OPAQUE         = 0,
};

/// Segment indices, repeat count and frame gap in the keypad movie's vibration script.
enum {
    ACROPOLIS_BRIDGE_KEYPAD_MOVIE_VIBRATION_FALL         = 0,
    ACROPOLIS_BRIDGE_KEYPAD_MOVIE_VIBRATION_RISE         = 1,
    ACROPOLIS_BRIDGE_KEYPAD_MOVIE_VIBRATION_HOLD         = 2,
    ACROPOLIS_BRIDGE_KEYPAD_MOVIE_VIBRATION_RELEASE      = 3,
    ACROPOLIS_BRIDGE_KEYPAD_MOVIE_VIBRATION_BINARY_PULSE = 4,
    ACROPOLIS_BRIDGE_KEYPAD_MOVIE_VIBRATION_FINAL_FADE   = 5,
    ACROPOLIS_BRIDGE_KEYPAD_MOVIE_VIBRATION_REPEATS      = 85,
    ACROPOLIS_BRIDGE_KEYPAD_MOVIE_VIBRATION_GAP_FRAMES   = 1
};

/// Segment indices and binary-lane repeat count in the arrival vibration script.
enum {
    ACROPOLIS_BRIDGE_ARRIVAL_VIBRATION_PULSE      = 0,
    ACROPOLIS_BRIDGE_ARRIVAL_VIBRATION_RISE       = 1,
    ACROPOLIS_BRIDGE_ARRIVAL_VIBRATION_PEAK       = 2,
    ACROPOLIS_BRIDGE_ARRIVAL_VIBRATION_FALL       = 3,
    ACROPOLIS_BRIDGE_ARRIVAL_VIBRATION_FINAL_HOLD = 4,
    ACROPOLIS_BRIDGE_ARRIVAL_VIBRATION_REPEATS    = 3
};

/// Victim reaction packed into the bridge enemy's category-4 contact key.
enum { ACROPOLIS_BRIDGE_ENEMY_ATTACK_REACTION_POISON = 3 };

#define ROOM_PART _gAcropolisBridgeBlockingSurfaceProperties
#include "assets/rooms/akropolis/bridge/surfaces.inc"

/// Attack rows shared by all bridge enemies.
///
/// Attack index 0 has base power 18 and applies the player's poison reaction.
/// The second all-zero row has no established consumer; it is an attack record,
/// not a sentinel. Both rows remain live with their shared parameter record.
static DamageAttack _gAcropolisBridgeEnemyAttacks[] = {
    { 18, ACROPOLIS_BRIDGE_ENEMY_ATTACK_REACTION_POISON },
    { 0, 0 },
};

/// Shared attack, HP, reward and hit-response parameters for bridge enemies.
///
/// Setup and the accepted-keypad command read the configured 80 HP; setup's
/// later assignment still reduces the live enemy to 1 HP. All instances borrow
/// this writable record and its attack rows while the bridge overlay is loaded.
static EnemyParams _gAcropolisBridgeEnemyParams = {
    .attacks              = _gAcropolisBridgeEnemyAttacks,
    .hpMax                = 80,
    .exp                  = 6,
    .bp                   = 36,
    .mp                   = 1,
    .critChance           = 100,
    .buildupSteps         = 0,
    .damageOverTimeChance = 100,
    .damageOverTimeTicks  = 0,
};

static TmdBone _gAcropolisBridgeModel13870Skeleton[4] = {
#include "assets/acropolis_bridge_model_13870_skeleton.inc"
};

static u32 _gAcropolisBridgeModel13870PartVerts[4] = {
#include "assets/acropolis_bridge_model_13870_partVerts.inc"
};

static SVECTOR _gAcropolisBridgeModel13870Verts[18] = {
#include "assets/acropolis_bridge_model_13870_verts.inc"
};

static SVECTOR _gAcropolisBridgeModel13870Normals[18] = {
#include "assets/acropolis_bridge_model_13870_normals.inc"
};

static u32 _gAcropolisBridgeModel13870Stream[220] = {
#include "assets/acropolis_bridge_model_13870_stream.inc"
};

static TmdSource _gAcropolisBridgeModel13870 = {
    0,
    1008,
    416,
    4,
    _gAcropolisBridgeModel13870PartVerts,
    _gAcropolisBridgeModel13870Verts,
    _gAcropolisBridgeModel13870Normals,
    _gAcropolisBridgeModel13870Skeleton,
    _gAcropolisBridgeModel13870Stream,
};

static AnimationPackedPose _gAcropolisBridgeAnimation13CD8Bank1[6] = {
#include "assets/acropolis_bridge_animation_13CD8_bank1.inc"
};

static AnimationPackedRotation _gAcropolisBridgeAnimation13CD8Bank4[9] = {
#include "assets/acropolis_bridge_animation_13CD8_bank4.inc"
};

static AnimationRecord _gAcropolisBridgeAnimation13CD8Records[25] = {
#include "assets/acropolis_bridge_animation_13CD8_records.inc"
};

static u16 _gAcropolisBridgeAnimation13CD8Indices[4] = {
#include "assets/acropolis_bridge_animation_13CD8_indices.inc"
};

static AnimationSet _gAcropolisBridgeAnimation13CD8 = {
    _gAcropolisBridgeAnimation13CD8Records,
    _gAcropolisBridgeAnimation13CD8Indices,
    { NULL, _gAcropolisBridgeAnimation13CD8Bank1, NULL, NULL, _gAcropolisBridgeAnimation13CD8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisBridgeAnimation13DD8Bank1[6] = {
#include "assets/acropolis_bridge_animation_13DD8_bank1.inc"
};

static AnimationPackedRotation _gAcropolisBridgeAnimation13DD8Bank4[9] = {
#include "assets/acropolis_bridge_animation_13DD8_bank4.inc"
};

static AnimationRecord _gAcropolisBridgeAnimation13DD8Records[25] = {
#include "assets/acropolis_bridge_animation_13DD8_records.inc"
};

static u16 _gAcropolisBridgeAnimation13DD8Indices[4] = {
#include "assets/acropolis_bridge_animation_13DD8_indices.inc"
};

static AnimationSet _gAcropolisBridgeAnimation13DD8 = {
    _gAcropolisBridgeAnimation13DD8Records,
    _gAcropolisBridgeAnimation13DD8Indices,
    { NULL, _gAcropolisBridgeAnimation13DD8Bank1, NULL, NULL, _gAcropolisBridgeAnimation13DD8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisBridgeAnimation13E48Bank1[2] = {
#include "assets/acropolis_bridge_animation_13E48_bank1.inc"
};

static AnimationPackedRotation _gAcropolisBridgeAnimation13E48Bank4[2] = {
#include "assets/acropolis_bridge_animation_13E48_bank4.inc"
};

static AnimationRecord _gAcropolisBridgeAnimation13E48Records[8] = {
#include "assets/acropolis_bridge_animation_13E48_records.inc"
};

static u16 _gAcropolisBridgeAnimation13E48Indices[4] = {
#include "assets/acropolis_bridge_animation_13E48_indices.inc"
};

static AnimationSet _gAcropolisBridgeAnimation13E48 = {
    _gAcropolisBridgeAnimation13E48Records,
    _gAcropolisBridgeAnimation13E48Indices,
    { NULL, _gAcropolisBridgeAnimation13E48Bank1, NULL, NULL, _gAcropolisBridgeAnimation13E48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisBridgeAnimation13F60Bank1[6] = {
#include "assets/acropolis_bridge_animation_13F60_bank1.inc"
};

static AnimationPackedRotation _gAcropolisBridgeAnimation13F60Bank4[10] = {
#include "assets/acropolis_bridge_animation_13F60_bank4.inc"
};

static AnimationRecord _gAcropolisBridgeAnimation13F60Records[30] = {
#include "assets/acropolis_bridge_animation_13F60_records.inc"
};

static u16 _gAcropolisBridgeAnimation13F60Indices[4] = {
#include "assets/acropolis_bridge_animation_13F60_indices.inc"
};

static AnimationSet _gAcropolisBridgeAnimation13F60 = {
    _gAcropolisBridgeAnimation13F60Records,
    _gAcropolisBridgeAnimation13F60Indices,
    { NULL, _gAcropolisBridgeAnimation13F60Bank1, NULL, NULL, _gAcropolisBridgeAnimation13F60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisBridgeAnimation13FE0Bank1[2] = {
#include "assets/acropolis_bridge_animation_13FE0_bank1.inc"
};

static AnimationPackedRotation _gAcropolisBridgeAnimation13FE0Bank4[2] = {
#include "assets/acropolis_bridge_animation_13FE0_bank4.inc"
};

static AnimationRecord _gAcropolisBridgeAnimation13FE0Records[12] = {
#include "assets/acropolis_bridge_animation_13FE0_records.inc"
};

static u16 _gAcropolisBridgeAnimation13FE0Indices[4] = {
#include "assets/acropolis_bridge_animation_13FE0_indices.inc"

};

static AnimationSet _gAcropolisBridgeAnimation13FE0 = {
    _gAcropolisBridgeAnimation13FE0Records,
    _gAcropolisBridgeAnimation13FE0Indices,
    { NULL, _gAcropolisBridgeAnimation13FE0Bank1, NULL, NULL, _gAcropolisBridgeAnimation13FE0Bank4, NULL, NULL, NULL },
};

/// Animation-set bank borrowed by every embedded bridge enemy's rig.
///
/// Slots 1..5 are patrol, chase, bob, reaction and collapse; slot 0 and the
/// retained slot 6 are NULL. The enemy driver plays three tracks from the
/// selected set. Keep the room's sets and streams loaded while enemy animation
/// contexts reference them.
static AnimationSet* _gAcropolisBridgeEnemyAnimationSets[7] = {
    NULL,
    &_gAcropolisBridgeAnimation13CD8,
    &_gAcropolisBridgeAnimation13DD8,
    &_gAcropolisBridgeAnimation13E48,
    &_gAcropolisBridgeAnimation13F60,
    &_gAcropolisBridgeAnimation13FE0,
    NULL,
};

/// Enemy animation transition durations, indexed by previous clip then new clip.
///
/// Both clip indices must be 0..5. Values count whole normal-rate frames:
/// 1 between patrol/chase, 8 from either into bob, and 0 for the other pairs.
/// The same duration starts each of the three driven animation slots.
static s16 _gAcropolisBridgeEnemyAnimationBlendFrames[6][6] = {
    { 0, 0, 0, 0, 0, 0 },
    { 0, 0, 1, 8, 0, 0 },
    { 0, 1, 0, 8, 0, 0 },
    { 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0 },
};

/// Twenty world-coordinate waypoints for the bridge's Boss Stranger walkers.
///
/// XYZ use signed game-coordinate units. Nearest-node scans examine only
/// nodes 0..9, while the authored patrol routes select through node 19.
/// Each record's final two zero bytes remain uninterpreted. Walkers borrow
/// this room-owned table for their entire lifetime.
static BossStrangerNode _gAcropolisBridgeEnemyPatrolNodes[20] = {
    { -0x376E, 1000, 600, { 0, 0 } },
    { -0x3962, 1000, -900, { 0, 0 } },
    { -0x376E, 1000, -2900, { 0, 0 } },
    { -0x3962, 1000, -5000, { 0, 0 } },
    { -0x2F58, 1000, 500, { 0, 0 } },
    { -0x2D64, 1000, -1100, { 0, 0 } },
    { -0x2F58, 1000, -2700, { 0, 0 } },
    { -0x2D64, 1000, -4000, { 0, 0 } },
    { -8475, 1000, 600, { 0, 0 } },
    { -8975, 1000, -900, { 0, 0 } },
    { -8475, 1000, -2900, { 0, 0 } },
    { -8975, 1000, -5000, { 0, 0 } },
    { -6091, 1000, 500, { 0, 0 } },
    { -5591, 1000, -1100, { 0, 0 } },
    { -6091, 1000, -2700, { 0, 0 } },
    { -5591, 1000, -4000, { 0, 0 } },
    { -3943, 1000, 600, { 0, 0 } },
    { -4243, 1000, -900, { 0, 0 } },
    { -3942, 1000, -2900, { 0, 0 } },
    { -4243, 1000, -5000, { 0, 0 } },
};

/// Node order used when a bridge walker replans toward the player.
///
/// The ten live entries visit nodes 1..9 then 0; there is no end marker.
/// The final two stored zeros are outside orderCount and have no proven role.
/// This is independent of each placement's end-marked patrol route.
static u8 _gAcropolisBridgeEnemyNavNodeOrder[12] = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    0,
    0,
    0,
};

/// Offset at which placement 1 starts in the stream shared with placement 0.
enum { ACROPOLIS_BRIDGE_ENEMY_ROUTE1_OFFSET = 8 };

/// Shared patrol-index stream for bridge enemy placements 0 and 1.
///
/// Placement 0 visits fourteen nodes, including the four consecutive zero
/// indices, before the end marker. Placement 1 starts at byte 8 and visits
/// six nodes, sharing that same marker. The byte after the marker is retained
/// storage with no proven role. Indices refer to the twenty patrol nodes.
static u8 _gAcropolisBridgeEnemyRoute0And1Nodes[16] = {
    8,
    0,
    1,
    6,
    0,
    0,
    0,
    0,
    3,
    2,
    5,
    9,
    11,
    4,
    BOSS_STRANGER_ROUTE_END,
    0,
};

/// Placement 2's four-node patrol and end marker; trailing zeros are unread.
static u8 _gAcropolisBridgeEnemyRoute2Nodes[8] = {
    0,
    7,
    10,
    14,
    BOSS_STRANGER_ROUTE_END,
    0,
    0,
    0,
};

/// Placement 3's five-node patrol and end marker; trailing zeros are unread.
static u8 _gAcropolisBridgeEnemyRoute3Nodes[8] = {
    4,
    8,
    9,
    6,
    7,
    BOSS_STRANGER_ROUTE_END,
    0,
    0,
};

/// Placement 4's eight-node patrol and end marker; trailing zeros are unread.
static u8 _gAcropolisBridgeEnemyRoute4Nodes[12] = {
    19,
    15,
    14,
    17,
    16,
    12,
    13,
    18,
    BOSS_STRANGER_ROUTE_END,
    0,
    0,
    0,
};

/// Placement 5's six-node patrol and end marker; the trailing zero is unread.
static u8 _gAcropolisBridgeEnemyRoute5Nodes[8] = {
    11,
    9,
    13,
    12,
    5,
    10,
    BOSS_STRANGER_ROUTE_END,
    0,
};

/// Placement 6's five-node patrol; no current spawn selects it. Trailing zeros are unread.
static u8 _gAcropolisBridgeEnemyRoute6Nodes[8] = {
    7,
    6,
    9,
    10,
    11,
    BOSS_STRANGER_ROUTE_END,
    0,
    0,
};

/// Placement 7's three-node patrol and end marker; no current spawn selects it.
static u8 _gAcropolisBridgeEnemyRoute7Nodes[4] = {
    14,
    13,
    6,
    BOSS_STRANGER_ROUTE_END,
};

/// Placement 8's six-node patrol; no current spawn selects it. The trailing zero is unread.
static u8 _gAcropolisBridgeEnemyRoute8Nodes[8] = {
    4,
    8,
    9,
    11,
    10,
    5,
    BOSS_STRANGER_ROUTE_END,
    0,
};

/// Patrol streams indexed by the enemy's placement index, which must be 0..8.
///
/// Default placements use 0..5. Every stream ends at BOSS_STRANGER_ROUTE_END
/// and names nodes 0..19; placements 0 and 1 share the first stream's suffix.
/// Walker route records borrow these pointers while the room remains loaded.
static u8* _gAcropolisBridgeEnemyRoutesByPlacement[9] = {
    _gAcropolisBridgeEnemyRoute0And1Nodes,
    &_gAcropolisBridgeEnemyRoute0And1Nodes[ACROPOLIS_BRIDGE_ENEMY_ROUTE1_OFFSET],
    _gAcropolisBridgeEnemyRoute2Nodes,
    _gAcropolisBridgeEnemyRoute3Nodes,
    _gAcropolisBridgeEnemyRoute4Nodes,
    _gAcropolisBridgeEnemyRoute5Nodes,
    _gAcropolisBridgeEnemyRoute6Nodes,
    _gAcropolisBridgeEnemyRoute7Nodes,
    _gAcropolisBridgeEnemyRoute8Nodes,
};

/// Bridge-enemy command and model-visibility messages, followed by the end marker.
///
/// APPLY borrows an ActorCommand; SET_MODEL_DRAW takes an integer visibility
/// word. The enemy task borrows this read-only table while the room is loaded.
static TaskMessageEntry _gAcropolisBridgeEnemyMessageTable[3] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, _acropolisBridgeEnemyApplyCommand },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _acropolisBridgeEnemySetModelDraw },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Nine bridge-enemy behavior handlers indexed by ACROPOLIS_BRIDGE_ENEMY_STATE_*.
///
/// INACTIVE through ANIMATE_AT_PLACEMENT occupy slots 0..8, with no end marker.
/// The frame dispatcher calls the selected handler directly on its live enemy
/// task; the room and callback code must remain loaded throughout dispatch.
static TaskFunc _gAcropolisBridgeEnemyStateHandlers[9] = {
    _acropolisBridgeEnemyInactive,
    _acropolisBridgeEnemyPatrolSunk,
    _acropolisBridgeEnemyChase,
    _acropolisBridgeEnemyRetreat,
    _acropolisBridgeEnemyFollowMovie,
    _acropolisBridgeEnemyDefeatedBob,
    _acropolisBridgeEnemyCollapse,
    _acropolisBridgeEnemyDeathEffect,
    _acropolisBridgeEnemyAnimateAtPlacement,
};

/// Scheduler priority of the bridge enemy's TMD task recipe.
enum { ACROPOLIS_BRIDGE_ENEMY_TASK_PRIORITY = 96 };

/// One bridge-enemy spawn recipe with its retained, uninterpreted storage tail.
///
/// Default area resources borrow `desc` at task index zero. It runs the enemy
/// callback on the embedded model; the overlay must remain loaded throughout
/// the task's lifetime. The following eight zero bytes have no established
/// consumer or ownership and remain represented by the existing storage type.
static _AcropolisBridgeEnemyTaskDescStorage _gAcropolisBridgeEnemyTaskDescStorage = {
    .desc      = { { { TASK_BODY_TMD, ACROPOLIS_BRIDGE_ENEMY_TASK_PRIORITY } }, _acropolisBridgeEnemyTask, { .model = &_gAcropolisBridgeModel13870 } },
    .unknown_C = { 0 },
};

/// Last bridge-model task handle produced by room setup, or NULL on spawn failure.
///
/// The room publishes this borrowed handle but never reads or clears it.
/// Model teardown can invalidate it; the handle owns no task lifetime.
static Task* _gAcropolisBridgeModelTask = NULL;

/// Keypad task handle borrowed by the sequence's result-wait state.
///
/// Set by keypad spawning and valid until taskPollKill succeeds. That poll
/// can release the task; the stored pointer is then stale and must not be used.
static Task* _gAcropolisBridgeKeypadTask = NULL;

/// Movie task handle borrowed by the accepted-code sequence's completion state.
///
/// Set by movie spawning and valid until taskPollKill succeeds. The pointer
/// is retained after polling but may then address released task storage.
static Task* _gAcropolisBridgeKeypadMovieTask = NULL;

/// Collected keypad exit word: 0 cancelled/failed, 1 accepted.
///
/// Written only by a successful keypad exit poll, then read by the next
/// sequence state to restore control or continue to the movie. It is separate
/// from the keypad's live exit-result accumulator and retained across spawns.
static s32 _gAcropolisBridgeKeypadResult = ACROPOLIS_BRIDGE_KEYPAD_CANCELLED;

/// Movie frame published to enemy presentation, with two retained bytes after it.
///
/// The movie task writes `movieFrame` on every non-exiting update, including its
/// post-playback delay. It must stay below 189 for the height-path lookup.
/// The retained bytes have no established format, role or separate owner.
static _AcropolisBridgeMovieFrameStorage _gAcropolisBridgeMovieFrameStorage = {
    .movieFrame = 0,
    .unknown_2  = { 0x22, 0xF2 },
};

static void        _acropolisBridgeOffsetArrivalSpriteFrame(s32 frame, s8 offsetX, s8 offsetY);
static inline void _acropolisBridgeSetCollisionBodyPosition(WorldCollisionBody* body, const SVECTOR3* position);
static inline void _acropolisBridgeInitWalkerScale(BossStrangerWalker* walker);
static inline void _acropolisBridgeLightModel(Task* task);
static inline void _acropolisBridgeResetEnemyScaleOnEntry(_AcropolisBridgeEnemyWork* work);
static inline void _acropolisBridgeResetEnemyScaleDuringShrink(_AcropolisBridgeEnemyWork* work);
static inline void _acropolisBridgeGrowEnemyScale(_AcropolisBridgeEnemyWork* work);
static inline s16  _acropolisBridgeAttackLanded(const Task* task);
static inline s32  _acropolisBridgeEnemyHasPlayerContact(const WorldCollisionContact* contacts);
/// Base requests for the bridge enemy's fall animation and player-body contact.
/// The positional helper adds the placement index in the sound instance byte.
enum {
    ACROPOLIS_BRIDGE_ENEMY_CONTACT_SOUND        = SOUND_CHARACTER(0x29, 2),
    ACROPOLIS_BRIDGE_ENEMY_FALL_ANIMATION_SOUND = SOUND_CHARACTER(0x29, 3)
};

static inline void _acropolisBridgePlayEnemySound(Task* task, Enemy* enemy, s32 baseSoundId);
static void        _acropolisBridgeEnemyApplyHit(Task* task, u32 attackKey);
static void        _acropolisBridgeEnemyRelightModel(Task* task, s16 recomposeRoot);

/// Reserves a textured-quad packet for the bridge's ground glow.
///
/// Requires word-aligned space for `sizeof(POLY_FT4)` bytes at `gGpuPrimCursor`.
/// Advances the cursor by one packet without checking capacity and initializes
/// only the DMA length and textured-quad opcode. The caller fills and links it.
/// A clipped glow still consumes the reservation; the packet is borrowed until
/// the frame arena is reused after GPU completion.
static inline POLY_FT4* _acropolisBridgeReserveGroundGlowQuad(void)
{
    POLY_FT4* quad;

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    return quad;
}

/// Resolves bridge departures and starts the first fire-escape arrival scene.
///
/// Borrows a complete request and writable reply; they may be the same record.
/// Copies all eight bytes before resolving. An unseen fire-escape departure
/// returns 2, selecting the arrival script instead of a normal transition;
/// execution marks it seen and clears the sanctuary blocker. Other departures
/// return 1, with executing promenade departures selecting room 2. Queries
/// perform no scene, flag or placement changes. Other message words are ignored.
static s32 _acropolisBridgeResolveRoomTransition(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ACROPOLIS_BRIDGE_TRANSITION_ALLOWED        = 1,
        ACROPOLIS_BRIDGE_TRANSITION_ARRIVAL_SCENE  = 2,
        ACROPOLIS_BRIDGE_SANCTUARY_CLEARED_VARIANT = 3,
        ACROPOLIS_BRIDGE_PROMENADE_RETURN_ROOM     = 2
    };
    GameLocationKey sanctuaryLocation;

    *reply = *request;
    if (request->areaId == GAME_AREA_AKROPOLIS_FIRE_ESCAPE) {
        if (gameFlagGetNibble(GAME_FLAG_BRIDGE_ARRIVAL_SCENE_SEEN) == 0) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                gameFlagSetNibble(GAME_FLAG_BRIDGE_ARRIVAL_SCENE_SEEN, 1);
                evsStartScriptWithSkip(_gAcropolisBridgeArrivalScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAcropolisBridgeArrivalSkipScript);
                gameFlagSetNibble(GAME_FLAG_SANCTUARY_BLOCKER_CLEARED, 1);
                sanctuaryLocation.stage = GAME_STAGE_AKROPOLIS;
                sanctuaryLocation.area  = GAME_AREA_AKROPOLIS_SANCTUARY;
                areaSetPlacementVariant(&sanctuaryLocation, ACROPOLIS_BRIDGE_SANCTUARY_CLEARED_VARIANT, AREA_VARIANT_RESET_ALWAYS);
            }
            return ACROPOLIS_BRIDGE_TRANSITION_ARRIVAL_SCENE;
        }
    }
    if ((request->areaId == GAME_AREA_AKROPOLIS_PROMENADE) && (request->queryOnly == ROOM_EVENT_EXECUTE)) {
        reply->roomState = ACROPOLIS_BRIDGE_PROMENADE_RETURN_ROOM;
    }
    return ACROPOLIS_BRIDGE_TRANSITION_ALLOWED;
}

/// Refuses every key-item-use request with result 0, leaving the room untouched.
///
/// The item ID and the other message arguments are ignored; no payload is retained.
static s32 _acropolisBridgeRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return 0;
}

/// Handles the keypad CAP command, starting its entry sequence if needed.
///
/// `commandIndex` is the integer CAP slot; only slot 7 acts. Completed bridge
/// progress selects the slot's completed caption, otherwise the keypad display
/// is blanked and its sequence task is spawned. Always returns 0; the receiver,
/// message ID and second payload are unused. The room's CAP and sprites must be live.
static s32 _acropolisBridgeHandleKeypadCommand(Task* task, s32 messageId, s32 commandIndex, s32 unusedArg)
{
    if (commandIndex == ACROPOLIS_BRIDGE_KEYPAD_COMMAND) {
        if (gameFlagGetNibble(GAME_FLAG_AKROPOLIS_BRIDGE_PROGRESS) == ACROPOLIS_BRIDGE_PROGRESS_KEYPAD_MOVIE_COMPLETE) {
            capStartSequenceSlot(ACROPOLIS_BRIDGE_KEYPAD_COMMAND, ACROPOLIS_BRIDGE_KEYPAD_CAPTION_PLAYBACK_MODE,
                                 ACROPOLIS_BRIDGE_KEYPAD_COMPLETE_CAPTION_VARIANT);
            return 0;
        }
        _acropolisBridgeShowKeypadCode(ACROPOLIS_BRIDGE_KEYPAD_CODE_BLANK, 1);
        taskSpawnFromTable(_gAcropolisBridgeRoomTaskDescriptors, ACROPOLIS_BRIDGE_ROOM_TASK_KEYPAD_SEQUENCE, 0, 0);
    }
    return 0;
}

/// Ignores room-action requests and returns 0 without reading the borrowed request.
static s32 _acropolisBridgeIgnoreRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArg)
{
    return 0;
}

/// Ignores room sound commands and returns 0 without changing sound state.
static s32 _acropolisBridgeIgnoreSoundCommand(Task* task, s32 messageId, s32 soundId, s32 unusedArg)
{
    return 0;
}

/// Room task's setup, message-receiving idle and kill states, indexed by state 0..2.
static const TaskFuncTable3 _gAcropolisBridgeRoomStates = {
    { _acropolisBridgeRoomSetup, _acropolisBridgeRoomIdle, taskKill }
};

/// Bridge-model setup, per-view visibility update and kill states, indexed by state 0..2.
static const TaskFuncTable3 _gAcropolisBridgeModelStates = {
    { _bridgeModelSetup, _acropolisBridgeUpdateModelVisibility, taskKill }
};

/// Dispatches the bridge model's setup, visibility update or teardown.
///
/// Requires a live TMD-bodied task with `state` in 0..2 and the room loaded.
/// Copies the three callback pointers by value before dispatch; teardown may
/// release the task.
static void _acropolisBridgeModelTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gAcropolisBridgeModelStates;
    states.funcs[task->state](task);
}

/// Fourteen states of the caption, keypad and accepted-code movie sequence.
///
/// Slots 0..1 start/wait for CAP; 2 is a one-update advance; 3..5 spawn,
/// collect and handle the keypad result. Slots 6..9 advance once each;
/// 10..12 spawn/wait for the movie and restore presentation, then 13 kills.
/// The dispatcher copies every callback pointer before selecting the state.
static const TaskFuncTable14 _gAcropolisBridgeKeypadSequenceStates = {
    { _acropolisBridgeStartKeypadCaption, _acropolisBridgeWaitForKeypadCaption, _acropolisBridgeAdvanceCutsceneState,
      _acropolisBridgeStartKeypad, _acropolisBridgeWaitForKeypadResult, _acropolisBridgeResumeAfterKeypad,
      _acropolisBridgeAdvanceCutsceneState, _acropolisBridgeAdvanceCutsceneState, _acropolisBridgeAdvanceCutsceneState,
      _acropolisBridgeAdvanceCutsceneState, _acropolisBridgeStartKeypadMovie, _acropolisBridgeFinishKeypadMovie,
      _acropolisBridgeRestoreAfterKeypadMovie, taskKill }
};

/// Keypad screen handlers indexed by ACROPOLIS_BRIDGE_KEYPAD_STATE_* (0..8).
///
/// Runs cursor setup, code entry, the Examine choice, code checking, result
/// blinks and close. The dispatcher copies all nine pointers before selecting
/// one; this is a task-state table, not an event-script command stream.
static const TaskFuncTable9 _gAcropolisBridgeKeypadStates = {
    { _acropolisBridgeKeypadOpen, _acropolisBridgeKeypadArmCursor, _acropolisBridgeKeypadEnterCode,
      _acropolisBridgeKeypadOpenExaminePrompt, _acropolisBridgeKeypadCollectExamineAnswer, _acropolisBridgeKeypadCheckCode,
      _acropolisBridgeKeypadBlinkAcceptedCode, _acropolisBridgeKeypadBlinkError, _acropolisBridgeKeypadClose }
};

/// Runs the caption, keypad and accepted-code movie sequence.
///
/// `task->state` must select one of the fourteen handlers. Cancellation ends
/// the sequence after restoring player control; acceptance plays the movie and
/// selects room 2. The final state restores arrival-sprite visibility and HUD.
static void _acropolisBridgeKeypadSequenceTask(Task* task)
{
    TaskFuncTable14 states;

    states = _gAcropolisBridgeKeypadSequenceStates;
    states.funcs[task->state](task);
}

/// Requests room-effect cancellation and locks attachment actions for the arrival scene.
///
/// Called at the start of the arrival event script; the attachment lock persists
/// until the surrounding scene restores control.
static void _acropolisBridgePrepareArrivalScene(void)
{
    roomEffectRequestCancelAll();
    gAttachmentState.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
}

/// Installs the room receiver, spawns the bridge model and synchronizes arrival sprites.
///
/// Requires the room resources loaded and the room task in its setup state.
/// Publishes the model handle and advances to idle; spawn failure leaves a null
/// handle. Arrival-scene progress is narrowed to its low byte for sprite selection.
static void _acropolisBridgeRoomSetup(Task* task)
{
    task->msgTable = _gAcropolisBridgeRoomMessageTable;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    _gAcropolisBridgeModelTask = taskSpawnFromTable(_gAcropolisBridgeRoomTaskDescriptors, ACROPOLIS_BRIDGE_ROOM_TASK_MODEL, 0, 0);
    task->state                = task->state + 1;
    _acropolisBridgeSetArrivalSpritesHidden(gameFlagGetNibble(GAME_FLAG_BRIDGE_ARRIVAL_SCENE_SEEN) & 0xFF);
}

/// Keeps the room task alive to receive messages after its setup state.
static void _acropolisBridgeRoomIdle(Task* task)
{
    char stackReservation[0x10]; // Preserves the target's otherwise unused 16-byte stack frame.
}

void acropolisBridgeRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gAcropolisBridgeRoomStates;
    states.funcs[task->state](task);
}

#include "../../shared/room/bridge_model_setup.inc.c"

/// Hides the bridge model on mapped camera views 8..10 and invalidates its root transform.
///
/// Replaces the object's flags with the active-draw skip bit or zero. The model
/// and root coordinate belong to the live model `task` and remain writable.
static void _acropolisBridgeUpdateModelVisibility(Task* task)
{
    enum { ACROPOLIS_BRIDGE_MODEL_HIDDEN_FIRST_VIEW = 8,
           ACROPOLIS_BRIDGE_MODEL_HIDDEN_VIEW_COUNT = 3 };
    TmdObject* model;
    GfxCoord*  coord;

    model = task->extra.tmd;
    coord = model->coords;
    if ((u32)(viewGetMappedIndex() - ACROPOLIS_BRIDGE_MODEL_HIDDEN_FIRST_VIEW) < (u32)ACROPOLIS_BRIDGE_MODEL_HIDDEN_VIEW_COUNT) {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        model->flags = 0;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Starts the keypad caption and advances the sequence to wait for CAP.
///
/// Requires the room's relocated CAP table. The playback request's result is
/// ignored; even an already-busy CAP advances this state.
static void _acropolisBridgeStartKeypadCaption(Task* task)
{
    capStartSequenceSlot(ACROPOLIS_BRIDGE_KEYPAD_COMMAND, ACROPOLIS_BRIDGE_KEYPAD_CAPTION_PLAYBACK_MODE,
                         ACROPOLIS_BRIDGE_KEYPAD_CAPTION_VARIANT);
    task->state = task->state + 1;
}

/// Waits for the switch caption, then opens the keypad view under scripted control.
///
/// Advances the live cutscene task once CAP is idle. Hides the HUD and player
/// model and holds player control while the following state starts the keypad.
static void _acropolisBridgeWaitForKeypadCaption(Task* task)
{
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_IS_BUSY, 0, 0) == 0) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = ACROPOLIS_BRIDGE_VIEW_KEYPAD;
        gGameSession->hideHud                                  = 1;
        playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        task->state = task->state + 1;
    }
}

/// Spawns the keypad and publishes its handle for the following result poll.
///
/// Requires the room's keypad bank loaded. Advances the sequence even if spawning
/// fails; the existing caller assumes a live handle when it polls.
static void _acropolisBridgeStartKeypad(Task* task)
{
    enum { ACROPOLIS_BRIDGE_KEYPAD_TASK_BANK  = 2,
           ACROPOLIS_BRIDGE_KEYPAD_TASK_INDEX = 8 };
    Task* keypadTask;

    keypadTask                  = taskSpawn(ACROPOLIS_BRIDGE_KEYPAD_TASK_BANK, ACROPOLIS_BRIDGE_KEYPAD_TASK_INDEX, 0, 0);
    task->state                 = task->state + 1;
    _gAcropolisBridgeKeypadTask = keypadTask;
}

/// Collects the keypad result and selects the overview or bridge movie view.
///
/// Polls the live keypad task once per tick until it requests exit. A cancelled
/// entry restores the overview and HUD; acceptance selects the movie view and
/// synchronously broadcasts the reveal command using a stack-owned payload.
/// The result remains available to the next cutscene state.
static void _acropolisBridgeWaitForKeypadResult(Task* task)
{
    ActorCommand movieCommand = { { { GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_PROMENADE } }, ACROPOLIS_BRIDGE_COMMAND_SHOW_MOVIE_ENEMIES };

    if (taskPollKill(_gAcropolisBridgeKeypadTask, &_gAcropolisBridgeKeypadResult) != 0) {
        if (_gAcropolisBridgeKeypadResult == ACROPOLIS_BRIDGE_KEYPAD_CANCELLED) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = ACROPOLIS_BRIDGE_VIEW_OVERVIEW;
            gGameSession->hideHud                                  = 0;
            task->state                                            = task->state + 1;
        } else {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = ACROPOLIS_BRIDGE_VIEW_KEYPAD_MOVIE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &movieCommand, ACTOR_COMMAND_MESSAGE_APPLY);
            task->state = task->state + 1;
        }
    }
}

/// Shows the player after the keypad and ends a cancelled movie sequence.
///
/// Cancellation resumes player control and kills the cutscene task. Acceptance
/// shows the model but keeps scripted control held and advances toward the movie.
static void _acropolisBridgeResumeAfterKeypad(Task* task)
{
    if (_gAcropolisBridgeKeypadResult == ACROPOLIS_BRIDGE_KEYPAD_CANCELLED) {
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
        playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
        taskKill(task);
        return;
    }
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    task->state += 1;
}

/// Advances the linear bridge cutscene by one state without other effects.
///
/// Used for the single-update steps at slots 2 and 6..9 of the cutscene table.
static void _acropolisBridgeAdvanceCutsceneState(Task* task)
{
    task->state = task->state + 1;
}

/// Spawns the accepted-code movie and advances the sequence to wait for it.
///
/// Publishes the movie task handle without transferring it into a child tree.
/// Requires the movie descriptor and room resources to remain loaded; the next
/// state assumes the spawn succeeded.
static void _acropolisBridgeStartKeypadMovie(Task* sequenceTask)
{
    Task* movieTask = taskSpawnFromTable(&_gAcropolisBridgeKeypadMovieTaskDesc, 0, 0, 0);
    s32   nextState = sequenceTask->state + 1;

    _gAcropolisBridgeKeypadMovieTask = movieTask;
    sequenceTask->state              = nextState;
}

/// Finishes the keypad movie and selects the following bridge room variant.
///
/// Waits for the live movie task to request exit, then shows the placed actors,
/// selects the overview, updates both saved and live room selectors and requests
/// a room-object refresh. Marks bridge progress complete, resumes player control
/// and advances the cutscene; the movie's result value is ignored.
static void _acropolisBridgeFinishKeypadMovie(Task* task)
{
    s32 stackReservation[2]; // Preserves the target's otherwise unused eight-byte stack storage.
    s32 movieResult;

    if (taskPollKill(_gAcropolisBridgeKeypadMovieTask, &movieResult) != 0) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, 1, ACTOR_MESSAGE_SET_MODEL_DRAW);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view      = ACROPOLIS_BRIDGE_VIEW_OVERVIEW;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = ACROPOLIS_BRIDGE_ROOM_AFTER_KEYPAD_MOVIE;
        gGameSession->location.roomState                            = ACROPOLIS_BRIDGE_ROOM_AFTER_KEYPAD_MOVIE;
        gGameSession->roomObjsDirty                                 = 1;
        gameFlagSetNibble(GAME_FLAG_AKROPOLIS_BRIDGE_PROGRESS, ACROPOLIS_BRIDGE_PROGRESS_KEYPAD_MOVIE_COMPLETE);
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
        task->state = task->state + 1;
    }
}

/// Restores arrival-sprite visibility and the HUD after the keypad movie.
///
/// Requires the loaded arrival sprite view. Advances the sequence to its
/// final kill state; player control was resumed by the preceding state.
static void _acropolisBridgeRestoreAfterKeypadMovie(Task* task)
{
    _acropolisBridgeSetArrivalSpritesHidden(gameFlagGetNibble(GAME_FLAG_BRIDGE_ARRIVAL_SCENE_SEEN) & 0xFF);
    gGameSession->hideHud = 0;
    task->state           = task->state + 1;
}

/// Plays the accepted-keypad movie and publishes its frame for enemy presentation.
///
/// Start at state zero with a zero kill countdown. Enqueues the location's
/// stream slot, waits for its first frame before attaching the vibration task,
/// waits for the CD queue to finish, then requests exit after 31 task updates.
/// The three unused command-argument bytes remain uninitialized. The captured
/// frame is updated on every non-exiting tick; child teardown follows the task.
static void _acropolisBridgeKeypadMovieTask(Task* movieTask)
{
    enum {
        ACROPOLIS_BRIDGE_MOVIE_START             = 0,
        ACROPOLIS_BRIDGE_MOVIE_WAIT_FIRST_FRAME  = 1,
        ACROPOLIS_BRIDGE_MOVIE_WAIT_FINISH       = 2,
        ACROPOLIS_BRIDGE_MOVIE_EXIT_DELAY        = 3,
        ACROPOLIS_BRIDGE_MOVIE_EXIT_DELAY_FRAMES = 31
    };
    u8          streamArgs[4];
    CdCmdQueue* queue;
    s16         exitDelayFrames;

    queue = &gCdCmdQueue;
    switch (movieTask->state) {
        case ACROPOLIS_BRIDGE_MOVIE_START:
            queue->movieFrame = 1;
            streamArgs[0]     = streamFindMovieSlot(&gGameSession->location, 0, 0);
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, streamArgs);
            movieTask->state = movieTask->state + 1;
            break;
        case ACROPOLIS_BRIDGE_MOVIE_WAIT_FIRST_FRAME:
            if (queue->movieReady == 0) {
                break;
            }
            taskReparent(movieTask, padScriptSpawn(_gAcropolisBridgeKeypadMovieVibrationCommands, _gAcropolisBridgeKeypadMovieVibrationSegments));
            movieTask->state = movieTask->state + 1;
            break;
        case ACROPOLIS_BRIDGE_MOVIE_WAIT_FINISH:
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                break;
            }
            movieTask->state = movieTask->state + 1;
            break;
        case ACROPOLIS_BRIDGE_MOVIE_EXIT_DELAY:
            exitDelayFrames          = movieTask->killCountdown + 1;
            movieTask->killCountdown = exitDelayFrames;
            if (exitDelayFrames >= ACROPOLIS_BRIDGE_MOVIE_EXIT_DELAY_FRAMES) {
                taskRequestKill(movieTask, 0);
                return;
            }
            break;
    }
    // Publish the STR frame for the enemies, including the post-playback delay.
    _gAcropolisBridgeMovieFrameStorage.movieFrame = queue->movieFrame;
}

/// Returns the enemy presentation's signed root Y sample for the current movie frame.
///
/// Uses the captured STR frame plus one, in game-coordinate units. The keypad
/// movie stops at frame 180, so its captured frames 0..180 select samples 1..181
/// of the 190-record path. The caller subtracts its own root-height offset.
/// The room path and captured frame must remain live.
static s16 _acropolisBridgeGetMovieEnemyHeight(void)
{
    return _gAcropolisBridgeMovieEnemyHeightPath[_gAcropolisBridgeMovieFrameStorage.movieFrame + 1].vy;
}

/// Opens the keypad with owned work and a separately spawned cursor task.
///
/// Requires live action-prompt state and mapped keypad view 8, including
/// batch 35. Allocation failure requests exit with result 0. Success
/// holds menu display and player control, clears hotspot hits and initializes a
/// blank entry; close releases the cursor and teardown releases the work.
/// Cursor spawning is unchecked, so the normal close path requires it to succeed.
static void _acropolisBridgeKeypadOpen(Task* task)
{
    _AcropolisBridgeKeypadWork* work;
    GameLocationKey*            location;
    ActionPromptHotspot*        hotspot;
    SpriteView*                 views;
    s32                         mappedView;

    work = memCalloc(sizeof(_AcropolisBridgeKeypadWork), 0);
    if (work == NULL) {
        taskRequestKill(task, 0);
        return;
    }
    task->spawnArg2.pointer = taskSpawnFromTable(&_gAcropolisBridgeKeypadCursorTaskDesc, 0, 1, 0);
    task->work              = work;
    work->field_0           = 0x14;
    work->code              = ACROPOLIS_BRIDGE_KEYPAD_CODE_BLANK;
    location                = &gGameSession->location;
    task->state++;
    mappedView                                                                     = viewGetMappedIndex();
    views                                                                          = gSpriteAreaTables[location->stage - 1][gGameSession->spriteVariant - 1].areaViews[location->area - 1];
    views[(u8)mappedView - 1].batches[ACROPOLIS_BRIDGE_KEYPAD_PROMPT_BATCH].hidden = 1;
    // Hold the keypad display and control until its close state.
    gGameSession->cutsceneHold = 1;
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
    displayAcquireMenuHold();
    gGameSession->eventState = 1;
    gGameSession->hideHud    = 1;
    for (hotspot = _gAcropolisBridgeKeypadHotspots; hotspot->id != ACTION_PROMPT_HOTSPOT_END; hotspot++) {
        hotspot->hit = 0;
    }
    _gAcropolisBridgeKeypadExitResult = ACROPOLIS_BRIDGE_KEYPAD_CANCELLED;
    _acropolisBridgeShowKeypadCode(work->code, 0);
}

/// Collects keypad digits after Examine, or opens Examine for the selected key.
///
/// Requires initialized keypad work and the sentinel-terminated hotspot table.
/// Confirm shifts digit IDs 0..9 into the low nibble; clear blanks all three.
/// A complete three-digit entry starts checking. Cancel closes with result 0
/// and takes precedence over completion on the same frame. CAP playback stops
/// cursor movement and input for that update.
static void _acropolisBridgeKeypadEnterCode(Task* task)
{
    enum {
        ACROPOLIS_BRIDGE_KEYPAD_CONFIRM_BUTTON      = 0,
        ACROPOLIS_BRIDGE_KEYPAD_CANCEL_BUTTON       = 1,
        ACROPOLIS_BRIDGE_KEYPAD_SHIFTED_DIGITS_MASK = 0xFF0
    };
    _AcropolisBridgeKeypadWork* work    = task->work;
    ActionPromptHotspot*        hotspot = _gAcropolisBridgeKeypadHotspots;
    ActionPrompt*               prompt  = gActionPromptSlots;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (capIsBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    } else {
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
        if (_actionPromptHitTestDefault(hotspot, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
            prompt->mode = ACTION_PROMPT_MODE_HIGHLIGHTED;
            if (prompt->buttons.slots[ACROPOLIS_BRIDGE_KEYPAD_CONFIRM_BUTTON].state == ACTION_PROMPT_BUTTON_PRESSED) {
                while (hotspot->id != ACTION_PROMPT_HOTSPOT_END) {
                    if (hotspot->hit != 0) {
                        // A first confirmed key offers Examine before accepting digits.
                        if (work->keypadExamined == 0) {
                            prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                            prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                            work->selectedKey   = hotspot->id;
                            work->promptKind    = hotspot->promptKind;
                            task->state         = ACROPOLIS_BRIDGE_KEYPAD_STATE_OPEN_EXAMINE;
                            return;
                        }
                        if (hotspot->id == ACROPOLIS_BRIDGE_KEYPAD_KEY_CLEAR) {
                            work->code       = ACROPOLIS_BRIDGE_KEYPAD_CODE_BLANK;
                            work->digitCount = 0;
                        } else {
                            work->code <<= 4;
                            work->code   = (work->code & ACROPOLIS_BRIDGE_KEYPAD_SHIFTED_DIGITS_MASK) | hotspot->id;
                            work->digitCount++;
                        }
                        sndEvtRequestScriptStart(SOUND_ACROPOLIS_BRIDGE_KEYPAD_BEEP, 0, 0);
                        break;
                    }
                    hotspot++;
                }
            }
        } else {
            prompt->mode = ACTION_PROMPT_MODE_NORMAL;
        }
        if (work->digitCount == ACROPOLIS_BRIDGE_KEYPAD_CODE_DIGITS) {
            task->state = ACROPOLIS_BRIDGE_KEYPAD_STATE_CHECK_CODE;
            work->timer = 0;
        }
        // Cancellation wins when completion and cancel occur together.
        if (prompt->buttons.slots[ACROPOLIS_BRIDGE_KEYPAD_CANCEL_BUTTON].state == ACTION_PROMPT_BUTTON_PRESSED) {
            task->state                       = ACROPOLIS_BRIDGE_KEYPAD_STATE_CLOSE;
            _gAcropolisBridgeKeypadExitResult = ACROPOLIS_BRIDGE_KEYPAD_CANCELLED;
        }
    }
    _acropolisBridgeShowKeypadCode(work->code, 0);
}

/// Advances one keypad result blink without resetting entry counters.
///
/// Borrows live work and sprite state. Acceptance blanks then shows the code;
/// rejection shows error then blanks. Two ten-frame halves are followed by one
/// counting update. The caller chooses what three completed blinks do.
/// accepted is zero for rejection and nonzero for acceptance; the final update
/// resets the frame timer and counts a blink without changing the display.
static inline void _acropolisBridgeKeypadStepResultBlink(_AcropolisBridgeKeypadWork* work, s32 accepted)
{
    s16 blinkFrame = work->timer;

    if (blinkFrame < ACROPOLIS_BRIDGE_KEYPAD_BLINK_PHASE_FRAMES) {
        if (accepted) {
            _acropolisBridgeShowKeypadCode(ACROPOLIS_BRIDGE_KEYPAD_CODE_BLANK, 0);
        } else {
            _acropolisBridgeShowKeypadError();
        }
        work->timer++;
    } else if (blinkFrame < 2 * ACROPOLIS_BRIDGE_KEYPAD_BLINK_PHASE_FRAMES) {
        _acropolisBridgeShowKeypadCode(accepted ? work->code : ACROPOLIS_BRIDGE_KEYPAD_CODE_BLANK, 0);
        work->timer++;
    } else {
        work->timer = 0;
        work->blinkCount++;
    }
}

/// Blinks the accepted code, then closes the keypad with result 1.
///
/// Requires live work, prompt state and mapped keypad view 8's batch 35. Each cycle
/// has ten blank frames, ten code frames and a counting frame. Entry retains
/// `timer` and `blinkCount`: the check leaves timer at 10, starting with the
/// code visible, and a previous rejection leaves three blinks, closing this
/// state on its first update. Cursor hotspot feedback remains active.
static void _acropolisBridgeKeypadBlinkAcceptedCode(Task* task)
{
    ActionPrompt*               prompt   = gActionPromptSlots;
    ActionPromptHotspot*        hotspots = _gAcropolisBridgeKeypadHotspots;
    _AcropolisBridgeKeypadWork* work     = task->work;
    GameLocationKey*            location = &gGameSession->location;
    SpriteView*                 views;
    s32                         mappedView;

    mappedView                                                                     = viewGetMappedIndex();
    views                                                                          = gSpriteAreaTables[location->stage - 1][gGameSession->spriteVariant - 1].areaViews[location->area - 1];
    views[(u8)mappedView - 1].batches[ACROPOLIS_BRIDGE_KEYPAD_PROMPT_BATCH].hidden = 0;

    _acropolisBridgeKeypadStepResultBlink(work, 1);

    if (_actionPromptHitTestDefault(hotspots, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HIGHLIGHTED;
    } else {
        prompt->mode = ACTION_PROMPT_MODE_NORMAL;
    }

    if (work->blinkCount == ACROPOLIS_BRIDGE_KEYPAD_BLINK_COUNT) {
        task->state                       = ACROPOLIS_BRIDGE_KEYPAD_STATE_CLOSE;
        _gAcropolisBridgeKeypadExitResult = ACROPOLIS_BRIDGE_KEYPAD_ACCEPTED;
    }
}

/// Blinks the keypad error and retries entry, closing after three rejected codes.
///
/// Requires initialized work and live prompt/display state. Each cycle shows
/// error for ten frames, blanks for ten and then counts a blink. Three cycles
/// clear the entry and increment the byte rejection count. `blinkCount` stays
/// at three on retry; the next rejected check resets it. Cursor hotspot
/// feedback remains active throughout.
static void _acropolisBridgeKeypadBlinkError(Task* task)
{
    ActionPrompt*               prompt   = gActionPromptSlots;
    ActionPromptHotspot*        hotspots = _gAcropolisBridgeKeypadHotspots;
    _AcropolisBridgeKeypadWork* work     = task->work;
    u8                          rejectedCount;

    viewGetMappedIndex();
    _acropolisBridgeKeypadStepResultBlink(work, 0);

    if (_actionPromptHitTestDefault(hotspots, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HIGHLIGHTED;
    } else {
        prompt->mode = ACTION_PROMPT_MODE_NORMAL;
    }

    if (work->blinkCount == ACROPOLIS_BRIDGE_KEYPAD_BLINK_COUNT) {
        task->state         = ACROPOLIS_BRIDGE_KEYPAD_STATE_ENTER_CODE;
        work->digitCount    = 0;
        work->code          = ACROPOLIS_BRIDGE_KEYPAD_CODE_BLANK;
        rejectedCount       = work->rejectedCount + 1;
        work->rejectedCount = rejectedCount;
        if (rejectedCount >= ACROPOLIS_BRIDGE_KEYPAD_REJECTED_LIMIT) {
            task->state                       = ACROPOLIS_BRIDGE_KEYPAD_STATE_CLOSE;
            _gAcropolisBridgeKeypadExitResult = ACROPOLIS_BRIDGE_KEYPAD_CANCELLED;
        }
    }
}

/// Selects the visible sprite for each nibble of the bridge's three-digit code.
///
/// Only bits 0..11 of `code` are used: hundreds, tens and ones index three
/// 16-entry rows, so each lookup is in bounds. Nibbles 0..9 show a digit;
/// 10..15 hide the digit band and show its blank sprite. Requires the eighth
/// view's writable batches 1..35. Always hides the error sprite; a nonzero
/// `hidePrompt` additionally hides batch 35, while zero preserves its visibility.
/// Updates flags for later sprite rendering.
static void _acropolisBridgeShowKeypadCode(s32 code, s16 hidePrompt)
{
    GameLocationKey* location = &gGameSession->location;
    SpriteBatch*     batches;
    s32              batchIndex;
    u8               hundredsBatch;
    u8               tensBatch;
    u8               onesBatch;

    /// Selects a digit or blank sprite in one ten-batch band.
    ///
    /// All arguments must be stable expressions without side effects: batches,
    /// selected digit and first/blank indices are read repeatedly. The selected
    /// batch must be one of the ten digits or the band's separate blank. `batchIndex`
    /// must be a writable s32 local and is left one past the band's last digit.
    /// Captures no caller identifiers. Invoke as a standalone statement inside
    /// a braced caller context: expansion is a complete if/else.
#define ACROPOLIS_BRIDGE_SELECT_KEYPAD_DIGIT(batches, digitBatch, firstBatch, blankBatch, batchIndex)                                \
    if ((digitBatch) == (blankBatch)) {                                                                                              \
        for ((batchIndex) = (firstBatch); (batchIndex) < (firstBatch) + ACROPOLIS_BRIDGE_KEYPAD_DIGIT_BATCH_COUNT; (batchIndex)++) { \
            (batches)[(batchIndex)].hidden = 1;                                                                                      \
        }                                                                                                                            \
        (batches)[(blankBatch)].hidden = 0;                                                                                          \
    } else {                                                                                                                         \
        for ((batchIndex) = (firstBatch); (batchIndex) < (firstBatch) + ACROPOLIS_BRIDGE_KEYPAD_DIGIT_BATCH_COUNT; (batchIndex)++) { \
            if ((digitBatch) == (batchIndex)) {                                                                                      \
                (batches)[(batchIndex)].hidden = 0;                                                                                  \
                (batches)[(blankBatch)].hidden = 1;                                                                                  \
            } else {                                                                                                                 \
                (batches)[(batchIndex)].hidden = 1;                                                                                  \
            }                                                                                                                        \
        }                                                                                                                            \
    }

    viewGetMappedIndex();
    batches = gSpriteAreaTables[location->stage - 1][gGameSession->spriteVariant - 1].areaViews[location->area - 1][ACROPOLIS_BRIDGE_KEYPAD_VIEW_INDEX].batches;

    if (hidePrompt != 0) {
        batches[ACROPOLIS_BRIDGE_KEYPAD_PROMPT_BATCH].hidden = 1;
    }

    hundredsBatch = _gAcropolisBridgeKeypadDigitBatches[0][((u32)code & 0xF00) >> 8];
    tensBatch     = _gAcropolisBridgeKeypadDigitBatches[1][((u32)code & 0xF0) >> 4];
    onesBatch     = _gAcropolisBridgeKeypadDigitBatches[2][code & 0xF];

    ACROPOLIS_BRIDGE_SELECT_KEYPAD_DIGIT(batches, hundredsBatch, ACROPOLIS_BRIDGE_KEYPAD_HUNDREDS_FIRST_BATCH, ACROPOLIS_BRIDGE_KEYPAD_HUNDREDS_BLANK_BATCH, batchIndex);
    ACROPOLIS_BRIDGE_SELECT_KEYPAD_DIGIT(batches, tensBatch, ACROPOLIS_BRIDGE_KEYPAD_TENS_FIRST_BATCH, ACROPOLIS_BRIDGE_KEYPAD_TENS_BLANK_BATCH, batchIndex);
    ACROPOLIS_BRIDGE_SELECT_KEYPAD_DIGIT(batches, onesBatch, ACROPOLIS_BRIDGE_KEYPAD_ONES_FIRST_BATCH, ACROPOLIS_BRIDGE_KEYPAD_ONES_BLANK_BATCH, batchIndex);

#undef ACROPOLIS_BRIDGE_SELECT_KEYPAD_DIGIT

    batches[ACROPOLIS_BRIDGE_KEYPAD_ERROR_BATCH].hidden = 1;
}

/// Hides the three ten-digit bands of the keypad's sprite display.
///
/// `batches` borrows the eighth view's live batch array, including indices 1..30.
static inline void _acropolisBridgeHideKeypadDigitBatches(SpriteBatch* batches)
{
    s32 batchIndex;

    for (batchIndex = ACROPOLIS_BRIDGE_KEYPAD_HUNDREDS_FIRST_BATCH;
         batchIndex < ACROPOLIS_BRIDGE_KEYPAD_HUNDREDS_FIRST_BATCH + ACROPOLIS_BRIDGE_KEYPAD_DIGIT_BATCH_COUNT;
         batchIndex++) {
        batches[batchIndex].hidden = 1;
    }
    for (batchIndex = ACROPOLIS_BRIDGE_KEYPAD_TENS_FIRST_BATCH;
         batchIndex < ACROPOLIS_BRIDGE_KEYPAD_TENS_FIRST_BATCH + ACROPOLIS_BRIDGE_KEYPAD_DIGIT_BATCH_COUNT;
         batchIndex++) {
        batches[batchIndex].hidden = 1;
    }
    for (batchIndex = ACROPOLIS_BRIDGE_KEYPAD_ONES_FIRST_BATCH;
         batchIndex < ACROPOLIS_BRIDGE_KEYPAD_ONES_FIRST_BATCH + ACROPOLIS_BRIDGE_KEYPAD_DIGIT_BATCH_COUNT;
         batchIndex++) {
        batches[batchIndex].hidden = 1;
    }
}

/// Shows the bridge keypad's error display, hiding its digits and blank indicators.
///
/// Requires the active session's loaded eighth sprite view, with batch elements
/// through 34. Changes visibility only; the remaining batches keep their flags.
static void _acropolisBridgeShowKeypadError(void)
{
    GameLocationKey* location = &gGameSession->location;
    SpriteBatch*     batches;

    viewGetMappedIndex();
    batches = gSpriteAreaTables[location->stage - 1][gGameSession->spriteVariant - 1].areaViews[location->area - 1][ACROPOLIS_BRIDGE_KEYPAD_VIEW_INDEX].batches;

    _acropolisBridgeHideKeypadDigitBatches(batches);
    batches[ACROPOLIS_BRIDGE_KEYPAD_ERROR_BATCH].hidden          = 0;
    batches[ACROPOLIS_BRIDGE_KEYPAD_HUNDREDS_BLANK_BATCH].hidden = 1;
    batches[ACROPOLIS_BRIDGE_KEYPAD_TENS_BLANK_BATCH].hidden     = 1;
    batches[ACROPOLIS_BRIDGE_KEYPAD_ONES_BLANK_BATCH].hidden     = 1;
}

#include "../../shared/room/action_prompt_outline_rect.inc.c"

/// Translates one arrival frame's three source rectangles by signed pixel offsets.
///
/// Defined only for the function below. Arguments must be side-effect-free:
/// each is evaluated repeatedly in source order (X then Y per rectangle).
/// `sourceArray` borrows three writable SpriteSource elements starting at the
/// constant `firstSource`; each sum narrows back to the destination's s16.
/// Captures no identifiers, retains no pointer and yields void. Commas preserve
/// the update order without introducing a new local scope.
#define ACROPOLIS_BRIDGE_OFFSET_ARRIVAL_SPRITE_SOURCES(sourceArray, firstSource, xOffset, yOffset) \
    ((void)((sourceArray)[(firstSource)].x0     += (xOffset),                                      \
            (sourceArray)[(firstSource)].y0     += (yOffset),                                      \
            (sourceArray)[(firstSource) + 1].x0 += (xOffset),                                      \
            (sourceArray)[(firstSource) + 1].y0 += (yOffset),                                      \
            (sourceArray)[(firstSource) + 2].x0 += (xOffset),                                      \
            (sourceArray)[(firstSource) + 2].y0 += (yOffset)))

/// Moves and selects one arrival-scene sprite frame in the tenth view.
///
/// The low byte of `frame` selects frame 0..2; other values hide all three and
/// move nothing. Offsets are signed pixels added to each of that frame's three
/// source rectangles, narrowed back to s16. Requires the active session's loaded
/// tenth view with nine sources and batch elements 1..3. Retains no pointer.
static void _acropolisBridgeOffsetArrivalSpriteFrame(s32 frame, s8 offsetX, s8 offsetY)
{
    GameSession*     session  = gGameSession;
    GameLocationKey* location = &session->location;
    SpriteView*      views;
    SpriteSource*    sources;
    SpriteBatch*     batches;
    s32              selectedFrame;

    views         = gSpriteAreaTables[location->stage - 1][session->spriteVariant - 1].areaViews[location->area - 1];
    batches       = views[ACROPOLIS_BRIDGE_ARRIVAL_VIEW_INDEX].batches;
    sources       = views[ACROPOLIS_BRIDGE_ARRIVAL_VIEW_INDEX].sources.elements;
    selectedFrame = frame & 0xFF;

    if (selectedFrame == ACROPOLIS_BRIDGE_ARRIVAL_FRAME_FIRST) {
        ACROPOLIS_BRIDGE_OFFSET_ARRIVAL_SPRITE_SOURCES(sources, 0, offsetX, offsetY);
        batches[ACROPOLIS_BRIDGE_ARRIVAL_BATCH_FIRST].hidden  = 0;
        batches[ACROPOLIS_BRIDGE_ARRIVAL_BATCH_SECOND].hidden = 1;
        batches[ACROPOLIS_BRIDGE_ARRIVAL_BATCH_THIRD].hidden  = 1;
    } else if (selectedFrame == ACROPOLIS_BRIDGE_ARRIVAL_FRAME_SECOND) {
        ACROPOLIS_BRIDGE_OFFSET_ARRIVAL_SPRITE_SOURCES(sources, 3, offsetX, offsetY);
        batches[ACROPOLIS_BRIDGE_ARRIVAL_BATCH_FIRST].hidden  = 1;
        batches[ACROPOLIS_BRIDGE_ARRIVAL_BATCH_SECOND].hidden = 0;
        batches[ACROPOLIS_BRIDGE_ARRIVAL_BATCH_THIRD].hidden  = 1;
    } else if (selectedFrame == ACROPOLIS_BRIDGE_ARRIVAL_FRAME_THIRD) {
        ACROPOLIS_BRIDGE_OFFSET_ARRIVAL_SPRITE_SOURCES(sources, 6, offsetX, offsetY);
        batches[ACROPOLIS_BRIDGE_ARRIVAL_BATCH_FIRST].hidden  = 1;
        batches[ACROPOLIS_BRIDGE_ARRIVAL_BATCH_SECOND].hidden = 1;
        batches[ACROPOLIS_BRIDGE_ARRIVAL_BATCH_THIRD].hidden  = 0;
    } else {
        batches[ACROPOLIS_BRIDGE_ARRIVAL_BATCH_FIRST].hidden  = 1;
        batches[ACROPOLIS_BRIDGE_ARRIVAL_BATCH_SECOND].hidden = 1;
        batches[ACROPOLIS_BRIDGE_ARRIVAL_BATCH_THIRD].hidden  = 1;
    }
}

#undef ACROPOLIS_BRIDGE_OFFSET_ARRIVAL_SPRITE_SOURCES

#include "../../shared/room/action_prompt_move_cursors.inc.c"

#include "../../shared/room/action_prompt_draw_cursor.inc.c"

/// Dispatches keypad cursor setup and the continuing cursor update.
///
/// Requires task state 0 or 1. Setup initializes both gameplay-owned prompts
/// and advances to state 1; updates move, classify and draw the selected ports
/// from `Task::spawnArg1.value` (1 port 0, 2 port 1, otherwise both).
/// The keypad controller owns this task's lifetime; no work block is allocated.
static void _acropolisBridgeKeypadCursorTask(Task* task)
{
    TaskFunc states[] = { _actionPromptResetDefault, _actionPromptMoveCursorsDefault };

    states[task->state](task);
}

/// Hides the two arrival-scene sprite batches when the argument's low byte is nonzero.
///
/// The active session selects this room's loaded sprite views. View-array
/// elements 1 and 4 require batch elements 11 and 16 respectively. A zero
/// low byte shows both batches; only their visibility changes.
static void _acropolisBridgeSetArrivalSpritesHidden(s32 arrivalSceneSeen)
{
    GameSession*     session  = gGameSession;
    GameLocationKey* location = &session->location;
    SpriteView*      views;
    SpriteBatch*     batches;

    views = gSpriteAreaTables[location->stage - 1][session->spriteVariant - 1].areaViews[location->area - 1];

    batches = views[1].batches;
    if ((arrivalSceneSeen & 0xFF) == 0) {
        batches[11].hidden = 0;
    } else {
        batches[11].hidden = 1;
    }

    batches = views[4].batches;
    if ((arrivalSceneSeen & 0xFF) == 0) {
        batches[16].hidden = 0;
    } else {
        batches[16].hidden = 1;
    }
}

/// Selects one arrival-scene sprite frame in the tenth view.
///
/// The argument's low byte selects frame 0..2; other values hide all three.
/// Requires the active session's loaded tenth view with batch elements 1..3.
/// Changes only `SpriteBatch::hidden`; the event script passes a complete word.
static void _acropolisBridgeSelectArrivalSpriteFrame(s32 frame)
{
    GameSession*     session  = gGameSession;
    GameLocationKey* location = &session->location;
    SpriteView*      views;
    SpriteBatch*     batches;
    s32              selectedFrame;

    views         = gSpriteAreaTables[location->stage - 1][session->spriteVariant - 1].areaViews[location->area - 1];
    batches       = views[ACROPOLIS_BRIDGE_ARRIVAL_VIEW_INDEX].batches;
    selectedFrame = frame & 0xFF;

    if (selectedFrame == ACROPOLIS_BRIDGE_ARRIVAL_FRAME_FIRST) {
        batches[ACROPOLIS_BRIDGE_ARRIVAL_BATCH_FIRST].hidden  = 0;
        batches[ACROPOLIS_BRIDGE_ARRIVAL_BATCH_SECOND].hidden = 1;
        batches[ACROPOLIS_BRIDGE_ARRIVAL_BATCH_THIRD].hidden  = 1;
    } else if (selectedFrame == ACROPOLIS_BRIDGE_ARRIVAL_FRAME_SECOND) {
        batches[ACROPOLIS_BRIDGE_ARRIVAL_BATCH_FIRST].hidden  = 1;
        batches[ACROPOLIS_BRIDGE_ARRIVAL_BATCH_SECOND].hidden = 0;
        batches[ACROPOLIS_BRIDGE_ARRIVAL_BATCH_THIRD].hidden  = 1;
    } else if (selectedFrame == ACROPOLIS_BRIDGE_ARRIVAL_FRAME_THIRD) {
        batches[ACROPOLIS_BRIDGE_ARRIVAL_BATCH_FIRST].hidden  = 1;
        batches[ACROPOLIS_BRIDGE_ARRIVAL_BATCH_SECOND].hidden = 1;
        batches[ACROPOLIS_BRIDGE_ARRIVAL_BATCH_THIRD].hidden  = 0;
    } else {
        batches[ACROPOLIS_BRIDGE_ARRIVAL_BATCH_FIRST].hidden  = 1;
        batches[ACROPOLIS_BRIDGE_ARRIVAL_BATCH_SECOND].hidden = 1;
        batches[ACROPOLIS_BRIDGE_ARRIVAL_BATCH_THIRD].hidden  = 1;
    }
}

/// Arms the keypad cursor at pixel (0, 0) and advances to code entry.
///
/// Requires the live gameplay action prompt. Enables aim-speed motion with the
/// idle cursor sprite and blanks the keypad display; button state is retained.
static void _acropolisBridgeKeypadArmCursor(Task* task)
{
    ActionPrompt* prompt = gActionPromptSlots;

    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    prompt->mode        = ACTION_PROMPT_MODE_NORMAL;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    _acropolisBridgeShowKeypadCode(ACROPOLIS_BRIDGE_KEYPAD_CODE_BLANK, 0);
    task->state++;
}

/// Opens Examine at the keypad cursor and advances to collect its answer.
///
/// Requires live keypad work and prompt state. Passes the selected hotspot's
/// command kind and pixel position to the item menu, keeping the entered code
/// visible while cursor movement and rendering are stopped.
static void _acropolisBridgeKeypadOpenExaminePrompt(Task* task)
{
    ActionPrompt*               prompt = gActionPromptSlots;
    _AcropolisBridgeKeypadWork* work   = task->work;

    _acropolisBridgeShowKeypadCode(work->code, 0);
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    itemMenuOpenHotspotCommands(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = ACROPOLIS_BRIDGE_KEYPAD_STATE_WAIT_EXAMINE;
}

/// Collects the keypad's Examine answer and returns to code entry.
///
/// Requires initialized keypad work and the live action prompt. Confirmation
/// enables digit entry and requests CAP slot 9 in the current display, variant
/// 0. Redraws the entered code and keeps cursor motion and drawing suspended;
/// cancellation also returns to entry without clearing the entered digits.
static void _acropolisBridgeKeypadCollectExamineAnswer(Task* task)
{
    enum { EXAMINE_CAP_SLOT            = 9,
           CAP_PLAY_IN_CURRENT_DISPLAY = 0,
           EXAMINE_VARIANT             = 0 };
    ActionPrompt*               prompt = gActionPromptSlots;
    _AcropolisBridgeKeypadWork* work   = task->work;

    _acropolisBridgeShowKeypadCode(work->code, 0);
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    if (itemMenuIsHotspotActionConfirmed() != 0) {
        work->keypadExamined = 1;
        capStartSequenceSlot(EXAMINE_CAP_SLOT, CAP_PLAY_IN_CURRENT_DISPLAY, EXAMINE_VARIANT);
    }
    task->state = ACROPOLIS_BRIDGE_KEYPAD_STATE_ENTER_CODE;
}

/// Checks a complete keypad code after ten updates and starts its result display.
///
/// Requires initialized work with three entered digits. Code 561 broadcasts
/// the accepted command to actors and plays confirmation; other codes play
/// rejection and reset the blink counters. Acceptance retains both counters.
/// The broadcast borrows the stack command only during synchronous dispatch.
/// While waiting for the check delay, this state does not update sprites or
/// cursor feedback.
static void _acropolisBridgeKeypadCheckCode(Task* task)
{
    ActionPrompt*               prompt   = gActionPromptSlots;
    _AcropolisBridgeKeypadWork* work     = task->work;
    ActionPromptHotspot*        hotspots = _gAcropolisBridgeKeypadHotspots;

    if (work->timer < ACROPOLIS_BRIDGE_KEYPAD_CHECK_DELAY_FRAMES) {
        work->timer++;
        return;
    }

    if (work->code != ACROPOLIS_BRIDGE_KEYPAD_CODE_CORRECT) {
        sndEvtRequestScriptStart(SOUND_ACROPOLIS_BRIDGE_CODE_REJECTED, 0, 0);
        work->blinkCount = 0;
        work->timer      = 0;
        task->state      = ACROPOLIS_BRIDGE_KEYPAD_STATE_REJECTED;
    } else {
        // Keep the inherited blink counters on acceptance.
        ActorCommand acceptedCommand = { { { GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_BRIDGE } }, ACROPOLIS_BRIDGE_COMMAND_KEYPAD_ACCEPTED };

        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &acceptedCommand, ACTOR_COMMAND_MESSAGE_APPLY);
        sndEvtRequestScriptStart(SOUND_ACROPOLIS_BRIDGE_CODE_ACCEPTED, 0, 0);
        task->state = ACROPOLIS_BRIDGE_KEYPAD_STATE_ACCEPTED;
    }
    _acropolisBridgeShowKeypadCode(work->code, 0);
    if (_actionPromptHitTestDefault(hotspots, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HIGHLIGHTED;
    } else {
        prompt->mode = ACTION_PROMPT_MODE_NORMAL;
    }
}

/// Closes the keypad, releases its cursor and requests exit with the saved result.
///
/// Requires a live cursor task in `spawnArg2.pointer` and a held menu display.
/// Blanks the code, restores HUD/event/player holds and rearms manual interaction
/// for ten eligible direction updates. Work remains task-owned until teardown;
/// the surrounding sequence restores the player model and scripted control.
static void _acropolisBridgeKeypadClose(Task* task)
{
    enum { ACROPOLIS_BRIDGE_KEYPAD_INTERACTION_REARM_UPDATES = 10 };

    displayReleaseMenuHold();
    _acropolisBridgeShowKeypadCode(ACROPOLIS_BRIDGE_KEYPAD_CODE_BLANK, 0);
    taskKill(task->spawnArg2.pointer);
    taskRequestKill(task, _gAcropolisBridgeKeypadExitResult);
    gGameSession->eventState          = 0;
    gGameSession->hideHud             = 0;
    gGameSession->cutsceneHold        = 0;
    gDirectionInteractionRearmUpdates = ACROPOLIS_BRIDGE_KEYPAD_INTERACTION_REARM_UPDATES;
}

#include "../../shared/room/action_prompt_hit_test.inc.c"
#undef ACTION_PROMPT_HIT_TEST

void acropolisBridgeKeypadTask(Task* task)
{
    TaskFuncTable9 states;

    states = _gAcropolisBridgeKeypadStates;
    states.funcs[task->state](task);
}

#include "../../shared/room/action_prompt_reset.inc.c"

void acropolisBridgeAmbientEffectsTask(Task* task)
{
    enum {
        ACROPOLIS_BRIDGE_AMBIENT_INITIALIZE         = 0,
        ACROPOLIS_BRIDGE_DISC_RADIUS_SCALE          = 256,
        ACROPOLIS_BRIDGE_DISC_WARM_COLOR            = 0x5C20,
        ACROPOLIS_BRIDGE_DISC_COOL_COLOR            = 0x50C2,
        ACROPOLIS_BRIDGE_WATER_VIEW_MASK            = (1 << (2 - 1)) | (1 << (6 - 1)) | (1 << (7 - 1)),
        ACROPOLIS_BRIDGE_WATER_MIN_LOCAL_Y          = 513,
        ACROPOLIS_BRIDGE_WATER_CHANCE_BIAS          = 32,
        ACROPOLIS_BRIDGE_WATER_RANDOM_MASK          = 511,
        ACROPOLIS_BRIDGE_WATER_RIPPLE_HALF_SIZE     = 64,
        ACROPOLIS_BRIDGE_WATER_SPRAY_ARGUMENT       = 0x1202180,
        ACROPOLIS_BRIDGE_PLAYER_FIRST_WATER_PART    = 14,
        ACROPOLIS_BRIDGE_PLAYER_WATER_PART_STRIDE   = 3,
        ACROPOLIS_BRIDGE_VIEW9_STAR_Y_OFFSET        = 576,
        ACROPOLIS_BRIDGE_STREAK_ENTRY_BURST_COUNT   = 30,
        ACROPOLIS_BRIDGE_STREAK_FAST_PHASE_BIT      = 0x200,
        ACROPOLIS_BRIDGE_STREAK_PACKET_BASE_BYTES   = 0xA000,
        ACROPOLIS_BRIDGE_STREAK_PACKET_BUFFER_BYTES = 0x7000
    };
    EffectWork* ambientWork;
    GfxCoord*   effectCoord;
    GfxCoord*   playerPart;
    Task*       playerTask;
    SVECTOR     glowPosition;
    u8          mappedView;
    s32         viewBit;
    s32         index;
    s32         axisDistance;
    s32         axisValue;
    s32         distanceSum;
    s32         previousComponent;
    s16         previousView;
    u16         randomBits;

    ambientWork = task->spawnArg2.pointer;
    effectCoord = task->extra.coordBody->coord;
    playerTask  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    playerPart  = playerTask->extra.tmd->coords;
    mappedView  = viewGetMappedIndex();
    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }

    if (task->state == ACROPOLIS_BRIDGE_AMBIENT_INITIALIZE) {
        gGameSession->field_80 = 0;
        task->msgTable         = _gAcropolisBridgeAmbientMessageTable;
        gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM_EFFECT);
        gRoomEffectWaterRippleId = EFFECT_ACROPOLIS_BRIDGE_WATER_RIPPLE;
        gRoomEffectWaterSprayId  = EFFECT_ACROPOLIS_BRIDGE_WATER_SPRAY;
        task->state              = task->state + 1;
        for (index = 0; index < ARRAY_SIZE(_gAcropolisBridgePlayerWaterPartPositions); index++) {
            playerPart                                          = &playerTask->extra.tmd->coords[ACROPOLIS_BRIDGE_PLAYER_FIRST_WATER_PART + index * ACROPOLIS_BRIDGE_PLAYER_WATER_PART_STRIDE];
            _gAcropolisBridgePlayerWaterPartPositions[index].vx = playerPart->workm.t[0];
            _gAcropolisBridgePlayerWaterPartPositions[index].vy = playerPart->workm.t[1];
            _gAcropolisBridgePlayerWaterPartPositions[index].vz = playerPart->workm.t[2];
        }
    }

    ambientWork->age = ambientWork->age + 1;
    switch (mappedView) {
        case 6:
            glowDrawTintedDiscNoBias(&_gAcropolisBridgeWarmDiscWorldPosition, ACROPOLIS_BRIDGE_DISC_RADIUS_SCALE, ACROPOLIS_BRIDGE_DISC_WARM_COLOR);
            break;
        case 7:
            glowDrawTintedDiscNoBias(&_gAcropolisBridgeWarmDiscWorldPosition, ACROPOLIS_BRIDGE_DISC_RADIUS_SCALE, ACROPOLIS_BRIDGE_DISC_WARM_COLOR);
            break;
        case 3:
        case 4:
        case 9:
            glowDrawTintedDiscNoBias(&_gAcropolisBridgeCoolDiscWorldPosition, ACROPOLIS_BRIDGE_DISC_RADIUS_SCALE, ACROPOLIS_BRIDGE_DISC_COOL_COLOR);
            break;
    }

    // Emit the glow families selected by this mapped camera.
    viewBit = 1 << (mappedView - 1);
    if (mappedView == ACROPOLIS_BRIDGE_VIEW_KEYPAD_MOVIE) {
        for (index = 0; index < ARRAY_SIZE(_gAcropolisBridgeStarGlowOffsets); index++) {
            if (_gAcropolisBridgeStarGlowViewMasks[index] & viewBit) {
                glowPosition.vx = 0;
                glowPosition.vy = -ACROPOLIS_BRIDGE_VIEW9_STAR_Y_OFFSET;
                glowPosition.vz = 0;
                glowPosition.vx = _gAcropolisBridgeStarGlowOffsets[index].vx;
                glowPosition.vy = _gAcropolisBridgeStarGlowOffsets[index].vy - ACROPOLIS_BRIDGE_VIEW9_STAR_Y_OFFSET;
                glowPosition.vz = _gAcropolisBridgeStarGlowOffsets[index].vz;
                effectSpawn((EFFECT_ACROPOLIS_BRIDGE_STAR_GLOW | EFFECT_SPAWN_UNLIMITED), effectCoord, ambientWork->age + index, &glowPosition);
            }
        }
    } else {
        for (index = 0; index < ARRAY_SIZE(_gAcropolisBridgeStarGlowOffsets); index++) {
            if (_gAcropolisBridgeStarGlowViewMasks[index] & viewBit) {
                effectSpawn((EFFECT_ACROPOLIS_BRIDGE_STAR_GLOW | EFFECT_SPAWN_UNLIMITED), effectCoord, ambientWork->age + index, &_gAcropolisBridgeStarGlowOffsets[index]);
                effectSpawn(EFFECT_ACROPOLIS_BRIDGE_GROUND_GLOW, effectCoord, ambientWork->age + index, &_gAcropolisBridgeGroundGlowOffsets[index]);
            }
        }
    }

    for (index = 0; index < 3; index++) {
        if (_gAcropolisBridgeLampGlowViewMasks[index] & viewBit) {
            effectSpawn(EFFECT_ACROPOLIS_BRIDGE_LAMP_GLOW, effectCoord, 0, &_gAcropolisBridgeLampGlowOffsets[index]);
        }
    }
    for (index = 3; index < 5; index++) {
        if (_gAcropolisBridgeLampGlowViewMasks[index] & viewBit) {
            effectSpawn(EFFECT_ACROPOLIS_BRIDGE_LAMP_GLOW, effectCoord, 1, &_gAcropolisBridgeLampGlowOffsets[index]);
        }
        if (_gAcropolisBridgeLampGlowViewMasks[index + 2] & viewBit) {
            effectSpawn(EFFECT_ACROPOLIS_BRIDGE_LAMP_GLOW, effectCoord, 2, &_gAcropolisBridgeLampGlowOffsets[index + 2]);
        }
    }
    if (_gAcropolisBridgeLampGlowViewMasks[11] & viewBit) {
        effectSpawn(EFFECT_ACROPOLIS_BRIDGE_LAMP_GLOW, effectCoord, 1, &_gAcropolisBridgeLampGlowOffsets[11]);
    }

    // Sample the two player joints; signed-halfword thresholds retain truncation.
    if ((viewBit & ACROPOLIS_BRIDGE_WATER_VIEW_MASK) && gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING && playerPart->coord.t[1] >= ACROPOLIS_BRIDGE_WATER_MIN_LOCAL_Y) {
        for (index = 0; index < ARRAY_SIZE(_gAcropolisBridgePlayerWaterPartPositions); index++) {
            playerPart   = &playerTask->extra.tmd->coords[ACROPOLIS_BRIDGE_PLAYER_FIRST_WATER_PART + index * ACROPOLIS_BRIDGE_PLAYER_WATER_PART_STRIDE];
            axisDistance = _gAcropolisBridgePlayerWaterPartPositions[index].vx - playerPart->workm.t[0];
            distanceSum  = axisDistance < 0;
            if (distanceSum) {
                axisDistance = playerPart->workm.t[0] - _gAcropolisBridgePlayerWaterPartPositions[index].vx;
            }
            previousComponent = _gAcropolisBridgePlayerWaterPartPositions[index].vy;
            axisValue         = previousComponent - playerPart->workm.t[1];
            if (axisValue < 0) {
                axisValue = playerPart->workm.t[1] - previousComponent;
            }
            distanceSum        = axisDistance + axisValue;
            previousComponent  = _gAcropolisBridgePlayerWaterPartPositions[index].vz;
            axisValue          = playerPart->workm.t[2];
            axisDistance       = previousComponent - axisValue;
            axisDistance       = ((axisDistance >= 0) ? (distanceSum + axisDistance) : (distanceSum + (axisValue - previousComponent))) + ACROPOLIS_BRIDGE_WATER_CHANCE_BIAS;
            ambientWork->angle = axisDistance;

            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            randomBits      = gRandomLcgState >> 16;
            if ((randomBits & ACROPOLIS_BRIDGE_WATER_RANDOM_MASK) < ambientWork->angle) {
                effectSpawn(gRoomEffectWaterRippleId, playerPart, ACROPOLIS_BRIDGE_WATER_RIPPLE_HALF_SIZE, NULL);
            }
            ambientWork->angle = ambientWork->angle - ACROPOLIS_BRIDGE_WATER_CHANCE_BIAS;
            gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            randomBits         = gRandomLcgState >> 16;
            if ((randomBits & ACROPOLIS_BRIDGE_WATER_RANDOM_MASK) < ambientWork->angle) {
                effectSpawn(gRoomEffectWaterSprayId, playerPart, ACROPOLIS_BRIDGE_WATER_SPRAY_ARGUMENT, NULL);
            }

            _gAcropolisBridgePlayerWaterPartPositions[index].vx = playerPart->workm.t[0];
            _gAcropolisBridgePlayerWaterPartPositions[index].vy = playerPart->workm.t[1];
            _gAcropolisBridgePlayerWaterPartPositions[index].vz = playerPart->workm.t[2];
        }
    }

    // Borrow byte-addressed packet storage from the file buffer for this frame.
    _gAcropolisBridgeStreakMoveCursor =
        (DR_MOVE*)(gFsActorBuffer2Base + (gDisplayState.otBuffer * ACROPOLIS_BRIDGE_STREAK_PACKET_BUFFER_BYTES + ACROPOLIS_BRIDGE_STREAK_PACKET_BASE_BYTES));

/// Seeds one view's framebuffer streaks with thirty attempted counted spawns.
///
/// Captures this function's ACROPOLIS_BRIDGE_STREAK_ENTRY_BURST_COUNT.
/// All inputs must be side-effect-free; spawnIndex must be a writable s32
/// lvalue. It is reset and left at the burst count. Other arguments are evaluated
/// once per spawn. Expands as a compound statement with its own for loop.
#define ACROPOLIS_BRIDGE_SEED_STREAKS(effectId, parentCoord, viewId, spawnIndex)                           \
    {                                                                                                      \
        for ((spawnIndex) = 0; (spawnIndex) < ACROPOLIS_BRIDGE_STREAK_ENTRY_BURST_COUNT; (spawnIndex)++) { \
            effectSpawn((effectId), (parentCoord), (viewId), NULL);                                        \
        }                                                                                                  \
    }

    // Seed streaks on view entry, then thin ongoing emission during battle.
    switch (_gAcropolisBridgeViewStreakStyles[mappedView - 1]) {
        case ACROPOLIS_BRIDGE_STREAK_NONE:
            break;
        case ACROPOLIS_BRIDGE_STREAK_FALLING:
            previousView = ambientWork->scale;
            if (previousView != mappedView) {
                ACROPOLIS_BRIDGE_SEED_STREAKS(EFFECT_ACROPOLIS_BRIDGE_FALLING_STREAK, effectCoord, (s32)mappedView, index);
            } else if (gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
                if (ambientWork->age & ACROPOLIS_BRIDGE_STREAK_FAST_PHASE_BIT) {
                    effectSpawn(EFFECT_ACROPOLIS_BRIDGE_FALLING_STREAK, effectCoord, (s32)(previousView), NULL);
                    effectSpawn(EFFECT_ACROPOLIS_BRIDGE_FALLING_STREAK, effectCoord, (s32)(previousView), NULL);
                } else {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    if (((gRandomLcgState >> 16) & 1) == 0) {
                        effectSpawn(EFFECT_ACROPOLIS_BRIDGE_FALLING_STREAK, effectCoord, (s32)(previousView), NULL);
                    }
                }
            } else {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if ((u16)((gRandomLcgState >> 16) % 3) == 0) {
                    effectSpawn(EFFECT_ACROPOLIS_BRIDGE_FALLING_STREAK, effectCoord, (s32)(previousView), NULL);
                }
            }
            break;
        case ACROPOLIS_BRIDGE_STREAK_MID_DUST:
            previousView = ambientWork->scale;
            if (previousView != mappedView) {
                ACROPOLIS_BRIDGE_SEED_STREAKS(EFFECT_ACROPOLIS_BRIDGE_MID_DUST_STREAK, effectCoord, (s32)mappedView, index);
            } else if (gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
                effectSpawn(EFFECT_ACROPOLIS_BRIDGE_MID_DUST_STREAK, effectCoord, (s32)(previousView), NULL);
                effectSpawn(EFFECT_ACROPOLIS_BRIDGE_MID_DUST_STREAK, effectCoord, (s32)(previousView), NULL);
            } else {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if ((u16)((gRandomLcgState >> 16) % 3) == 0) {
                    effectSpawn(EFFECT_ACROPOLIS_BRIDGE_MID_DUST_STREAK, effectCoord, (s32)(previousView), NULL);
                }
            }
            break;
        case ACROPOLIS_BRIDGE_STREAK_LOW_DUST:
            previousView = ambientWork->scale;
            if (previousView != mappedView) {
                ACROPOLIS_BRIDGE_SEED_STREAKS(EFFECT_ACROPOLIS_BRIDGE_LOW_DUST_STREAK, effectCoord, (s32)mappedView, index);
            } else if (gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
                effectSpawn(EFFECT_ACROPOLIS_BRIDGE_LOW_DUST_STREAK, effectCoord, (s32)(previousView), NULL);
                effectSpawn(EFFECT_ACROPOLIS_BRIDGE_LOW_DUST_STREAK, effectCoord, (s32)(previousView), NULL);
            } else {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if ((u16)((gRandomLcgState >> 16) % 3) == 0) {
                    effectSpawn(EFFECT_ACROPOLIS_BRIDGE_LOW_DUST_STREAK, effectCoord, (s32)(previousView), NULL);
                }
            }
            break;
        case ACROPOLIS_BRIDGE_STREAK_TALL_DUST:
            previousView = ambientWork->scale;
            if (previousView != mappedView) {
                ACROPOLIS_BRIDGE_SEED_STREAKS(EFFECT_ACROPOLIS_BRIDGE_TALL_DUST_STREAK, effectCoord, (s32)mappedView, index);
            } else if (gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
                effectSpawn(EFFECT_ACROPOLIS_BRIDGE_TALL_DUST_STREAK, effectCoord, (s32)(previousView), NULL);
                effectSpawn(EFFECT_ACROPOLIS_BRIDGE_TALL_DUST_STREAK, effectCoord, (s32)(previousView), NULL);
            } else {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if ((u16)((gRandomLcgState >> 16) % 3) == 0) {
                    effectSpawn(EFFECT_ACROPOLIS_BRIDGE_TALL_DUST_STREAK, effectCoord, (s32)(previousView), NULL);
                }
            }
            break;
        case ACROPOLIS_BRIDGE_STREAK_PARTICLE:
            previousView = ambientWork->scale;
            if (previousView != mappedView) {
                ACROPOLIS_BRIDGE_SEED_STREAKS(EFFECT_ACROPOLIS_BRIDGE_PARTICLE_STREAK, effectCoord, (s32)mappedView, index);
            } else if (gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
                effectSpawn(EFFECT_ACROPOLIS_BRIDGE_PARTICLE_STREAK, effectCoord, (s32)(previousView), NULL);
                effectSpawn(EFFECT_ACROPOLIS_BRIDGE_PARTICLE_STREAK, effectCoord, (s32)(previousView), NULL);
            } else {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if ((u16)((gRandomLcgState >> 16) % 3) == 0) {
                    effectSpawn(EFFECT_ACROPOLIS_BRIDGE_PARTICLE_STREAK, effectCoord, (s32)(previousView), NULL);
                }
            }
            break;
    }

#undef ACROPOLIS_BRIDGE_SEED_STREAKS

    ambientWork->scale = mappedView;
}

/// Seeds a streak's lifetime, pixel width and frames per row, then advances its task state.
///
/// `task` and `work` borrow live writable objects. Sets `scale` to 30..119
/// frames, `angle` to 16..79 pixels and `period` to 1..4 frames per row. Consumes
/// three shared LCG draws, retaining the u16-to-u32 lifetime roll; keeps no pointer.
static inline void _acropolisBridgeInitializeStreakLifetime(Task* task, EffectWork* work)
{
    enum { ACROPOLIS_BRIDGE_STREAK_MIN_LIFETIME_FRAMES   = 30,
           ACROPOLIS_BRIDGE_STREAK_LIFETIME_RANGE_FRAMES = 90,
           ACROPOLIS_BRIDGE_STREAK_MIN_WIDTH_PIXELS      = 16,
           ACROPOLIS_BRIDGE_STREAK_WIDTH_RANGE_PIXELS    = 64,
           ACROPOLIS_BRIDGE_STREAK_MAX_FRAMES_PER_ROW    = 4 };
    u16 randomBits;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    randomBits      = gRandomLcgState >> 16;
    work->scale     = (u32)randomBits % ACROPOLIS_BRIDGE_STREAK_LIFETIME_RANGE_FRAMES + ACROPOLIS_BRIDGE_STREAK_MIN_LIFETIME_FRAMES;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->angle     = ((gRandomLcgState >> 16) & (ACROPOLIS_BRIDGE_STREAK_WIDTH_RANGE_PIXELS - 1)) + ACROPOLIS_BRIDGE_STREAK_MIN_WIDTH_PIXELS;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->period    = ((gRandomLcgState >> 16) & (ACROPOLIS_BRIDGE_STREAK_MAX_FRAMES_PER_ROW - 1)) + 1;
    task->state++;
}

void acropolisBridgeFallingStreakTask(Task* task)
{
    enum {
        ACROPOLIS_BRIDGE_DRAW_BUFFER_STRIDE_PIXELS = 272,
        ACROPOLIS_BRIDGE_STREAK_LAST_SOURCE_ROW    = 239
    };
    EffectWork* work;
    RECT        rect;
    DR_MOVE*    movePacket;
    u16         randomBits;
    s32         randomColumn;
    s32         columnSpan;
    s32         bufferY;
    s32         x;
    s32         y;
    s32         depth;

    work    = task->spawnArg2.pointer;
    bufferY = gDisplayState.drawBuffer * ACROPOLIS_BRIDGE_DRAW_BUFFER_STRIDE_PIXELS;
    if ((u8)viewGetMappedIndex() == task->spawnArg1.value) {
        // Seed a screen-space strip; its fields hold pixels and frame counts.
        if (work->age == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            randomBits      = gRandomLcgState >> 16;
            work->move.vy   = (u32)randomBits % 144 + 0x60;
            /* x and y double as the drift and spread of the column window here */
            x               = (work->move.vy - 0x60) / 3;
            y               = work->move.vy < 0xA0 ? (work->move.vy - 0x60) / 2 : 0x20;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            randomColumn    = gRandomLcgState >> 16;
            columnSpan      = y + 0x90;
            work->move.vx   = randomColumn % (x + columnSpan) + (0x40 - x);
            _acropolisBridgeInitializeStreakLifetime(task, work);
        }
        // Copy the current buffer row down by one pixel at the row-derived depth.
        y     = work->move.vy + work->age / work->period;
        x     = work->move.vx;
        depth = 0x840 - (y - 0x60) * 8;
        if (y < ACROPOLIS_BRIDGE_STREAK_LAST_SOURCE_ROW) {
            rect.x                            = x;
            rect.y                            = y + bufferY;
            rect.w                            = work->angle;
            rect.h                            = 1;
            movePacket                        = _gAcropolisBridgeStreakMoveCursor;
            _gAcropolisBridgeStreakMoveCursor = movePacket + 1;
            SetDrawMove(movePacket, &rect, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> 4), movePacket);
        }
        work->age++;
        if (work->age <= work->scale && y < ACROPOLIS_BRIDGE_STREAK_LAST_SOURCE_ROW) {
            return;
        }
    }
    effectKillTask(work, task);
}

void acropolisBridgeMidDustStreakTask(Task* task)
{
    enum {
        ACROPOLIS_BRIDGE_DRAW_BUFFER_STRIDE_PIXELS = 272,
        ACROPOLIS_BRIDGE_STREAK_LAST_SOURCE_ROW    = 239
    };
    EffectWork* work;
    RECT        rect;
    DR_MOVE*    movePacket;
    u16         randomBits;
    s32         randomColumn;
    s32         column;
    s32         columnSpan;
    s32         bufferY;
    s32         x;
    s32         y;
    s32         depth;

    work    = task->spawnArg2.pointer;
    bufferY = gDisplayState.drawBuffer * ACROPOLIS_BRIDGE_DRAW_BUFFER_STRIDE_PIXELS;
    if ((u8)viewGetMappedIndex() == task->spawnArg1.value) {
        // Seed a screen-space strip; its fields hold pixels and frame counts.
        if (work->age == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            randomBits      = gRandomLcgState >> 16;
            work->move.vy   = (u32)randomBits % 168 + 0x48;
            /* x and y double as the drift and spread of the column window here */
            x               = work->move.vy < 0xA0 ? work->move.vy - 0x48 : 0x58;
            y               = work->move.vy < 0x78 ? (work->move.vy - 0x48) * 5 / 3 : 0x50;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            randomColumn    = gRandomLcgState >> 16;
            columnSpan      = y + 0x48;
            column          = randomColumn % (x + columnSpan) + 0x58;
            column         -= x;
            work->move.vx   = column;
            _acropolisBridgeInitializeStreakLifetime(task, work);
        }
        // Copy the current buffer row down by one pixel at the row-derived depth.
        y     = work->move.vy + work->age / work->period;
        x     = work->move.vx;
        depth = (0xF0 - y) * (0xF0 - y) / 15 + 0x2C0;
        if (y < ACROPOLIS_BRIDGE_STREAK_LAST_SOURCE_ROW) {
            rect.x                            = x;
            rect.y                            = y + bufferY;
            rect.w                            = work->angle;
            rect.h                            = 1;
            movePacket                        = _gAcropolisBridgeStreakMoveCursor;
            _gAcropolisBridgeStreakMoveCursor = movePacket + 1;
            SetDrawMove(movePacket, &rect, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> 4), movePacket);
        }
        work->age++;
        if (work->age <= work->scale && y < ACROPOLIS_BRIDGE_STREAK_LAST_SOURCE_ROW) {
            return;
        }
    }
    effectKillTask(work, task);
}

void acropolisBridgeLowDustStreakTask(Task* task)
{
    enum {
        ACROPOLIS_BRIDGE_DRAW_BUFFER_STRIDE_PIXELS = 272,
        ACROPOLIS_BRIDGE_STREAK_LAST_SOURCE_ROW    = 239
    };
    EffectWork* work;
    RECT        rect;
    DR_MOVE*    movePacket;
    u16         randomBits;
    s32         randomColumn;
    s32         column;
    s32         columnSpan;
    s32         bufferY;
    s32         x;
    s32         y;
    s32         depth;

    work    = task->spawnArg2.pointer;
    bufferY = gDisplayState.drawBuffer * ACROPOLIS_BRIDGE_DRAW_BUFFER_STRIDE_PIXELS;
    if ((u8)viewGetMappedIndex() == task->spawnArg1.value) {
        // Seed a screen-space strip; its fields hold pixels and frame counts.
        if (work->age == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            randomBits      = gRandomLcgState >> 16;
            work->move.vy   = (u32)randomBits % 136 + 0x68;
            /* x and y double as the drift and spread of the column window here */
            x               = work->move.vy < 0x78 ? (work->move.vy - 0x68) * 2 : 0x20;
            y               = work->move.vy < 0x78 ? (work->move.vy - 0x68) * 9 : 0x90;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            randomColumn    = gRandomLcgState >> 16;
            columnSpan      = y + 0x40;
            column          = randomColumn % (x + columnSpan) + 0x20;
            column         -= x;
            work->move.vx   = column;
            _acropolisBridgeInitializeStreakLifetime(task, work);
        }
        // Copy the current buffer row down by one pixel at the row-derived depth.
        y     = work->move.vy + work->age / work->period;
        x     = work->move.vx;
        depth = 0x600 - (y - 0x68) * 8;
        if (y < ACROPOLIS_BRIDGE_STREAK_LAST_SOURCE_ROW) {
            rect.x                            = x;
            rect.y                            = y + bufferY;
            rect.w                            = work->angle;
            rect.h                            = 1;
            movePacket                        = _gAcropolisBridgeStreakMoveCursor;
            _gAcropolisBridgeStreakMoveCursor = movePacket + 1;
            SetDrawMove(movePacket, &rect, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> 4), movePacket);
        }
        work->age++;
        if (work->age <= work->scale && y < ACROPOLIS_BRIDGE_STREAK_LAST_SOURCE_ROW) {
            return;
        }
    }
    effectKillTask(work, task);
}

void acropolisBridgeTallDustStreakTask(Task* task)
{
    enum {
        ACROPOLIS_BRIDGE_DRAW_BUFFER_STRIDE_PIXELS = 272,
        ACROPOLIS_BRIDGE_STREAK_LAST_SOURCE_ROW    = 239
    };
    EffectWork* work;
    RECT        rect;
    DR_MOVE*    movePacket;
    u16         randomBits;
    s32         randomColumn;
    s32         column;
    s32         columnSpan;
    s32         bufferY;
    s32         x;
    s32         y;
    s32         depth;

    work    = task->spawnArg2.pointer;
    bufferY = gDisplayState.drawBuffer * ACROPOLIS_BRIDGE_DRAW_BUFFER_STRIDE_PIXELS;
    if ((u8)viewGetMappedIndex() == task->spawnArg1.value) {
        // Seed a screen-space strip; its fields hold pixels and frame counts.
        if (work->age == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            randomBits      = gRandomLcgState >> 16;
            work->move.vy   = (u32)randomBits % 168 + 0x48;
            /* x and y double as the drift and spread of the column window here */
            x               = (work->move.vy - 0x48) / 3;
            y               = (work->move.vy - 0x48) / 2;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            randomColumn    = gRandomLcgState >> 16;
            columnSpan      = y + 0x48;
            column          = randomColumn % (x + columnSpan) + 0x58;
            column         -= x;
            work->move.vx   = column;
            _acropolisBridgeInitializeStreakLifetime(task, work);
        }
        // Copy the current buffer row down by one pixel at the row-derived depth.
        y     = work->move.vy + work->age / work->period;
        x     = work->move.vx;
        depth = (0xF0 - y) * (0xF0 - y) / 15 + 0x400;
        if (y < ACROPOLIS_BRIDGE_STREAK_LAST_SOURCE_ROW) {
            rect.x                            = x;
            rect.y                            = y + bufferY;
            rect.w                            = work->angle;
            rect.h                            = 1;
            movePacket                        = _gAcropolisBridgeStreakMoveCursor;
            _gAcropolisBridgeStreakMoveCursor = movePacket + 1;
            SetDrawMove(movePacket, &rect, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> 4), movePacket);
        }
        work->age++;
        if (work->age <= work->scale && y < ACROPOLIS_BRIDGE_STREAK_LAST_SOURCE_ROW) {
            return;
        }
    }
    effectKillTask(work, task);
}

void acropolisBridgeParticleStreakTask(Task* task)
{
    enum {
        ACROPOLIS_BRIDGE_DRAW_BUFFER_STRIDE_PIXELS = 272,
        ACROPOLIS_BRIDGE_STREAK_LAST_SOURCE_ROW    = 239
    };
    EffectWork* work;
    RECT        rect;
    DR_MOVE*    movePacket;
    s32         randomColumn;
    s32         bufferY;
    s32         x;
    s32         y;
    s32         depth;

    work    = task->spawnArg2.pointer;
    bufferY = gDisplayState.drawBuffer * ACROPOLIS_BRIDGE_DRAW_BUFFER_STRIDE_PIXELS;
    if ((u8)viewGetMappedIndex() == task->spawnArg1.value) {
        // Seed a screen-space strip; its fields hold pixels and frame counts.
        if (work->age == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vy   = ((gRandomLcgState >> 16) & 0x7F) + 0x68;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            randomColumn    = gRandomLcgState >> 16;
            work->move.vx   = work->move.vy < 0x80 ? randomColumn % ((work->move.vy - 0x58) * 6) : randomColumn % 240;
            _acropolisBridgeInitializeStreakLifetime(task, work);
        }
        // Copy the current buffer row down by one pixel at the row-derived depth.
        y     = work->move.vy + work->age / work->period;
        x     = work->move.vx;
        depth = 0x800 - (y - 0x68) * 8;
        if (y < ACROPOLIS_BRIDGE_STREAK_LAST_SOURCE_ROW) {
            rect.x                            = x;
            rect.y                            = y + bufferY;
            rect.w                            = work->angle;
            rect.h                            = 1;
            movePacket                        = _gAcropolisBridgeStreakMoveCursor;
            _gAcropolisBridgeStreakMoveCursor = movePacket + 1;
            SetDrawMove(movePacket, &rect, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> 4), movePacket);
        }
        work->age++;
        if (work->age <= work->scale && y < ACROPOLIS_BRIDGE_STREAK_LAST_SOURCE_ROW) {
            return;
        }
    }
    effectKillTask(work, task);
}

/// Binds the shared star glow to the bridge's exported `void (Task*)` callback.
///
/// Define an unqualified function identifier before including the star fragment;
/// the fragment defines that function and undefines this binding afterwards.
#define GLOW_STAR_TASK acropolisBridgeGlowStarTask
#include "../../shared/effects/acropolis_glows_star.inc.c"

void acropolisBridgeGroundGlowTask(Task* task)
{
    enum {
        ACROPOLIS_BRIDGE_GROUND_GLOW_HALF_EXTENT   = 0x300, // Game-coordinate units
        ACROPOLIS_BRIDGE_GROUND_GLOW_DEPTH_BIAS    = 0x20,  // Camera Z / 4
        ACROPOLIS_BRIDGE_GROUND_GLOW_TEXTURE_PAGE  = 0x2B,
        ACROPOLIS_BRIDGE_GROUND_GLOW_PALETTE       = 0x4381,
        ACROPOLIS_BRIDGE_GROUND_GLOW_TEXTURE_TOP_V = 0x10,
        ACROPOLIS_BRIDGE_GROUND_GLOW_TEXTURE_LAST  = 0x27,
        ACROPOLIS_BRIDGE_GROUND_GLOW_GREY_MASK     = 0xF
    };
    _AcropolisBridgeQuadScratch** scratchCursor;
    _AcropolisBridgeQuadScratch*  reservedTop;
    _AcropolisBridgeQuadScratch*  quadScratch;
    const EffectUnitQuadCorner*   unitCorners;
    POLY_FT4*                     quad;
    GfxCoord*                     coord;
    EffectWork*                   work;
    const MATRIX*                 worldMatrix;
    s32                           cornerIndex;
    u8                            grey;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    actorRenderComposeCoord(coord);

    scratchCursor  = &SCRATCH_STACK_CURSOR(_AcropolisBridgeQuadScratch);
    cornerIndex    = 0;
    worldMatrix    = &coord->workm;
    unitCorners    = _gAcropolisBridgeGroundGlowUnitCorners;
    reservedTop    = *scratchCursor - 1;
    work->age      = task->spawnArg1.halves.low;
    *scratchCursor = reservedTop;
    quadScratch    = *scratchCursor;
    for (; cornerIndex < ARRAY_SIZE(quadScratch->vertices); cornerIndex++) {
        (&quadScratch->vertices[cornerIndex])->vx = unitCorners[cornerIndex].axis0Sign * ACROPOLIS_BRIDGE_GROUND_GLOW_HALF_EXTENT;
        (&quadScratch->vertices[cornerIndex])->vy = 0;
        (&quadScratch->vertices[cornerIndex])->vz = unitCorners[cornerIndex].axis1Sign * ACROPOLIS_BRIDGE_GROUND_GLOW_HALF_EXTENT;
        gte_SetRotMatrix(worldMatrix);
        gte_ldv0(&quadScratch->vertices[cornerIndex]);
        gte_rtv0();
        gte_stsv(&quadScratch->vertices[cornerIndex]);
        // Replace the rotated corners with the origin, narrowed to signed halfwords.
        (&quadScratch->vertices[cornerIndex])->vx = coord->workm.t[0];
        (&quadScratch->vertices[cornerIndex])->vy = coord->workm.t[1];
        (&quadScratch->vertices[cornerIndex])->vz = coord->workm.t[2];
    }

    // Project all four corners into one packet, then apply the ordering-depth bias.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    // Reserve before the depth test; a clipped glow still consumes its packet.
    quad = _acropolisBridgeReserveGroundGlowQuad();
    gte_stsxy(&quad->x0);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    setUV4(quad, 0, ACROPOLIS_BRIDGE_GROUND_GLOW_TEXTURE_TOP_V,
           ACROPOLIS_BRIDGE_GROUND_GLOW_TEXTURE_LAST, ACROPOLIS_BRIDGE_GROUND_GLOW_TEXTURE_TOP_V,
           0, ACROPOLIS_BRIDGE_GROUND_GLOW_TEXTURE_TOP_V + ACROPOLIS_BRIDGE_GROUND_GLOW_TEXTURE_LAST,
           ACROPOLIS_BRIDGE_GROUND_GLOW_TEXTURE_LAST,
           ACROPOLIS_BRIDGE_GROUND_GLOW_TEXTURE_TOP_V + ACROPOLIS_BRIDGE_GROUND_GLOW_TEXTURE_LAST);
    gte_stsxy3(&quad->x1, &quad->x2, &quad->x3);
    gte_stszotz(&quadScratch->otz);
    quadScratch->otz += ACROPOLIS_BRIDGE_GROUND_GLOW_DEPTH_BIAS;
    if (quadScratch->otz >= GLOW_MIN_DEPTH) {
        quad->tpage     = ACROPOLIS_BRIDGE_GROUND_GLOW_TEXTURE_PAGE;
        quad->clut      = ACROPOLIS_BRIDGE_GROUND_GLOW_PALETTE;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        grey            = (gRandomLcgState >> 16) & ACROPOLIS_BRIDGE_GROUND_GLOW_GREY_MASK;
        setRGB0(quad, grey, grey, grey);
        setSemiTrans(quad, 1);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_AcropolisBridgeQuadScratch);
    effectKillTask(work, task);
}

/// Binds the included lamp glow to the bridge's exported `void (Task*)` callback.
///
/// Define this function identifier immediately before the lamp fragment, which
/// undefines it. No arguments, conditional alternatives or tokens are constructed.
#define GLOW_LAMP_TASK acropolisBridgeGlowLampTask
#include "../../shared/effects/acropolis_glows_lamp.inc.c"

/// Seeds the arrival scene's dust plane with 96 edge motes and eight interior motes.
///
/// `ACROPOLIS_BRIDGE_EFFECT_MESSAGE_ARRIVAL_DUST` targets the coordinate body; all payload words
/// are ignored. Each burst consumes two LCG draws per mote and attempts a counted
/// spawn at X -15960, Y -2000..-465 and Z -2500..-1530, with edge jitter.
/// Spawn snapshots the stack-owned offset. Returns 0 even when spawns fail.
static s32 _acropolisBridgeSpawnArrivalDust(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    enum {
        ACROPOLIS_BRIDGE_DUST_EDGE_BURSTS      = 32,
        ACROPOLIS_BRIDGE_DUST_INTERIOR_COUNT   = 8,
        ACROPOLIS_BRIDGE_DUST_PLANE_X          = -15960,
        ACROPOLIS_BRIDGE_DUST_HEIGHT_RANGE     = 1536,
        ACROPOLIS_BRIDGE_DUST_DEPTH_RANGE      = 970,
        ACROPOLIS_BRIDGE_DUST_EDGE_WIDTH_MASK  = 15,
        ACROPOLIS_BRIDGE_DUST_EDGE_Y_BASE_WORD = 0xF830,
        ACROPOLIS_BRIDGE_DUST_EDGE_Z_BASE_WORD = 0xF63C,
        ACROPOLIS_BRIDGE_DUST_EDGE_Z_FAR_WORD  = 0xFA06,
        ACROPOLIS_BRIDGE_DUST_INTERIOR_Y_BASE  = -2000,
        ACROPOLIS_BRIDGE_DUST_INTERIOR_Z_BASE  = -2500
    };
    GfxCoord* effectCoord;
    SVECTOR   position;
    s32       burstIndex;

    effectCoord = task->extra.coordBody->coord;

/// Spawns one mote on each of the arrival dust plane's three emitting edges.
///
/// Requires a side-effect-free live coordinate pointer and writable SVECTOR
/// lvalue, both evaluated repeatedly. Consumes six shared LCG draws and captures
/// the function's ACROPOLIS_BRIDGE_DUST_* bounds; each spawn snapshots XYZ.
/// Expands as one compound statement, with no return or loop control.
#define ACROPOLIS_BRIDGE_SPAWN_EDGE_DUST(parentCoord, offset)                                                                         \
    {                                                                                                                                 \
        (offset).vx     = ACROPOLIS_BRIDGE_DUST_PLANE_X;                                                                              \
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;                                             \
        (offset).vy     = (gRandomLcgState >> 16) % ACROPOLIS_BRIDGE_DUST_HEIGHT_RANGE + ACROPOLIS_BRIDGE_DUST_EDGE_Y_BASE_WORD;      \
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;                                             \
        (offset).vz     = ((gRandomLcgState >> 16) & ACROPOLIS_BRIDGE_DUST_EDGE_WIDTH_MASK) + ACROPOLIS_BRIDGE_DUST_EDGE_Z_BASE_WORD; \
        effectSpawn(EFFECT_ACROPOLIS_BRIDGE_DUST_MOTE, (parentCoord), 0, &(offset));                                                  \
                                                                                                                                      \
        (offset).vx     = ACROPOLIS_BRIDGE_DUST_PLANE_X;                                                                              \
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;                                             \
        (offset).vy     = (gRandomLcgState >> 16) % ACROPOLIS_BRIDGE_DUST_HEIGHT_RANGE + ACROPOLIS_BRIDGE_DUST_EDGE_Y_BASE_WORD;      \
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;                                             \
        (offset).vz     = ACROPOLIS_BRIDGE_DUST_EDGE_Z_FAR_WORD - ((gRandomLcgState >> 16) & ACROPOLIS_BRIDGE_DUST_EDGE_WIDTH_MASK);  \
        effectSpawn(EFFECT_ACROPOLIS_BRIDGE_DUST_MOTE, (parentCoord), 0, &(offset));                                                  \
                                                                                                                                      \
        (offset).vx     = ACROPOLIS_BRIDGE_DUST_PLANE_X;                                                                              \
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;                                             \
        (offset).vy     = ((gRandomLcgState >> 16) & ACROPOLIS_BRIDGE_DUST_EDGE_WIDTH_MASK) + ACROPOLIS_BRIDGE_DUST_EDGE_Y_BASE_WORD; \
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;                                             \
        (offset).vz     = (u16)(gRandomLcgState >> 16) % ACROPOLIS_BRIDGE_DUST_DEPTH_RANGE + ACROPOLIS_BRIDGE_DUST_EDGE_Z_BASE_WORD;  \
        effectSpawn(EFFECT_ACROPOLIS_BRIDGE_DUST_MOTE, (parentCoord), 0, &(offset));                                                  \
    }

    // Seed both side edges and the bottom edge of the arrival dust plane.
    burstIndex = 0;
    do {
        ACROPOLIS_BRIDGE_SPAWN_EDGE_DUST(effectCoord, position);
        burstIndex++;
    } while (burstIndex < ACROPOLIS_BRIDGE_DUST_EDGE_BURSTS);

#undef ACROPOLIS_BRIDGE_SPAWN_EDGE_DUST

    // Fill the plane with eight additional motes.
    burstIndex = 0;
    do {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        position.vx     = ACROPOLIS_BRIDGE_DUST_PLANE_X;
        position.vy     = (gRandomLcgState >> 16) % ACROPOLIS_BRIDGE_DUST_HEIGHT_RANGE + ACROPOLIS_BRIDGE_DUST_INTERIOR_Y_BASE;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        position.vz     = (u16)(gRandomLcgState >> 16) % ACROPOLIS_BRIDGE_DUST_DEPTH_RANGE + ACROPOLIS_BRIDGE_DUST_INTERIOR_Z_BASE;
        effectSpawn(EFFECT_ACROPOLIS_BRIDGE_DUST_MOTE, effectCoord, 0, &position);
        burstIndex++;
    } while (burstIndex < ACROPOLIS_BRIDGE_DUST_INTERIOR_COUNT);

    return 0;
}

void acropolisBridgeDustMoteTask(Task* task)
{
    enum {
        ACROPOLIS_BRIDGE_DUST_MOTE_LIFETIME_FRAMES    = 31,
        ACROPOLIS_BRIDGE_DUST_MOTE_END_Y              = -29,
        ACROPOLIS_BRIDGE_DUST_MOTE_GRAVITY_PER_UPDATE = 6
    };
    EffectPointTileScratch* tileScratch;
    TILE_1*                 prim;
    GfxCoord*               coord;
    EffectWork*             work;

    coord       = task->extra.coordBody->coord;
    tileScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectPointTileScratch);
    work        = task->spawnArg2.pointer;
    actorRenderComposeCoord(coord);

    if (work->age == 0) {
        work->move.vz   = 0;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->move.vx   = (gRandomLcgState >> 16) & 0xF;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->move.vy   = ((gRandomLcgState >> 16) & 3) - 1;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->scale     = ((gRandomLcgState >> 16) & 0x3F) + 0x30;
    }

    coord->coord.t[0]  += work->move.vx;
    coord->coord.t[1]  += work->move.vy;
    coord->coord.t[2]  += work->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    // Draw the composition cached before this update's local movement.
    // Only an accepted depth accelerates the mote; keep that ordering.
    tileScratch->viewPoint.vx = coord->workm.t[0];
    tileScratch->viewPoint.vy = coord->workm.t[1];
    tileScratch->viewPoint.vz = coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&tileScratch->viewPoint);
    gte_rtps();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setTile1(prim);
    gte_stsxy(&prim->x0);
    gte_stszotz(&tileScratch->depth);
    if (tileScratch->depth >= EFFECT_POINT_TILE_MIN_DEPTH) {
        setRGB0(prim, work->scale >> 1, work->scale, work->scale);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)tileScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
        gpuSetPrimitiveBlendMode(prim, GPU_BLEND_AVERAGE, tileScratch->depth);
        work->move.vy += ACROPOLIS_BRIDGE_DUST_MOTE_GRAVITY_PER_UPDATE;
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectPointTileScratch);
    work->age++;
    if (work->age >= ACROPOLIS_BRIDGE_DUST_MOTE_LIFETIME_FRAMES || coord->coord.t[1] >= ACROPOLIS_BRIDGE_DUST_MOTE_END_Y) {
        effectKillTask(work, task);
    }
}

void acropolisBridgeWaterRippleTask(Task* task)
{
    enum {
        ACROPOLIS_BRIDGE_RIPPLE_STATE_NEW           = 0,
        ACROPOLIS_BRIDGE_RIPPLE_STATE_ACTIVE        = 1,
        ACROPOLIS_BRIDGE_RIPPLE_INITIAL_BRIGHTNESS  = 64,
        ACROPOLIS_BRIDGE_RIPPLE_SPAWN_SIZE_MASK     = 0xFFF,
        ACROPOLIS_BRIDGE_RIPPLE_GROWTH_PER_UPDATE   = 32,
        ACROPOLIS_BRIDGE_RIPPLE_FADE_PER_UPDATE     = 2,
        ACROPOLIS_BRIDGE_RIPPLE_MIN_FADE_BRIGHTNESS = 3
    };
    EffectWork* work;
    GfxCoord*   coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        // Frozen and cancelled updates draw the retained ripple once.
        _acropolisBridgeDrawWaterRipple(coord, work->angle, work->scale);
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
    } else {
        work->age++;
        switch (task->state) {
            case ACROPOLIS_BRIDGE_RIPPLE_STATE_NEW:
                // Initialization leaves workm cached; the caller controls composition.
                work->scale     = ACROPOLIS_BRIDGE_RIPPLE_INITIAL_BRIGHTNESS;
                work->angle     = task->spawnArg1.halves.low & ACROPOLIS_BRIDGE_RIPPLE_SPAWN_SIZE_MASK;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gfxRotMatrixY(&coord->coord, (gRandomLcgState >> 16) & ACTOR_TRANSFORM_ANGLE_MASK, GRAPHICS_ROTATION_REPLACE);
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                task->state         = ACROPOLIS_BRIDGE_RIPPLE_STATE_ACTIVE;
                /* fallthrough */
            case ACROPOLIS_BRIDGE_RIPPLE_STATE_ACTIVE:
                work->angle += ACROPOLIS_BRIDGE_RIPPLE_GROWTH_PER_UPDATE;
                _acropolisBridgeDrawWaterRipple(coord, work->angle, work->scale);
                if (work->scale >= ACROPOLIS_BRIDGE_RIPPLE_MIN_FADE_BRIGHTNESS) {
                    work->scale -= ACROPOLIS_BRIDGE_RIPPLE_FADE_PER_UPDATE;
                } else {
                    effectKillTask(work, task);
                }
                break;
        }
    }
}

/// Draws an additive water-ripple square in the composed coordinate's local X/Z plane.
///
/// `halfSize` is the half-side in coordinate units; running tasks use 32..5119.
/// Sign products, rotations and translations retain their low 16 bits as GTE
/// coordinates. `brightness` supplies each GPU colour byte (128 is neutral).
/// `coord` is borrowed with `workm` already composed in `GsWSMATRIX`'s input space.
///
/// Projects one corner, then three, directly into a `POLY_FT4`. A nonnegative
/// final GTE flag links the packet at SZ3/4 plus one. The packet arena must have
/// room even when projection fails. One complete `OverlayFlaggedQuadScratch` is
/// reserved and released; GTE state changes and no pointer is retained.
static void _acropolisBridgeDrawWaterRipple(const GfxCoord* coord, s32 halfSize, s16 brightness)
{
    enum {
        ACROPOLIS_BRIDGE_RIPPLE_TEXTURE_TOP         = 56,
        ACROPOLIS_BRIDGE_RIPPLE_TEXTURE_SPAN        = 55,
        ACROPOLIS_BRIDGE_RIPPLE_MODULATED_QUAD_CODE = 0x2C
    };
    OverlayFlaggedQuadScratch* quadScratch;
    POLY_FT4*                  quad;
    SVECTOR*                   corner;
    s32                        cornerIndex;

    quadScratch = SCRATCH_STACK_RESERVE_BLOCK(OverlayFlaggedQuadScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(gEffectUnitQuadCorners); cornerIndex++) {
        quadScratch->corners[cornerIndex].vx = (u16)((EffectUnitQuadCorner*)gEffectUnitQuadCorners)[cornerIndex].axis0Sign * halfSize;
        /* Spelled as an offset rather than `&quadScratch->corners[cornerIndex]`, which is the same
           address: the member form lets CSE share one register with the GTE
           macros' `&quadScratch->corners[cornerIndex]`, and the original keeps two. */
        corner     = (SVECTOR*)((u8*)quadScratch + cornerIndex * sizeof(SVECTOR) + OFFSET_OF(OverlayFlaggedQuadScratch, corners));
        corner->vy = 0;
        corner->vz = (u16)((EffectUnitQuadCorner*)gEffectUnitQuadCorners)[cornerIndex].axis1Sign * halfSize;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&quadScratch->corners[cornerIndex]);
        gte_rtv0();
        gte_stsv(&quadScratch->corners[cornerIndex]);
        quadScratch->corners[cornerIndex].vx += coord->workm.t[0];
        corner->vy                           += coord->workm.t[1];
        corner->vz                           += coord->workm.t[2];
    }

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&quadScratch->corners[0]);
    gte_rtps();
    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
    setcode(quad, ACROPOLIS_BRIDGE_RIPPLE_MODULATED_QUAD_CODE);
    gte_stsxy(&quad->x0);
    gte_ldv3(&quadScratch->corners[1], &quadScratch->corners[2], &quadScratch->corners[3]);
    gte_rtpt();
    setUV4(quad, 0, ACROPOLIS_BRIDGE_RIPPLE_TEXTURE_TOP, ACROPOLIS_BRIDGE_RIPPLE_TEXTURE_SPAN, ACROPOLIS_BRIDGE_RIPPLE_TEXTURE_TOP, 0, ACROPOLIS_BRIDGE_RIPPLE_TEXTURE_TOP + ACROPOLIS_BRIDGE_RIPPLE_TEXTURE_SPAN, ACROPOLIS_BRIDGE_RIPPLE_TEXTURE_SPAN, ACROPOLIS_BRIDGE_RIPPLE_TEXTURE_TOP + ACROPOLIS_BRIDGE_RIPPLE_TEXTURE_SPAN);
    gte_stsxy3(&quad->x1, &quad->x2, &quad->x3);
    gte_stflg(&quadScratch->flag);
    if (quadScratch->flag >= 0) {
        gte_stszotz(&quadScratch->otz);
        quadScratch->otz++;
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 704, 0);
        setRGB0(quad, brightness, brightness, brightness);
        quad->clut = getClut(272, 271);
        setSemiTrans(quad, 1);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(OverlayFlaggedQuadScratch);
}

#include "../../shared/effects/effect_sprite_debris.inc.c"

/// Draws a spinning raw-texture debris cell at a composed coordinate's translation.
///
/// `coord` is borrowed read-only with `workm` in `GsWSMATRIX`'s input space; its
/// translation is narrowed to signed 16-bit coordinates. `frame` selects a 32x32
/// cell in the eight-cell row at V=224; callers use 0..7 and UV stores wrap to
/// bytes without a `frame` check. `size` is a signed perspective numerator: the
/// half-diagonal is `size` * 31 / (SZ3/4 + 1) pixels. `angle` uses 4096 units per
/// turn; Q12 rotated products round down when negative.
///
/// Allocates one `POLY_FT4` even on a rejected projection. Accepted packets use
/// raw texture and additive semitransparency. Requires a live packet arena and
/// initialized scratch stack; releases one `EffectBillboardScratch` on every
/// path. GTE state is overwritten and no pointer is retained.
static void _effectSpriteDrawChip(const GfxCoord* coord, u16 frame, s16 size, s16 angle)
{
    enum {
        EFFECT_SPRITE_CHIP_CELL_PITCH_TEXELS        = 32,
        EFFECT_SPRITE_CHIP_UV_SPAN_TEXELS           = EFFECT_SPRITE_CHIP_CELL_PITCH_TEXELS - 1,
        EFFECT_SPRITE_CHIP_FIRST_TEXEL_ROW          = 224,
        EFFECT_SPRITE_CHIP_LAST_TEXEL_ROW           = 255,
        EFFECT_SPRITE_CHIP_QUARTER_TURN             = ONE / 4,
        EFFECT_SPRITE_CHIP_TRIG_FRACTION_BITS       = 12,
        EFFECT_SPRITE_CHIP_MODULATED_QUAD_CODE      = 0x2C,
        EFFECT_SPRITE_CHIP_RAW_SEMITRANSPARENT_BITS = 3
    };
    void**                  scratchSlot;
    EffectBillboardScratch* scratchEnd;
    EffectBillboardScratch* block;
    s32*                    depthOutput;
    POLY_FT4*               quad;
    s32                     cornerAngle;
    s32                     uLeft;
    s32                     uRight;

    scratchSlot = SCRATCH_STACK_CURSOR_SLOT;
    scratchEnd  = *scratchSlot;
    block       = scratchEnd - 1;
    depthOutput = &block->depth;

    block->worldPoint.vx = (u16)coord->workm.t[0];
    block->worldPoint.vy = (u16)coord->workm.t[1];
    block->worldPoint.vz = (u16)coord->workm.t[2];
    *scratchSlot         = block;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
    setcode(quad, EFFECT_SPRITE_CHIP_MODULATED_QUAD_CODE);
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);

    if (block->projectionFlags >= 0) {
        gte_stszotz(depthOutput);
        block->depth++;
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 704, 0);
        quad->clut  = getClut(304, 271);
        uLeft       = frame * EFFECT_SPRITE_CHIP_CELL_PITCH_TEXELS;
        uRight      = uLeft + EFFECT_SPRITE_CHIP_UV_SPAN_TEXELS;
        setUV4(quad, uLeft, EFFECT_SPRITE_CHIP_FIRST_TEXEL_ROW, uRight, EFFECT_SPRITE_CHIP_FIRST_TEXEL_ROW, uLeft, EFFECT_SPRITE_CHIP_LAST_TEXEL_ROW, uRight, EFFECT_SPRITE_CHIP_LAST_TEXEL_ROW);
        setcode(quad, getcode(quad) | EFFECT_SPRITE_CHIP_RAW_SEMITRANSPARENT_BITS);

        cornerAngle          = angle;
        block->cornerOffsetX = (size * EFFECT_SPRITE_CHIP_UV_SPAN_TEXELS / block->depth * rsin(cornerAngle)) >> EFFECT_SPRITE_CHIP_TRIG_FRACTION_BITS;
        block->cornerOffsetY = (size * EFFECT_SPRITE_CHIP_UV_SPAN_TEXELS / block->depth * rcos(cornerAngle)) >> EFFECT_SPRITE_CHIP_TRIG_FRACTION_BITS;
        quad->x0             = block->screenX + (u16)block->cornerOffsetX;
        quad->x3             = block->screenX - (u16)block->cornerOffsetX;
        quad->y0             = block->screenY - (u16)block->cornerOffsetY;
        quad->y3             = block->screenY + (u16)block->cornerOffsetY;

        cornerAngle         += EFFECT_SPRITE_CHIP_QUARTER_TURN;
        block->cornerOffsetX = (size * EFFECT_SPRITE_CHIP_UV_SPAN_TEXELS / block->depth * rsin(cornerAngle)) >> EFFECT_SPRITE_CHIP_TRIG_FRACTION_BITS;
        block->cornerOffsetY = (size * EFFECT_SPRITE_CHIP_UV_SPAN_TEXELS / block->depth * rcos(cornerAngle)) >> EFFECT_SPRITE_CHIP_TRIG_FRACTION_BITS;
        quad->x1             = block->screenX + (u16)block->cornerOffsetX;
        quad->x2             = block->screenX - (u16)block->cornerOffsetX;
        quad->y1             = block->screenY - (u16)block->cornerOffsetY;
        quad->y2             = block->screenY + (u16)block->cornerOffsetY;

        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BYTES_AT(scratchSlot, sizeof(*block));
}

/// Draws an upright raw-texture debris billboard at a composed coordinate's translation.
///
/// `coord` is borrowed read-only with `workm` in `GsWSMATRIX`'s input space; translation
/// is narrowed to signed 16-bit coordinates. `frame` bits 0..2 select a 4x2 grid
/// of 56x56 cells beginning at V=112. `size` is a signed perspective numerator:
/// the half-side is `size` * 55 / (SZ3/4 + 1) pixels. The anchor lies three
/// quarters down the square. Screen-coordinate and UV stores retain low bits.
///
/// Allocates one `POLY_FT4` even on a rejected projection. Accepted packets use
/// raw texture and additive semitransparency. Requires a live packet arena and
/// initialized scratch stack; releases one `_AcropolisBridgeBillboardScratch`
/// on every path. GTE state is overwritten and no pointer is retained.
static void _effectSpriteDrawBillboard(const GfxCoord* coord, u16 frame, s16 size)
{
    enum {
        EFFECT_SPRITE_BILLBOARD_CELL_PITCH_TEXELS        = 56,
        EFFECT_SPRITE_BILLBOARD_UV_SPAN_TEXELS           = EFFECT_SPRITE_BILLBOARD_CELL_PITCH_TEXELS - 1,
        EFFECT_SPRITE_BILLBOARD_COLUMN_MASK              = 3,
        EFFECT_SPRITE_BILLBOARD_FRAME_MASK               = 7,
        EFFECT_SPRITE_BILLBOARD_ROW_SHIFT                = 2,
        EFFECT_SPRITE_BILLBOARD_FIRST_TEXEL_ROW          = 112,
        EFFECT_SPRITE_BILLBOARD_MODULATED_QUAD_CODE      = 0x2C,
        EFFECT_SPRITE_BILLBOARD_RAW_SEMITRANSPARENT_BITS = 3
    };
    void**                            scratchSlot;
    _AcropolisBridgeBillboardScratch* scratchEnd;
    _AcropolisBridgeBillboardScratch* block;
    POLY_FT4*                         quad;
    s32*                              depthOutput;
    u32                               cellBits;
    s32                               uLeft;
    s32                               rowV;
    s8                                vTopBits;
    s8                                vBottomBits;

    scratchSlot = SCRATCH_STACK_CURSOR_SLOT;
    scratchEnd  = *scratchSlot;
    block       = scratchEnd - 1;
    depthOutput = &block->depth;

    block->worldPoint.vx = (u16)coord->workm.t[0];
    block->worldPoint.vy = (u16)coord->workm.t[1];
    block->worldPoint.vz = (u16)coord->workm.t[2];
    *scratchSlot         = block;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&(scratchEnd - 1)->worldPoint);
    gte_rtps();

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
    setcode(quad, EFFECT_SPRITE_BILLBOARD_MODULATED_QUAD_CODE);
    gte_stsxy(&(scratchEnd - 1)->screenX);
    gte_stflg(&(scratchEnd - 1)->projectionFlags);

    if (block->projectionFlags >= 0) {
        gte_stszotz(depthOutput);
        (scratchEnd - 1)->depth++;
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 704, 0);
        quad->clut  = getClut(288, 271);
        cellBits    = frame;
        uLeft       = (cellBits & EFFECT_SPRITE_BILLBOARD_COLUMN_MASK) * EFFECT_SPRITE_BILLBOARD_CELL_PITCH_TEXELS;
        rowV        = ((cellBits & EFFECT_SPRITE_BILLBOARD_FRAME_MASK) >> EFFECT_SPRITE_BILLBOARD_ROW_SHIFT) * EFFECT_SPRITE_BILLBOARD_CELL_PITCH_TEXELS;
        vTopBits    = rowV + EFFECT_SPRITE_BILLBOARD_FIRST_TEXEL_ROW;
        vBottomBits = rowV + EFFECT_SPRITE_BILLBOARD_FIRST_TEXEL_ROW + EFFECT_SPRITE_BILLBOARD_UV_SPAN_TEXELS;
        setUV4(quad, uLeft, vTopBits, uLeft + EFFECT_SPRITE_BILLBOARD_UV_SPAN_TEXELS, vTopBits, uLeft, vBottomBits, uLeft + EFFECT_SPRITE_BILLBOARD_UV_SPAN_TEXELS, vBottomBits);
        setcode(quad, getcode(quad) | EFFECT_SPRITE_BILLBOARD_RAW_SEMITRANSPARENT_BITS);

        block->screenExtent = size * EFFECT_SPRITE_BILLBOARD_UV_SPAN_TEXELS / (scratchEnd - 1)->depth;

        quad->x0 = quad->x2 = block->screenX - (u16)block->screenExtent;
        quad->x1 = quad->x3 = block->screenX + (u16)block->screenExtent;
        quad->y0 = quad->y1 = block->screenY - (u16)block->screenExtent - (block->screenExtent >> 1);
        quad->y2 = quad->y3 = block->screenY + (block->screenExtent >> 1);

        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)(scratchEnd - 1)->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BYTES_AT(scratchSlot, sizeof(_AcropolisBridgeBillboardScratch));
}

/// Diagnostic when the Boss Stranger's node-order plan finds no candidate slot pair.
///
/// Printed when `bestGap` remains `BOSS_STRANGER_PLAN_GAP_NONE` after the slot search.
static const char _gBossStrangerNoPlanPairMessage[] = "s->root_cnt == 0xff about \n";

/// Enemy-task setup, frame update and destruction handlers, indexed by Task state 0..2.
///
/// Each receives the live Enemy and its Task. Dispatch copies all three
/// pointers; this table is independent of the enemy work's behavior states.
static const EnemyTaskFuncTable3 _gAcropolisBridgeEnemyTaskStates = {
    { _acropolisBridgeEnemySetup, _acropolisBridgeEnemyFrame, enemyDestroy }
};

#include "../../shared/effects/glow_draw_tinted_disc_no_bias.inc.c"

#include "../../shared/creatures/boss_stranger/boss_stranger.inc.c"

#include "../../shared/creatures/boss_stranger/boss_stranger_inlines.inc.c"

/// Applies enemy reveal and movie-start commands, returning 1.
///
/// Borrows a readable `ActorCommand` for the synchronous dispatch. The reveal
/// command reveals placements 0..2. Keypad acceptance admits those placements
/// when battle references are nonzero, at least two, and at least three,
/// respectively; admitted enemies regain their configured HP and enter the
/// movie-following state. Rejected or other placements become inactive and hidden.
/// Other commands leave the enemy unchanged; the remaining message words are ignored.
static s32 _acropolisBridgeEnemyApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    _AcropolisBridgeEnemyWork* work  = task->work;
    Enemy*                     enemy = task->spawnArg2.pointer;
    TmdObject*                 model = task->extra.tmd;
    s32                        placementIndex;
    u16                        commandId;

    if (command->context.key == ACROPOLIS_BRIDGE_COMMAND_CONTEXT_MOVIE && command->command == ACROPOLIS_BRIDGE_COMMAND_SHOW_MOVIE_ENEMIES) {
        placementIndex = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        switch (placementIndex) {
            case 0:
            case 1:
            case 2:
                model->flags = 0;
                break;
        }
    }
    // Admit each placement only when its battle-reference threshold is met.
    if (command->context.key == ACROPOLIS_BRIDGE_COMMAND_CONTEXT_KEYPAD) {
        commandId = command->command;
        if (commandId == ACROPOLIS_BRIDGE_COMMAND_KEYPAD_ACCEPTED) {
            placementIndex = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
            switch (placementIndex) {
                case 0:
                    if (gSceneCombatState.battleRefs != 0) {
                        break;
                    }
                    work->state = ACROPOLIS_BRIDGE_ENEMY_STATE_INACTIVE;
                    goto hide;
                case 1:
                    if (gSceneCombatState.battleRefs >= 2) {
                        break;
                    }
                    work->state = ACROPOLIS_BRIDGE_ENEMY_STATE_INACTIVE;
                    goto hide;
                case 2:
                    if (gSceneCombatState.battleRefs < 3) {
                        goto reset;
                    }
                    break;
                default:
                    work->state = ACROPOLIS_BRIDGE_ENEMY_STATE_INACTIVE;
                    goto hide;
            }
            {
                u16 hp    = _gAcropolisBridgeEnemyParams.hpMax;
                enemy->hp = hp;
                work->hp  = hp;
            }
            work->state = ACROPOLIS_BRIDGE_ENEMY_STATE_FOLLOW_MOVIE;
            goto done;
        reset:
            work->state = ACROPOLIS_BRIDGE_ENEMY_STATE_INACTIVE;
        hide:
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
    }
done:
    return 1;
}

/// Commits a started clip and resets its per-update counter.
///
/// Requires writable work aliases captured for the playback phase and request.
/// Latches the started clip, selects playing and clears the update count;
/// neither pointer is retained.
static inline void _acropolisBridgeCommitEnemyAnimation(_AcropolisBridgeEnemyWork* playbackWork,
                                                        _AcropolisBridgeEnemyWork* requestWork)
{
    playbackWork->prevAnimId = playbackWork->animId;
    requestWork->animRequest = ACROPOLIS_BRIDGE_ENEMY_ANIM_REQUEST_PLAYING;
    requestWork->animFrame   = 0;
}

/// Starts or advances the bridge enemy's three driven animation slots.
///
/// Requires live enemy work and animation rig storage. Blend and reset requests
/// restart slots 1..3 and clear the update counter; blend duration is selected
/// by the previous and requested clip, each in 0..5. A playing request advances
/// all three slots and increments the signed-halfword update counter once.
/// Every driven slot receives the requested rate, in sixteenths of normal speed.
/// Other request values leave playback unchanged.
static void _acropolisBridgeEnemyUpdateAnimation(Task* task)
{
    enum { ACROPOLIS_BRIDGE_ENEMY_FIRST_DRIVEN_SLOT = 1 };
    _AcropolisBridgeEnemyWork* work;
    _AcropolisBridgeEnemyWork* blendWork;
    _AcropolisBridgeEnemyWork* resetWork;
    _AcropolisBridgeEnemyWork* playbackWork;
    s32                        blendSlot;
    s32                        resetSlot;
    s32                        playingSlot;

    work = task->work;
    if (work->animRequest == ACROPOLIS_BRIDGE_ENEMY_ANIM_REQUEST_BLEND) {
        blendWork = task->work;
        for (blendSlot = ACROPOLIS_BRIDGE_ENEMY_FIRST_DRIVEN_SLOT; blendSlot < ARRAY_SIZE(blendWork->rig.slots); blendSlot++) {
            blendWork->rig.slots[blendSlot].rate = blendWork->animRate;
            animationSeekSlotWithBlend(&blendWork->rig.anim, blendSlot, blendWork->animId, 0,
                                       _gAcropolisBridgeEnemyAnimationBlendFrames[blendWork->prevAnimId][blendWork->animId]);
        }
        _acropolisBridgeCommitEnemyAnimation(blendWork, work);
        return;
    }
    if (work->animRequest == ACROPOLIS_BRIDGE_ENEMY_ANIM_REQUEST_RESET) {
        resetWork = task->work;
        for (resetSlot = ACROPOLIS_BRIDGE_ENEMY_FIRST_DRIVEN_SLOT; resetSlot < ARRAY_SIZE(resetWork->rig.slots); resetSlot++) {
            resetWork->rig.slots[resetSlot].rate = resetWork->animRate;
            animationResetSlot(&resetWork->rig.anim, resetSlot, resetWork->animId);
        }
        _acropolisBridgeCommitEnemyAnimation(resetWork, work);
        return;
    }
    if (work->animRequest == ACROPOLIS_BRIDGE_ENEMY_ANIM_REQUEST_PLAYING) {
        work->animFrame++;
        playbackWork = task->work;
        for (playingSlot = ACROPOLIS_BRIDGE_ENEMY_FIRST_DRIVEN_SLOT; playingSlot < ARRAY_SIZE(playbackWork->rig.slots); playingSlot++) {
            playbackWork->rig.slots[playingSlot].rate = playbackWork->animRate;
            animationTickSlot(&playbackWork->rig.anim, playingSlot);
        }
    }
}

/// Copies a local collision-body position in whole coordinate units.
///
/// Borrows three readable position halfwords and the writable body; copies only
/// XYZ without retaining either pointer or changing the body's coordinate owner.
static inline void _acropolisBridgeSetCollisionBodyPosition(WorldCollisionBody* body, const SVECTOR3* position)
{
    body->pos.vx = position->vx;
    body->pos.vy = position->vy;
    body->pos.vz = position->vz;
}

/// Initializes the bridge walker's uniform Q12 scale matrix and reserves its scratch vector.
///
/// Clears translation and off-diagonal entries; zero and `ONE` both leave
/// identity. Requires an initialized scratch stack with room for one `VECTOR`.
/// Leaves that block live for `_acropolisBridgeLightModel` to reuse and release.
static inline void _acropolisBridgeInitWalkerScale(BossStrangerWalker* walker)
{
    VECTOR* scratchEnd;
    VECTOR* scaleVector;
    s32     scaleAmount;

    scratchEnd                   = SCRATCH_STACK_CURSOR(VECTOR);
    scaleAmount                  = walker->scale;
    walker->scaleMtx.m[2][1]     = 0;
    walker->scaleMtx.m[2][0]     = 0;
    walker->scaleMtx.m[1][2]     = 0;
    walker->scaleMtx.m[1][0]     = 0;
    walker->scaleMtx.m[0][2]     = 0;
    walker->scaleMtx.m[0][1]     = 0;
    walker->scaleMtx.m[2][2]     = ONE;
    walker->scaleMtx.m[1][1]     = ONE;
    walker->scaleMtx.m[0][0]     = ONE;
    walker->scaleMtx.t[2]        = 0;
    walker->scaleMtx.t[1]        = 0;
    walker->scaleMtx.t[0]        = 0;
    scaleVector                  = scratchEnd - 1;
    SCRATCH_STACK_CURSOR(VECTOR) = scaleVector;
    if (scaleAmount != 0 && scaleAmount != ONE) {
        scaleVector->vz = scaleAmount;
        scaleVector->vy = scaleAmount;
        scaleVector->vx = scaleAmount;
        ScaleMatrix(&walker->scaleMtx, scaleVector);
    }
}

/// Composes and lights the bridge enemy model at its root's world position.
///
/// Requires a live model and the one-`VECTOR` scratch block reserved by
/// `_acropolisBridgeInitWalkerScale`. Reuses that block for the root translation,
/// queries all three lights, then releases it. Lighting may use nested scratch.
static inline void _acropolisBridgeLightModel(Task* task)
{
    VECTOR* worldPosition;

    worldPosition = SCRATCH_STACK_CURSOR(VECTOR);
    actorRenderComposeCoord(task->extra.tmd->coords);
    worldPosition->vx = task->extra.tmd->coords->workm.t[0];
    worldPosition->vy = task->extra.tmd->coords->workm.t[1];
    worldPosition->vz = task->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(task->extra.tmd, worldPosition, 0, 3);
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Allocates and initializes the bridge enemy's animation, collision and walker.
///
/// Requires a live enemy and TMD task with four coordinates. Owns a zeroed work
/// block until enemy teardown, and destroys the task if allocation fails. Uses
/// model parts 3 and 1 for body and attack spheres. Seeds live HP to 1 despite
/// loading the parameter HP first; accepted keypad commands later restore it.
/// The walker borrows room tables: the first ten nodes/order entries for scans,
/// and routes whose bytes may select any of the twenty nodes. The placement
/// index must select one of nine routes; the room's spawn table uses indices 0..5.
/// Normal entry starts sunk by 1500 coordinate units; room 2 starts placements
/// 0..2 at their fixed presentation positions and leaves other placements inactive.
/// The copied one-based player ID must select live player storage (only 1 is established).
static void _acropolisBridgeEnemySetup(Enemy* enemy, Task* task)
{
    enum {
        ACROPOLIS_BRIDGE_ENEMY_BODY_ID              = 0x29,
        ACROPOLIS_BRIDGE_ENEMY_INITIAL_SPEED_UNITS  = 48,
        ACROPOLIS_BRIDGE_ENEMY_INITIAL_TURN_LIMIT   = 48,
        ACROPOLIS_BRIDGE_ENEMY_INITIAL_SPEED_STEP   = 1,
        ACROPOLIS_BRIDGE_ENEMY_COLLISION_RADIUS     = 256,
        ACROPOLIS_BRIDGE_ENEMY_INITIAL_SINK_DEPTH   = 1500,
        ACROPOLIS_BRIDGE_ENEMY_INITIAL_PRESENTATION = 8,
        ACROPOLIS_BRIDGE_ENEMY_NAV_NODE_COUNT       = 10
    };
    TmdObject*                 model;
    TmdObject*                 lightingModel;
    GfxCoord*                  rootCoord;
    GfxCoord*                  walkerCoord;
    _AcropolisBridgeEnemyWork* work;
    BossStrangerWalker*        walker;
    WorldCollisionBody*        body;
    WorldCollisionBody*        attackBody;
    s32                        placementIndex;
    s32                        initialWalkerState;
    u16                        configuredHp;
    s32                        heightAxis;
    SVECTOR3                   origin;

    model      = task->extra.tmd;
    rootCoord  = model->coords;
    work       = memCalloc(sizeof(_AcropolisBridgeEnemyWork), 0);
    task->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    lightingModel           = task->extra.tmd;
    lightingModel->lightMtx = &work->lightMtx;
    lightingModel->colorMtx = &work->colorMtx;
    enemy->field_4          = &rootCoord->coord;
    enemy->field_48         = 0;
    enemy->param            = &_gAcropolisBridgeEnemyParams;
    configuredHp            = _gAcropolisBridgeEnemyParams.hpMax;
    enemy->recs             = work->bodyContacts;
    enemy->hp               = configuredHp;
    work->hp                = 1;
    work->field_10E         = 1;
    enemy->hpMax            = work->hp;
    enemy->hp               = enemy->hpMax;
    // Bind the rig and both collision spheres before enabling the walker.
    animationInitContext(&work->rig.anim, _gAcropolisBridgeEnemyAnimationSets, model, work->rig.poses,
                         work->rig.slots);
    work->animRate         = ANIMATION_RATE_ONE;
    body                   = &work->body;
    body->coord            = &task->extra.tmd->coords[3];
    body->context.contacts = work->bodyContacts;
    body->pos.vx           = 0;
    body->pos.vy           = 0;
    body->pos.vz           = 0;
    body->key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | ACROPOLIS_BRIDGE_ENEMY_BODY_ID;
    body->radius           = ACROPOLIS_BRIDGE_ENEMY_COLLISION_RADIUS;
    body->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, body);
    body->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(body->context.contacts, ARRAY_SIZE(work->bodyContacts), 0);
    origin.vx                    = 0;
    origin.vy                    = 0;
    origin.vz                    = 0;
    attackBody                   = &work->attack;
    attackBody->coord            = &task->extra.tmd->coords[1];
    attackBody->context.contacts = work->attackContacts;
    _acropolisBridgeSetCollisionBodyPosition(attackBody, &origin);
    attackBody->radius = ACROPOLIS_BRIDGE_ENEMY_COLLISION_RADIUS;
    attackBody->flags  = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, attackBody);
    worldCollisionInitContacts(attackBody->context.contacts, ARRAY_SIZE(work->attackContacts), 0);
    work->attack.key  = damagePackEnemyAttackKey(enemy, 0);
    rootCoord->parent = &gGfxViewCoord;
    work->yaw         = ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]);
    work->animRequest = ACROPOLIS_BRIDGE_ENEMY_ANIM_REQUEST_RESET;
    work->animId      = ACROPOLIS_BRIDGE_ENEMY_ANIM_CHASE;
    _acropolisBridgeEnemyUpdateAnimation(task);
    enemy->coord      = &task->extra.tmd->coords[3];
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    task->msgTable                = _gAcropolisBridgeEnemyMessageTable;
    work->prevState               = -1;
    work->state                   = ACROPOLIS_BRIDGE_ENEMY_STATE_PATROL_SUNK;
    work->sinkDepth               = ACROPOLIS_BRIDGE_ENEMY_INITIAL_SINK_DEPTH;
    work->baseHeight              = rootCoord->coord.t[1];
    rootCoord->coord.t[1]        += work->sinkDepth;

    // The embedded navigation and route records borrow the room tables.
    work->walker.navData.nodes         = _gAcropolisBridgeEnemyPatrolNodes;
    work->walker.navData.nodeCount     = ACROPOLIS_BRIDGE_ENEMY_NAV_NODE_COUNT;
    work->walker.navData.nodeOrder     = _gAcropolisBridgeEnemyNavNodeOrder;
    work->walker.navData.orderCount    = ACROPOLIS_BRIDGE_ENEMY_NAV_NODE_COUNT;
    work->walker.routeData.nodeIndices = _gAcropolisBridgeEnemyRoutesByPlacement[enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT];
    work->walker.nav                   = &work->walker.navData;
    work->walker.routeData.cursor      = 0;
    work->walker.route                 = &work->walker.routeData;
    walkerCoord                        = task->extra.tmd->coords;
    work->walker.avoidCount            = ARRAY_SIZE(work->bodyContacts);
    walker                             = &work->walker;
    work->walker.recs                  = 0;
    work->walker.avoidRecs             = work->bodyContacts;
    work->walker.scale                 = ONE;
    work->walker.recCount              = 0;
    work->walker.turnLimit             = ACROPOLIS_BRIDGE_ENEMY_INITIAL_TURN_LIMIT;
    work->walker.coord                 = walkerCoord;
    walker->speedTarget                = ACROPOLIS_BRIDGE_ENEMY_INITIAL_SPEED_UNITS;
    walker->speed                      = 0;
    walker->speedStep                  = ACROPOLIS_BRIDGE_ENEMY_INITIAL_SPEED_STEP;
    initialWalkerState                 = BOSS_STRANGER_WALKER_PATROL;
    work->walker.state                 = initialWalkerState;
    work->walker.lockHeight            = 1;
    work->walker.skipGround            = 1;
    work->walker.playerId              = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId;

    _acropolisBridgeInitWalkerScale(walker);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _acropolisBridgeLightModel(task);
    heightAxis = 1;
    if (gSceneCombatState.battleRefs < 3) {
        sceneAcquireBattleRef(0);
    }
    if (gGameSession->location.roomState == ACROPOLIS_BRIDGE_ROOM_AFTER_KEYPAD_MOVIE) {
        placementIndex = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        switch (placementIndex) {
            case 0:
                work->state                         = ACROPOLIS_BRIDGE_ENEMY_INITIAL_PRESENTATION;
                task->extra.tmd->coords->coord.t[0] = -0x22C4;
                task->extra.tmd->coords->coord.t[1] = -0x3E8;
                task->extra.tmd->coords->coord.t[2] = -0x640;
                break;
            case 1:
                work->state                                  = ACROPOLIS_BRIDGE_ENEMY_INITIAL_PRESENTATION;
                task->extra.tmd->coords->coord.t[0]          = -0x270F;
                task->extra.tmd->coords->coord.t[heightAxis] = -0x3E8;
                task->extra.tmd->coords->coord.t[2]          = -0x7D0;
                break;
            case 2:
                work->state                         = ACROPOLIS_BRIDGE_ENEMY_INITIAL_PRESENTATION;
                task->extra.tmd->coords->coord.t[0] = -0x2EE0;
                task->extra.tmd->coords->coord.t[1] = -0x3E8;
                task->extra.tmd->coords->coord.t[2] = -0x5DC;
                break;
            default:
                work->state = ACROPOLIS_BRIDGE_ENEMY_STATE_INACTIVE;
                break;
        }
    }
    task->state++;
}

/// Rebuilds the enemy walker's uniform scale matrix during state entry.
///
/// Clears translation and off-diagonal entries, then uses `walker.scale` in Q12
/// units (`ONE` is full scale). Zero and `ONE` both leave identity; the zero case
/// is retained behavior. Borrows one `VECTOR` from the initialized scratch stack
/// and releases it before returning. The two reset helpers retain the ordering
/// of the entry and shrinking updates.
static inline void _acropolisBridgeResetEnemyScaleOnEntry(_AcropolisBridgeEnemyWork* work)
{
    BossStrangerWalker* walker;
    VECTOR*             scratchEnd;
    VECTOR*             scaleVector;
    s32                 scaleAmount;

    scratchEnd                   = SCRATCH_STACK_CURSOR(VECTOR);
    walker                       = &work->walker;
    scaleAmount                  = walker->scale;
    scaleVector                  = scratchEnd - 1;
    SCRATCH_STACK_CURSOR(VECTOR) = scaleVector;
    walker->scaleMtx.m[2][1]     = 0;
    walker->scaleMtx.m[2][0]     = 0;
    walker->scaleMtx.m[1][2]     = 0;
    walker->scaleMtx.m[1][0]     = 0;
    walker->scaleMtx.m[0][2]     = 0;
    walker->scaleMtx.m[0][1]     = 0;
    walker->scaleMtx.m[2][2]     = ONE;
    walker->scaleMtx.m[1][1]     = ONE;
    walker->scaleMtx.m[0][0]     = ONE;
    walker->scaleMtx.t[2]        = 0;
    walker->scaleMtx.t[1]        = 0;
    walker->scaleMtx.t[0]        = 0;
    if (scaleAmount != 0 && scaleAmount != ONE) {
        scaleVector->vz        = scaleAmount;
        scaleVector->vy        = scaleAmount;
        ((scratchEnd - 1))->vx = scaleAmount;
        ScaleMatrix(&work->walker.scaleMtx, scaleVector);
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Rebuilds the enemy walker's uniform scale matrix during shrinking.
///
/// Clears translation and off-diagonal entries, then uses `walker.scale` in Q12
/// units (`ONE` is full scale). Zero and `ONE` both leave identity; the zero case
/// is retained behavior. Borrows one `VECTOR` from the initialized scratch stack
/// and releases it before returning. The two reset helpers retain the ordering
/// of the entry and shrinking updates.
static inline void _acropolisBridgeResetEnemyScaleDuringShrink(_AcropolisBridgeEnemyWork* work)
{
    BossStrangerWalker* walker;
    VECTOR*             scratchEnd;
    VECTOR*             scaleVector;
    s32                 scaleAmount;

    walker                   = &work->walker;
    scratchEnd               = SCRATCH_STACK_CURSOR(VECTOR);
    walker->scaleMtx.m[2][2] = ONE;
    walker->scaleMtx.m[1][1] = ONE;
    walker->scaleMtx.m[0][0] = ONE;
    scaleAmount              = walker->scale;
    walker->scaleMtx.m[2][1] = 0;
    walker->scaleMtx.m[2][0] = 0;
    walker->scaleMtx.m[1][2] = 0;
    walker->scaleMtx.m[1][0] = 0;
    walker->scaleMtx.m[0][2] = 0;
    walker->scaleMtx.m[0][1] = 0;
    walker->scaleMtx.t[2]    = 0;
    walker->scaleMtx.t[1]    = 0;
    walker->scaleMtx.t[0]    = 0;
    scaleVector              = (scratchEnd - 1);

    SCRATCH_STACK_CURSOR(VECTOR) = scaleVector;
    if (scaleAmount != 0 && scaleAmount != ONE) {
        scaleVector->vz        = scaleAmount;
        scaleVector->vy        = scaleAmount;
        ((scratchEnd - 1))->vx = scaleAmount;
        ScaleMatrix(&work->walker.scaleMtx, scaleVector);
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Patrols while sinking and shrinking the bridge enemy below its spawn height.
///
/// Requires a live model, enemy and work block. Entry disables the attack sphere,
/// keeps the receiving body enabled and resets patrol playback. The root offset
/// increases toward 1800 coordinate units and the Q12 scale shrinks toward half
/// size; their final steps are not clamped. A route arrival changes the walker's
/// speed tuning. Player root Y at least 701 selects the chase state.
static void _acropolisBridgeEnemyPatrolSunk(Task* task)
{
    _AcropolisBridgeEnemyWork* work;
    BossStrangerWalker*        walker;
    BossStrangerWalker*        arrivedWalker;
    Enemy*                     enemy;
    PlayerStatus*              player;

    player = &gPlayerStatus;
    work   = task->work;
    enemy  = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->walker.state            = BOSS_STRANGER_WALKER_PATROL;
        work->walker.routeData.cursor = 0;
        work->attack.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->body.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        _acropolisBridgeResetEnemyScaleOnEntry(work);
        work->walker.turnLimit = 0x60;
        walker                 = &work->walker;
        walker->speedTarget    = 0x20;
        walker->speed          = 0;
        walker->speedStep      = 1;
        work->animRequest      = ACROPOLIS_BRIDGE_ENEMY_ANIM_REQUEST_RESET;
        work->animId           = ACROPOLIS_BRIDGE_ENEMY_ANIM_PATROL;
        work->animRate         = ANIMATION_RATE_ONE;
    }
    if (work->walker.routeData.arrived == 1) {
        arrivedWalker              = &work->walker;
        arrivedWalker->speedTarget = 0x20;
        arrivedWalker->speed       = 0x60;
        arrivedWalker->speedStep   = 2;
    }
    // Sink and shrink without clamping the final steps.
    if (work->sinkDepth < ACROPOLIS_BRIDGE_ENEMY_SINK_LIMIT) {
        work->sinkDepth += 0x2D;
        task->extra.tmd->coords->coord.t[1] =
            work->baseHeight + work->sinkDepth;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->walker.scale >= 0x801) {
        work->walker.scale -= 0x33;
        _acropolisBridgeResetEnemyScaleDuringShrink(work);
    } else {
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    }
    _bossStrangerTick(&work->walker);
    _acropolisBridgeEnemyUpdateAnimation(task);
    if (player->coordMtx->t[1] >= ACROPOLIS_BRIDGE_ENEMY_CHASE_START_Y) {
        work->state = ACROPOLIS_BRIDGE_ENEMY_STATE_CHASE;
    }
}

/// Grows the bridge enemy's Q12 scale by 136 and rebuilds its uniform scale matrix.
///
/// The caller stops invoking this at full scale (`ONE`); the final addition can
/// overshoot and is not clamped. Zero and `ONE` leave identity. Borrows one
/// `VECTOR` from the initialized scratch stack and releases it before returning.
static inline void _acropolisBridgeGrowEnemyScale(_AcropolisBridgeEnemyWork* work)
{
    enum { ACROPOLIS_BRIDGE_ENEMY_SCALE_GROWTH_PER_UPDATE = 136 };
    BossStrangerWalker* walker;
    VECTOR*             scratchEnd;
    VECTOR*             scaleVector;
    s32                 scaleAmount;

    work->walker.scale      += ACROPOLIS_BRIDGE_ENEMY_SCALE_GROWTH_PER_UPDATE;
    walker                   = &work->walker;
    scratchEnd               = SCRATCH_STACK_CURSOR(VECTOR);
    walker->scaleMtx.m[2][2] = ONE;
    walker->scaleMtx.m[1][1] = ONE;
    walker->scaleMtx.m[0][0] = ONE;
    scaleAmount              = walker->scale;
    walker->scaleMtx.m[2][1] = 0;
    walker->scaleMtx.m[2][0] = 0;
    walker->scaleMtx.m[1][2] = 0;
    walker->scaleMtx.m[1][0] = 0;
    walker->scaleMtx.m[0][2] = 0;
    walker->scaleMtx.m[0][1] = 0;
    walker->scaleMtx.t[2]    = 0;
    walker->scaleMtx.t[1]    = 0;
    walker->scaleMtx.t[0]    = 0;
    scaleVector              = scratchEnd - 1;

    SCRATCH_STACK_CURSOR(VECTOR) = scaleVector;
    if (scaleAmount != 0 && scaleAmount != ONE) {
        scaleVector->vz      = scaleAmount;
        scaleVector->vy      = scaleAmount;
        (scratchEnd - 1)->vx = scaleAmount;
        ScaleMatrix(&work->walker.scaleMtx, scaleVector);
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Returns 1 when the bridge enemy's attack sphere has struck something, else 0.
///
/// Reads the live enemy work's one-entry attack contact table without clearing it.
static inline s16 _acropolisBridgeAttackLanded(const Task* task)
{
    const _AcropolisBridgeEnemyWork* work = task->work;

    if (work->attackContacts[0].key.value == 0) {
        return 0;
    }
    return 1;
}

/// Raises and grows the enemy while chasing with its attack sphere enabled.
///
/// Requires live enemy/model/work storage. Entry engages battle, configures the
/// chase and starts playback at five times normal speed, retaining current walker
/// speed. Each update reduces the sink offset and grows the Q12 scale without
/// clamping the final step. An attack contact selects retreat; player root Y
/// below 801 subsequently overrides that selection with submerged patrol.
static void _acropolisBridgeEnemyChase(Task* task)
{
    _AcropolisBridgeEnemyWork* work;
    BossStrangerWalker*        walker;
    Enemy*                     enemy;
    PlayerStatus*              player;
    u16                        entrySpeed;

    player = &gPlayerStatus;
    work   = task->work;
    if (work->stateEntered != 0) {
        enemy = task->spawnArg2.pointer;
        sceneEngageBattle(1);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        entrySpeed                    = work->walker.speed;
        walker                        = &work->walker;
        work->walker.turnLimit        = 0x100;
        walker->speedTarget           = 0xA0;
        walker->speedStep             = 6;
        walker->speed                 = entrySpeed;
        work->walker.state            = BOSS_STRANGER_WALKER_CHASE;
        work->attack.flags           |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->body.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->attack.key              = damagePackEnemyAttackKey(enemy, 0);
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ACROPOLIS_BRIDGE_ENEMY_ANIM_REQUEST_RESET;
        work->animId                  = ACROPOLIS_BRIDGE_ENEMY_ANIM_CHASE;
        work->animRate                = 5 * ANIMATION_RATE_ONE;
    }
    if (work->sinkDepth > 0) {
        work->sinkDepth -= 0x3C;
        task->extra.tmd->coords->coord.t[1] =
            work->baseHeight + work->sinkDepth;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->walker.scale < ONE) {
        _acropolisBridgeGrowEnemyScale(work);
    }
    _bossStrangerTick(&work->walker);
    _acropolisBridgeEnemyUpdateAnimation(task);
    // The player-height gate deliberately overrides an attack-contact transition.
    if (_acropolisBridgeAttackLanded(task)) {
        work->state = ACROPOLIS_BRIDGE_ENEMY_STATE_RETREAT;
    }
    if (player->coordMtx->t[1] < ACROPOLIS_BRIDGE_ENEMY_CHASE_KEEP_Y) {
        work->state = ACROPOLIS_BRIDGE_ENEMY_STATE_PATROL_SUNK;
    }
}

/// Sinks and shrinks the enemy after its attack while resuming its patrol route.
///
/// Requires live enemy/model/work storage. Entry disables the attack sphere,
/// resets the route cursor and restarts patrol playback. The sink offset and Q12
/// scale retain their unclamped final steps. Once the route cursor advances,
/// player root Y below 801 selects submerged patrol; otherwise chase resumes.
static void _acropolisBridgeEnemyRetreat(Task* task)
{
    _AcropolisBridgeEnemyWork* work;
    BossStrangerWalker*        walker;
    Enemy*                     enemy;
    PlayerStatus*              player;

    player = &gPlayerStatus;
    work   = task->work;
    enemy  = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->walker.state            = BOSS_STRANGER_WALKER_PATROL;
        work->walker.routeData.cursor = 0;
        work->attack.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        _acropolisBridgeResetEnemyScaleOnEntry(work);
        work->walker.turnLimit = 0x200;
        walker                 = &work->walker;
        walker->speedTarget    = 0x20;
        walker->speed          = 0x80;
        walker->speedStep      = 3;
        work->animRequest      = ACROPOLIS_BRIDGE_ENEMY_ANIM_REQUEST_RESET;
        work->animId           = ACROPOLIS_BRIDGE_ENEMY_ANIM_PATROL;
        work->animRate         = ANIMATION_RATE_ONE;
    }
    if (work->sinkDepth < ACROPOLIS_BRIDGE_ENEMY_SINK_LIMIT) {
        work->sinkDepth += 0x2D;
        task->extra.tmd->coords->coord.t[1] =
            work->baseHeight + work->sinkDepth;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->walker.scale >= 0x500) {
        work->walker.scale -= 0x46;
        _acropolisBridgeResetEnemyScaleDuringShrink(work);
    } else if (enemy->node.state.parts.flags == 0) {
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    }
    _bossStrangerTick(&work->walker);
    _acropolisBridgeEnemyUpdateAnimation(task);
    // Finish retreat only after the patrol route has advanced.
    if (work->walker.routeData.cursor != 0) {
        if (player->coordMtx->t[1] < ACROPOLIS_BRIDGE_ENEMY_CHASE_KEEP_Y) {
            work->state = ACROPOLIS_BRIDGE_ENEMY_STATE_PATROL_SUNK;
        } else {
            work->state = ACROPOLIS_BRIDGE_ENEMY_STATE_CHASE;
        }
    }
}

/// Returns whether the enemy's three-entry contact prefix contains a player body.
///
/// Player and companion bodies share this key category. Scanning stops at the
/// first zero key, so entries beyond a gap are deliberately ignored. The caller
/// borrows the live `bodyContacts` array; no entries or pointers are changed.
static inline s32 _acropolisBridgeEnemyHasPlayerContact(const WorldCollisionContact* contacts)
{
    enum { ACROPOLIS_BRIDGE_ENEMY_BODY_CONTACT_COUNT = 3 };
    s16 contactIndex;

    for (contactIndex = 0; contactIndex < ACROPOLIS_BRIDGE_ENEMY_BODY_CONTACT_COUNT; contactIndex++) {
        if (contacts[contactIndex].key.value == 0) {
            return 0;
        }
        if ((contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
            return 1;
        }
    }
    return 0;
}

/// Queues a positional sound with the enemy's placement index as its instance tag.
///
/// `baseSoundId` supplies the packed bank and entry, with a zero instance byte.
/// The three bridge placements OR their index into that byte. Pan and attenuation
/// come from the model root and retain their signed-byte narrowing. Sound
/// resources and the live `task`/model must remain valid for the request.
static inline void _acropolisBridgePlayEnemySound(Task* task, Enemy* enemy, s32 baseSoundId)
{
    enum { ACROPOLIS_BRIDGE_SOUND_INSTANCE_SHIFT = 8 };
    s32 soundId;
    s32 panOffset;

    soundId   = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << ACROPOLIS_BRIDGE_SOUND_INSTANCE_SHIFT) | baseSoundId;
    panOffset = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, panOffset,
                             (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
}

/// Updates the movie-driven root height, yaw and unclamped Q12 presentation scale.
///
/// Requires the live model, enemy work and movie frame. Uses a signed root Y
/// sample minus 200; scale is `ONE` below Y 500 and loses four Q12 units per
/// additional coordinate unit. Yaw uses 4096 units per turn. Marks the root dirty.
static inline void _acropolisBridgeUpdateMovieEnemyTransform(Task* task, _AcropolisBridgeEnemyWork* work)
{
    VECTOR scale;
    s32    scaleAmount;
    s32    heightForScale;

    task->extra.tmd->coords->coord.t[1] =
        _acropolisBridgeGetMovieEnemyHeight() - ACROPOLIS_BRIDGE_ENEMY_PRESENTATION_PATH_Y_OFFSET;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    heightForScale                        = task->extra.tmd->coords->coord.t[1];
    if (heightForScale < ACROPOLIS_BRIDGE_ENEMY_PRESENTATION_SCALE_START_Y) {
        scaleAmount = ONE;
        scale.vx = scale.vy = scale.vz = scaleAmount;
    } else {
        scaleAmount     = ONE;
        heightForScale -= ACROPOLIS_BRIDGE_ENEMY_PRESENTATION_SCALE_START_Y;
        heightForScale *= 4;
        scaleAmount    -= heightForScale;
        scale.vx = scale.vy = scale.vz = scaleAmount;
    }
    gfxRotMatrixY(&task->extra.tmd->coords->coord, work->yaw, 1);
    ScaleMatrix(&task->extra.tmd->coords->coord, &scale);
}

/// Positions and animates the enemy in step with the keypad movie.
///
/// Requires live enemy/model/work storage and the captured keypad movie frame.
/// Entry disables attacks, initializes presentation colors and yaw and selects the X/Z
/// position for placements 0..2. Root Y follows the movie path minus 200 units.
/// Height controls an unclamped Q12 scale; playback completion and random rolls
/// restart the reaction clip. Its fifth update randomizes yaw. Contact with
/// a player or companion enters the death-effect state, sounding contact only
/// while HP remains positive.
static void _acropolisBridgeEnemyFollowMovie(Task* task)
{
    _AcropolisBridgeEnemyWork* work;
    _AcropolisBridgeEnemyWork* reloadedWork;
    Enemy*                     enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    // Select the placement-specific movie origin and fixed presentation colors.
    if (work->stateEntered != 0) {
        work->attack.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        enemy->node.state.parts.flags = 0;
        work->animRate                = 2 * ANIMATION_RATE_ONE;
        work->animRequest             = ACROPOLIS_BRIDGE_ENEMY_ANIM_REQUEST_RESET;
        work->animId                  = ACROPOLIS_BRIDGE_ENEMY_ANIM_PATROL;
        switch (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) {
            case 0:
                task->extra.tmd->coords->coord.t[0] = -0x22C4;
                task->extra.tmd->coords->coord.t[2] = -0x640;
                break;
            case 1:
                task->extra.tmd->coords->coord.t[0] = -0x270F;
                task->extra.tmd->coords->coord.t[2] = -0x7D0;
                break;
            case 2:
                task->extra.tmd->coords->coord.t[0] = -0x2EE0;
                task->extra.tmd->coords->coord.t[2] = -0x5DC;
                break;
        }
        work->colorMtx.t[1]    = 0x80;
        work->colorMtx.t[0]    = 0x80;
        work->colorMtx.t[2]    = 0x5A0;
        work->colorMtx.m[2][1] = 0xC0;
        work->colorMtx.m[2][0] = 0xC0;
        work->colorMtx.m[2][2] = 0x5A0;
        work->colorMtx.m[1][1] = 0xC0;
        work->colorMtx.m[1][0] = 0xC0;
        work->colorMtx.m[1][2] = 0x5A0;
        work->colorMtx.m[0][1] = 0xC0;
        work->colorMtx.m[0][0] = 0xC0;
        work->colorMtx.m[0][2] = 0x5A0;
        gRandomLcgState        = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->yaw              = gRandomLcgState >> 16;
    }
    _acropolisBridgeUpdateMovieEnemyTransform(task, work);
    if (task->extra.tmd->coords->coord.t[1] < ACROPOLIS_BRIDGE_ENEMY_PRESENTATION_SCALE_START_Y &&
        work->animId != ACROPOLIS_BRIDGE_ENEMY_ANIM_REACTION) {
        work->animRequest = ACROPOLIS_BRIDGE_ENEMY_ANIM_REQUEST_RESET;
        work->animId      = ACROPOLIS_BRIDGE_ENEMY_ANIM_REACTION;
        _acropolisBridgePlayEnemySound(task, enemy, ACROPOLIS_BRIDGE_ENEMY_FALL_ANIMATION_SOUND);
    }
    if (work->animId == ACROPOLIS_BRIDGE_ENEMY_ANIM_REACTION) {
        if (task->extra.tmd->coords->coord.t[1] >= ACROPOLIS_BRIDGE_ENEMY_PRESENTATION_RETRY_MIN_Y &&
            (s32)((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) + ACROPOLIS_BRIDGE_ENEMY_PRESENTATION_RETRY_TICKS) < work->animFrame) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 16) & 0xF) == 0) {
                work->animRate    = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) * 2) + ANIMATION_RATE_ONE;
                work->animRequest = ACROPOLIS_BRIDGE_ENEMY_ANIM_REQUEST_RESET;
                work->animId      = ACROPOLIS_BRIDGE_ENEMY_ANIM_REACTION;
                _acropolisBridgePlayEnemySound(task, enemy, ACROPOLIS_BRIDGE_ENEMY_FALL_ANIMATION_SOUND);
            }
        }
    }
    if (task->extra.tmd->coords->coord.t[1] < ACROPOLIS_BRIDGE_ENEMY_PRESENTATION_POSE_MAX_Y) {
        reloadedWork = task->work;
        if (reloadedWork->rig.slots[1].currentPose.indices.recordIndex == reloadedWork->rig.slots[1].nextPose.indices.recordIndex) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 16) & 0x1F) == 0) {
                work->animRate    = ANIMATION_RATE_ONE;
                work->animRequest = ACROPOLIS_BRIDGE_ENEMY_ANIM_REQUEST_RESET;
                work->animId      = ACROPOLIS_BRIDGE_ENEMY_ANIM_REACTION;
                _acropolisBridgePlayEnemySound(task, enemy, ACROPOLIS_BRIDGE_ENEMY_FALL_ANIMATION_SOUND);
            }
        }
    }
    // Compare the adjacent clip and update halfwords with one word load.
    if (*(s32*)&work->animId == ACROPOLIS_BRIDGE_ENEMY_ANIM_YAW_REROLL_WORD) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->yaw       = gRandomLcgState >> 16;
    }
    // Player and companion bodies trigger the death presentation.
    if (_acropolisBridgeEnemyHasPlayerContact(work->bodyContacts) != 0) {
        if (work->hp > 0) {
            _acropolisBridgePlayEnemySound(task, enemy, ACROPOLIS_BRIDGE_ENEMY_CONTACT_SOUND);
        }
        work->state = ACROPOLIS_BRIDGE_ENEMY_STATE_DEATH_EFFECT;
    }
    _acropolisBridgeEnemyUpdateAnimation(task);
}

/// Animates the enemy at its existing placement after the keypad movie.
///
/// Requires live enemy/model/work storage. Entry disables attacks and initializes
/// presentation colors, playback and yaw without relocating the root. Root Y controls
/// an unclamped Q12 scale; playback completion and random rolls restart the
/// reaction clip. Its fifth update randomizes yaw. Contact with a player or
/// companion enters the death-effect state and sounds contact if HP is positive.
/// This handler does not integrate vertical movement.
static void _acropolisBridgeEnemyAnimateAtPlacement(Task* task)
{
    _AcropolisBridgeEnemyWork* work;
    _AcropolisBridgeEnemyWork* reloadedWork;
    Enemy*                     enemy;
    VECTOR                     scale;
    s32                        scaleAmount;
    s32 // Derive the scale from the existing root height.
        heightForScale;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    // Initialize playback and colors while retaining the placed root.
    if (work->stateEntered != 0) {
        work->attack.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        enemy->node.state.parts.flags = 0;
        work->animRate                = 2 * ANIMATION_RATE_ONE;
        work->animRequest             = ACROPOLIS_BRIDGE_ENEMY_ANIM_REQUEST_RESET;
        work->animId                  = ACROPOLIS_BRIDGE_ENEMY_ANIM_PATROL;
        work->colorMtx.t[1]           = 0x80;
        work->colorMtx.t[0]           = 0x80;
        work->colorMtx.t[2]           = 0x5A0;
        work->colorMtx.m[2][1]        = 0xC0;
        work->colorMtx.m[2][0]        = 0xC0;
        work->colorMtx.m[2][2]        = 0x5A0;
        work->colorMtx.m[1][1]        = 0xC0;
        work->colorMtx.m[1][0]        = 0xC0;
        work->colorMtx.m[1][2]        = 0x5A0;
        work->colorMtx.m[0][1]        = 0xC0;
        work->colorMtx.m[0][0]        = 0xC0;
        work->colorMtx.m[0][2]        = 0x5A0;
        gRandomLcgState               = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->yaw                     = gRandomLcgState >> 16;
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    heightForScale                        = task->extra.tmd->coords->coord.t[1];
    if (heightForScale < ACROPOLIS_BRIDGE_ENEMY_PRESENTATION_SCALE_START_Y) {
        scaleAmount = ONE;
        scale.vx = scale.vy = scale.vz = scaleAmount;
    } else {
        scaleAmount     = ONE;
        heightForScale -= ACROPOLIS_BRIDGE_ENEMY_PRESENTATION_SCALE_START_Y;
        heightForScale *= 4;
        scaleAmount    -= heightForScale;
        scale.vx = scale.vy = scale.vz = scaleAmount;
    }
    gfxRotMatrixY(&task->extra.tmd->coords->coord, work->yaw, 1);
    ScaleMatrix(&task->extra.tmd->coords->coord, &scale);
    if (task->extra.tmd->coords->coord.t[1] < ACROPOLIS_BRIDGE_ENEMY_PRESENTATION_SCALE_START_Y &&
        work->animId != ACROPOLIS_BRIDGE_ENEMY_ANIM_REACTION) {
        work->animRequest = ACROPOLIS_BRIDGE_ENEMY_ANIM_REQUEST_RESET;
        work->animId      = ACROPOLIS_BRIDGE_ENEMY_ANIM_REACTION;
        _acropolisBridgePlayEnemySound(task, enemy, ACROPOLIS_BRIDGE_ENEMY_FALL_ANIMATION_SOUND);
    }
    if (work->animId == ACROPOLIS_BRIDGE_ENEMY_ANIM_REACTION) {
        if (task->extra.tmd->coords->coord.t[1] >= ACROPOLIS_BRIDGE_ENEMY_PRESENTATION_RETRY_MIN_Y &&
            (s32)((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) + ACROPOLIS_BRIDGE_ENEMY_PRESENTATION_RETRY_TICKS) < work->animFrame) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 16) & 0xF) == 0) {
                work->animRate    = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) * 2) + ANIMATION_RATE_ONE;
                work->animRequest = ACROPOLIS_BRIDGE_ENEMY_ANIM_REQUEST_RESET;
                work->animId      = ACROPOLIS_BRIDGE_ENEMY_ANIM_REACTION;
                _acropolisBridgePlayEnemySound(task, enemy, ACROPOLIS_BRIDGE_ENEMY_FALL_ANIMATION_SOUND);
            }
        }
    }
    if (task->extra.tmd->coords->coord.t[1] < ACROPOLIS_BRIDGE_ENEMY_PRESENTATION_POSE_MAX_Y) {
        reloadedWork = task->work;
        if (reloadedWork->rig.slots[1].currentPose.indices.recordIndex == reloadedWork->rig.slots[1].nextPose.indices.recordIndex) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 16) & 0x1F) == 0) {
                work->animRate    = ANIMATION_RATE_ONE;
                work->animRequest = ACROPOLIS_BRIDGE_ENEMY_ANIM_REQUEST_RESET;
                work->animId      = ACROPOLIS_BRIDGE_ENEMY_ANIM_REACTION;
                _acropolisBridgePlayEnemySound(task, enemy, ACROPOLIS_BRIDGE_ENEMY_FALL_ANIMATION_SOUND);
            }
        }
    }
    // Compare the adjacent clip and update halfwords with one word load.
    if (*(s32*)&work->animId == ACROPOLIS_BRIDGE_ENEMY_ANIM_YAW_REROLL_WORD) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->yaw       = gRandomLcgState >> 16;
    }
    // Player and companion bodies trigger the death presentation.
    if (_acropolisBridgeEnemyHasPlayerContact(work->bodyContacts) != 0) {
        if (work->hp > 0) {
            _acropolisBridgePlayEnemySound(task, enemy, ACROPOLIS_BRIDGE_ENEMY_CONTACT_SOUND);
        }
        work->state = ACROPOLIS_BRIDGE_ENEMY_STATE_DEATH_EFFECT;
    }
    _acropolisBridgeEnemyUpdateAnimation(task);
}

/// Bobs the defeated enemy and pushes it away from the player on body contact.
///
/// Requires live enemy/model/work storage. Entry disables attacks, retains body
/// contacts, prevents lock-on and blends into the bobbing clip. The sink offset
/// eases toward zero without clamping, with a sine offset added to root Y.
/// On player/companion contact the root turns by 16 angle units and moves in X/Z
/// by the negative normalized 3D player offset scaled to 16 coordinate units.
/// Angles use 4096 units per turn; the displacement retains halfword truncation.
static void _acropolisBridgeEnemyDefeatedBob(Task* task)
{
    _AcropolisBridgeEnemyWork* work;
    Enemy*                     enemy;
    GfxCoord*                  coord;
    SVECTOR                    displacement;
    SVECTOR*                   displacementPtr;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->attack.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->body.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->animRequest             = ACROPOLIS_BRIDGE_ENEMY_ANIM_REQUEST_BLEND;
        work->animId                  = ACROPOLIS_BRIDGE_ENEMY_ANIM_BOB;
        work->animRate                = ANIMATION_RATE_ONE;
        gRandomLcgState               = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gfxRotMatrixY(&task->extra.tmd->coords->coord, gRandomLcgState >> 16, 1);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->sinkDepth > 0) {
        work->sinkDepth -= 3;
    }
    task->extra.tmd->coords->coord.t[1] = work->baseHeight + work->sinkDepth;
    task->extra.tmd->coords->coord.t[1] +=
        rsin((gDisplayState.animFrame << 5) + task->extra.tmd->coords->coord.t[0]) >> 6;
    // Player or companion contact triggers a push away from the player root.
    if (_acropolisBridgeEnemyHasPlayerContact(work->bodyContacts) != 0) {
        coord               = task->extra.tmd->coords;
        displacement.vx     = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
        displacementPtr     = &displacement;
        displacementPtr->vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
        displacementPtr->vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
        gfxRotMatrixY(&task->extra.tmd->coords->coord, 0x10, 0);
        VectorNormalSS(displacementPtr, displacementPtr);
        gte_lddp(-0x10);
        gte_ldsv(displacementPtr);
        gte_gpf12();
        gte_stsv(displacementPtr);
        task->extra.tmd->coords->coord.t[0] += displacement.vx;
        task->extra.tmd->coords->coord.t[2] += displacement.vz;
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _acropolisBridgeEnemyUpdateAnimation(task);
}

/// Plays the defeated enemy's collapse animation and timed burn/fade.
///
/// Requires live work, enemy and a model with coordinate 2. Entry allocates
/// primitive buffers, disables both collision pairs and lock-on, blends clip 5
/// and randomizes root yaw. Frames 10/22/28/34 start burning, darken, enable
/// translucency and hide drawing. Animation and the counter stop at frame 101;
/// this state retains the task and work and does not credit rewards.
static void _acropolisBridgeEnemyCollapse(Task* task)
{
    enum {
        ACROPOLIS_BRIDGE_ENEMY_COLLAPSE_ANIMATION         = 5,
        ACROPOLIS_BRIDGE_ENEMY_COLLAPSE_BURN_FRAME        = 10,
        ACROPOLIS_BRIDGE_ENEMY_COLLAPSE_BLACK_FRAME       = 22,
        ACROPOLIS_BRIDGE_ENEMY_COLLAPSE_TRANSLUCENT_FRAME = 28,
        ACROPOLIS_BRIDGE_ENEMY_COLLAPSE_HIDE_FRAME        = 34,
        ACROPOLIS_BRIDGE_ENEMY_COLLAPSE_END_FRAME         = 101
    };
    _AcropolisBridgeEnemyWork* work  = task->work;
    Enemy*                     enemy = task->spawnArg2.pointer;
    s32                        deathFrame;

    if (work->stateEntered != 0) {
        tmdAllocPrimitiveBuffer(task->extra.tmd);
        work->attack.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->animRequest             = ACROPOLIS_BRIDGE_ENEMY_ANIM_REQUEST_BLEND;
        work->animId                  = ACROPOLIS_BRIDGE_ENEMY_COLLAPSE_ANIMATION;
        work->animRate                = ANIMATION_RATE_ONE;
        gRandomLcgState               = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gfxRotMatrixY(&task->extra.tmd->coords->coord, gRandomLcgState >> 16, 1);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        worldTargetDisableNodeLockOn(&enemy->node);
        if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_IDLE && gSceneCombatState.battleRefs != 0) {
            sceneEngageBattle(1);
        }
        work->deathFrame = 0;
    }
    // Advance the pose and timed burn/fade through frame 101, then hold.
    if (work->deathFrame < ACROPOLIS_BRIDGE_ENEMY_COLLAPSE_END_FRAME) {
        work->deathFrame++;
        _acropolisBridgeEnemyUpdateAnimation(task);
        deathFrame = work->deathFrame;
        switch (deathFrame) {
            case ACROPOLIS_BRIDGE_ENEMY_COLLAPSE_BURN_FRAME:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                effectSpawn(EFFECT_CORPSE_BURN, &task->extra.tmd->coords[2], 1, NULL);
                break;
            case ACROPOLIS_BRIDGE_ENEMY_COLLAPSE_TRANSLUCENT_FRAME:
                task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                break;
            case ACROPOLIS_BRIDGE_ENEMY_COLLAPSE_BLACK_FRAME:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                break;
            case ACROPOLIS_BRIDGE_ENEMY_COLLAPSE_HIDE_FRAME:
                task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                break;
        }
    }
}

/// Starts the bridge enemy's hit/burn death effect and timed fade.
///
/// Requires live enemy/work and model coordinate 1. Entry disables collision
/// and lock-on, engages a pending battle and credits rewards only with positive
/// enemy HP. Spawns the weapon-row-1 hit effect and corpse burn, starts vibration,
/// then changes model flags at frame 2, darkens at 30 and hides at 44. The frame
/// counter and animation stop at 101; this state retains the task and work.
static void _acropolisBridgeEnemyDeathEffect(Task* task)
{
    enum {
        ACROPOLIS_BRIDGE_ENEMY_DEATH_MOTOR_FRAMES          = 3,
        ACROPOLIS_BRIDGE_ENEMY_DEATH_MOTOR_START_INTENSITY = 255,
        ACROPOLIS_BRIDGE_ENEMY_DEATH_MOTOR_END_INTENSITY   = 8,
        ACROPOLIS_BRIDGE_ENEMY_DEATH_FLAGS_FRAME           = 2,
        ACROPOLIS_BRIDGE_ENEMY_DEATH_BLACK_FRAME           = 30,
        ACROPOLIS_BRIDGE_ENEMY_DEATH_HIDE_FRAME            = 44,
        ACROPOLIS_BRIDGE_ENEMY_DEATH_END_FRAME             = 101,
        ACROPOLIS_BRIDGE_ENEMY_DEATH_HIT_ATTACK_KEY        = 0x1001,
        ACROPOLIS_BRIDGE_ENEMY_DEATH_HIT_ARGUMENT_LOW      = 160,
        ACROPOLIS_BRIDGE_ENEMY_DEATH_HIT_EXTRA_PUFF_COUNT  = 2
    };
    _AcropolisBridgeEnemyWork* work  = task->work;
    Enemy*                     enemy = task->spawnArg2.pointer;
    s32                        deathFrame;

    if (work->stateEntered != 0) {
        work->attack.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        worldTargetDisableNodeLockOn(&enemy->node);
        if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_IDLE && gSceneCombatState.battleRefs != 0) {
            sceneEngageBattle(1);
        }
        if (enemy->hp > 0) {
            sceneReleaseBattleRefWithRewards(task, 0x29);
        }
        work->effectArg.coord      = &task->extra.tmd->coords[1];
        work->effectArg.spawnArgLo = ACROPOLIS_BRIDGE_ENEMY_DEATH_HIT_ARGUMENT_LOW;
        work->effectArg.spawnArgHi = ACROPOLIS_BRIDGE_ENEMY_DEATH_HIT_EXTRA_PUFF_COUNT;
        effectSpawnHit(damageGetPlayerAttackEffectId(ACROPOLIS_BRIDGE_ENEMY_DEATH_HIT_ATTACK_KEY), &task->extra.tmd->coords[1], NULL,
                       &work->effectArg);
        worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
        effectSpawn(EFFECT_CORPSE_BURN, &task->extra.tmd->coords[1], 1, NULL);
        task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
        work->deathFrame       = 0;
        padScriptSpawnVariableMotorRamp(ACROPOLIS_BRIDGE_ENEMY_DEATH_MOTOR_FRAMES,
                                        ACROPOLIS_BRIDGE_ENEMY_DEATH_MOTOR_START_INTENSITY,
                                        ACROPOLIS_BRIDGE_ENEMY_DEATH_MOTOR_END_INTENSITY);
    }
    if (work->deathFrame < ACROPOLIS_BRIDGE_ENEMY_DEATH_END_FRAME) {
        work->deathFrame++;
        _acropolisBridgeEnemyUpdateAnimation(task);
        deathFrame = work->deathFrame;
        switch (deathFrame) {
            case ACROPOLIS_BRIDGE_ENEMY_DEATH_FLAGS_FRAME:
                // Frame 2 also supplies TMD_OBJECT_SEMI_TRANS.
                task->extra.tmd->flags = deathFrame;
                break;
            case ACROPOLIS_BRIDGE_ENEMY_DEATH_BLACK_FRAME:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                break;
            case ACROPOLIS_BRIDGE_ENEMY_DEATH_HIDE_FRAME:
                task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                break;
        }
    }
}

/// Accounts for a hit's healing credit, damage readout and enemy HP loss.
///
/// Borrows live work and enemy storage; `damage` is nonnegative HP and
/// `attackKey` is the player's packed attack key. Life Drain sees pre-hit HP.
/// Both subtractions narrow to signed halfwords without clamping; the work's
/// remaining HP then replaces the enemy's. The readout borrows the enemy node.
static inline void _acropolisBridgeEnemyAccountHitHp(_AcropolisBridgeEnemyWork* work, Enemy* enemy, u32 attackKey, s32 damage)
{
    damageAccumulateLifeDrainHp(enemy, attackKey, damage, 0);
    enemy->hp -= damage;
    worldTargetAddReadoutAmount(&enemy->node, damage, 0);
    work->hp -= damage;
    enemy->hp = work->hp;
}

/// Applies an attack contact's HP damage and selects the enemy's defeat behavior.
///
/// Requires live enemy/work and model coordinate 1. `attackKey` is a packed
/// category-2 collision key with valid weapon/PE rows. Damage uses distance
/// zero and no reaction scaling, is quadrupled for a critical hit, credits Life Drain
/// and the target readout, then narrows into both HP halfwords. A lethal hit
/// credits rewards only while a battle reference remains. Already-defeated
/// calls skip damage/effects: bobbing and collapse stay active, movie-following
/// or placement animation selects collapse, and every other state selects bobbing.
static void _acropolisBridgeEnemyApplyHit(Task* task, u32 attackKey)
{
    enum {
        ACROPOLIS_BRIDGE_ENEMY_HIT_ARGUMENT_LOW       = 128,
        ACROPOLIS_BRIDGE_ENEMY_HIT_ARGUMENT_HIGH      = 2,
        ACROPOLIS_BRIDGE_ENEMY_CRITICAL_DAMAGE_FACTOR = 4,
        ACROPOLIS_BRIDGE_ENEMY_UNUSED_REACTION_SCALE  = 4096,
        ACROPOLIS_BRIDGE_ENEMY_REWARD_ARGUMENT        = 0x29
    };
    _AcropolisBridgeEnemyWork* work  = task->work;
    Enemy*                     enemy = task->spawnArg2.pointer;
    s32                        damage;
    s16                        state;

    // Account for the hit before choosing the defeat presentation.
    if (work->hp > 0) {
        damage                     = damageComputePlayerAttack(attackKey, 0, 0, ACROPOLIS_BRIDGE_ENEMY_UNUSED_REACTION_SCALE);
        work->effectArg.coord      = &task->extra.tmd->coords[1];
        work->effectArg.spawnArgLo = ACROPOLIS_BRIDGE_ENEMY_HIT_ARGUMENT_LOW;
        work->effectArg.spawnArgHi = ACROPOLIS_BRIDGE_ENEMY_HIT_ARGUMENT_HIGH;
        effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), &task->extra.tmd->coords[1],
                       NULL, &work->effectArg);
        if (damageRollCriticalHit(enemy, attackKey, 0) != 0) {
            damage *= ACROPOLIS_BRIDGE_ENEMY_CRITICAL_DAMAGE_FACTOR;
            effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[1], 0, NULL);
        }
        _acropolisBridgeEnemyAccountHitHp(work, enemy, attackKey, damage);
        if (work->hp > 0) {
            return;
        }
        if (gSceneCombatState.battleRefs != 0) {
            sceneReleaseBattleRefWithRewards(task, ACROPOLIS_BRIDGE_ENEMY_REWARD_ARGUMENT);
        }
        if (work->hp > 0) {
            return;
        }
    }
    // Preserve active defeat states; a falling enemy collapses instead of bobbing.
    state = work->state;
    if (state < ACROPOLIS_BRIDGE_ENEMY_STATE_DEATH_EFFECT) {
        if (state >= ACROPOLIS_BRIDGE_ENEMY_STATE_DEFEATED_BOB) {
            return;
        }
        if (state != ACROPOLIS_BRIDGE_ENEMY_STATE_FOLLOW_MOVIE) {
            work->state = ACROPOLIS_BRIDGE_ENEMY_STATE_DEFEATED_BOB;
            return;
        }
        work->state = ACROPOLIS_BRIDGE_ENEMY_STATE_COLLAPSE;
    } else {
        if (state != ACROPOLIS_BRIDGE_ENEMY_STATE_ANIMATE_AT_PLACEMENT) {
            work->state = ACROPOLIS_BRIDGE_ENEMY_STATE_DEFEATED_BOB;
            return;
        }
        work->state = ACROPOLIS_BRIDGE_ENEMY_STATE_COLLAPSE;
    }
}

#include "../../shared/room/room_find_attack_contact.inc.c"

/// Marks whether this update enters a different bridge-enemy behavior state.
///
/// Borrows initialized writable enemy work. A changed state sets the signed
/// halfword entry latch to 1; an unchanged state clears it to 0. Commits the
/// current state as the next comparison after hit processing, so hit-driven
/// transitions receive entry handling too. Initial previous state -1 marks
/// the first active update as an entry. No pointer is retained.
static inline void _acropolisBridgeTrackEnemyStateEntry(_AcropolisBridgeEnemyWork* work)
{
    if (work->prevState != work->state) {
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = work->state;
}

/// Updates the bridge enemy's lighting, visibility, contacts and behavior.
///
/// Requires the live enemy, its model and `_AcropolisBridgeEnemyWork` in
/// `task->work`, with behavior state 0..8. Paused actors only clear active
/// contacts; hidden actors also suppress drawing. Running actors synchronize
/// primitive-buffer ownership with views 8/22, take the first attack contact,
/// mark state entry and dispatch behavior. With no battle references, only
/// defeated-bobbing/collapse states survive; other states become inactive.
/// Borrows one `_AcropolisBridgeHitScratch` and releases it after dispatch.
static void _acropolisBridgeEnemyFrame(Enemy* enemy, Task* task)
{
    _AcropolisBridgeEnemyWork*  work;
    _AcropolisBridgeEnemyWork*  currentWork;
    _AcropolisBridgeHitScratch* hitScratch;
    TmdObject*                  model;
    VECTOR                      worldPosition;
    s32                         mappedView;
    s32                         attackKey;
    u16                         state;

    // Refresh lighting even while actor behavior is paused.
    work                                  = task->work;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
    worldPosition.vx = task->extra.tmd->coords->workm.t[0];
    worldPosition.vy = task->extra.tmd->coords->workm.t[1];
    worldPosition.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(enemy, &worldPosition, 0, 0);

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            state = (u16)work->state;
            if ((u32)(state - ACROPOLIS_BRIDGE_ENEMY_STATE_COLLAPSE) >= 2U) {
                if (state != ACROPOLIS_BRIDGE_ENEMY_STATE_INACTIVE) {
                    mappedView = viewGetMappedIndex() & 0xFF;
                    switch (mappedView) {
                        case 8:
                            if ((s32)work->syncedView != mappedView) {
                                task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                                break;
                            }
                            task->extra.tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                            model                   = task->extra.tmd;
                            if (model->buffer != NULL) {
                                tmdFreePrimitiveBuffer(model);
                            }
                            break;
                        case 22:
                            if (work->state == ACROPOLIS_BRIDGE_ENEMY_STATE_FOLLOW_MOVIE) {
                                task->extra.tmd->flags = 0;
                                break;
                            }
                            task->extra.tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                            model                   = task->extra.tmd;
                            if (model->buffer != NULL) {
                                tmdFreePrimitiveBuffer(model);
                            }
                            break;
                        default:
                            tmdAllocPrimitiveBuffer(task->extra.tmd);
                            task->extra.tmd->flags = 0;
                            break;
                    }
                    work->syncedView = viewGetMappedIndex() & 0xFF;
                }
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            state = (u16)work->state;
            if ((u32)(state - ACROPOLIS_BRIDGE_ENEMY_STATE_COLLAPSE) >= 2U && state != ACROPOLIS_BRIDGE_ENEMY_STATE_INACTIVE) {
                worldCollisionClearContacts(&work->bodyContacts[0]);
                worldCollisionClearContacts(&work->attackContacts[0]);
            }
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            worldCollisionClearContacts(&work->bodyContacts[0]);
            worldCollisionClearContacts(&work->attackContacts[0]);
            return;
    }

    // Consume the first attack contact before dispatching the behavior state.
    hitScratch      = SCRATCH_STACK_RESERVE_BLOCK(_AcropolisBridgeHitScratch);
    attackKey       = _roomFindAttackContact(&hitScratch->point, work->bodyContacts, ARRAY_SIZE(work->bodyContacts));
    hitScratch->key = attackKey;
    if (attackKey != 0) {
        _acropolisBridgeEnemyApplyHit(task, attackKey);
    }

    currentWork = task->work;
    _acropolisBridgeTrackEnemyStateEntry(currentWork);
    _gAcropolisBridgeEnemyStateHandlers[work->state](task);
    worldCollisionClearContacts(&work->bodyContacts[0]);
    worldCollisionClearContacts(&work->attackContacts[0]);
    if (gSceneCombatState.battleRefs == 0) {
        if ((u32)((u16)work->state - ACROPOLIS_BRIDGE_ENEMY_STATE_DEFEATED_BOB) >= 2U) {
            work->state = ACROPOLIS_BRIDGE_ENEMY_STATE_INACTIVE;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_AcropolisBridgeHitScratch);
}

/// Applies the bridge enemy's model-draw request and returns 1.
///
/// Zero replaces the flags with `TMD_OBJECT_SKIP_ACTIVE_DRAW`. Bit 0 takes
/// precedence and clears all flags; otherwise bit 1 adds
/// `TMD_OBJECT_SKIP_AUTO_BUFFER`. Other nonzero requests leave the flags alone.
/// Requires the live model; does not allocate or release its primitive buffer.
static s32 _acropolisBridgeEnemySetModelDraw(Task* task, s32 messageId, s32 drawRequest, s32 unusedArg)
{
    enum {
        ACROPOLIS_BRIDGE_ENEMY_DRAW_HIDE             = 0,
        ACROPOLIS_BRIDGE_ENEMY_DRAW_SHOW             = 1,
        ACROPOLIS_BRIDGE_ENEMY_DRAW_SKIP_AUTO_BUFFER = 2
    };
    TmdObject* model;

    model = task->extra.tmd;
    if (drawRequest == ACROPOLIS_BRIDGE_ENEMY_DRAW_HIDE) {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else if (drawRequest & ACROPOLIS_BRIDGE_ENEMY_DRAW_SHOW) {
        model->flags = 0;
    } else if (drawRequest & ACROPOLIS_BRIDGE_ENEMY_DRAW_SKIP_AUTO_BUFFER) {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 1;
}

/// Rebuilds the bridge enemy model's lighting at its root's world position.
///
/// `recomposeRoot == 1` marks the root dirty and composes it before sampling;
/// every other value uses its existing world matrix. Requires a live model
/// with writable light and colour matrices and a composed current view.
/// Borrows one `VECTOR` from the scratch stack, queries all three light slots
/// and releases it after the query, including any nested lighting scratch.
static void _acropolisBridgeEnemyRelightModel(Task* task, s16 recomposeRoot)
{
    enum {
        ACROPOLIS_BRIDGE_ENEMY_RELIGHT_RECOMPOSE_ROOT = 1,
        ACROPOLIS_BRIDGE_ENEMY_RELIGHT_LIGHT_COUNT    = 3
    };
    VECTOR** scratchCursor;
    VECTOR*  previousTop;
    VECTOR*  worldPosition;

    scratchCursor  = &SCRATCH_STACK_CURSOR(VECTOR);
    previousTop    = *scratchCursor;
    worldPosition  = previousTop - 1;
    *scratchCursor = worldPosition;
    if (recomposeRoot == ACROPOLIS_BRIDGE_ENEMY_RELIGHT_RECOMPOSE_ROOT) {
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(task->extra.tmd->coords);
    }
    (previousTop - 1)->vx = task->extra.tmd->coords->workm.t[0];
    worldPosition->vy     = task->extra.tmd->coords->workm.t[1];
    worldPosition->vz     = task->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(task->extra.tmd, worldPosition, 0, ACROPOLIS_BRIDGE_ENEMY_RELIGHT_LIGHT_COUNT);
    *scratchCursor += 1;
}

/// Hides and disarms the bridge enemy in its inactive state.
///
/// Requires live enemy work, spawn argument and model. On state entry, hides
/// active drawing, disables both collision pairs and prevents target locking.
/// Later updates disable automatic buffer allocation and release an existing
/// primitive buffer. The work block, model and enemy remain live.
static void _acropolisBridgeEnemyInactive(Task* task)
{
    _AcropolisBridgeEnemyWork* work  = task->work;
    TmdObject*                 model = task->extra.tmd;

    if (work->stateEntered != 0) {
        Enemy* enemy = task->spawnArg2.pointer;

        model->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->attack.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        return;
    }
    model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    if (model->buffer != NULL) {
        tmdFreePrimitiveBuffer(model);
    }
}

/// Dispatches the bridge enemy's setup, frame update or teardown.
///
/// `task->state` must be 0..2 and `spawnArg2.pointer` a live `Enemy`. Setup owns
/// the work allocation and collision links; the final handler destroys them.
static void _acropolisBridgeEnemyTask(Task* task)
{
    EnemyTaskFuncTable3 states;

    states = _gAcropolisBridgeEnemyTaskStates;
    states.funcs[task->state](task->spawnArg2.pointer, task);
}
