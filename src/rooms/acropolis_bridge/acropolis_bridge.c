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
#include "gameplay/object_fields.h"
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
#include "../../shared/action_prompt.h"
#include "../../shared/glow_draw.h"

/// Selects the bridge billboard's `u16` frame and `s16` size arguments.
///
/// Presence-only configuration for the first inclusion of `effect_sprite.h`.
/// The shared debris task calls this room's billboard implementation; the
/// chip drawer retains the common signature. Undefine after the header include.
#define EFFECT_SPRITE_BILLBOARD_HALFWORD_ARGUMENTS
// Exported instance: another image refers to this package's copy by name.
#define effectSpriteDebrisTask acropolisBridgeEffectSpriteDebrisTask
#include "../../shared/effect_sprite.h"
#undef EFFECT_SPRITE_BILLBOARD_HALFWORD_ARGUMENTS

#include "../../shared/boss_stranger.h"
#include "../../shared/bridge_model.h"
#include "../../shared/acropolis_glows.h"

/// `ActionPromptHotspot::id` of the keypad's clear key. The ten digit keys
/// carry their own digit, 0..9, as their id.
#define ACROPOLIS_BRIDGE_KEYPAD_KEY_CLEAR 0xA

/// Digits in a complete bridge code; the keypad checks the entry once this
/// many have been typed.
#define ACROPOLIS_BRIDGE_KEYPAD_CODE_DIGITS 3

/// `_AcropolisBridgeKeypadWork::code` with nothing entered. A nibble above 9
/// draws as an empty digit, so this value blanks all three.
#define ACROPOLIS_BRIDGE_KEYPAD_CODE_BLANK 0xFFF

/// The code the keypad accepts, one digit per nibble in the order typed.
#define ACROPOLIS_BRIDGE_KEYPAD_CODE_CORRECT 0x561

/// Frames a complete entry stays on the display before it is checked.
#define ACROPOLIS_BRIDGE_KEYPAD_CHECK_DELAY_FRAMES 10

/// Frames in each half of one blink of the keypad's result display. A blink
/// cycle is two halves, and the frame after them counts the cycle.
#define ACROPOLIS_BRIDGE_KEYPAD_BLINK_PHASE_FRAMES 10

/// Blink cycles the result display runs after a code is checked.
#define ACROPOLIS_BRIDGE_KEYPAD_BLINK_COUNT 3

/// Wrong codes after which the keypad closes instead of taking another entry.
#define ACROPOLIS_BRIDGE_KEYPAD_REJECTED_LIMIT 3

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
/// `effectSpriteDrawBillboard` reserves one block and copies the piece's
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

extern ActionPromptHotspot D_acropolis_bridge_8018983C[];

extern s32 D_acropolis_bridge_801917A8;

/// Cursor into the packet buffer the bridge's screen-smear effects draw from.
/// Every `DR_MOVE` task of the room takes the packet it points at
/// and bumps it by one, the same way `gGpuPrimCursor` works for the main
/// primitive heap.
extern DR_MOVE* D_acropolis_bridge_801917AC;

extern EffectUnitQuadCorner D_acropolis_bridge_8018990C[4];

extern EnemyParams D_acropolis_bridge_80190C5C;

static void func_acropolis_bridge_8017DC68(Task* arg0);

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u16 D_acropolis_bridge_801917A4[2];

extern TaskMessageEntry D_acropolis_bridge_80188E4C[];
extern TaskDesc         D_acropolis_bridge_80188E7C[];
extern EvsCommand       D_acropolis_bridge_80188EBC[];
extern EvsCommand       D_acropolis_bridge_8018912C[];
extern TaskDesc         D_acropolis_bridge_80189234;
extern SVECTOR          D_acropolis_bridge_80189240[];
extern TaskDesc         D_acropolis_bridge_80189830;

/// Three 16-entry rows, one per digit of the bridge code, mapping a nibble to
/// the SPRT command that renders it. Entries above 9 hold the row's blank
/// sentinel.
extern u8 D_acropolis_bridge_801898CC[3][16];

extern TaskMessageEntry D_acropolis_bridge_801898FC[];
extern SVECTOR          D_acropolis_bridge_8018991C[7];
extern SVECTOR          D_acropolis_bridge_80189954[7];
extern SVECTOR          D_acropolis_bridge_8018998C[12];
extern u16              D_acropolis_bridge_801899EC[8];
extern u16              D_acropolis_bridge_801899FC[16];
extern u16              D_acropolis_bridge_80189A1C[12];
extern SVECTOR          D_acropolis_bridge_80189A34[2];
extern SVECTOR          D_acropolis_bridge_80189A44;
extern SVECTOR          D_acropolis_bridge_80189A4C;

/// The two 0x18-byte script work blocks `Gp_SpawnScript18` copies from when the
/// bridge cutscene starts.
extern PadScriptCmd              D_acropolis_bridge_80190B8C[6];
extern PadScriptVibrationSegment D_acropolis_bridge_80190BA4[6];

extern s16 D_acropolis_bridge_801915E4[][6];

/// State handler table the per-frame tick dispatches through on
/// `_AcropolisBridgeEnemyWork::state`.
extern void (*D_acropolis_bridge_8019175C[])(Task*);

extern Task* D_acropolis_bridge_80191794;
extern Task* D_acropolis_bridge_80191798;
extern Task* D_acropolis_bridge_8019179C;
extern s32   D_acropolis_bridge_801917A0;

static void func_acropolis_bridge_8017D98C(Task* task);
static void func_acropolis_bridge_8017D9FC(Task* task);
static void func_acropolis_bridge_8017DB08(Task* task);
static void func_acropolis_bridge_8017DB60(Task* task);
static void func_acropolis_bridge_8017DBA0(Task* task);
static void func_acropolis_bridge_8017DC1C(Task* task);
static void func_acropolis_bridge_8017DD24(Task* task);
static void func_acropolis_bridge_8017DD88(Task* task);
static void func_acropolis_bridge_8017DD9C(Task* task);
static void func_acropolis_bridge_8017DDEC(Task* task);
static void func_acropolis_bridge_8017DE94(Task* task);
static s16  func_acropolis_bridge_8017E024(void);
static void func_acropolis_bridge_8017E04C(Task* task);
static void func_acropolis_bridge_8017E1D0(Task* task);
static void func_acropolis_bridge_8017E3A0(Task* task);
static void func_acropolis_bridge_8017E4FC(Task* task);
static void func_acropolis_bridge_8017E81C(void);
void        func_acropolis_bridge_8017F2D0(s32 flags);
static void func_acropolis_bridge_8017F404(Task* task);
static void func_acropolis_bridge_8017F460(Task* task);
static void func_acropolis_bridge_8017F4CC(Task* task);
static void func_acropolis_bridge_8017F544(Task* task);
static void func_acropolis_bridge_8017F658(Task* task);
static void func_acropolis_bridge_801827EC(GfxCoord* coord, s32 arg1, s16 arg2);
static void func_acropolis_bridge_8018581C(Task* task);
static void func_acropolis_bridge_80185988(Enemy* enemy, Task* task);
static void func_acropolis_bridge_80187850(Enemy* enemy, Task* task);

/// Work block of the enemy that lurks below the bridge, allocated into `Task::work`.
///
/// `state` indexes `D_acropolis_bridge_8019175C`; each handler runs its entry
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

static void func_acropolis_bridge_8017E60C(s32 digits, s32 hidePrompt);

void func_acropolis_bridge_8017DEE4(Task*);
void func_acropolis_bridge_8017F280(Task*);
s32  func_acropolis_bridge_801820A0(Task*, s32, s32, s32);

extern WorldCollisionGrid    D_acropolis_bridge_8018A89C[1];
extern WorldCollisionGrid    D_acropolis_bridge_8018B694[1];
extern WorldCollisionTrigger D_acropolis_bridge_8018B6B8[4];
extern WorldCollisionTrigger D_acropolis_bridge_8018B7E8[7];
extern WorldCollisionTrigger D_acropolis_bridge_8018B9FC[7];

extern WorldCoordRoomLights      D_acropolis_bridge_80190A0C[1];
extern PadScriptCmd              D_acropolis_bridge_80190BBC[6];
extern PadScriptVibrationSegment D_acropolis_bridge_80190BD4[5];
void                             func_acropolis_bridge_8017D954(void);
void                             func_acropolis_bridge_8017F2D0(s32);
void                             func_acropolis_bridge_8017F358(s32);

static SVECTOR _gAcropolisBridgeModel0AD9CVerts[171];
static TmdBone _gAcropolisBridgeModel0AD9CSkeleton[1];
static u32     _gAcropolisBridgeModel0AD9CPartVerts[1];
static u32     _gAcropolisBridgeModel0AD9CStream[691];
s32            func_acropolis_bridge_8017D6F4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32            func_acropolis_bridge_8017D7F0(Task*, s32, s32, s32);
s32            func_acropolis_bridge_8017D7F8(Task*, s32, s32, s32);
s32            func_acropolis_bridge_8017D868(Task*, s32, s32, s32);
s32            func_acropolis_bridge_8017D870(Task*, s32, s32, s32);
void           func_acropolis_bridge_8017D878(Task*);
void           func_acropolis_bridge_8017D8D0(Task*);

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

extern _AcropolisBridgeEnemyTaskDescStorage D_acropolis_bridge_80191780;

extern WorldCollisionFootstepSounds D_acropolis_bridge_80190BE8;
extern WorldCollisionFootstepSounds D_acropolis_bridge_80190BF4;
s32                                 func_acropolis_bridge_801856E0(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);
s32                                 func_acropolis_bridge_80187BD0(Task*, s32, s32, s32);
void                                func_acropolis_bridge_80185F28(Task*);
void                                func_acropolis_bridge_801861A0(Task*);
void                                func_acropolis_bridge_801863A8(Task*);
void                                func_acropolis_bridge_80186618(Task*);
void                                func_acropolis_bridge_80186BBC(Task*);
void                                func_acropolis_bridge_80187078(Task*);
void                                func_acropolis_bridge_80187310(Task*);
void                                func_acropolis_bridge_801874DC(Task*);
void                                func_acropolis_bridge_80187D04(Task*);
void                                func_acropolis_bridge_80187D80(Task*);

static TmdBone _gAcropolisBridgeModel0AD9CSkeleton[1] = {
#include "assets/acropolis_bridge_model_0AD9C_skeleton.inc"
};

static u32 _gAcropolisBridgeModel0AD9CPartVerts[1] = {
#include "assets/acropolis_bridge_model_0AD9C_partVerts.inc"
};

static SVECTOR _gAcropolisBridgeModel0AD9CVerts[171] = {
#include "assets/acropolis_bridge_model_0AD9C_verts.inc"
};

static u32 _gAcropolisBridgeModel0AD9CStream[691] = {
#include "assets/acropolis_bridge_model_0AD9C_stream.inc"
};

TmdSource D_acropolis_bridge_80188E28[1] = {
    { 0, 5480, 0, 1, _gAcropolisBridgeModel0AD9CPartVerts, _gAcropolisBridgeModel0AD9CVerts, &_gAcropolisBridgeModel0AD9CVerts[171], _gAcropolisBridgeModel0AD9CSkeleton, _gAcropolisBridgeModel0AD9CStream },
};

TaskMessageEntry D_acropolis_bridge_80188E4C[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_acropolis_bridge_8017D6F4 },
    { ROOM_MESSAGE_COMMAND, func_acropolis_bridge_8017D7F8 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_acropolis_bridge_8017D868 },
    { 5105, func_acropolis_bridge_8017D7F0 },
    { ROOM_MESSAGE_SOUND, func_acropolis_bridge_8017D870 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_acropolis_bridge_80188E7C[3] = {
    { { { TASK_BODY_TMD, 192 } }, func_acropolis_bridge_8017D878, { .model = D_acropolis_bridge_80188E28 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_bridge_8017D8D0, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

AnimationPlayRequest D_acropolis_bridge_80188EA0 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

EvsSceneKey D_acropolis_bridge_80188EB4 = { 1, 11, 11 };

EvsCommand D_acropolis_bridge_80188EBC[26] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_acropolis_bridge_8017D954 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_acropolis_bridge_8017F358 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_bridge_80188EA0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_acropolis_bridge_80188EB4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SHAKE_SCREEN, { .value = 4 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_acropolis_bridge_80190BBC }, { .vibrationSegments = D_acropolis_bridge_80190BD4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_ROOM_EFFECT }, { .value = 0 }, { .value = 3104 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_acropolis_bridge_8017F358 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_acropolis_bridge_8017F358 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_acropolis_bridge_8017F2D0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FINISH_SCENE_STREAM, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_bridge_8018912C[11] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_acropolis_bridge_8017F358 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_acropolis_bridge_8017F2D0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_acropolis_bridge_80189234 = { { { TASK_BODY_NONE, 192 } }, func_acropolis_bridge_8017DEE4, { .value = 0 } };

SVECTOR D_acropolis_bridge_80189240[190] = {
    { -0x2710, 1200, -2000, 0 },
    { -0x2710, 1183, -2000, 0 },
    { -0x2710, 1167, -2000, 0 },
    { -0x2710, 1150, -2000, 0 },
    { -0x2710, 1134, -2000, 0 },
    { -0x2710, 1117, -2000, 0 },
    { -0x2710, 1101, -2000, 0 },
    { -0x2710, 1084, -2000, 0 },
    { -0x2710, 1068, -2000, 0 },
    { -0x2710, 1051, -2000, 0 },
    { -0x2710, 1035, -2000, 0 },
    { -0x2710, 1019, -2000, 0 },
    { -0x2710, 1002, -2000, 0 },
    { -0x2710, 986, -2000, 0 },
    { -0x2710, 969, -2000, 0 },
    { -0x2710, 953, -2000, 0 },
    { -0x2710, 936, -2000, 0 },
    { -0x2710, 920, -2000, 0 },
    { -0x2710, 903, -2000, 0 },
    { -0x2710, 887, -2000, 0 },
    { -0x2710, 870, -2000, 0 },
    { -0x2710, 854, -2000, 0 },
    { -0x2710, 838, -2000, 0 },
    { -0x2710, 821, -2000, 0 },
    { -0x2710, 805, -2000, 0 },
    { -0x2710, 788, -2000, 0 },
    { -0x2710, 772, -2000, 0 },
    { -0x2710, 755, -2000, 0 },
    { -0x2710, 739, -2000, 0 },
    { -0x2710, 722, -2000, 0 },
    { -0x2710, 706, -2000, 0 },
    { -0x2710, 690, -2000, 0 },
    { -0x2710, 673, -2000, 0 },
    { -0x2710, 657, -2000, 0 },
    { -0x2710, 640, -2000, 0 },
    { -0x2710, 624, -2000, 0 },
    { -0x2710, 607, -2000, 0 },
    { -0x2710, 591, -2000, 0 },
    { -0x2710, 574, -2000, 0 },
    { -0x2710, 558, -2000, 0 },
    { -0x2710, 541, -2000, 0 },
    { -0x2710, 525, -2000, 0 },
    { -0x2710, 509, -2000, 0 },
    { -0x2710, 492, -2000, 0 },
    { -0x2710, 476, -2000, 0 },
    { -0x2710, 459, -2000, 0 },
    { -0x2710, 443, -2000, 0 },
    { -0x2710, 426, -2000, 0 },
    { -0x2710, 410, -2000, 0 },
    { -0x2710, 393, -2000, 0 },
    { -0x2710, 377, -2000, 0 },
    { -0x2710, 360, -2000, 0 },
    { -0x2710, 344, -2000, 0 },
    { -0x2710, 328, -2000, 0 },
    { -0x2710, 311, -2000, 0 },
    { -0x2710, 295, -2000, 0 },
    { -0x2710, 278, -2000, 0 },
    { -0x2710, 262, -2000, 0 },
    { -0x2710, 245, -2000, 0 },
    { -0x2710, 229, -2000, 0 },
    { -0x2710, 212, -2000, 0 },
    { -0x2710, 196, -2000, 0 },
    { -0x2710, 180, -2000, 0 },
    { -0x2710, 163, -2000, 0 },
    { -0x2710, 147, -2000, 0 },
    { -0x2710, 130, -2000, 0 },
    { -0x2710, 114, -2000, 0 },
    { -0x2710, 97, -2000, 0 },
    { -0x2710, 81, -2000, 0 },
    { -0x2710, 64, -2000, 0 },
    { -0x2710, 48, -2000, 0 },
    { -0x2710, 31, -2000, 0 },
    { -0x2710, 15, -2000, 0 },
    { -0x2710, 0, -2000, 0 },
    { -0x2710, -17, -2000, 0 },
    { -0x2710, -33, -2000, 0 },
    { -0x2710, -50, -2000, 0 },
    { -0x2710, -66, -2000, 0 },
    { -0x2710, -83, -2000, 0 },
    { -0x2710, -99, -2000, 0 },
    { -0x2710, -116, -2000, 0 },
    { -0x2710, -132, -2000, 0 },
    { -0x2710, -149, -2000, 0 },
    { -0x2710, -165, -2000, 0 },
    { -0x2710, -181, -2000, 0 },
    { -0x2710, -198, -2000, 0 },
    { -0x2710, -214, -2000, 0 },
    { -0x2710, -231, -2000, 0 },
    { -0x2710, -247, -2000, 0 },
    { -0x2710, -264, -2000, 0 },
    { -0x2710, -280, -2000, 0 },
    { -0x2710, -297, -2000, 0 },
    { -0x2710, -313, -2000, 0 },
    { -0x2710, -330, -2000, 0 },
    { -0x2710, -346, -2000, 0 },
    { -0x2710, -362, -2000, 0 },
    { -0x2710, -379, -2000, 0 },
    { -0x2710, -395, -2000, 0 },
    { -0x2710, -412, -2000, 0 },
    { -0x2710, -428, -2000, 0 },
    { -0x2710, -445, -2000, 0 },
    { -0x2710, -461, -2000, 0 },
    { -0x2710, -478, -2000, 0 },
    { -0x2710, -494, -2000, 0 },
    { -0x2710, -510, -2000, 0 },
    { -0x2710, -527, -2000, 0 },
    { -0x2710, -543, -2000, 0 },
    { -0x2710, -560, -2000, 0 },
    { -0x2710, -576, -2000, 0 },
    { -0x2710, -593, -2000, 0 },
    { -0x2710, -609, -2000, 0 },
    { -0x2710, -626, -2000, 0 },
    { -0x2710, -642, -2000, 0 },
    { -0x2710, -659, -2000, 0 },
    { -0x2710, -675, -2000, 0 },
    { -0x2710, -691, -2000, 0 },
    { -0x2710, -708, -2000, 0 },
    { -0x2710, -724, -2000, 0 },
    { -0x2710, -741, -2000, 0 },
    { -0x2710, -757, -2000, 0 },
    { -0x2710, -774, -2000, 0 },
    { -0x2710, -790, -2000, 0 },
    { -0x2710, -792, -2000, 0 },
    { -0x2710, -783, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
    { -0x2710, -800, -2000, 0 },
};

TaskDesc D_acropolis_bridge_80189830 = { { { TASK_BODY_NONE, 192 } }, func_acropolis_bridge_8017F280, { .value = 0 } };

ActionPromptHotspot D_acropolis_bridge_8018983C[12] = {
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
    { 42, 23, 40, 21, 10, 0, 0 },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
};

u8 D_acropolis_bridge_801898CC[3][16] = {
    { 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 33, 33, 33, 33, 33, 33 },
    { 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 32, 32, 32, 32, 32, 32 },
    { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 31, 31, 31, 31, 31, 31 },
};

TaskMessageEntry D_acropolis_bridge_801898FC[2] = {
    { 3104, func_acropolis_bridge_801820A0 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

EffectUnitQuadCorner D_acropolis_bridge_8018990C[4] = {
    { -1, 1 },
    { 1, 1 },
    { -1, -1 },
    { 1, -1 },
};

SVECTOR D_acropolis_bridge_8018991C[7] = {
    { -3920, -1494, -3450, 0 },
    { -6900, -1494, -3564, 0 },
    { -9950, -1494, -3564, 0 },
    { -0x3264, -1494, -3564, 0 },
    { -7090, -1494, -500, 0 },
    { -0x2788, -1494, -500, 0 },
    { -0x334A, -1494, -500, 0 },
};

SVECTOR D_acropolis_bridge_80189954[7] = {
    { -3920, 300, -2543, 0 },
    { -6900, 300, -2543, 0 },
    { -9950, 300, -2543, 0 },
    { -0x3264, 300, -2543, 0 },
    { -7090, 300, -1524, 0 },
    { -0x2788, 300, -1524, 0 },
    { -0x334A, 300, -1524, 0 },
};

SVECTOR D_acropolis_bridge_8018998C[12] = {
    { 810, -3000, -3720, 0 },
    { 650, -3000, -4000, 0 },
    { 810, -3000, -4270, 0 },
    { -2470, -2050, -740, 0 },
    { -2520, -2050, -3120, 0 },
    { -0x3E80, -1700, -1330, 0 },
    { -0x3E80, -1700, -2700, 0 },
    { 1680, -1700, 0x2760, 0 },
    { 1680, -1700, 8430, 0 },
    { 1640, -360, 7020, 0 },
    { -2150, -360, 8460, 0 },
    { -3400, -370, 2300, 0 },
};

u16 D_acropolis_bridge_801899EC[8] = {
    364,
    360,
    328,
    346,
    364,
    328,
    346,
    0,
};

u16 D_acropolis_bridge_801899FC[16] = {
    0,
    3,
    0,
    5,
    6,
    1,
    2,
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

u16 D_acropolis_bridge_80189A1C[12] = {
    4,
    68,
    64,
    64,
    68,
    768,
    768,
    0,
    0,
    0,
    0,
    40,
};

SVECTOR D_acropolis_bridge_80189A34[2] = { 0 };

SVECTOR D_acropolis_bridge_80189A44 = { -4700, -1000, -620, 0 };

SVECTOR D_acropolis_bridge_80189A4C = { -4700, -1000, -520, 0 };

WorldCollisionRoomResources D_acropolis_bridge_80189A54[2] = {
    { D_acropolis_bridge_8018A89C, D_acropolis_bridge_8018B6B8, D_acropolis_bridge_8018B7E8, NULL },
    { D_acropolis_bridge_8018B694, D_acropolis_bridge_8018B6B8, D_acropolis_bridge_8018B9FC, NULL },
};

u8 D_acropolis_bridge_80189A74[12] = {
    1,
    5,
    6,
    7,
    2,
    3,
    4,
    8,
    9,
    10,
    0,
    0,
};

u8* D_acropolis_bridge_80189A80[2] = {
    gViewIdentityMap,
    D_acropolis_bridge_80189A74,
};

ViewCount D_acropolis_bridge_80189A88[2] = { 10, 10 };

WorldCoordRoomLighting D_acropolis_bridge_80189A8C[2] = {
    { D_acropolis_bridge_80190A0C, NULL },
    { D_acropolis_bridge_80190A0C, NULL },
};

// Two 12-byte direction-facing rows used by Gp_MsgPlayerDirFacing.
u8 D_acropolis_bridge_80189A9C[24] = {
    2,
    2,
    2,
    2,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
};

DirectionWarpEntry D_acropolis_bridge_80189AB4[2] = {
    { { { .word = 3072 }, -3643, 2, -2161 }, { 0, 0, 0, 0 }, { { .word = 3072 }, -3643, 2, -2161 }, { 0, 0, 0, 0 }, 0x510E0008, 0x510E0007, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_NONE, 491 },
    { { { .word = 1024 }, -0x3D4E, 0, -2098 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -0x3D4E, 0, -2098 }, { 0, 0, 0, 0 }, 0x510E0006, 0x510E0005, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 490 },
};

static SVECTOR _gAcropolisBridgeCollision0D2DCNormals[19] = {
#include "assets/acropolis_bridge_collision_0D2DC_normals.inc"
};

static SVECTOR _gAcropolisBridgeCollision0D2DCVerts[193] = {
#include "assets/acropolis_bridge_collision_0D2DC_verts.inc"
};

static WorldCollisionGridFace _gAcropolisBridgeCollision0D2DCFaces[92] = {
#include "assets/acropolis_bridge_collision_0D2DC_faces.inc"
};

static s16 _gAcropolisBridgeCollision0D2DCCells[274] = {
#include "assets/acropolis_bridge_collision_0D2DC_cells.inc"
};

#define GRID_CELL(i) (&_gAcropolisBridgeCollision0D2DCCells[i])
static s16* _gAcropolisBridgeCollision0D2DCTable[25] = {
#include "assets/acropolis_bridge_collision_0D2DC_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_acropolis_bridge_8018A89C[1] = {
    { NULL, _gAcropolisBridgeCollision0D2DCNormals, _gAcropolisBridgeCollision0D2DCVerts, _gAcropolisBridgeCollision0D2DCFaces, _gAcropolisBridgeCollision0D2DCTable, 0x3E94, 8470, 5, 5, 4000, 92 },
};

static SVECTOR _gAcropolisBridgeCollision0E0D4Normals[17] = {
#include "assets/acropolis_bridge_collision_0E0D4_normals.inc"
};

static SVECTOR _gAcropolisBridgeCollision0E0D4Verts[205] = {
#include "assets/acropolis_bridge_collision_0E0D4_verts.inc"
};

static WorldCollisionGridFace _gAcropolisBridgeCollision0E0D4Faces[93] = {
#include "assets/acropolis_bridge_collision_0E0D4_faces.inc"
};

static s16 _gAcropolisBridgeCollision0E0D4Cells[274] = {
#include "assets/acropolis_bridge_collision_0E0D4_cells.inc"
};

#define GRID_CELL(i) (&_gAcropolisBridgeCollision0E0D4Cells[i])
static s16* _gAcropolisBridgeCollision0E0D4Table[25] = {
#include "assets/acropolis_bridge_collision_0E0D4_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_acropolis_bridge_8018B694[1] = {
    { NULL, _gAcropolisBridgeCollision0E0D4Normals, _gAcropolisBridgeCollision0E0D4Verts, _gAcropolisBridgeCollision0E0D4Faces, _gAcropolisBridgeCollision0E0D4Table, 0x3E94, 8470, 5, 5, 4000, 93 },
};

WorldCollisionTrigger D_acropolis_bridge_8018B6B8[4] = {
    { NULL, NULL, NULL, { -7584, 0, -1984, 0 }, { { 0, -4175, -2304, 0 }, { 0, -4209, 2304, 0 }, { 0, 4209, -2304, 0 }, { 0, 4175, 2304, 0 } }, { 4116, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4775, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7040, 0, -1984, 0 }, { { 0, -4175, 2304, 0 }, { 0, -4209, -2304, 0 }, { 0, 4209, 2304, 0 }, { 0, 4175, -2304, 0 } }, { -4117, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4775, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -0x2E00, 0, -1984, 0 }, { { 0, -4175, 2304, 0 }, { 0, -4209, -2304, 0 }, { 0, 4209, 2304, 0 }, { 0, 4175, -2304, 0 } }, { -4117, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4775, 0, 2, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -0x301F, 0, -1983, 0 }, { { 0, -4175, -2304, 0 }, { 0, -4209, 2304, 0 }, { 0, 4209, -2304, 0 }, { 0, 4175, 2304, 0 } }, { 4116, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4775, 0, 7, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_acropolis_bridge_8018B7E8[7] = {
    { NULL, NULL, NULL, { -2995, -144, -2076, 0 }, { { -336, 0, -1024, 0 }, { 336, 0, -1024, 0 }, { -336, 0, 1024, 0 }, { 336, 0, 1024, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 1070, WORLD_COLLISION_TRIGGER_ACTION_WARP, 11, 21, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4544, -128, -448, 0 }, { { -576, 0, -576, 0 }, { 576, 0, -576, 0 }, { -576, 0, 576, 0 }, { 576, 0, 576, 0 } }, { 0, 4105, 0, 0 }, { 4094, 0, -1, 0 }, 814, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5009, -64, -2432, 0 }, { { -208, 0, -1024, 0 }, { 208, 0, -1024, 0 }, { -208, 0, 1024, 0 }, { 208, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4091, 0, 201, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100, 133, 192, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6176, 992, -2016, 0 }, { { -208, 0, -1024, 0 }, { 208, 0, -1024, 0 }, { -208, 0, 1024, 0 }, { 208, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4091, 0, 200, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_FACING, 133, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -0x33C0, 928, -2048, 0 }, { { -208, 0, -1024, 0 }, { 208, 0, -1024, 0 }, { -208, 0, 1024, 0 }, { 208, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4091, 0, 201, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_FACING, 133, 192, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -0x38E0, -64, -1984, 0 }, { { -208, 0, -1024, 0 }, { 208, 0, -1024, 0 }, { -208, 0, 1024, 0 }, { 208, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100, 133, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -0x3D00, -64, -2016, 0 }, { { -208, 0, -1024, 0 }, { 208, 0, -1024, 0 }, { -208, 0, 1024, 0 }, { 208, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4091, 0, 201, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_WARP, 15, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_acropolis_bridge_8018B9FC[7] = {
    { NULL, NULL, NULL, { -2979, -112, -1404, 0 }, { { -288, 0, -1024, 0 }, { 288, 0, -1024, 0 }, { -288, 0, 1024, 0 }, { 288, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1063, WORLD_COLLISION_TRIGGER_ACTION_WARP, 11, 21, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4544, -128, -448, 0 }, { { -576, 0, -576, 0 }, { 576, 0, -576, 0 }, { -576, 0, 576, 0 }, { 576, 0, 576, 0 } }, { 0, 4105, 0, 0 }, { 4094, 0, -1, 0 }, 814, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4893, -64, -2624, 0 }, { { -208, 0, -1024, 0 }, { 208, 0, -1024, 0 }, { -208, 0, 1024, 0 }, { 208, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4091, 0, 201, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_FACING, 69, 192, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6113, -992, -2016, 0 }, { { -367, 0, -1024, 0 }, { 368, 0, -1024, 0 }, { -367, 0, 1024, 0 }, { 368, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { -4091, 0, 200, 0 }, 1086, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100, 69, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -0x33D0, -1056, -2048, 0 }, { { -256, 0, -1024, 0 }, { 256, 0, -1024, 0 }, { -256, 0, 1024, 0 }, { 256, 0, 1024, 0 } }, { 0, 4096, 0, 0 }, { 4091, 0, 201, 0 }, 1055, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100, 69, 192, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -0x38C0, -64, -1984, 0 }, { { -240, 0, -1024, 0 }, { 240, 0, -1024, 0 }, { -240, 0, 1024, 0 }, { 240, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1047, WORLD_COLLISION_TRIGGER_ACTION_FACING, 69, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -0x3DA0, -64, -2016, 0 }, { { -208, 0, -1024, 0 }, { 208, 0, -1024, 0 }, { -208, 0, 1024, 0 }, { 208, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4091, 0, 201, 0 }, 1039, WORLD_COLLISION_TRIGGER_ACTION_WARP, 15, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

SpriteBatch D_acropolis_bridge_8018BC10[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_bridge_8018BC20[104] = {
    { 143, 0x3FC0, { .fields = { 40, 80 } }, -128, 16, 1200, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 88 } }, -88, 16, 1200, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 32, 1200, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -56, 64, 1200, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 80 } }, 96, 40, 1025, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 88, 80, 1025, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 96, 40, 650, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 64 } }, -152, -96, 1000, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 16 } }, -152, -32, 1006, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 56 } }, -144, -16, 1005, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 16, 1005, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -96, 40, 1005, { .fields = { 8, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 56 } }, 104, -96, 753, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, 120, -40, 752, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 80 } }, 104, -8, 650, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, 128, 72, 650, { .fields = { 120, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 64, 32, 1000, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 64, 40, 939, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 64, 48, 885, { .fields = { 24, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 64, 56, 836, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 64, 64, 777, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 64, 72, 754, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 64, 80, 718, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 64, 88, 686, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 64, 96, 609, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 64, 104, 598, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 64, 112, 575, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, 24, 1087, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -48, 32, 1000, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, 40, 939, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -64, 48, 895, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 56, 836, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, 64, 810, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -80, 72, 712, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, 80, 718, { .fields = { 32, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, 88, 698, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -104, 96, 661, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, 104, 630, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -120, 112, 605, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 64, 40, 925, { .fields = { 0, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 56, 88, 880, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 56, 104, 901, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 64, 104, 636, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 56, 1135, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -40, 32, 1116, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -48, 48, 972, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 48, 972, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -64, 80, 852, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -72, 72, 872, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -96, 104, 677, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 56, 48, 904, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 56, 56, 1002, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 56, 64, 1090, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 56, 72, 855, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 56, 80, 867, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 56, 96, 932, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 56, 112, 731, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -16, -32, 1363, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -24, -32, 1214, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -32, -8, 1124, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 64, -32, 1179, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, -16, 1118, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 64, 0, 1069, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 16, 1025, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 56, 1187, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -40, 64, 1134, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 8, 64, 1122, { .fields = { 48, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -48, 72, 1078, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 8, 72, 1078, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -56, 80, 1027, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 8, 80, 1027, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -56, 88, 981, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 8, 88, 981, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -64, 96, 939, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 0, 96, 944, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, -72, 104, 901, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, 0, 104, 901, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, -72, 112, 862, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, 0, 112, 865, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 72, 1097, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 8, 1328, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 104, 16 } }, -32, 16, 1242, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 104, 16 } }, -32, 32, 1185, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 104, 16 } }, -32, 48, 1131, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 40, 64, 1097, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -24, 0, 1275, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -24, 8, 1288, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 16, 1275, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 40, 16, 1252, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -8, -24, 1728, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -16, -16, 1630, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -16, -8, 1463, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -16, 0, 1341, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -16, 8, 1375, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 56, 8, 1375, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 48, 56, 1375, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -48, 1441, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, -40, 1214, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 0, -56, 1696, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -8, -48, 1722, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -16, -48, 1329, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 64, -32, 1264, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 80, 80 } }, -8, -96, 1725, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 8 } }, 16, -16, 1627, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_bridge_8018C440[13] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 1, 0 } },
    { 6, 10, 0, 0, { 6, 0 } },
    { 16, 23, 0, 0, { 2, 0 } },
    { 39, 18, 0, 0, { 7, 0 } },
    { 57, 7, 0, 0, { 5, 0 } },
    { 64, 15, 0, 0, { 8, 0 } },
    { 79, 6, 0, 0, { 4, 0 } },
    { 85, 4, 0, 0, { 9, 0 } },
    { 89, 7, 0, 0, { 0, 0 } },
    { 96, 6, 0, 0, { 10, 0 } },
    { 102, 2, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_bridge_8018C4A8[112] = {
    { 143, 0x3FC0, { .fields = { 72, 112 } }, -160, -96, 1425, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 24 } }, -96, -120, 1400, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 24 } }, -88, -96, 1425, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 88 } }, -88, -72, 1429, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 24 } }, 16, -120, 1375, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 16, -96, 1420, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 48 } }, 16, -48, 1425, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 56 } }, 48, -96, 1525, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 40 } }, 48, -40, 1525, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 88 } }, 104, -88, 1525, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 136, -88, 1525, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 144 } }, -160, -96, 1250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 72 } }, -144, -40, 1651, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -128, 16, 850, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 72 } }, -120, 0, 850, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 64 } }, -80, 0, 850, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, 128, -88, 1478, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -24, 1450, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -96, 16, 1450, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -128, 16, 1125, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -120, 0, 1150, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -112, -16, 1275, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -104, -24, 1375, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 16, -32, 1561, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 24, -32, 1421, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 32, -24, 1275, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 40, -8, 1176, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 48, 0, 1082, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 64, 8 } }, -40, -8, 1661, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 64, 8 } }, -72, 0, 1566, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 40, 8 } }, -8, 0, 1566, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 72, 8 } }, -96, 8, 1470, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 56, 8 } }, -24, 8, 1470, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 80, 8 } }, -104, 16, 1385, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 64, 8 } }, -24, 16, 1385, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 72, 8 } }, -104, 24, 1309, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 72, 8 } }, -32, 24, 1309, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 72, 8 } }, -104, 32, 1242, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 80, 8 } }, -32, 32, 1242, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 80, 8 } }, -112, 40, 1200, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 80, 8 } }, -32, 40, 1200, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 80, 8 } }, -112, 48, 1175, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 80, 8 } }, -32, 48, 1175, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 96, 8 } }, -112, 56, 1150, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -112, 64, 1125, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 40 } }, -104, -24, 1428, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 64 } }, 48, -72, 875, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 48 } }, 40, -8, 807, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 72 } }, -160, 48, 403, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 56, 96, 790, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 64, 56, 843, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 88, 48, 930, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 40 } }, 136, 80, 739, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 72, 40, 867, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -56, 56, 843, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -64, 56, 886, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 32 } }, -56, 56, 893, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 40 } }, -8, 56, 880, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 40 } }, 40, 48, 907, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, 8, 96, 790, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 32, 800, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 88, 8 } }, -64, 48, 750, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, 24, 40, 800, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 80, 8 } }, 24, 48, 750, { .fields = { 80, 8 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 96, 8 } }, -72, 56, 712, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 88, 8 } }, 24, 56, 712, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 96, 8 } }, -72, 64, 687, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 96, 8 } }, 24, 64, 687, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 96, 8 } }, -72, 72, 662, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 96, 8 } }, 24, 72, 662, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 96, 16 } }, -72, 80, 612, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 112, 16 } }, 24, 80, 612, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 96, 16 } }, -72, 96, 587, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 128, 16 } }, 24, 96, 587, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 104, 8 } }, -80, 112, 550, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 136, 8 } }, 24, 112, 550, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 88, -16, 1280, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -72, -8, 600, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -64, 16, 725, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -80, 16, 575, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -72, 48, 500, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -64, 88, 550, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -88, 48, 475, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 32 } }, -96, 88, 425, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 88, 16, 695, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 96, -16, 650, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 104, -16, 583, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 120, 0, 529, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 136, 8, 502, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 144, 16, 465, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 128, 32, 531, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 120, 56, 565, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 112, 80, 572, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -72, -8, 725, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 40, 725, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, 16, 806, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 72 } }, 72, -16, 806, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -64, 0, 771, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 64 } }, 56, -8, 845, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -64, 0, 771, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 48, -8, 982, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -64, 8, 838, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 40, -8, 1051, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 80, 16 } }, 0, 40, 850, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, -64, 48, 850, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 16 } }, -16, 40, 900, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 56, 32, 900, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, -64, 48, 900, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 48, 16 } }, 16, 32, 950, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 80, 16 } }, -64, 40, 950, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, 24, 32, 1000, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 80, 16 } }, -56, 40, 1000, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_bridge_8018CD68[19] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 6, 0 } },
    { 11, 6, 0, 0, { 9, 0 } },
    { 17, 11, 0, 0, { 2, 0 } },
    { 28, 17, 0, 0, { 10, 0 } },
    { 45, 3, 0, 0, { 0, 0 } },
    { 48, 8, 0, 0, { 11, 0 } },
    { 56, 4, 0, 0, { 1, 0 } },
    { 60, 16, 0, 0, { 12, 0 } },
    { 76, 17, 0, 0, { 8, 0 } },
    { 93, 4, 0, 0, { 13, 0 } },
    { 97, 2, 0, 0, { 4, 0 } },
    { 99, 2, 0, 0, { 14, 0 } },
    { 101, 2, 0, 0, { 3, 0 } },
    { 103, 2, 0, 0, { 15, 0 } },
    { 105, 3, 0, 0, { 7, 0 } },
    { 108, 2, 0, 0, { 16, 0 } },
    { 110, 2, 0, 0, { 5, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_bridge_8018CE00[153] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, -112, 3000, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 56 } }, -128, -120, 3000, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 56 } }, 0, -120, 3000, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 40 } }, -56, -80, 2500, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -64, -72, 2500, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -80, 2875, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, -160, -120, 2000, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 32 } }, 8, -104, 2674, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 16, -72, 2822, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 104, 64 } }, 56, -120, 2750, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -80, -120, 2875, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -88, -112, 2750, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, -96, -104, 2500, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -104, -96, 2250, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, -72, 2500, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -56, -88, 2904, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 0, -88, 2904, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, -80, 2561, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -56, -72, 3108, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 72, 8 } }, -56, -64, 2249, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -56, -56, 2099, { .fields = { 0, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -32, -56, 2297, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 112, 1014, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -80, -32, 2425, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -96, 16, 1800, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 40, 1800, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -96, 112, 990, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, 24, -32, 2550, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, 40, 8, 1875, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, 80, 96, 1014, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 40 } }, -96, -80, 2025, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -80, -40, 2012, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 40 } }, 24, -88, 2200, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 32 } }, 24, -48, 2137, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 40 } }, -136, 56, 825, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 24 } }, -160, 96, 800, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 40 } }, 104, 40, 843, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 40 } }, 80, 80, 800, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 48 } }, -120, -40, 1400, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 40 } }, -96, 8, 1350, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 16 } }, -120, 8, 1350, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 56 } }, 48, -48, 1550, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, 48, 8, 1475, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 56 } }, -136, 56, 769, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 48 } }, 104, 40, 842, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 64, 8 } }, -40, -64, 2299, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 64, 8 } }, -40, -56, 2149, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 64, 8 } }, -40, -48, 2034, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 72, 8 } }, -40, -40, 1903, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 72, 8 } }, -40, -32, 1794, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 80, 8 } }, -40, -24, 1708, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 16 } }, -40, -16, 1586, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 88, 16 } }, -40, 0, 1450, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -48, 16, 1342, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, 8, 16, 1342, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -48, 32, 1239, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, 8, 32, 1235, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -48, 48, 1155, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, 8, 48, 1155, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -48, 64, 1062, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, 8, 64, 1062, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 72, 16 } }, -48, 80, 991, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 72, 16 } }, 24, 80, 994, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 80, 16 } }, -56, 96, 932, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 72, 16 } }, 24, 96, 935, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 80, 8 } }, -56, 112, 921, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 72, 8 } }, 24, 112, 924, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -40, -88, 2116, { .fields = { 0, 8 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 8, -88, 2681, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, -80, 2000, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -40, -72, 1831, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 16, -72, 2361, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -40, -64, 1700, { .fields = { 120, 8 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -40, -56, 1587, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 24, -56, 2089, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -40, -48, 1488, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -40, -40, 1401, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 32, -40, 1901, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -40, -32, 1323, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, 40, -24, 1753, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -48, 8, 1035, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -48, 16, 994, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -48, 24, 953, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -48, 32, 916, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -48, 40, 882, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -48, 48, 850, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -48, 56, 821, { .fields = { 112, 8 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -48, 64, 794, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, 80, 64, 1294, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, -48, -24, 1253, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, -48, -16, 1191, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, -48, -8, 1134, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -48, 0, 1082, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -56, 72, 768, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -56, 80, 725, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 8, -80, 2525, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 16, -64, 2214, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 24, -48, 1988, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, 32, -32, 1823, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 40, -16, 1691, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 40, -8, 1704, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 48, 0, 1582, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 48, 8, 1578, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 56, 16, 1492, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 56, 24, 1464, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 64, 32, 1416, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 64, 40, 1385, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 72, 48, 1347, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, 72, 56, 1321, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -40, -80, 2003, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -40, -72, 1831, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -40, -64, 1700, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -40, -56, 1587, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -40, -48, 1956, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -40, -40, 1549, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -40, -32, 1739, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -40, -24, 1449, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -40, -16, 1485, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, 48, -8, 1168, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -40, 0, 1374, { .fields = { 104, 8 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, 56, 8, 1078, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -40, 32, 1121, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -48, 80, 837, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -48, 88, 949, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, -48, -8, 1134, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, -48, 8, 1361, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, -48, 16, 992, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, -48, 24, 1081, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, -48, 40, 929, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, -48, 48, 958, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, -48, 56, 988, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, -48, 64, 1020, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, -48, 72, 1018, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, -48, 96, 900, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, -48, 104, 899, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 8, -88, 2250, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 16, -80, 2000, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 40 } }, 24, -56, 1750, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 32, -40, 1500, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 40, -24, 1250, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 48, 8, 1142, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, 40, 1044, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 16 } }, 64, 32, 969, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 80, 943, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 40 } }, 72, 56, 879, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 16 } }, 80, 56, 814, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, -32, -64, 2299, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -16, -48, 2299, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -40, -80, 2375, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, -24, -64, 2375, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 8, -88, 2375, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, -128, -96, 2987, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 24, 8 } }, -112, -64, 2987, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_bridge_8018D9F4[17] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 7, 0 } },
    { 3, 11, 0, 0, { 8, 0 } },
    { 14, 4, 0, 0, { 3, 0 } },
    { 18, 4, 0, 0, { 9, 0 } },
    { 22, 8, 0, 0, { 1, 0 } },
    { 30, 8, 0, 0, { 10, 0 } },
    { 38, 5, 0, 0, { 6, 0 } },
    { 43, 2, 0, 0, { 11, 0 } },
    { 45, 22, 0, 0, { 2, 0 } },
    { 67, 42, 0, 0, { 12, 0 } },
    { 109, 37, 0, 0, { 5, 0 } },
    { 146, 2, 0, 0, { 13, 0 } },
    { 148, 0, 0, 0, { 0, 0 } },
    { 148, 3, 0, 0, { 14, 0 } },
    { 151, 2, 0, 0, { 4, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_bridge_8018DA7C[112] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 104, 72, 713, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, -160, -40, 700, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 24 } }, -160, 32, 720, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 56 } }, -160, 56, 822, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 40 } }, 112, 16, 533, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 120, 56, 571, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 40 } }, 96, 80, 713, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 88, 104, 713, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 72 } }, -160, 48, 1175, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -40, 16, 1200, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 40, 24, 1125, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, 80, 104, 924, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -80, 16, 747, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -88, 24, 742, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 16 } }, -96, 32, 737, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, -16, 32, 737, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 16 } }, -112, 48, 730, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, -40, 48, 730, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, 16, 48, 730, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, -120, 64, 655, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, -56, 64, 685, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, 8, 64, 682, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 16 } }, -136, 80, 618, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, -64, 80, 618, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 16 } }, 0, 80, 620, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 24 } }, -152, 96, 573, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 24 } }, -80, 96, 570, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 24 } }, 0, 96, 574, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 64, -8, 581, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -160, 48, 462, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -104, -24, 646, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, -16, 652, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -96, 0, 688, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -88, 16, 730, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -88, 32, 747, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -112, -16, 611, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -120, -8, 587, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -128, 0, 561, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -136, 8, 538, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -144, 16, 518, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -136, 24, 545, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -128, 40, 580, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -120, 56, 618, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -112, 72, 635, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -152, 24, 495, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 16 } }, -160, 32, 483, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -160, 104, 498, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 64, 0, 561, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 64, 8, 537, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 64, 16, 515, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 64, 24, 495, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 32, 706, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 32, 468, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 64, 48, 436, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 48, 657, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 64, 64, 408, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 64, 80, 383, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 64, 96, 356, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 72, 626, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 88, 505, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 48, 104, 543, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -80, 32, 750, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -72, 64, 750, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -24, 80, 750, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 40 } }, -32, 40, 750, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, 24, 48, 750, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 32, 80, 750, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -72, 16, 825, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 16 } }, -32, 24, 825, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 16 } }, 16, 40, 825, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, -64, 16, 912, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -16, 24, 912, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, 24, 32, 912, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -56, 8, 1000, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -24, 16, 1000, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 24, 24, 1000, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -48, 8, 1100, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -8, 16, 1100, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 24, 24, 1100, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -80, -24, 950, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 56, -8, 590, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -72, -16, 975, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -64, -24, 1000, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 56, -8, 733, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -56, -24, 1025, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 56, -8, 821, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 48, 32, 920, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -48, -24, 1031, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 56, -8, 920, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -16, -24, 1474, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 88, 8 } }, -24, -16, 1448, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 88, 8 } }, -24, -8, 1387, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 96, 8 } }, -32, 0, 1327, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -40, 8, 1272, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, 16, 8, 1272, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 96, 8 } }, -32, 24, 1225, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -40, 16, 1225, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 8, 16, 1225, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 8, 32, 1225, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, 0, 1135, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -16, -48, 1428, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -24, -48, 1314, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -32, -40, 1482, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -40, -24, 1092, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 56, -40, 1231, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 56, -32, 1165, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 56, -24, 1106, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 56, -16, 1053, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 56, -8, 922, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 96, 96 } }, -16, -104, 1428, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 80, 8 } }, -16, -8, 1386, { .fields = { 8, 8 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 40, 8 } }, 24, 0, 1326, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_bridge_8018E33C[18] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 13, 0 } },
    { 8, 4, 0, 0, { 2, 0 } },
    { 12, 16, 0, 0, { 9, 0 } },
    { 28, 33, 0, 0, { 3, 0 } },
    { 61, 6, 0, 0, { 11, 0 } },
    { 67, 3, 0, 0, { 0, 0 } },
    { 70, 3, 0, 0, { 12, 0 } },
    { 73, 3, 0, 0, { 1, 0 } },
    { 76, 3, 0, 0, { 8, 0 } },
    { 79, 2, 0, 0, { 6, 0 } },
    { 81, 3, 0, 0, { 15, 0 } },
    { 84, 2, 0, 0, { 7, 0 } },
    { 86, 3, 0, 0, { 10, 0 } },
    { 89, 10, 0, 0, { 4, 0 } },
    { 99, 10, 0, 0, { 14, 0 } },
    { 109, 3, 0, 0, { 5, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_bridge_8018E3CC[142] = {
    { 143, 0x3FC0, { .fields = { 48, 88 } }, -160, -112, 1875, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, -112, -112, 1875, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, -72, -120, 1747, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -72, -96, 1747, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -72, -80, 1747, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 32 } }, -72, -56, 1909, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 16, -120, 1800, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 16, -104, 1825, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, 16, -88, 1843, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 8, -64, 1914, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, 40, -112, 1875, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 72 } }, 80, -112, 1875, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -48, 1893, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, -16, 1942, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, -8, 1962, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 0, 1982, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 8, 2003, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 16, 2024, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -112, -72, 1843, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -136, -104, 1577, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, -160, -88, 1267, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -88, -24, 1500, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 56 } }, -72, -32, 1500, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 64 } }, 32, -80, 1575, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 80, -80, 1624, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 40, -16, 1886, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, -16, 1806, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 64 } }, 112, -104, 1677, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 16, -64, 1867, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -80, -56, 1781, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 16, -56, 1781, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -80, -48, 1710, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 24, -8, 1499, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 24, 0, 1639, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -80, 8, 1591, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, -40, 1630, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, -32, 1572, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, -88, -24, 1423, { .fields = { 80, 8 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -88, -16, 1446, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -88, -8, 1513, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -88, 0, 1540, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 16, -48, 1702, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 16, -40, 1638, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 16, -32, 1734, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 16, -24, 1742, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 16, -16, 1765, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -8, -48, 2047, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, -48, -40, 2018, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -72, -32, 1921, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 104, 8 } }, -80, -24, 1863, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -80, -16, 1794, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -80, -8, 1730, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 112, 8 } }, -80, 0, 1675, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -80, 8, 1650, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 48 } }, -136, 16, 896, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 48 } }, -136, 64, 1148, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -80, 48, 1125, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -160, 40, 1125, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 40 } }, 72, 0, 1146, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 88, 40, 1148, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 56 } }, 64, 40, 1319, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 56 } }, 136, 0, 1128, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 32 } }, -120, 16, 912, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 32 } }, 72, 0, 994, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -48, 8, 1587, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -48, 16, 1587, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -48, 24, 1587, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 16, 16, 1600, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -48, 24, 1600, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -48, 32, 1600, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -24, 32, 1587, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, -40, 40, 1587, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -40, 48, 1587, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -40, 64, 1575, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -40, 48, 1575, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -40, 56, 1575, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 16, 56, 1587, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -40, 64, 1557, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -40, 72, 1519, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -40, 80, 1479, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -40, 88, 1442, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -40, 96, 1407, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -40, 104, 1369, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -40, 112, 1338, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -40, 56, 1505, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -48, 104, 1152, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 24, 48, 1745, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, 72, 1372, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 40, 96, 1368, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 32, 40, 1400, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -40, 48, 1350, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 32, 48, 1375, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -40, 56, 1325, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 32, 56, 1302, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -40, 64, 1300, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -40, 72, 1265, { .fields = { 72, 8 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 40, 80, 1257, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 40, 88, 1197, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 48, 112, 1139, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -48, 80, 1232, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -48, 88, 1200, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -48, 96, 1130, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -48, 104, 1141, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -48, 112, 1114, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 32, 64, 1325, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 32, 72, 1301, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 40, 96, 1195, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 40, 104, 1166, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 24, 16, 1563, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 16, 24, 1617, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 24, 24, 1600, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 24, 32, 1624, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -40, 40, 1581, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -40, 56, 1531, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -40, 64, 1453, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -48, 24, 1600, { .fields = { 56, 8 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -48, 32, 1587, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -48, 48, 1560, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 24, 40, 1476, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 24, 48, 1593, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 24, 56, 1595, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 24, -16, 1425, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -48, -8, 1418, { .fields = { 40, 8 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 24, -8, 1417, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 24, 0, 1412, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 24, 8, 1402, { .fields = { 48, 8 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 32, 40, 1375, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -40, 48, 1350, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -48, 0, 1411, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -48, 8, 1388, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -48, 16, 1388, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -48, 24, 1388, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -48, 32, 1378, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -48, 40, 1353, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 24, 16, 1406, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 24, 24, 1406, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 24, 32, 1390, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -128, 104, 1437, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 56 } }, -120, 64, 1437, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 64 } }, 56, 48, 1525, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, 64, 1525, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 32 } }, -160, -64, 1862, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_bridge_8018EEE4[19] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 6, 0 } },
    { 12, 16, 0, 0, { 9, 0 } },
    { 28, 18, 0, 0, { 2, 0 } },
    { 46, 8, 0, 0, { 10, 0 } },
    { 54, 8, 0, 0, { 0, 0 } },
    { 62, 2, 0, 0, { 11, 0 } },
    { 64, 3, 0, 0, { 1, 0 } },
    { 67, 3, 0, 0, { 12, 0 } },
    { 70, 3, 0, 0, { 8, 0 } },
    { 73, 3, 0, 0, { 13, 0 } },
    { 76, 8, 0, 0, { 4, 0 } },
    { 84, 5, 0, 0, { 14, 0 } },
    { 89, 19, 0, 0, { 3, 0 } },
    { 108, 13, 0, 0, { 15, 0 } },
    { 121, 16, 0, 0, { 7, 0 } },
    { 137, 4, 0, 0, { 16, 0 } },
    { 141, 1, 0, 0, { 5, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_bridge_8018EF7C[133] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, -104, 3000, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 56 } }, -128, -104, 3000, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 56 } }, -80, -104, 3000, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, -48, -120, 3000, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 0, -120, 3000, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 56 } }, 16, -104, 3000, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 48 } }, 48, -96, 3000, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 40 } }, -160, -120, 2987, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -128, -120, 2987, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 64 } }, -96, -104, 2987, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 32 } }, -72, -80, 2987, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 16 } }, -64, -64, 2575, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 32 } }, -56, -72, 2585, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, 8, -96, 2750, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 40 } }, 16, -72, 2750, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, 8, -32, 2946, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 64 } }, 56, -112, 2987, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 72 } }, 88, -120, 2987, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, -160, -88, 600, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 40 } }, -160, -64, 600, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 40 } }, -160, -24, 575, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 96 } }, -160, 16, 550, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 24 } }, -88, 56, 550, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 24, 8 } }, -160, 112, 550, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -64, -72, 2943, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, 0, -72, 2816, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 40 } }, 136, -72, 600, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 144, -32, 575, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 128, 72, 550, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 32 } }, 120, 8, 550, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 32 } }, 112, 40, 550, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 64, 8 } }, -56, -56, 2835, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 72, 8 } }, -56, -48, 2645, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 72 } }, 96, 48, 800, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 56 } }, -104, 64, 800, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 56, 24 } }, -160, 96, 800, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 64 } }, -104, -8, 1550, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 16 } }, -112, 8, 1550, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 56 } }, 48, -8, 1550, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 56 } }, -96, -96, 2000, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 24 } }, -80, -40, 2000, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 48 } }, 24, -96, 2000, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 40, 24 } }, 24, -48, 2000, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 72 } }, -136, -96, 1350, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 40 } }, -112, -24, 1350, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 80 } }, 56, -96, 1350, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 24 } }, 48, -16, 1350, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 32 } }, -32, -64, 2455, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 32 } }, 8, -64, 2453, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 56, 8 } }, -40, -40, 3281, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -32, -40, 2125, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, 8, -40, 2125, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -40, -16, 1625, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -40, -8, 1500, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -40, 0, 1325, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -40, 8, 1200, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -40, 16, 1100, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -40, 24, 1025, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -40, 32, 950, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -40, 40, 925, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 8, 8 } }, -40, 48, 875, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, -40, -32, 1875, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, -40, -24, 1750, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, -48, 56, 837, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, -48, 64, 777, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, -48, 72, 782, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -48, 80, 746, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -48, 88, 700, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 8, -32, 1875, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 16, -24, 1625, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 24, -16, 1525, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, 24, -8, 1375, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, 32, 0, 1225, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, 40, 8, 1150, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, 40, 16, 1075, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, 48, 24, 975, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, 56, 32, 925, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 24, 8 } }, 56, 40, 875, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, 64, 48, 850, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, 72, 56, 800, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, 80, 64, 775, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 8 } }, 80, 72, 750, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 16 } }, -32, -32, 2186, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, -16, 1758, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, -8, 1782, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 0, 1645, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 8, 1579, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 16, 1525, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 16 } }, -40, 24, 1061, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 16 } }, -40, 40, 1080, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -40, 56, 940, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, 80, 996, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -40, 80, 982, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 16 } }, -40, 104, 829, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, -32, 3008, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, -24, 1971, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 32 } }, 24, -16, 1684, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 40 } }, 32, 0, 1584, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 40 } }, 40, 8, 1493, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 48, 16, 1303, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 32 } }, 56, 32, 1131, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 64, 48, 997, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 80, 72, 863, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 104, 888, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -24, 2437, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -40, -16, 2189, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 24, 16 } }, -40, 0, 1837, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 24, 16 } }, -40, 16, 1583, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 24, 16 } }, -40, 32, 1400, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 24, 16 } }, -40, 48, 1256, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 32, 16 } }, -48, 64, 1114, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 32, 16 } }, -48, 80, 1018, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -48, 96, 916, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 16 } }, 8, -16, 2189, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 24, 16 } }, 8, 0, 1843, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 24, 16 } }, 16, 16, 1594, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 32, 16 } }, 24, 32, 1390, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 32, 16 } }, 32, 48, 1239, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 32, 16 } }, 40, 64, 1118, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 32, 16 } }, 48, 80, 1018, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, 48, 96, 916, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 16, 16 } }, -56, 80, 595, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 32, 16 } }, -72, 96, 483, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -80, 112, 440, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 72, 675, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 96, 64, 650, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 104, 72, 625, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 120, 72, 575, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 136, 72, 519, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -48, 96, 728, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 72, 112, 816, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, 88, 80, 778, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 104, 96, 692, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_bridge_8018F9E0[18] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 13, 0 } },
    { 7, 11, 0, 0, { 2, 0 } },
    { 18, 13, 0, 0, { 9, 0 } },
    { 31, 2, 0, 0, { 3, 0 } },
    { 33, 3, 0, 0, { 11, 0 } },
    { 36, 3, 0, 0, { 0, 0 } },
    { 39, 4, 0, 0, { 12, 0 } },
    { 43, 4, 0, 0, { 1, 0 } },
    { 47, 2, 0, 0, { 8, 0 } },
    { 49, 0, 0, 0, { 6, 0 } },
    { 49, 1, 0, 0, { 15, 0 } },
    { 50, 32, 0, 0, { 7, 0 } },
    { 82, 22, 0, 0, { 10, 0 } },
    { 104, 17, 0, 0, { 4, 0 } },
    { 121, 8, 0, 0, { 14, 0 } },
    { 129, 4, 0, 0, { 5, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_bridge_8018FA70[40] = {
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -32, 85, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -32, -32, 85, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -32, 85, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -32, 85, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -40, -32, 85, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -32, 85, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -32, 85, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -40, -32, 85, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -32, 85, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -32, 85, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -64, -32, 85, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -48, -32, 85, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -64, -32, 85, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -56, -32, 85, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -64, -32, 85, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -64, -32, 85, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -64, -32, 85, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -56, -32, 85, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -64, -32, 85, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -64, -32, 85, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, -32, 85, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -72, -32, 85, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, -32, 85, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, -32, 85, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -80, -32, 85, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, -32, 85, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -80, -32, 85, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -80, -32, 85, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -80, -32, 85, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -80, -32, 85, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, -24, 85, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, -24, 85, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, -24, 85, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, -80, -32, 85, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 72, 88 } }, -160, -64, 85, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 72, 88 } }, -8, -64, 85, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 80, 88 } }, -88, -64, 95, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 80, 96 } }, -160, 24, 84, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 80, 96 } }, -80, 24, 84, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 72, 96 } }, 0, 24, 84, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_bridge_8018FD90[37] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 14, 0 } },
    { 1, 1, 0, 0, { 18, 0 } },
    { 2, 1, 0, 0, { 0, 0 } },
    { 3, 1, 0, 0, { 19, 0 } },
    { 4, 1, 0, 0, { 1, 0 } },
    { 5, 1, 0, 0, { 20, 0 } },
    { 6, 1, 0, 0, { 5, 0 } },
    { 7, 1, 0, 0, { 21, 0 } },
    { 8, 1, 0, 0, { 13, 0 } },
    { 9, 1, 0, 0, { 22, 0 } },
    { 10, 1, 0, 0, { 4, 0 } },
    { 11, 1, 0, 0, { 23, 0 } },
    { 12, 1, 0, 0, { 7, 0 } },
    { 13, 1, 0, 0, { 24, 0 } },
    { 14, 1, 0, 0, { 17, 0 } },
    { 15, 1, 0, 0, { 25, 0 } },
    { 16, 1, 0, 0, { 3, 0 } },
    { 17, 1, 0, 0, { 26, 0 } },
    { 18, 1, 0, 0, { 9, 0 } },
    { 19, 1, 0, 0, { 27, 0 } },
    { 20, 1, 0, 0, { 16, 0 } },
    { 21, 1, 0, 0, { 28, 0 } },
    { 22, 1, 0, 0, { 6, 0 } },
    { 23, 1, 0, 0, { 29, 0 } },
    { 24, 1, 0, 0, { 8, 0 } },
    { 25, 1, 0, 0, { 30, 0 } },
    { 26, 1, 0, 0, { 2, 0 } },
    { 27, 1, 0, 0, { 31, 0 } },
    { 28, 1, 0, 0, { 10, 0 } },
    { 29, 1, 0, 0, { 32, 0 } },
    { 30, 1, 0, 0, { 12, 0 } },
    { 31, 1, 0, 0, { 33, 0 } },
    { 32, 1, 0, 0, { 15, 0 } },
    { 33, 1, 0, 0, { 34, 0 } },
    { 34, 6, 0, 0, { 11, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_bridge_8018FEB8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_bridge_8018FEC8[9] = {
    { 143, 0x3FC0, { .fields = { 64, 16 } }, 24, 104, 750, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -72, 56, 750, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 140, 0x3FC0, { .fields = { 136, 208 } }, -48, -104, 750, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 136, 208 } }, -48, -104, 750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 64, 16 } }, 24, 104, 750, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -72, 56, 750, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 141, 0x4040, { .fields = { 136, 208 } }, -48, -104, 750, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 24, 24 } }, -72, 56, 750, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 64, 16 } }, 24, 104, 750, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_bridge_8018FF7C[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, 3, 0, 0, { 2, 0 } },
    { 6, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_acropolis_bridge_8018FFA4[10] = {
    { { .empty = D_acropolis_bridge_8018BC10 }, D_acropolis_bridge_8018BC10, NULL },
    { { .elements = D_acropolis_bridge_8018BC20 }, D_acropolis_bridge_8018C440, NULL },
    { { .elements = D_acropolis_bridge_8018C4A8 }, D_acropolis_bridge_8018CD68, NULL },
    { { .elements = D_acropolis_bridge_8018CE00 }, D_acropolis_bridge_8018D9F4, NULL },
    { { .elements = D_acropolis_bridge_8018DA7C }, D_acropolis_bridge_8018E33C, NULL },
    { { .elements = D_acropolis_bridge_8018E3CC }, D_acropolis_bridge_8018EEE4, NULL },
    { { .elements = D_acropolis_bridge_8018EF7C }, D_acropolis_bridge_8018F9E0, NULL },
    { { .elements = D_acropolis_bridge_8018FA70 }, D_acropolis_bridge_8018FD90, NULL },
    { { .empty = D_acropolis_bridge_8018FEB8 }, D_acropolis_bridge_8018FEB8, NULL },
    { { .elements = D_acropolis_bridge_8018FEC8 }, D_acropolis_bridge_8018FF7C, NULL },
};

AreaResource D_acropolis_bridge_8019001C[2] = {
    { 41, 41, AREA_RESOURCE_FILE_GROUP_BASE_60, 0, { 0, 0 }, &D_acropolis_bridge_80191780.desc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_bridge_80190034[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_bridge_80190040[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_acropolis_bridge_8019004C[12] = {
    { NULL, NULL },
    { D_map_akropolis_8017BC7C, D_acropolis_bridge_8019001C },
    { D_map_akropolis_8017BCEC, D_acropolis_bridge_80190034 },
    { D_map_akropolis_8017BCFC, D_acropolis_bridge_80190040 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

/// Point lights shared by both bridge room entries in every view.
///
/// Overlay-owned writable records: room updates parent and compose the
/// transforms, and shading queries overwrite attenuation. Positions and
/// falloff radii use integer world units; RGB uses 12 fractional bits
/// (`ONE` is full intensity). Storage remains live while this overlay is loaded.
static WorldCoordPointLight _gAcropolisBridgePointLights[] = {
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1367, -507, 6799 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { ONE, 3686, 2457 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 6000,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -335, -2500, 3516 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 3686, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 0,
        .outer = 0x2B5C,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1970, -507, 8207 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { ONE, 3686, 2457 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 6000,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1342, -2000, 0x2854 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 3686, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 200,
        .outer = 9000,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -80, -2500, -3995 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 3686, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 0x3714,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2333, -2000, -763 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 3686, 3276, 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 200,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2333, -2000, -3192 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 3686, 3276, 2867 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 200,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6854, -1500, -3373 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 3686, 2457, 2457 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 3955,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -9907, -1500, -3373 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 3686, 2457, 2457 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 6000,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x324D, -1500, -3373 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 3686, 2457, 2457 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 6000,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x33A3, -1500, -719 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 3686, 2457, 2457 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 6000,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x2808, -1500, -719 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 3686, 2457, 2457 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 6000,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -7195, -1500, -719 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 3604, 2457, 2457 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 4000,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x38AB, 1600, -4070 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 2457, 3686, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 7500,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x2D1E, 1600, -4070 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 2457, 3686, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 7500,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5464, 1600, -4070 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 2457, 3686, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 5179,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -8422, 1600, -4070 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 2457, 3686, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 7500,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -8422, 1600, -22 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 2457, 3686, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 7500,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -5464, 1600, -22 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 2457, 3686, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 501,
        .outer = 3961,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x2D1E, 1600, -22 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 2457, 3686, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 7500,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x38AB, 1600, -22 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 2457, 3686, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 7500,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x3D7E, 2000, -3035 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { ONE, ONE, 3276 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 200,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x3D7E, 2000, -994 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { ONE, ONE, 3276 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 200,
        .outer = 3000,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -80, -2500, -8518 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 3686, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 0x3714,
    },
    {
        .head = {
            .transform  = { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4630, -1500, -3219 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } },
            .color      = { 3686, 2457, 2457 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 0,
        .outer = 2,
    },
};

WorldCoordRoomLights D_acropolis_bridge_80190A0C[1] = {
    { 0, NULL, ARRAY_SIZE(_gAcropolisBridgePointLights), _gAcropolisBridgePointLights, 0, NULL },
};

ViewCamera D_acropolis_bridge_80190A24[10] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 9107, 0x7530, 3098 } }, 499 },
    { { { { -990, 0, 3974 }, { 1381, 3840, 344 }, { -3726, 1423, -928 } }, { 9483, 1851, 1361 } }, 230 },
    { { { { -716, 0, -4032 }, { -1803, 3663, 320 }, { 3607, 1831, -640 } }, { 8500, 2876, 1406 } }, 230 },
    { { { { -388, 0, -4077 }, { -2430, 3288, 231 }, { 3273, 2441, -312 } }, { 0x3AE4, 4509, 1554 } }, 230 },
    { { { { -1039, 0, 3961 }, { 2408, 3252, 631 }, { -3146, 2489, -825 } }, { 0x2BF7, 3255, 1351 } }, 230 },
    { { { { -386, 0, -4077 }, { -2842, 2936, 269 }, { 2923, 2855, -276 } }, { 9497, 4691, 1750 } }, 230 },
    { { { { -431, 0, -4073 }, { -1488, 3812, 157 }, { 3791, 1496, -401 } }, { 0x3CF8, 1974, 1394 } }, 230 },
    { { { { 0, 0, 4096 }, { 3580, 1989, 0 }, { -1989, 3580, 0 } }, { 4620, 1300, 500 } }, 230 },
    { { { { 1369, 0, 3860 }, { 952, 3969, -337 }, { -3741, 1010, 1326 } }, { 1976, 2297, 3308 } }, 230 },
    { { { { -990, 0, 3974 }, { 1381, 3840, 344 }, { -3726, 1423, -928 } }, { 0x3552, 1930, 1439 } }, 230 },
};

PadScriptCmd D_acropolis_bridge_80190B8C[6] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 4), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, 85), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 2) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 3) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 2), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 5) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_acropolis_bridge_80190BA4[6] = {
    { 120, 110, 30, 1 },
    { 110, 150, 225, 1 },
    { 180, 180, 6, 1 },
    { 85, 55, 22, 1 },
    { 0, 0, 1, 0 },
    { 50, 45, 60, 1 },
};

PadScriptCmd D_acropolis_bridge_80190BBC[6] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, 3), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 2) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 3) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 4), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 3) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 14) }
};

PadScriptVibrationSegment D_acropolis_bridge_80190BD4[5] = {
    { 0, 0, 1, 0 },
    { 100, 200, 6, 1 },
    { 255, 255, 8, 1 },
    { 200, 100, 10, 1 },
    { 0, 0, 8, 0 },
};

WorldCollisionFootstepSounds D_acropolis_bridge_80190BE8 = {
    0x10000025,
    0x10000027,
    0x10000025,
};

WorldCollisionFootstepSounds D_acropolis_bridge_80190BF4 = {
    0x10000015,
    0x10000017,
    0x10000019,
};

s32 D_acropolis_bridge_80190C00[3] = {
    0x10000025,
    0x10000027,
    0x10000025,
};

WorldCollisionSurfaceProperties D_acropolis_bridge_80190C0C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_acropolis_bridge_80190C14[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_acropolis_bridge_80190C1C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_bridge_80190BE8 },
};

WorldCollisionSurfaceProperties D_acropolis_bridge_80190C24[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_bridge_80190BF4 },
};

WorldCollisionSurfaceProperties D_acropolis_bridge_80190C2C[1] = {
    { 1, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_SUPPRESS_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties* D_acropolis_bridge_80190C34[8] = {
    D_acropolis_bridge_80190C0C,
    D_acropolis_bridge_80190C14,
    D_acropolis_bridge_80190C1C,
    D_acropolis_bridge_80190C0C,
    D_acropolis_bridge_80190C24,
    D_acropolis_bridge_80190C0C,
    D_acropolis_bridge_80190C0C,
    D_acropolis_bridge_80190C2C,
};

DamageAttack D_acropolis_bridge_80190C54[2] = {
    { 18, 3 },
    { 0, 0 },
};

EnemyParams D_acropolis_bridge_80190C5C = { D_acropolis_bridge_80190C54, 80, 6, 36, 1, 100, 0, 100, 0 };

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

AnimationSet* D_acropolis_bridge_801915C8[7] = {
    NULL,
    &_gAcropolisBridgeAnimation13CD8,
    &_gAcropolisBridgeAnimation13DD8,
    &_gAcropolisBridgeAnimation13E48,
    &_gAcropolisBridgeAnimation13F60,
    &_gAcropolisBridgeAnimation13FE0,
    NULL,
};

s16 D_acropolis_bridge_801915E4[6][6] = {
    { 0, 0, 0, 0, 0, 0 },
    { 0, 0, 1, 8, 0, 0 },
    { 0, 1, 0, 8, 0, 0 },
    { 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0 },
};

BossStrangerNode D_acropolis_bridge_8019162C[20] = {
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

u8 D_acropolis_bridge_801916CC[12] = {
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

u8 D_acropolis_bridge_801916D8[8] = {
    8,
    0,
    1,
    6,
    0,
    0,
    0,
    0,
};

u8 D_acropolis_bridge_801916E0[8] = {
    3,
    2,
    5,
    9,
    11,
    4,
    OVERLAY_WALKER_ROUTE_END,
    0,
};

u8 D_acropolis_bridge_801916E8[8] = {
    0,
    7,
    10,
    14,
    OVERLAY_WALKER_ROUTE_END,
    0,
    0,
    0,
};

u8 D_acropolis_bridge_801916F0[8] = {
    4,
    8,
    9,
    6,
    7,
    OVERLAY_WALKER_ROUTE_END,
    0,
    0,
};

u8 D_acropolis_bridge_801916F8[12] = {
    19,
    15,
    14,
    17,
    16,
    12,
    13,
    18,
    OVERLAY_WALKER_ROUTE_END,
    0,
    0,
    0,
};

u8 D_acropolis_bridge_80191704[8] = {
    11,
    9,
    13,
    12,
    5,
    10,
    OVERLAY_WALKER_ROUTE_END,
    0,
};

u8 D_acropolis_bridge_8019170C[8] = {
    7,
    6,
    9,
    10,
    11,
    OVERLAY_WALKER_ROUTE_END,
    0,
    0,
};

u8 D_acropolis_bridge_80191714[4] = {
    14,
    13,
    6,
    OVERLAY_WALKER_ROUTE_END,
};

u8 D_acropolis_bridge_80191718[8] = {
    4,
    8,
    9,
    11,
    10,
    5,
    OVERLAY_WALKER_ROUTE_END,
    0,
};

u8* D_acropolis_bridge_80191720[9] = {
    D_acropolis_bridge_801916D8,
    D_acropolis_bridge_801916E0,
    D_acropolis_bridge_801916E8,
    D_acropolis_bridge_801916F0,
    D_acropolis_bridge_801916F8,
    D_acropolis_bridge_80191704,
    D_acropolis_bridge_8019170C,
    D_acropolis_bridge_80191714,
    D_acropolis_bridge_80191718,
};

TaskMessageEntry D_acropolis_bridge_80191744[3] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, func_acropolis_bridge_801856E0 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_acropolis_bridge_80187BD0 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

void (*D_acropolis_bridge_8019175C[9])(Task*) = {
    func_acropolis_bridge_80187D04,
    func_acropolis_bridge_80185F28,
    func_acropolis_bridge_801861A0,
    func_acropolis_bridge_801863A8,
    func_acropolis_bridge_80186618,
    func_acropolis_bridge_80187078,
    func_acropolis_bridge_80187310,
    func_acropolis_bridge_801874DC,
    func_acropolis_bridge_80186BBC,
};

_AcropolisBridgeEnemyTaskDescStorage D_acropolis_bridge_80191780 = { { { { TASK_BODY_TMD, 96 } }, func_acropolis_bridge_80187D80, { .model = &_gAcropolisBridgeModel13870 } }, { 0 } };

Task* D_acropolis_bridge_80191794 = NULL;

Task* D_acropolis_bridge_80191798 = NULL;

Task* D_acropolis_bridge_8019179C = NULL;

s32 D_acropolis_bridge_801917A0 = 0;

u16 D_acropolis_bridge_801917A4[2] = {
    0,
    0xF222,
};

s32 D_acropolis_bridge_801917A8;

DR_MOVE* D_acropolis_bridge_801917AC;

extern AnimationSet* D_acropolis_bridge_801915C8[7];

extern BossStrangerNode D_acropolis_bridge_8019162C[];

extern u8 D_acropolis_bridge_801916CC[];

extern u8* D_acropolis_bridge_80191720[];

static void            func_acropolis_bridge_8017EB4C(s32 state, s8 dx, s8 dy);
static __inline__ void walkerStep(BossStrangerWalker* walker, BossStrangerTickScratch* head,
                                  BossStrangerTickScratch* block);
static __inline__ void bridge_set_obj_pos(WorldCollisionBody* obj, SVECTOR3* pos);
static __inline__ void _acropolisBridgeInitWalkerScale(BossStrangerWalker* walker);
static __inline__ void _acropolisBridgeLightModel(Task* task, GfxCoord* coord);
static __inline__ void bridge_reset_scale_mtx_entry(_AcropolisBridgeEnemyWork* work);
static __inline__ void bridge_reset_scale_mtx_shrink(_AcropolisBridgeEnemyWork* work);
static __inline__ void bridge_scale_up(_AcropolisBridgeEnemyWork* work);
static __inline__ s16  _acropolisBridgeWasHit(Task* task);
static __inline__ s32  bridge_rec_kind1(WorldCollisionContact* recs);
static __inline__ void bridge_play_snd(Task* task, Enemy* enemy, s32 base);
static void            func_acropolis_bridge_801876A8(Task* task, u32 attackId);
static void            func_acropolis_bridge_80187C10(Task* task, s16 arg1);

/// Room message handler: answers msg 0xF (first use of the bridge) by running
/// the cutscene once and marking the area object, and msg 0xB by asking for
/// response 2 in the outgoing copy.
s32 func_acropolis_bridge_8017D6F4(Task* task, s32 msgId, RoomEventMsg* in, RoomEventMsg* out)
{
    GameLocationKey key;

    *out = *in;
    if (in->areaId == GAME_AREA_ACROPOLIS_FIRE_ESCAPE) {
        if (gameFlagGetNibble(GAME_FLAG_BRIDGE_ARRIVAL_SCENE_SEEN) == 0) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                gameFlagSetNibble(GAME_FLAG_BRIDGE_ARRIVAL_SCENE_SEEN, 1);
                func_800E8634(D_acropolis_bridge_80188EBC, 0, D_acropolis_bridge_8018912C);
                gameFlagSetNibble(GAME_FLAG_SANCTUARY_BLOCKER_CLEARED, 1);
                key.stage = GAME_STAGE_ACROPOLIS;
                key.area  = GAME_AREA_ACROPOLIS_SANCTUARY;
                areaSetPlacementVariant(&key, 3, AREA_VARIANT_RESET_ALWAYS);
            }
            return 2;
        }
    }
    if ((in->areaId == GAME_AREA_ACROPOLIS_PROMENADE) && (in->queryOnly == ROOM_EVENT_EXECUTE)) {
        out->room = 2;
    }
    return 1;
}

s32 func_acropolis_bridge_8017D7F0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Slot-7 handler for the "player used the bridge switch" message: with the
/// room's progress nibble already at 3 it just restarts cap slot 7, otherwise
/// it clears the script step and spawns entry 1 of the room task table.
s32 func_acropolis_bridge_8017D7F8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 7) {
        if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_BRIDGE_PROGRESS) == 3) {
            Gp_StartCapSlot(7, 1, 2);
            return 0;
        }
        func_acropolis_bridge_8017E60C(ACROPOLIS_BRIDGE_KEYPAD_CODE_BLANK, 1);
        taskSpawnFromTable(D_acropolis_bridge_80188E7C, 1, 0, 0);
    }
    return 0;
}

s32 func_acropolis_bridge_8017D868(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_acropolis_bridge_8017D870(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// State handlers of the room's own task.
static const TaskFuncTable3 D_acropolis_bridge_8017D5C4 = {
    { func_acropolis_bridge_8017D98C, func_acropolis_bridge_8017D9FC, taskKill }
};

/// State handlers of the bridge model task.
static const TaskFuncTable3 D_acropolis_bridge_8017D5D0 = {
    { bridgeModelSetup, func_acropolis_bridge_8017DB08, taskKill }
};

/// Three-state dispatcher of the bridge model task: setup, per-frame update,
/// then `taskKill`. The table is copied onto the stack before the call.
void func_acropolis_bridge_8017D878(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_bridge_8017D5D0;
    sp.funcs[task->state](task);
}

/// State handlers of the room's cutscene task.
static const TaskFuncTable14 D_acropolis_bridge_8017D5DC = {
    { func_acropolis_bridge_8017DB60, func_acropolis_bridge_8017DBA0, func_acropolis_bridge_8017DD88,
      func_acropolis_bridge_8017DC1C, func_acropolis_bridge_8017DC68, func_acropolis_bridge_8017DD24,
      func_acropolis_bridge_8017DD88, func_acropolis_bridge_8017DD88, func_acropolis_bridge_8017DD88,
      func_acropolis_bridge_8017DD88, func_acropolis_bridge_8017DD9C, func_acropolis_bridge_8017DDEC,
      func_acropolis_bridge_8017DE94, taskKill }
};

/// State handlers of the room's prompt script task.
static const TaskFuncTable9 D_acropolis_bridge_8017D614 = {
    { func_acropolis_bridge_8017E04C, func_acropolis_bridge_8017F404, func_acropolis_bridge_8017E1D0,
      func_acropolis_bridge_8017F460, func_acropolis_bridge_8017F4CC, func_acropolis_bridge_8017F544,
      func_acropolis_bridge_8017E3A0, func_acropolis_bridge_8017E4FC, func_acropolis_bridge_8017F658 }
};

/// Fourteen-state dispatcher of the room's cutscene task: copies the handler
/// table onto the stack and calls the entry named by `Task::state`.
void func_acropolis_bridge_8017D8D0(Task* task)
{
    TaskFuncTable14 states;

    states = D_acropolis_bridge_8017D5DC;
    states.funcs[task->state](task);
}

void func_acropolis_bridge_8017D954(void)
{
    Gp_PulseState1C();
    Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
}

static void func_acropolis_bridge_8017D98C(Task* arg0)
{
    arg0->msgTable = D_acropolis_bridge_80188E4C;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    D_acropolis_bridge_80191794 = taskSpawnFromTable(D_acropolis_bridge_80188E7C, 0, 0, 0);
    arg0->state                 = (s32)(arg0->state + 1);
    func_acropolis_bridge_8017F2D0(gameFlagGetNibble(GAME_FLAG_BRIDGE_ARRIVAL_SCENE_SEEN) & 0xFF);
}

static void func_acropolis_bridge_8017D9FC(Task* task)
{
    char pad[0x10];
}

/// Three-state dispatcher of the room's own task: setup, an empty idle state,
/// then `taskKill`. The table is copied onto the stack before the call.
void func_acropolis_bridge_8017DA0C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_bridge_8017D5C4;
    sp.funcs[task->state](task);
}

#include "../../shared/bridge_model_setup.inc.c"

/// Per-frame state of the bridge model task: raises bit 0x80 of the object's
/// flags on camera views 8..10 and clears them elsewhere, then clears the root
/// coordinate's `composeStamp` so its world matrix is rebuilt this frame.
static void func_acropolis_bridge_8017DB08(Task* task)
{
    TmdObject* extra;
    GfxCoord*  coord;

    extra = task->extra.tmd;
    coord = extra->coords;
    if ((u32)(Gp_GetViewIndex() - 8) < 3U) {
        extra->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        extra->flags = 0;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

static void func_acropolis_bridge_8017DB60(Task* arg0)
{
    Gp_StartCapSlot(7, 1, 1);
    arg0->state = (s32)(arg0->state + 1);
}

static void func_acropolis_bridge_8017DBA0(Task* arg0)
{
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_IS_BUSY, 0, 0) == 0) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 8;
        gGameSession->hideHud                                      = 1;
        Gp_MsgPlayer3F3(0);
        Gp_MsgPlayerWeapon(0);
        arg0->state = (s32)(arg0->state + 1);
    }
}

static void func_acropolis_bridge_8017DC1C(Task* arg0)
{
    Task* temp_v0;

    temp_v0                     = Task_Spawn(2, 8, 0, 0);
    arg0->state                 = (s32)(arg0->state + 1);
    D_acropolis_bridge_80191798 = temp_v0;
}

static void func_acropolis_bridge_8017DC68(Task* arg0)
{
    ActorCommand msg = { { { 1, 0xB } }, 1 };

    if (Task_PollKill(D_acropolis_bridge_80191798, &D_acropolis_bridge_801917A0) != 0) {
        if (D_acropolis_bridge_801917A0 == 0) {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 6;
            gGameSession->hideHud                                      = 0;
            arg0->state                                                = (s32)(arg0->state + 1);
        } else {
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 9;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
            arg0->state = (s32)(arg0->state + 1);
        }
    }
}

static void func_acropolis_bridge_8017DD24(Task* arg0)
{
    if (D_acropolis_bridge_801917A0 == 0) {
        Gp_MsgPlayerWeapon(1);
        Gp_MsgPlayer3F3(1);
        taskKill(arg0);
        return;
    }
    Gp_MsgPlayer3F3(1);
    arg0->state += 1;
}

static void func_acropolis_bridge_8017DD88(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

static void func_acropolis_bridge_8017DD9C(Task* arg0)
{
    Task* task = taskSpawnFromTable(&D_acropolis_bridge_80189234, 0, 0, 0);
    s32   next = arg0->state + 1;

    D_acropolis_bridge_8019179C = task;
    arg0->state                 = next;
}

static void func_acropolis_bridge_8017DDEC(Task* arg0)
{
    s32 unused[2]; // never read; the target still reserves sp+0x10..sp+0x18 for it
    s32 killed;

    if (Task_PollKill(D_acropolis_bridge_8019179C, &killed) != 0) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), 0x7DA, 1, 0x7D5);
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 6;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 2;
        gGameSession->location.loc.room                            = 2;
        gGameSession->roomObjsDirty                                = 1;
        gameFlagSetNibble(GAME_FLAG_ACROPOLIS_BRIDGE_PROGRESS, 3);
        Gp_MsgPlayerWeapon(1);
        arg0->state = arg0->state + 1;
    }
}

static void func_acropolis_bridge_8017DE94(Task* arg0)
{
    func_acropolis_bridge_8017F2D0(gameFlagGetNibble(GAME_FLAG_BRIDGE_ARRIVAL_SCENE_SEEN) & 0xFF);
    gGameSession->hideHud = 0;
    arg0->state           = (s32)(arg0->state + 1);
}

void func_acropolis_bridge_8017DEE4(Task* arg0)
{
    u8          slotParam[4];
    CdCmdQueue* queue;
    Task*       task;
    s16         count;

    task  = arg0;
    queue = &gCdCmdQueue;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
    }
    goto tail;

L_case0:
    queue->movieFrame = 1;
    slotParam[0]      = Stream_FindSlot((u8*)&gGameSession->location.loc, 0, 0);
    CdCmd_Enqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
    goto advance;

L_case1:
    if (queue->movieReady == 0) {
        goto tail;
    }
    taskReparent(task, Gp_SpawnScript18(D_acropolis_bridge_80190B8C, D_acropolis_bridge_80190BA4));
    goto advance;

L_case2:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        goto tail;
    }
advance:
    task->state = task->state + 1;
    goto tail;

L_case3:
    count               = task->killCountdown + 1;
    task->killCountdown = count;
    if (count >= 0x1F) {
        Task_RequestKill(task, 0);
        return;
    }
tail:
    D_acropolis_bridge_801917A4[0] = queue->movieFrame;
}

static s16 func_acropolis_bridge_8017E024(void)
{
    return D_acropolis_bridge_80189240[D_acropolis_bridge_801917A4[0] + 1].vy;
}

/// Brings the bridge's action-prompt script online: allocates its
/// `_AcropolisBridgeKeypadWork` block, spawns the prompt task it drives,
/// blanks the entered code, raises the "bridge is up" sprite command of the
/// camera the player is on, and clears every hotspot's hit flag so the first
/// hit test starts clean. A failed allocation kills the task instead.
static void func_acropolis_bridge_8017E04C(Task* task)
{
    _AcropolisBridgeKeypadWork* work;
    GameLocationKey*            sess;
    ActionPromptHotspot*        hs;
    SpriteView*                 rec;
    s32                         view;

    work = memCalloc(sizeof(_AcropolisBridgeKeypadWork), 0);
    if (work == NULL) {
        Task_RequestKill(task, 0);
        return;
    }
    task->spawnArg2.pointer = taskSpawnFromTable(&D_acropolis_bridge_80189830, 0, 1, 0);
    task->work              = work;
    work->field_0           = 0x14;
    work->code              = ACROPOLIS_BRIDGE_KEYPAD_CODE_BLANK;
    sess                    = &gGameSession->location.loc;
    task->state++;
    view                                 = Gp_GetViewIndex();
    rec                                  = Gp_SprtTables[sess->stage - 1][gGameSession->spriteVariant - 1].areaViews[sess->area - 1];
    rec[(u8)view - 1].batches[35].hidden = 1;
    gGameSession->cutsceneHold           = 1;
    Gp_MsgPlayer3F3(0);
    Display_AcquireRef();
    gGameSession->eventState = 1;
    gGameSession->hideHud    = 1;
    for (hs = D_acropolis_bridge_8018983C; hs->id != ACTION_PROMPT_HOTSPOT_END; hs++) {
        hs->hit = 0;
    }
    D_acropolis_bridge_801917A8 = 0;
    func_acropolis_bridge_8017E60C(work->code, 0);
}

/// Runs one frame of the bridge's action prompt while the player is entering a
/// code: the cursor is hit-tested against the room's hotspot table, and a
/// confirm press on a hit hotspot either latches that hotspot in `selectedKey`
/// and `promptKind` and goes to state 3 (while `keypadExamined` is clear) or
/// shifts its id into `code`'s low nibble and beeps. The clear key blanks
/// `code` instead. A complete entry ends the script in state 5, a cancel press
/// ends it in state 8, and a busy cap suspends the whole scan for that frame.
static void func_acropolis_bridge_8017E1D0(Task* task)
{
    _AcropolisBridgeKeypadWork* work   = task->work;
    ActionPromptHotspot*        hs     = D_acropolis_bridge_8018983C;
    ActionPrompt*               prompt = D_80114D28;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    } else {
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
        if (actionPromptHitTest(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
            prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
            if (prompt->buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED) {
                while (hs->id != ACTION_PROMPT_HOTSPOT_END) {
                    if (hs->hit != 0) {
                        if (work->keypadExamined == 0) {
                            prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                            prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                            work->selectedKey   = hs->id;
                            work->promptKind    = hs->promptKind;
                            task->state         = 3;
                            return;
                        }
                        if (hs->id == ACROPOLIS_BRIDGE_KEYPAD_KEY_CLEAR) {
                            work->code       = ACROPOLIS_BRIDGE_KEYPAD_CODE_BLANK;
                            work->digitCount = 0;
                        } else {
                            work->code <<= 4;
                            work->code   = (work->code & 0xFF0) | hs->id;
                            work->digitCount++;
                        }
                        sndEvtRequestScriptStart(SOUND_ACROPOLIS_BRIDGE_KEYPAD_BEEP, 0, 0);
                        break;
                    }
                    hs++;
                }
            }
        } else {
            prompt->mode = ACTION_PROMPT_MODE_IDLE;
        }
        if (work->digitCount == ACROPOLIS_BRIDGE_KEYPAD_CODE_DIGITS) {
            task->state = 5;
            work->timer = 0;
        }
        if (prompt->buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED) {
            task->state                 = 8;
            D_acropolis_bridge_801917A8 = 0;
        }
    }
    func_acropolis_bridge_8017E60C(work->code, 0);
}

/// Winds the bridge prompt back down, the mirror of
/// `func_acropolis_bridge_8017E04C`: it clears the "bridge is up" sprite
/// command of the camera the player is on, then runs the same twenty-frame
/// pass as `func_acropolis_bridge_8017E4FC` - the first ten frames blank the
/// code display, the next ten show the entered `code`, and the frame after
/// them resets `timer` and counts one blink in `blinkCount`.
/// The cursor is hit-tested against the room's hotspot table either way so
/// `mode` reports whether it sits over one, and the third blink ends the script
/// in state 8 with `D_acropolis_bridge_801917A8` raised.
static void func_acropolis_bridge_8017E3A0(Task* task)
{
    ActionPrompt*               prompt = D_80114D28;
    ActionPromptHotspot*        hs     = D_acropolis_bridge_8018983C;
    _AcropolisBridgeKeypadWork* work   = task->work;
    GameLocationKey*            sess   = &gGameSession->location.loc;
    SpriteView*                 rec;
    s32                         view;
    s16                         tick;
    s32                         step;

    view                                 = Gp_GetViewIndex();
    rec                                  = Gp_SprtTables[sess->stage - 1][gGameSession->spriteVariant - 1].areaViews[sess->area - 1];
    rec[(u8)view - 1].batches[35].hidden = 0;

    tick = work->timer;
    step = ACROPOLIS_BRIDGE_KEYPAD_CODE_BLANK;
    if (tick >= ACROPOLIS_BRIDGE_KEYPAD_BLINK_PHASE_FRAMES) {
        if (tick >= 2 * ACROPOLIS_BRIDGE_KEYPAD_BLINK_PHASE_FRAMES) {
            goto reset;
        }
        step = work->code;
    }
    func_acropolis_bridge_8017E60C(step, 0);
    work->timer++;
    goto after;

reset:
    work->timer = 0;
    work->blinkCount++;

after:
    if (actionPromptHitTest(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
    } else {
        prompt->mode = ACTION_PROMPT_MODE_IDLE;
    }

    if (work->blinkCount == ACROPOLIS_BRIDGE_KEYPAD_BLINK_COUNT) {
        task->state                 = 8;
        D_acropolis_bridge_801917A8 = 1;
    }
}

/// Idles the bridge prompt for twenty frames per pass: the first ten frames
/// keep the prompt task ticking through `func_acropolis_bridge_8017E81C`, the
/// next ten blank the code display, and the frame after them counts one blink
/// in `blinkCount`. Either way the cursor is re-hit-tested against the room's
/// hotspot table so `mode` reports whether it sits over one. After three
/// blinks the script rewinds to state 2 with `code` blank for another entry,
/// and once `rejectedCount` reaches its limit it gives up into state 8.
static void func_acropolis_bridge_8017E4FC(Task* task)
{
    ActionPrompt*               prompt = D_80114D28;
    ActionPromptHotspot*        hs     = D_acropolis_bridge_8018983C;
    _AcropolisBridgeKeypadWork* work   = task->work;
    s16                         tick;
    u8                          retry;

    Gp_GetViewIndex();
    tick = work->timer;
    if (tick < ACROPOLIS_BRIDGE_KEYPAD_BLINK_PHASE_FRAMES) {
        func_acropolis_bridge_8017E81C();
        work->timer++;
    } else if (tick < 2 * ACROPOLIS_BRIDGE_KEYPAD_BLINK_PHASE_FRAMES) {
        func_acropolis_bridge_8017E60C(ACROPOLIS_BRIDGE_KEYPAD_CODE_BLANK, 0);
        work->timer++;
    } else {
        work->timer = 0;
        work->blinkCount++;
    }

    if (actionPromptHitTest(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
    } else {
        prompt->mode = ACTION_PROMPT_MODE_IDLE;
    }

    if (work->blinkCount == ACROPOLIS_BRIDGE_KEYPAD_BLINK_COUNT) {
        task->state         = 2;
        work->digitCount    = 0;
        work->code          = ACROPOLIS_BRIDGE_KEYPAD_CODE_BLANK;
        retry               = work->rejectedCount + 1;
        work->rejectedCount = retry;
        if (retry >= ACROPOLIS_BRIDGE_KEYPAD_REJECTED_LIMIT) {
            task->state                 = 8;
            D_acropolis_bridge_801917A8 = 0;
        }
    }
}

/// Draws the three-digit bridge code onto the current room's eighth SPRT
/// record. Each nibble of `digits` indexes one row of
/// `D_acropolis_bridge_801898CC`, which maps it to the single command left
/// drawing in that digit's band - commands 1..10, 11..20 and 21..30 - while
/// every other command in the band gets its skip-OT-link flag set. A nibble
/// above 9 maps to the row's sentinel (0x1F / 0x20 / 0x21), which blanks the
/// band and shows the placeholder at command 31, 32 or 33 instead, so
/// `func_acropolis_bridge_8017E60C(0xFFF, 0)` clears the whole display.
/// Command 34 is always hidden; `hidePrompt` also hides command 35.
static void func_acropolis_bridge_8017E60C(s32 digits, s32 hidePrompt)
{
    GameLocationKey* sess = &gGameSession->location.loc;
    SpriteBatch*     batches;
    s32              i;
    u8               hi;
    u8               mid;
    u8               lo;

    Gp_GetViewIndex();
    batches = Gp_SprtTables[sess->stage - 1][gGameSession->spriteVariant - 1].areaViews[sess->area - 1][7].batches;

    if ((s16)hidePrompt != 0) {
        batches[35].hidden = 1;
    }

    hi  = D_acropolis_bridge_801898CC[0][((u32)digits & 0xF00) >> 8];
    mid = D_acropolis_bridge_801898CC[1][((u32)digits & 0xF0) >> 4];
    lo  = D_acropolis_bridge_801898CC[2][digits & 0xF];

    if (hi == 0x21) {
        for (i = 0x15; i < 0x1F; i++) {
            batches[i].hidden = 1;
        }
        batches[33].hidden = 0;
    } else {
        for (i = 0x15; i < 0x1F; i++) {
            if (hi == i) {
                batches[i].hidden  = 0;
                batches[33].hidden = 1;
            } else {
                batches[i].hidden = 1;
            }
        }
    }

    if (mid == 0x20) {
        for (i = 0xB; i < 0x15; i++) {
            batches[i].hidden = 1;
        }
        batches[32].hidden = 0;
    } else {
        for (i = 0xB; i < 0x15; i++) {
            if (mid == i) {
                batches[i].hidden  = 0;
                batches[32].hidden = 1;
            } else {
                batches[i].hidden = 1;
            }
        }
    }

    if (lo == 0x1F) {
        for (i = 1; i < 0xB; i++) {
            batches[i].hidden = 1;
        }
        batches[31].hidden = 0;
    } else {
        for (i = 1; i < 0xB; i++) {
            if (lo == i) {
                batches[i].hidden  = 0;
                batches[31].hidden = 1;
            } else {
                batches[i].hidden = 1;
            }
        }
    }

    batches[34].hidden = 1;
}

/// Shows one frame of the bridge prompt: in the current room's eighth SPRT
/// record, every command from 1 to 33 gets its skip-OT-link flag set and only
/// command 34 is left drawing. `func_acropolis_bridge_8017E4FC` calls this on
/// each of the first ten frames of a pass.
static void func_acropolis_bridge_8017E81C(void)
{
    GameLocationKey* sess = &gGameSession->location.loc;
    SpriteBatch*     batches;
    s32              i;

    Gp_GetViewIndex();
    batches = Gp_SprtTables[sess->stage - 1][gGameSession->spriteVariant - 1].areaViews[sess->area - 1][7].batches;

    for (i = 0x15; i < 0x1F; i++) {
        batches[i].hidden = 1;
    }
    for (i = 0xB; i < 0x15; i++) {
        batches[i].hidden = 1;
    }
    for (i = 1; i < 0xB; i++) {
        batches[i].hidden = 1;
    }
    batches[34].hidden = 0;
    batches[33].hidden = 1;
    batches[32].hidden = 1;
    batches[31].hidden = 1;
}

#include "../../shared/action_prompt_outline_rect.inc.c"

/// Slides one of three mutually exclusive bridge sprites in view 9 by
/// `(dx, dy)` and makes it the visible one. Each state owns three consecutive
/// `SpriteSource` entries, which move together, and one of the three
/// `SpriteBatch` slots; `Gp_LinkViewSprts` treats a nonzero `hidden` as "skip
/// OT-linking", so the selected command gets 0 and the other two get 1. A
/// state outside 0..2 moves nothing and hides all three.
static void func_acropolis_bridge_8017EB4C(s32 state, s8 dx, s8 dy)
{
    GameSession*     g    = gGameSession;
    GameLocationKey* sess = &g->location.loc;
    SpriteView*      rec;
    SpriteSource*    sources;
    SpriteBatch*     batches;
    s32              mode;

    rec     = Gp_SprtTables[sess->stage - 1][g->spriteVariant - 1].areaViews[sess->area - 1];
    batches = rec[9].batches;
    sources = rec[9].sources.elements;
    mode    = state & 0xFF;

    if (mode == 0) {
        sources[0].x0    += dx;
        sources[0].y0    += dy;
        sources[1].x0    += dx;
        sources[1].y0    += dy;
        sources[2].x0    += dx;
        sources[2].y0    += dy;
        batches[1].hidden = 0;
        batches[2].hidden = 1;
        batches[3].hidden = 1;
    } else if (mode == 1) {
        sources[3].x0    += dx;
        sources[3].y0    += dy;
        sources[4].x0    += dx;
        sources[4].y0    += dy;
        sources[5].x0    += dx;
        sources[5].y0    += dy;
        batches[1].hidden = 1;
        batches[2].hidden = 0;
        batches[3].hidden = 1;
    } else if (mode == 2) {
        sources[6].x0    += dx;
        sources[6].y0    += dy;
        sources[7].x0    += dx;
        sources[7].y0    += dy;
        sources[8].x0    += dx;
        sources[8].y0    += dy;
        batches[1].hidden = 1;
        batches[2].hidden = 1;
        batches[3].hidden = 0;
    } else {
        batches[1].hidden = 1;
        batches[2].hidden = 1;
        batches[3].hidden = 1;
    }
}

#include "../../shared/action_prompt_move_cursors.inc.c"

#include "../../shared/action_prompt_draw_cursor.inc.c"

/// Two-state dispatcher of the room's prompt script task: state 0 resets the
/// action prompts, state 1 moves and draws their cursors.
void func_acropolis_bridge_8017F280(Task* task)
{
    TaskFunc states[2] = { actionPromptReset, actionPromptMoveCursors };

    states[task->state](task);
}

/// Repaints the two bridge sprites that game flag nibble 0x10 governs: one
/// sprite command in view 2 of this room's sprite record and one in view 5.
/// `Gp_LinkViewSprts` reads `field_4` to decide whether to skip OT-linking a
/// command's prims, so a zero nibble draws both and a non-zero one hides them.
void func_acropolis_bridge_8017F2D0(s32 flags)
{
    GameSession*     g    = gGameSession;
    GameLocationKey* sess = &g->location.loc;
    SpriteView*      rec;
    SpriteBatch*     batches;

    rec = Gp_SprtTables[sess->stage - 1][g->spriteVariant - 1].areaViews[sess->area - 1];

    batches = rec[1].batches;
    if ((flags & 0xFF) == 0) {
        batches[11].hidden = 0;
    } else {
        batches[11].hidden = 1;
    }

    batches = rec[4].batches;
    if ((flags & 0xFF) == 0) {
        batches[16].hidden = 0;
    } else {
        batches[16].hidden = 1;
    }
}

/// Picks which of three mutually exclusive bridge sprites view 9 of this room
/// draws. `Gp_LinkViewSprts` treats a nonzero `field_4` as "skip OT-linking",
/// so the selected command gets 0 and the other two get 1; a state outside
/// 0..2 hides all three.
void func_acropolis_bridge_8017F358(s32 state)
{
    GameSession*     g    = gGameSession;
    GameLocationKey* sess = &g->location.loc;
    SpriteView*      rec;
    SpriteBatch*     batches;
    s32              mode;

    rec     = Gp_SprtTables[sess->stage - 1][g->spriteVariant - 1].areaViews[sess->area - 1];
    batches = rec[9].batches;
    mode    = state & 0xFF;

    if (mode == 0) {
        batches[1].hidden = 0;
        batches[2].hidden = 1;
        batches[3].hidden = 1;
    } else if (mode == 1) {
        batches[1].hidden = 1;
        batches[2].hidden = 0;
        batches[3].hidden = 1;
    } else if (mode == 2) {
        batches[1].hidden = 1;
        batches[2].hidden = 1;
        batches[3].hidden = 0;
    } else {
        batches[1].hidden = 1;
        batches[2].hidden = 1;
        batches[3].hidden = 1;
    }
}

/// Arms the action prompt for a fresh script step: parks the cursor at the top
/// left with the highlight mode on and the cursor speed at 0x80, tears down any
/// prompt still up, then advances the task to its next state.
static void func_acropolis_bridge_8017F404(Task* task)
{
    ActionPrompt* prompt = D_80114D28;

    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    prompt->mode        = ACTION_PROMPT_MODE_IDLE;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    func_acropolis_bridge_8017E60C(ACROPOLIS_BRIDGE_KEYPAD_CODE_BLANK, 0);
    task->state++;
}

/// Opens the command prompt for the key latched in the work block: redraws the
/// entered `code`, hides the cursor, then spawns the prompt at the coordinates
/// the gameplay side left in `D_80114D28` with the key's `promptKind` as its
/// display mode, and advances the task to state 4.
static void func_acropolis_bridge_8017F460(Task* task)
{
    ActionPrompt*               prompt = D_80114D28;
    _AcropolisBridgeKeypadWork* work   = task->work;

    func_acropolis_bridge_8017E60C(work->code, 0);
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

/// Collects the answer to the command prompt opened for the latched key:
/// redraws the entered `code`, keeps the cursor hidden, and advances the task
/// to state 2. If `func_800D4EC0` reports the command was confirmed,
/// `keypadExamined` is raised (which the hotspot scan in
/// `func_acropolis_bridge_8017E1D0` gates on, so keys type from then on) and
/// cap slot 9 is started.
static void func_acropolis_bridge_8017F4CC(Task* task)
{
    ActionPrompt*               prompt = D_80114D28;
    _AcropolisBridgeKeypadWork* work   = task->work;

    func_acropolis_bridge_8017E60C(work->code, 0);
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    if (func_800D4EC0() != 0) {
        work->keypadExamined = 1;
        Gp_StartCapSlot(9, 0, 0);
    }
    task->state = 2;
}

/// Holds a complete entry on the display for ten frames, then checks it. The
/// correct code is the one the room answers with message 0x7DA before its
/// confirmation sound and state 6; every other entry just clears `blinkCount`
/// and `timer`, plays the rejection sound and goes to state 7. Either way the
/// entered `code` is redrawn and the cursor is re-hit-tested against the
/// room's hotspot table, so `mode` reports whether it ended up over one.
static void func_acropolis_bridge_8017F544(Task* task)
{
    ActionPrompt*               prompt = D_80114D28;
    _AcropolisBridgeKeypadWork* work   = task->work;
    ActionPromptHotspot*        hs     = D_acropolis_bridge_8018983C;

    if (work->timer < ACROPOLIS_BRIDGE_KEYPAD_CHECK_DELAY_FRAMES) {
        work->timer++;
        return;
    }

    if (work->code != ACROPOLIS_BRIDGE_KEYPAD_CODE_CORRECT) {
        sndEvtRequestScriptStart(SOUND_ACROPOLIS_BRIDGE_CODE_REJECTED, 0, 0);
        work->blinkCount = 0;
        work->timer      = 0;
        task->state      = 7;
    } else {
        ActorCommand msg = { { { 1, 0xE } }, 2 };

        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
        sndEvtRequestScriptStart(SOUND_ACROPOLIS_BRIDGE_CODE_ACCEPTED, 0, 0);
        task->state = 6;
    }
    func_acropolis_bridge_8017E60C(work->code, 0);
    if (actionPromptHitTest(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
    } else {
        prompt->mode = ACTION_PROMPT_MODE_IDLE;
    }
}

static void func_acropolis_bridge_8017F658(Task* task)
{
    Display_ReleaseRef();
    func_acropolis_bridge_8017E60C(ACROPOLIS_BRIDGE_KEYPAD_CODE_BLANK, 0);
    taskKill(task->spawnArg2.pointer);
    Task_RequestKill(task, D_acropolis_bridge_801917A8);
    gGameSession->eventState   = 0;
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    D_80114D08                 = 0xA;
}

#include "../../shared/action_prompt_hit_test.inc.c"

/// Nine-state dispatcher of this room's script task: copies the handler table
/// out of the overlay's rodata onto the stack and tails into the entry named by
/// `Task::state`.
void func_acropolis_bridge_8017F788(Task* task)
{
    TaskFuncTable9 states;

    states = D_acropolis_bridge_8017D614;
    states.funcs[task->state](task);
}

#include "../../shared/action_prompt_reset.inc.c"

/// Per-frame driver for the bridge's ambient effect field, and the room's
/// message-table owner. On the first frame it publishes
/// `D_acropolis_bridge_801898FC` as slot 5's `taskMessageDispatch` table and seeds
/// `D_acropolis_bridge_80189A34` with the two tracked cable joints' world
/// positions.
///
/// Each frame it re-spawns the effects the current camera can see: the two
/// per-view bitmask tables (`D_acropolis_bridge_801899EC` /
/// `D_acropolis_bridge_80189A1C`) say which of the placed emitters are visible
/// from view `Gp_GetViewIndex()`, and each visible entry spawns its dust
/// (0x600B1 / 0x600B2) or spark (0x600B3) at the matching `SVECTOR`. View 9
/// lifts the dust 0x240 above the placed point.
///
/// On views 2, 5 and 6 (`bit & 0x62`) it also trails debris off the two moving
/// joints: `field_26` is the Manhattan distance the joint travelled since last
/// frame, biased by 0x20, and two `gRandomLcgState` rolls against that distance
/// decide whether this frame emits `gRoomEffectWaterRippleId` / `gRoomEffectWaterSprayId`. The joint's
/// new position is written back for the next frame's delta.
///
/// `D_acropolis_bridge_801899FC` finally maps the view onto one of five
/// looping ambience effects (0x600B4..0x600B8): entering the view bursts 30
/// copies at once, and staying in it emits one per frame - one in two while
/// `gRoomEffectState->battleState` says no battle is engaged, one in three while one
/// is, so that the ambience thins out during a fight.
void func_acropolis_bridge_8017F868(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    GfxCoord*   part;
    Task*       owner;
    SVECTOR     pos;
    u8          view;
    s32         bit;
    s32         i;
    s32         delta;
    s32         axis;
    s32         dist;
    s32         prev;
    s16         lastView;
    u16         rnd;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    owner = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    part  = owner->extra.tmd->coords;
    view  = Gp_GetViewIndex();
    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }

    if (task->state == 0) {
        gGameSession->field_80 = 0;
        task->msgTable         = D_acropolis_bridge_801898FC;
        gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM_EFFECT);
        gRoomEffectWaterRippleId = EFFECT_ACROPOLIS_BRIDGE_WATER_RIPPLE;
        gRoomEffectWaterSprayId  = EFFECT_ACROPOLIS_BRIDGE_WATER_SPRAY;
        task->state              = task->state + 1;
        for (i = 0; i < 2; i++) {
            part                              = &owner->extra.tmd->coords[14 + i * 3];
            D_acropolis_bridge_80189A34[i].vx = part->workm.t[0];
            D_acropolis_bridge_80189A34[i].vy = part->workm.t[1];
            D_acropolis_bridge_80189A34[i].vz = part->workm.t[2];
        }
    }

    work->age = work->age + 1;
    switch (view) {
        case 6:
            glowDrawTintedDiscNoBias(&D_acropolis_bridge_80189A44, 0x100, 0x5C20);
            break;
        case 7:
            glowDrawTintedDiscNoBias(&D_acropolis_bridge_80189A44, 0x100, 0x5C20);
            break;
        case 3:
        case 4:
        case 9:
            glowDrawTintedDiscNoBias(&D_acropolis_bridge_80189A4C, 0x100, 0x50C2);
            break;
    }

    bit = 1 << (view - 1);
    if (view == 9) {
        for (i = 0; i < 7; i++) {
            if (D_acropolis_bridge_801899EC[i] & bit) {
                pos.vx = 0;
                pos.vy = -0x240;
                pos.vz = 0;
                pos.vx = D_acropolis_bridge_8018991C[i].vx;
                pos.vy = D_acropolis_bridge_8018991C[i].vy - 0x240;
                pos.vz = D_acropolis_bridge_8018991C[i].vz;
                Gp_SpawnEff((EFFECT_ACROPOLIS_BRIDGE_STAR_GLOW | EFFECT_SPAWN_UNLIMITED), coord, work->age + i, &pos);
            }
        }
    } else {
        for (i = 0; i < 7; i++) {
            if (D_acropolis_bridge_801899EC[i] & bit) {
                Gp_SpawnEff((EFFECT_ACROPOLIS_BRIDGE_STAR_GLOW | EFFECT_SPAWN_UNLIMITED), coord, work->age + i, &D_acropolis_bridge_8018991C[i]);
                Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_GROUND_GLOW, coord, work->age + i, &D_acropolis_bridge_80189954[i]);
            }
        }
    }

    for (i = 0; i < 3; i++) {
        if (D_acropolis_bridge_80189A1C[i] & bit) {
            Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_LAMP_GLOW, coord, 0, &D_acropolis_bridge_8018998C[i]);
        }
    }
    for (i = 3; i < 5; i++) {
        if (D_acropolis_bridge_80189A1C[i] & bit) {
            Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_LAMP_GLOW, coord, 1, &D_acropolis_bridge_8018998C[i]);
        }
        if (D_acropolis_bridge_80189A1C[i + 2] & bit) {
            Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_LAMP_GLOW, coord, 2, &D_acropolis_bridge_8018998C[i + 2]);
        }
    }
    if (D_acropolis_bridge_80189A1C[11] & bit) {
        Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_LAMP_GLOW, coord, 1, &D_acropolis_bridge_8018998C[11]);
    }

    if ((bit & 0x62) && gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING && part->coord.t[1] >= 0x201) {
        for (i = 0; i < 2; i++) {
            part  = &owner->extra.tmd->coords[14 + i * 3];
            delta = D_acropolis_bridge_80189A34[i].vx - part->workm.t[0];
            dist  = delta < 0;
            if (dist) {
                delta = part->workm.t[0] - D_acropolis_bridge_80189A34[i].vx;
            }
            prev = D_acropolis_bridge_80189A34[i].vy;
            axis = prev - part->workm.t[1];
            if (axis < 0) {
                axis = part->workm.t[1] - prev;
            }
            dist        = delta + axis;
            prev        = D_acropolis_bridge_80189A34[i].vz;
            axis        = part->workm.t[2];
            delta       = prev - axis;
            delta       = ((delta >= 0) ? (dist + delta) : (dist + (axis - prev))) + 0x20;
            work->angle = delta;

            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rnd             = gRandomLcgState >> 16;
            if ((rnd & 0x1FF) < work->angle) {
                Gp_SpawnEff(gRoomEffectWaterRippleId, part, 0x40, NULL);
            }
            work->angle     = work->angle - 0x20;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rnd             = gRandomLcgState >> 16;
            if ((rnd & 0x1FF) < work->angle) {
                Gp_SpawnEff(gRoomEffectWaterSprayId, part, 0x1202180, NULL);
            }

            D_acropolis_bridge_80189A34[i].vx = part->workm.t[0];
            D_acropolis_bridge_80189A34[i].vy = part->workm.t[1];
            D_acropolis_bridge_80189A34[i].vz = part->workm.t[2];
        }
    }

    D_acropolis_bridge_801917AC =
        (DR_MOVE*)((u8*)Fs_ActorLoadBase2 + (gDisplayState.otBuffer * 0x7000 + 0xA000));

    switch (D_acropolis_bridge_801899FC[view - 1]) {
        case 0:
            break;
        case 1:
            lastView = work->scale;
            if (lastView != view) {
                for (i = 0; i < 0x1E; i++) {
                    Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_FALLING_STREAK, coord, (s32)(view), NULL);
                }
            } else if (gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
                if (work->age & 0x200) {
                    Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_FALLING_STREAK, coord, (s32)(lastView), NULL);
                    Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_FALLING_STREAK, coord, (s32)(lastView), NULL);
                } else {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    if (((gRandomLcgState >> 16) & 1) == 0) {
                        Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_FALLING_STREAK, coord, (s32)(lastView), NULL);
                    }
                }
            } else {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if ((u16)((gRandomLcgState >> 16) % 3) == 0) {
                    Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_FALLING_STREAK, coord, (s32)(lastView), NULL);
                }
            }
            break;
        case 2:
            lastView = work->scale;
            if (lastView != view) {
                for (i = 0; i < 0x1E; i++) {
                    Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_MID_DUST_STREAK, coord, (s32)(view), NULL);
                }
            } else if (gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
                Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_MID_DUST_STREAK, coord, (s32)(lastView), NULL);
                Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_MID_DUST_STREAK, coord, (s32)(lastView), NULL);
            } else {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if ((u16)((gRandomLcgState >> 16) % 3) == 0) {
                    Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_MID_DUST_STREAK, coord, (s32)(lastView), NULL);
                }
            }
            break;
        case 3:
            lastView = work->scale;
            if (lastView != view) {
                for (i = 0; i < 0x1E; i++) {
                    Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_LOW_DUST_STREAK, coord, (s32)(view), NULL);
                }
            } else if (gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
                Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_LOW_DUST_STREAK, coord, (s32)(lastView), NULL);
                Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_LOW_DUST_STREAK, coord, (s32)(lastView), NULL);
            } else {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if ((u16)((gRandomLcgState >> 16) % 3) == 0) {
                    Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_LOW_DUST_STREAK, coord, (s32)(lastView), NULL);
                }
            }
            break;
        case 5:
            lastView = work->scale;
            if (lastView != view) {
                for (i = 0; i < 0x1E; i++) {
                    Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_TALL_DUST_STREAK, coord, (s32)(view), NULL);
                }
            } else if (gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
                Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_TALL_DUST_STREAK, coord, (s32)(lastView), NULL);
                Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_TALL_DUST_STREAK, coord, (s32)(lastView), NULL);
            } else {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if ((u16)((gRandomLcgState >> 16) % 3) == 0) {
                    Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_TALL_DUST_STREAK, coord, (s32)(lastView), NULL);
                }
            }
            break;
        case 6:
            lastView = work->scale;
            if (lastView != view) {
                for (i = 0; i < 0x1E; i++) {
                    Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_PARTICLE_STREAK, coord, (s32)(view), NULL);
                }
            } else if (gRoomEffectState->battleState != ROOM_EFFECT_BATTLE_ENGAGED) {
                Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_PARTICLE_STREAK, coord, (s32)(lastView), NULL);
                Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_PARTICLE_STREAK, coord, (s32)(lastView), NULL);
            } else {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if ((u16)((gRandomLcgState >> 16) % 3) == 0) {
                    Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_PARTICLE_STREAK, coord, (s32)(lastView), NULL);
                }
            }
            break;
    }

    work->scale = view;
}

/// The wide variant of the bridge's falling dust streak: same one-pixel `DR_MOVE`
/// smear as `func_acropolis_bridge_80180FF0`, rolled over the whole drop height
/// instead of the upper band. The first frame rolls the streak out of
/// `gRandomLcgState`: `move.vy` is the row it starts on (0x60..0xEF),
/// `scale` the lifetime in frames, `angle` the width and `period` the
/// number of frames each row of fall takes. The column window widens with the
/// starting row - it runs from `0x40 - (vy - 0x60) / 3` to `0xD0 + spread`,
/// where `spread` is half the drop from 0x60 capped at 0x20 - so streaks that
/// begin higher up stay nearer the middle of the screen.
/// `gDisplayState.drawBuffer` picks the buffer half, and the OT slot is the row
/// scaled into the 0x800-deep range so a streak sorts against the room behind
/// it. The task releases itself once the camera turns away, the lifetime runs
/// out, or the streak falls off the bottom of the screen.
void func_acropolis_bridge_80180320(Task* task)
{
    EffectWork* work;
    RECT        rect;
    DR_MOVE*    mv;
    u16         rnd;
    s32         rndx;
    s32         range;
    s32         bufferY;
    s32         x;
    s32         y;
    s32         depth;

    work    = task->spawnArg2.pointer;
    bufferY = gDisplayState.drawBuffer * 0x110;
    if ((u8)Gp_GetViewIndex() == task->spawnArg1.value) {
        if (work->age == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rnd             = gRandomLcgState >> 16;
            work->move.vy   = (u32)rnd % 144 + 0x60;
            /* x and y double as the drift and spread of the column window here */
            x               = (work->move.vy - 0x60) / 3;
            y               = work->move.vy < 0xA0 ? (work->move.vy - 0x60) / 2 : 0x20;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rndx            = gRandomLcgState >> 16;
            range           = y + 0x90;
            work->move.vx   = rndx % (x + range) + (0x40 - x);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rnd             = gRandomLcgState >> 16;
            work->scale     = (u32)rnd % 90 + 0x1E;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = ((gRandomLcgState >> 16) & 0x3F) + 0x10;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->period    = ((gRandomLcgState >> 16) & 3) + 1;
            task->state++;
        }
        y     = work->move.vy + work->age / work->period;
        x     = work->move.vx;
        depth = 0x840 - (y - 0x60) * 8;
        if (y < 0xEF) {
            rect.x                      = x;
            rect.y                      = y + bufferY;
            rect.w                      = work->angle;
            rect.h                      = 1;
            mv                          = D_acropolis_bridge_801917AC;
            D_acropolis_bridge_801917AC = mv + 1;
            SetDrawMove(mv, &rect, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> 4), mv);
        }
        work->age++;
        if (work->age <= work->scale && y < 0xEF) {
            return;
        }
    }
    effectKillTask(work, task);
}

/// The mid variant of the bridge's falling dust streak: the same one-pixel
/// `DR_MOVE` smear as `func_acropolis_bridge_80180FF0`, rolled over the whole
/// drop height and sorted by a squared depth ramp like
/// `func_acropolis_bridge_80180CC0`, but nearer the camera. The first frame
/// rolls the streak out of `gRandomLcgState`: `move.vy` is the row it starts on
/// (0x48..0xEF), `scale` the lifetime in frames, `angle` the width and
/// `period` the number of frames each row of fall takes. The column window
/// widens with the starting row - it runs from `0x58 - drift` to
/// `0xA0 + spread`, where `drift` is the whole drop from 0x48 capped at 0x58
/// and `spread` five thirds of it capped at 0x50 - so streaks that begin higher
/// up stay nearer the middle of the screen. `gDisplayState.drawBuffer` picks the
/// buffer half, and the OT slot grows with the *square* of the distance left to
/// fall, so a streak near the bottom of the screen sorts sharply in front of
/// one still high up. The task releases itself once the camera turns away, the
/// lifetime runs out, or the streak falls off the bottom of the screen.
void func_acropolis_bridge_8018063C(Task* task)
{
    EffectWork* work;
    RECT        rect;
    DR_MOVE*    mv;
    u16         rnd;
    s32         rndx;
    s32         col;
    s32         range;
    s32         bufferY;
    s32         x;
    s32         y;
    s32         depth;

    work    = task->spawnArg2.pointer;
    bufferY = gDisplayState.drawBuffer * 0x110;
    if ((u8)Gp_GetViewIndex() == task->spawnArg1.value) {
        if (work->age == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rnd             = gRandomLcgState >> 16;
            work->move.vy   = (u32)rnd % 168 + 0x48;
            /* x and y double as the drift and spread of the column window here */
            x               = work->move.vy < 0xA0 ? work->move.vy - 0x48 : 0x58;
            y               = work->move.vy < 0x78 ? (work->move.vy - 0x48) * 5 / 3 : 0x50;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rndx            = gRandomLcgState >> 16;
            range           = y + 0x48;
            col             = rndx % (x + range) + 0x58;
            col            -= x;
            work->move.vx   = col;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rnd             = gRandomLcgState >> 16;
            work->scale     = (u32)rnd % 90 + 0x1E;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = ((gRandomLcgState >> 16) & 0x3F) + 0x10;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->period    = ((gRandomLcgState >> 16) & 3) + 1;
            task->state++;
        }
        y     = work->move.vy + work->age / work->period;
        x     = work->move.vx;
        depth = (0xF0 - y) * (0xF0 - y) / 15 + 0x2C0;
        if (y < 0xEF) {
            rect.x                      = x;
            rect.y                      = y + bufferY;
            rect.w                      = work->angle;
            rect.h                      = 1;
            mv                          = D_acropolis_bridge_801917AC;
            D_acropolis_bridge_801917AC = mv + 1;
            SetDrawMove(mv, &rect, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> 4), mv);
        }
        work->age++;
        if (work->age <= work->scale && y < 0xEF) {
            return;
        }
    }
    effectKillTask(work, task);
}

/// The narrow variant of the bridge's falling dust streak: the same one-pixel
/// `DR_MOVE` smear as `func_acropolis_bridge_80180FF0`, but rolled over the
/// lower part of the drop and sorted nearer the camera. The first frame rolls
/// the streak out of `gRandomLcgState`: `move.vy` is the row it starts on
/// (0x68..0xEF), `scale` the lifetime in frames, `angle` the width and
/// `period` the number of frames each row of fall takes. The column window
/// widens with the starting row - it runs from `0x20 - drift` to
/// `0x60 + spread`, where `drift` is twice and `spread` nine times the drop
/// from 0x68, both capped once the streak starts at 0x78 or below - so streaks
/// that begin higher up stay nearer the middle of the screen.
/// `gDisplayState.drawBuffer` picks the buffer half, and the OT slot is the row
/// scaled into the 0x600-deep range so a streak sorts against the room behind
/// it. The task releases itself once the camera turns away, the lifetime runs
/// out, or the streak falls off the bottom of the screen.
void func_acropolis_bridge_8018099C(Task* task)
{
    EffectWork* work;
    RECT        rect;
    DR_MOVE*    mv;
    u16         rnd;
    s32         rndx;
    s32         col;
    s32         range;
    s32         bufferY;
    s32         x;
    s32         y;
    s32         depth;

    work    = task->spawnArg2.pointer;
    bufferY = gDisplayState.drawBuffer * 0x110;
    if ((u8)Gp_GetViewIndex() == task->spawnArg1.value) {
        if (work->age == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rnd             = gRandomLcgState >> 16;
            work->move.vy   = (u32)rnd % 136 + 0x68;
            /* x and y double as the drift and spread of the column window here */
            x               = work->move.vy < 0x78 ? (work->move.vy - 0x68) * 2 : 0x20;
            y               = work->move.vy < 0x78 ? (work->move.vy - 0x68) * 9 : 0x90;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rndx            = gRandomLcgState >> 16;
            range           = y + 0x40;
            col             = rndx % (x + range) + 0x20;
            col            -= x;
            work->move.vx   = col;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rnd             = gRandomLcgState >> 16;
            work->scale     = (u32)rnd % 90 + 0x1E;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = ((gRandomLcgState >> 16) & 0x3F) + 0x10;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->period    = ((gRandomLcgState >> 16) & 3) + 1;
            task->state++;
        }
        y     = work->move.vy + work->age / work->period;
        x     = work->move.vx;
        depth = 0x600 - (y - 0x68) * 8;
        if (y < 0xEF) {
            rect.x                      = x;
            rect.y                      = y + bufferY;
            rect.w                      = work->angle;
            rect.h                      = 1;
            mv                          = D_acropolis_bridge_801917AC;
            D_acropolis_bridge_801917AC = mv + 1;
            SetDrawMove(mv, &rect, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> 4), mv);
        }
        work->age++;
        if (work->age <= work->scale && y < 0xEF) {
            return;
        }
    }
    effectKillTask(work, task);
}

/// The tallest variant of the bridge's falling dust streak: the same one-pixel
/// `DR_MOVE` smear as `func_acropolis_bridge_80180FF0`, but rolled over the
/// whole screen height and sorted by a squared depth ramp. The first frame
/// rolls the streak out of `gRandomLcgState`: `move.vy` is the row it starts on
/// (0x48..0xEF), `scale` the lifetime in frames, `angle` the width and
/// `period` the number of frames each row of fall takes. The column window
/// widens with the starting row - it runs from `0x58 - drift` to
/// `0xA0 + spread`, where `drift` is a third and `spread` a half of the drop
/// from 0x48 - so streaks that begin higher up stay nearer the middle of the
/// screen. `gDisplayState.drawBuffer` picks the buffer half, and the OT slot
/// grows with the *square* of the distance left to fall, so a streak near the
/// bottom of the screen sorts sharply in front of one still high up. The task
/// releases itself once the camera turns away, the lifetime runs out, or the
/// streak falls off the bottom of the screen.
void func_acropolis_bridge_80180CC0(Task* task)
{
    EffectWork* work;
    RECT        rect;
    DR_MOVE*    mv;
    u16         rnd;
    s32         rndx;
    s32         col;
    s32         range;
    s32         bufferY;
    s32         x;
    s32         y;
    s32         depth;

    work    = task->spawnArg2.pointer;
    bufferY = gDisplayState.drawBuffer * 0x110;
    if ((u8)Gp_GetViewIndex() == task->spawnArg1.value) {
        if (work->age == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rnd             = gRandomLcgState >> 16;
            work->move.vy   = (u32)rnd % 168 + 0x48;
            /* x and y double as the drift and spread of the column window here */
            x               = (work->move.vy - 0x48) / 3;
            y               = (work->move.vy - 0x48) / 2;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rndx            = gRandomLcgState >> 16;
            range           = y + 0x48;
            col             = rndx % (x + range) + 0x58;
            col            -= x;
            work->move.vx   = col;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rnd             = gRandomLcgState >> 16;
            work->scale     = (u32)rnd % 90 + 0x1E;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = ((gRandomLcgState >> 16) & 0x3F) + 0x10;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->period    = ((gRandomLcgState >> 16) & 3) + 1;
            task->state++;
        }
        y     = work->move.vy + work->age / work->period;
        x     = work->move.vx;
        depth = (0xF0 - y) * (0xF0 - y) / 15 + 0x400;
        if (y < 0xEF) {
            rect.x                      = x;
            rect.y                      = y + bufferY;
            rect.w                      = work->angle;
            rect.h                      = 1;
            mv                          = D_acropolis_bridge_801917AC;
            D_acropolis_bridge_801917AC = mv + 1;
            SetDrawMove(mv, &rect, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> 4), mv);
        }
        work->age++;
        if (work->age <= work->scale && y < 0xEF) {
            return;
        }
    }
    effectKillTask(work, task);
}

/// One falling dust streak on the bridge, drawn as a `DR_MOVE` that smears a
/// one-pixel-tall strip of the frame buffer down by a pixel. The first frame
/// rolls the whole streak out of `gRandomLcgState`: `move.vy` is the row it
/// starts on (0x68..0xE7), `move.vx` the column, `scale` the lifetime in
/// frames, `angle` the width and `period` the number of frames each row of
/// fall takes. The column is drawn from a range that widens with the starting
/// row - `(vy - 0x58) * 6`, capped at the full 240-pixel width once the streak
/// starts at 0x80 or below the horizon - so streaks that begin higher up stay
/// nearer the middle of the screen. `gDisplayState.drawBuffer` picks the buffer
/// half, and the OT slot is the row scaled into the 0x800-deep range so a
/// streak sorts against the room behind it. The task releases itself once the
/// camera turns away, the lifetime runs out, or the streak falls off the bottom
/// of the screen.
void func_acropolis_bridge_80180FF0(Task* task)
{
    EffectWork* work;
    RECT        rect;
    DR_MOVE*    mv;
    u16         rnd;
    s32         rndx;
    s32         bufferY;
    s32         x;
    s32         y;
    s32         depth;

    work    = task->spawnArg2.pointer;
    bufferY = gDisplayState.drawBuffer * 0x110;
    if ((u8)Gp_GetViewIndex() == task->spawnArg1.value) {
        if (work->age == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vy   = ((gRandomLcgState >> 16) & 0x7F) + 0x68;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rndx            = gRandomLcgState >> 16;
            work->move.vx   = work->move.vy < 0x80 ? rndx % ((work->move.vy - 0x58) * 6) : rndx % 240;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rnd             = gRandomLcgState >> 16;
            work->scale     = (u32)rnd % 90 + 0x1E;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = ((gRandomLcgState >> 16) & 0x3F) + 0x10;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->period    = ((gRandomLcgState >> 16) & 3) + 1;
            task->state++;
        }
        y     = work->move.vy + work->age / work->period;
        x     = work->move.vx;
        depth = 0x800 - (y - 0x68) * 8;
        if (y < 0xEF) {
            rect.x                      = x;
            rect.y                      = y + bufferY;
            rect.w                      = work->angle;
            rect.h                      = 1;
            mv                          = D_acropolis_bridge_801917AC;
            D_acropolis_bridge_801917AC = mv + 1;
            SetDrawMove(mv, &rect, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> 4), mv);
        }
        work->age++;
        if (work->age <= work->scale && y < 0xEF) {
            return;
        }
    }
    effectKillTask(work, task);
}

#define ACROPOLIS_GLOWS_STAR_TASK func_acropolis_bridge_801812F4
#include "../../shared/acropolis_glows_star.inc.c"

/// The bridge's dust cloud: one semi-transparent `POLY_FT4` billboard placed at
/// the task's world position. The four corners are taken from the unit quad in
/// `D_acropolis_bridge_8018990C`, scaled by 0x300 and rotated by the
/// coordinate's `workm` - and then each rotated corner is overwritten with that
/// same `workm` translation, so all four collapse onto the object origin. The
/// quad is projected with one `RTPS` plus one `RTPT` directly into the
/// primitive, tinted a random grey, and linked into the OT at the `RTPS` depth
/// biased by 0x20; depths under 0x11 are dropped rather than drawn. The task
/// releases its work block on every tick, so the puff lasts one frame.
void func_acropolis_bridge_801819C8(Task* task)
{
    void**                       scratch;
    _AcropolisBridgeQuadScratch* head;
    _AcropolisBridgeQuadScratch* block;
    EffectUnitQuadCorner*        corners;
    POLY_FT4*                    prim;
    GfxCoord*                    coord;
    EffectWork*                  work;
    MATRIX*                      m;
    SVECTOR*                     v;
    s32                          i;
    u8                           col;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    actorRenderComposeCoord(coord);

    scratch   = SCRATCH_STACK_CURSOR_SLOT;
    i         = 0;
    m         = &coord->workm;
    corners   = D_acropolis_bridge_8018990C;
    head      = SCRATCH_HEAD_AT(scratch, _AcropolisBridgeQuadScratch) - 1;
    work->age = task->spawnArg1.halves.low;
    *scratch  = head;
    block     = *scratch;
    do {
        // `v` is `&block->vertices[i]`, reached as the member of a block shifted
        // by `i` vectors. Every typed spelling adds the member offset before the
        // index, which is the GTE operands' address, and the two then share one
        // register; the original keeps a second one for the stores through `v`.
        v     = ((_AcropolisBridgeQuadScratch*)((SVECTOR*)block + i))->vertices;
        v->vx = corners[i].axis0Sign * 0x300;
        v->vy = 0;
        v->vz = corners[i].axis1Sign * 0x300;
        gte_SetRotMatrix(m);
        gte_ldv0(&block->vertices[i]);
        gte_rtv0();
        gte_stsv(&block->vertices[i]);
        (u16) v->vx = (u16)coord->workm.t[0];
        i++;
        (u16) v->vy = (u16)coord->workm.t[1];
        (u16) v->vz = (u16)coord->workm.t[2];
    } while (i < ARRAY_SIZE(D_acropolis_bridge_8018990C));

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vertices[0]);
    gte_rtps();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&prim->x0);
    gte_ldv3(&block->vertices[1], &block->vertices[2], &block->vertices[3]);
    gte_rtpt();
    setUV4(prim, 0, 0x10, 0x27, 0x10, 0, 0x37, 0x27, 0x37);
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&block->otz);
    block->otz += 0x20;
    if (block->otz >= 0x11) {
        prim->tpage     = 0x2B;
        prim->clut      = 0x4381;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        col             = (gRandomLcgState >> 16) & 0xF;
        setRGB0(prim, col, col, col);
        setSemiTrans(prim, 1);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_AcropolisBridgeQuadScratch);
    effectKillTask(work, task);
}

#define ACROPOLIS_GLOWS_LAMP_TASK func_acropolis_bridge_80181D28
#include "../../shared/acropolis_glows_lamp.inc.c"

s32 func_acropolis_bridge_801820A0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    GfxCoord* coord;
    SVECTOR   pos;
    s32       i;

    coord = task->extra.tmd->coords;

    i = 0;
    do {
        pos.vx          = -0x3E58;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        pos.vy          = (gRandomLcgState >> 16) % 1536 + 0xF830;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        pos.vz          = ((gRandomLcgState >> 16) & 0xF) + 0xF63C;
        Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_DUST_MOTE, coord, 0, &pos);

        pos.vx          = -0x3E58;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        pos.vy          = (gRandomLcgState >> 16) % 1536 + 0xF830;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        pos.vz          = 0xFA06 - ((gRandomLcgState >> 16) & 0xF);
        Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_DUST_MOTE, coord, 0, &pos);

        pos.vx          = -0x3E58;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        pos.vy          = ((gRandomLcgState >> 16) & 0xF) + 0xF830;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        pos.vz          = (u16)(gRandomLcgState >> 16) % 970 + 0xF63C;
        Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_DUST_MOTE, coord, 0, &pos);
        i++;
    } while (i < 0x20);

    i = 0;
    do {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        pos.vx          = -0x3E58;
        pos.vy          = (gRandomLcgState >> 16) % 1536 - 0x7D0;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        pos.vz          = (u16)(gRandomLcgState >> 16) % 970 - 0x9C4;
        Gp_SpawnEff(EFFECT_ACROPOLIS_BRIDGE_DUST_MOTE, coord, 0, &pos);
        i++;
    } while (i < 8);

    return 0;
}

/// One falling mote of the bridge's ambient dust: drifts the task's coordinate
/// frame by the per-mote velocity in `EffectWork::move`, projects the
/// result through `GsWSMATRIX` with a single `RTPS`, and links a 1x1 tile into
/// the OT at the resulting depth. The velocity and the grey level are rolled
/// once, on the first tick (`age == 0`); the mote is released after 0x1F
/// ticks or once it has fallen past y = -0x1D.
void func_acropolis_bridge_80182394(Task* task)
{
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
    // Project the cached view position; screen coordinates go straight into the tile.
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
        work->move.vy += 6;
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectPointTileScratch);
    work->age++;
    if (work->age >= 0x1F || coord->coord.t[1] >= -0x1D) {
        effectKillTask(work, task);
    }
}

void func_acropolis_bridge_80182694(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        func_acropolis_bridge_801827EC(coord, work->angle, work->scale);
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
    } else {
        work->age++;
        switch (task->state) {
            case 0:
                work->scale     = 0x40;
                work->angle     = task->spawnArg1.halves.low & 0xFFF;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gfxRotMatrixY(&coord->coord, (gRandomLcgState >> 16) & 0xFFF, 1);
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                task->state         = 1;
                /* fallthrough */
            case 1:
                work->angle += 0x20;
                func_acropolis_bridge_801827EC(coord, work->angle, work->scale);
                if (work->scale >= 3) {
                    work->scale -= 2;
                } else {
                    effectKillTask(work, task);
                }
                break;
        }
    }
}

/// Draws the flash the bridge collapse throws off as a screen-facing quad: the
/// unit quad `D_80111E38` scaled to `arg1` half-size, rotated by the task's own
/// `GfxCoord` (`workm`) and then projected through `GsWSMATRIX` into a
/// 0x28-byte scratch stack block. The first corner goes through `rtps` and
/// the other three through `rtpt`; a GTE error (`gte_stflg` sign bit) drops the
/// quad rather than drawing it. The `POLY_FT4` is the 0x38x0x38 cell at
/// `(0, 0x38)` of tpage 0x2B, modulated by the grey `arg2` and drawn
/// semi-transparent, and links into the OT at the projected depth.
static void func_acropolis_bridge_801827EC(GfxCoord* coord, s32 arg1, s16 arg2)
{
    OverlayFlaggedQuadScratch* blk;
    POLY_FT4*                  prim;
    SVECTOR*                   sv;
    s32                        i;

    blk = SCRATCH_STACK_RESERVE_BLOCK(OverlayFlaggedQuadScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < ARRAY_SIZE(D_80111E38); i++) {
        blk->corners[i].vx = (u16)D_80111E38[i].axis0Sign * arg1;
        /* Spelled as an offset rather than `&blk->corners[i]`, which is the same
           address: the member form lets CSE share one register with the GTE
           macros' `&blk->corners[i]`, and the original keeps two. */
        sv     = (SVECTOR*)((u8*)blk + i * sizeof(SVECTOR) + OFFSET_OF(OverlayFlaggedQuadScratch, corners));
        sv->vy = 0;
        sv->vz = (u16)D_80111E38[i].axis1Sign * arg1;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->corners[i]);
        gte_rtv0();
        gte_stsv(&blk->corners[i]);
        blk->corners[i].vx += coord->workm.t[0];
        sv->vy             += coord->workm.t[1];
        sv->vz             += coord->workm.t[2];
    }

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->corners[0]);
    gte_rtps();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->corners[1], &blk->corners[2], &blk->corners[3]);
    gte_rtpt();
    setUV4(prim, 0, 0x38, 0x37, 0x38, 0, 0x6F, 0x37, 0x6F);
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stflg(&blk->flag);
    if (blk->flag >= 0) {
        gte_stszotz(&blk->otz);
        blk->otz++;
        prim->tpage = 0x2B;
        setRGB0(prim, arg2, arg2, arg2);
        prim->clut = 0x43D1;
        setSemiTrans(prim, 1);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(OverlayFlaggedQuadScratch);
}

#include "../../shared/effect_sprite_debris.inc.c"

/// Draws one piece of the bridge's blown debris as a screen-facing quad. The
/// piece's world position is copied out of `coord->workm.t` and projected
/// through `GsWSMATRIX` with a single `RTPS`; a GTE error (`gte_stflg` sign
/// bit) drops the piece rather than drawing it. The `POLY_FT4` is centred on
/// the projected point, its two diagonals `size * 31 / depth` long and turned by
/// `angle` and `angle + 0x400`, so the quad shrinks with distance and spins
/// with the piece. `frame` picks the animation cell: the texture window is the
/// 0x1F-wide column starting at `frame * 0x20` on rows 0xE0..0xFF of tpage
/// 0x2B. The primitive is semi-transparent with texture blending off
/// (`code |= 3`) and links into the OT at the projected depth.
void effectSpriteDrawChip(GfxCoord* coord, u16 frame, s16 size, s16 angle)
{
    void**                  scratch;
    EffectBillboardScratch* scratchHead;
    EffectBillboardScratch* block;
    s32*                    depthOutput;
    POLY_FT4*               prim;
    s32                     ang;
    s32                     u;
    s32                     uu;

    scratch     = SCRATCH_STACK_CURSOR_SLOT;
    scratchHead = *scratch;
    block       = scratchHead - 1;
    depthOutput = &block->depth;

    block->worldPoint.vx = (u16)coord->workm.t[0];
    block->worldPoint.vy = (u16)coord->workm.t[1];
    block->worldPoint.vz = (u16)coord->workm.t[2];
    *scratch             = block;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();

    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);

    if (block->projectionFlags >= 0) {
        gte_stszotz(depthOutput);
        block->depth++;
        prim->tpage = 0x2B;
        prim->clut  = 0x43D3;
        u           = frame << 5;
        uu          = u + 0x1F;
        setUV4(prim, u, 0xE0, uu, 0xE0, u, 0xFF, uu, 0xFF);
        setcode(prim, getcode(prim) | 3);

        ang                  = angle;
        block->cornerOffsetX = (size * 31 / block->depth * rsin(ang)) >> 12;
        block->cornerOffsetY = (size * 31 / block->depth * rcos(ang)) >> 12;
        prim->x0             = block->screenX + (u16)block->cornerOffsetX;
        prim->x3             = block->screenX - (u16)block->cornerOffsetX;
        prim->y0             = block->screenY - (u16)block->cornerOffsetY;
        prim->y3             = block->screenY + (u16)block->cornerOffsetY;

        ang                 += 0x400;
        block->cornerOffsetX = (size * 31 / block->depth * rsin(ang)) >> 12;
        block->cornerOffsetY = (size * 31 / block->depth * rcos(ang)) >> 12;
        prim->x1             = block->screenX + (u16)block->cornerOffsetX;
        prim->x2             = block->screenX - (u16)block->cornerOffsetX;
        prim->y1             = block->screenY - (u16)block->cornerOffsetY;
        prim->y2             = block->screenY + (u16)block->cornerOffsetY;

        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, sizeof(*block));
}

/// Draws one piece of the bridge's blown debris as an upright screen-facing
/// quad, the unrotated counterpart of `effectSpriteDrawChip`. The piece's
/// world position is copied out of `coord->workm.t` and projected through
/// `GsWSMATRIX` with one perspective transform. A negative GTE flag word
/// drops the piece. The `POLY_FT4` is a square whose half-side is
/// `size * 55 / depth`, shifted up so the projected point sits three quarters
/// of the way down it. The quad stays axis-aligned and shrinks with distance.
///
/// `frame` picks the animation cell from a 4-by-2 grid of 0x38 by 0x38 cells
/// on tpage 0x2B: bits 0-1 pick the column and bit 2 the row, and the cell's
/// texture V is that row plus 0x70. The primitive is semi-transparent with
/// texture blending off (`code |= 3`) and links into the ordering table at
/// the projected depth.
void effectSpriteDrawBillboard(GfxCoord* coord, u16 frame, s16 size)
{
    void**                            scratch;
    u8*                               head;
    _AcropolisBridgeBillboardScratch* block;
    POLY_FT4*                         prim;
    _AcropolisBridgeBillboardScratch* depthOut;
    u32                               cell;
    s32                               u;
    s32                               v;
    s8                                vTop;
    s8                                vBot;

    scratch  = SCRATCH_STACK_CURSOR_SLOT;
    head     = *scratch;
    block    = (_AcropolisBridgeBillboardScratch*)(head - sizeof(_AcropolisBridgeBillboardScratch));
    depthOut = block;

    block->worldPoint.vx = (u16)coord->workm.t[0];
    block->worldPoint.vy = (u16)coord->workm.t[1];
    block->worldPoint.vz = (u16)coord->workm.t[2];
    *scratch             = block;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    // The vector, screen position and flag word are addressed from the cursor
    // saved in `head`, as are the depth increment and the ordering-table read.
    // The depth store goes through `depthOut`.
    gte_ldv0(&((_AcropolisBridgeBillboardScratch*)(head - sizeof(_AcropolisBridgeBillboardScratch)))->worldPoint);
    gte_rtps();

    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&((_AcropolisBridgeBillboardScratch*)(head - sizeof(_AcropolisBridgeBillboardScratch)))->screenX);
    gte_stflg(&((_AcropolisBridgeBillboardScratch*)(head - sizeof(_AcropolisBridgeBillboardScratch)))->projectionFlags);

    if (block->projectionFlags >= 0) {
        gte_stszotz(&depthOut->depth);
        ((_AcropolisBridgeBillboardScratch*)(head - sizeof(_AcropolisBridgeBillboardScratch)))->depth++;
        prim->tpage = 0x2B;
        prim->clut  = 0x43D2;
        cell        = frame;
        u           = (cell & 3) * 0x38;
        v           = ((cell & 7) >> 2) * 0x38;
        vTop        = v + 0x70;
        vBot        = v + 0x70 + 0x37;
        setUV4(prim, u, vTop, u + 0x37, vTop, u, vBot, u + 0x37, vBot);
        setcode(prim, getcode(prim) | 3);

        block->screenExtent = size * 55 / ((_AcropolisBridgeBillboardScratch*)(head - sizeof(_AcropolisBridgeBillboardScratch)))->depth;

        prim->x0 = prim->x2 = block->screenX - (u16)block->screenExtent;
        prim->x1 = prim->x3 = block->screenX + (u16)block->screenExtent;
        prim->y0 = prim->y1 = block->screenY - (u16)block->screenExtent - (block->screenExtent >> 1);
        prim->y2 = prim->y3 = block->screenY + (block->screenExtent >> 1);

        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)((_AcropolisBridgeBillboardScratch*)(head - sizeof(_AcropolisBridgeBillboardScratch)))->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, sizeof(_AcropolisBridgeBillboardScratch));
}

/// Debug message the walker's route search prints when no candidate beat its
/// initial best of 0xFF.
static const char _gPatrolNoPairMsg[] = "s->root_cnt == 0xff about \n";

/// The bridge enemy's three state handlers: setup, per-frame tick and teardown.
static const EnemyTaskFuncTable3 D_acropolis_bridge_8017D6E8 = {
    { func_acropolis_bridge_80185988, func_acropolis_bridge_80187850, enemyDestroy }
};

#include "../../shared/glow_draw_tinted_disc_no_bias.inc.c"

#include "../../shared/boss_stranger_arrived.inc.c"

#include "../../shared/boss_stranger_follow_route.inc.c"

#include "../../shared/boss_stranger_nearest_actor.inc.c"

#include "../../shared/boss_stranger_nearest_self.inc.c"

#include "../../shared/boss_stranger_plan_toward.inc.c"

#include "../../shared/boss_stranger_ground_step.inc.c"

#include "../../shared/boss_stranger_avoid_contacts.inc.c"

#include "../../shared/boss_stranger_turn_toward.inc.c"

/// Runs the walker's per-frame step inside the `BossStrangerTickScratch` frame
/// `bossStrangerTick` opened for it. `head` is the scratch cursor as it was
/// before the frame was reserved, one frame past `block`, so the position the
/// states steer towards is `head[-1].goal`, the same object as `block->goal`.
///
/// State 1 heads straight for the selected player's matrix translation.
/// State 2 walks `nav`'s `nodeOrder` and re-plans when the state or a node
/// byte changes, and state 3 follows the patrol route. `speed` then ramps
/// towards `speedTarget` by at most `speedStep` a frame; while it is non-zero
/// it scales (`GPF`) the normalised facing column of the model matrix into the
/// per-frame world step, which is added to the coordinate's translation and
/// kept in `moveStep`. `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen`
/// zeroes the step instead.
static __inline__ void walkerStep(BossStrangerWalker* walker, BossStrangerTickScratch* head,
                                  BossStrangerTickScratch* block)
{
    u8*           head2;
    SVECTOR3*     pos;
    PlayerStatus* cfg;
    SVECTOR*      sv;
    SVECTOR*      gsv;
    SVECTOR*      step;
    GfxCoord*     coord;
    s16           sdiff;
    s32           diff;
    s16           speed;
    s32           cur;
    s32           target;
    s32           result;

    switch (walker->state) {
        case BOSS_STRANGER_WALKER_IDLE:
            break;
        case BOSS_STRANGER_WALKER_CHASE:
            cfg              = &gPlayerStatus + (walker->playerId - 1);
            pos              = &head[-1].goal;
            head[-1].goal.vx = (u16)cfg->coordMtx->t[0];
            pos->vy          = (u16)cfg->coordMtx->t[1];
            pos->vz          = (u16)cfg->coordMtx->t[2];
            break;
        case BOSS_STRANGER_WALKER_CLOSE:
            SCRATCH_STACK_RESERVE_BYTES(4);
            walker->actorNode = bossStrangerNodeNearestActor(walker, 1);
            walker->selfNode  = bossStrangerNodeNearestSelf(walker);
            if (walker->prevState != walker->state || walker->selfNode != walker->prevSelfNode ||
                walker->actorNode != walker->prevActorNode) {
                bossStrangerPlanToward(walker, 1);
                walker->node = walker->nav->nodeOrder[walker->cursor];
            }
            walker->prevState     = walker->state;
            walker->prevSelfNode  = walker->selfNode;
            walker->prevActorNode = walker->actorNode;
            if (bossStrangerArrived(walker) != 0) {
                walker->cursor += (u8)walker->orderStep;
                walker->node    = walker->nav->nodeOrder[walker->cursor];
                SCRATCH_STACK_RELEASE_BYTES(4);
            }
            break;
        case BOSS_STRANGER_WALKER_PATROL:
            bossStrangerFollowRoute(walker, &head[-1].goal);
            break;
    }
    bossStrangerTurnToward(walker, &block->goal);

    cur    = walker->speedTarget;
    target = walker->speed;
    if (cur != target) {
        diff  = cur - target;
        sdiff = diff;
        if (sdiff > walker->speedStep) {
            result = target + walker->speedStep;
        } else if (sdiff < -walker->speedStep) {
            result = target - walker->speedStep;
        } else {
            result = target + diff;
        }
        walker->speed = result;
    }

    coord = walker->coord;
    speed = walker->speed;
    step  = &walker->moveStep;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1) {
        step->vz            = 0;
        step->vy            = 0;
        walker->moveStep.vx = 0;
    } else {
        head2                    = SCRATCH_STACK_CURSOR(u8);
        sv                       = (SVECTOR*)(head2 - 8);
        SCRATCH_STACK_CURSOR(u8) = (u8*)sv;
        /* The ROM keeps a second copy of the block address for the GTE
           transfers; without it `sv` and the copy share one register. */
        gsv = sv;
        if (speed != 0) {
            gfxReadMatrixZAxis(&coord->coord, sv);
            VectorNormalSS(sv, sv);
            gte_lddp(speed);
            gte_ldsv(gsv);
            gte_gpf12();
            gte_stsv(gsv);
            coord->coord.t[0]  += ((SVECTOR*)(head2 - 8))->vx;
            coord->coord.t[1]  += sv->vy;
            coord->coord.t[2]  += sv->vz;
            walker->moveStep    = *(SVECTOR*)(head2 - 8);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        SCRATCH_STACK_RELEASE_BYTES(8);
    }
    if (walker->skipGround == 0) {
        bossStrangerApplyGroundStep(walker);
    }
    if (walker->skipAvoid == 0) {
        bossStrangerAvoidContacts(walker);
    }
}

#include "../../shared/boss_stranger_inlines.inc.c"

#include "../../shared/boss_stranger_tick.inc.c"

/// Handles the room's 0x7DB broadcast for the bridge enemy. Message 0x0B01/1
/// (the bridge is being lowered) restores the model's default flag set while
/// the enemy is still in one of its first three spawn variants, and message
/// 0x0E01/2 (the bridge run has ended) decides whether the enemy is armed for
/// this variant: variant 0 needs `gSceneCombatState.battleRefs` to be set at all, variant 1 needs
/// it to be at least 2 and variant 2 at least 3. When it is, the enemy and the
/// work block are given the stat block's starting HP and the behaviour state
/// advances to 4; otherwise the state resets to 0 and the mesh is hidden behind
/// the default flag set. Always reports success.
s32 func_acropolis_bridge_801856E0(Task* task, s32 msgId, ActorCommand* msg, s32 arg3)
{
    _AcropolisBridgeEnemyWork* work  = (_AcropolisBridgeEnemyWork*)task->work;
    Enemy*                     enemy = (Enemy*)task->spawnArg2.pointer;
    TmdObject*                 extra = task->extra.tmd;
    s32                        variant;
    u16                        sub;

    if (msg->context.key == 0xB01 && msg->command == 1) {
        variant = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        switch (variant) {
            case 0:
            case 1:
            case 2:
                extra->flags = 0;
                break;
        }
    }
    if (msg->context.key == 0xE01) {
        sub = msg->command;
        if (sub == 2) {
            variant = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
            switch (variant) {
                case 0:
                    if (gSceneCombatState.battleRefs != 0) {
                        break;
                    }
                    work->state = 0;
                    goto hide;
                case 1:
                    if (gSceneCombatState.battleRefs >= 2) {
                        break;
                    }
                    work->state = 0;
                    goto hide;
                case 2:
                    if (gSceneCombatState.battleRefs < 3) {
                        goto reset;
                    }
                    break;
                default:
                    work->state = 0;
                    goto hide;
            }
            {
                u16 hp    = D_acropolis_bridge_80190C5C.hpMax;
                enemy->hp = hp;
                work->hp  = hp;
            }
            work->state = 4;
            goto done;
        reset:
            work->state = 0;
        hide:
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
    }
done:
    return 1;
}

/// Drives the bridge enemy's three animation slots from the state word at
/// `animRequest`. Request 1 restarts every slot on animation `animId` with the
/// blend value the room's `D_acropolis_bridge_801915E4` table holds for the
/// (previous, next) animation pair, state 2 resets them without a blend, and
/// both then latch `animId` as `prevAnimId` and hand over to
/// state 3, which just ticks the slots once per frame and counts frames in
/// `animFrame`. Every path first copies `animRate` into each slot's
/// `rate`.
static void func_acropolis_bridge_8018581C(Task* task)
{
    _AcropolisBridgeEnemyWork* work;
    _AcropolisBridgeEnemyWork* start;
    _AcropolisBridgeEnemyWork* reset;
    _AcropolisBridgeEnemyWork* tick;
    s32                        i;
    s32                        j;
    s32                        k;

    work = (_AcropolisBridgeEnemyWork*)task->work;
    if (work->animRequest == 1) {
        start = (_AcropolisBridgeEnemyWork*)task->work;
        for (i = 1; i < 4; i++) {
            start->rig.slots[i].rate = start->animRate;
            animationSeekSlotWithBlend(&start->rig.anim, i, start->animId, 0,
                                       D_acropolis_bridge_801915E4[start->prevAnimId][start->animId]);
        }
        start->prevAnimId = start->animId;
        goto advance;
    }
    if (work->animRequest == 2) {
        reset = (_AcropolisBridgeEnemyWork*)task->work;
        for (j = 1; j < 4; j++) {
            reset->rig.slots[j].rate = reset->animRate;
            animationResetSlot(&reset->rig.anim, j, reset->animId);
        }
        reset->prevAnimId = reset->animId;
    advance:
        work->animRequest = 3;
        work->animFrame   = 0;
        return;
    }
    if (work->animRequest == 3) {
        work->animFrame++;
        tick = (_AcropolisBridgeEnemyWork*)task->work;
        for (k = 1; k < 4; k++) {
            tick->rig.slots[k].rate = tick->animRate;
            animationTickSlot(&tick->rig.anim, k);
        }
    }
}

/// Copies a scratch `SVECTOR3` onto a `WorldCollisionBody`'s three position halfwords.
static __inline__ void bridge_set_obj_pos(WorldCollisionBody* obj, SVECTOR3* pos)
{
    obj->pos.vx = pos->vx;
    obj->pos.vy = pos->vy;
    obj->pos.vz = pos->vz;
}

/// Rebuilds the walker's scale matrix: resets it to identity and, unless
/// `scale` is 0 or unity, scales it by `scale` on all three axes. The `VECTOR`
/// handed to `ScaleMatrix` is taken from the scratch stack and left there for
/// `_acropolisBridgeLightModel` to reuse and release.
static __inline__ void _acropolisBridgeInitWalkerScale(BossStrangerWalker* walker)
{
    VECTOR* head;
    VECTOR* scale;
    s32     amount;

    head                         = SCRATCH_STACK_CURSOR(VECTOR);
    amount                       = walker->scale;
    walker->scaleMtx.m[2][1]     = 0;
    walker->scaleMtx.m[2][0]     = 0;
    walker->scaleMtx.m[1][2]     = 0;
    walker->scaleMtx.m[1][0]     = 0;
    walker->scaleMtx.m[0][2]     = 0;
    walker->scaleMtx.m[0][1]     = 0;
    walker->scaleMtx.m[2][2]     = 0x1000;
    walker->scaleMtx.m[1][1]     = 0x1000;
    walker->scaleMtx.m[0][0]     = 0x1000;
    walker->scaleMtx.t[2]        = 0;
    walker->scaleMtx.t[1]        = 0;
    walker->scaleMtx.t[0]        = 0;
    scale                        = head - 1;
    SCRATCH_STACK_CURSOR(VECTOR) = scale;
    if (amount != 0 && amount != 0x1000) {
        scale->vz = amount;
        scale->vy = amount;
        scale->vx = amount;
        ScaleMatrix(&walker->scaleMtx, scale);
    }
}

/// Recomputes `coord`'s world matrix and hands the model's root translation to
/// `worldCoordSetModelLighting`, staged in the scratch `VECTOR` on top of the stack, which
/// it then releases.
static __inline__ void _acropolisBridgeLightModel(Task* task, GfxCoord* coord)
{
    VECTOR* vec;

    vec = SCRATCH_STACK_CURSOR(VECTOR);
    actorRenderComposeCoord(coord);
    vec->vx = task->extra.tmd->coords->workm.t[0];
    vec->vy = task->extra.tmd->coords->workm.t[1];
    vec->vz = task->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(task->extra.tmd, vec, 0, 3);
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// One-time setup for the bridge enemy: allocates the 0x294-byte work block,
/// points the model's light and colour matrices at it, hangs the enemy off the
/// room's stat block, starts the animation context on three slots, and links
/// the model and hit-box `WorldCollisionBody`s (lists 2 and 3) onto the part coordinates at
/// `field_8[3]` and `field_8[1]`. The walker half is seeded next: its patrol
/// tables are the copies embedded in the walker itself, the route is the entry
/// `D_acropolis_bridge_80191720` holds for this spawn variant, and the scale
/// matrix is rebuilt from `scale` through a `VECTOR` borrowed from the scratch
/// arena -- the same block is then reused for the world position handed to
/// `worldCoordSetModelLighting` before it is released. The model starts sunk 0x5DC
/// (`sinkDepth`) below the root height it spawned at (`baseHeight`). In the
/// third visit (`gGameSession->location.loc.room == 2`) the three known variants start
/// in state 8 at a fixed position instead of state 1.
static void func_acropolis_bridge_80185988(Enemy* enemy, Task* task)
{
    TmdObject*                 obj;
    TmdObject*                 obj2;
    GfxCoord*                  coord;
    GfxCoord*                  coord2;
    _AcropolisBridgeEnemyWork* work;
    BossStrangerWalker*        walker;
    WorldCollisionBody*        link;
    WorldCollisionBody*        link2;
    s32                        variant;
    s32                        step;
    u16                        hp;
    s32                        axisY;
    SVECTOR3                   pos;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    work       = memCalloc(sizeof(_AcropolisBridgeEnemyWork), 0);
    task->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    obj2            = task->extra.tmd;
    obj2->lightMtx  = &work->lightMtx;
    obj2->colorMtx  = &work->colorMtx;
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    enemy->param    = &D_acropolis_bridge_80190C5C;
    hp              = D_acropolis_bridge_80190C5C.hpMax;
    enemy->recs     = work->bodyContacts;
    enemy->hp       = hp;
    work->hp        = 1;
    work->field_10E = 1;
    enemy->hpMax    = work->hp;
    enemy->hp       = enemy->hpMax;
    animationInitContext(&work->rig.anim, D_acropolis_bridge_801915C8, obj, work->rig.poses,
                         work->rig.slots);
    work->animRate         = ANIMATION_RATE_ONE;
    link                   = &work->body;
    link->coord            = &task->extra.tmd->coords[3];
    link->context.contacts = work->bodyContacts;
    link->pos.vx           = 0;
    link->pos.vy           = 0;
    link->pos.vz           = 0;
    link->key              = 0x30029;
    link->radius           = 0x100;
    link->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, link);
    link->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(link->context.contacts, 3, 0);
    pos.vx                  = 0;
    pos.vy                  = 0;
    pos.vz                  = 0;
    link2                   = &work->attack;
    link2->coord            = &task->extra.tmd->coords[1];
    link2->context.contacts = work->attackContacts;
    bridge_set_obj_pos(link2, &pos);
    link2->radius = 0x100;
    link2->flags  = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, link2);
    worldCollisionInitContacts(link2->context.contacts, 1, 0);
    work->attack.key  = Gp_PackObjPair(enemy, 0);
    coord->parent     = &gGfxViewCoord;
    work->yaw         = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    work->animRequest = 2;
    work->animId      = 2;
    func_acropolis_bridge_8018581C(task);
    enemy->coord      = &task->extra.tmd->coords[3];
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = 0;
    enemy->bodyPos.vz = 0;
    Gp_LinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    task->msgTable                = D_acropolis_bridge_80191744;
    work->prevState               = -1;
    work->state                   = 1;
    work->sinkDepth               = 0x5DC;
    work->baseHeight              = coord->coord.t[1];
    coord->coord.t[1]            += work->sinkDepth;

    work->walker.navData.nodes         = D_acropolis_bridge_8019162C;
    work->walker.navData.nodeCount     = 0xA;
    work->walker.navData.nodeOrder     = D_acropolis_bridge_801916CC;
    work->walker.navData.orderCount    = 0xA;
    work->walker.routeData.nodeIndices = D_acropolis_bridge_80191720[enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT];
    work->walker.nav                   = &work->walker.navData;
    work->walker.routeData.cursor      = 0;
    work->walker.route                 = &work->walker.routeData;
    coord2                             = task->extra.tmd->coords;
    work->walker.avoidCount            = 3;
    walker                             = &work->walker;
    work->walker.recs                  = 0;
    work->walker.avoidRecs             = work->bodyContacts;
    work->walker.scale                 = 0x1000;
    work->walker.recCount              = 0;
    work->walker.turnLimit             = 0x30;
    work->walker.coord                 = coord2;
    walker->speedTarget                = 0x30;
    walker->speed                      = 0;
    walker->speedStep                  = 1;
    step                               = BOSS_STRANGER_WALKER_PATROL;
    work->walker.state                 = step;
    work->walker.lockHeight            = 1;
    work->walker.skipGround            = 1;
    work->walker.playerId              = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId;

    _acropolisBridgeInitWalkerScale(walker);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    _acropolisBridgeLightModel(task, task->extra.tmd->coords);
    axisY = 1;
    if (gSceneCombatState.battleRefs < 3) {
        Gp_IncStateF0Ref(0);
    }
    if (gGameSession->location.loc.room == 2) {
        variant = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
        switch (variant) {
            case 0:
                work->state                         = 8;
                task->extra.tmd->coords->coord.t[0] = -0x22C4;
                task->extra.tmd->coords->coord.t[1] = -0x3E8;
                task->extra.tmd->coords->coord.t[2] = -0x640;
                break;
            case 1:
                work->state                             = 8;
                task->extra.tmd->coords->coord.t[0]     = -0x270F;
                task->extra.tmd->coords->coord.t[axisY] = -0x3E8;
                task->extra.tmd->coords->coord.t[2]     = -0x7D0;
                break;
            case 2:
                work->state                         = 8;
                task->extra.tmd->coords->coord.t[0] = -0x2EE0;
                task->extra.tmd->coords->coord.t[1] = -0x3E8;
                task->extra.tmd->coords->coord.t[2] = -0x5DC;
                break;
            default:
                work->state = 0;
                break;
        }
    }
    task->state++;
}

/// Resets the walker's scale matrix to a uniform `walker->scale` scale, through
/// a `VECTOR` borrowed from the scratch arena and released again. Identity is
/// left in place at the two ends of the ramp (0 and full size), where scaling
/// would be a no-op anyway. `func_acropolis_bridge_80185F28` and
/// `func_acropolis_bridge_801863A8` both inline it on their first frame, where
/// the scratch block is taken and released around the whole matrix reset, and
/// the shrink variant below once per frame of the shrink, where the diagonal is
/// written before the block is taken.
static __inline__ void bridge_reset_scale_mtx_entry(_AcropolisBridgeEnemyWork* work)
{
    BossStrangerWalker* walker;
    u8*                 head;
    VECTOR*             scale;
    s32                 amount;

    head                         = SCRATCH_STACK_CURSOR(u8);
    walker                       = &work->walker;
    amount                       = walker->scale;
    scale                        = (VECTOR*)(head - 0x10);
    SCRATCH_STACK_CURSOR(VECTOR) = scale;
    walker->scaleMtx.m[2][1]     = 0;
    walker->scaleMtx.m[2][0]     = 0;
    walker->scaleMtx.m[1][2]     = 0;
    walker->scaleMtx.m[1][0]     = 0;
    walker->scaleMtx.m[0][2]     = 0;
    walker->scaleMtx.m[0][1]     = 0;
    walker->scaleMtx.m[2][2]     = 0x1000;
    walker->scaleMtx.m[1][1]     = 0x1000;
    walker->scaleMtx.m[0][0]     = 0x1000;
    walker->scaleMtx.t[2]        = 0;
    walker->scaleMtx.t[1]        = 0;
    walker->scaleMtx.t[0]        = 0;
    if (amount != 0 && amount != 0x1000) {
        scale->vz                    = amount;
        scale->vy                    = amount;
        ((VECTOR*)(head - 0x10))->vx = amount;
        ScaleMatrix(&work->walker.scaleMtx, scale);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// The same matrix reset as `bridge_reset_scale_mtx_entry`, in the statement
/// order the shrink halves of `func_acropolis_bridge_80185F28` and
/// `func_acropolis_bridge_801863A8` use (and the one `bridge_scale_up` uses for
/// the spawn ramp).
static __inline__ void bridge_reset_scale_mtx_shrink(_AcropolisBridgeEnemyWork* work)
{
    BossStrangerWalker* walker;
    u8*                 head;
    VECTOR*             scale;
    s32                 amount;

    walker                   = &work->walker;
    head                     = SCRATCH_STACK_CURSOR(u8);
    walker->scaleMtx.m[2][2] = 0x1000;
    walker->scaleMtx.m[1][1] = 0x1000;
    walker->scaleMtx.m[0][0] = 0x1000;
    amount                   = walker->scale;
    walker->scaleMtx.m[2][1] = 0;
    walker->scaleMtx.m[2][0] = 0;
    walker->scaleMtx.m[1][2] = 0;
    walker->scaleMtx.m[1][0] = 0;
    walker->scaleMtx.m[0][2] = 0;
    walker->scaleMtx.m[0][1] = 0;
    walker->scaleMtx.t[2]    = 0;
    walker->scaleMtx.t[1]    = 0;
    walker->scaleMtx.t[0]    = 0;
    scale                    = (VECTOR*)(head - 0x10);

    SCRATCH_STACK_CURSOR(VECTOR) = scale;
    if (amount != 0 && amount != 0x1000) {
        scale->vz                    = amount;
        scale->vy                    = amount;
        ((VECTOR*)(head - 0x10))->vx = amount;
        ScaleMatrix(&work->walker.scaleMtx, scale);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// Runs the bridge enemy's approach state. On the first frame (work block still
/// live) it parks the walker in step 3, clears the hand-over flag, swaps bit 15
/// between the two behaviour flag words, rebuilds the scale matrix and restarts
/// the animation slots on animation 1. Every frame after that it sinks the
/// model root by 0x2D until `sinkDepth` reaches 0x708, and
/// shrinks the walker by 0x33 a frame down to 0x801 -- rebuilding the scale
/// matrix as it goes -- tagging the enemy's link node on every frame it is
/// already that small, then ticks the walker and the animation slots. The
/// behaviour state becomes 2 once the player is at or above the bridge.
void func_acropolis_bridge_80185F28(Task* task)
{
    _AcropolisBridgeEnemyWork* work;
    BossStrangerWalker*        walker;
    BossStrangerWalker*        walker2;
    Enemy*                     enemy;
    PlayerStatus*              cfg;

    cfg   = &gPlayerStatus;
    work  = (_AcropolisBridgeEnemyWork*)task->work;
    enemy = (Enemy*)task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->walker.state            = BOSS_STRANGER_WALKER_PATROL;
        work->walker.routeData.cursor = 0;
        work->attack.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->body.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        bridge_reset_scale_mtx_entry(work);
        work->walker.turnLimit = 0x60;
        walker                 = &work->walker;
        walker->speedTarget    = 0x20;
        walker->speed          = 0;
        walker->speedStep      = 1;
        work->animRequest      = 2;
        work->animId           = 1;
        work->animRate         = ANIMATION_RATE_ONE;
    }
    if (work->walker.routeData.arrived == 1) {
        walker2              = &work->walker;
        walker2->speedTarget = 0x20;
        walker2->speed       = 0x60;
        walker2->speedStep   = 2;
    }
    if (work->sinkDepth < 0x708) {
        work->sinkDepth += 0x2D;
        task->extra.tmd->coords->coord.t[1] =
            work->baseHeight + work->sinkDepth;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->walker.scale >= 0x801) {
        work->walker.scale -= 0x33;
        bridge_reset_scale_mtx_shrink(work);
    } else {
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    }
    bossStrangerTick(&work->walker);
    func_acropolis_bridge_8018581C(task);
    if (cfg->coordMtx->t[1] >= 0x2BD) {
        work->state = 2;
    }
}

/// Rebuilds the bridge enemy's model matrix for the spawn scale-up. `scale`
/// ramps 0x88 per frame until it reaches 0x1000, and until then the matrix is
/// reset to identity and scaled uniformly by it through a `VECTOR` taken from
/// the scratch stack.
static __inline__ void bridge_scale_up(_AcropolisBridgeEnemyWork* work)
{
    BossStrangerWalker* walker;
    u8*                 head;
    VECTOR*             scale;
    s32                 amount;

    work->walker.scale      += 0x88;
    walker                   = &work->walker;
    head                     = SCRATCH_STACK_CURSOR(u8);
    walker->scaleMtx.m[2][2] = 0x1000;
    walker->scaleMtx.m[1][1] = 0x1000;
    walker->scaleMtx.m[0][0] = 0x1000;
    amount                   = walker->scale;
    walker->scaleMtx.m[2][1] = 0;
    walker->scaleMtx.m[2][0] = 0;
    walker->scaleMtx.m[1][2] = 0;
    walker->scaleMtx.m[1][0] = 0;
    walker->scaleMtx.m[0][2] = 0;
    walker->scaleMtx.m[0][1] = 0;
    walker->scaleMtx.t[2]    = 0;
    walker->scaleMtx.t[1]    = 0;
    walker->scaleMtx.t[0]    = 0;
    scale                    = (VECTOR*)(head - 0x10);

    SCRATCH_STACK_CURSOR(VECTOR) = scale;
    if (amount != 0 && amount != 0x1000) {
        scale->vz                    = amount;
        scale->vy                    = amount;
        ((VECTOR*)(head - 0x10))->vx = amount;
        ScaleMatrix(&work->walker.scaleMtx, scale);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}

/// Reports whether the bridge enemy's hit box has recorded a contact: its
/// one-entry contact table is occupied once something has struck it.
static __inline__ s16 _acropolisBridgeWasHit(Task* task)
{
    if (((_AcropolisBridgeEnemyWork*)task->work)->attackContacts[0].key.value == 0) {
        return 0;
    }
    return 1;
}

/// Runs the bridge enemy's spawn state. On the first frame (work block still
/// live) it tags the link node while `Gp_PackObjPair` rebuilds the enemy's
/// pair table, sets bit 15 of both behaviour flag words, seeds the walker's
/// first patrol step and starts the reset animation. Every frame after that it
/// counts `sinkDepth` down 0x3C at a time -- raising the
/// model root by it while it runs -- scales the model up until it reaches full
/// size, ticks the walker and the animation slots, and finally advances to
/// state 3 once the work block reports it is done, or resets to state 1 when
/// the player has dropped below the bridge.
void func_acropolis_bridge_801861A0(Task* task)
{
    _AcropolisBridgeEnemyWork* work;
    BossStrangerWalker*        walker;
    Enemy*                     enemy;
    PlayerStatus*              cfg;
    u16                        height;

    cfg  = &gPlayerStatus;
    work = (_AcropolisBridgeEnemyWork*)task->work;
    if (work->stateEntered != 0) {
        enemy = (Enemy*)task->spawnArg2.pointer;
        Gp_ArmStateF0(1);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        height                        = work->walker.speed;
        walker                        = &work->walker;
        work->walker.turnLimit        = 0x100;
        walker->speedTarget           = 0xA0;
        walker->speedStep             = 6;
        walker->speed                 = height;
        work->walker.state            = BOSS_STRANGER_WALKER_CHASE;
        work->attack.flags           |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->body.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->attack.key              = Gp_PackObjPair(enemy, 0);
        enemy->node.state.parts.flags = 0;
        work->animRequest             = 2;
        work->animId                  = 2;
        work->animRate                = 5 * ANIMATION_RATE_ONE;
    }
    if (work->sinkDepth > 0) {
        work->sinkDepth -= 0x3C;
        task->extra.tmd->coords->coord.t[1] =
            work->baseHeight + work->sinkDepth;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->walker.scale < 0x1000) {
        bridge_scale_up(work);
    }
    bossStrangerTick(&work->walker);
    func_acropolis_bridge_8018581C(task);
    if (_acropolisBridgeWasHit(task)) {
        work->state = 3;
    }
    if (cfg->coordMtx->t[1] < 0x321) {
        work->state = 1;
    }
}

/// Runs the bridge enemy's retreat state. On the first frame (work block still
/// live) it parks the walker in step 3, clears the hand-over flag, drops bit 15
/// of the hit box's flags, rebuilds the scale matrix and restarts the animation slots
/// on animation 1. Every frame after that it sinks the model root by 0x2D
/// until `sinkDepth` reaches 0x708, shrinks the walker by
/// 0x46 a frame down to 0x500 -- rebuilding the scale matrix as it goes, and
/// once it is that small tagging the enemy's link node instead -- then ticks
/// the walker and the animation slots. When the hand-over flag is set the
/// behaviour state becomes 1 while the player is below the bridge and 2
/// otherwise.
void func_acropolis_bridge_801863A8(Task* task)
{
    _AcropolisBridgeEnemyWork* work;
    BossStrangerWalker*        walker;
    Enemy*                     enemy;
    PlayerStatus*              cfg;

    cfg   = &gPlayerStatus;
    work  = (_AcropolisBridgeEnemyWork*)task->work;
    enemy = (Enemy*)task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->walker.state            = BOSS_STRANGER_WALKER_PATROL;
        work->walker.routeData.cursor = 0;
        work->attack.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        bridge_reset_scale_mtx_entry(work);
        work->walker.turnLimit = 0x200;
        walker                 = &work->walker;
        walker->speedTarget    = 0x20;
        walker->speed          = 0x80;
        walker->speedStep      = 3;
        work->animRequest      = 2;
        work->animId           = 1;
        work->animRate         = ANIMATION_RATE_ONE;
    }
    if (work->sinkDepth < 0x708) {
        work->sinkDepth += 0x2D;
        task->extra.tmd->coords->coord.t[1] =
            work->baseHeight + work->sinkDepth;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    if (work->walker.scale >= 0x500) {
        work->walker.scale -= 0x46;
        bridge_reset_scale_mtx_shrink(work);
    } else if (enemy->node.state.parts.flags == 0) {
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    }
    bossStrangerTick(&work->walker);
    func_acropolis_bridge_8018581C(task);
    if (work->walker.routeData.cursor != 0) {
        if (cfg->coordMtx->t[1] < 0x321) {
            work->state = 1;
        } else {
            work->state = 2;
        }
    }
}

/// Reports whether the bridge enemy is standing on a kind-1 surface: the first
/// three collision records are scanned in order and the scan stops at the first
/// empty one, so an occupied record whose `key` high halfword is 1 has to
/// come before any gap in the table.
static __inline__ s32 bridge_rec_kind1(WorldCollisionContact* recs)
{
    s16 i;

    for (i = 0; i < 3; i++) {
        if (recs[i].key.value == 0) {
            return 0;
        }
        if ((recs[i].key.value & 0xFFFF0000) == 0x10000) {
            return 1;
        }
    }
    return 0;
}

/// Plays one of the bridge enemy's positional sounds. The spawn variant in the
/// enemy's `field_8` high nibble picks the bank, so the event id is that nibble
/// shifted into byte 1 of `base`, and the pan and depth come from the model's
/// root coordinate.
static __inline__ void bridge_play_snd(Task* task, Enemy* enemy, s32 base)
{
    s32 snd;
    s32 pan;

    snd = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | base;
    pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(snd, pan,
                             (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
}

/// Runs the bridge enemy's plunge into the gorge. On the first frame (work
/// block still live) it disables the hit box, clears the enemy's link node tag,
/// seeds the three behaviour parameters and moves the model root out over the
/// gorge -- the X and Z it drops to depend on which of the three spawn variants
/// this is -- then loads the light-blend colour matrix and gives the root a
/// random yaw. Every frame after that the fall height comes from
/// `func_acropolis_bridge_8017E024`, and the model is spun on its own yaw and
/// scaled down once it is past 0x1F4, four units of scale per unit of depth.
/// Three chances to restart the scream animation are rolled on the way down --
/// on crossing 0x1F4, then one in sixteen frames while the animation has run
/// long enough, then one in thirty-two frames below 0x320 with the second
/// animation slot finished -- and the yaw is re-rolled while animation 4 is in
/// its fifth playback step. Landing on a kind-1 surface plays the impact sound
/// and hands over to state 7.
void func_acropolis_bridge_80186618(Task* task)
{
    _AcropolisBridgeEnemyWork* work;
    _AcropolisBridgeEnemyWork* anim;
    Enemy*                     enemy;
    VECTOR                     scale;
    s32                        amount;
    s32                        height;

    work  = (_AcropolisBridgeEnemyWork*)task->work;
    enemy = (Enemy*)task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->attack.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        enemy->node.state.parts.flags = 0;
        work->animRate                = 2 * ANIMATION_RATE_ONE;
        work->animRequest             = 2;
        work->animId                  = 1;
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
    task->extra.tmd->coords->coord.t[1] =
        func_acropolis_bridge_8017E024() - 0xC8;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    height                                = task->extra.tmd->coords->coord.t[1];
    if (height < 0x1F4) {
        amount   = 0x1000;
        scale.vx = scale.vy = scale.vz = amount;
    } else {
        amount   = 0x1000;
        height  -= 0x1F4;
        height  *= 4;
        amount  -= height;
        scale.vx = scale.vy = scale.vz = amount;
    }
    gfxRotMatrixY(&task->extra.tmd->coords->coord, work->yaw, 1);
    ScaleMatrix(&task->extra.tmd->coords->coord, &scale);
    if (task->extra.tmd->coords->coord.t[1] < 0x1F4 &&
        work->animId != 4) {
        work->animRequest = 2;
        work->animId      = 4;
        bridge_play_snd(task, enemy, 0x40290003);
    }
    if (work->animId == 4) {
        if (task->extra.tmd->coords->coord.t[1] >= -0x3DD &&
            (s32)((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) + 8) < work->animFrame) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 16) & 0xF) == 0) {
                work->animRate    = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) * 2) + ANIMATION_RATE_ONE;
                work->animRequest = 2;
                work->animId      = 4;
                bridge_play_snd(task, enemy, 0x40290003);
            }
        }
    }
    if (task->extra.tmd->coords->coord.t[1] < 0x320) {
        anim = (_AcropolisBridgeEnemyWork*)task->work;
        if (anim->rig.slots[1].currentPose.indices.recordIndex == anim->rig.slots[1].nextPose.indices.recordIndex) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 16) & 0x1F) == 0) {
                work->animRate    = ANIMATION_RATE_ONE;
                work->animRequest = 2;
                work->animId      = 4;
                bridge_play_snd(task, enemy, 0x40290003);
            }
        }
    }
    // Animation 4 at its fifth tick, compared as one word.
    if (*(s32*)&work->animId == 0x50004) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->yaw       = gRandomLcgState >> 16;
    }
    if (bridge_rec_kind1(work->bodyContacts) != 0) {
        if (work->hp > 0) {
            bridge_play_snd(task, enemy, 0x40290002);
        }
        work->state = 7;
    }
    func_acropolis_bridge_8018581C(task);
}

/// Runs the bridge enemy's fall at its current position. Seeds the hit-box,
/// animation and colour state on entry, then scales and spins the model by
/// height, rolls chances to restart its scream and enters state 7 on landing.
void func_acropolis_bridge_80186BBC(Task* task)
{
    _AcropolisBridgeEnemyWork* work;
    _AcropolisBridgeEnemyWork* anim;
    Enemy*                     enemy;
    VECTOR                     scale;
    s32                        amount;
    s32                        height;

    work  = (_AcropolisBridgeEnemyWork*)task->work;
    enemy = (Enemy*)task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->attack.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        enemy->node.state.parts.flags = 0;
        work->animRate                = 2 * ANIMATION_RATE_ONE;
        work->animRequest             = 2;
        work->animId                  = 1;
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
    height                                = task->extra.tmd->coords->coord.t[1];
    if (height < 0x1F4) {
        amount   = 0x1000;
        scale.vx = scale.vy = scale.vz = amount;
    } else {
        amount   = 0x1000;
        height  -= 0x1F4;
        height  *= 4;
        amount  -= height;
        scale.vx = scale.vy = scale.vz = amount;
    }
    gfxRotMatrixY(&task->extra.tmd->coords->coord, work->yaw, 1);
    ScaleMatrix(&task->extra.tmd->coords->coord, &scale);
    if (task->extra.tmd->coords->coord.t[1] < 0x1F4 &&
        work->animId != 4) {
        work->animRequest = 2;
        work->animId      = 4;
        bridge_play_snd(task, enemy, 0x40290003);
    }
    if (work->animId == 4) {
        if (task->extra.tmd->coords->coord.t[1] >= -0x3DD &&
            (s32)((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) + 8) < work->animFrame) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 16) & 0xF) == 0) {
                work->animRate    = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) * 2) + ANIMATION_RATE_ONE;
                work->animRequest = 2;
                work->animId      = 4;
                bridge_play_snd(task, enemy, 0x40290003);
            }
        }
    }
    if (task->extra.tmd->coords->coord.t[1] < 0x320) {
        anim = (_AcropolisBridgeEnemyWork*)task->work;
        if (anim->rig.slots[1].currentPose.indices.recordIndex == anim->rig.slots[1].nextPose.indices.recordIndex) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 16) & 0x1F) == 0) {
                work->animRate    = ANIMATION_RATE_ONE;
                work->animRequest = 2;
                work->animId      = 4;
                bridge_play_snd(task, enemy, 0x40290003);
            }
        }
    }
    // Animation 4 at its fifth tick, compared as one word.
    if (*(s32*)&work->animId == 0x50004) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->yaw       = gRandomLcgState >> 16;
    }
    if (bridge_rec_kind1(work->bodyContacts) != 0) {
        if (work->hp > 0) {
            bridge_play_snd(task, enemy, 0x40290002);
        }
        work->state = 7;
    }
    func_acropolis_bridge_8018581C(task);
}

/// Runs the bridge enemy's fall. On the first frame (work block still live) it
/// clears bit 15 of the hit box's flags and sets it in the model's, tags the link node,
/// seeds the three behaviour parameters and gives the model root coordinate a
/// random yaw from the shared LCG. Every frame after that it eases
/// `sinkDepth` back to zero three units at a time, sets the model
/// root height from it and adds an `rsin` bob driven by the frame counter. Once
/// the enemy is resting on a kind-1 surface it also drifts the model root
/// towards the camera position by a sixteenth of the normalized direction,
/// yawing the root by 0x10 first.
void func_acropolis_bridge_80187078(Task* task)
{
    _AcropolisBridgeEnemyWork* work;
    Enemy*                     enemy;
    GfxCoord*                  coord;
    SVECTOR                    dir;
    SVECTOR*                   d;

    work  = (_AcropolisBridgeEnemyWork*)task->work;
    enemy = (Enemy*)task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->attack.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->body.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->animRequest             = 1;
        work->animId                  = 3;
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
    if (bridge_rec_kind1(work->bodyContacts) != 0) {
        coord  = task->extra.tmd->coords;
        dir.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
        d      = &dir;
        d->vy  = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
        d->vz  = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
        gfxRotMatrixY(&task->extra.tmd->coords->coord, 0x10, 0);
        VectorNormalSS(d, d);
        gte_lddp(-0x10);
        gte_ldsv(d);
        gte_gpf12();
        gte_stsv(d);
        task->extra.tmd->coords->coord.t[0] += dir.vx;
        task->extra.tmd->coords->coord.t[2] += dir.vz;
    }
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    func_acropolis_bridge_8018581C(task);
}

/// Runs the bridge enemy's collapse sequence. On the first frame (work block
/// still live) it allocates the model's aux buffers, clears bit 15 of both
/// behaviour flag words, tags the link node, seeds the three behaviour
/// parameters, gives the model root coordinate a random yaw from the shared
/// LCG, drops the enemy's actor slots, arms the pending `gSceneCombatState` request
/// and restarts the frame counter. Every frame after that it ticks the counter
/// up to 100, runs `func_acropolis_bridge_8018581C` and, on frames 10, 22, 28
/// and 34, steps the light mode and model flags through the fade-out.
void func_acropolis_bridge_80187310(Task* task)
{
    _AcropolisBridgeEnemyWork* work  = (_AcropolisBridgeEnemyWork*)task->work;
    Enemy*                     enemy = (Enemy*)task->spawnArg2.pointer;
    s32                        step;

    if (work->stateEntered != 0) {
        tmdAllocPrimitiveBuffer(task->extra.tmd);
        work->attack.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->animRequest             = 1;
        work->animId                  = 5;
        work->animRate                = ANIMATION_RATE_ONE;
        gRandomLcgState               = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gfxRotMatrixY(&task->extra.tmd->coords->coord, gRandomLcgState >> 16, 1);
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_ClearNodeSlots(&enemy->node);
        if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_IDLE && gSceneCombatState.battleRefs != 0) {
            Gp_ArmStateF0(1);
        }
        work->deathFrame = 0;
    }
    if (work->deathFrame < 0x65) {
        work->deathFrame++;
        func_acropolis_bridge_8018581C(task);
        step = work->deathFrame;
        switch (step) {
            case 10:
                Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
                Gp_SpawnEff(EFFECT_CORPSE_BURN, &task->extra.tmd->coords[2], 1, NULL);
                break;
            case 28:
                task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                break;
            case 22:
                Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
                break;
            case 34:
                task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                break;
        }
    }
}

/// Runs the bridge enemy's death sequence. On the first frame (work block still
/// live) it clears bit 15 of both behaviour flag words, tags the link node and
/// drops its actor slots, arms the pending `gSceneCombatState` request, credits the
/// kill if the enemy still had HP, spawns the death effect on the model's
/// second part coordinate, switches the model to light mode 1, shakes the pad
/// and restarts the frame counter. Every frame after that it ticks the counter
/// up to 100, runs `func_acropolis_bridge_8018581C` and, on frames 2, 30 and
/// 44, steps the model flags / light mode through the fade-out.
void func_acropolis_bridge_801874DC(Task* task)
{
    _AcropolisBridgeEnemyWork* work  = (_AcropolisBridgeEnemyWork*)task->work;
    Enemy*                     enemy = (Enemy*)task->spawnArg2.pointer;
    s32                        step;

    if (work->stateEntered != 0) {
        work->attack.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        Gp_ClearNodeSlots(&enemy->node);
        if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_IDLE && gSceneCombatState.battleRefs != 0) {
            Gp_ArmStateF0(1);
        }
        if (enemy->hp > 0) {
            Gp_ReleaseStateF0Add(task, 0x29);
        }
        work->effectArg.coord      = &task->extra.tmd->coords[1];
        work->effectArg.spawnArgLo = 0xA0;
        work->effectArg.spawnArgHi = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &task->extra.tmd->coords[1], NULL,
                      &work->effectArg);
        Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
        Gp_SpawnEff(EFFECT_CORPSE_BURN, &task->extra.tmd->coords[1], 1, NULL);
        task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
        work->deathFrame       = 0;
        Gp_SpawnPadLerp(3, 0xFF, 8);
    }
    if (work->deathFrame < 0x65) {
        work->deathFrame++;
        func_acropolis_bridge_8018581C(task);
        step = work->deathFrame;
        switch (step) {
            case 2:
                task->extra.tmd->flags = step;
                break;
            case 30:
                Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
                break;
            case 44:
                task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                break;
        }
    }
}

/// Applies one hit to the bridge enemy. While the work block still has HP it
/// rolls the damage for the incoming attack id, spawns the hit effect on the
/// model's second part coordinate, quadruples the damage on a critical roll,
/// credits it to the kill tally and the link node, and subtracts it from both
/// the work block's and the enemy's HP; the pending `gSceneCombatState` request is
/// armed once the HP runs out. When there is no HP left to take (before or
/// after the hit) it steps the behaviour state instead: 5 and 6 are already
/// reaction states and stay put, 4 and 8 advance to 6, everything else resets
/// to 5.
static void func_acropolis_bridge_801876A8(Task* task, u32 attackId)
{
    _AcropolisBridgeEnemyWork* work  = (_AcropolisBridgeEnemyWork*)task->work;
    Enemy*                     enemy = (Enemy*)task->spawnArg2.pointer;
    s32                        damage;
    s16                        state;

    if (work->hp > 0) {
        damage                     = Gp_ComputeDamage(attackId, 0, 0, 0x1000);
        work->effectArg.coord      = &task->extra.tmd->coords[1];
        work->effectArg.spawnArgLo = 0x80;
        work->effectArg.spawnArgHi = 2;
        func_800FDB18(Gp_GetIdParam1(attackId) & 0xFFFF, &task->extra.tmd->coords[1],
                      NULL, &work->effectArg);
        if (Gp_RollEnemyChance(enemy, attackId, 0) != 0) {
            damage *= 4;
            Gp_SpawnEff(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[1], 0, NULL);
        }
        func_800E2C78(enemy, attackId, damage, 0);
        enemy->hp -= damage;
        func_800DA6E8(&enemy->node, damage, 0);
        work->hp -= damage;
        enemy->hp = work->hp;
        if (work->hp > 0) {
            return;
        }
        if (gSceneCombatState.battleRefs != 0) {
            Gp_ReleaseStateF0Add(task, 0x29);
        }
        if (work->hp > 0) {
            return;
        }
    }
    state = work->state;
    if (state < 7) {
        if (state >= 5) {
            return;
        }
        if (state != 4) {
            work->state = 5;
            return;
        }
        work->state = 6;
    } else {
        if (state != 8) {
            work->state = 5;
            return;
        }
        work->state = 6;
    }
}

/// Ticks the bridge enemy once per frame. It refreshes the model's root
/// coordinate and relights it, then branches on the global pause mode
/// `gSceneCombatState.actorControl`: mode 1 only releases the collision records, mode 2 also hides
/// the mesh, and mode 0 keeps the model's visibility in step with the camera
/// -- re-allocating or releasing the TMD's aux buffers when the view changes,
/// and remembering the view it last synced to in `syncedView`. Outside the
/// death and cleanup states it then borrows an `_AcropolisBridgeHitScratch`, scans
/// the body's three collision records for a hit (high halfword 0x2), applies
/// it through `func_acropolis_bridge_801876A8`, raises `stateEntered` on the frame
/// the behaviour state changes, runs the state's handler from
/// `D_acropolis_bridge_8019175C`, clears both record tables and -- while no
/// `gSceneCombatState` request is pending -- resets any state other than 5 or 6 back
/// to 0.
static void func_acropolis_bridge_80187850(Enemy* enemy, Task* task)
{
    _AcropolisBridgeEnemyWork*  work;
    _AcropolisBridgeEnemyWork*  cur;
    _AcropolisBridgeHitScratch* block;
    TmdObject*                  extra;
    WorldCollisionContact*      recs;
    VECTOR                      pos;
    s32                         mode;
    s32                         view;
    s32                         hit;
    u16                         state;
    s16                         i;

    work                                  = (_AcropolisBridgeEnemyWork*)task->work;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
    pos.vx = task->extra.tmd->coords->workm.t[0];
    pos.vy = task->extra.tmd->coords->workm.t[1];
    pos.vz = task->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    mode = gSceneCombatState.actorControl;
    if (mode == 1) {
        goto paused;
    }
    if (mode >= 2) {
        goto ge2;
    }
    if (mode == 0) {
        goto running;
    }
    goto body;
ge2:
    if (mode == 2) {
        goto hidden;
    }
    goto body;

running:
    state = (u16)work->state;
    if ((u32)(state - 6) >= 2U) {
        if (state != 0) {
            view = Gp_GetViewIndex() & 0xFF;
            switch (view) {
                case 8:
                    if ((s32)work->syncedView == view) {
                        goto drop;
                    }
                    task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    goto resync;
                case 22:
                    if (work->state == 4) {
                        goto draw;
                    }
                drop:
                    task->extra.tmd->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                    extra                   = task->extra.tmd;
                    if (extra->buffer != NULL) {
                        tmdFreePrimitiveBuffer(extra);
                    }
                    goto resync;
                default:
                    tmdAllocPrimitiveBuffer(task->extra.tmd);
                draw:
                    task->extra.tmd->flags = 0;
                    break;
            }
        resync:
            work->syncedView = Gp_GetViewIndex() & 0xFF;
        }
    }
    goto body;

paused:
    state = (u16)work->state;
    if ((u32)(state - 6) >= 2U && state != 0) {
        Gp_ClearRec18Occupied(&work->bodyContacts[0]);
        Gp_ClearRec18Occupied(&work->attackContacts[0]);
    }
    return;

hidden:
    task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    Gp_ClearRec18Occupied(&work->bodyContacts[0]);
    Gp_ClearRec18Occupied(&work->attackContacts[0]);
    return;

body:
    block = SCRATCH_STACK_RESERVE_BLOCK(_AcropolisBridgeHitScratch);
    recs  = work->bodyContacts;
    i     = 0;
    do {
        if (recs[i].key.value == 0) {
            goto missed;
        }
        if ((recs[i].key.value & 0xFFFF0000) == 0x20000) {
            block->point.vx = recs[i].point.vx;
            block->point.vy = recs[i].point.vy;
            block->point.vz = recs[i].point.vz;
            hit             = recs[i].key.value;
            goto hitTaken;
        }
        i++;
    } while (i < 3);
missed:
    hit = 0;
hitTaken:
    block->key = hit;
    if (hit != 0) {
        func_acropolis_bridge_801876A8(task, hit);
    }

    cur = (_AcropolisBridgeEnemyWork*)task->work;
    if (cur->prevState != cur->state) {
        cur->stateEntered = 1;
    } else {
        cur->stateEntered = 0;
    }
    cur->prevState = cur->state;
    D_acropolis_bridge_8019175C[work->state](task);
    Gp_ClearRec18Occupied(&work->bodyContacts[0]);
    Gp_ClearRec18Occupied(&work->attackContacts[0]);
    if (gSceneCombatState.battleRefs == 0) {
        if ((u32)((u16)work->state - 5) >= 2U) {
            work->state = 0;
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_AcropolisBridgeHitScratch);
}

/// Applies a visibility request to the bridge task's model flags: no request
/// restores the default flag set, bit 0 hides the mesh outright and bit 1 adds
/// the "skip drawing" bit to whatever flags are already set. Always reports
/// success.
s32 func_acropolis_bridge_80187BD0(Task* task, s32 arg1, s32 flags, s32 arg3)
{
    TmdObject* extra;

    extra = task->extra.tmd;
    if (flags == 0) {
        extra->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else if (flags & 1) {
        extra->flags = 0;
    } else if (flags & 2) {
        extra->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 1;
}

/// Relights the bridge enemy's model. Borrows a `VECTOR` from the scratchpad
/// arena, optionally refreshes the TMD's root coordinate first (`arg1 == 1`),
/// then feeds that part's world translation to `worldCoordSetModelLighting` so the object's
/// colour matrix is rebuilt for its current position, and releases the scratch.
static void func_acropolis_bridge_80187C10(Task* task, s16 arg1)
{
    void**  scratch;
    u8*     head;
    VECTOR* pos;

    scratch  = SCRATCH_STACK_CURSOR_SLOT;
    head     = *scratch;
    pos      = (VECTOR*)(head - 0x10);
    *scratch = pos;
    if (arg1 == 1) {
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(task->extra.tmd->coords);
    }
    ((VECTOR*)(head - 0x10))->vx = task->extra.tmd->coords->workm.t[0];
    pos->vy                      = task->extra.tmd->coords->workm.t[1];
    pos->vz                      = task->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(task->extra.tmd, pos, 0, 3);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Shuts the bridge enemy's animation down. While the work block is still live
/// it hides the mesh behind the default flag set, tags the enemy's link node
/// and clears bit 15 of both behaviour flag words; once the work block has been
/// cleared it instead adds the "skip drawing" bit and releases the TMD's aux
/// buffers.
void func_acropolis_bridge_80187D04(Task* task)
{
    _AcropolisBridgeEnemyWork* work  = (_AcropolisBridgeEnemyWork*)task->work;
    TmdObject*                 extra = task->extra.tmd;

    if (work->stateEntered != 0) {
        Enemy* enemy = (Enemy*)task->spawnArg2.pointer;

        extra->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->attack.flags           &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        return;
    }
    extra->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    if (extra->buffer != NULL) {
        tmdFreePrimitiveBuffer(extra);
    }
}

/// Runs the bridge enemy's current state handler - setup, per-frame tick or
/// teardown - copying the table onto the stack before the call.
void func_acropolis_bridge_80187D80(Task* task)
{
    EnemyTaskFuncTable3 sp;

    sp = D_acropolis_bridge_8017D6E8;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}
