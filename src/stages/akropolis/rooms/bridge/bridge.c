#include "stages/akropolis/rooms/bridge.h"

#include "config.h"
#include "../../event_flags.h"
#include "../../room_states.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/abs.h>
#include <psyq/memory.h>
#include <psyq/stdio.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/action_prompt.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/enemy_params.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/item_menu.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/player_actor.h"
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
#include "main/event_flags.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/pad_types.h"
#include "main/random.h"
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

#include "overlay.h"

#include "rooms/room_common.h"

#include "assets/enemies/bass/sounds.h"

#include "assets/rooms/akropolis/bridge/declarations.inc"

/// Selects the private hotspot test used by the bridge keypad.
#define ACTION_PROMPT_HIT_TEST _actionPromptHitTestDefault
static s32 ACTION_PROMPT_HIT_TEST(ActionPromptHotspot* hotspots, s16 cursorX, s16 cursorY);
#include "../../../../shared/room/action_prompt.h"
#include "../../../../shared/effects/glow_draw.h"

/// Selects the bridge billboard's `u16` frame and `s16` size arguments.
#define EFFECT_SPRITE_BILLBOARD_HALFWORD_ARGUMENTS
/// Binds the shared debris task to the bridge's exported `void (Task*)` callback.
#define EFFECT_SPRITE_DEBRIS_TASK akropolisBridgeEffectSpriteDebrisTask
#include "../../../../shared/effects/effect_sprite.h"
#undef EFFECT_SPRITE_BILLBOARD_HALFWORD_ARGUMENTS

#include "../../../../shared/creatures/boss_stranger/boss_stranger.h"
#include "../../../../shared/room/bridge_model.h"
#include "../../../../shared/effects/acropolis_glows.h"

/// Keypad hotspot ID that clears all entered digits.
#define KEYPAD_KEY_CLEAR 0xA

/// Digits in a complete bridge code; the keypad checks the entry once this many have been typed.
#define KEYPAD_CODE_DIGITS 3

/// `_KeypadWork::code` with nothing entered.
#define KEYPAD_CODE_BLANK 0xFFF

/// Accepted bridge code 561, packed as hundreds/tens/ones in bits 8..11/4..7/0..3.
#define KEYPAD_CODE_CORRECT 0x561

/// Waiting updates before a complete entry is checked on the following update.
#define KEYPAD_CHECK_DELAY_FRAMES 10

/// Display updates in each visible/blank phase of a keypad result blink.
#define KEYPAD_BLINK_PHASE_FRAMES 10

/// Counted blink cycles at which the keypad finishes displaying its result.
#define KEYPAD_BLINK_COUNT 3

/// Completed rejection displays after which the keypad closes with cancellation.
#define KEYPAD_REJECTED_LIMIT 3

/// Dispatch indices of the keypad task; stored in `Task::state`.
enum {
    KEYPAD_STATE_ENTER_CODE   = 2,
    KEYPAD_STATE_OPEN_EXAMINE = 3,
    KEYPAD_STATE_WAIT_EXAMINE = 4,
    KEYPAD_STATE_CHECK_CODE   = 5,
    KEYPAD_STATE_ACCEPTED     = 6,
    KEYPAD_STATE_REJECTED     = 7,
    KEYPAD_STATE_CLOSE        = 8
};

/// CAP command and variants for the keypad caption.
enum {
    KEYPAD_COMMAND                  = 7,
    KEYPAD_CAPTION_VARIANT          = 1,
    KEYPAD_COMPLETE_CAPTION_VARIANT = 2,
    KEYPAD_CAPTION_PLAYBACK_MODE    = 1
};

/// Message that seeds the arrival scene's dust plane on the room-effect task.
enum { EFFECT_MESSAGE_ARRIVAL_DUST = 3104 };

/// Arrival-scene sprite selection, applied to the argument's low byte.
enum {
    ARRIVAL_FRAME_FIRST  = 0,
    ARRIVAL_FRAME_SECOND = 1,
    ARRIVAL_FRAME_THIRD  = 2,
    ARRIVAL_VIEW_INDEX   = 9,
    ARRIVAL_BATCH_FIRST  = 1,
    ARRIVAL_BATCH_SECOND = 2,
    ARRIVAL_BATCH_THIRD  = 3
};

/// Batch indices in the eighth view's three-digit keypad display.
enum {
    KEYPAD_VIEW_INDEX           = 7,
    KEYPAD_DIGIT_BATCH_COUNT    = 10,
    KEYPAD_ONES_FIRST_BATCH     = 1,
    KEYPAD_TENS_FIRST_BATCH     = 11,
    KEYPAD_HUNDREDS_FIRST_BATCH = 21,
    KEYPAD_ONES_BLANK_BATCH     = 31,
    KEYPAD_TENS_BLANK_BATCH     = 32,
    KEYPAD_HUNDREDS_BLANK_BATCH = 33,
    KEYPAD_ERROR_BATCH          = 34,
    KEYPAD_PROMPT_BATCH         = 35
};

/// Cutscene views, keypad result and post-movie bridge placement variant.
enum {
    VIEW_OVERVIEW     = 6,
    VIEW_KEYPAD       = 8,
    VIEW_KEYPAD_MOVIE = 9,
    KEYPAD_CANCELLED  = 0,
    KEYPAD_ACCEPTED   = 1,
};

/// Actor commands for revealing the enemies and starting their movie presentation.
enum {
    COMMAND_CONTEXT_MOVIE      = GAME_STAGE_AKROPOLIS | (AKROPOLIS_ROOM_ID_PROMENADE << 8),
    COMMAND_CONTEXT_KEYPAD     = GAME_STAGE_AKROPOLIS | (AKROPOLIS_ROOM_ID_BRIDGE << 8),
    COMMAND_SHOW_MOVIE_ENEMIES = 1,
    COMMAND_KEYPAD_ACCEPTED    = 2
};

/// Enemy behavior indices used by this room's state dispatcher.
enum {
    ENEMY_STATE_INACTIVE             = 0,
    ENEMY_STATE_PATROL_SUNK          = 1,
    ENEMY_STATE_CHASE                = 2,
    ENEMY_STATE_RETREAT              = 3,
    ENEMY_STATE_FOLLOW_MOVIE         = 4,
    ENEMY_STATE_DEFEATED_BOB         = 5,
    ENEMY_STATE_COLLAPSE             = 6,
    ENEMY_STATE_DEATH_EFFECT         = 7,
    ENEMY_STATE_ANIMATE_AT_PLACEMENT = 8
};

/// Enemy animation requests and clips; playback rates use `ANIMATION_RATE_ONE`.
enum {
    ENEMY_ANIM_REQUEST_BLEND   = 1,
    ENEMY_ANIM_REQUEST_RESET   = 2,
    ENEMY_ANIM_REQUEST_PLAYING = 3,
    ENEMY_ANIM_PATROL          = 1,
    ENEMY_ANIM_CHASE           = 2,
    ENEMY_ANIM_BOB             = 3,
    ENEMY_ANIM_FALL            = 4,
    ENEMY_ANIM_YAW_REROLL_TICK = 5,
    ENEMY_ANIM_YAW_REROLL_WORD = (ENEMY_ANIM_YAW_REROLL_TICK << 16) |
                                 ENEMY_ANIM_FALL
};

/// Root Y thresholds in game-coordinate units; the two chase gates retain their different values.
enum {
    ENEMY_CHASE_START_Y = 701,
    ENEMY_CHASE_KEEP_Y  = 801,
    ENEMY_SINK_LIMIT    = 1800
};

/// The enemy's walking by state: turn limits and speeds, how fast it sinks and rises, and how small it shrinks.
enum {
    ENEMY_PATROL_TURN_LIMIT    = 0x60,
    ENEMY_PATROL_SPEED         = 0x20,
    ENEMY_PATROL_ARRIVED_SPEED = 0x60, // Restarting at a route point
    ENEMY_CHASE_TURN_LIMIT     = 0x100,
    ENEMY_CHASE_SPEED          = 0xA0,
    ENEMY_RETREAT_TURN_LIMIT   = 0x200,
    ENEMY_RETREAT_START_SPEED  = 0x80,
    ENEMY_SINK_STEP            = 45,
    ENEMY_RISE_STEP            = 60,
    ENEMY_PATROL_MIN_SCALE     = ONE / 2,
    ENEMY_PATROL_SHRINK_STEP   = 51,
    ENEMY_RETREAT_MIN_SCALE    = 0x500,
    ENEMY_RETREAT_SHRINK_STEP  = 70,
};

/// Presentation root Y gates in coordinate units and restart delay in updates.
enum {
    ENEMY_PRESENTATION_PATH_Y_OFFSET = 200,
    ENEMY_PRESENTATION_SCALE_START_Y = 500,
    ENEMY_PRESENTATION_RETRY_MIN_Y   = -989,
    ENEMY_PRESENTATION_POSE_MAX_Y    = 800,
    ENEMY_PRESENTATION_RETRY_TICKS   = 8
};

/// Work block of the task that runs the bridge's code keypad screen, allocated by its first state and kept at
/// `Task::work`.
typedef struct {
    s32 field_0;        // Set to 0x14 when the task starts and never read; role unproven
    s16 code;           // Digits entered so far, one per nibble with the newest lowest; KEYPAD_CODE_BLANK when none
    u8  digitCount;     // Digits typed into `code` since it was last blank
    u8  rejectedCount;  // Wrong codes entered since the keypad opened
    s16 blinkCount;     // Blink cycles the result display has completed
    s16 timer;          // Frames the current timed step has run: the hold before a complete entry is checked, then the position inside a blink cycle
    s16 selectedKey;    // `ActionPromptHotspot::id` of the key confirmed before the keypad was examined; stored and never read
    s8  promptKind;     // `ActionPromptHotspot::promptKind` of that key, forwarded when its command prompt opens
    s8  keypadExamined; // Whether the Examine command was accepted on the keypad (0 a key confirm offers Examine, 1 it types the key)
} _KeypadWork;
STATIC_ASSERT_SIZEOF(_KeypadWork, 0x10);

/// Scratch-stack workspace for the four-corner quad one of the bridge's effect tasks projects straight into its
/// packet.
typedef struct {
    s32     otz;         // Last projected corner's SZ3 / 4, then with the drawer's ordering bias added
    u8      field_4[8];  // Reserved with the block but never read or written; role unproven
    SVECTOR vertices[4]; // Local corner workspace, then the world positions supplied to the projection
} _QuadScratch;
STATIC_ASSERT_SIZEOF(_QuadScratch, 0x2C);

/// Scratch-stack workspace for one upright debris billboard.
typedef struct {
    s32     depth;           // SZ3 / 4 plus one; divisor for the half-side and ordering-table depth
    s32     screenExtent;    // Half-side of the upright quad, in pixels
    s32     projectionFlags; // GTE FLAG word; a negative value rejects the projection
    SVECTOR worldPoint;      // World position at projection, each component narrowed to 16 bits
    u16     screenX;         // Raw projected center X; first half of the GTE screen-position word
    u16     screenY;         // Raw projected center Y; second half of the same GTE word
} _BillboardScratch;
STATIC_ASSERT_SIZEOF(_BillboardScratch, 0x18);

/// Exit result shared by the bridge keypad's singleton state callbacks.
static s32 _gAkropolisBridgeKeypadExitResult;

/// Next frame-local GPU image-move packet for the bridge's framebuffer streaks.
static DR_MOVE* _gAkropolisBridgeStreakMoveCursor;

static void _actionPromptResetDefault(Task* task);
static void _akropolisBridgeWaitForKeypadResult(Task* task);

/// Keypad-movie position used to synchronize the bridge enemy's height.
typedef struct {
    u16  movieFrame;   // One-based encoded movie position (0 before the first movie-task update)
    byte unknown_2[2]; // Retained bytes; format, role and object boundary unproven
} _MovieFrameStorage;
STATIC_ASSERT_SIZEOF(_MovieFrameStorage, 4);

static void _akropolisBridgeInitializeRoomTask(Task* task);
static void _akropolisBridgeIdleRoomTask(Task* task);
static void _akropolisBridgeUpdateModelVisibility(Task* task);
static void _akropolisBridgeStartKeypadCaption(Task* task);
static void _akropolisBridgeWaitForKeypadCaption(Task* task);
static void _akropolisBridgeStartKeypad(Task* task);
static void _akropolisBridgeResumeAfterKeypad(Task* task);
static void _akropolisBridgeAdvanceCutsceneState(Task* task);
static void _akropolisBridgeStartKeypadMovie(Task* sequenceTask);
static void _akropolisBridgeFinishKeypadMovie(Task* task);
static void _akropolisBridgeRestoreAfterKeypadMovie(Task* task);
static void _akropolisBridgeKeypadOpen(Task* task);
static void _akropolisBridgeKeypadEnterCode(Task* task);
static void _akropolisBridgeKeypadBlinkAcceptedCode(Task* task);
static void _akropolisBridgeKeypadBlinkError(Task* task);
static void _akropolisBridgeShowKeypadError(void);
static void _akropolisBridgeSetArrivalSpritesHidden(s32 arrivalSceneSeen);
static void _akropolisBridgeKeypadArmCursor(Task* task);
static void _akropolisBridgeKeypadOpenExaminePrompt(Task* task);
static void _akropolisBridgeKeypadCollectExamineAnswer(Task* task);
static void _akropolisBridgeKeypadCheckCode(Task* task);
static void _akropolisBridgeKeypadClose(Task* task);
static void _akropolisBridgeDrawWaterRipple(const GfxCoord* coord, s32 halfSize, s16 brightness);
static void _akropolisBridgeEnemySetup(Enemy* enemy, Task* task);
static void _akropolisBridgeEnemyFrame(Enemy* enemy, Task* task);

/// Work block of the enemy that lurks below the bridge, allocated into `Task::work`.
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
    MATRIX                colorMtx;          // the model's color matrix; loaded with a fixed blend while falling
    EffectSpawnArg        effectArg;         // placement of the hit and death effects
    s16                   sinkDepth;         // distance the model root sits below `baseHeight` (0 fully risen)
    s16                   baseHeight;        // model root height at spawn
    BossStrangerWalker    walker;            // patrol/chase movement
    u16                   deathFrame;        // ticks into the collapse or death sequence, stops at 101
    u16                   syncedView;        // camera view index the mesh visibility was last synced to
} _EnemyWork;
STATIC_ASSERT_SIZEOF(_EnemyWork, 0x294);

/// Scratch-stack block holding the attack contact the enemy's per-frame tick found on its body.
typedef struct {
    SVECTOR point; // the contact's world position; only vx/vy/vz are written, and only when an attack contact was found
    s32     key;   // the contact's packed key, handed to the damage step as the attack id (0 no attack contact this frame)
} _HitScratch;
STATIC_ASSERT_SIZEOF(_HitScratch, 0xC);

/// Ticks the walker task: steps its patrol route and drives its animation.

/// Returns the patrol node nearest the given actor's coordinate, by squared distance in the XZ plane.

static void _akropolisBridgeShowKeypadCode(s32 code, s16 hidePrompt);

static void _akropolisBridgeKeypadMovieTask(Task* movieTask);
static void _akropolisBridgeKeypadCursorTask(Task* task);
static s32  _akropolisBridgeSpawnArrivalDust(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg);

static void _akropolisBridgePrepareArrivalScene(void);
static void _akropolisBridgeSelectArrivalSpriteFrame(s32 frame);

static s32  _akropolisBridgeResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _akropolisBridgeRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32  _akropolisBridgeHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);
static s32  _akropolisBridgeIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg);
static s32  _akropolisBridgeIgnoreSoundCue(Task* unusedTask, s32 unusedMessageId, s32 unusedCueIndex, s32 unusedSecondArg);
static void _akropolisBridgeModelTask(Task* task);
static void _akropolisBridgeKeypadSequenceTask(Task* task);

/// Storage of the one-entry task descriptor table the bridge enemy spawns from.
typedef struct {
    TaskDesc desc;         // The enemy's spawn recipe; the table's only entry
    u8       unknown_C[8]; // Zero in the image; no access established and role unproven
} _EnemyTaskDescStorage;
STATIC_ASSERT_SIZEOF(_EnemyTaskDescStorage, 0x14);

static _EnemyTaskDescStorage _gAkropolisBridgeEnemyTaskDescStorage;

static s32  _akropolisBridgeEnemyApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);
static s32  _akropolisBridgeEnemySetModelDraw(Task* task, s32 messageId, s32 drawRequest, s32 unusedArg);
static void _akropolisBridgeEnemyPatrolSunk(Task* task);
static void _akropolisBridgeEnemyChase(Task* task);
static void _akropolisBridgeEnemyRetreat(Task* task);
static void _akropolisBridgeEnemyFollowMovie(Task* task);
static void _akropolisBridgeEnemyAnimateAtPlacement(Task* task);
static void _akropolisBridgeEnemyDefeatedBob(Task* task);
static void _akropolisBridgeEnemyCollapse(Task* task);
static void _akropolisBridgeEnemyDeathEffect(Task* task);
static void _akropolisBridgeEnemyInactive(Task* task);
static void _akropolisBridgeEnemyTask(Task* task);

/// Fixed horizontal translucent plane drawn by the bridge room's model task.
#include "assets/rooms/akropolis/bridge/water_surface.inc"

/// Room-transition, keypad, action, item-use and sound message bindings.
static TaskMessageEntry _gAkropolisBridgeRoomMessageTable[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _akropolisBridgeResolveRoomTransition },
    { ROOM_MESSAGE_COMMAND, _akropolisBridgeHandleRoomCommand },
    { DIRECTION_MESSAGE_ROOM_ACTION, _akropolisBridgeIgnoreRoomAction },
    { ROOM_MESSAGE_USE_KEY_ITEM, _akropolisBridgeRejectKeyItemUse },
    { ROOM_MESSAGE_SOUND, _akropolisBridgeIgnoreSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Indices of the two room-owned task recipes.
enum {
    ROOM_TASK_MODEL           = 0,
    ROOM_TASK_KEYPAD_SEQUENCE = 1,
};

/// Bridge-model and keypad-sequence task recipes, followed by the bank end marker.
static TaskDesc _gAkropolisBridgeRoomTaskDescriptors[3] = {
    { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, _akropolisBridgeModelTask, { .model = &_gAkropolisBridgeWaterSurfaceModel } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisBridgeKeypadSequenceTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

#include "assets/rooms/akropolis/bridge/events.inc"

/// Bodyless accepted-code movie recipe at descriptor index zero.
static TaskDesc _gAkropolisBridgeKeypadMovieTaskDesc = {
    { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisBridgeKeypadMovieTask, { .value = 0 }
};

/// Raw movie samples supplying the bridge enemies' presentation height.
static SVECTOR _gAkropolisBridgeMovieEnemyHeightPath[190] = {
#include "assets/rooms/akropolis/bridge/keypad_movie_enemy_heights.inc"
};

/// Bodyless keypad cursor recipe, borrowed until the keypad closes its cursor.
static TaskDesc _gAkropolisBridgeKeypadCursorTaskDesc = {
    { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _akropolisBridgeKeypadCursorTask, { .value = 0 }
};

/// Cursor hit regions for the bridge keypad's ten digit keys and clear key.
static ActionPromptHotspot _gAkropolisBridgeKeypadHotspots[] = {
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
    { 42, 23, 40, 21, KEYPAD_KEY_CLEAR, 0, 0 },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
};

/// Nibble-to-sprite-batch lookup for the keypad's hundreds, tens and ones rows.
static u8 _gAkropolisBridgeKeypadDigitBatches[KEYPAD_CODE_DIGITS][16] = {
    { KEYPAD_HUNDREDS_FIRST_BATCH, KEYPAD_HUNDREDS_FIRST_BATCH + 1,
      KEYPAD_HUNDREDS_FIRST_BATCH + 2, KEYPAD_HUNDREDS_FIRST_BATCH + 3,
      KEYPAD_HUNDREDS_FIRST_BATCH + 4, KEYPAD_HUNDREDS_FIRST_BATCH + 5,
      KEYPAD_HUNDREDS_FIRST_BATCH + 6, KEYPAD_HUNDREDS_FIRST_BATCH + 7,
      KEYPAD_HUNDREDS_FIRST_BATCH + 8, KEYPAD_HUNDREDS_FIRST_BATCH + 9,
      KEYPAD_HUNDREDS_BLANK_BATCH, KEYPAD_HUNDREDS_BLANK_BATCH,
      KEYPAD_HUNDREDS_BLANK_BATCH, KEYPAD_HUNDREDS_BLANK_BATCH,
      KEYPAD_HUNDREDS_BLANK_BATCH, KEYPAD_HUNDREDS_BLANK_BATCH },
    { KEYPAD_TENS_FIRST_BATCH, KEYPAD_TENS_FIRST_BATCH + 1,
      KEYPAD_TENS_FIRST_BATCH + 2, KEYPAD_TENS_FIRST_BATCH + 3,
      KEYPAD_TENS_FIRST_BATCH + 4, KEYPAD_TENS_FIRST_BATCH + 5,
      KEYPAD_TENS_FIRST_BATCH + 6, KEYPAD_TENS_FIRST_BATCH + 7,
      KEYPAD_TENS_FIRST_BATCH + 8, KEYPAD_TENS_FIRST_BATCH + 9,
      KEYPAD_TENS_BLANK_BATCH, KEYPAD_TENS_BLANK_BATCH,
      KEYPAD_TENS_BLANK_BATCH, KEYPAD_TENS_BLANK_BATCH,
      KEYPAD_TENS_BLANK_BATCH, KEYPAD_TENS_BLANK_BATCH },
    { KEYPAD_ONES_FIRST_BATCH, KEYPAD_ONES_FIRST_BATCH + 1,
      KEYPAD_ONES_FIRST_BATCH + 2, KEYPAD_ONES_FIRST_BATCH + 3,
      KEYPAD_ONES_FIRST_BATCH + 4, KEYPAD_ONES_FIRST_BATCH + 5,
      KEYPAD_ONES_FIRST_BATCH + 6, KEYPAD_ONES_FIRST_BATCH + 7,
      KEYPAD_ONES_FIRST_BATCH + 8, KEYPAD_ONES_FIRST_BATCH + 9,
      KEYPAD_ONES_BLANK_BATCH, KEYPAD_ONES_BLANK_BATCH,
      KEYPAD_ONES_BLANK_BATCH, KEYPAD_ONES_BLANK_BATCH,
      KEYPAD_ONES_BLANK_BATCH, KEYPAD_ONES_BLANK_BATCH },
};

/// Arrival-dust message receiver for the room's ambient effect task.
static TaskMessageEntry _gAkropolisBridgeAmbientMessageTable[2] = {
    { EFFECT_MESSAGE_ARRIVAL_DUST, _akropolisBridgeSpawnArrivalDust },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// X/Z unit signs of the horizontal ground-glow quad, in GPU strip order.
static EffectUnitQuadCorner _gAkropolisBridgeGroundGlowUnitCorners[4] = {
    { -1, 1 },
    { 1, 1 },
    { -1, -1 },
    { 1, -1 },
};

/// Framebuffer-streak families selected by the ambient task's mapped camera.
enum {
    STREAK_NONE      = 0,
    STREAK_FALLING   = 1,
    STREAK_MID_DUST  = 2,
    STREAK_LOW_DUST  = 3,
    STREAK_TALL_DUST = 5,
    STREAK_PARTICLE  = 6
};

#define ROOM_PART _gAkropolisBridgeStarGlowOffsets
#include "assets/rooms/akropolis/bridge/markers.inc"
#define ROOM_PART _gAkropolisBridgeStarGlowViewMasks
#include "assets/rooms/akropolis/bridge/view_masks.inc"
/// Maps a one-based camera view to its framebuffer-streak family.
static u16 _gAkropolisBridgeViewStreakStyles[16] = {
    STREAK_NONE,
    STREAK_LOW_DUST,
    STREAK_NONE,
    STREAK_TALL_DUST,
    STREAK_PARTICLE,
    STREAK_FALLING,
    STREAK_MID_DUST,
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

#define ROOM_PART _gAkropolisBridgeLampGlowViewMasks
#include "assets/rooms/akropolis/bridge/view_masks.inc"
#define ROOM_PART _gAkropolisBridgePlayerWaterPartPositions
#include "assets/rooms/akropolis/bridge/markers.inc"
#define ROOM_PART gAkropolisBridgeRoomCollisionResources
#include "assets/rooms/akropolis/bridge/header.inc"

u8 gAkropolisBridgeStairSurfaceRows[2][12] = {
    { 2, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
};
/// Cue-2 bases in the loaded type-1 bank; cue 1 uses the following entry.
enum {
    SURFACE_2_WALK_CUE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x25,
    SURFACE_2_RUN_CUE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x27,
};

#define ROOM_PART gAkropolisBridgeWarpEntries
#include "assets/rooms/akropolis/bridge/header.inc"
#include "assets/rooms/akropolis/bridge/geometry.inc"
#include "assets/rooms/akropolis/bridge/view_boundaries.inc"
#include "assets/rooms/akropolis/bridge/action_triggers.inc"
#include "assets/rooms/akropolis/bridge/sprites.inc"
#include "assets/rooms/akropolis/bridge/placements.inc"
#include "assets/rooms/akropolis/bridge/lights.inc"
#include "assets/rooms/akropolis/bridge/cameras.inc"
#include "assets/rooms/akropolis/bridge/vibrations.inc"
#define ROOM_PART _gAkropolisBridgeSurface2FootstepSounds
#include "assets/rooms/akropolis/bridge/surfaces.inc"
/// Unreferenced footstep requests; their role is unproven.
static s32 _gAkropolisBridgeUnusedFootstepSounds[3] = {
    SURFACE_2_WALK_CUE,
    SURFACE_2_RUN_CUE,
    SURFACE_2_WALK_CUE,
};

/// Victim reaction packed into the bridge enemy's category-4 contact key.
enum { ENEMY_ATTACK_REACTION_POISON = 3 };

#define ROOM_PART _gAkropolisBridgeBlockingSurfaceProperties
#include "assets/rooms/akropolis/bridge/surfaces.inc"
/// Attack rows shared by all bridge enemies.
static DamageAttack _gAkropolisBridgeEnemyAttacks[] = {
    { 18, ENEMY_ATTACK_REACTION_POISON },
    { 0, 0 },
};

/// Shared attack, HP, reward and hit-response parameters for bridge enemies.
static EnemyParams _gAkropolisBridgeEnemyParams = {
    .attacks              = _gAkropolisBridgeEnemyAttacks,
    .hpMax                = 80,
    .exp                  = 6,
    .bp                   = 36,
    .mp                   = 1,
    .critChance           = 100,
    .buildupSteps         = 0,
    .damageOverTimeChance = 100,
    .damageOverTimeTicks  = 0,
};

#include "assets/enemies/bass/body.inc"

#include "assets/rooms/akropolis/bridge/clips.inc"

/// The Bass's clips by slot, borrowed by every bridge enemy's rig.
#include "assets/enemies/bass/bank.inc"

/// Enemy animation transition durations, indexed by previous clip then new clip.
static s16 _gAkropolisBridgeEnemyAnimationBlendFrames[6][6] = {
    { 0, 0, 0, 0, 0, 0 },
    { 0, 0, 1, 8, 0, 0 },
    { 0, 1, 0, 8, 0, 0 },
    { 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0 },
};

/// Twenty world-coordinate waypoints for the bridge's Boss Stranger walkers.
static BossStrangerNode _gAkropolisBridgeEnemyPatrolNodes[20] = {
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
static u8 _gAkropolisBridgeEnemyNavNodeOrder[12] = {
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
enum { ENEMY_ROUTE1_OFFSET = 8 };

/// Shared patrol-index stream for bridge enemy placements 0 and 1.
static u8 _gAkropolisBridgeEnemyRoute0And1Nodes[16] = {
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
static u8 _gAkropolisBridgeEnemyRoute2Nodes[8] = {
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
static u8 _gAkropolisBridgeEnemyRoute3Nodes[8] = {
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
static u8 _gAkropolisBridgeEnemyRoute4Nodes[12] = {
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
static u8 _gAkropolisBridgeEnemyRoute5Nodes[8] = {
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
static u8 _gAkropolisBridgeEnemyRoute6Nodes[8] = {
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
static u8 _gAkropolisBridgeEnemyRoute7Nodes[4] = {
    14,
    13,
    6,
    BOSS_STRANGER_ROUTE_END,
};

/// Placement 8's six-node patrol; no current spawn selects it. The trailing zero is unread.
static u8 _gAkropolisBridgeEnemyRoute8Nodes[8] = {
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
static u8* _gAkropolisBridgeEnemyRoutesByPlacement[9] = {
    _gAkropolisBridgeEnemyRoute0And1Nodes,
    &_gAkropolisBridgeEnemyRoute0And1Nodes[ENEMY_ROUTE1_OFFSET],
    _gAkropolisBridgeEnemyRoute2Nodes,
    _gAkropolisBridgeEnemyRoute3Nodes,
    _gAkropolisBridgeEnemyRoute4Nodes,
    _gAkropolisBridgeEnemyRoute5Nodes,
    _gAkropolisBridgeEnemyRoute6Nodes,
    _gAkropolisBridgeEnemyRoute7Nodes,
    _gAkropolisBridgeEnemyRoute8Nodes,
};

/// Bridge-enemy command and model-visibility messages, followed by the end marker.
static TaskMessageEntry _gAkropolisBridgeEnemyMessageTable[3] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, _akropolisBridgeEnemyApplyCommand },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _akropolisBridgeEnemySetModelDraw },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Nine bridge-enemy behavior handlers indexed by AKROPOLIS_BRIDGE_ENEMY_STATE_*.
static TaskFunc _gAkropolisBridgeEnemyStateHandlers[9] = {
    _akropolisBridgeEnemyInactive,
    _akropolisBridgeEnemyPatrolSunk,
    _akropolisBridgeEnemyChase,
    _akropolisBridgeEnemyRetreat,
    _akropolisBridgeEnemyFollowMovie,
    _akropolisBridgeEnemyDefeatedBob,
    _akropolisBridgeEnemyCollapse,
    _akropolisBridgeEnemyDeathEffect,
    _akropolisBridgeEnemyAnimateAtPlacement,
};

/// One bridge-enemy spawn recipe with its retained, uninterpreted storage tail.
static _EnemyTaskDescStorage _gAkropolisBridgeEnemyTaskDescStorage = {
    .desc      = { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _akropolisBridgeEnemyTask, { .model = &_gBassBody } },
    .unknown_C = { 0 },
};

/// Last bridge-model task handle produced by room setup, or NULL on spawn failure.
static Task* _gAkropolisBridgeModelTask = NULL;

/// Keypad task handle borrowed by the sequence's result-wait state.
static Task* _gAkropolisBridgeKeypadTask = NULL;

/// Movie task handle borrowed by the accepted-code sequence's completion state.
static Task* _gAkropolisBridgeKeypadMovieTask = NULL;

/// Collected keypad exit word: 0 cancelled/failed, 1 accepted.
static s32 _gAkropolisBridgeKeypadResult = KEYPAD_CANCELLED;

/// Movie frame published to enemy presentation, with two retained bytes after it.
static _MovieFrameStorage _gAkropolisBridgeMovieFrameStorage = {
    .movieFrame = 0,
    .unknown_2  = { 0x22, 0xF2 },
};

static void        _akropolisBridgeOffsetArrivalSpriteFrame(s32 frame, s8 offsetX, s8 offsetY);
static inline void _akropolisBridgeSetCollisionBodyPosition(WorldCollisionBody* body, const SVECTOR3* position);
static inline void _akropolisBridgeInitWalkerScale(BossStrangerWalker* walker);
static inline void _akropolisBridgeLightModel(Task* task);
static inline void _akropolisBridgeResetEnemyScaleOnEntry(_EnemyWork* work);
static inline void _akropolisBridgeResetEnemyScaleDuringShrink(_EnemyWork* work);
static inline void _akropolisBridgeGrowEnemyScale(_EnemyWork* work);
static inline s16  _akropolisBridgeAttackLanded(const Task* task);
static inline s32  _akropolisBridgeEnemyHasPlayerContact(const WorldCollisionContact* contacts);
/// Base requests for the bridge enemy's fall animation and player-body contact.
enum {
    ENEMY_CONTACT_SOUND        = SOUND_CHARACTER(SOUND_BANK_BASS, BASS_SOUND_CONTACT),
    ENEMY_FALL_ANIMATION_SOUND = SOUND_CHARACTER(SOUND_BANK_BASS, BASS_SOUND_FALL)
};

static inline void _akropolisBridgePlayEnemySound(Task* task, Enemy* enemy, s32 baseSoundId);
static void        _akropolisBridgeEnemyApplyHit(Task* task, u32 attackKey);
static void        _akropolisBridgeEnemyRelightModel(Task* task, s16 recomposeRoot);

/// Reserves a textured-quad packet for the bridge's ground glow.
static inline POLY_FT4* _akropolisBridgeReserveGroundGlowQuad(void)
{
    POLY_FT4* quad;

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    return quad;
}

/// The doors: the fire escape's plays its scene the first time instead of leaving, after which Rupert has left the
/// sanctuary; the promenade's leads to its state after the sanctuary.
static s32 _akropolisBridgeResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    GameLocationKey sanctuaryLocation;

    *reply = *request;
    if (request->areaId == AKROPOLIS_ROOM_ID_FIRE_ESCAPE) {
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_BRIDGE_FIRE_ESCAPE_DOOR_SCENE_SEEN) == 0) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_BRIDGE_FIRE_ESCAPE_DOOR_SCENE_SEEN, 1);
                evsStartScriptWithSkip(_gAkropolisBridgeArrivalScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gAkropolisBridgeArrivalSkipScript);
                gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_SANCTUARY_RUPERT_LEFT, 1);
                sanctuaryLocation.stage = GAME_STAGE_AKROPOLIS;
                sanctuaryLocation.area  = AKROPOLIS_ROOM_ID_SANCTUARY;
                areaSetPlacementVariant(&sanctuaryLocation, AKROPOLIS_BRIDGE_SANCTUARY_RUPERT_LEFT_VARIANT, AREA_VARIANT_RESET_ALWAYS);
            }
            return ROOM_EVENT_ROOM_ACTION;
        }
    }
    if ((request->areaId == AKROPOLIS_ROOM_ID_PROMENADE) && (request->queryOnly == ROOM_EVENT_EXECUTE)) {
        reply->roomState = AKROPOLIS_ROOM_STATE_PROMENADE_AFTER_SANCTUARY;
    }
    return ROOM_EVENT_DEPART;
}

/// No key item can be used here.
static s32 _akropolisBridgeRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Room command 7, the keypad: its caption once the bridge is raised, otherwise the code entry.
static s32 _akropolisBridgeHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    if (command == KEYPAD_COMMAND) {
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_BRIDGE_PROGRESS) == AKROPOLIS_BRIDGE_RAISED) {
            capStartSequenceSlot(KEYPAD_COMMAND, KEYPAD_CAPTION_PLAYBACK_MODE,
                                 KEYPAD_COMPLETE_CAPTION_VARIANT);
            return 0;
        }
        _akropolisBridgeShowKeypadCode(KEYPAD_CODE_BLANK, 1);
        taskSpawnFromTable(_gAkropolisBridgeRoomTaskDescriptors, ROOM_TASK_KEYPAD_SEQUENCE, 0, 0);
    }
    return 0;
}

/// Room actions do nothing here.
static s32 _akropolisBridgeIgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg)
{
    return 0;
}

/// Room sound cues do nothing here.
static s32 _akropolisBridgeIgnoreSoundCue(Task* unusedTask, s32 unusedMessageId, s32 unusedCueIndex, s32 unusedSecondArg)
{
    return 0;
}

/// Room task's setup, message-receiving idle and kill states, indexed by state 0..2.
static const TaskFuncTable3 _gAkropolisBridgeRoomStates = {
    { _akropolisBridgeInitializeRoomTask, _akropolisBridgeIdleRoomTask, taskKill }
};

/// Bridge-model setup, per-view visibility update and kill states, indexed by state 0..2.
static const TaskFuncTable3 _gAkropolisBridgeModelStates = {
    { _bridgeModelSetup, _akropolisBridgeUpdateModelVisibility, taskKill }
};

/// Dispatches the bridge model's setup, visibility update or teardown.
static void _akropolisBridgeModelTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gAkropolisBridgeModelStates;
    states.funcs[task->state](task);
}

/// Fourteen states of the caption, keypad and accepted-code movie sequence.
static const TaskFuncTable14 _gAkropolisBridgeKeypadSequenceStates = {
    { _akropolisBridgeStartKeypadCaption, _akropolisBridgeWaitForKeypadCaption, _akropolisBridgeAdvanceCutsceneState,
      _akropolisBridgeStartKeypad, _akropolisBridgeWaitForKeypadResult, _akropolisBridgeResumeAfterKeypad,
      _akropolisBridgeAdvanceCutsceneState, _akropolisBridgeAdvanceCutsceneState, _akropolisBridgeAdvanceCutsceneState,
      _akropolisBridgeAdvanceCutsceneState, _akropolisBridgeStartKeypadMovie, _akropolisBridgeFinishKeypadMovie,
      _akropolisBridgeRestoreAfterKeypadMovie, taskKill }
};

/// Keypad screen handlers indexed by AKROPOLIS_BRIDGE_KEYPAD_STATE_* (0..8).
static const TaskFuncTable9 _gAkropolisBridgeKeypadStates = {
    { _akropolisBridgeKeypadOpen, _akropolisBridgeKeypadArmCursor, _akropolisBridgeKeypadEnterCode,
      _akropolisBridgeKeypadOpenExaminePrompt, _akropolisBridgeKeypadCollectExamineAnswer, _akropolisBridgeKeypadCheckCode,
      _akropolisBridgeKeypadBlinkAcceptedCode, _akropolisBridgeKeypadBlinkError, _akropolisBridgeKeypadClose }
};

/// Runs the caption, keypad and accepted-code movie sequence.
static void _akropolisBridgeKeypadSequenceTask(Task* task)
{
    TaskFuncTable14 states;

    states = _gAkropolisBridgeKeypadSequenceStates;
    states.funcs[task->state](task);
}

/// Requests room-effect cancellation and locks attachment actions for the arrival scene.
static void _akropolisBridgePrepareArrivalScene(void)
{
    roomEffectRequestCancelAll();
    gAttachmentState.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
}

/// Installs the room receiver, spawns the bridge model and synchronizes arrival sprites.
static void _akropolisBridgeInitializeRoomTask(Task* task)
{
    task->msgTable = _gAkropolisBridgeRoomMessageTable;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    _gAkropolisBridgeModelTask = taskSpawnFromTable(_gAkropolisBridgeRoomTaskDescriptors, ROOM_TASK_MODEL, 0, 0);
    task->state                = task->state + 1;
    _akropolisBridgeSetArrivalSpritesHidden(gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_BRIDGE_FIRE_ESCAPE_DOOR_SCENE_SEEN) & 0xFF);
}

/// Keeps the room task alive to receive messages after its setup state.
static void _akropolisBridgeIdleRoomTask(Task* task)
{
    char stackReservation[0x10]; // Preserves the target's otherwise unused 16-byte stack frame.
}

void akropolisBridgeRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gAkropolisBridgeRoomStates;
    states.funcs[task->state](task);
}

#include "../../../../shared/room/bridge_model_setup.inc.c"

/// Hides the bridge model on mapped camera views 8..10 and invalidates its root transform.
static void _akropolisBridgeUpdateModelVisibility(Task* task)
{
    enum { FIRST_VIEW = 8,
           VIEW_COUNT = 3 };
    TmdObject* model;
    GfxCoord*  coord;

    model = task->extra.tmd;
    coord = model->coords;
    if ((u32)(viewGetMappedIndex() - FIRST_VIEW) < (u32)VIEW_COUNT) {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        model->flags = 0;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Starts the keypad caption and advances the sequence to wait for CAP.
static void _akropolisBridgeStartKeypadCaption(Task* task)
{
    capStartSequenceSlot(KEYPAD_COMMAND, KEYPAD_CAPTION_PLAYBACK_MODE,
                         KEYPAD_CAPTION_VARIANT);
    task->state = task->state + 1;
}

/// Waits for the switch caption, then opens the keypad view under scripted control.
static void _akropolisBridgeWaitForKeypadCaption(Task* task)
{
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_IS_BUSY, 0, 0) == 0) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = VIEW_KEYPAD;
        gGameSession->hideHud                                  = 1;
        playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
        task->state = task->state + 1;
    }
}

/// Spawns the keypad and publishes its handle for the following result poll.
static void _akropolisBridgeStartKeypad(Task* task)
{
    enum { BANK  = 2,
           INDEX = 8 };
    Task* keypadTask;

    keypadTask                  = taskSpawn(BANK, INDEX, 0, 0);
    task->state                 = task->state + 1;
    _gAkropolisBridgeKeypadTask = keypadTask;
}

/// Collects the keypad result and selects the overview or bridge movie view.
static void _akropolisBridgeWaitForKeypadResult(Task* task)
{
    ActorCommand movieCommand = { { { GAME_STAGE_AKROPOLIS, AKROPOLIS_ROOM_ID_PROMENADE } }, COMMAND_SHOW_MOVIE_ENEMIES };

    if (taskPollKill(_gAkropolisBridgeKeypadTask, &_gAkropolisBridgeKeypadResult) != 0) {
        if (_gAkropolisBridgeKeypadResult == KEYPAD_CANCELLED) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = VIEW_OVERVIEW;
            gGameSession->hideHud                                  = 0;
            task->state                                            = task->state + 1;
        } else {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = VIEW_KEYPAD_MOVIE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &movieCommand, ACTOR_COMMAND_MESSAGE_APPLY);
            task->state = task->state + 1;
        }
    }
}

/// Shows the player after the keypad and ends a cancelled movie sequence.
static void _akropolisBridgeResumeAfterKeypad(Task* task)
{
    if (_gAkropolisBridgeKeypadResult == KEYPAD_CANCELLED) {
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
        playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
        taskKill(task);
        return;
    }
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
    task->state += 1;
}

/// Advances the linear bridge cutscene by one state without other effects.
static void _akropolisBridgeAdvanceCutsceneState(Task* task)
{
    task->state = task->state + 1;
}

/// Spawns the accepted-code movie and advances the sequence to wait for it.
static void _akropolisBridgeStartKeypadMovie(Task* sequenceTask)
{
    Task* movieTask = taskSpawnFromTable(&_gAkropolisBridgeKeypadMovieTaskDesc, 0, 0, 0);
    s32   nextState = sequenceTask->state + 1;

    _gAkropolisBridgeKeypadMovieTask = movieTask;
    sequenceTask->state              = nextState;
}

/// Finishes the keypad movie and selects the following bridge room variant.
static void _akropolisBridgeFinishKeypadMovie(Task* task)
{
    s32 stackReservation[2]; // Preserves the target's otherwise unused eight-byte stack storage.
    s32 movieResult;

    if (taskPollKill(_gAkropolisBridgeKeypadMovieTask, &movieResult) != 0) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, 1, ACTOR_MESSAGE_SET_MODEL_DRAW);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view      = VIEW_OVERVIEW;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = AKROPOLIS_ROOM_STATE_BRIDGE_RAISED;
        gGameSession->location.roomState                            = AKROPOLIS_ROOM_STATE_BRIDGE_RAISED;
        gGameSession->roomObjsDirty                                 = 1;
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_BRIDGE_PROGRESS, AKROPOLIS_BRIDGE_RAISED);
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
        task->state = task->state + 1;
    }
}

/// Restores arrival-sprite visibility and the HUD after the keypad movie.
static void _akropolisBridgeRestoreAfterKeypadMovie(Task* task)
{
    _akropolisBridgeSetArrivalSpritesHidden(gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_BRIDGE_FIRE_ESCAPE_DOOR_SCENE_SEEN) & 0xFF);
    gGameSession->hideHud = 0;
    task->state           = task->state + 1;
}

/// Plays the accepted-keypad movie and publishes its frame for enemy presentation.
static void _akropolisBridgeKeypadMovieTask(Task* movieTask)
{
    enum {
        START             = 0,
        WAIT_FIRST_FRAME  = 1,
        WAIT_FINISH       = 2,
        EXIT_DELAY        = 3,
        EXIT_DELAY_FRAMES = 31
    };
    u8          streamArgs[4];
    CdCmdQueue* queue;
    s16         exitDelayFrames;

    queue = &gCdCmdQueue;
    switch (movieTask->state) {
        case START:
            queue->movieFrame = 1;
            streamArgs[0]     = streamFindMovieSlot(&gGameSession->location, 0, 0);
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, streamArgs);
            movieTask->state = movieTask->state + 1;
            break;
        case WAIT_FIRST_FRAME:
            if (queue->movieReady == 0) {
                break;
            }
            taskReparent(movieTask, padScriptSpawn(_gAkropolisBridgeKeypadMovieVibrationCommands, _gAkropolisBridgeKeypadMovieVibrationSegments));
            movieTask->state = movieTask->state + 1;
            break;
        case WAIT_FINISH:
            if ((cdCmdIsIdle() & 0xFFFF) == 0) {
                break;
            }
            movieTask->state = movieTask->state + 1;
            break;
        case EXIT_DELAY:
            exitDelayFrames          = movieTask->killCountdown + 1;
            movieTask->killCountdown = exitDelayFrames;
            if (exitDelayFrames >= EXIT_DELAY_FRAMES) {
                taskRequestKill(movieTask, 0);
                return;
            }
            break;
    }
    // Publish the STR frame for the enemies, including the post-playback delay.
    _gAkropolisBridgeMovieFrameStorage.movieFrame = queue->movieFrame;
}

/// Returns the enemy presentation's signed root Y sample for the current movie frame.
static s16 _akropolisBridgeGetMovieEnemyHeight(void)
{
    return _gAkropolisBridgeMovieEnemyHeightPath[_gAkropolisBridgeMovieFrameStorage.movieFrame + 1].vy;
}

/// Opens the keypad with owned work and a separately spawned cursor task.
static void _akropolisBridgeKeypadOpen(Task* task)
{
    _KeypadWork*         work;
    GameLocationKey*     location;
    ActionPromptHotspot* hotspot;
    SpriteView*          views;
    s32                  mappedView;

    work = memCalloc(sizeof(_KeypadWork), 0);
    if (work == NULL) {
        taskRequestKill(task, 0);
        return;
    }
    task->spawnArg2.pointer = taskSpawnFromTable(&_gAkropolisBridgeKeypadCursorTaskDesc, 0, 1, 0);
    task->work              = work;
    work->field_0           = 0x14;
    work->code              = KEYPAD_CODE_BLANK;
    location                = &gGameSession->location;
    task->state++;
    mappedView                                                    = viewGetMappedIndex();
    views                                                         = gSpriteAreaTables[location->stage - 1][gGameSession->spriteVariant - 1].areaViews[location->area - 1];
    views[(u8)mappedView - 1].batches[KEYPAD_PROMPT_BATCH].hidden = 1;
    // Hold the keypad display and control until its close state.
    gGameSession->cutsceneHold = 1;
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
    displayAcquireMenuHold();
    gGameSession->eventState = 1;
    gGameSession->hideHud    = 1;
    for (hotspot = _gAkropolisBridgeKeypadHotspots; hotspot->id != ACTION_PROMPT_HOTSPOT_END; hotspot++) {
        hotspot->hit = 0;
    }
    _gAkropolisBridgeKeypadExitResult = KEYPAD_CANCELLED;
    _akropolisBridgeShowKeypadCode(work->code, 0);
}

/// Collects keypad digits after Examine, or opens Examine for the selected key.
static void _akropolisBridgeKeypadEnterCode(Task* task)
{
    enum {
        CONFIRM_BUTTON      = 0,
        CANCEL_BUTTON       = 1,
        SHIFTED_DIGITS_MASK = 0xFF0
    };
    _KeypadWork*         work    = task->work;
    ActionPromptHotspot* hotspot = _gAkropolisBridgeKeypadHotspots;
    ActionPrompt*        prompt  = gActionPromptSlots;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (capIsBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    } else {
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
        if (_actionPromptHitTestDefault(hotspot, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
            prompt->mode = ACTION_PROMPT_MODE_HIGHLIGHTED;
            if (prompt->buttons.slots[CONFIRM_BUTTON].state == ACTION_PROMPT_BUTTON_PRESSED) {
                while (hotspot->id != ACTION_PROMPT_HOTSPOT_END) {
                    if (hotspot->hit != 0) {
                        // A first confirmed key offers Examine before accepting digits.
                        if (work->keypadExamined == 0) {
                            prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                            prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                            work->selectedKey   = hotspot->id;
                            work->promptKind    = hotspot->promptKind;
                            task->state         = KEYPAD_STATE_OPEN_EXAMINE;
                            return;
                        }
                        if (hotspot->id == KEYPAD_KEY_CLEAR) {
                            work->code       = KEYPAD_CODE_BLANK;
                            work->digitCount = 0;
                        } else {
                            work->code <<= 4;
                            work->code   = (work->code & SHIFTED_DIGITS_MASK) | hotspot->id;
                            work->digitCount++;
                        }
                        sndEvtRequestScriptStart(SOUND_AKROPOLIS_BRIDGE_KEYPAD_BEEP, 0, 0);
                        break;
                    }
                    hotspot++;
                }
            }
        } else {
            prompt->mode = ACTION_PROMPT_MODE_NORMAL;
        }
        if (work->digitCount == KEYPAD_CODE_DIGITS) {
            task->state = KEYPAD_STATE_CHECK_CODE;
            work->timer = 0;
        }
        // Cancellation wins when completion and cancel occur together.
        if (prompt->buttons.slots[CANCEL_BUTTON].state == ACTION_PROMPT_BUTTON_PRESSED) {
            task->state                       = KEYPAD_STATE_CLOSE;
            _gAkropolisBridgeKeypadExitResult = KEYPAD_CANCELLED;
        }
    }
    _akropolisBridgeShowKeypadCode(work->code, 0);
}

/// Advances one keypad result blink without resetting entry counters.
static inline void _akropolisBridgeKeypadStepResultBlink(_KeypadWork* work, s32 accepted)
{
    s16 blinkFrame = work->timer;

    if (blinkFrame < KEYPAD_BLINK_PHASE_FRAMES) {
        if (accepted) {
            _akropolisBridgeShowKeypadCode(KEYPAD_CODE_BLANK, 0);
        } else {
            _akropolisBridgeShowKeypadError();
        }
        work->timer++;
    } else if (blinkFrame < 2 * KEYPAD_BLINK_PHASE_FRAMES) {
        _akropolisBridgeShowKeypadCode(accepted ? work->code : KEYPAD_CODE_BLANK, 0);
        work->timer++;
    } else {
        work->timer = 0;
        work->blinkCount++;
    }
}

/// Blinks the accepted code, then closes the keypad with result 1.
static void _akropolisBridgeKeypadBlinkAcceptedCode(Task* task)
{
    ActionPrompt*        prompt   = gActionPromptSlots;
    ActionPromptHotspot* hotspots = _gAkropolisBridgeKeypadHotspots;
    _KeypadWork*         work     = task->work;
    GameLocationKey*     location = &gGameSession->location;
    SpriteView*          views;
    s32                  mappedView;

    mappedView                                                    = viewGetMappedIndex();
    views                                                         = gSpriteAreaTables[location->stage - 1][gGameSession->spriteVariant - 1].areaViews[location->area - 1];
    views[(u8)mappedView - 1].batches[KEYPAD_PROMPT_BATCH].hidden = 0;

    _akropolisBridgeKeypadStepResultBlink(work, 1);

    if (_actionPromptHitTestDefault(hotspots, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HIGHLIGHTED;
    } else {
        prompt->mode = ACTION_PROMPT_MODE_NORMAL;
    }

    if (work->blinkCount == KEYPAD_BLINK_COUNT) {
        task->state                       = KEYPAD_STATE_CLOSE;
        _gAkropolisBridgeKeypadExitResult = KEYPAD_ACCEPTED;
    }
}

/// Blinks the keypad error and retries entry, closing after three rejected codes.
static void _akropolisBridgeKeypadBlinkError(Task* task)
{
    ActionPrompt*        prompt   = gActionPromptSlots;
    ActionPromptHotspot* hotspots = _gAkropolisBridgeKeypadHotspots;
    _KeypadWork*         work     = task->work;
    u8                   rejectedCount;

    viewGetMappedIndex();
    _akropolisBridgeKeypadStepResultBlink(work, 0);

    if (_actionPromptHitTestDefault(hotspots, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HIGHLIGHTED;
    } else {
        prompt->mode = ACTION_PROMPT_MODE_NORMAL;
    }

    if (work->blinkCount == KEYPAD_BLINK_COUNT) {
        task->state         = KEYPAD_STATE_ENTER_CODE;
        work->digitCount    = 0;
        work->code          = KEYPAD_CODE_BLANK;
        rejectedCount       = work->rejectedCount + 1;
        work->rejectedCount = rejectedCount;
        if (rejectedCount >= KEYPAD_REJECTED_LIMIT) {
            task->state                       = KEYPAD_STATE_CLOSE;
            _gAkropolisBridgeKeypadExitResult = KEYPAD_CANCELLED;
        }
    }
}

/// Selects the visible sprite for each nibble of the bridge's three-digit code.
static void _akropolisBridgeShowKeypadCode(s32 code, s16 hidePrompt)
{
    GameLocationKey* location = &gGameSession->location;
    SpriteBatch*     batches;
    s32              batchIndex;
    u8               hundredsBatch;
    u8               tensBatch;
    u8               onesBatch;

    /// Selects a digit or blank sprite in one ten-batch band.
#define SELECT_KEYPAD_DIGIT(batches, digitBatch, firstBatch, blankBatch, batchIndex)                                \
    if ((digitBatch) == (blankBatch)) {                                                                             \
        for ((batchIndex) = (firstBatch); (batchIndex) < (firstBatch) + KEYPAD_DIGIT_BATCH_COUNT; (batchIndex)++) { \
            (batches)[(batchIndex)].hidden = 1;                                                                     \
        }                                                                                                           \
        (batches)[(blankBatch)].hidden = 0;                                                                         \
    } else {                                                                                                        \
        for ((batchIndex) = (firstBatch); (batchIndex) < (firstBatch) + KEYPAD_DIGIT_BATCH_COUNT; (batchIndex)++) { \
            if ((digitBatch) == (batchIndex)) {                                                                     \
                (batches)[(batchIndex)].hidden = 0;                                                                 \
                (batches)[(blankBatch)].hidden = 1;                                                                 \
            } else {                                                                                                \
                (batches)[(batchIndex)].hidden = 1;                                                                 \
            }                                                                                                       \
        }                                                                                                           \
    }

    viewGetMappedIndex();
    batches = gSpriteAreaTables[location->stage - 1][gGameSession->spriteVariant - 1].areaViews[location->area - 1][KEYPAD_VIEW_INDEX].batches;

    if (hidePrompt != 0) {
        batches[KEYPAD_PROMPT_BATCH].hidden = 1;
    }

    hundredsBatch = _gAkropolisBridgeKeypadDigitBatches[0][((u32)code & 0xF00) >> 8];
    tensBatch     = _gAkropolisBridgeKeypadDigitBatches[1][((u32)code & 0xF0) >> 4];
    onesBatch     = _gAkropolisBridgeKeypadDigitBatches[2][code & 0xF];

    SELECT_KEYPAD_DIGIT(batches, hundredsBatch, KEYPAD_HUNDREDS_FIRST_BATCH, KEYPAD_HUNDREDS_BLANK_BATCH, batchIndex);
    SELECT_KEYPAD_DIGIT(batches, tensBatch, KEYPAD_TENS_FIRST_BATCH, KEYPAD_TENS_BLANK_BATCH, batchIndex);
    SELECT_KEYPAD_DIGIT(batches, onesBatch, KEYPAD_ONES_FIRST_BATCH, KEYPAD_ONES_BLANK_BATCH, batchIndex);

#undef SELECT_KEYPAD_DIGIT

    batches[KEYPAD_ERROR_BATCH].hidden = 1;
}

/// Hides the three ten-digit bands of the keypad's sprite display.
static inline void _akropolisBridgeHideKeypadDigitBatches(SpriteBatch* batches)
{
    s32 batchIndex;

    for (batchIndex = KEYPAD_HUNDREDS_FIRST_BATCH;
         batchIndex < KEYPAD_HUNDREDS_FIRST_BATCH + KEYPAD_DIGIT_BATCH_COUNT;
         batchIndex++) {
        batches[batchIndex].hidden = 1;
    }
    for (batchIndex = KEYPAD_TENS_FIRST_BATCH;
         batchIndex < KEYPAD_TENS_FIRST_BATCH + KEYPAD_DIGIT_BATCH_COUNT;
         batchIndex++) {
        batches[batchIndex].hidden = 1;
    }
    for (batchIndex = KEYPAD_ONES_FIRST_BATCH;
         batchIndex < KEYPAD_ONES_FIRST_BATCH + KEYPAD_DIGIT_BATCH_COUNT;
         batchIndex++) {
        batches[batchIndex].hidden = 1;
    }
}

/// Shows the bridge keypad's error display, hiding its digits and blank indicators.
static void _akropolisBridgeShowKeypadError(void)
{
    GameLocationKey* location = &gGameSession->location;
    SpriteBatch*     batches;

    viewGetMappedIndex();
    batches = gSpriteAreaTables[location->stage - 1][gGameSession->spriteVariant - 1].areaViews[location->area - 1][KEYPAD_VIEW_INDEX].batches;

    _akropolisBridgeHideKeypadDigitBatches(batches);
    batches[KEYPAD_ERROR_BATCH].hidden          = 0;
    batches[KEYPAD_HUNDREDS_BLANK_BATCH].hidden = 1;
    batches[KEYPAD_TENS_BLANK_BATCH].hidden     = 1;
    batches[KEYPAD_ONES_BLANK_BATCH].hidden     = 1;
}

#include "../../../../shared/room/action_prompt_outline_rect.inc.c"

/// Translates one arrival frame's three source rectangles by signed pixel offsets.
#define OFFSET_ARRIVAL_SPRITE_SOURCES(sourceArray, firstSource, xOffset, yOffset) \
    ((void)((sourceArray)[(firstSource)].x0     += (xOffset),                     \
            (sourceArray)[(firstSource)].y0     += (yOffset),                     \
            (sourceArray)[(firstSource) + 1].x0 += (xOffset),                     \
            (sourceArray)[(firstSource) + 1].y0 += (yOffset),                     \
            (sourceArray)[(firstSource) + 2].x0 += (xOffset),                     \
            (sourceArray)[(firstSource) + 2].y0 += (yOffset)))

/// Moves and selects one arrival-scene sprite frame in the tenth view.
static void _akropolisBridgeOffsetArrivalSpriteFrame(s32 frame, s8 offsetX, s8 offsetY)
{
    GameSession*     session  = gGameSession;
    GameLocationKey* location = &session->location;
    SpriteView*      views;
    SpriteSource*    sources;
    SpriteBatch*     batches;
    s32              selectedFrame;

    views         = gSpriteAreaTables[location->stage - 1][session->spriteVariant - 1].areaViews[location->area - 1];
    batches       = views[ARRIVAL_VIEW_INDEX].batches;
    sources       = views[ARRIVAL_VIEW_INDEX].sources.elements;
    selectedFrame = frame & 0xFF;

    if (selectedFrame == ARRIVAL_FRAME_FIRST) {
        OFFSET_ARRIVAL_SPRITE_SOURCES(sources, 0, offsetX, offsetY);
        batches[ARRIVAL_BATCH_FIRST].hidden  = 0;
        batches[ARRIVAL_BATCH_SECOND].hidden = 1;
        batches[ARRIVAL_BATCH_THIRD].hidden  = 1;
    } else if (selectedFrame == ARRIVAL_FRAME_SECOND) {
        OFFSET_ARRIVAL_SPRITE_SOURCES(sources, 3, offsetX, offsetY);
        batches[ARRIVAL_BATCH_FIRST].hidden  = 1;
        batches[ARRIVAL_BATCH_SECOND].hidden = 0;
        batches[ARRIVAL_BATCH_THIRD].hidden  = 1;
    } else if (selectedFrame == ARRIVAL_FRAME_THIRD) {
        OFFSET_ARRIVAL_SPRITE_SOURCES(sources, 6, offsetX, offsetY);
        batches[ARRIVAL_BATCH_FIRST].hidden  = 1;
        batches[ARRIVAL_BATCH_SECOND].hidden = 1;
        batches[ARRIVAL_BATCH_THIRD].hidden  = 0;
    } else {
        batches[ARRIVAL_BATCH_FIRST].hidden  = 1;
        batches[ARRIVAL_BATCH_SECOND].hidden = 1;
        batches[ARRIVAL_BATCH_THIRD].hidden  = 1;
    }
}

#undef OFFSET_ARRIVAL_SPRITE_SOURCES

#include "../../../../shared/room/action_prompt_move_cursors.inc.c"

#include "../../../../shared/room/action_prompt_draw_cursor.inc.c"

/// Dispatches keypad cursor setup and the continuing cursor update.
static void _akropolisBridgeKeypadCursorTask(Task* task)
{
    TaskFunc states[] = { _actionPromptResetDefault, _actionPromptMoveCursorsDefault };

    states[task->state](task);
}

/// Hides the two arrival-scene sprite batches when the argument's low byte is nonzero.
static void _akropolisBridgeSetArrivalSpritesHidden(s32 arrivalSceneSeen)
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
static void _akropolisBridgeSelectArrivalSpriteFrame(s32 frame)
{
    GameSession*     session  = gGameSession;
    GameLocationKey* location = &session->location;
    SpriteView*      views;
    SpriteBatch*     batches;
    s32              selectedFrame;

    views         = gSpriteAreaTables[location->stage - 1][session->spriteVariant - 1].areaViews[location->area - 1];
    batches       = views[ARRIVAL_VIEW_INDEX].batches;
    selectedFrame = frame & 0xFF;

    if (selectedFrame == ARRIVAL_FRAME_FIRST) {
        batches[ARRIVAL_BATCH_FIRST].hidden  = 0;
        batches[ARRIVAL_BATCH_SECOND].hidden = 1;
        batches[ARRIVAL_BATCH_THIRD].hidden  = 1;
    } else if (selectedFrame == ARRIVAL_FRAME_SECOND) {
        batches[ARRIVAL_BATCH_FIRST].hidden  = 1;
        batches[ARRIVAL_BATCH_SECOND].hidden = 0;
        batches[ARRIVAL_BATCH_THIRD].hidden  = 1;
    } else if (selectedFrame == ARRIVAL_FRAME_THIRD) {
        batches[ARRIVAL_BATCH_FIRST].hidden  = 1;
        batches[ARRIVAL_BATCH_SECOND].hidden = 1;
        batches[ARRIVAL_BATCH_THIRD].hidden  = 0;
    } else {
        batches[ARRIVAL_BATCH_FIRST].hidden  = 1;
        batches[ARRIVAL_BATCH_SECOND].hidden = 1;
        batches[ARRIVAL_BATCH_THIRD].hidden  = 1;
    }
}

/// Arms the keypad cursor at pixel (0, 0) and advances to code entry.
static void _akropolisBridgeKeypadArmCursor(Task* task)
{
    ActionPrompt* prompt = gActionPromptSlots;

    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    prompt->mode        = ACTION_PROMPT_MODE_NORMAL;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    _akropolisBridgeShowKeypadCode(KEYPAD_CODE_BLANK, 0);
    task->state++;
}

/// Opens Examine at the keypad cursor and advances to collect its answer.
static void _akropolisBridgeKeypadOpenExaminePrompt(Task* task)
{
    ActionPrompt* prompt = gActionPromptSlots;
    _KeypadWork*  work   = task->work;

    _akropolisBridgeShowKeypadCode(work->code, 0);
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    itemMenuOpenHotspotCommands(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = KEYPAD_STATE_WAIT_EXAMINE;
}

/// Collects the keypad's Examine answer and returns to code entry.
static void _akropolisBridgeKeypadCollectExamineAnswer(Task* task)
{
    enum { EXAMINE_CAP_SLOT            = 9,
           CAP_PLAY_IN_CURRENT_DISPLAY = 0,
           EXAMINE_VARIANT             = 0 };
    ActionPrompt* prompt = gActionPromptSlots;
    _KeypadWork*  work   = task->work;

    _akropolisBridgeShowKeypadCode(work->code, 0);
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    if (itemMenuIsHotspotActionConfirmed() != 0) {
        work->keypadExamined = 1;
        capStartSequenceSlot(EXAMINE_CAP_SLOT, CAP_PLAY_IN_CURRENT_DISPLAY, EXAMINE_VARIANT);
    }
    task->state = KEYPAD_STATE_ENTER_CODE;
}

/// Checks a complete keypad code after ten updates and starts its result display.
static void _akropolisBridgeKeypadCheckCode(Task* task)
{
    ActionPrompt*        prompt   = gActionPromptSlots;
    _KeypadWork*         work     = task->work;
    ActionPromptHotspot* hotspots = _gAkropolisBridgeKeypadHotspots;

    if (work->timer < KEYPAD_CHECK_DELAY_FRAMES) {
        work->timer++;
        return;
    }

    if (work->code != KEYPAD_CODE_CORRECT) {
        sndEvtRequestScriptStart(SOUND_AKROPOLIS_BRIDGE_CODE_REJECTED, 0, 0);
        work->blinkCount = 0;
        work->timer      = 0;
        task->state      = KEYPAD_STATE_REJECTED;
    } else {
        // Keep the inherited blink counters on acceptance.
        ActorCommand acceptedCommand = { { { GAME_STAGE_AKROPOLIS, AKROPOLIS_ROOM_ID_BRIDGE } }, COMMAND_KEYPAD_ACCEPTED };

        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &acceptedCommand, ACTOR_COMMAND_MESSAGE_APPLY);
        sndEvtRequestScriptStart(SOUND_AKROPOLIS_BRIDGE_CODE_ACCEPTED, 0, 0);
        task->state = KEYPAD_STATE_ACCEPTED;
    }
    _akropolisBridgeShowKeypadCode(work->code, 0);
    if (_actionPromptHitTestDefault(hotspots, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HIGHLIGHTED;
    } else {
        prompt->mode = ACTION_PROMPT_MODE_NORMAL;
    }
}

/// Closes the keypad, releases its cursor and requests exit with the saved result.
static void _akropolisBridgeKeypadClose(Task* task)
{
    enum { KEYPAD_INTERACTION_REARM_UPDATES = 10 };

    displayReleaseMenuHold();
    _akropolisBridgeShowKeypadCode(KEYPAD_CODE_BLANK, 0);
    taskKill(task->spawnArg2.pointer);
    taskRequestKill(task, _gAkropolisBridgeKeypadExitResult);
    gGameSession->eventState          = 0;
    gGameSession->hideHud             = 0;
    gGameSession->cutsceneHold        = 0;
    gDirectionInteractionRearmUpdates = KEYPAD_INTERACTION_REARM_UPDATES;
}

#include "../../../../shared/room/action_prompt_hit_test.inc.c"
#undef ACTION_PROMPT_HIT_TEST

void akropolisBridgeKeypadTask(Task* task)
{
    TaskFuncTable9 states;

    states = _gAkropolisBridgeKeypadStates;
    states.funcs[task->state](task);
}

#include "../../../../shared/room/action_prompt_reset.inc.c"

void akropolisBridgeAmbientEffectsTask(Task* task)
{
    enum {
        AMBIENT_INITIALIZE         = 0,
        DISC_RADIUS_SCALE          = 256,
        DISC_WARM_COLOR            = 0x5C20,
        DISC_COOL_COLOR            = 0x50C2,
        WATER_VIEW_MASK            = (1 << (2 - 1)) | (1 << (6 - 1)) | (1 << (7 - 1)),
        WATER_MIN_LOCAL_Y          = 513,
        WATER_CHANCE_BIAS          = 32,
        WATER_RANDOM_MASK          = 511,
        WATER_RIPPLE_HALF_SIZE     = 64,
        WATER_SPRAY_ARGUMENT       = 0x1202180,
        PLAYER_FIRST_WATER_PART    = 14,
        PLAYER_WATER_PART_STRIDE   = 3,
        VIEW9_STAR_Y_OFFSET        = 576,
        STREAK_ENTRY_BURST_COUNT   = 30,
        STREAK_FAST_PHASE_BIT      = 0x200,
        STREAK_PACKET_BASE_BYTES   = 0xA000,
        STREAK_PACKET_BUFFER_BYTES = 0x7000
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

    if (task->state == AMBIENT_INITIALIZE) {
        gGameSession->field_80 = 0;
        task->msgTable         = _gAkropolisBridgeAmbientMessageTable;
        gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM_EFFECT);
        gRoomEffectWaterRippleId = EFFECT_AKROPOLIS_BRIDGE_WATER_RIPPLE;
        gRoomEffectWaterSprayId  = EFFECT_AKROPOLIS_BRIDGE_WATER_SPRAY;
        task->state              = task->state + 1;
        for (index = 0; index < ARRAY_SIZE(_gAkropolisBridgePlayerWaterPartPositions); index++) {
            playerPart                                          = &playerTask->extra.tmd->coords[PLAYER_FIRST_WATER_PART + index * PLAYER_WATER_PART_STRIDE];
            _gAkropolisBridgePlayerWaterPartPositions[index].vx = playerPart->workm.t[0];
            _gAkropolisBridgePlayerWaterPartPositions[index].vy = playerPart->workm.t[1];
            _gAkropolisBridgePlayerWaterPartPositions[index].vz = playerPart->workm.t[2];
        }
    }

    ambientWork->age = ambientWork->age + 1;
    switch (mappedView) {
        case 6:
            glowDrawTintedDiscNoBias(&_gAkropolisBridgeWarmDiscWorldPosition, DISC_RADIUS_SCALE, DISC_WARM_COLOR);
            break;
        case 7:
            glowDrawTintedDiscNoBias(&_gAkropolisBridgeWarmDiscWorldPosition, DISC_RADIUS_SCALE, DISC_WARM_COLOR);
            break;
        case 3:
        case 4:
        case 9:
            glowDrawTintedDiscNoBias(&_gAkropolisBridgeCoolDiscWorldPosition, DISC_RADIUS_SCALE, DISC_COOL_COLOR);
            break;
    }

    // Emit the glow families selected by this mapped camera.
    viewBit = 1 << (mappedView - 1);
    if (mappedView == VIEW_KEYPAD_MOVIE) {
        for (index = 0; index < ARRAY_SIZE(_gAkropolisBridgeStarGlowOffsets); index++) {
            if (_gAkropolisBridgeStarGlowViewMasks[index] & viewBit) {
                glowPosition.vx = 0;
                glowPosition.vy = -VIEW9_STAR_Y_OFFSET;
                glowPosition.vz = 0;
                glowPosition.vx = _gAkropolisBridgeStarGlowOffsets[index].vx;
                glowPosition.vy = _gAkropolisBridgeStarGlowOffsets[index].vy - VIEW9_STAR_Y_OFFSET;
                glowPosition.vz = _gAkropolisBridgeStarGlowOffsets[index].vz;
                effectSpawn((EFFECT_AKROPOLIS_BRIDGE_STAR_GLOW | EFFECT_SPAWN_UNLIMITED), effectCoord, ambientWork->age + index, &glowPosition);
            }
        }
    } else {
        for (index = 0; index < ARRAY_SIZE(_gAkropolisBridgeStarGlowOffsets); index++) {
            if (_gAkropolisBridgeStarGlowViewMasks[index] & viewBit) {
                effectSpawn((EFFECT_AKROPOLIS_BRIDGE_STAR_GLOW | EFFECT_SPAWN_UNLIMITED), effectCoord, ambientWork->age + index, &_gAkropolisBridgeStarGlowOffsets[index]);
                effectSpawn(EFFECT_AKROPOLIS_BRIDGE_GROUND_GLOW, effectCoord, ambientWork->age + index, &_gAkropolisBridgeGroundGlowOffsets[index]);
            }
        }
    }

    for (index = 0; index < 3; index++) {
        if (_gAkropolisBridgeLampGlowViewMasks[index] & viewBit) {
            effectSpawn(EFFECT_AKROPOLIS_BRIDGE_LAMP_GLOW, effectCoord, 0, &_gAkropolisBridgeLampGlowOffsets[index]);
        }
    }
    for (index = 3; index < 5; index++) {
        if (_gAkropolisBridgeLampGlowViewMasks[index] & viewBit) {
            effectSpawn(EFFECT_AKROPOLIS_BRIDGE_LAMP_GLOW, effectCoord, 1, &_gAkropolisBridgeLampGlowOffsets[index]);
        }
        if (_gAkropolisBridgeLampGlowViewMasks[index + 2] & viewBit) {
            effectSpawn(EFFECT_AKROPOLIS_BRIDGE_LAMP_GLOW, effectCoord, 2, &_gAkropolisBridgeLampGlowOffsets[index + 2]);
        }
    }
    if (_gAkropolisBridgeLampGlowViewMasks[11] & viewBit) {
        effectSpawn(EFFECT_AKROPOLIS_BRIDGE_LAMP_GLOW, effectCoord, 1, &_gAkropolisBridgeLampGlowOffsets[11]);
    }

    // Sample the two player joints; signed-halfword thresholds retain truncation.
    if ((viewBit & WATER_VIEW_MASK) && gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING && playerPart->coord.t[1] >= WATER_MIN_LOCAL_Y) {
        for (index = 0; index < ARRAY_SIZE(_gAkropolisBridgePlayerWaterPartPositions); index++) {
            playerPart   = &playerTask->extra.tmd->coords[PLAYER_FIRST_WATER_PART + index * PLAYER_WATER_PART_STRIDE];
            axisDistance = _gAkropolisBridgePlayerWaterPartPositions[index].vx - playerPart->workm.t[0];
            distanceSum  = axisDistance < 0;
            if (distanceSum) {
                axisDistance = playerPart->workm.t[0] - _gAkropolisBridgePlayerWaterPartPositions[index].vx;
            }
            previousComponent = _gAkropolisBridgePlayerWaterPartPositions[index].vy;
            axisValue         = previousComponent - playerPart->workm.t[1];
            if (axisValue < 0) {
                axisValue = playerPart->workm.t[1] - previousComponent;
            }
            distanceSum        = axisDistance + axisValue;
            previousComponent  = _gAkropolisBridgePlayerWaterPartPositions[index].vz;
            axisValue          = playerPart->workm.t[2];
            axisDistance       = previousComponent - axisValue;
            axisDistance       = ((axisDistance >= 0) ? (distanceSum + axisDistance) : (distanceSum + (axisValue - previousComponent))) + WATER_CHANCE_BIAS;
            ambientWork->angle = axisDistance;

            RANDOM_LCG_ADVANCE();
            randomBits = RANDOM_LCG_DRAW();
            if ((randomBits & WATER_RANDOM_MASK) < ambientWork->angle) {
                effectSpawn(gRoomEffectWaterRippleId, playerPart, WATER_RIPPLE_HALF_SIZE, NULL);
            }
            ambientWork->angle = ambientWork->angle - WATER_CHANCE_BIAS;
            RANDOM_LCG_ADVANCE();
            randomBits = RANDOM_LCG_DRAW();
            if ((randomBits & WATER_RANDOM_MASK) < ambientWork->angle) {
                effectSpawn(gRoomEffectWaterSprayId, playerPart, WATER_SPRAY_ARGUMENT, NULL);
            }

            _gAkropolisBridgePlayerWaterPartPositions[index].vx = playerPart->workm.t[0];
            _gAkropolisBridgePlayerWaterPartPositions[index].vy = playerPart->workm.t[1];
            _gAkropolisBridgePlayerWaterPartPositions[index].vz = playerPart->workm.t[2];
        }
    }

    // Borrow byte-addressed packet storage from the file buffer for this frame.
    _gAkropolisBridgeStreakMoveCursor =
        (DR_MOVE*)(gFsActorBuffer2Base + (gDisplayState.otBuffer * STREAK_PACKET_BUFFER_BYTES + STREAK_PACKET_BASE_BYTES));

/// Seeds one view's framebuffer streaks with thirty attempted counted spawns.
#define SEED_STREAKS(effectId, parentCoord, viewId, spawnIndex)                           \
    {                                                                                     \
        for ((spawnIndex) = 0; (spawnIndex) < STREAK_ENTRY_BURST_COUNT; (spawnIndex)++) { \
            effectSpawn((effectId), (parentCoord), (viewId), NULL);                       \
        }                                                                                 \
    }

    // Seed streaks on view entry, then thin ongoing emission during battle.
    switch (_gAkropolisBridgeViewStreakStyles[mappedView - 1]) {
        case STREAK_NONE:
            break;
        case STREAK_FALLING:
            previousView = ambientWork->scale;
            if (previousView != mappedView) {
                SEED_STREAKS(EFFECT_AKROPOLIS_BRIDGE_FALLING_STREAK, effectCoord, (s32)mappedView, index);
            } else if (gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
                if (ambientWork->age & STREAK_FAST_PHASE_BIT) {
                    effectSpawn(EFFECT_AKROPOLIS_BRIDGE_FALLING_STREAK, effectCoord, (s32)(previousView), NULL);
                    effectSpawn(EFFECT_AKROPOLIS_BRIDGE_FALLING_STREAK, effectCoord, (s32)(previousView), NULL);
                } else {
                    RANDOM_LCG_ADVANCE();
                    if ((RANDOM_LCG_DRAW() & 1) == 0) {
                        effectSpawn(EFFECT_AKROPOLIS_BRIDGE_FALLING_STREAK, effectCoord, (s32)(previousView), NULL);
                    }
                }
            } else {
                RANDOM_LCG_ADVANCE();
                if ((u16)(RANDOM_LCG_DRAW() % 3) == 0) {
                    effectSpawn(EFFECT_AKROPOLIS_BRIDGE_FALLING_STREAK, effectCoord, (s32)(previousView), NULL);
                }
            }
            break;
        case STREAK_MID_DUST:
            previousView = ambientWork->scale;
            if (previousView != mappedView) {
                SEED_STREAKS(EFFECT_AKROPOLIS_BRIDGE_MID_DUST_STREAK, effectCoord, (s32)mappedView, index);
            } else if (gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
                effectSpawn(EFFECT_AKROPOLIS_BRIDGE_MID_DUST_STREAK, effectCoord, (s32)(previousView), NULL);
                effectSpawn(EFFECT_AKROPOLIS_BRIDGE_MID_DUST_STREAK, effectCoord, (s32)(previousView), NULL);
            } else {
                RANDOM_LCG_ADVANCE();
                if ((u16)(RANDOM_LCG_DRAW() % 3) == 0) {
                    effectSpawn(EFFECT_AKROPOLIS_BRIDGE_MID_DUST_STREAK, effectCoord, (s32)(previousView), NULL);
                }
            }
            break;
        case STREAK_LOW_DUST:
            previousView = ambientWork->scale;
            if (previousView != mappedView) {
                SEED_STREAKS(EFFECT_AKROPOLIS_BRIDGE_LOW_DUST_STREAK, effectCoord, (s32)mappedView, index);
            } else if (gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
                effectSpawn(EFFECT_AKROPOLIS_BRIDGE_LOW_DUST_STREAK, effectCoord, (s32)(previousView), NULL);
                effectSpawn(EFFECT_AKROPOLIS_BRIDGE_LOW_DUST_STREAK, effectCoord, (s32)(previousView), NULL);
            } else {
                RANDOM_LCG_ADVANCE();
                if ((u16)(RANDOM_LCG_DRAW() % 3) == 0) {
                    effectSpawn(EFFECT_AKROPOLIS_BRIDGE_LOW_DUST_STREAK, effectCoord, (s32)(previousView), NULL);
                }
            }
            break;
        case STREAK_TALL_DUST:
            previousView = ambientWork->scale;
            if (previousView != mappedView) {
                SEED_STREAKS(EFFECT_AKROPOLIS_BRIDGE_TALL_DUST_STREAK, effectCoord, (s32)mappedView, index);
            } else if (gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
                effectSpawn(EFFECT_AKROPOLIS_BRIDGE_TALL_DUST_STREAK, effectCoord, (s32)(previousView), NULL);
                effectSpawn(EFFECT_AKROPOLIS_BRIDGE_TALL_DUST_STREAK, effectCoord, (s32)(previousView), NULL);
            } else {
                RANDOM_LCG_ADVANCE();
                if ((u16)(RANDOM_LCG_DRAW() % 3) == 0) {
                    effectSpawn(EFFECT_AKROPOLIS_BRIDGE_TALL_DUST_STREAK, effectCoord, (s32)(previousView), NULL);
                }
            }
            break;
        case STREAK_PARTICLE:
            previousView = ambientWork->scale;
            if (previousView != mappedView) {
                SEED_STREAKS(EFFECT_AKROPOLIS_BRIDGE_PARTICLE_STREAK, effectCoord, (s32)mappedView, index);
            } else if (gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
                effectSpawn(EFFECT_AKROPOLIS_BRIDGE_PARTICLE_STREAK, effectCoord, (s32)(previousView), NULL);
                effectSpawn(EFFECT_AKROPOLIS_BRIDGE_PARTICLE_STREAK, effectCoord, (s32)(previousView), NULL);
            } else {
                RANDOM_LCG_ADVANCE();
                if ((u16)(RANDOM_LCG_DRAW() % 3) == 0) {
                    effectSpawn(EFFECT_AKROPOLIS_BRIDGE_PARTICLE_STREAK, effectCoord, (s32)(previousView), NULL);
                }
            }
            break;
    }

#undef SEED_STREAKS

    ambientWork->scale = mappedView;
}

/// Seeds a streak's lifetime, pixel width and frames per row, then advances its task state.
static inline void _akropolisBridgeInitializeStreakLifetime(Task* task, EffectWork* work)
{
    enum { MIN_LIFETIME_FRAMES   = 30,
           LIFETIME_RANGE_FRAMES = 90,
           MIN_WIDTH_PIXELS      = 16,
           WIDTH_RANGE_PIXELS    = 64,
           MAX_FRAMES_PER_ROW    = 4 };
    u16 randomBits;
    RANDOM_LCG_ADVANCE();
    randomBits  = RANDOM_LCG_DRAW();
    work->scale = (u32)randomBits % LIFETIME_RANGE_FRAMES + MIN_LIFETIME_FRAMES;
    RANDOM_LCG_ADVANCE();
    work->angle = (RANDOM_LCG_DRAW() & (WIDTH_RANGE_PIXELS - 1)) + MIN_WIDTH_PIXELS;
    RANDOM_LCG_ADVANCE();
    work->period = (RANDOM_LCG_DRAW() & (MAX_FRAMES_PER_ROW - 1)) + 1;
    task->state++;
}

void akropolisBridgeFallingStreakTask(Task* task)
{
    enum {
        STREAK_LAST_SOURCE_ROW = 239
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
    bufferY = gDisplayState.drawBuffer * DISPLAY_BUFFER_STRIDE_ROWS;
    if ((u8)viewGetMappedIndex() == task->spawnArg1.value) {
        // Seed a screen-space strip; its fields hold pixels and frame counts.
        if (work->age == 0) {
            RANDOM_LCG_ADVANCE();
            randomBits    = RANDOM_LCG_DRAW();
            work->move.vy = (u32)randomBits % 144 + 0x60;
            /* x and y double as the drift and spread of the column window here */
            x = (work->move.vy - 0x60) / 3;
            y = work->move.vy < 0xA0 ? (work->move.vy - 0x60) / 2 : 0x20;
            RANDOM_LCG_ADVANCE();
            randomColumn  = RANDOM_LCG_DRAW();
            columnSpan    = y + 0x90;
            work->move.vx = randomColumn % (x + columnSpan) + (0x40 - x);
            _akropolisBridgeInitializeStreakLifetime(task, work);
        }
        // Copy the current buffer row down by one pixel at the row-derived depth.
        y     = work->move.vy + work->age / work->period;
        x     = work->move.vx;
        depth = 0x840 - (y - 0x60) * 8;
        if (y < STREAK_LAST_SOURCE_ROW) {
            rect.x                            = x;
            rect.y                            = y + bufferY;
            rect.w                            = work->angle;
            rect.h                            = 1;
            movePacket                        = _gAkropolisBridgeStreakMoveCursor;
            _gAkropolisBridgeStreakMoveCursor = movePacket + 1;
            SetDrawMove(movePacket, &rect, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> 4), movePacket);
        }
        work->age++;
        if (work->age <= work->scale && y < STREAK_LAST_SOURCE_ROW) {
            return;
        }
    }
    effectKillTask(work, task);
}

void akropolisBridgeMidDustStreakTask(Task* task)
{
    enum {
        STREAK_LAST_SOURCE_ROW = 239
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
    bufferY = gDisplayState.drawBuffer * DISPLAY_BUFFER_STRIDE_ROWS;
    if ((u8)viewGetMappedIndex() == task->spawnArg1.value) {
        // Seed a screen-space strip; its fields hold pixels and frame counts.
        if (work->age == 0) {
            RANDOM_LCG_ADVANCE();
            randomBits    = RANDOM_LCG_DRAW();
            work->move.vy = (u32)randomBits % 168 + 0x48;
            /* x and y double as the drift and spread of the column window here */
            x = work->move.vy < 0xA0 ? work->move.vy - 0x48 : 0x58;
            y = work->move.vy < 0x78 ? (work->move.vy - 0x48) * 5 / 3 : 0x50;
            RANDOM_LCG_ADVANCE();
            randomColumn  = RANDOM_LCG_DRAW();
            columnSpan    = y + 0x48;
            column        = randomColumn % (x + columnSpan) + 0x58;
            column       -= x;
            work->move.vx = column;
            _akropolisBridgeInitializeStreakLifetime(task, work);
        }
        // Copy the current buffer row down by one pixel at the row-derived depth.
        y     = work->move.vy + work->age / work->period;
        x     = work->move.vx;
        depth = (0xF0 - y) * (0xF0 - y) / 15 + 0x2C0;
        if (y < STREAK_LAST_SOURCE_ROW) {
            rect.x                            = x;
            rect.y                            = y + bufferY;
            rect.w                            = work->angle;
            rect.h                            = 1;
            movePacket                        = _gAkropolisBridgeStreakMoveCursor;
            _gAkropolisBridgeStreakMoveCursor = movePacket + 1;
            SetDrawMove(movePacket, &rect, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> 4), movePacket);
        }
        work->age++;
        if (work->age <= work->scale && y < STREAK_LAST_SOURCE_ROW) {
            return;
        }
    }
    effectKillTask(work, task);
}

void akropolisBridgeLowDustStreakTask(Task* task)
{
    enum {
        STREAK_LAST_SOURCE_ROW = 239
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
    bufferY = gDisplayState.drawBuffer * DISPLAY_BUFFER_STRIDE_ROWS;
    if ((u8)viewGetMappedIndex() == task->spawnArg1.value) {
        // Seed a screen-space strip; its fields hold pixels and frame counts.
        if (work->age == 0) {
            RANDOM_LCG_ADVANCE();
            randomBits    = RANDOM_LCG_DRAW();
            work->move.vy = (u32)randomBits % 136 + 0x68;
            /* x and y double as the drift and spread of the column window here */
            x = work->move.vy < 0x78 ? (work->move.vy - 0x68) * 2 : 0x20;
            y = work->move.vy < 0x78 ? (work->move.vy - 0x68) * 9 : 0x90;
            RANDOM_LCG_ADVANCE();
            randomColumn  = RANDOM_LCG_DRAW();
            columnSpan    = y + 0x40;
            column        = randomColumn % (x + columnSpan) + 0x20;
            column       -= x;
            work->move.vx = column;
            _akropolisBridgeInitializeStreakLifetime(task, work);
        }
        // Copy the current buffer row down by one pixel at the row-derived depth.
        y     = work->move.vy + work->age / work->period;
        x     = work->move.vx;
        depth = 0x600 - (y - 0x68) * 8;
        if (y < STREAK_LAST_SOURCE_ROW) {
            rect.x                            = x;
            rect.y                            = y + bufferY;
            rect.w                            = work->angle;
            rect.h                            = 1;
            movePacket                        = _gAkropolisBridgeStreakMoveCursor;
            _gAkropolisBridgeStreakMoveCursor = movePacket + 1;
            SetDrawMove(movePacket, &rect, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> 4), movePacket);
        }
        work->age++;
        if (work->age <= work->scale && y < STREAK_LAST_SOURCE_ROW) {
            return;
        }
    }
    effectKillTask(work, task);
}

void akropolisBridgeTallDustStreakTask(Task* task)
{
    enum {
        STREAK_LAST_SOURCE_ROW = 239
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
    bufferY = gDisplayState.drawBuffer * DISPLAY_BUFFER_STRIDE_ROWS;
    if ((u8)viewGetMappedIndex() == task->spawnArg1.value) {
        // Seed a screen-space strip; its fields hold pixels and frame counts.
        if (work->age == 0) {
            RANDOM_LCG_ADVANCE();
            randomBits    = RANDOM_LCG_DRAW();
            work->move.vy = (u32)randomBits % 168 + 0x48;
            /* x and y double as the drift and spread of the column window here */
            x = (work->move.vy - 0x48) / 3;
            y = (work->move.vy - 0x48) / 2;
            RANDOM_LCG_ADVANCE();
            randomColumn  = RANDOM_LCG_DRAW();
            columnSpan    = y + 0x48;
            column        = randomColumn % (x + columnSpan) + 0x58;
            column       -= x;
            work->move.vx = column;
            _akropolisBridgeInitializeStreakLifetime(task, work);
        }
        // Copy the current buffer row down by one pixel at the row-derived depth.
        y     = work->move.vy + work->age / work->period;
        x     = work->move.vx;
        depth = (0xF0 - y) * (0xF0 - y) / 15 + 0x400;
        if (y < STREAK_LAST_SOURCE_ROW) {
            rect.x                            = x;
            rect.y                            = y + bufferY;
            rect.w                            = work->angle;
            rect.h                            = 1;
            movePacket                        = _gAkropolisBridgeStreakMoveCursor;
            _gAkropolisBridgeStreakMoveCursor = movePacket + 1;
            SetDrawMove(movePacket, &rect, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> 4), movePacket);
        }
        work->age++;
        if (work->age <= work->scale && y < STREAK_LAST_SOURCE_ROW) {
            return;
        }
    }
    effectKillTask(work, task);
}

void akropolisBridgeParticleStreakTask(Task* task)
{
    enum {
        STREAK_LAST_SOURCE_ROW = 239
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
    bufferY = gDisplayState.drawBuffer * DISPLAY_BUFFER_STRIDE_ROWS;
    if ((u8)viewGetMappedIndex() == task->spawnArg1.value) {
        // Seed a screen-space strip; its fields hold pixels and frame counts.
        if (work->age == 0) {
            RANDOM_LCG_ADVANCE();
            work->move.vy = (RANDOM_LCG_DRAW() & 0x7F) + 0x68;
            RANDOM_LCG_ADVANCE();
            randomColumn  = RANDOM_LCG_DRAW();
            work->move.vx = work->move.vy < 0x80 ? randomColumn % ((work->move.vy - 0x58) * 6) : randomColumn % 240;
            _akropolisBridgeInitializeStreakLifetime(task, work);
        }
        // Copy the current buffer row down by one pixel at the row-derived depth.
        y     = work->move.vy + work->age / work->period;
        x     = work->move.vx;
        depth = 0x800 - (y - 0x68) * 8;
        if (y < STREAK_LAST_SOURCE_ROW) {
            rect.x                            = x;
            rect.y                            = y + bufferY;
            rect.w                            = work->angle;
            rect.h                            = 1;
            movePacket                        = _gAkropolisBridgeStreakMoveCursor;
            _gAkropolisBridgeStreakMoveCursor = movePacket + 1;
            SetDrawMove(movePacket, &rect, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> 4), movePacket);
        }
        work->age++;
        if (work->age <= work->scale && y < STREAK_LAST_SOURCE_ROW) {
            return;
        }
    }
    effectKillTask(work, task);
}

/// Binds the shared star glow to the bridge's exported `void (Task*)` callback.
#define GLOW_STAR_TASK akropolisBridgeGlowStarTask
#include "../../../../shared/effects/acropolis_glows_star.inc.c"

void akropolisBridgeGroundGlowTask(Task* task)
{
    enum {
        HALF_EXTENT   = 0x300, // Game-coordinate units
        DEPTH_BIAS    = 0x20,  // Camera Z / 4
        TEXTURE_PAGE  = 0x2B,
        PALETTE       = 0x4381,
        TEXTURE_TOP_V = 0x10,
        TEXTURE_LAST  = 0x27,
        GREY_MASK     = 0xF
    };
    _QuadScratch**              scratchCursor;
    _QuadScratch*               reservedTop;
    _QuadScratch*               quadScratch;
    const EffectUnitQuadCorner* unitCorners;
    POLY_FT4*                   quad;
    GfxCoord*                   coord;
    EffectWork*                 work;
    const MATRIX*               worldMatrix;
    s32                         cornerIndex;
    u8                          grey;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    actorRenderComposeCoord(coord);

    scratchCursor  = &SCRATCH_STACK_CURSOR(_QuadScratch);
    cornerIndex    = 0;
    worldMatrix    = &coord->workm;
    unitCorners    = _gAkropolisBridgeGroundGlowUnitCorners;
    reservedTop    = *scratchCursor - 1;
    work->age      = task->spawnArg1.halves.low;
    *scratchCursor = reservedTop;
    quadScratch    = *scratchCursor;
    for (; cornerIndex < ARRAY_SIZE(quadScratch->vertices); cornerIndex++) {
        (&quadScratch->vertices[cornerIndex])->vx = unitCorners[cornerIndex].axis0Sign * HALF_EXTENT;
        (&quadScratch->vertices[cornerIndex])->vy = 0;
        (&quadScratch->vertices[cornerIndex])->vz = unitCorners[cornerIndex].axis1Sign * HALF_EXTENT;
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
    quad = _akropolisBridgeReserveGroundGlowQuad();
    gte_stsxy(&quad->x0);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    setUV4(quad, 0, TEXTURE_TOP_V,
           TEXTURE_LAST, TEXTURE_TOP_V,
           0, TEXTURE_TOP_V + TEXTURE_LAST,
           TEXTURE_LAST,
           TEXTURE_TOP_V + TEXTURE_LAST);
    gte_stsxy3(&quad->x1, &quad->x2, &quad->x3);
    gte_stszotz(&quadScratch->otz);
    quadScratch->otz += DEPTH_BIAS;
    if (quadScratch->otz >= GLOW_MIN_DEPTH) {
        quad->tpage = TEXTURE_PAGE;
        quad->clut  = PALETTE;
        RANDOM_LCG_ADVANCE();
        grey = RANDOM_LCG_DRAW() & GREY_MASK;
        setRGB0(quad, grey, grey, grey);
        setSemiTrans(quad, 1);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(quadScratch->otz),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_QuadScratch);
    effectKillTask(work, task);
}

/// Binds the included lamp glow to the bridge's exported `void (Task*)` callback.
#define GLOW_LAMP_TASK akropolisBridgeGlowLampTask
#include "../../../../shared/effects/acropolis_glows_lamp.inc.c"

/// Seeds the arrival scene's dust plane with 96 edge motes and eight interior motes.
static s32 _akropolisBridgeSpawnArrivalDust(Task* task, s32 messageId, s32 unusedFirstArg, s32 unusedSecondArg)
{
    enum {
        EDGE_BURSTS      = 32,
        INTERIOR_COUNT   = 8,
        PLANE_X          = -15960,
        HEIGHT_RANGE     = 1536,
        DEPTH_RANGE      = 970,
        EDGE_WIDTH_MASK  = 15,
        EDGE_Y_BASE_WORD = 0xF830,
        EDGE_Z_BASE_WORD = 0xF63C,
        EDGE_Z_FAR_WORD  = 0xFA06,
        INTERIOR_Y_BASE  = -2000,
        INTERIOR_Z_BASE  = -2500
    };
    GfxCoord* effectCoord;
    SVECTOR   position;
    s32       burstIndex;

    effectCoord = task->extra.coordBody->coord;

/// Spawns one mote on each of the arrival dust plane's three emitting edges.
#define SPAWN_EDGE_DUST(parentCoord, offset)                                         \
    {                                                                                \
        (offset).vx = PLANE_X;                                                       \
        RANDOM_LCG_ADVANCE();                                                        \
        (offset).vy = RANDOM_LCG_DRAW() % HEIGHT_RANGE + EDGE_Y_BASE_WORD;           \
        RANDOM_LCG_ADVANCE();                                                        \
        (offset).vz = (RANDOM_LCG_DRAW() & EDGE_WIDTH_MASK) + EDGE_Z_BASE_WORD;      \
        effectSpawn(EFFECT_AKROPOLIS_BRIDGE_DUST_MOTE, (parentCoord), 0, &(offset)); \
                                                                                     \
        (offset).vx = PLANE_X;                                                       \
        RANDOM_LCG_ADVANCE();                                                        \
        (offset).vy = RANDOM_LCG_DRAW() % HEIGHT_RANGE + EDGE_Y_BASE_WORD;           \
        RANDOM_LCG_ADVANCE();                                                        \
        (offset).vz = EDGE_Z_FAR_WORD - (RANDOM_LCG_DRAW() & EDGE_WIDTH_MASK);       \
        effectSpawn(EFFECT_AKROPOLIS_BRIDGE_DUST_MOTE, (parentCoord), 0, &(offset)); \
                                                                                     \
        (offset).vx = PLANE_X;                                                       \
        RANDOM_LCG_ADVANCE();                                                        \
        (offset).vy = (RANDOM_LCG_DRAW() & EDGE_WIDTH_MASK) + EDGE_Y_BASE_WORD;      \
        RANDOM_LCG_ADVANCE();                                                        \
        (offset).vz = (u16)RANDOM_LCG_DRAW() % DEPTH_RANGE + EDGE_Z_BASE_WORD;       \
        effectSpawn(EFFECT_AKROPOLIS_BRIDGE_DUST_MOTE, (parentCoord), 0, &(offset)); \
    }

    // Seed both side edges and the bottom edge of the arrival dust plane.
    burstIndex = 0;
    do {
        SPAWN_EDGE_DUST(effectCoord, position);
        burstIndex++;
    } while (burstIndex < EDGE_BURSTS);

#undef SPAWN_EDGE_DUST

    // Fill the plane with eight additional motes.
    burstIndex = 0;
    do {
        RANDOM_LCG_ADVANCE();
        position.vx = PLANE_X;
        position.vy = RANDOM_LCG_DRAW() % HEIGHT_RANGE + INTERIOR_Y_BASE;
        RANDOM_LCG_ADVANCE();
        position.vz = (u16)RANDOM_LCG_DRAW() % DEPTH_RANGE + INTERIOR_Z_BASE;
        effectSpawn(EFFECT_AKROPOLIS_BRIDGE_DUST_MOTE, effectCoord, 0, &position);
        burstIndex++;
    } while (burstIndex < INTERIOR_COUNT);

    return 0;
}

void akropolisBridgeDustMoteTask(Task* task)
{
    enum {
        LIFETIME_FRAMES    = 31,
        END_Y              = -29,
        GRAVITY_PER_UPDATE = 6
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
        work->move.vz = 0;
        RANDOM_LCG_ADVANCE();
        work->move.vx = RANDOM_LCG_DRAW() & 0xF;
        RANDOM_LCG_ADVANCE();
        work->move.vy = (RANDOM_LCG_DRAW() & 3) - 1;
        RANDOM_LCG_ADVANCE();
        work->scale = (RANDOM_LCG_DRAW() & 0x3F) + 0x30;
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
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(tileScratch->depth),
                prim);
        gpuSetPrimitiveBlendMode(prim, GPU_BLEND_AVERAGE, tileScratch->depth);
        work->move.vy += GRAVITY_PER_UPDATE;
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectPointTileScratch);
    work->age++;
    if (work->age >= LIFETIME_FRAMES || coord->coord.t[1] >= END_Y) {
        effectKillTask(work, task);
    }
}

void akropolisBridgeWaterRippleTask(Task* task)
{
    enum {
        STATE_NEW           = 0,
        STATE_ACTIVE        = 1,
        INITIAL_BRIGHTNESS  = 64,
        SPAWN_SIZE_MASK     = 0xFFF,
        GROWTH_PER_UPDATE   = 32,
        FADE_PER_UPDATE     = 2,
        MIN_FADE_BRIGHTNESS = 3
    };
    EffectWork* work;
    GfxCoord*   coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        // Frozen and cancelled updates draw the retained ripple once.
        _akropolisBridgeDrawWaterRipple(coord, work->angle, work->scale);
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
    } else {
        work->age++;
        switch (task->state) {
            case STATE_NEW:
                // Initialization leaves workm cached; the caller controls composition.
                work->scale = INITIAL_BRIGHTNESS;
                work->angle = task->spawnArg1.halves.low & SPAWN_SIZE_MASK;
                RANDOM_LCG_ADVANCE();
                gfxRotMatrixY(&coord->coord, RANDOM_LCG_DRAW() & ACTOR_TRANSFORM_ANGLE_MASK, GRAPHICS_ROTATION_REPLACE);
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                task->state         = STATE_ACTIVE;
                /* fallthrough */
            case STATE_ACTIVE:
                work->angle += GROWTH_PER_UPDATE;
                _akropolisBridgeDrawWaterRipple(coord, work->angle, work->scale);
                if (work->scale >= MIN_FADE_BRIGHTNESS) {
                    work->scale -= FADE_PER_UPDATE;
                } else {
                    effectKillTask(work, task);
                }
                break;
        }
    }
}

/// Draws an additive water-ripple square in the composed coordinate's local X/Z plane.
static void _akropolisBridgeDrawWaterRipple(const GfxCoord* coord, s32 halfSize, s16 brightness)
{
    enum {
        TEXTURE_TOP         = 56,
        TEXTURE_SPAN        = 55,
        MODULATED_QUAD_CODE = 0x2C
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
    setcode(quad, MODULATED_QUAD_CODE);
    gte_stsxy(&quad->x0);
    gte_ldv3(&quadScratch->corners[1], &quadScratch->corners[2], &quadScratch->corners[3]);
    gte_rtpt();
    setUV4(quad, 0, TEXTURE_TOP, TEXTURE_SPAN, TEXTURE_TOP, 0, TEXTURE_TOP + TEXTURE_SPAN, TEXTURE_SPAN, TEXTURE_TOP + TEXTURE_SPAN);
    gte_stsxy3(&quad->x1, &quad->x2, &quad->x3);
    gte_stflg(&quadScratch->flag);
    if (quadScratch->flag >= 0) {
        gte_stszotz(&quadScratch->otz);
        quadScratch->otz++;
        quad->tpage = getTPage(0, GPU_BLEND_ADD, 704, 0);
        setRGB0(quad, brightness, brightness, brightness);
        quad->clut = getClut(272, 271);
        setSemiTrans(quad, 1);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(quadScratch->otz),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(OverlayFlaggedQuadScratch);
}

#include "../../../../shared/effects/effect_sprite_debris.inc.c"

/// Draws a spinning raw-texture debris cell at a composed coordinate's translation.
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

        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(block->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BYTES_AT(scratchSlot, sizeof(*block));
}

/// Draws an upright raw-texture debris billboard at a composed coordinate's translation.
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
    void**             scratchSlot;
    _BillboardScratch* scratchEnd;
    _BillboardScratch* block;
    POLY_FT4*          quad;
    s32*               depthOutput;
    u32                cellBits;
    s32                uLeft;
    s32                rowV;
    s8                 vTopBits;
    s8                 vBottomBits;

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

        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH((scratchEnd - 1)->depth),
                quad);
    }
    SCRATCH_STACK_RELEASE_BYTES_AT(scratchSlot, sizeof(_BillboardScratch));
}

/// Diagnostic when the Boss Stranger's node-order plan finds no candidate slot pair.
static const char _gBossStrangerNoPlanPairMessage[] = "s->root_cnt == 0xff about \n";

/// Enemy-task setup, frame update and destruction handlers, indexed by Task state 0..2.
static const EnemyTaskFuncTable3 _gAkropolisBridgeEnemyTaskStates = {
    { _akropolisBridgeEnemySetup, _akropolisBridgeEnemyFrame, enemyDestroy }
};

#include "../../../../shared/effects/glow_draw_tinted_disc_no_bias.inc.c"

#include "../../../../shared/creatures/boss_stranger/boss_stranger.inc.c"

#include "../../../../shared/creatures/boss_stranger/boss_stranger_inlines.inc.c"

/// Applies enemy reveal and movie-start commands, returning 1.
static s32 _akropolisBridgeEnemyApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    _EnemyWork* work  = task->work;
    Enemy*      enemy = task->spawnArg2.pointer;
    TmdObject*  model = task->extra.tmd;
    s32         placementIndex;
    u16         commandId;

    if (command->context.key == COMMAND_CONTEXT_MOVIE && command->command == COMMAND_SHOW_MOVIE_ENEMIES) {
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
    if (command->context.key == COMMAND_CONTEXT_KEYPAD) {
        commandId = command->command;
        if (commandId == COMMAND_KEYPAD_ACCEPTED) {
            placementIndex = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
            switch (placementIndex) {
                case 0:
                    if (gSceneCombatState.battleRefs != 0) {
                        break;
                    }
                    work->state = ENEMY_STATE_INACTIVE;
                    goto hide;
                case 1:
                    if (gSceneCombatState.battleRefs >= 2) {
                        break;
                    }
                    work->state = ENEMY_STATE_INACTIVE;
                    goto hide;
                case 2:
                    if (gSceneCombatState.battleRefs < 3) {
                        goto reset;
                    }
                    break;
                default:
                    work->state = ENEMY_STATE_INACTIVE;
                    goto hide;
            }
            {
                u16 hp    = _gAkropolisBridgeEnemyParams.hpMax;
                enemy->hp = hp;
                work->hp  = hp;
            }
            work->state = ENEMY_STATE_FOLLOW_MOVIE;
            goto done;
        reset:
            work->state = ENEMY_STATE_INACTIVE;
        hide:
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
    }
done:
    return 1;
}

/// Commits a started clip and resets its per-update counter.
static inline void _akropolisBridgeCommitEnemyAnimation(_EnemyWork* playbackWork,
                                                        _EnemyWork* requestWork)
{
    playbackWork->prevAnimId = playbackWork->animId;
    requestWork->animRequest = ENEMY_ANIM_REQUEST_PLAYING;
    requestWork->animFrame   = 0;
}

/// Starts or advances the bridge enemy's three driven animation slots.
static void _akropolisBridgeEnemyUpdateAnimation(Task* task)
{
    enum { ENEMY_FIRST_DRIVEN_SLOT = 1 };
    _EnemyWork* work;
    _EnemyWork* blendWork;
    _EnemyWork* resetWork;
    _EnemyWork* playbackWork;
    s32         blendSlot;
    s32         resetSlot;
    s32         playingSlot;

    work = task->work;
    if (work->animRequest == ENEMY_ANIM_REQUEST_BLEND) {
        blendWork = task->work;
        for (blendSlot = ENEMY_FIRST_DRIVEN_SLOT; blendSlot < ARRAY_SIZE(blendWork->rig.slots); blendSlot++) {
            blendWork->rig.slots[blendSlot].rate = blendWork->animRate;
            animationSeekSlotWithBlend(&blendWork->rig.anim, blendSlot, blendWork->animId, 0,
                                       _gAkropolisBridgeEnemyAnimationBlendFrames[blendWork->prevAnimId][blendWork->animId]);
        }
        _akropolisBridgeCommitEnemyAnimation(blendWork, work);
        return;
    }
    if (work->animRequest == ENEMY_ANIM_REQUEST_RESET) {
        resetWork = task->work;
        for (resetSlot = ENEMY_FIRST_DRIVEN_SLOT; resetSlot < ARRAY_SIZE(resetWork->rig.slots); resetSlot++) {
            resetWork->rig.slots[resetSlot].rate = resetWork->animRate;
            animationResetSlot(&resetWork->rig.anim, resetSlot, resetWork->animId);
        }
        _akropolisBridgeCommitEnemyAnimation(resetWork, work);
        return;
    }
    if (work->animRequest == ENEMY_ANIM_REQUEST_PLAYING) {
        work->animFrame++;
        playbackWork = task->work;
        for (playingSlot = ENEMY_FIRST_DRIVEN_SLOT; playingSlot < ARRAY_SIZE(playbackWork->rig.slots); playingSlot++) {
            playbackWork->rig.slots[playingSlot].rate = playbackWork->animRate;
            animationTickSlot(&playbackWork->rig.anim, playingSlot);
        }
    }
}

/// Copies a local collision-body position in whole coordinate units.
static inline void _akropolisBridgeSetCollisionBodyPosition(WorldCollisionBody* body, const SVECTOR3* position)
{
    body->pos.vx = position->vx;
    body->pos.vy = position->vy;
    body->pos.vz = position->vz;
}

/// Initializes the bridge walker's uniform Q12 scale matrix and reserves its scratch vector.
static inline void _akropolisBridgeInitWalkerScale(BossStrangerWalker* walker)
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
static inline void _akropolisBridgeLightModel(Task* task)
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
static void _akropolisBridgeEnemySetup(Enemy* enemy, Task* task)
{
    enum {
        BODY_ID              = 0x29,
        INITIAL_SPEED_UNITS  = 48,
        INITIAL_TURN_LIMIT   = 48,
        INITIAL_SPEED_STEP   = 1,
        COLLISION_RADIUS     = 256,
        INITIAL_SINK_DEPTH   = 1500,
        INITIAL_PRESENTATION = 8,
        NAV_NODE_COUNT       = 10
    };
    TmdObject*          model;
    TmdObject*          lightingModel;
    GfxCoord*           rootCoord;
    GfxCoord*           walkerCoord;
    _EnemyWork*         work;
    BossStrangerWalker* walker;
    WorldCollisionBody* body;
    WorldCollisionBody* attackBody;
    s32                 placementIndex;
    s32                 initialWalkerState;
    u16                 configuredHp;
    s32                 heightAxis;
    SVECTOR3            origin;

    model      = task->extra.tmd;
    rootCoord  = model->coords;
    work       = memCalloc(sizeof(_EnemyWork), 0);
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
    enemy->param            = &_gAkropolisBridgeEnemyParams;
    configuredHp            = _gAkropolisBridgeEnemyParams.hpMax;
    enemy->recs             = work->bodyContacts;
    enemy->hp               = configuredHp;
    work->hp                = 1;
    work->field_10E         = 1;
    enemy->hpMax            = work->hp;
    enemy->hp               = enemy->hpMax;
    // Bind the rig and both collision spheres before enabling the walker.
    animationInitContext(&work->rig.anim, _gBassAnimationSets, model, work->rig.poses,
                         work->rig.slots);
    work->animRate         = ANIMATION_RATE_ONE;
    body                   = &work->body;
    body->coord            = &task->extra.tmd->coords[3];
    body->context.contacts = work->bodyContacts;
    body->pos.vx           = 0;
    body->pos.vy           = 0;
    body->pos.vz           = 0;
    body->key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | BODY_ID;
    body->radius           = COLLISION_RADIUS;
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
    _akropolisBridgeSetCollisionBodyPosition(attackBody, &origin);
    attackBody->radius = COLLISION_RADIUS;
    attackBody->flags  = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, attackBody);
    worldCollisionInitContacts(attackBody->context.contacts, ARRAY_SIZE(work->attackContacts), 0);
    work->attack.key  = damagePackEnemyAttackKey(enemy, 0);
    rootCoord->parent = &gGfxViewCoord;
    work->yaw         = ratan2(-rootCoord->coord.m[2][0], rootCoord->coord.m[2][2]);
    work->animRequest = ENEMY_ANIM_REQUEST_RESET;
    work->animId      = ENEMY_ANIM_CHASE;
    _akropolisBridgeEnemyUpdateAnimation(task);
    enemy->coord      = &task->extra.tmd->coords[3];
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    task->msgTable                = _gAkropolisBridgeEnemyMessageTable;
    work->prevState               = -1;
    work->state                   = ENEMY_STATE_PATROL_SUNK;
    work->sinkDepth               = INITIAL_SINK_DEPTH;
    work->baseHeight              = rootCoord->coord.t[1];
    rootCoord->coord.t[1]        += work->sinkDepth;

    // The embedded navigation and route records borrow the room tables.
    work->walker.navData.nodes         = _gAkropolisBridgeEnemyPatrolNodes;
    work->walker.navData.nodeCount     = NAV_NODE_COUNT;
    work->walker.navData.nodeOrder     = _gAkropolisBridgeEnemyNavNodeOrder;
    work->walker.navData.orderCount    = NAV_NODE_COUNT;
    work->walker.routeData.nodeIndices = _gAkropolisBridgeEnemyRoutesByPlacement[enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT];
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
    work->walker.turnLimit             = INITIAL_TURN_LIMIT;
    work->walker.coord                 = walkerCoord;
    walker->speedTarget                = INITIAL_SPEED_UNITS;
    walker->speed                      = 0;
    walker->speedStep                  = INITIAL_SPEED_STEP;
    initialWalkerState                 = BOSS_STRANGER_WALKER_PATROL;
    work->walker.state                 = initialWalkerState;
    work->walker.lockHeight            = 1;
    work->walker.skipGround            = 1;
    work->walker.playerId              = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId;

    _akropolisBridgeInitWalkerScale(walker);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _akropolisBridgeLightModel(task);
    heightAxis = 1;
    if (gSceneCombatState.battleRefs < 3) {
        sceneAcquireBattleRef(0);
    }
    if (gGameSession->location.roomState == AKROPOLIS_ROOM_STATE_BRIDGE_RAISED) {
        placementIndex = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        switch (placementIndex) {
            case 0:
                work->state                         = INITIAL_PRESENTATION;
                task->extra.tmd->coords->coord.t[0] = -0x22C4;
                task->extra.tmd->coords->coord.t[1] = -0x3E8;
                task->extra.tmd->coords->coord.t[2] = -0x640;
                break;
            case 1:
                work->state                                  = INITIAL_PRESENTATION;
                task->extra.tmd->coords->coord.t[0]          = -0x270F;
                task->extra.tmd->coords->coord.t[heightAxis] = -0x3E8;
                task->extra.tmd->coords->coord.t[2]          = -0x7D0;
                break;
            case 2:
                work->state                         = INITIAL_PRESENTATION;
                task->extra.tmd->coords->coord.t[0] = -0x2EE0;
                task->extra.tmd->coords->coord.t[1] = -0x3E8;
                task->extra.tmd->coords->coord.t[2] = -0x5DC;
                break;
            default:
                work->state = ENEMY_STATE_INACTIVE;
                break;
        }
    }
    task->state++;
}

/// Rebuilds the enemy walker's uniform scale matrix during state entry.
static inline void _akropolisBridgeResetEnemyScaleOnEntry(_EnemyWork* work)
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
static inline void _akropolisBridgeResetEnemyScaleDuringShrink(_EnemyWork* work)
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
static void _akropolisBridgeEnemyPatrolSunk(Task* task)
{
    _EnemyWork*         work;
    BossStrangerWalker* walker;
    BossStrangerWalker* arrivedWalker;
    Enemy*              enemy;
    PlayerStatus*       player;

    player = &gPlayerStatus;
    work   = task->work;
    enemy  = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->walker.state            = BOSS_STRANGER_WALKER_PATROL;
        work->walker.routeData.cursor = 0;
        work->attack.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->body.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        _akropolisBridgeResetEnemyScaleOnEntry(work);
        work->walker.turnLimit = ENEMY_PATROL_TURN_LIMIT;
        walker                 = &work->walker;
        walker->speedTarget    = ENEMY_PATROL_SPEED;
        walker->speed          = 0;
        walker->speedStep      = 1;
        work->animRequest      = ENEMY_ANIM_REQUEST_RESET;
        work->animId           = ENEMY_ANIM_PATROL;
        work->animRate         = ANIMATION_RATE_ONE;
    }
    if (work->walker.routeData.arrived == 1) {
        arrivedWalker              = &work->walker;
        arrivedWalker->speedTarget = ENEMY_PATROL_SPEED;
        arrivedWalker->speed       = ENEMY_PATROL_ARRIVED_SPEED;
        arrivedWalker->speedStep   = 2;
    }
    // Sink and shrink without clamping the final steps.
    if (work->sinkDepth < ENEMY_SINK_LIMIT) {
        work->sinkDepth += ENEMY_SINK_STEP;
        task->extra.tmd->coords->coord.t[1] =
            work->baseHeight + work->sinkDepth;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->walker.scale > ENEMY_PATROL_MIN_SCALE) {
        work->walker.scale -= ENEMY_PATROL_SHRINK_STEP;
        _akropolisBridgeResetEnemyScaleDuringShrink(work);
    } else {
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    }
    _bossStrangerTick(&work->walker);
    _akropolisBridgeEnemyUpdateAnimation(task);
    if (player->coordMtx->t[1] >= ENEMY_CHASE_START_Y) {
        work->state = ENEMY_STATE_CHASE;
    }
}

/// Grows the bridge enemy's Q12 scale by 136 and rebuilds its uniform scale matrix.
static inline void _akropolisBridgeGrowEnemyScale(_EnemyWork* work)
{
    enum { ENEMY_SCALE_GROWTH_PER_UPDATE = 136 };
    BossStrangerWalker* walker;
    VECTOR*             scratchEnd;
    VECTOR*             scaleVector;
    s32                 scaleAmount;

    work->walker.scale      += ENEMY_SCALE_GROWTH_PER_UPDATE;
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
static inline s16 _akropolisBridgeAttackLanded(const Task* task)
{
    const _EnemyWork* work = task->work;

    if (work->attackContacts[0].key.value == 0) {
        return 0;
    }
    return 1;
}

/// Raises and grows the enemy while chasing with its attack sphere enabled.
static void _akropolisBridgeEnemyChase(Task* task)
{
    _EnemyWork*         work;
    BossStrangerWalker* walker;
    Enemy*              enemy;
    PlayerStatus*       player;
    u16                 entrySpeed;

    player = &gPlayerStatus;
    work   = task->work;
    if (work->stateEntered != 0) {
        enemy = task->spawnArg2.pointer;
        sceneEngageBattle(1);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        entrySpeed                    = work->walker.speed;
        walker                        = &work->walker;
        work->walker.turnLimit        = ENEMY_CHASE_TURN_LIMIT;
        walker->speedTarget           = ENEMY_CHASE_SPEED;
        walker->speedStep             = 6;
        walker->speed                 = entrySpeed;
        work->walker.state            = BOSS_STRANGER_WALKER_CHASE;
        work->attack.flags           |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->body.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->attack.key              = damagePackEnemyAttackKey(enemy, 0);
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ENEMY_ANIM_REQUEST_RESET;
        work->animId                  = ENEMY_ANIM_CHASE;
        work->animRate                = 5 * ANIMATION_RATE_ONE;
    }
    if (work->sinkDepth > 0) {
        work->sinkDepth -= ENEMY_RISE_STEP;
        task->extra.tmd->coords->coord.t[1] =
            work->baseHeight + work->sinkDepth;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->walker.scale < ONE) {
        _akropolisBridgeGrowEnemyScale(work);
    }
    _bossStrangerTick(&work->walker);
    _akropolisBridgeEnemyUpdateAnimation(task);
    // The player-height gate deliberately overrides an attack-contact transition.
    if (_akropolisBridgeAttackLanded(task)) {
        work->state = ENEMY_STATE_RETREAT;
    }
    if (player->coordMtx->t[1] < ENEMY_CHASE_KEEP_Y) {
        work->state = ENEMY_STATE_PATROL_SUNK;
    }
}

/// Sinks and shrinks the enemy after its attack while resuming its patrol route.
static void _akropolisBridgeEnemyRetreat(Task* task)
{
    _EnemyWork*         work;
    BossStrangerWalker* walker;
    Enemy*              enemy;
    PlayerStatus*       player;

    player = &gPlayerStatus;
    work   = task->work;
    enemy  = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->walker.state            = BOSS_STRANGER_WALKER_PATROL;
        work->walker.routeData.cursor = 0;
        work->attack.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        _akropolisBridgeResetEnemyScaleOnEntry(work);
        work->walker.turnLimit = ENEMY_RETREAT_TURN_LIMIT;
        walker                 = &work->walker;
        walker->speedTarget    = ENEMY_PATROL_SPEED;
        walker->speed          = ENEMY_RETREAT_START_SPEED;
        walker->speedStep      = 3;
        work->animRequest      = ENEMY_ANIM_REQUEST_RESET;
        work->animId           = ENEMY_ANIM_PATROL;
        work->animRate         = ANIMATION_RATE_ONE;
    }
    if (work->sinkDepth < ENEMY_SINK_LIMIT) {
        work->sinkDepth += ENEMY_SINK_STEP;
        task->extra.tmd->coords->coord.t[1] =
            work->baseHeight + work->sinkDepth;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->walker.scale >= ENEMY_RETREAT_MIN_SCALE) {
        work->walker.scale -= ENEMY_RETREAT_SHRINK_STEP;
        _akropolisBridgeResetEnemyScaleDuringShrink(work);
    } else if (enemy->node.state.parts.flags == 0) {
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    }
    _bossStrangerTick(&work->walker);
    _akropolisBridgeEnemyUpdateAnimation(task);
    // Finish retreat only after the patrol route has advanced.
    if (work->walker.routeData.cursor != 0) {
        if (player->coordMtx->t[1] < ENEMY_CHASE_KEEP_Y) {
            work->state = ENEMY_STATE_PATROL_SUNK;
        } else {
            work->state = ENEMY_STATE_CHASE;
        }
    }
}

/// Returns whether the enemy's three-entry contact prefix contains a player body.
static inline s32 _akropolisBridgeEnemyHasPlayerContact(const WorldCollisionContact* contacts)
{
    enum { ENEMY_BODY_CONTACT_COUNT = 3 };
    s16 contactIndex;

    for (contactIndex = 0; contactIndex < ENEMY_BODY_CONTACT_COUNT; contactIndex++) {
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
static inline void _akropolisBridgePlayEnemySound(Task* task, Enemy* enemy, s32 baseSoundId)
{
    enum { SOUND_INSTANCE_SHIFT = 8 };
    s32 soundId;
    s32 panOffset;

    soundId   = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << SOUND_INSTANCE_SHIFT) | baseSoundId;
    panOffset = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, panOffset,
                             (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
}

/// Updates the movie-driven root height, yaw and unclamped Q12 presentation scale.
static inline void _akropolisBridgeUpdateMovieEnemyTransform(Task* task, _EnemyWork* work)
{
    VECTOR scale;
    s32    scaleAmount;
    s32    heightForScale;

    task->extra.tmd->coords->coord.t[1] =
        _akropolisBridgeGetMovieEnemyHeight() - ENEMY_PRESENTATION_PATH_Y_OFFSET;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    heightForScale                        = task->extra.tmd->coords->coord.t[1];
    if (heightForScale < ENEMY_PRESENTATION_SCALE_START_Y) {
        scaleAmount = ONE;
        scale.vx = scale.vy = scale.vz = scaleAmount;
    } else {
        scaleAmount     = ONE;
        heightForScale -= ENEMY_PRESENTATION_SCALE_START_Y;
        heightForScale *= 4;
        scaleAmount    -= heightForScale;
        scale.vx = scale.vy = scale.vz = scaleAmount;
    }
    gfxRotMatrixY(&task->extra.tmd->coords->coord, work->yaw, 1);
    ScaleMatrix(&task->extra.tmd->coords->coord, &scale);
}

/// Positions and animates the enemy in step with the keypad movie.
static void _akropolisBridgeEnemyFollowMovie(Task* task)
{
    _EnemyWork* work;
    _EnemyWork* reloadedWork;
    Enemy*      enemy;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    // Select the placement-specific movie origin and fixed presentation colors.
    if (work->stateEntered != 0) {
        work->attack.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        enemy->node.state.parts.flags = 0;
        work->animRate                = 2 * ANIMATION_RATE_ONE;
        work->animRequest             = ENEMY_ANIM_REQUEST_RESET;
        work->animId                  = ENEMY_ANIM_PATROL;
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
        RANDOM_LCG_ADVANCE();
        work->yaw = RANDOM_LCG_DRAW();
    }
    _akropolisBridgeUpdateMovieEnemyTransform(task, work);
    if (task->extra.tmd->coords->coord.t[1] < ENEMY_PRESENTATION_SCALE_START_Y &&
        work->animId != ENEMY_ANIM_FALL) {
        work->animRequest = ENEMY_ANIM_REQUEST_RESET;
        work->animId      = ENEMY_ANIM_FALL;
        _akropolisBridgePlayEnemySound(task, enemy, ENEMY_FALL_ANIMATION_SOUND);
    }
    if (work->animId == ENEMY_ANIM_FALL) {
        if (task->extra.tmd->coords->coord.t[1] >= ENEMY_PRESENTATION_RETRY_MIN_Y &&
            (s32)((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) + ENEMY_PRESENTATION_RETRY_TICKS) < work->animFrame) {
            RANDOM_LCG_ADVANCE();
            if ((RANDOM_LCG_DRAW() & 0xF) == 0) {
                work->animRate    = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) * 2) + ANIMATION_RATE_ONE;
                work->animRequest = ENEMY_ANIM_REQUEST_RESET;
                work->animId      = ENEMY_ANIM_FALL;
                _akropolisBridgePlayEnemySound(task, enemy, ENEMY_FALL_ANIMATION_SOUND);
            }
        }
    }
    if (task->extra.tmd->coords->coord.t[1] < ENEMY_PRESENTATION_POSE_MAX_Y) {
        reloadedWork = task->work;
        if (reloadedWork->rig.slots[1].currentPose.indices.recordIndex == reloadedWork->rig.slots[1].nextPose.indices.recordIndex) {
            RANDOM_LCG_ADVANCE();
            if ((RANDOM_LCG_DRAW() & 0x1F) == 0) {
                work->animRate    = ANIMATION_RATE_ONE;
                work->animRequest = ENEMY_ANIM_REQUEST_RESET;
                work->animId      = ENEMY_ANIM_FALL;
                _akropolisBridgePlayEnemySound(task, enemy, ENEMY_FALL_ANIMATION_SOUND);
            }
        }
    }
    // Compare the adjacent clip and update halfwords with one word load.
    if (*(s32*)&work->animId == ENEMY_ANIM_YAW_REROLL_WORD) {
        RANDOM_LCG_ADVANCE();
        work->yaw = RANDOM_LCG_DRAW();
    }
    // Player and companion bodies trigger the death presentation.
    if (_akropolisBridgeEnemyHasPlayerContact(work->bodyContacts) != 0) {
        if (work->hp > 0) {
            _akropolisBridgePlayEnemySound(task, enemy, ENEMY_CONTACT_SOUND);
        }
        work->state = ENEMY_STATE_DEATH_EFFECT;
    }
    _akropolisBridgeEnemyUpdateAnimation(task);
}

/// Animates the enemy at its existing placement after the keypad movie.
static void _akropolisBridgeEnemyAnimateAtPlacement(Task* task)
{
    _EnemyWork* work;
    _EnemyWork* reloadedWork;
    Enemy*      enemy;
    VECTOR      scale;
    s32         scaleAmount;
    s32 // Derive the scale from the existing root height.
        heightForScale;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    // Initialize playback and colors while retaining the placed root.
    if (work->stateEntered != 0) {
        work->attack.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        enemy->node.state.parts.flags = 0;
        work->animRate                = 2 * ANIMATION_RATE_ONE;
        work->animRequest             = ENEMY_ANIM_REQUEST_RESET;
        work->animId                  = ENEMY_ANIM_PATROL;
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
        RANDOM_LCG_ADVANCE();
        work->yaw = RANDOM_LCG_DRAW();
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    heightForScale                        = task->extra.tmd->coords->coord.t[1];
    if (heightForScale < ENEMY_PRESENTATION_SCALE_START_Y) {
        scaleAmount = ONE;
        scale.vx = scale.vy = scale.vz = scaleAmount;
    } else {
        scaleAmount     = ONE;
        heightForScale -= ENEMY_PRESENTATION_SCALE_START_Y;
        heightForScale *= 4;
        scaleAmount    -= heightForScale;
        scale.vx = scale.vy = scale.vz = scaleAmount;
    }
    gfxRotMatrixY(&task->extra.tmd->coords->coord, work->yaw, 1);
    ScaleMatrix(&task->extra.tmd->coords->coord, &scale);
    if (task->extra.tmd->coords->coord.t[1] < ENEMY_PRESENTATION_SCALE_START_Y &&
        work->animId != ENEMY_ANIM_FALL) {
        work->animRequest = ENEMY_ANIM_REQUEST_RESET;
        work->animId      = ENEMY_ANIM_FALL;
        _akropolisBridgePlayEnemySound(task, enemy, ENEMY_FALL_ANIMATION_SOUND);
    }
    if (work->animId == ENEMY_ANIM_FALL) {
        if (task->extra.tmd->coords->coord.t[1] >= ENEMY_PRESENTATION_RETRY_MIN_Y &&
            (s32)((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) + ENEMY_PRESENTATION_RETRY_TICKS) < work->animFrame) {
            RANDOM_LCG_ADVANCE();
            if ((RANDOM_LCG_DRAW() & 0xF) == 0) {
                work->animRate    = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) * 2) + ANIMATION_RATE_ONE;
                work->animRequest = ENEMY_ANIM_REQUEST_RESET;
                work->animId      = ENEMY_ANIM_FALL;
                _akropolisBridgePlayEnemySound(task, enemy, ENEMY_FALL_ANIMATION_SOUND);
            }
        }
    }
    if (task->extra.tmd->coords->coord.t[1] < ENEMY_PRESENTATION_POSE_MAX_Y) {
        reloadedWork = task->work;
        if (reloadedWork->rig.slots[1].currentPose.indices.recordIndex == reloadedWork->rig.slots[1].nextPose.indices.recordIndex) {
            RANDOM_LCG_ADVANCE();
            if ((RANDOM_LCG_DRAW() & 0x1F) == 0) {
                work->animRate    = ANIMATION_RATE_ONE;
                work->animRequest = ENEMY_ANIM_REQUEST_RESET;
                work->animId      = ENEMY_ANIM_FALL;
                _akropolisBridgePlayEnemySound(task, enemy, ENEMY_FALL_ANIMATION_SOUND);
            }
        }
    }
    // Compare the adjacent clip and update halfwords with one word load.
    if (*(s32*)&work->animId == ENEMY_ANIM_YAW_REROLL_WORD) {
        RANDOM_LCG_ADVANCE();
        work->yaw = RANDOM_LCG_DRAW();
    }
    // Player and companion bodies trigger the death presentation.
    if (_akropolisBridgeEnemyHasPlayerContact(work->bodyContacts) != 0) {
        if (work->hp > 0) {
            _akropolisBridgePlayEnemySound(task, enemy, ENEMY_CONTACT_SOUND);
        }
        work->state = ENEMY_STATE_DEATH_EFFECT;
    }
    _akropolisBridgeEnemyUpdateAnimation(task);
}

/// Bobs the defeated enemy and pushes it away from the player on body contact.
static void _akropolisBridgeEnemyDefeatedBob(Task* task)
{
    _EnemyWork* work;
    Enemy*      enemy;
    GfxCoord*   coord;
    SVECTOR     displacement;
    SVECTOR*    displacementPtr;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->attack.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->body.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->animRequest             = ENEMY_ANIM_REQUEST_BLEND;
        work->animId                  = ENEMY_ANIM_BOB;
        work->animRate                = ANIMATION_RATE_ONE;
        RANDOM_LCG_ADVANCE();
        gfxRotMatrixY(&task->extra.tmd->coords->coord, RANDOM_LCG_DRAW(), 1);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->sinkDepth > 0) {
        work->sinkDepth -= 3;
    }
    task->extra.tmd->coords->coord.t[1] = work->baseHeight + work->sinkDepth;
    task->extra.tmd->coords->coord.t[1] +=
        rsin((gDisplayState.animFrame << 5) + task->extra.tmd->coords->coord.t[0]) >> 6;
    // Player or companion contact triggers a push away from the player root.
    if (_akropolisBridgeEnemyHasPlayerContact(work->bodyContacts) != 0) {
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
    _akropolisBridgeEnemyUpdateAnimation(task);
}

/// Plays the defeated enemy's collapse animation and timed burn/fade.
static void _akropolisBridgeEnemyCollapse(Task* task)
{
    enum {
        ANIMATION         = 5,
        BURN_FRAME        = 10,
        BLACK_FRAME       = 22,
        TRANSLUCENT_FRAME = 28,
        HIDE_FRAME        = 34,
        END_FRAME         = 101
    };
    _EnemyWork* work  = task->work;
    Enemy*      enemy = task->spawnArg2.pointer;
    s32         deathFrame;

    if (work->stateEntered != 0) {
        tmdAllocPrimitiveBuffer(task->extra.tmd);
        work->attack.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->animRequest             = ENEMY_ANIM_REQUEST_BLEND;
        work->animId                  = ANIMATION;
        work->animRate                = ANIMATION_RATE_ONE;
        RANDOM_LCG_ADVANCE();
        gfxRotMatrixY(&task->extra.tmd->coords->coord, RANDOM_LCG_DRAW(), 1);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        worldTargetDisableNodeLockOn(&enemy->node);
        if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_IDLE && gSceneCombatState.battleRefs != 0) {
            sceneEngageBattle(1);
        }
        work->deathFrame = 0;
    }
    // Advance the pose and timed burn/fade through frame 101, then hold.
    if (work->deathFrame < END_FRAME) {
        work->deathFrame++;
        _akropolisBridgeEnemyUpdateAnimation(task);
        deathFrame = work->deathFrame;
        switch (deathFrame) {
            case BURN_FRAME:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                effectSpawn(EFFECT_CORPSE_BURN, &task->extra.tmd->coords[2], 1, NULL);
                break;
            case TRANSLUCENT_FRAME:
                task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                break;
            case BLACK_FRAME:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                break;
            case HIDE_FRAME:
                task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                break;
        }
    }
}

/// Starts the bridge enemy's hit/burn death effect and timed fade.
static void _akropolisBridgeEnemyDeathEffect(Task* task)
{
    enum {
        MOTOR_FRAMES          = 3,
        MOTOR_START_INTENSITY = 255,
        MOTOR_END_INTENSITY   = 8,
        FLAGS_FRAME           = 2,
        BLACK_FRAME           = 30,
        HIDE_FRAME            = 44,
        END_FRAME             = 101,
        HIT_ATTACK_KEY        = 0x1001,
        HIT_ARGUMENT_LOW      = 160,
        HIT_EXTRA_PUFF_COUNT  = 2
    };
    _EnemyWork* work  = task->work;
    Enemy*      enemy = task->spawnArg2.pointer;
    s32         deathFrame;

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
        work->effectArg.spawnArgLo = HIT_ARGUMENT_LOW;
        work->effectArg.spawnArgHi = HIT_EXTRA_PUFF_COUNT;
        effectSpawnHit(damageGetPlayerAttackEffectId(HIT_ATTACK_KEY), &task->extra.tmd->coords[1], NULL,
                       &work->effectArg);
        worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
        effectSpawn(EFFECT_CORPSE_BURN, &task->extra.tmd->coords[1], 1, NULL);
        task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
        work->deathFrame       = 0;
        padScriptSpawnVariableMotorRamp(MOTOR_FRAMES,
                                        MOTOR_START_INTENSITY,
                                        MOTOR_END_INTENSITY);
    }
    if (work->deathFrame < END_FRAME) {
        work->deathFrame++;
        _akropolisBridgeEnemyUpdateAnimation(task);
        deathFrame = work->deathFrame;
        switch (deathFrame) {
            case FLAGS_FRAME:
                // Frame 2 also supplies TMD_OBJECT_SEMI_TRANS.
                task->extra.tmd->flags = deathFrame;
                break;
            case BLACK_FRAME:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                break;
            case HIDE_FRAME:
                task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                break;
        }
    }
}

/// Accounts for a hit's healing credit, damage readout and enemy HP loss.
static inline void _akropolisBridgeEnemyAccountHitHp(_EnemyWork* work, Enemy* enemy, u32 attackKey, s32 damage)
{
    damageAccumulateLifeDrainHp(enemy, attackKey, damage, 0);
    enemy->hp -= damage;
    worldTargetAddReadoutAmount(&enemy->node, damage, 0);
    work->hp -= damage;
    enemy->hp = work->hp;
}

/// Applies an attack contact's HP damage and selects the enemy's defeat behavior.
static void _akropolisBridgeEnemyApplyHit(Task* task, u32 attackKey)
{
    enum {
        HIT_ARGUMENT_LOW       = 128,
        HIT_ARGUMENT_HIGH      = 2,
        CRITICAL_DAMAGE_FACTOR = 4,
        UNUSED_REACTION_SCALE  = 4096,
        REWARD_ARGUMENT        = 0x29
    };
    _EnemyWork* work  = task->work;
    Enemy*      enemy = task->spawnArg2.pointer;
    s32         damage;
    s16         state;

    // Account for the hit before choosing the defeat presentation.
    if (work->hp > 0) {
        damage                     = damageComputePlayerAttack(attackKey, 0, 0, UNUSED_REACTION_SCALE);
        work->effectArg.coord      = &task->extra.tmd->coords[1];
        work->effectArg.spawnArgLo = HIT_ARGUMENT_LOW;
        work->effectArg.spawnArgHi = HIT_ARGUMENT_HIGH;
        effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), &task->extra.tmd->coords[1],
                       NULL, &work->effectArg);
        if (damageRollCriticalHit(enemy, attackKey, 0) != 0) {
            damage *= CRITICAL_DAMAGE_FACTOR;
            effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[1], 0, NULL);
        }
        _akropolisBridgeEnemyAccountHitHp(work, enemy, attackKey, damage);
        if (work->hp > 0) {
            return;
        }
        if (gSceneCombatState.battleRefs != 0) {
            sceneReleaseBattleRefWithRewards(task, REWARD_ARGUMENT);
        }
        if (work->hp > 0) {
            return;
        }
    }
    // Preserve active defeat states; a falling enemy collapses instead of bobbing.
    state = work->state;
    if (state < ENEMY_STATE_DEATH_EFFECT) {
        if (state >= ENEMY_STATE_DEFEATED_BOB) {
            return;
        }
        if (state != ENEMY_STATE_FOLLOW_MOVIE) {
            work->state = ENEMY_STATE_DEFEATED_BOB;
            return;
        }
        work->state = ENEMY_STATE_COLLAPSE;
    } else {
        if (state != ENEMY_STATE_ANIMATE_AT_PLACEMENT) {
            work->state = ENEMY_STATE_DEFEATED_BOB;
            return;
        }
        work->state = ENEMY_STATE_COLLAPSE;
    }
}

#include "../../../../shared/room/room_find_attack_contact.inc.c"

/// Marks whether this update enters a different bridge-enemy behavior state.
static inline void _akropolisBridgeTrackEnemyStateEntry(_EnemyWork* work)
{
    if (work->prevState != work->state) {
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = work->state;
}

/// Updates the bridge enemy's lighting, visibility, contacts and behavior.
static void _akropolisBridgeEnemyFrame(Enemy* enemy, Task* task)
{
    _EnemyWork*  work;
    _EnemyWork*  currentWork;
    _HitScratch* hitScratch;
    TmdObject*   model;
    VECTOR       worldPosition;
    s32          mappedView;
    s32          attackKey;
    u16          state;

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
            if ((u32)(state - ENEMY_STATE_COLLAPSE) >= 2U) {
                if (state != ENEMY_STATE_INACTIVE) {
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
                            if (work->state == ENEMY_STATE_FOLLOW_MOVIE) {
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
            if ((u32)(state - ENEMY_STATE_COLLAPSE) >= 2U && state != ENEMY_STATE_INACTIVE) {
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
    hitScratch      = SCRATCH_STACK_RESERVE_BLOCK(_HitScratch);
    attackKey       = _roomFindAttackContact(&hitScratch->point, work->bodyContacts, ARRAY_SIZE(work->bodyContacts));
    hitScratch->key = attackKey;
    if (attackKey != 0) {
        _akropolisBridgeEnemyApplyHit(task, attackKey);
    }

    currentWork = task->work;
    _akropolisBridgeTrackEnemyStateEntry(currentWork);
    _gAkropolisBridgeEnemyStateHandlers[work->state](task);
    worldCollisionClearContacts(&work->bodyContacts[0]);
    worldCollisionClearContacts(&work->attackContacts[0]);
    if (gSceneCombatState.battleRefs == 0) {
        if ((u32)((u16)work->state - ENEMY_STATE_DEFEATED_BOB) >= 2U) {
            work->state = ENEMY_STATE_INACTIVE;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_HitScratch);
}

/// Applies the bridge enemy's model-draw request and returns 1.
static s32 _akropolisBridgeEnemySetModelDraw(Task* task, s32 messageId, s32 drawRequest, s32 unusedArg)
{
    enum {
        HIDE             = 0,
        SHOW             = 1,
        SKIP_AUTO_BUFFER = 2
    };
    TmdObject* model;

    model = task->extra.tmd;
    if (drawRequest == HIDE) {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else if (drawRequest & SHOW) {
        model->flags = 0;
    } else if (drawRequest & SKIP_AUTO_BUFFER) {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 1;
}

/// Rebuilds the bridge enemy model's lighting at its root's world position.
static void _akropolisBridgeEnemyRelightModel(Task* task, s16 recomposeRoot)
{
    enum {
        RECOMPOSE_ROOT = 1,
        LIGHT_COUNT    = 3
    };
    VECTOR** scratchCursor;
    VECTOR*  previousTop;
    VECTOR*  worldPosition;

    scratchCursor  = &SCRATCH_STACK_CURSOR(VECTOR);
    previousTop    = *scratchCursor;
    worldPosition  = previousTop - 1;
    *scratchCursor = worldPosition;
    if (recomposeRoot == RECOMPOSE_ROOT) {
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(task->extra.tmd->coords);
    }
    (previousTop - 1)->vx = task->extra.tmd->coords->workm.t[0];
    worldPosition->vy     = task->extra.tmd->coords->workm.t[1];
    worldPosition->vz     = task->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(task->extra.tmd, worldPosition, 0, LIGHT_COUNT);
    *scratchCursor += 1;
}

/// Hides and disarms the bridge enemy in its inactive state.
static void _akropolisBridgeEnemyInactive(Task* task)
{
    _EnemyWork* work  = task->work;
    TmdObject*  model = task->extra.tmd;

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
static void _akropolisBridgeEnemyTask(Task* task)
{
    EnemyTaskFuncTable3 states;

    states = _gAkropolisBridgeEnemyTaskStates;
    states.funcs[task->state](task->spawnArg2.pointer, task);
}
