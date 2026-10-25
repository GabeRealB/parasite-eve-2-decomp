#include "actors/actor_560800.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/display.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflow.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/shelter_b1_pod_service_gantry.h"
#include "../../shared/actor/actor_messages.h"
#include "../../shared/screen/screen_fade.h"
#include "../../shared/screen/screen_fade_step_down.inc.c"
#include "../../shared/screen/screen_fade_step_up.inc.c"

#include "assets/engine/animation/player_schema.h"

/// Work block of the task that runs the package's cutscene, allocated zeroed at
/// its full size by that task's first state and kept at its `Task::work`.
///
/// The scene plays Aya Brea, Eve, Kyle Madigan and No. 9 against one event
/// script. The block holds the cast - the player's own task and the model
/// tasks this package spawns for the others - and one cue per actor, through
/// which the script tells each of them what to do in the current cut. Every
/// spawned task hangs below the cutscene task in the teardown tree and dies
/// with it; the player's task is borrowed and outlives the scene.
///
/// Cues 1 to 33 are the scene's cuts, posted in order to the whole cast as each
/// cut opens. 35 to 39 are posted to one actor in the middle of a cut, and are
/// numbered per actor: 35 is one cue for Aya and another for Kyle. The scene's
/// own cue is numbered separately again. A handler acts on the cues that
/// concern its actor and clears the rest unhandled.
///
/// The player keeps the animation player of an ordinary actor, so the scene
/// tracks which of its own animations Aya is in and chains the next one itself.
/// The other three run animation scripts inside their own tasks.
typedef struct {
    Task*            player;            // The player's task, playing Aya; borrowed, never killed here
    Task*            eve;               // Eve's body: the masked model at first, replaced by a second model with its own texture when the scene cue asks; names a dead task for the few frames the swap takes
    Task*            kyle;              // Kyle Madigan's body
    Task*            no9;               // No. 9's body
    Task*            kyleGunHand;       // Kyle's hand model on the body part that also carries the gun
    Task*            kyleFreeHand;      // Kyle's other hand model
    Task*            kyleGun;           // Kyle's handgun model; may be NULL, and every use checks
    Task*            no9Gunblade;       // No. 9's gunblade model; may be NULL, and every use checks
    Task*            chainGroup;        // Task driving the eight jointed chain models, which follow a part of Eve's or No. 9's body; takes `ActorCommand`s
    Task*            carrierModel;      // Single pulsing model that travels vertically and ends by carrying No. 9's body off with it; takes `ActorCommand`s
    ActorCutsceneCue playerCue;         // Aya's cue: animations, slides and the decals spawned at her feet
    ActorCutsceneCue no9Cue;            // No. 9's cue: animation changes
    ActorCutsceneCue eveCue;            // Eve's cue: animation changes, and hiding the chain group; `step` and `counter` are never read
    ActorCutsceneCue kyleCue;           // Kyle's cue: animations, turns of the body, the two shots and showing or hiding him with his hands and gun
    byte             unknown_48[0x10];  // Never accessed; the size of two more cues, but nothing shows they are cues
    ActorCutsceneCue sceneCue;          // Cue for the scene itself (1 replaces Eve's model); only `id` is ever read
    u16              playerAnimId;      // Animation Aya is playing, an index into the package's player animation sets and into the script that names its successor
    s16              playerAnimHold;    // Frames the current player animation has been held, for a script step that lasts a fixed time
    u16              shotDamageApplied; // 1 once the shot has cost the player 50 HP, so skipping the scene applies it exactly once
    s16              keepEffects;       // 1 while opening a cut must leave the room's running effects alone; 0 lets each new cut cancel them all
} _Actor560800CutsceneWork;
STATIC_ASSERT_SIZEOF(_Actor560800CutsceneWork, 0x68);

/// Work block of one of the cast's models: Eve's, Kyle Madigan's and No. 9's
/// bodies, and the hands, handgun and gunblade that ride on a body's part.
///
/// Each of those tasks allocates the block zeroed at its full size and keeps
/// it at `Task::work`; the model object borrows `light` and `color` for as
/// long as the block lives. A body binds `rig` to its model and plays one clip
/// at a time over every slot it drives, following `animChain` from clip to
/// clip, and the cutscene's cue handlers change the clip from outside. An
/// attachment hangs from a part of its body and moves with it: it uses the two
/// matrices and reads `relit`, which nothing sets for it, and leaves the rest
/// zero.
///
/// Only Kyle's task reads the three turn angles and `animPaused`. The angles
/// are composed onto his parts after his clip is applied. A running clip
/// rewrites the parts every frame, so an angle lasts until it is changed;
/// while `animPaused` holds the pose the angles are applied once and then
/// cleared, since they would otherwise accumulate. Eve's cue handler stores to
/// `part4Yaw` and `animPaused` in her block as well, to no effect.
typedef struct {
    ActorAnimRig20      rig;             // Playback storage; Kyle uses all twenty slots, Eve and No. 9 the first nineteen
    MATRIX              light;           // Light-direction matrix lent to the model object
    MATRIX              color;           // Light-colour matrix lent to the model object
    ActorAnimChainLink* animChain;       // The body's chain of clips: one link per clip, indexed by `animId`
    u16                 animId;          // Clip the slots were last seeded with, an index into the body's animation sets
    u16                 slotCount;       // Rig slots the body uses, slot 0 included: slots 1 to `slotCount` - 1 are seeded and ticked (20 for Kyle, 19 for Eve and No. 9)
    u16                 relit;           // Nonzero while the model is lit afresh each frame from the room's lights at the place it stands
    u16                 animHold;        // Frames `animId` has been held, for a link that lasts a fixed time
    s16                 part4Yaw;        // Kyle: turn about Y composed onto his part 4, 4096 to a turn
    s16                 floorQuadHidden; // No. 9: nonzero stops the square drawn on the floor below the body
    s16                 part2Roll;       // Kyle: turn about Z composed onto his part 2, 4096 to a turn
    s16                 part4Pitch;      // Kyle: turn about X composed onto his part 4 after `part4Yaw`, 4096 to a turn
    s16                 animRate;        // Playback rate named by the last change of clip, `ANIMATION_RATE_ONE` for normal speed; a restart gives it to the slots, a blend only records it, and the chain passes it on to the next link
    s16                 animPaused;      // Kyle: nonzero holds the pose - the slots are not ticked and the chain does not advance
} _Actor560800CastWork;
STATIC_ASSERT_SIZEOF(_Actor560800CastWork, 0x4CC);

/// Work block of one of the scene's three prop models: a jointed chain, the
/// copy of a chain that falls away once the chain is broken, and the carrier.
///
/// Each of those tasks allocates the block zeroed at its full size and keeps
/// it at `Task::work`; the model object borrows `light` and `color` for as
/// long as the block lives. The three use different members, and what one
/// leaves alone stays zero unless noted.
///
/// A chain is a seven-part model, one of eight its group spawns and numbers.
/// Its root sits at `position` plus `offset`. While it bends, each part's
/// rotation is rebuilt every frame from `rot`: the root sways, and parts 1 to
/// 5 turn a step at a time towards the body part the group follows. A group
/// command instead plays the chain's own clip over `rig`, or breaks the chain,
/// which spawns the falling copy in the chain's pose and ends the chain's
/// task.
///
/// The falling copy keeps the pose it was given, turns parts 3 to 5 further by
/// `swing`, and moves along Y by a `speed` that grows every frame until it has
/// gone far enough to be removed.
///
/// The carrier is a single model that travels along Y while its second part
/// is scaled by `pulseScale`, and that ends the scene by moving off together
/// with No. 9's body.
typedef struct {
    ActorAnimRig7 rig;            // Chain: playback storage for its clip, bound at spawn; slot 0, the root's, is never started
    MATRIX        light;          // Light-direction matrix lent to the model object
    MATRIX        color;          // Light-colour matrix lent to the model object
    SVECTOR       rot[7];         // Chain and falling copy: rotation of the model part at the same index, 4096 to a turn
    SVECTOR       swing[7];       // Falling copy: rotation added about X and Z to parts 3 to 5, on top of `rot`
    SVECTOR       offset;         // Chain: displacement of the root from `position`; only Y moves, dipping when the tip comes near the target and easing back
    SVECTOR       position;       // Chain: where the last placement put the root, in its parent's space; the group steps its Y while the chains travel to the first arrangement
    u16           swingDir[7];    // Falling copy: per part, bit 0 would make `swing`'s X rise and bit 1 its Z; cleared at spawn and never set, so both fall by 20 a frame
    Task*         parent;         // Task named at spawn, below which this one hangs: the chain group for a chain or a falling copy, the cutscene for the carrier
    u32           swayPhase;      // Chain: random draw taken at spawn that offsets the phase of the root's sway, so the eight do not sway in step
    u32           field_274;      // Chain: second random draw taken at spawn; never read, role unproven
    s16           pulseScale;     // Carrier: X and Z scale of its second part, `ONE` for the model's own size; each of its states grows it towards 0x1800 or shrinks it. A chain stores a ten-bit random draw here and never reads it
    byte          unknown_27A[2]; // Never accessed
    s16           pulseGrowing;   // Carrier: direction of the next step of `pulseScale` (0 shrinking, 1 growing), set by the state before each step. A placement stores 1 in a chain's when it uses the third table, else 0, and no chain reads it
    s16           dipStep;        // Chain: step of the dip in `offset` (0 waiting for the tip to come near the target, 1 dipping, 2 easing back)
    s16           chainNumber;    // Chain: which of the eight it is, 1 to 8. Also the clip it plays, and what decides whether a state or view hides it; odd-numbered chains turn and sway at twice the step. A falling copy holds the carrier's Y here instead, unread
    s16           carryStep;      // Carrier: step of its closing sequence (0 Y advances by 100 a frame to -3000, 1 on to -1200 while the part grows, 2 the part shrinks back below the model's own size, 3 the carrier and No. 9's body both move by -20 a frame)
    byte          unknown_284[2]; // Never accessed
    s16           speed;          // Carrier and falling copy: units the root moves along Y each frame; raised as it goes
    s16           fallDistance;   // Falling copy: distance moved so far
    s16           fallStartY;     // Falling copy: Y of its root when its task started
} _Actor560800PropWork;
STATIC_ASSERT_SIZEOF(_Actor560800PropWork, 0x28C);

/// Body the chains reach for, kept in
/// `_Actor560800ChainGroupWork::targetEntryId`. The values are the entry ids
/// the bodies' placement records carry in the area layout.
enum {
    ACTOR_560800_CHAIN_TARGET_NO9 = 0x22, // No. 9's body
    ACTOR_560800_CHAIN_TARGET_EVE = 0x83, // Eve's body
};

/// Work block of the task that owns the scene's eight chains, allocated zeroed
/// at its full size and kept at `Task::work`.
///
/// The group spawns the chains, numbers them 1 to 8 and holds their tasks. It
/// has no model of its own: a placement message arranges the chains from one
/// of five tables, a draw message shows or hides them all, and the cutscene's
/// commands choose the next arrangement and the body to reach for, set the
/// chains travelling or playing their clips, and break them one at a time.
/// Every frame it records where the body part the chains reach for is, which
/// each chain reads back through its `parent`.
typedef struct {
    MATRIX targetWorld;     // World matrix of part 9 of the body `targetEntryId` names, with 0x78 taken off the Y of its translation; the chains turn towards the translation, which is not valid until a command has named a body
    Task*  chains[8];       // The chains, in spawn order; NULL once one has been broken or removed
    Task*  cutscene;        // The cutscene task, named at spawn: the group hangs below it and reads the cast's bodies from its work block
    s16    field_44;        // Cleared by the command that sets the chains travelling; never read, role unproven
    s16    placeMode;       // Arrangement the next placement makes (0-2 the chains at the first, second or third table's positions under the group's root; 3 each chain freed into view space at the placement plus the fourth table's position; 4 chains 5 to 8 removed and the rest hung below the carrier in the fifth table's pose); every placement returns it to 0
    s16    placeResetsBend; // Nonzero when the next placement in modes 0-2 also loads each chain's root rotation from the table and straightens its other parts; cleared by that placement
    s16    targetEntryId;   // Body the chains reach for (0 none yet, `ACTOR_560800_CHAIN_TARGET_EVE`, `ACTOR_560800_CHAIN_TARGET_NO9`)
} _Actor560800ChainGroupWork;
STATIC_ASSERT_SIZEOF(_Actor560800ChainGroupWork, 0x4C);

/// Scratch block of a chain's bend, on the scratch stack for one walk down
/// its parts.
///
/// The walk goes from the root outwards. For each part it works out where the
/// next joint lies with the rotations set so far, measures the bearing from
/// there to the target, and turns the part one step towards it; angles are
/// 4096 to a turn. The block is cleared when it is reserved.
typedef struct {
    MATRIX  chain;            // Rotation accumulated from the root through the parts already turned
    MATRIX  link;             // `chain` times the current part's rotation before it is turned
    SVECTOR pos;              // Position of the current part's joint, summed from the root's
    SVECTOR ang;              // Sum of `rot` over the root and the parts already turned
    SVECTOR aim;              // `ang` plus the current part's rotation; its X then becomes the error against the bearing to the target
    SVECTOR joint;            // The next part's joint, carried through `link` to a position and then through `chain` to the step added to `pos`
    byte    unknown_60[0x10]; // Never accessed
    SVECTOR rot[7];           // Working copy of `_Actor560800PropWork::rot`, copied in before the walk and back out after it
} _Actor560800ChainScratch;
STATIC_ASSERT_SIZEOF(_Actor560800ChainScratch, 0xA8);

// Indices in the package's cast and prop descriptor tables.
enum {
    ACTOR_560800_TASK_FADE_IN          = 2,
    ACTOR_560800_TASK_FADE_OUT         = 3,
    ACTOR_560800_TASK_EVE_MASKED       = 4,
    ACTOR_560800_TASK_KYLE             = 5,
    ACTOR_560800_TASK_NO9              = 6,
    ACTOR_560800_TASK_KYLE_GUN_HAND    = 7,
    ACTOR_560800_TASK_KYLE_FREE_HAND   = 8,
    ACTOR_560800_TASK_KYLE_GUN         = 9,
    ACTOR_560800_TASK_NO9_GUNBLADE     = 10,
    ACTOR_560800_TASK_EVE_REPLACEMENT  = 11,
    ACTOR_560800_TASK_REPLACE_EVE      = 12,
    ACTOR_560800_TASK_AWAIT_PLAYBACK   = 13,
    ACTOR_560800_PROP_TASK_CHAIN_GROUP = 0,
    ACTOR_560800_PROP_TASK_CHAIN       = 1,
    ACTOR_560800_PROP_TASK_CARRIER     = 2,
    ACTOR_560800_CAST_INITIALIZE       = 0,
    ACTOR_560800_EVE_PLACE_REPLACEMENT = 1,
    ACTOR_560800_EVE_ACTIVE            = 2,
    ACTOR_560800_KYLE_RIG_SLOTS        = 20,
    ACTOR_560800_EVE_NO9_RIG_SLOTS     = 19,
};

// Cast shadow dimensions use coordinate units; lighting queries all three rows.
enum {
    ACTOR_560800_GROUND_SHADOW_SIDE    = 0x300,
    ACTOR_560800_GROUND_SHADOW_LOCAL_Y = 0x380,
    ACTOR_560800_MODEL_LIGHT_COUNT     = 3,
};

// Attachment spawnArg1 selects both the parent part and texture source.
enum {
    ACTOR_560800_ATTACHMENT_FREE_HAND = 0,
    ACTOR_560800_ATTACHMENT_GUN_HAND  = 1,
    ACTOR_560800_ATTACHMENT_GUN       = 2,
    ACTOR_560800_ATTACHMENT_GUNBLADE  = 3,
    ACTOR_560800_KYLE_PLACEMENT_ENTRY = 0x65,
};

// Chain-group commands are borrowed ActorCommand records; only command is read.
enum {
    ACTOR_560800_CHAIN_COMMAND_RELIGHT       = 0,
    ACTOR_560800_CHAIN_COMMAND_TARGET_EVE    = 1,
    ACTOR_560800_CHAIN_COMMAND_EXTEND        = 2,
    ACTOR_560800_CHAIN_COMMAND_THIRD_POSE    = 3,
    ACTOR_560800_CHAIN_COMMAND_FIRST_POSE    = 4,
    ACTOR_560800_CHAIN_COMMAND_DETACHED_CLIP = 5,
    ACTOR_560800_CHAIN_COMMAND_CARRIER_POSE  = 6,
    ACTOR_560800_CHAIN_COMMAND_TARGET_NO9    = 7,
    ACTOR_560800_CHAIN_COMMAND_BREAK_NEXT    = 8,
};

enum {
    ACTOR_560800_CHAIN_GROUP_INITIALIZE = 0,
    ACTOR_560800_CHAIN_GROUP_FOLLOW     = 1,
    ACTOR_560800_CHAIN_GROUP_EXTEND     = 2,
    ACTOR_560800_CHAIN_GROUP_BREAK_NEXT = 3,
    ACTOR_560800_CHAIN_BEND             = 1,
    ACTOR_560800_CHAIN_START_CLIP       = 2,
    ACTOR_560800_CHAIN_BREAK            = 4,
};

// Carrier command values shared by the scene stream and its prop receiver.
enum {
    ACTOR_560800_CARRIER_RELIGHT        = 0,
    ACTOR_560800_CARRIER_HIDE           = 1,
    ACTOR_560800_CARRIER_FIRST_DESCENT  = 2,
    ACTOR_560800_CARRIER_SLOW_DESCENT   = 3,
    ACTOR_560800_CARRIER_RISE           = 4,
    ACTOR_560800_CARRIER_SECOND_DESCENT = 5,
    ACTOR_560800_CARRIER_CARRY_NO9      = 6,
};

enum {
    ACTOR_560800_SCENE_PHASE_FIRST  = 1,
    ACTOR_560800_SCENE_PHASE_SECOND = 2,
    ACTOR_560800_SCENE_PHASE_THIRD  = 3,
    ACTOR_560800_PHASE_CAP_COMMAND  = 1,
};

static s32 _gActor560800FirstPhaseElapsedTicks;
static s32 _gActor560800SecondPhaseElapsedTicks;
static s32 _gActor560800ThirdPhaseElapsedTicks;

static s32 _gActor560800FirstPhaseStartTick;
static s32 _gActor560800SecondPhaseStartTick;
static s32 _gActor560800ThirdPhaseStartTick;

static u32 _gActor560800SavedRandomState;

static Task* _gActor560800CarrierTask;

static void _actor560800HandlePlayerCue(Task* task);
static void _actor560800HandleEveCue(Task* task);
static void _actor560800HandleNo9Cue(Task* task);
static void _actor560800HandleKyleCue(Task* task);

// Message-table callbacks use the argument views required by this TU.

static s32 _actor560800TickCastAnimationChain(Task* task);

static AnimationSet _gActor560800Animation421C4;
static AnimationSet _gActor560800Animation42474;
static AnimationSet _gActor560800Animation42720;
static AnimationSet _gActor560800Animation429CC;
static AnimationSet _gActor560800Animation42C7C;
static AnimationSet _gActor560800Animation42F44;
static AnimationSet _gActor560800Animation43200;
static AnimationSet _gActor560800Animation434A0;

static TmdSource _gActor560800Model40064;
static TmdSource _gActor560800Model40E78;
static TmdSource _gActor560800Model41AC4;
static void      _actor560800IntactChainTask(Task* task);
static void      _actor560800FallingChainTask(Task* task);
static void      _actor560800PlaceChainGroup(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArg);
static void      _actor560800ApplyChainGroupCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);
static void      _actor560800ChainGroupTask(Task* task);
static void      _actor560800ApplyCarrierCommand(Task* task, s32 messageId, ActorCommand* command, s32 unusedArg);
static void      _actor560800CarrierTask(Task* task);
static void      _actor560800SetChainGroupDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg);
static void      _actor560800SetCarrierModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg);
static void      _actorMsgPlaceYawPitchRollCarrier(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArg);

static AnimationSet _gActor560800Animation1EE00;
static AnimationSet _gActor560800Animation1F0F8;
static AnimationSet _gActor560800Animation1F2E4;
static AnimationSet _gActor560800Animation1F6B8;
static AnimationSet _gActor560800Animation20810;
static AnimationSet _gActor560800Animation20D5C;
static AnimationSet _gActor560800Animation210F4;
static AnimationSet _gActor560800Animation2135C;
static AnimationSet _gActor560800Animation217E0;
static AnimationSet _gActor560800Animation21C64;
static AnimationSet _gActor560800Animation21EAC;
static AnimationSet _gActor560800Animation221E0;
static AnimationSet _gActor560800Animation223C4;
static AnimationSet _gActor560800Animation227D4;
static AnimationSet _gActor560800Animation22B54;
static AnimationSet _gActor560800Animation22E5C;
static AnimationSet _gActor560800Animation23268;
static AnimationSet _gActor560800Animation2361C;
static AnimationSet _gActor560800Animation23A7C;
static AnimationSet _gActor560800Animation23DD0;
static AnimationSet _gActor560800Animation24008;
static AnimationSet _gActor560800Animation24778;
static AnimationSet _gActor560800Animation24A6C;
static AnimationSet _gActor560800Animation24CE8;
static AnimationSet _gActor560800Animation2547C;
static AnimationSet _gActor560800Animation25A60;
static AnimationSet _gActor560800Animation25F18;
static AnimationSet _gActor560800Animation26260;
static AnimationSet _gActor560800Animation268DC;
static AnimationSet _gActor560800Animation26AB4;
static AnimationSet _gActor560800Animation26ED0;
static AnimationSet _gActor560800Animation2708C;
static AnimationSet _gActor560800Animation27280;
static AnimationSet _gActor560800Animation274F8;
static AnimationSet _gActor560800Animation2A7D0;
static AnimationSet _gActor560800Animation2AABC;
static AnimationSet _gActor560800Animation2AC68;
static AnimationSet _gActor560800Animation2AE40;
static AnimationSet _gActor560800Animation2B020;
static AnimationSet _gActor560800Animation2B2DC;
static AnimationSet _gActor560800Animation2B9B4;
static AnimationSet _gActor560800Animation2BD38;
static AnimationSet _gActor560800Animation2BF38;
static AnimationSet _gActor560800Animation2C0F4;
static AnimationSet _gActor560800Animation2C728;
static AnimationSet _gActor560800Animation2CDEC;
static AnimationSet _gActor560800Animation2DA6C;
static AnimationSet _gActor560800Animation2E370;
static AnimationSet _gActor560800Animation2EC58;
static AnimationSet _gActor560800Animation2F0FC;
static AnimationSet _gActor560800Animation2F728;
static AnimationSet _gActor560800Animation2FF90;
static AnimationSet _gActor560800Animation30290;
static AnimationSet _gActor560800Animation30C2C;
static AnimationSet _gActor560800Animation31014;
static AnimationSet _gActor560800Animation315E0;
static AnimationSet _gActor560800Animation31A28;
static AnimationSet _gActor560800Animation320A0;
static AnimationSet _gActor560800Animation3254C;
static AnimationSet _gActor560800Animation32C24;
static AnimationSet _gActor560800Animation33998;
static AnimationSet _gActor560800Animation33E18;
static AnimationSet _gActor560800Animation3404C;
static AnimationSet _gActor560800Animation34D08;
static AnimationSet _gActor560800Animation3552C;
static AnimationSet _gActor560800Animation35928;
static AnimationSet _gActor560800Animation35C50;
static AnimationSet _gActor560800Animation361B8;
static AnimationSet _gActor560800Animation367B4;
static AnimationSet _gActor560800Animation371E8;
static AnimationSet _gActor560800Animation37F5C;
static AnimationSet _gActor560800Animation38288;
static AnimationSet _gActor560800Animation389F0;
static AnimationSet _gActor560800Animation38E5C;
static AnimationSet _gActor560800Animation39368;
static AnimationSet _gActor560800Animation39F6C;
static AnimationSet _gActor560800Animation3A4EC;
static AnimationSet _gActor560800Animation3A804;
static AnimationSet _gActor560800Animation3B170;
static AnimationSet _gActor560800Animation3B78C;
static AnimationSet _gActor560800Animation3BF2C;
static AnimationSet _gActor560800Animation3C070;
static AnimationSet _gActor560800Animation3CBE0;
extern TmdSource    gEveMaskedBody;
static TmdSource    _gEveBody;
extern TmdSource    gKyleBody;
static TmdSource    _gGolemNo9BodyDryfield;
extern TmdSource    gKyleHandLeft2;
extern TmdSource    gKyleHandRight;
static TmdSource    _gGolemNo9Gunblade;
extern TmdSource    gKyleP229;
static void         _actor560800EveBodyTask(Task* task);
static void         _actor560800CastAttachmentTask(Task* task);
static void         _actor560800KyleBodyTask(Task* task);
static void         _actor560800No9BodyTask(Task* task);
static void         _actor560800RelightCast(void);
static void         _actor560800HideCastMember(u32 memberId);
static void         _actor560800PlaceCastForCut(s32 cutId);
static void         _actor560800FireKyleGun(s32 unusedArg);
static void         _actor560800FinishScenePhase(s32 phaseId);
static void         _actor560800CutsceneTask(Task* task);
static void         _actor560800FadeOutTask(Task* task);
static void         _actor560800FadeInTask(Task* task);
static void         _actor560800SetCastModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg);
static void         _actor560800SpawnFadeIn(s32 intensityStep);
static void         _actor560800SpawnFadeOut(s32 intensityStep);
static void         _actor560800SendChainCommand(s16 commandId);
static void         _actor560800SendCarrierCommand(s16 commandId);
static void         _actor560800PostPlayerCue(s16 cueId);
static void         _actor560800BlendScenePlayerAnimation(s16 animationId);
static void         _actor560800BlendEveAnimation(u16 animationId);
static void         _actor560800BlendNo9Animation(u16 animationId);
static void         _actor560800SwapSceneTextureStrip(void);
static void         _actor560800PostKyleCue(s16 cueId);
static void         _actor560800BlendKyleAnimation(u16 animationId);
static void         _actor560800StartSoundCue(s32 cueId);
static void         _actor560800ReplaceEveBodyTask(Task* task);
static void         _actor560800PostSceneCue(s16 cueId);
static void         _actor560800PostCastCue(s16 cueId);
static void         _actor560800ApplyShotDamage(void);
static void         _actor560800SkipScene(void);
static void         _actor560800StageSceneAudioStart(void);
static void         _actor560800StartScenePhasePlayback(s32 phaseId);
static void         _actor560800HoldDisplayUntilScenePlaybackEnds(void);
static void         _actor560800AwaitScenePlaybackTask(Task* task);
static void         _actor560800SelectSecondCapFile(void);
static void         _actor560800SelectThirdCapFile(void);
static void         _actor560800DiscardTask(Task* task);

static void _actor560800MovieTask(Task* task);
static void _actor560800StartMovieTask(Task* task);

#include "assets/characters/eve/masked_body.inc"

#include "assets/characters/eve/body.inc"

#include "assets/characters/kyle/body.inc"

#include "assets/enemies/golem/no_9/body_dryfield.inc"

#include "assets/characters/kyle/hand_left_2.inc"

#include "assets/characters/kyle/hand_right.inc"

#include "assets/enemies/golem/no_9/gunblade.inc"

#include "assets/characters/kyle/p229.inc"

static AnimationPackedPose _gActor560800Animation1EE00Bank1[20] = {
#include "assets/actor_560800_animation_1EE00_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation1EE00Bank4[111] = {
#include "assets/actor_560800_animation_1EE00_bank4.inc"
};

static AnimationRecord _gActor560800Animation1EE00Records[399] = {
#include "assets/actor_560800_animation_1EE00_records.inc"
};

static u16 _gActor560800Animation1EE00Indices[20] = {
#include "assets/actor_560800_animation_1EE00_indices.inc"
};

static AnimationSet _gActor560800Animation1EE00 = {
    _gActor560800Animation1EE00Records,
    _gActor560800Animation1EE00Indices,
    { NULL, _gActor560800Animation1EE00Bank1, NULL, NULL, _gActor560800Animation1EE00Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation1F0F8Bank1[3] = {
#include "assets/actor_560800_animation_1F0F8_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation1F0F8Bank4[61] = {
#include "assets/actor_560800_animation_1F0F8_bank4.inc"
};

static AnimationRecord _gActor560800Animation1F0F8Records[100] = {
#include "assets/actor_560800_animation_1F0F8_records.inc"
};

static u16 _gActor560800Animation1F0F8Indices[20] = {
#include "assets/actor_560800_animation_1F0F8_indices.inc"
};

static AnimationSet _gActor560800Animation1F0F8 = {
    _gActor560800Animation1F0F8Records,
    _gActor560800Animation1F0F8Indices,
    { NULL, _gActor560800Animation1F0F8Bank1, NULL, NULL, _gActor560800Animation1F0F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation1F2E4Bank1[2] = {
#include "assets/actor_560800_animation_1F2E4_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation1F2E4Bank4[15] = {
#include "assets/actor_560800_animation_1F2E4_bank4.inc"
};

static AnimationRecord _gActor560800Animation1F2E4Records[82] = {
#include "assets/actor_560800_animation_1F2E4_records.inc"
};

static u16 _gActor560800Animation1F2E4Indices[20] = {
#include "assets/actor_560800_animation_1F2E4_indices.inc"
};

static AnimationSet _gActor560800Animation1F2E4 = {
    _gActor560800Animation1F2E4Records,
    _gActor560800Animation1F2E4Indices,
    { NULL, _gActor560800Animation1F2E4Bank1, NULL, NULL, _gActor560800Animation1F2E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation1F6B8Bank1[16] = {
#include "assets/actor_560800_animation_1F6B8_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation1F6B8Bank4[63] = {
#include "assets/actor_560800_animation_1F6B8_bank4.inc"
};

static AnimationRecord _gActor560800Animation1F6B8Records[114] = {
#include "assets/actor_560800_animation_1F6B8_records.inc"
};

static u16 _gActor560800Animation1F6B8Indices[20] = {
#include "assets/actor_560800_animation_1F6B8_indices.inc"
};

static AnimationSet _gActor560800Animation1F6B8 = {
    _gActor560800Animation1F6B8Records,
    _gActor560800Animation1F6B8Indices,
    { NULL, _gActor560800Animation1F6B8Bank1, NULL, NULL, _gActor560800Animation1F6B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation20810Bank1[128] = {
#include "assets/actor_560800_animation_20810_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation20810Bank4[202] = {
#include "assets/actor_560800_animation_20810_bank4.inc"
};

static AnimationRecord _gActor560800Animation20810Records[504] = {
#include "assets/actor_560800_animation_20810_records.inc"
};

static u16 _gActor560800Animation20810Indices[20] = {
#include "assets/actor_560800_animation_20810_indices.inc"
};

static AnimationSet _gActor560800Animation20810 = {
    _gActor560800Animation20810Records,
    _gActor560800Animation20810Indices,
    { NULL, _gActor560800Animation20810Bank1, NULL, NULL, _gActor560800Animation20810Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation20D5CBank1[17] = {
#include "assets/actor_560800_animation_20D5C_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation20D5CBank4[83] = {
#include "assets/actor_560800_animation_20D5C_bank4.inc"
};

static AnimationRecord _gActor560800Animation20D5CRecords[185] = {
#include "assets/actor_560800_animation_20D5C_records.inc"
};

static u16 _gActor560800Animation20D5CIndices[20] = {
#include "assets/actor_560800_animation_20D5C_indices.inc"
};

static AnimationSet _gActor560800Animation20D5C = {
    _gActor560800Animation20D5CRecords,
    _gActor560800Animation20D5CIndices,
    { NULL, _gActor560800Animation20D5CBank1, NULL, NULL, _gActor560800Animation20D5CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation210F4Bank1[5] = {
#include "assets/actor_560800_animation_210F4_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation210F4Bank4[72] = {
#include "assets/actor_560800_animation_210F4_bank4.inc"
};

static AnimationRecord _gActor560800Animation210F4Records[123] = {
#include "assets/actor_560800_animation_210F4_records.inc"
};

static u16 _gActor560800Animation210F4Indices[20] = {
#include "assets/actor_560800_animation_210F4_indices.inc"
};

static AnimationSet _gActor560800Animation210F4 = {
    _gActor560800Animation210F4Records,
    _gActor560800Animation210F4Indices,
    { NULL, _gActor560800Animation210F4Bank1, NULL, NULL, _gActor560800Animation210F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation2135CBank1[3] = {
#include "assets/actor_560800_animation_2135C_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation2135CBank4[42] = {
#include "assets/actor_560800_animation_2135C_bank4.inc"
};

static AnimationRecord _gActor560800Animation2135CRecords[83] = {
#include "assets/actor_560800_animation_2135C_records.inc"
};

static u16 _gActor560800Animation2135CIndices[20] = {
#include "assets/actor_560800_animation_2135C_indices.inc"
};

static AnimationSet _gActor560800Animation2135C = {
    _gActor560800Animation2135CRecords,
    _gActor560800Animation2135CIndices,
    { NULL, _gActor560800Animation2135CBank1, NULL, NULL, _gActor560800Animation2135CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation217E0Bank1[2] = {
#include "assets/actor_560800_animation_217E0_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation217E0Bank4[63] = {
#include "assets/actor_560800_animation_217E0_bank4.inc"
};

static AnimationRecord _gActor560800Animation217E0Records[200] = {
#include "assets/actor_560800_animation_217E0_records.inc"
};

static u16 _gActor560800Animation217E0Indices[20] = {
#include "assets/actor_560800_animation_217E0_indices.inc"
};

static AnimationSet _gActor560800Animation217E0 = {
    _gActor560800Animation217E0Records,
    _gActor560800Animation217E0Indices,
    { NULL, _gActor560800Animation217E0Bank1, NULL, NULL, _gActor560800Animation217E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation21C64Bank1[4] = {
#include "assets/actor_560800_animation_21C64_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation21C64Bank4[105] = {
#include "assets/actor_560800_animation_21C64_bank4.inc"
};

static AnimationRecord _gActor560800Animation21C64Records[152] = {
#include "assets/actor_560800_animation_21C64_records.inc"
};

static u16 _gActor560800Animation21C64Indices[20] = {
#include "assets/actor_560800_animation_21C64_indices.inc"
};

static AnimationSet _gActor560800Animation21C64 = {
    _gActor560800Animation21C64Records,
    _gActor560800Animation21C64Indices,
    { NULL, _gActor560800Animation21C64Bank1, NULL, NULL, _gActor560800Animation21C64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation21EACBank1[6] = {
#include "assets/actor_560800_animation_21EAC_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation21EACBank4[33] = {
#include "assets/actor_560800_animation_21EAC_bank4.inc"
};

static AnimationRecord _gActor560800Animation21EACRecords[75] = {
#include "assets/actor_560800_animation_21EAC_records.inc"
};

static u16 _gActor560800Animation21EACIndices[20] = {
#include "assets/actor_560800_animation_21EAC_indices.inc"
};

static AnimationSet _gActor560800Animation21EAC = {
    _gActor560800Animation21EACRecords,
    _gActor560800Animation21EACIndices,
    { NULL, _gActor560800Animation21EACBank1, NULL, NULL, _gActor560800Animation21EACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation221E0Bank1[7] = {
#include "assets/actor_560800_animation_221E0_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation221E0Bank4[54] = {
#include "assets/actor_560800_animation_221E0_bank4.inc"
};

static AnimationRecord _gActor560800Animation221E0Records[110] = {
#include "assets/actor_560800_animation_221E0_records.inc"
};

static u16 _gActor560800Animation221E0Indices[20] = {
#include "assets/actor_560800_animation_221E0_indices.inc"
};

static AnimationSet _gActor560800Animation221E0 = {
    _gActor560800Animation221E0Records,
    _gActor560800Animation221E0Indices,
    { NULL, _gActor560800Animation221E0Bank1, NULL, NULL, _gActor560800Animation221E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation223C4Bank1[2] = {
#include "assets/actor_560800_animation_223C4_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation223C4Bank4[24] = {
#include "assets/actor_560800_animation_223C4_bank4.inc"
};

static AnimationRecord _gActor560800Animation223C4Records[71] = {
#include "assets/actor_560800_animation_223C4_records.inc"
};

static u16 _gActor560800Animation223C4Indices[20] = {
#include "assets/actor_560800_animation_223C4_indices.inc"
};

static AnimationSet _gActor560800Animation223C4 = {
    _gActor560800Animation223C4Records,
    _gActor560800Animation223C4Indices,
    { NULL, _gActor560800Animation223C4Bank1, NULL, NULL, _gActor560800Animation223C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation227D4Bank1[6] = {
#include "assets/actor_560800_animation_227D4_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation227D4Bank4[53] = {
#include "assets/actor_560800_animation_227D4_bank4.inc"
};

static AnimationRecord _gActor560800Animation227D4Records[169] = {
#include "assets/actor_560800_animation_227D4_records.inc"
};

static u16 _gActor560800Animation227D4Indices[20] = {
#include "assets/actor_560800_animation_227D4_indices.inc"
};

static AnimationSet _gActor560800Animation227D4 = {
    _gActor560800Animation227D4Records,
    _gActor560800Animation227D4Indices,
    { NULL, _gActor560800Animation227D4Bank1, NULL, NULL, _gActor560800Animation227D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation22B54Bank1[6] = {
#include "assets/actor_560800_animation_22B54_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation22B54Bank4[70] = {
#include "assets/actor_560800_animation_22B54_bank4.inc"
};

static AnimationRecord _gActor560800Animation22B54Records[116] = {
#include "assets/actor_560800_animation_22B54_records.inc"
};

static u16 _gActor560800Animation22B54Indices[20] = {
#include "assets/actor_560800_animation_22B54_indices.inc"
};

static AnimationSet _gActor560800Animation22B54 = {
    _gActor560800Animation22B54Records,
    _gActor560800Animation22B54Indices,
    { NULL, _gActor560800Animation22B54Bank1, NULL, NULL, _gActor560800Animation22B54Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation22E5CBank1[2] = {
#include "assets/actor_560800_animation_22E5C_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation22E5CBank4[49] = {
#include "assets/actor_560800_animation_22E5C_bank4.inc"
};

static AnimationRecord _gActor560800Animation22E5CRecords[119] = {
#include "assets/actor_560800_animation_22E5C_records.inc"
};

static u16 _gActor560800Animation22E5CIndices[20] = {
#include "assets/actor_560800_animation_22E5C_indices.inc"
};

static AnimationSet _gActor560800Animation22E5C = {
    _gActor560800Animation22E5CRecords,
    _gActor560800Animation22E5CIndices,
    { NULL, _gActor560800Animation22E5CBank1, NULL, NULL, _gActor560800Animation22E5CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation23268Bank1[9] = {
#include "assets/actor_560800_animation_23268_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation23268Bank4[68] = {
#include "assets/actor_560800_animation_23268_bank4.inc"
};

static AnimationRecord _gActor560800Animation23268Records[144] = {
#include "assets/actor_560800_animation_23268_records.inc"
};

static u16 _gActor560800Animation23268Indices[20] = {
#include "assets/actor_560800_animation_23268_indices.inc"
};

static AnimationSet _gActor560800Animation23268 = {
    _gActor560800Animation23268Records,
    _gActor560800Animation23268Indices,
    { NULL, _gActor560800Animation23268Bank1, NULL, NULL, _gActor560800Animation23268Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation2361CBank1[12] = {
#include "assets/actor_560800_animation_2361C_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation2361CBank4[68] = {
#include "assets/actor_560800_animation_2361C_bank4.inc"
};

static AnimationRecord _gActor560800Animation2361CRecords[113] = {
#include "assets/actor_560800_animation_2361C_records.inc"
};

static u16 _gActor560800Animation2361CIndices[20] = {
#include "assets/actor_560800_animation_2361C_indices.inc"
};

static AnimationSet _gActor560800Animation2361C = {
    _gActor560800Animation2361CRecords,
    _gActor560800Animation2361CIndices,
    { NULL, _gActor560800Animation2361CBank1, NULL, NULL, _gActor560800Animation2361CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation23A7CBank1[14] = {
#include "assets/actor_560800_animation_23A7C_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation23A7CBank4[81] = {
#include "assets/actor_560800_animation_23A7C_bank4.inc"
};

static AnimationRecord _gActor560800Animation23A7CRecords[137] = {
#include "assets/actor_560800_animation_23A7C_records.inc"
};

static u16 _gActor560800Animation23A7CIndices[20] = {
#include "assets/actor_560800_animation_23A7C_indices.inc"
};

static AnimationSet _gActor560800Animation23A7C = {
    _gActor560800Animation23A7CRecords,
    _gActor560800Animation23A7CIndices,
    { NULL, _gActor560800Animation23A7CBank1, NULL, NULL, _gActor560800Animation23A7CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation23DD0Bank1[15] = {
#include "assets/actor_560800_animation_23DD0_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation23DD0Bank4[34] = {
#include "assets/actor_560800_animation_23DD0_bank4.inc"
};

static AnimationRecord _gActor560800Animation23DD0Records[114] = {
#include "assets/actor_560800_animation_23DD0_records.inc"
};

static u16 _gActor560800Animation23DD0Indices[20] = {
#include "assets/actor_560800_animation_23DD0_indices.inc"
};

static AnimationSet _gActor560800Animation23DD0 = {
    _gActor560800Animation23DD0Records,
    _gActor560800Animation23DD0Indices,
    { NULL, _gActor560800Animation23DD0Bank1, NULL, NULL, _gActor560800Animation23DD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation24008Bank1[3] = {
#include "assets/actor_560800_animation_24008_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation24008Bank4[37] = {
#include "assets/actor_560800_animation_24008_bank4.inc"
};

static AnimationRecord _gActor560800Animation24008Records[76] = {
#include "assets/actor_560800_animation_24008_records.inc"
};

static u16 _gActor560800Animation24008Indices[20] = {
#include "assets/actor_560800_animation_24008_indices.inc"
};

static AnimationSet _gActor560800Animation24008 = {
    _gActor560800Animation24008Records,
    _gActor560800Animation24008Indices,
    { NULL, _gActor560800Animation24008Bank1, NULL, NULL, _gActor560800Animation24008Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation24778Bank1[32] = {
#include "assets/actor_560800_animation_24778_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation24778Bank4[140] = {
#include "assets/actor_560800_animation_24778_bank4.inc"
};

static AnimationRecord _gActor560800Animation24778Records[220] = {
#include "assets/actor_560800_animation_24778_records.inc"
};

static u16 _gActor560800Animation24778Indices[20] = {
#include "assets/actor_560800_animation_24778_indices.inc"
};

static AnimationSet _gActor560800Animation24778 = {
    _gActor560800Animation24778Records,
    _gActor560800Animation24778Indices,
    { NULL, _gActor560800Animation24778Bank1, NULL, NULL, _gActor560800Animation24778Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation24A6CBank1[3] = {
#include "assets/actor_560800_animation_24A6C_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation24A6CBank4[64] = {
#include "assets/actor_560800_animation_24A6C_bank4.inc"
};

static AnimationRecord _gActor560800Animation24A6CRecords[96] = {
#include "assets/actor_560800_animation_24A6C_records.inc"
};

static u16 _gActor560800Animation24A6CIndices[20] = {
#include "assets/actor_560800_animation_24A6C_indices.inc"
};

static AnimationSet _gActor560800Animation24A6C = {
    _gActor560800Animation24A6CRecords,
    _gActor560800Animation24A6CIndices,
    { NULL, _gActor560800Animation24A6CBank1, NULL, NULL, _gActor560800Animation24A6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation24CE8Bank1[2] = {
#include "assets/actor_560800_animation_24CE8_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation24CE8Bank4[32] = {
#include "assets/actor_560800_animation_24CE8_bank4.inc"
};

static AnimationRecord _gActor560800Animation24CE8Records[101] = {
#include "assets/actor_560800_animation_24CE8_records.inc"
};

static u16 _gActor560800Animation24CE8Indices[20] = {
#include "assets/actor_560800_animation_24CE8_indices.inc"
};

static AnimationSet _gActor560800Animation24CE8 = {
    _gActor560800Animation24CE8Records,
    _gActor560800Animation24CE8Indices,
    { NULL, _gActor560800Animation24CE8Bank1, NULL, NULL, _gActor560800Animation24CE8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation2547CBank1[21] = {
#include "assets/actor_560800_animation_2547C_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation2547CBank4[156] = {
#include "assets/actor_560800_animation_2547C_bank4.inc"
};

static AnimationRecord _gActor560800Animation2547CRecords[246] = {
#include "assets/actor_560800_animation_2547C_records.inc"
};

static u16 _gActor560800Animation2547CIndices[20] = {
#include "assets/actor_560800_animation_2547C_indices.inc"
};

static AnimationSet _gActor560800Animation2547C = {
    _gActor560800Animation2547CRecords,
    _gActor560800Animation2547CIndices,
    { NULL, _gActor560800Animation2547CBank1, NULL, NULL, _gActor560800Animation2547CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation25A60Bank1[2] = {
#include "assets/actor_560800_animation_25A60_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation25A60Bank4[152] = {
#include "assets/actor_560800_animation_25A60_bank4.inc"
};

static AnimationRecord _gActor560800Animation25A60Records[199] = {
#include "assets/actor_560800_animation_25A60_records.inc"
};

static u16 _gActor560800Animation25A60Indices[20] = {
#include "assets/actor_560800_animation_25A60_indices.inc"
};

static AnimationSet _gActor560800Animation25A60 = {
    _gActor560800Animation25A60Records,
    _gActor560800Animation25A60Indices,
    { NULL, _gActor560800Animation25A60Bank1, NULL, NULL, _gActor560800Animation25A60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation25F18Bank1[6] = {
#include "assets/actor_560800_animation_25F18_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation25F18Bank4[83] = {
#include "assets/actor_560800_animation_25F18_bank4.inc"
};

static AnimationRecord _gActor560800Animation25F18Records[181] = {
#include "assets/actor_560800_animation_25F18_records.inc"
};

static u16 _gActor560800Animation25F18Indices[20] = {
#include "assets/actor_560800_animation_25F18_indices.inc"
};

static AnimationSet _gActor560800Animation25F18 = {
    _gActor560800Animation25F18Records,
    _gActor560800Animation25F18Indices,
    { NULL, _gActor560800Animation25F18Bank1, NULL, NULL, _gActor560800Animation25F18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation26260Bank1[5] = {
#include "assets/actor_560800_animation_26260_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation26260Bank4[66] = {
#include "assets/actor_560800_animation_26260_bank4.inc"
};

static AnimationRecord _gActor560800Animation26260Records[109] = {
#include "assets/actor_560800_animation_26260_records.inc"
};

static u16 _gActor560800Animation26260Indices[20] = {
#include "assets/actor_560800_animation_26260_indices.inc"
};

static AnimationSet _gActor560800Animation26260 = {
    _gActor560800Animation26260Records,
    _gActor560800Animation26260Indices,
    { NULL, _gActor560800Animation26260Bank1, NULL, NULL, _gActor560800Animation26260Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation268DCBank1[10] = {
#include "assets/actor_560800_animation_268DC_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation268DCBank4[142] = {
#include "assets/actor_560800_animation_268DC_bank4.inc"
};

static AnimationRecord _gActor560800Animation268DCRecords[223] = {
#include "assets/actor_560800_animation_268DC_records.inc"
};

static u16 _gActor560800Animation268DCIndices[20] = {
#include "assets/actor_560800_animation_268DC_indices.inc"
};

static AnimationSet _gActor560800Animation268DC = {
    _gActor560800Animation268DCRecords,
    _gActor560800Animation268DCIndices,
    { NULL, _gActor560800Animation268DCBank1, NULL, NULL, _gActor560800Animation268DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation26AB4Bank1[3] = {
#include "assets/actor_560800_animation_26AB4_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation26AB4Bank4[29] = {
#include "assets/actor_560800_animation_26AB4_bank4.inc"
};

static AnimationRecord _gActor560800Animation26AB4Records[60] = {
#include "assets/actor_560800_animation_26AB4_records.inc"
};

static u16 _gActor560800Animation26AB4Indices[20] = {
#include "assets/actor_560800_animation_26AB4_indices.inc"
};

static AnimationSet _gActor560800Animation26AB4 = {
    _gActor560800Animation26AB4Records,
    _gActor560800Animation26AB4Indices,
    { NULL, _gActor560800Animation26AB4Bank1, NULL, NULL, _gActor560800Animation26AB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation26ED0Bank1[3] = {
#include "assets/actor_560800_animation_26ED0_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation26ED0Bank4[80] = {
#include "assets/actor_560800_animation_26ED0_bank4.inc"
};

static AnimationRecord _gActor560800Animation26ED0Records[154] = {
#include "assets/actor_560800_animation_26ED0_records.inc"
};

static u16 _gActor560800Animation26ED0Indices[20] = {
#include "assets/actor_560800_animation_26ED0_indices.inc"
};

static AnimationSet _gActor560800Animation26ED0 = {
    _gActor560800Animation26ED0Records,
    _gActor560800Animation26ED0Indices,
    { NULL, _gActor560800Animation26ED0Bank1, NULL, NULL, _gActor560800Animation26ED0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation2708CBank1[2] = {
#include "assets/actor_560800_animation_2708C_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation2708CBank4[25] = {
#include "assets/actor_560800_animation_2708C_bank4.inc"
};

static AnimationRecord _gActor560800Animation2708CRecords[60] = {
#include "assets/actor_560800_animation_2708C_records.inc"
};

static u16 _gActor560800Animation2708CIndices[20] = {
#include "assets/actor_560800_animation_2708C_indices.inc"
};

static AnimationSet _gActor560800Animation2708C = {
    _gActor560800Animation2708CRecords,
    _gActor560800Animation2708CIndices,
    { NULL, _gActor560800Animation2708CBank1, NULL, NULL, _gActor560800Animation2708CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation27280Bank1[2] = {
#include "assets/actor_560800_animation_27280_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation27280Bank4[19] = {
#include "assets/actor_560800_animation_27280_bank4.inc"
};

static AnimationRecord _gActor560800Animation27280Records[80] = {
#include "assets/actor_560800_animation_27280_records.inc"
};

static u16 _gActor560800Animation27280Indices[20] = {
#include "assets/actor_560800_animation_27280_indices.inc"
};

static AnimationSet _gActor560800Animation27280 = {
    _gActor560800Animation27280Records,
    _gActor560800Animation27280Indices,
    { NULL, _gActor560800Animation27280Bank1, NULL, NULL, _gActor560800Animation27280Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation274F8Bank1[2] = {
#include "assets/actor_560800_animation_274F8_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation274F8Bank4[50] = {
#include "assets/actor_560800_animation_274F8_bank4.inc"
};

static AnimationRecord _gActor560800Animation274F8Records[82] = {
#include "assets/actor_560800_animation_274F8_records.inc"
};

static u16 _gActor560800Animation274F8Indices[20] = {
#include "assets/actor_560800_animation_274F8_indices.inc"
};

static AnimationSet _gActor560800Animation274F8 = {
    _gActor560800Animation274F8Records,
    _gActor560800Animation274F8Indices,
    { NULL, _gActor560800Animation274F8Bank1, NULL, NULL, _gActor560800Animation274F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation27A30Bank1[15] = {
#include "assets/actor_560800_animation_27A30_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation27A30Bank4[113] = {
#include "assets/actor_560800_animation_27A30_bank4.inc"
};

static AnimationRecord _gActor560800Animation27A30Records[156] = {
#include "assets/actor_560800_animation_27A30_records.inc"
};

static u16 _gActor560800Animation27A30Indices[20] = {
#include "assets/actor_560800_animation_27A30_indices.inc"
};

static AnimationSet _gActor560800Animation27A30 = {
    _gActor560800Animation27A30Records,
    _gActor560800Animation27A30Indices,
    { NULL, _gActor560800Animation27A30Bank1, NULL, NULL, _gActor560800Animation27A30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation27ED8Bank1[7] = {
#include "assets/actor_560800_animation_27ED8_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation27ED8Bank4[102] = {
#include "assets/actor_560800_animation_27ED8_bank4.inc"
};

static AnimationRecord _gActor560800Animation27ED8Records[155] = {
#include "assets/actor_560800_animation_27ED8_records.inc"
};

static u16 _gActor560800Animation27ED8Indices[20] = {
#include "assets/actor_560800_animation_27ED8_indices.inc"
};

static AnimationSet _gActor560800Animation27ED8 = {
    _gActor560800Animation27ED8Records,
    _gActor560800Animation27ED8Indices,
    { NULL, _gActor560800Animation27ED8Bank1, NULL, NULL, _gActor560800Animation27ED8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation288D4Bank1[34] = {
#include "assets/actor_560800_animation_288D4_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation288D4Bank4[225] = {
#include "assets/actor_560800_animation_288D4_bank4.inc"
};

static AnimationRecord _gActor560800Animation288D4Records[292] = {
#include "assets/actor_560800_animation_288D4_records.inc"
};

static u16 _gActor560800Animation288D4Indices[20] = {
#include "assets/actor_560800_animation_288D4_indices.inc"
};

static AnimationSet _gActor560800Animation288D4 = {
    _gActor560800Animation288D4Records,
    _gActor560800Animation288D4Indices,
    { NULL, _gActor560800Animation288D4Bank1, NULL, NULL, _gActor560800Animation288D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation292D0Bank1[43] = {
#include "assets/actor_560800_animation_292D0_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation292D0Bank4[210] = {
#include "assets/actor_560800_animation_292D0_bank4.inc"
};

static AnimationRecord _gActor560800Animation292D0Records[280] = {
#include "assets/actor_560800_animation_292D0_records.inc"
};

static u16 _gActor560800Animation292D0Indices[20] = {
#include "assets/actor_560800_animation_292D0_indices.inc"
};

static AnimationSet _gActor560800Animation292D0 = {
    _gActor560800Animation292D0Records,
    _gActor560800Animation292D0Indices,
    { NULL, _gActor560800Animation292D0Bank1, NULL, NULL, _gActor560800Animation292D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation295E0Bank1[2] = {
#include "assets/actor_560800_animation_295E0_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation295E0Bank4[46] = {
#include "assets/actor_560800_animation_295E0_bank4.inc"
};

static AnimationRecord _gActor560800Animation295E0Records[124] = {
#include "assets/actor_560800_animation_295E0_records.inc"
};

static u16 _gActor560800Animation295E0Indices[20] = {
#include "assets/actor_560800_animation_295E0_indices.inc"
};

static AnimationSet _gActor560800Animation295E0 = {
    _gActor560800Animation295E0Records,
    _gActor560800Animation295E0Indices,
    { NULL, _gActor560800Animation295E0Bank1, NULL, NULL, _gActor560800Animation295E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation29894Bank1[2] = {
#include "assets/actor_560800_animation_29894_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation29894Bank4[36] = {
#include "assets/actor_560800_animation_29894_bank4.inc"
};

static AnimationRecord _gActor560800Animation29894Records[111] = {
#include "assets/actor_560800_animation_29894_records.inc"
};

static u16 _gActor560800Animation29894Indices[20] = {
#include "assets/actor_560800_animation_29894_indices.inc"
};

static AnimationSet _gActor560800Animation29894 = {
    _gActor560800Animation29894Records,
    _gActor560800Animation29894Indices,
    { NULL, _gActor560800Animation29894Bank1, NULL, NULL, _gActor560800Animation29894Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation29B98Bank1[6] = {
#include "assets/actor_560800_animation_29B98_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation29B98Bank4[46] = {
#include "assets/actor_560800_animation_29B98_bank4.inc"
};

static AnimationRecord _gActor560800Animation29B98Records[109] = {
#include "assets/actor_560800_animation_29B98_records.inc"
};

static u16 _gActor560800Animation29B98Indices[20] = {
#include "assets/actor_560800_animation_29B98_indices.inc"
};

static AnimationSet _gActor560800Animation29B98 = {
    _gActor560800Animation29B98Records,
    _gActor560800Animation29B98Indices,
    { NULL, _gActor560800Animation29B98Bank1, NULL, NULL, _gActor560800Animation29B98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation2A294Bank1[10] = {
#include "assets/actor_560800_animation_2A294_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation2A294Bank4[147] = {
#include "assets/actor_560800_animation_2A294_bank4.inc"
};

static AnimationRecord _gActor560800Animation2A294Records[250] = {
#include "assets/actor_560800_animation_2A294_records.inc"
};

static u16 _gActor560800Animation2A294Indices[20] = {
#include "assets/actor_560800_animation_2A294_indices.inc"
};

static AnimationSet _gActor560800Animation2A294 = {
    _gActor560800Animation2A294Records,
    _gActor560800Animation2A294Indices,
    { NULL, _gActor560800Animation2A294Bank1, NULL, NULL, _gActor560800Animation2A294Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation2A50CBank1[4] = {
#include "assets/actor_560800_animation_2A50C_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation2A50CBank4[51] = {
#include "assets/actor_560800_animation_2A50C_bank4.inc"
};

static AnimationRecord _gActor560800Animation2A50CRecords[75] = {
#include "assets/actor_560800_animation_2A50C_records.inc"
};

static u16 _gActor560800Animation2A50CIndices[20] = {
#include "assets/actor_560800_animation_2A50C_indices.inc"
};

static AnimationSet _gActor560800Animation2A50C = {
    _gActor560800Animation2A50CRecords,
    _gActor560800Animation2A50CIndices,
    { NULL, _gActor560800Animation2A50CBank1, NULL, NULL, _gActor560800Animation2A50CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation2A7D0Bank1[3] = {
#include "assets/actor_560800_animation_2A7D0_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation2A7D0Bank4[30] = {
#include "assets/actor_560800_animation_2A7D0_bank4.inc"
};

static AnimationRecord _gActor560800Animation2A7D0Records[118] = {
#include "assets/actor_560800_animation_2A7D0_records.inc"
};

static u16 _gActor560800Animation2A7D0Indices[20] = {
#include "assets/actor_560800_animation_2A7D0_indices.inc"
};

static AnimationSet _gActor560800Animation2A7D0 = {
    _gActor560800Animation2A7D0Records,
    _gActor560800Animation2A7D0Indices,
    { NULL, _gActor560800Animation2A7D0Bank1, NULL, NULL, _gActor560800Animation2A7D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation2AABCBank1[5] = {
#include "assets/actor_560800_animation_2AABC_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation2AABCBank4[40] = {
#include "assets/actor_560800_animation_2AABC_bank4.inc"
};

static AnimationRecord _gActor560800Animation2AABCRecords[112] = {
#include "assets/actor_560800_animation_2AABC_records.inc"
};

static u16 _gActor560800Animation2AABCIndices[20] = {
#include "assets/actor_560800_animation_2AABC_indices.inc"
};

static AnimationSet _gActor560800Animation2AABC = {
    _gActor560800Animation2AABCRecords,
    _gActor560800Animation2AABCIndices,
    { NULL, _gActor560800Animation2AABCBank1, NULL, NULL, _gActor560800Animation2AABCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation2AC68Bank1[2] = {
#include "assets/actor_560800_animation_2AC68_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation2AC68Bank4[18] = {
#include "assets/actor_560800_animation_2AC68_bank4.inc"
};

static AnimationRecord _gActor560800Animation2AC68Records[63] = {
#include "assets/actor_560800_animation_2AC68_records.inc"
};

static u16 _gActor560800Animation2AC68Indices[20] = {
#include "assets/actor_560800_animation_2AC68_indices.inc"
};

static AnimationSet _gActor560800Animation2AC68 = {
    _gActor560800Animation2AC68Records,
    _gActor560800Animation2AC68Indices,
    { NULL, _gActor560800Animation2AC68Bank1, NULL, NULL, _gActor560800Animation2AC68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation2AE40Bank1[3] = {
#include "assets/actor_560800_animation_2AE40_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation2AE40Bank4[11] = {
#include "assets/actor_560800_animation_2AE40_bank4.inc"
};

static AnimationRecord _gActor560800Animation2AE40Records[78] = {
#include "assets/actor_560800_animation_2AE40_records.inc"
};

static u16 _gActor560800Animation2AE40Indices[20] = {
#include "assets/actor_560800_animation_2AE40_indices.inc"
};

static AnimationSet _gActor560800Animation2AE40 = {
    _gActor560800Animation2AE40Records,
    _gActor560800Animation2AE40Indices,
    { NULL, _gActor560800Animation2AE40Bank1, NULL, NULL, _gActor560800Animation2AE40Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation2B020Bank1[2] = {
#include "assets/actor_560800_animation_2B020_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation2B020Bank4[26] = {
#include "assets/actor_560800_animation_2B020_bank4.inc"
};

static AnimationRecord _gActor560800Animation2B020Records[68] = {
#include "assets/actor_560800_animation_2B020_records.inc"
};

static u16 _gActor560800Animation2B020Indices[20] = {
#include "assets/actor_560800_animation_2B020_indices.inc"
};

static AnimationSet _gActor560800Animation2B020 = {
    _gActor560800Animation2B020Records,
    _gActor560800Animation2B020Indices,
    { NULL, _gActor560800Animation2B020Bank1, NULL, NULL, _gActor560800Animation2B020Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation2B2DCBank1[2] = {
#include "assets/actor_560800_animation_2B2DC_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation2B2DCBank4[55] = {
#include "assets/actor_560800_animation_2B2DC_bank4.inc"
};

static AnimationRecord _gActor560800Animation2B2DCRecords[94] = {
#include "assets/actor_560800_animation_2B2DC_records.inc"
};

static u16 _gActor560800Animation2B2DCIndices[20] = {
#include "assets/actor_560800_animation_2B2DC_indices.inc"
};

static AnimationSet _gActor560800Animation2B2DC = {
    _gActor560800Animation2B2DCRecords,
    _gActor560800Animation2B2DCIndices,
    { NULL, _gActor560800Animation2B2DCBank1, NULL, NULL, _gActor560800Animation2B2DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation2B9B4Bank1[8] = {
#include "assets/actor_560800_animation_2B9B4_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation2B9B4Bank4[127] = {
#include "assets/actor_560800_animation_2B9B4_bank4.inc"
};

static AnimationRecord _gActor560800Animation2B9B4Records[267] = {
#include "assets/actor_560800_animation_2B9B4_records.inc"
};

static u16 _gActor560800Animation2B9B4Indices[20] = {
#include "assets/actor_560800_animation_2B9B4_indices.inc"
};

static AnimationSet _gActor560800Animation2B9B4 = {
    _gActor560800Animation2B9B4Records,
    _gActor560800Animation2B9B4Indices,
    { NULL, _gActor560800Animation2B9B4Bank1, NULL, NULL, _gActor560800Animation2B9B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation2BD38Bank1[13] = {
#include "assets/actor_560800_animation_2BD38_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation2BD38Bank4[50] = {
#include "assets/actor_560800_animation_2BD38_bank4.inc"
};

static AnimationRecord _gActor560800Animation2BD38Records[116] = {
#include "assets/actor_560800_animation_2BD38_records.inc"
};

static u16 _gActor560800Animation2BD38Indices[20] = {
#include "assets/actor_560800_animation_2BD38_indices.inc"
};

static AnimationSet _gActor560800Animation2BD38 = {
    _gActor560800Animation2BD38Records,
    _gActor560800Animation2BD38Indices,
    { NULL, _gActor560800Animation2BD38Bank1, NULL, NULL, _gActor560800Animation2BD38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation2BF38Bank1[2] = {
#include "assets/actor_560800_animation_2BF38_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation2BF38Bank4[29] = {
#include "assets/actor_560800_animation_2BF38_bank4.inc"
};

static AnimationRecord _gActor560800Animation2BF38Records[73] = {
#include "assets/actor_560800_animation_2BF38_records.inc"
};

static u16 _gActor560800Animation2BF38Indices[20] = {
#include "assets/actor_560800_animation_2BF38_indices.inc"
};

static AnimationSet _gActor560800Animation2BF38 = {
    _gActor560800Animation2BF38Records,
    _gActor560800Animation2BF38Indices,
    { NULL, _gActor560800Animation2BF38Bank1, NULL, NULL, _gActor560800Animation2BF38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation2C0F4Bank1[2] = {
#include "assets/actor_560800_animation_2C0F4_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation2C0F4Bank4[21] = {
#include "assets/actor_560800_animation_2C0F4_bank4.inc"
};

static AnimationRecord _gActor560800Animation2C0F4Records[64] = {
#include "assets/actor_560800_animation_2C0F4_records.inc"
};

static u16 _gActor560800Animation2C0F4Indices[20] = {
#include "assets/actor_560800_animation_2C0F4_indices.inc"
};

static AnimationSet _gActor560800Animation2C0F4 = {
    _gActor560800Animation2C0F4Records,
    _gActor560800Animation2C0F4Indices,
    { NULL, _gActor560800Animation2C0F4Bank1, NULL, NULL, _gActor560800Animation2C0F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation2C728Bank1[19] = {
#include "assets/actor_560800_animation_2C728_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation2C728Bank4[126] = {
#include "assets/actor_560800_animation_2C728_bank4.inc"
};

static AnimationRecord _gActor560800Animation2C728Records[194] = {
#include "assets/actor_560800_animation_2C728_records.inc"
};

static u16 _gActor560800Animation2C728Indices[20] = {
#include "assets/actor_560800_animation_2C728_indices.inc"
};

static AnimationSet _gActor560800Animation2C728 = {
    _gActor560800Animation2C728Records,
    _gActor560800Animation2C728Indices,
    { NULL, _gActor560800Animation2C728Bank1, NULL, NULL, _gActor560800Animation2C728Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation2CDECBank1[33] = {
#include "assets/actor_560800_animation_2CDEC_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation2CDECBank4[124] = {
#include "assets/actor_560800_animation_2CDEC_bank4.inc"
};

static AnimationRecord _gActor560800Animation2CDECRecords[190] = {
#include "assets/actor_560800_animation_2CDEC_records.inc"
};

static u16 _gActor560800Animation2CDECIndices[20] = {
#include "assets/actor_560800_animation_2CDEC_indices.inc"
};

static AnimationSet _gActor560800Animation2CDEC = {
    _gActor560800Animation2CDECRecords,
    _gActor560800Animation2CDECIndices,
    { NULL, _gActor560800Animation2CDECBank1, NULL, NULL, _gActor560800Animation2CDECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation2DA6CBank1[78] = {
#include "assets/actor_560800_animation_2DA6C_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation2DA6CBank4[206] = {
#include "assets/actor_560800_animation_2DA6C_bank4.inc"
};

static AnimationRecord _gActor560800Animation2DA6CRecords[340] = {
#include "assets/actor_560800_animation_2DA6C_records.inc"
};

static u16 _gActor560800Animation2DA6CIndices[20] = {
#include "assets/actor_560800_animation_2DA6C_indices.inc"
};

static AnimationSet _gActor560800Animation2DA6C = {
    _gActor560800Animation2DA6CRecords,
    _gActor560800Animation2DA6CIndices,
    { NULL, _gActor560800Animation2DA6CBank1, NULL, NULL, _gActor560800Animation2DA6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation2E370Bank1[41] = {
#include "assets/actor_560800_animation_2E370_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation2E370Bank4[163] = {
#include "assets/actor_560800_animation_2E370_bank4.inc"
};

static AnimationRecord _gActor560800Animation2E370Records[271] = {
#include "assets/actor_560800_animation_2E370_records.inc"
};

static u16 _gActor560800Animation2E370Indices[20] = {
#include "assets/actor_560800_animation_2E370_indices.inc"
};

static AnimationSet _gActor560800Animation2E370 = {
    _gActor560800Animation2E370Records,
    _gActor560800Animation2E370Indices,
    { NULL, _gActor560800Animation2E370Bank1, NULL, NULL, _gActor560800Animation2E370Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation2EC58Bank1[5] = {
#include "assets/actor_560800_animation_2EC58_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation2EC58Bank4[165] = {
#include "assets/actor_560800_animation_2EC58_bank4.inc"
};

static AnimationRecord _gActor560800Animation2EC58Records[370] = {
#include "assets/actor_560800_animation_2EC58_records.inc"
};

static u16 _gActor560800Animation2EC58Indices[20] = {
#include "assets/actor_560800_animation_2EC58_indices.inc"
};

static AnimationSet _gActor560800Animation2EC58 = {
    _gActor560800Animation2EC58Records,
    _gActor560800Animation2EC58Indices,
    { NULL, _gActor560800Animation2EC58Bank1, NULL, NULL, _gActor560800Animation2EC58Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation2F0FCBank1[7] = {
#include "assets/actor_560800_animation_2F0FC_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation2F0FCBank4[86] = {
#include "assets/actor_560800_animation_2F0FC_bank4.inc"
};

static AnimationRecord _gActor560800Animation2F0FCRecords[170] = {
#include "assets/actor_560800_animation_2F0FC_records.inc"
};

static u16 _gActor560800Animation2F0FCIndices[20] = {
#include "assets/actor_560800_animation_2F0FC_indices.inc"
};

static AnimationSet _gActor560800Animation2F0FC = {
    _gActor560800Animation2F0FCRecords,
    _gActor560800Animation2F0FCIndices,
    { NULL, _gActor560800Animation2F0FCBank1, NULL, NULL, _gActor560800Animation2F0FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation2F728Bank1[16] = {
#include "assets/actor_560800_animation_2F728_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation2F728Bank4[137] = {
#include "assets/actor_560800_animation_2F728_bank4.inc"
};

static AnimationRecord _gActor560800Animation2F728Records[190] = {
#include "assets/actor_560800_animation_2F728_records.inc"
};

static u16 _gActor560800Animation2F728Indices[20] = {
#include "assets/actor_560800_animation_2F728_indices.inc"
};

static AnimationSet _gActor560800Animation2F728 = {
    _gActor560800Animation2F728Records,
    _gActor560800Animation2F728Indices,
    { NULL, _gActor560800Animation2F728Bank1, NULL, NULL, _gActor560800Animation2F728Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation2FF90Bank1[44] = {
#include "assets/actor_560800_animation_2FF90_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation2FF90Bank4[149] = {
#include "assets/actor_560800_animation_2FF90_bank4.inc"
};

static AnimationRecord _gActor560800Animation2FF90Records[237] = {
#include "assets/actor_560800_animation_2FF90_records.inc"
};

static u16 _gActor560800Animation2FF90Indices[20] = {
#include "assets/actor_560800_animation_2FF90_indices.inc"
};

static AnimationSet _gActor560800Animation2FF90 = {
    _gActor560800Animation2FF90Records,
    _gActor560800Animation2FF90Indices,
    { NULL, _gActor560800Animation2FF90Bank1, NULL, NULL, _gActor560800Animation2FF90Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation30290Bank1[8] = {
#include "assets/actor_560800_animation_30290_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation30290Bank4[33] = {
#include "assets/actor_560800_animation_30290_bank4.inc"
};

static AnimationRecord _gActor560800Animation30290Records[115] = {
#include "assets/actor_560800_animation_30290_records.inc"
};

static u16 _gActor560800Animation30290Indices[20] = {
#include "assets/actor_560800_animation_30290_indices.inc"
};

static AnimationSet _gActor560800Animation30290 = {
    _gActor560800Animation30290Records,
    _gActor560800Animation30290Indices,
    { NULL, _gActor560800Animation30290Bank1, NULL, NULL, _gActor560800Animation30290Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation30C2CBank1[41] = {
#include "assets/actor_560800_animation_30C2C_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation30C2CBank4[193] = {
#include "assets/actor_560800_animation_30C2C_bank4.inc"
};

static AnimationRecord _gActor560800Animation30C2CRecords[279] = {
#include "assets/actor_560800_animation_30C2C_records.inc"
};

static u16 _gActor560800Animation30C2CIndices[20] = {
#include "assets/actor_560800_animation_30C2C_indices.inc"
};

static AnimationSet _gActor560800Animation30C2C = {
    _gActor560800Animation30C2CRecords,
    _gActor560800Animation30C2CIndices,
    { NULL, _gActor560800Animation30C2CBank1, NULL, NULL, _gActor560800Animation30C2CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation31014Bank1[2] = {
#include "assets/actor_560800_animation_31014_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation31014Bank4[62] = {
#include "assets/actor_560800_animation_31014_bank4.inc"
};

static AnimationRecord _gActor560800Animation31014Records[162] = {
#include "assets/actor_560800_animation_31014_records.inc"
};

static u16 _gActor560800Animation31014Indices[20] = {
#include "assets/actor_560800_animation_31014_indices.inc"
};

static AnimationSet _gActor560800Animation31014 = {
    _gActor560800Animation31014Records,
    _gActor560800Animation31014Indices,
    { NULL, _gActor560800Animation31014Bank1, NULL, NULL, _gActor560800Animation31014Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation315E0Bank1[2] = {
#include "assets/actor_560800_animation_315E0_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation315E0Bank4[121] = {
#include "assets/actor_560800_animation_315E0_bank4.inc"
};

static AnimationRecord _gActor560800Animation315E0Records[224] = {
#include "assets/actor_560800_animation_315E0_records.inc"
};

static u16 _gActor560800Animation315E0Indices[20] = {
#include "assets/actor_560800_animation_315E0_indices.inc"
};

static AnimationSet _gActor560800Animation315E0 = {
    _gActor560800Animation315E0Records,
    _gActor560800Animation315E0Indices,
    { NULL, _gActor560800Animation315E0Bank1, NULL, NULL, _gActor560800Animation315E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation31A28Bank1[3] = {
#include "assets/actor_560800_animation_31A28_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation31A28Bank4[100] = {
#include "assets/actor_560800_animation_31A28_bank4.inc"
};

static AnimationRecord _gActor560800Animation31A28Records[145] = {
#include "assets/actor_560800_animation_31A28_records.inc"
};

static u16 _gActor560800Animation31A28Indices[20] = {
#include "assets/actor_560800_animation_31A28_indices.inc"
};

static AnimationSet _gActor560800Animation31A28 = {
    _gActor560800Animation31A28Records,
    _gActor560800Animation31A28Indices,
    { NULL, _gActor560800Animation31A28Bank1, NULL, NULL, _gActor560800Animation31A28Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation320A0Bank1[17] = {
#include "assets/actor_560800_animation_320A0_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation320A0Bank4[145] = {
#include "assets/actor_560800_animation_320A0_bank4.inc"
};

static AnimationRecord _gActor560800Animation320A0Records[198] = {
#include "assets/actor_560800_animation_320A0_records.inc"
};

static u16 _gActor560800Animation320A0Indices[20] = {
#include "assets/actor_560800_animation_320A0_indices.inc"
};

static AnimationSet _gActor560800Animation320A0 = {
    _gActor560800Animation320A0Records,
    _gActor560800Animation320A0Indices,
    { NULL, _gActor560800Animation320A0Bank1, NULL, NULL, _gActor560800Animation320A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation3254CBank1[2] = {
#include "assets/actor_560800_animation_3254C_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation3254CBank4[79] = {
#include "assets/actor_560800_animation_3254C_bank4.inc"
};

static AnimationRecord _gActor560800Animation3254CRecords[194] = {
#include "assets/actor_560800_animation_3254C_records.inc"
};

static u16 _gActor560800Animation3254CIndices[20] = {
#include "assets/actor_560800_animation_3254C_indices.inc"
};

static AnimationSet _gActor560800Animation3254C = {
    _gActor560800Animation3254CRecords,
    _gActor560800Animation3254CIndices,
    { NULL, _gActor560800Animation3254CBank1, NULL, NULL, _gActor560800Animation3254CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation32C24Bank1[2] = {
#include "assets/actor_560800_animation_32C24_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation32C24Bank4[177] = {
#include "assets/actor_560800_animation_32C24_bank4.inc"
};

static AnimationRecord _gActor560800Animation32C24Records[235] = {
#include "assets/actor_560800_animation_32C24_records.inc"
};

static u16 _gActor560800Animation32C24Indices[20] = {
#include "assets/actor_560800_animation_32C24_indices.inc"
};

static AnimationSet _gActor560800Animation32C24 = {
    _gActor560800Animation32C24Records,
    _gActor560800Animation32C24Indices,
    { NULL, _gActor560800Animation32C24Bank1, NULL, NULL, _gActor560800Animation32C24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation33998Bank1[21] = {
#include "assets/actor_560800_animation_33998_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation33998Bank4[342] = {
#include "assets/actor_560800_animation_33998_bank4.inc"
};

static AnimationRecord _gActor560800Animation33998Records[436] = {
#include "assets/actor_560800_animation_33998_records.inc"
};

static u16 _gActor560800Animation33998Indices[20] = {
#include "assets/actor_560800_animation_33998_indices.inc"
};

static AnimationSet _gActor560800Animation33998 = {
    _gActor560800Animation33998Records,
    _gActor560800Animation33998Indices,
    { NULL, _gActor560800Animation33998Bank1, NULL, NULL, _gActor560800Animation33998Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation33E18Bank1[17] = {
#include "assets/actor_560800_animation_33E18_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation33E18Bank4[87] = {
#include "assets/actor_560800_animation_33E18_bank4.inc"
};

static AnimationRecord _gActor560800Animation33E18Records[130] = {
#include "assets/actor_560800_animation_33E18_records.inc"
};

static u16 _gActor560800Animation33E18Indices[20] = {
#include "assets/actor_560800_animation_33E18_indices.inc"
};

static AnimationSet _gActor560800Animation33E18 = {
    _gActor560800Animation33E18Records,
    _gActor560800Animation33E18Indices,
    { NULL, _gActor560800Animation33E18Bank1, NULL, NULL, _gActor560800Animation33E18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation3404CBank1[2] = {
#include "assets/actor_560800_animation_3404C_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation3404CBank4[36] = {
#include "assets/actor_560800_animation_3404C_bank4.inc"
};

static AnimationRecord _gActor560800Animation3404CRecords[79] = {
#include "assets/actor_560800_animation_3404C_records.inc"
};

static u16 _gActor560800Animation3404CIndices[20] = {
#include "assets/actor_560800_animation_3404C_indices.inc"
};

static AnimationSet _gActor560800Animation3404C = {
    _gActor560800Animation3404CRecords,
    _gActor560800Animation3404CIndices,
    { NULL, _gActor560800Animation3404CBank1, NULL, NULL, _gActor560800Animation3404CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation34D08Bank1[67] = {
#include "assets/actor_560800_animation_34D08_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation34D08Bank4[227] = {
#include "assets/actor_560800_animation_34D08_bank4.inc"
};

static AnimationRecord _gActor560800Animation34D08Records[367] = {
#include "assets/actor_560800_animation_34D08_records.inc"
};

static u16 _gActor560800Animation34D08Indices[20] = {
#include "assets/actor_560800_animation_34D08_indices.inc"
};

static AnimationSet _gActor560800Animation34D08 = {
    _gActor560800Animation34D08Records,
    _gActor560800Animation34D08Indices,
    { NULL, _gActor560800Animation34D08Bank1, NULL, NULL, _gActor560800Animation34D08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation3552CBank1[25] = {
#include "assets/actor_560800_animation_3552C_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation3552CBank4[180] = {
#include "assets/actor_560800_animation_3552C_bank4.inc"
};

static AnimationRecord _gActor560800Animation3552CRecords[246] = {
#include "assets/actor_560800_animation_3552C_records.inc"
};

static u16 _gActor560800Animation3552CIndices[20] = {
#include "assets/actor_560800_animation_3552C_indices.inc"
};

static AnimationSet _gActor560800Animation3552C = {
    _gActor560800Animation3552CRecords,
    _gActor560800Animation3552CIndices,
    { NULL, _gActor560800Animation3552CBank1, NULL, NULL, _gActor560800Animation3552CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation35928Bank1[18] = {
#include "assets/actor_560800_animation_35928_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation35928Bank4[69] = {
#include "assets/actor_560800_animation_35928_bank4.inc"
};

static AnimationRecord _gActor560800Animation35928Records[112] = {
#include "assets/actor_560800_animation_35928_records.inc"
};

static u16 _gActor560800Animation35928Indices[20] = {
#include "assets/actor_560800_animation_35928_indices.inc"
};

static AnimationSet _gActor560800Animation35928 = {
    _gActor560800Animation35928Records,
    _gActor560800Animation35928Indices,
    { NULL, _gActor560800Animation35928Bank1, NULL, NULL, _gActor560800Animation35928Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation35C50Bank1[15] = {
#include "assets/actor_560800_animation_35C50_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation35C50Bank4[24] = {
#include "assets/actor_560800_animation_35C50_bank4.inc"
};

static AnimationRecord _gActor560800Animation35C50Records[113] = {
#include "assets/actor_560800_animation_35C50_records.inc"
};

static u16 _gActor560800Animation35C50Indices[20] = {
#include "assets/actor_560800_animation_35C50_indices.inc"
};

static AnimationSet _gActor560800Animation35C50 = {
    _gActor560800Animation35C50Records,
    _gActor560800Animation35C50Indices,
    { NULL, _gActor560800Animation35C50Bank1, NULL, NULL, _gActor560800Animation35C50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation361B8Bank1[26] = {
#include "assets/actor_560800_animation_361B8_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation361B8Bank4[95] = {
#include "assets/actor_560800_animation_361B8_bank4.inc"
};

static AnimationRecord _gActor560800Animation361B8Records[153] = {
#include "assets/actor_560800_animation_361B8_records.inc"
};

static u16 _gActor560800Animation361B8Indices[20] = {
#include "assets/actor_560800_animation_361B8_indices.inc"
};

static AnimationSet _gActor560800Animation361B8 = {
    _gActor560800Animation361B8Records,
    _gActor560800Animation361B8Indices,
    { NULL, _gActor560800Animation361B8Bank1, NULL, NULL, _gActor560800Animation361B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation367B4Bank1[22] = {
#include "assets/actor_560800_animation_367B4_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation367B4Bank4[124] = {
#include "assets/actor_560800_animation_367B4_bank4.inc"
};

static AnimationRecord _gActor560800Animation367B4Records[173] = {
#include "assets/actor_560800_animation_367B4_records.inc"
};

static u16 _gActor560800Animation367B4Indices[20] = {
#include "assets/actor_560800_animation_367B4_indices.inc"
};

static AnimationSet _gActor560800Animation367B4 = {
    _gActor560800Animation367B4Records,
    _gActor560800Animation367B4Indices,
    { NULL, _gActor560800Animation367B4Bank1, NULL, NULL, _gActor560800Animation367B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation371E8Bank1[46] = {
#include "assets/actor_560800_animation_371E8_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation371E8Bank4[175] = {
#include "assets/actor_560800_animation_371E8_bank4.inc"
};

static AnimationRecord _gActor560800Animation371E8Records[320] = {
#include "assets/actor_560800_animation_371E8_records.inc"
};

static u16 _gActor560800Animation371E8Indices[20] = {
#include "assets/actor_560800_animation_371E8_indices.inc"
};

static AnimationSet _gActor560800Animation371E8 = {
    _gActor560800Animation371E8Records,
    _gActor560800Animation371E8Indices,
    { NULL, _gActor560800Animation371E8Bank1, NULL, NULL, _gActor560800Animation371E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation37F5CBank1[46] = {
#include "assets/actor_560800_animation_37F5C_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation37F5CBank4[259] = {
#include "assets/actor_560800_animation_37F5C_bank4.inc"
};

static AnimationRecord _gActor560800Animation37F5CRecords[444] = {
#include "assets/actor_560800_animation_37F5C_records.inc"
};

static u16 _gActor560800Animation37F5CIndices[20] = {
#include "assets/actor_560800_animation_37F5C_indices.inc"
};

static AnimationSet _gActor560800Animation37F5C = {
    _gActor560800Animation37F5CRecords,
    _gActor560800Animation37F5CIndices,
    { NULL, _gActor560800Animation37F5CBank1, NULL, NULL, _gActor560800Animation37F5CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation38288Bank1[2] = {
#include "assets/actor_560800_animation_38288_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation38288Bank4[47] = {
#include "assets/actor_560800_animation_38288_bank4.inc"
};

static AnimationRecord _gActor560800Animation38288Records[130] = {
#include "assets/actor_560800_animation_38288_records.inc"
};

static u16 _gActor560800Animation38288Indices[20] = {
#include "assets/actor_560800_animation_38288_indices.inc"
};

static AnimationSet _gActor560800Animation38288 = {
    _gActor560800Animation38288Records,
    _gActor560800Animation38288Indices,
    { NULL, _gActor560800Animation38288Bank1, NULL, NULL, _gActor560800Animation38288Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation389F0Bank1[35] = {
#include "assets/actor_560800_animation_389F0_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation389F0Bank4[141] = {
#include "assets/actor_560800_animation_389F0_bank4.inc"
};

static AnimationRecord _gActor560800Animation389F0Records[208] = {
#include "assets/actor_560800_animation_389F0_records.inc"
};

static u16 _gActor560800Animation389F0Indices[20] = {
#include "assets/actor_560800_animation_389F0_indices.inc"
};

static AnimationSet _gActor560800Animation389F0 = {
    _gActor560800Animation389F0Records,
    _gActor560800Animation389F0Indices,
    { NULL, _gActor560800Animation389F0Bank1, NULL, NULL, _gActor560800Animation389F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation38E5CBank1[17] = {
#include "assets/actor_560800_animation_38E5C_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation38E5CBank4[83] = {
#include "assets/actor_560800_animation_38E5C_bank4.inc"
};

static AnimationRecord _gActor560800Animation38E5CRecords[129] = {
#include "assets/actor_560800_animation_38E5C_records.inc"
};

static u16 _gActor560800Animation38E5CIndices[20] = {
#include "assets/actor_560800_animation_38E5C_indices.inc"
};

static AnimationSet _gActor560800Animation38E5C = {
    _gActor560800Animation38E5CRecords,
    _gActor560800Animation38E5CIndices,
    { NULL, _gActor560800Animation38E5CBank1, NULL, NULL, _gActor560800Animation38E5CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation39368Bank1[2] = {
#include "assets/actor_560800_animation_39368_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation39368Bank4[94] = {
#include "assets/actor_560800_animation_39368_bank4.inc"
};

static AnimationRecord _gActor560800Animation39368Records[203] = {
#include "assets/actor_560800_animation_39368_records.inc"
};

static u16 _gActor560800Animation39368Indices[20] = {
#include "assets/actor_560800_animation_39368_indices.inc"
};

static AnimationSet _gActor560800Animation39368 = {
    _gActor560800Animation39368Records,
    _gActor560800Animation39368Indices,
    { NULL, _gActor560800Animation39368Bank1, NULL, NULL, _gActor560800Animation39368Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation39F6CBank1[92] = {
#include "assets/actor_560800_animation_39F6C_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation39F6CBank4[173] = {
#include "assets/actor_560800_animation_39F6C_bank4.inc"
};

static AnimationRecord _gActor560800Animation39F6CRecords[300] = {
#include "assets/actor_560800_animation_39F6C_records.inc"
};

static u16 _gActor560800Animation39F6CIndices[20] = {
#include "assets/actor_560800_animation_39F6C_indices.inc"
};

static AnimationSet _gActor560800Animation39F6C = {
    _gActor560800Animation39F6CRecords,
    _gActor560800Animation39F6CIndices,
    { NULL, _gActor560800Animation39F6CBank1, NULL, NULL, _gActor560800Animation39F6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation3A4ECBank1[8] = {
#include "assets/actor_560800_animation_3A4EC_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation3A4ECBank4[89] = {
#include "assets/actor_560800_animation_3A4EC_bank4.inc"
};

static AnimationRecord _gActor560800Animation3A4ECRecords[219] = {
#include "assets/actor_560800_animation_3A4EC_records.inc"
};

static u16 _gActor560800Animation3A4ECIndices[20] = {
#include "assets/actor_560800_animation_3A4EC_indices.inc"
};

static AnimationSet _gActor560800Animation3A4EC = {
    _gActor560800Animation3A4ECRecords,
    _gActor560800Animation3A4ECIndices,
    { NULL, _gActor560800Animation3A4ECBank1, NULL, NULL, _gActor560800Animation3A4ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation3A804Bank1[9] = {
#include "assets/actor_560800_animation_3A804_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation3A804Bank4[55] = {
#include "assets/actor_560800_animation_3A804_bank4.inc"
};

static AnimationRecord _gActor560800Animation3A804Records[96] = {
#include "assets/actor_560800_animation_3A804_records.inc"
};

static u16 _gActor560800Animation3A804Indices[20] = {
#include "assets/actor_560800_animation_3A804_indices.inc"
};

static AnimationSet _gActor560800Animation3A804 = {
    _gActor560800Animation3A804Records,
    _gActor560800Animation3A804Indices,
    { NULL, _gActor560800Animation3A804Bank1, NULL, NULL, _gActor560800Animation3A804Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation3B170Bank1[43] = {
#include "assets/actor_560800_animation_3B170_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation3B170Bank4[181] = {
#include "assets/actor_560800_animation_3B170_bank4.inc"
};

static AnimationRecord _gActor560800Animation3B170Records[273] = {
#include "assets/actor_560800_animation_3B170_records.inc"
};

static u16 _gActor560800Animation3B170Indices[20] = {
#include "assets/actor_560800_animation_3B170_indices.inc"
};

static AnimationSet _gActor560800Animation3B170 = {
    _gActor560800Animation3B170Records,
    _gActor560800Animation3B170Indices,
    { NULL, _gActor560800Animation3B170Bank1, NULL, NULL, _gActor560800Animation3B170Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation3B78CBank1[13] = {
#include "assets/actor_560800_animation_3B78C_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation3B78CBank4[120] = {
#include "assets/actor_560800_animation_3B78C_bank4.inc"
};

static AnimationRecord _gActor560800Animation3B78CRecords[212] = {
#include "assets/actor_560800_animation_3B78C_records.inc"
};

static u16 _gActor560800Animation3B78CIndices[20] = {
#include "assets/actor_560800_animation_3B78C_indices.inc"
};

static AnimationSet _gActor560800Animation3B78C = {
    _gActor560800Animation3B78CRecords,
    _gActor560800Animation3B78CIndices,
    { NULL, _gActor560800Animation3B78CBank1, NULL, NULL, _gActor560800Animation3B78CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation3BF2CBank1[38] = {
#include "assets/actor_560800_animation_3BF2C_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation3BF2CBank4[143] = {
#include "assets/actor_560800_animation_3BF2C_bank4.inc"
};

static AnimationRecord _gActor560800Animation3BF2CRecords[211] = {
#include "assets/actor_560800_animation_3BF2C_records.inc"
};

static u16 _gActor560800Animation3BF2CIndices[20] = {
#include "assets/actor_560800_animation_3BF2C_indices.inc"
};

static AnimationSet _gActor560800Animation3BF2C = {
    _gActor560800Animation3BF2CRecords,
    _gActor560800Animation3BF2CIndices,
    { NULL, _gActor560800Animation3BF2CBank1, NULL, NULL, _gActor560800Animation3BF2CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation3C070Bank1[2] = {
#include "assets/actor_560800_animation_3C070_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation3C070Bank4[17] = {
#include "assets/actor_560800_animation_3C070_bank4.inc"
};

static AnimationRecord _gActor560800Animation3C070Records[38] = {
#include "assets/actor_560800_animation_3C070_records.inc"
};

static u16 _gActor560800Animation3C070Indices[20] = {
#include "assets/actor_560800_animation_3C070_indices.inc"
};

static AnimationSet _gActor560800Animation3C070 = {
    _gActor560800Animation3C070Records,
    _gActor560800Animation3C070Indices,
    { NULL, _gActor560800Animation3C070Bank1, NULL, NULL, _gActor560800Animation3C070Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation3CBE0Bank1[13] = {
#include "assets/actor_560800_animation_3CBE0_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation3CBE0Bank4[259] = {
#include "assets/actor_560800_animation_3CBE0_bank4.inc"
};

static AnimationRecord _gActor560800Animation3CBE0Records[414] = {
#include "assets/actor_560800_animation_3CBE0_records.inc"
};

static u16 _gActor560800Animation3CBE0Indices[20] = {
#include "assets/actor_560800_animation_3CBE0_indices.inc"
};

static AnimationSet _gActor560800Animation3CBE0 = {
    _gActor560800Animation3CBE0Records,
    _gActor560800Animation3CBE0Indices,
    { NULL, _gActor560800Animation3CBE0Bank1, NULL, NULL, _gActor560800Animation3CBE0Bank4, NULL, NULL, NULL },
};

enum {
    ACTOR_560800_MOVIE_TASK_PLAYBACK = 1,
};

TaskDesc gActor560800MovieTaskDescriptors[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _actor560800StartMovieTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _actor560800MovieTask, { .value = 0 } },
};

/// Scene animation bank borrowed by Aya's player task.
///
/// Clip IDs 0..5 and 10..12 are populated; 6..9 are absent. Requests install this
/// bank; scene clip IDs also index its clip chain. Keep the overlay and clip data
/// loaded until the player has returned to its equipped-weapon animation bank.
static AnimationSet* _gActor560800PlayerAnimationSets[13] = {
    &_gActor560800Animation27A30,
    &_gActor560800Animation27ED8,
    &_gActor560800Animation288D4,
    &_gActor560800Animation292D0,
    &_gActor560800Animation295E0,
    &_gActor560800Animation29894,
    NULL,
    NULL,
    NULL,
    NULL,
    &_gActor560800Animation2A294,
    &_gActor560800Animation2A50C,
    &_gActor560800Animation29B98,
};

/// Clip bank borrowed by Kyle's twenty-slot scene rig.
///
/// Clip ids 0..35 index both this bank and `_gActor560800KyleAnimationChain`;
/// 23 and 24 are absent and are never requested. Non-root slots 1..19 borrow
/// the selected clip's tracks. Keep the bank and clip data loaded through
/// body teardown.
static AnimationSet* _gActor560800KyleAnimationSets[36] = {
    &_gActor560800Animation1EE00,
    &_gActor560800Animation1F0F8,
    &_gActor560800Animation1F2E4,
    &_gActor560800Animation1F6B8,
    &_gActor560800Animation20810,
    &_gActor560800Animation20D5C,
    &_gActor560800Animation210F4,
    &_gActor560800Animation2135C,
    &_gActor560800Animation217E0,
    &_gActor560800Animation21C64,
    &_gActor560800Animation21EAC,
    &_gActor560800Animation221E0,
    &_gActor560800Animation223C4,
    &_gActor560800Animation227D4,
    &_gActor560800Animation22B54,
    &_gActor560800Animation22E5C,
    &_gActor560800Animation23268,
    &_gActor560800Animation2361C,
    &_gActor560800Animation23A7C,
    &_gActor560800Animation23DD0,
    &_gActor560800Animation24008,
    &_gActor560800Animation24778,
    &_gActor560800Animation24A6C,
    NULL,
    NULL,
    &_gActor560800Animation2547C,
    &_gActor560800Animation24CE8,
    &_gActor560800Animation26ED0,
    &_gActor560800Animation2708C,
    &_gActor560800Animation25A60,
    &_gActor560800Animation26AB4,
    &_gActor560800Animation27280,
    &_gActor560800Animation274F8,
    &_gActor560800Animation268DC,
    &_gActor560800Animation25F18,
    &_gActor560800Animation26260,
};

/// Clip bank shared by Eve's masked and replacement scene bodies.
///
/// The nineteen-slot rigs borrow clips 0..5; slots 6..10 are NULL and are
/// never requested. The six live ids also index `_gActor560800EveAnimationChain`.
/// Keep the bank and clip data loaded through teardown of both bodies.
static AnimationSet* _gActor560800EveAnimationSets[11] = {
    &_gActor560800Animation2A7D0,
    &_gActor560800Animation2AABC,
    &_gActor560800Animation2AC68,
    &_gActor560800Animation2AE40,
    &_gActor560800Animation2B020,
    &_gActor560800Animation2B2DC,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

/// Clip bank borrowed by No. 9's nineteen-slot scene rig.
///
/// Live clip ids are 0..41 and 45; 42..44 are absent and never requested.
/// Ids also index the body's 46-link chain. Non-root slots 1..18 borrow the
/// selected clip's tracks until body teardown.
static AnimationSet* _gActor560800No9AnimationSets[46] = {
    &_gActor560800Animation2B9B4,
    &_gActor560800Animation2BD38,
    &_gActor560800Animation2BF38,
    &_gActor560800Animation2C0F4,
    &_gActor560800Animation2C728,
    &_gActor560800Animation2CDEC,
    &_gActor560800Animation2DA6C,
    &_gActor560800Animation2E370,
    &_gActor560800Animation2EC58,
    &_gActor560800Animation2F0FC,
    &_gActor560800Animation2F728,
    &_gActor560800Animation2FF90,
    &_gActor560800Animation30290,
    &_gActor560800Animation30C2C,
    &_gActor560800Animation31014,
    &_gActor560800Animation315E0,
    &_gActor560800Animation31A28,
    &_gActor560800Animation320A0,
    &_gActor560800Animation3254C,
    &_gActor560800Animation32C24,
    &_gActor560800Animation33998,
    &_gActor560800Animation33E18,
    &_gActor560800Animation3404C,
    &_gActor560800Animation34D08,
    &_gActor560800Animation3552C,
    &_gActor560800Animation35928,
    &_gActor560800Animation35C50,
    &_gActor560800Animation361B8,
    &_gActor560800Animation367B4,
    &_gActor560800Animation371E8,
    &_gActor560800Animation37F5C,
    &_gActor560800Animation38288,
    &_gActor560800Animation389F0,
    &_gActor560800Animation38E5C,
    &_gActor560800Animation39368,
    &_gActor560800Animation39F6C,
    &_gActor560800Animation3A4EC,
    &_gActor560800Animation3A804,
    &_gActor560800Animation3B170,
    &_gActor560800Animation3B78C,
    &_gActor560800Animation3BF2C,
    &_gActor560800Animation3C070,
    NULL,
    NULL,
    NULL,
    &_gActor560800Animation3CBE0,
};

/// Link modes and terminal successor for this scene's clip chains.
///
/// Zero hold waits for playback to settle. Maximum hold is 65535 updates for
/// unsigned cast counters; Aya's signed counter cannot reach that value.
enum {
    ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END = 0,
    ACTOR_560800_ANIMATION_CHAIN_MAX_HOLD      = 0xFFFF,
    ACTOR_560800_ANIMATION_CHAIN_END           = -1,
};

/// Successor and hold rules indexed by Aya's scene clip id.
///
/// The scene requests populated ids 0..5, 10 and 12; clip 1 advances to 10,
/// which holds for 90 updates. Every other link is terminal. Zero hold waits
/// for player playback to end; clip 12's maximum hold cannot be reached by
/// the signed-halfword counter. Weapon-bank requests leave this scene id intact.
static ActorAnimChainLink _gActor560800PlayerAnimationChain[ARRAY_SIZE(_gActor560800PlayerAnimationSets)] = {
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 10 },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { 90, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_MAX_HOLD, ACTOR_560800_ANIMATION_CHAIN_END },
};

/// Successor and hold rules indexed by Kyle's scene clip id.
///
/// One link per bank slot (0..35); absent clips 23/24 are never selected.
/// Zero hold waits for the non-root tracks to settle; timed holds count visible,
/// unpaused body updates. Links blend to their successor over ten frames,
/// retaining the body's recorded rate. A negative successor is terminal.
static ActorAnimChainLink _gActor560800KyleAnimationChain[ARRAY_SIZE(_gActor560800KyleAnimationSets)] = {
    { ACTOR_560800_ANIMATION_CHAIN_MAX_HOLD, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 2 },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 4 },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { 100, 6 },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 26 },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_MAX_HOLD, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 13 },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { 40, 28 },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 26 },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 26 },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 26 },
    { 60, 35 },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 26 },
};

/// Terminal links for Eve's six populated scene clips.
///
/// Ids 0..5 are reached by body initialization, cues and scripted blends.
/// Each link waits for the non-root tracks to settle and then reports completion;
/// none selects another clip. Hidden updates leave playback unchanged.
static ActorAnimChainLink _gActor560800EveAnimationChain[6] = {
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
};

/// No. 9 scene-clip successor and hold rules, indexed by clip id 0..45.
///
/// Zero hold waits for non-root playback to settle; 65535 waits that many
/// visible updates. Negative successors are terminal. Scene callbacks and cut
/// cues select populated ids 0..41 or 45; successors also stay in that domain,
/// leaving absent bank slots 42..44 unused. The body borrows this table through
/// teardown; hidden updates neither tick its playback nor advance its hold.
static ActorAnimChainLink _gActor560800No9AnimationChain[ARRAY_SIZE(_gActor560800No9AnimationSets)] = {
    { ACTOR_560800_ANIMATION_CHAIN_MAX_HOLD, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_MAX_HOLD, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 3 },
    { ACTOR_560800_ANIMATION_CHAIN_MAX_HOLD, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_MAX_HOLD, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 6 },
    { ACTOR_560800_ANIMATION_CHAIN_MAX_HOLD, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 8 },
    { ACTOR_560800_ANIMATION_CHAIN_MAX_HOLD, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_MAX_HOLD, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_MAX_HOLD, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 12 },
    { ACTOR_560800_ANIMATION_CHAIN_MAX_HOLD, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 14 },
    { ACTOR_560800_ANIMATION_CHAIN_MAX_HOLD, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 14 },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 18 },
    { ACTOR_560800_ANIMATION_CHAIN_MAX_HOLD, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 22 },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 18 },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 15 },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 26 },
    { ACTOR_560800_ANIMATION_CHAIN_MAX_HOLD, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 14 },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 15 },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 30 },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 31 },
    { ACTOR_560800_ANIMATION_CHAIN_MAX_HOLD, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, 34 },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_MAX_HOLD, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_MAX_HOLD, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_MAX_HOLD, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_WAIT_CLIP_END, ACTOR_560800_ANIMATION_CHAIN_END },
    { ACTOR_560800_ANIMATION_CHAIN_MAX_HOLD, ACTOR_560800_ANIMATION_CHAIN_END },
};

/// Hide marker for Aya in scene cuts 1, 4, 10, 13.
///
/// Zero position X selects hiding; no placement message reads the remaining fields.
static ActorTransform _gActor560800PlayerCut1Hidden = { 0 };

/// Hide marker for Aya in scene cuts 2, 23.
///
/// Zero position X selects hiding; no placement message reads the remaining fields.
static ActorTransform _gActor560800PlayerCut2Hidden = { 0 };

/// Root placement for Aya in scene cuts 3, 5, 9, 12.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800PlayerCut3Placement = { { 6400, 0, 3350, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Hide marker for Aya in scene cuts 6, 8, 11.
///
/// Zero position X selects hiding; no placement message reads the remaining fields.
static ActorTransform _gActor560800PlayerCut6Hidden = { 0 };

/// Root placement for Aya in scene cuts 7, 19.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800PlayerCut7Placement = { { 5400, 0, 3350, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Root placement for Aya in scene cuts 14, 16, 18, 20.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800PlayerCut14Placement = { { 6000, 0, 3200, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Hide marker for Aya in scene cuts 15, 17.
///
/// Zero position X selects hiding; no placement message reads the remaining fields.
static ActorTransform _gActor560800PlayerCut15Hidden = { 0 };

/// Root placement for Aya in scene cut 21.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800PlayerCut21Placement = { { 5850, 0, 3200, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Hide marker for Aya in scene cuts 22, 24.
///
/// Zero position X selects hiding; no placement message reads the remaining fields.
static ActorTransform _gActor560800PlayerCut22Hidden = { 0 };

/// Hide marker for Aya in scene cut 25.
///
/// Zero position X selects hiding; no placement message reads the remaining fields.
static ActorTransform _gActor560800PlayerCut25Hidden = { 0 };

/// Hide marker for Aya in scene cut 26.
///
/// Zero position X selects hiding; no placement message reads the remaining fields.
static ActorTransform _gActor560800PlayerCut26Hidden = { 0 };

/// Hide marker for Aya in scene cut 27.
///
/// Zero position X selects hiding; no placement message reads the remaining fields.
static ActorTransform _gActor560800PlayerCut27Hidden = { 0 };

/// Aya placement for scene cuts 28, 30 and 32, followed by identical record bytes.
///
/// The cut table selects only element 0. The second complete record has the same
/// position and quarter-turn yaw; its role is unproven. Both remain in storage.
static ActorTransform _gActor560800PlayerCut28Placements[2] = {
    { { 5850, 0, 3200, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } }, // Cuts 28, 30 and 32
    { { 5850, 0, 3200, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } }, // Never selected; role unproven
};

/// Root placement for Aya in scene cut 33.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800PlayerCut33Placement = { { 5850, 0, 3200, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Root placement for Aya in scene cuts 29, 31.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800PlayerCut29Placement = { { 5000, 0, 3200, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Root placement for No. 9 in scene cuts 1, 4, 10, 13.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800No9Cut1Placement = { { 7500, 0, 3000, 0 }, { 0, 0, 0, 0 } };

/// Root placement for No. 9 in scene cuts 2, 23.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800No9Cut2Placement = { { 7850, 0, 3400, 0 }, { 0, 0, 0, 0 } };

/// Hide marker for No. 9 in scene cuts 3, 5, 9, 12.
///
/// Zero position X selects hiding; no placement message reads the remaining fields.
static ActorTransform _gActor560800No9Cut3Hidden = { 0 };

/// Root placement for No. 9 in scene cuts 6, 8, 11.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800No9Cut6Placement = { { 7500, 0, 3400, 0 }, { 0, 0, 0, 0 } };

/// Hide marker for No. 9 in scene cuts 7, 19.
///
/// Zero position X selects hiding; no placement message reads the remaining fields.
static ActorTransform _gActor560800No9Cut7Hidden = { 0 };

/// Hide marker for No. 9 in scene cuts 14, 16, 18, 20.
///
/// Zero position X selects hiding; no placement message reads the remaining fields.
static ActorTransform _gActor560800No9Cut14Hidden = { 0 };

/// Root placement for No. 9 in scene cuts 15, 17.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800No9Cut15Placement = { { 9150, 0, 3750, 0 }, { 0, -170, 0, 0 } };

/// Hide marker for No. 9 in scene cut 21.
///
/// Zero position X selects hiding; no placement message reads the remaining fields.
static ActorTransform _gActor560800No9Cut21Hidden = { 0 };

/// Root placement for No. 9 in scene cuts 22, 24.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800No9Cut22Placement = { { 8050, 0, 3700, 0 }, { 0, 0, 0, 0 } };

/// Hide marker for No. 9 in scene cut 25.
///
/// Zero position X selects hiding; no placement message reads the remaining fields.
static ActorTransform _gActor560800No9Cut25Hidden = { 0 };

/// Root placement for No. 9 in scene cut 26.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800No9Cut26Placement = { { 6950, 0, 3650, 0 }, { 0, 3 * ACTOR_TRANSFORM_ANGLE_TURN / 8, 0, 0 } };

/// Root placement for No. 9 in scene cut 27.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800No9Cut27Placement = { { 6950, 0, 3650, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 3, 0, 0 } };

/// Hide marker for No. 9 in scene cuts 28, 30, 32.
///
/// Zero position X selects hiding; no placement message reads the remaining fields.
static ActorTransform _gActor560800No9Cut28Hidden = { 0 };

/// Hide marker for No. 9 in scene cuts 29, 31.
///
/// Zero position X selects hiding; no placement message reads the remaining fields.
static ActorTransform _gActor560800No9Cut29Hidden = { 0 };

/// Hide marker for No. 9 in scene cut 33.
///
/// Zero position X selects hiding; no placement message reads the remaining fields.
static ActorTransform _gActor560800No9Cut33Hidden = { 0 };

/// Root placement for Eve in scene cuts 1, 4, 10, 13.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800EveCut1Placement = { { 8350, 0, 4000, 0 }, { 0, 0, 0, 0 } };

/// Root placement for Eve in scene cuts 2, 23.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800EveCut2Placement = { { 8350, 0, 4000, 0 }, { 0, 0, 0, 0 } };

/// Hide marker for Eve in scene cuts 3, 5, 9, 12.
///
/// Zero position X selects hiding; no placement message reads the remaining fields.
static ActorTransform _gActor560800EveCut3Hidden = { 0 };

/// Root placement for Eve in scene cuts 6, 8, 11.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800EveCut6Placement = { { 8350, 0, 4000, 0 }, { 0, 0, 0, 0 } };

/// Hide marker for Eve in scene cuts 7, 19.
///
/// Zero position X selects hiding; no placement message reads the remaining fields.
static ActorTransform _gActor560800EveCut7Hidden = { 0 };

/// Hide marker for Eve in scene cuts 14, 16, 18, 20.
///
/// Zero position X selects hiding; no placement message reads the remaining fields.
static ActorTransform _gActor560800EveCut14Hidden = { 0 };

/// Hide marker for Eve in scene cuts 15, 17.
///
/// Zero position X selects hiding; no placement message reads the remaining fields.
static ActorTransform _gActor560800EveCut15Hidden = { 0 };

/// Hide marker for Eve in scene cut 21.
///
/// Zero position X selects hiding; no placement message reads the remaining fields.
static ActorTransform _gActor560800EveCut21Hidden = { 0 };

/// Root placement for Eve in scene cuts 22, 24.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800EveCut22Placement = { { 8350, 0, 4300, 0 }, { 0, 0, 0, 0 } };

/// Root placement for Eve in scene cut 25.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800EveCut25Placement = { { 8340, 0, 4000, 0 }, { 0, 0, 0, 0 } };

/// Root placement of Eve's replacement body and of her body in cut 26.
///
/// Whole game units under the view coordinate and quarter-turn yaw. The body
/// uses this record after the texture/model swap; dispatch retains no pointer.
static ActorTransform _gActor560800EveReplacementPlacement = { { 8130, 0, 2850, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Hide marker for Eve in scene cut 27.
///
/// Zero position X selects hiding; no placement message reads the remaining fields.
static ActorTransform _gActor560800EveCut27Hidden = { 0 };

/// Root placement for Eve in scene cuts 28, 30, 32.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800EveCut28Placement = { { 8700, 0, 3200, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Hide marker for Eve in scene cuts 29, 31.
///
/// Zero position X selects hiding; no placement message reads the remaining fields.
static ActorTransform _gActor560800EveCut29Hidden = { 0 };

/// Root placement for Eve in scene cut 33.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800EveCut33Placement = { { 8600, 0, 3500, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 8, 0, 0 } };

enum { ACTOR_560800_KYLE_APPROACH_PLACEMENT = 5 };

/// Kyle's six reusable scene placements and hide markers.
///
/// Indices 0..5: cuts 1/4/10/13; 2/23; hidden 3/5/9/12; 6/8/11;
/// hidden 7/19; approach endpoint in cut 14 and placement in 16/18/20.
/// Positions use whole game units under the view coordinate; yaw uses 4096
/// units per turn. Dispatch borrows a single record and retains no pointer.
static ActorTransform _gActor560800KyleCutPlacements[6] = {
    { { 9400, 0, 2600, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 8, 0, 0 } },
    { { 9400, 0, 2600, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } },
    { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } },
    { { 9400, 0, 2600, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } },
    { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } },
    { { 7800, 0, 3200, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } },
};

/// Root placement for Kyle in scene cuts 15, 17.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800KyleCut15Placement = { { 7800, 0, 3200, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Root placement for Kyle in scene cut 21.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800KyleCut21Placement = { { 7800, 0, 3200, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Root placement for Kyle in scene cuts 22, 24.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800KyleCut22Placement = { { 6350, 0, 3200, 0 }, { 0, -967, 0, 0 } };

/// Root placement for Kyle in scene cut 25.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800KyleCut25Placement = { { 6350, 0, 3200, 0 }, { 0, 796, 0, 0 } };

/// Root placement for Kyle in scene cut 26.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800KyleCut26Placement = { { 8450, 0, 2850, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Hide marker for Kyle in scene cut 27.
///
/// Zero position X suppresses placement and hides his body, hands and handgun.
/// The other components of this record are not consumed.
static ActorTransform _gActor560800KyleCut27Hidden = { 0 };

/// Root placement for Kyle in scene cuts 28, 30 and 32.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800KyleCut28Placement = { { 8450, 0, 2850, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Hide marker for Kyle in scene cuts 29 and 31.
///
/// Zero position X suppresses placement and hides his body, hands and handgun.
/// The other components of this record are not consumed.
static ActorTransform _gActor560800KyleCut29Hidden = { 0 };

/// Root placement for Kyle in scene cut 33.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800KyleCut33Placement = { { 8450, 0, 2850, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 8, 0, 0 } };

/// Starting root placement for Kyle's approach in scene cut 14.
///
/// Position uses whole game units under the view coordinate; yaw uses 4096 units
/// per turn. The placement message borrows the record only during dispatch.
static ActorTransform _gActor560800KyleCut14Placement = { { 10800, 0, 3200, 0 }, { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Messages accepted by the scene bodies and their hand/weapon attachments.
///
/// No end marker: send only `ACTOR_MESSAGE_SET_MODEL_DRAW` with an integer
/// draw mode (0 no change, 1 show, 2 hide/skip automatic buffering), or
/// `ACTOR_MESSAGE_PLACE` with a borrowed `ActorTransform` in the root parent's
/// frame (Y/X/Z Euler order, 4096 units per turn). Dispatch is synchronous;
/// the callbacks return void, so every sender must ignore the result.
static TaskMessageEntry _gActor560800CastMessages[2] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor560800SetCastModelDraw },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceYawPitchRoll },
};

/// Aya's root placement or hide marker indexed by scene cut.
///
/// Cuts 1..33 have non-NULL entries; slot 0 is NULL and must not be used.
/// Nonzero position X shows and places the actor, while zero X hides it.
/// Positions use whole game units under the view coordinate; Euler angles use
/// 4096 units per turn. Entries borrow one record synchronously during dispatch;
/// the table and records must remain loaded through scene teardown.
static const ActorTransform* _gActor560800PlayerPlacementsByCut[34] = {
    NULL,
    &_gActor560800PlayerCut1Hidden,
    &_gActor560800PlayerCut2Hidden,
    &_gActor560800PlayerCut3Placement,
    &_gActor560800PlayerCut1Hidden,
    &_gActor560800PlayerCut3Placement,
    &_gActor560800PlayerCut6Hidden,
    &_gActor560800PlayerCut7Placement,
    &_gActor560800PlayerCut6Hidden,
    &_gActor560800PlayerCut3Placement,
    &_gActor560800PlayerCut1Hidden,
    &_gActor560800PlayerCut6Hidden,
    &_gActor560800PlayerCut3Placement,
    &_gActor560800PlayerCut1Hidden,
    &_gActor560800PlayerCut14Placement,
    &_gActor560800PlayerCut15Hidden,
    &_gActor560800PlayerCut14Placement,
    &_gActor560800PlayerCut15Hidden,
    &_gActor560800PlayerCut14Placement,
    &_gActor560800PlayerCut7Placement,
    &_gActor560800PlayerCut14Placement,
    &_gActor560800PlayerCut21Placement,
    &_gActor560800PlayerCut22Hidden,
    &_gActor560800PlayerCut2Hidden,
    &_gActor560800PlayerCut22Hidden,
    &_gActor560800PlayerCut25Hidden,
    &_gActor560800PlayerCut26Hidden,
    &_gActor560800PlayerCut27Hidden,
    _gActor560800PlayerCut28Placements,
    &_gActor560800PlayerCut29Placement,
    _gActor560800PlayerCut28Placements,
    &_gActor560800PlayerCut29Placement,
    _gActor560800PlayerCut28Placements,
    &_gActor560800PlayerCut33Placement,
};

/// No. 9's root placement or hide marker indexed by scene cut.
///
/// Cuts 1..33 have non-NULL entries; slot 0 is NULL and must not be used.
/// Nonzero position X shows and places the actor, while zero X hides it.
/// Positions use whole game units under the view coordinate; Euler angles use
/// 4096 units per turn. Entries borrow one record synchronously during dispatch;
/// the table and records must remain loaded through scene teardown.
static const ActorTransform* _gActor560800No9PlacementsByCut[34] = {
    NULL,
    &_gActor560800No9Cut1Placement,
    &_gActor560800No9Cut2Placement,
    &_gActor560800No9Cut3Hidden,
    &_gActor560800No9Cut1Placement,
    &_gActor560800No9Cut3Hidden,
    &_gActor560800No9Cut6Placement,
    &_gActor560800No9Cut7Hidden,
    &_gActor560800No9Cut6Placement,
    &_gActor560800No9Cut3Hidden,
    &_gActor560800No9Cut1Placement,
    &_gActor560800No9Cut6Placement,
    &_gActor560800No9Cut3Hidden,
    &_gActor560800No9Cut1Placement,
    &_gActor560800No9Cut14Hidden,
    &_gActor560800No9Cut15Placement,
    &_gActor560800No9Cut14Hidden,
    &_gActor560800No9Cut15Placement,
    &_gActor560800No9Cut14Hidden,
    &_gActor560800No9Cut7Hidden,
    &_gActor560800No9Cut14Hidden,
    &_gActor560800No9Cut21Hidden,
    &_gActor560800No9Cut22Placement,
    &_gActor560800No9Cut2Placement,
    &_gActor560800No9Cut22Placement,
    &_gActor560800No9Cut25Hidden,
    &_gActor560800No9Cut26Placement,
    &_gActor560800No9Cut27Placement,
    &_gActor560800No9Cut28Hidden,
    &_gActor560800No9Cut29Hidden,
    &_gActor560800No9Cut28Hidden,
    &_gActor560800No9Cut29Hidden,
    &_gActor560800No9Cut28Hidden,
    &_gActor560800No9Cut33Hidden,
};

/// Kyle's root placement or hide marker indexed by scene cut.
///
/// Cuts 1..33 have non-NULL entries; slot 0 is NULL and must not be used.
/// Nonzero position X shows and places the actor, while zero X hides it.
/// Positions use whole game units under the view coordinate; Euler angles use
/// 4096 units per turn. Entries borrow one record synchronously during dispatch;
/// the table and records must remain loaded through scene teardown.
static const ActorTransform* _gActor560800KylePlacementsByCut[34] = {
    NULL,
    _gActor560800KyleCutPlacements,
    &_gActor560800KyleCutPlacements[1],
    &_gActor560800KyleCutPlacements[2],
    _gActor560800KyleCutPlacements,
    &_gActor560800KyleCutPlacements[2],
    &_gActor560800KyleCutPlacements[3],
    &_gActor560800KyleCutPlacements[4],
    &_gActor560800KyleCutPlacements[3],
    &_gActor560800KyleCutPlacements[2],
    _gActor560800KyleCutPlacements,
    &_gActor560800KyleCutPlacements[3],
    &_gActor560800KyleCutPlacements[2],
    _gActor560800KyleCutPlacements,
    &_gActor560800KyleCut14Placement,
    &_gActor560800KyleCut15Placement,
    &_gActor560800KyleCutPlacements[ACTOR_560800_KYLE_APPROACH_PLACEMENT],
    &_gActor560800KyleCut15Placement,
    &_gActor560800KyleCutPlacements[ACTOR_560800_KYLE_APPROACH_PLACEMENT],
    &_gActor560800KyleCutPlacements[4],
    &_gActor560800KyleCutPlacements[ACTOR_560800_KYLE_APPROACH_PLACEMENT],
    &_gActor560800KyleCut21Placement,
    &_gActor560800KyleCut22Placement,
    &_gActor560800KyleCutPlacements[1],
    &_gActor560800KyleCut22Placement,
    &_gActor560800KyleCut25Placement,
    &_gActor560800KyleCut26Placement,
    &_gActor560800KyleCut27Hidden,
    &_gActor560800KyleCut28Placement,
    &_gActor560800KyleCut29Hidden,
    &_gActor560800KyleCut28Placement,
    &_gActor560800KyleCut29Hidden,
    &_gActor560800KyleCut28Placement,
    &_gActor560800KyleCut33Placement,
};

/// Eve's root placement or hide marker indexed by scene cut.
///
/// Cuts 1..33 have non-NULL entries; slot 0 is NULL and must not be used.
/// Nonzero position X shows and places the actor, while zero X hides it.
/// Positions use whole game units under the view coordinate; Euler angles use
/// 4096 units per turn. Entries borrow one record synchronously during dispatch;
/// the table and records must remain loaded through scene teardown.
static const ActorTransform* _gActor560800EvePlacementsByCut[34] = {
    NULL,
    &_gActor560800EveCut1Placement,
    &_gActor560800EveCut2Placement,
    &_gActor560800EveCut3Hidden,
    &_gActor560800EveCut1Placement,
    &_gActor560800EveCut3Hidden,
    &_gActor560800EveCut6Placement,
    &_gActor560800EveCut7Hidden,
    &_gActor560800EveCut6Placement,
    &_gActor560800EveCut3Hidden,
    &_gActor560800EveCut1Placement,
    &_gActor560800EveCut6Placement,
    &_gActor560800EveCut3Hidden,
    &_gActor560800EveCut1Placement,
    &_gActor560800EveCut14Hidden,
    &_gActor560800EveCut15Hidden,
    &_gActor560800EveCut14Hidden,
    &_gActor560800EveCut15Hidden,
    &_gActor560800EveCut14Hidden,
    &_gActor560800EveCut7Hidden,
    &_gActor560800EveCut14Hidden,
    &_gActor560800EveCut21Hidden,
    &_gActor560800EveCut22Placement,
    &_gActor560800EveCut2Placement,
    &_gActor560800EveCut22Placement,
    &_gActor560800EveCut25Placement,
    &_gActor560800EveReplacementPlacement,
    &_gActor560800EveCut27Hidden,
    &_gActor560800EveCut28Placement,
    &_gActor560800EveCut29Hidden,
    &_gActor560800EveCut28Placement,
    &_gActor560800EveCut29Hidden,
    &_gActor560800EveCut28Placement,
    &_gActor560800EveCut33Placement,
};

/// Bank 0x404D, instance zero, in the resident sound-script request encoding.
enum { ACTOR_560800_SOUND_SCRIPT_BANK = 0x404D0000 };

/// Scene sound-cue ids mapped to complete resident script requests.
///
/// Script callbacks use cues 1..18, selecting entries 1..18 of bank 0x404D
/// with instance tag zero. Slot 0 is a no-op request. The bank must remain
/// loaded through queued starts and resulting playback.
static s32 _gActor560800SoundScriptsByCue[19] = {
    SOUND_SCRIPT_REQUEST_NO_OP,
    ACTOR_560800_SOUND_SCRIPT_BANK | 1,
    ACTOR_560800_SOUND_SCRIPT_BANK | 2,
    ACTOR_560800_SOUND_SCRIPT_BANK | 3,
    ACTOR_560800_SOUND_SCRIPT_BANK | 4,
    ACTOR_560800_SOUND_SCRIPT_BANK | 5,
    ACTOR_560800_SOUND_SCRIPT_BANK | 6,
    ACTOR_560800_SOUND_SCRIPT_BANK | 7,
    ACTOR_560800_SOUND_SCRIPT_BANK | 8,
    ACTOR_560800_SOUND_SCRIPT_BANK | 9,
    ACTOR_560800_SOUND_SCRIPT_BANK | 10,
    ACTOR_560800_SOUND_SCRIPT_BANK | 11,
    ACTOR_560800_SOUND_SCRIPT_BANK | 12,
    ACTOR_560800_SOUND_SCRIPT_BANK | 13,
    ACTOR_560800_SOUND_SCRIPT_BANK | 14,
    ACTOR_560800_SOUND_SCRIPT_BANK | 15,
    ACTOR_560800_SOUND_SCRIPT_BANK | 16,
    ACTOR_560800_SOUND_SCRIPT_BANK | 17,
    ACTOR_560800_SOUND_SCRIPT_BANK | 18,
};

/// Scene/audio stream selection for the first cutscene phase.
///
/// Group 6, stream 8, qualifier 11; the second stream qualifier is zero.
/// The event script and CAP controller borrow this key while selecting playback;
/// keep the package loaded through phase completion.
static EvsSceneKey _gActor560800FirstPhaseSceneKey = { .group = 6, .streamId = 8, .subId = 11 };

/// Scene/audio stream selection for the second cutscene phase.
///
/// Group 6, stream 8, qualifier 21; the second stream qualifier is zero.
/// The event script and CAP controller borrow this key while selecting playback;
/// keep the package loaded through phase completion.
static EvsSceneKey _gActor560800SecondPhaseSceneKey = { .group = 6, .streamId = 8, .subId = 21 };

/// Scene/audio stream selection for the third cutscene phase.
///
/// Group 6, stream 8, qualifier 31; the second stream qualifier is zero.
/// The event script and CAP controller borrow this key while selecting playback;
/// keep the package loaded through phase completion.
static EvsSceneKey _gActor560800ThirdPhaseSceneKey = { .group = 6, .streamId = 8, .subId = 31 };

/// Three-phase gantry scene for Aya, Eve, Kyle and No. 9, with eight jointed chains.
///
/// The 364 commands select phase streams, place cuts 1..33 and post cast/prop
/// cues. Phase 3 applies a nonfatal 50-HP shot, breaks chains and replaces Eve's
/// body. CAP cues pace the script; the controller supplies the paired skip
/// stream with HUD state retained. Commands, callbacks and scene resources
/// remain borrowed until event completion and scene-tree teardown.
static EvsCommand _gActor560800CutsceneScript[364] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_560800_PHASE_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800SendChainCommand }, { .value = ACTOR_560800_CHAIN_COMMAND_TARGET_EVE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gActor560800FirstPhaseSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800StartScenePhasePlayback }, { .value = ACTOR_560800_SCENE_PHASE_FIRST }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800SpawnFadeIn }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800SendChainCommand }, { .value = ACTOR_560800_CHAIN_COMMAND_EXTEND }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800SendChainCommand }, { .value = ACTOR_560800_CHAIN_COMMAND_THIRD_POSE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800StartSoundCue }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendNo9Animation }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendNo9Animation }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800SendChainCommand }, { .value = ACTOR_560800_CHAIN_COMMAND_FIRST_POSE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800StartSoundCue }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800StartSoundCue }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendNo9Animation }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendNo9Animation }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800StartSoundCue }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendNo9Animation }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendNo9Animation }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800StartSoundCue }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendNo9Animation }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendNo9Animation }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800StartSoundCue }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendNo9Animation }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800StartSoundCue }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendNo9Animation }, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800StartSoundCue }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendNo9Animation }, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendNo9Animation }, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendNo9Animation }, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800StartSoundCue }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendNo9Animation }, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800StartSoundCue }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800StartSoundCue }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendNo9Animation }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800StartSoundCue }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendNo9Animation }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800StartSoundCue }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800StartSoundCue }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800StartSoundCue }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800StartSoundCue }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800StartSoundCue }, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800StartSoundCue }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendNo9Animation }, { .value = 27 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostKyleCue }, { .value = 37 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostKyleCue }, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800SpawnFadeOut }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800FinishScenePhase }, { .value = ACTOR_560800_SCENE_PHASE_FIRST }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800HoldDisplayUntilScenePlaybackEnds }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800SelectSecondCapFile }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_560800_PHASE_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gActor560800SecondPhaseSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800SpawnFadeIn }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800SwapSceneTextureStrip }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800StartScenePhasePlayback }, { .value = ACTOR_560800_SCENE_PHASE_SECOND }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostPlayerCue }, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800BlendScenePlayerAnimation }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendKyleAnimation }, { .value = 27 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendKyleAnimation }, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendKyleAnimation }, { .value = 34 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendKyleAnimation }, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendKyleAnimation }, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostKyleCue }, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendKyleAnimation }, { .value = 27 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800BlendScenePlayerAnimation }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendKyleAnimation }, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendKyleAnimation }, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendNo9Animation }, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendNo9Animation }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendKyleAnimation }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800BlendScenePlayerAnimation }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800FinishScenePhase }, { .value = ACTOR_560800_SCENE_PHASE_SECOND }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800HoldDisplayUntilScenePlaybackEnds }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800SelectThirdCapFile }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_560800_PHASE_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800ApplyShotDamage }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800SpawnFadeIn }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gActor560800ThirdPhaseSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800StartScenePhasePlayback }, { .value = ACTOR_560800_SCENE_PHASE_THIRD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendKyleAnimation }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800SendChainCommand }, { .value = ACTOR_560800_CHAIN_COMMAND_DETACHED_CLIP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 24 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 24 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostKyleCue }, { .value = 36 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800SendCarrierCommand }, { .value = ACTOR_560800_CARRIER_RELIGHT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800SendCarrierCommand }, { .value = ACTOR_560800_CARRIER_FIRST_DESCENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800SendChainCommand }, { .value = ACTOR_560800_CHAIN_COMMAND_CARRIER_POSE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800FireKyleGun }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800SendChainCommand }, { .value = ACTOR_560800_CHAIN_COMMAND_BREAK_NEXT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800FireKyleGun }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendEveAnimation }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800SendChainCommand }, { .value = ACTOR_560800_CHAIN_COMMAND_BREAK_NEXT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800SendCarrierCommand }, { .value = ACTOR_560800_CARRIER_SLOW_DESCENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800FireKyleGun }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800SendChainCommand }, { .value = ACTOR_560800_CHAIN_COMMAND_BREAK_NEXT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800FireKyleGun }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800SendChainCommand }, { .value = ACTOR_560800_CHAIN_COMMAND_BREAK_NEXT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800SendCarrierCommand }, { .value = ACTOR_560800_CARRIER_RISE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800SpawnFadeOut }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostSceneCue }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800SendCarrierCommand }, { .value = ACTOR_560800_CARRIER_HIDE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU32 = _actor560800HideCastMember }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800SpawnFadeIn }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendNo9Animation }, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800SendCarrierCommand }, { .value = ACTOR_560800_CARRIER_SECOND_DESCENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 27 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800SendCarrierCommand }, { .value = ACTOR_560800_CARRIER_HIDE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 27 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendNo9Animation }, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendNo9Animation }, { .value = 37 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800SendCarrierCommand }, { .value = ACTOR_560800_CARRIER_CARRY_NO9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendNo9Animation }, { .value = 39 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800SpawnFadeOut }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU32 = _actor560800HideCastMember }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800SpawnFadeIn }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostKyleCue }, { .value = 39 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 31 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 31 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendEveAnimation }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendEveAnimation }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800PlaceCastForCut }, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800RelightCast }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor560800PostCastCue }, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = _actor560800BlendKyleAnimation }, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800SpawnFadeOut }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor560800FinishScenePhase }, { .value = ACTOR_560800_SCENE_PHASE_THIRD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Gantry-scene skip path preserving the nonfatal shot and restoring presentation.
///
/// Fades across cue/effect cancellation and texture-strip restoration, stops
/// area music, cleans up the scene and ends scripted player control. The ten
/// commands and callback code are borrowed until interpreter completion; the
/// controller then restores saved random state and tears down the cast tree.
static EvsCommand _gActor560800CutsceneSkipScript[10] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800SkipScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor560800SwapSceneTextureStrip }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc gActor560800CutsceneTaskDescriptors[14] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _actor560800CutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _actor560800DiscardTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _actor560800FadeInTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _actor560800FadeOutTask, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_CONTROL } }, _actor560800EveBodyTask, { .model = &gEveMaskedBody } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_CONTROL } }, _actor560800KyleBodyTask, { .model = &gKyleBody } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_CONTROL } }, _actor560800No9BodyTask, { .model = &_gGolemNo9BodyDryfield } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_CONTROL } }, _actor560800CastAttachmentTask, { .model = &gKyleHandLeft2 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_CONTROL } }, _actor560800CastAttachmentTask, { .model = &gKyleHandRight } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_CONTROL } }, _actor560800CastAttachmentTask, { .model = &gKyleP229 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_CONTROL } }, _actor560800CastAttachmentTask, { .model = &_gGolemNo9Gunblade } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_CONTROL } }, _actor560800EveBodyTask, { .model = &_gEveBody } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _actor560800ReplaceEveBodyTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _actor560800AwaitScenePlaybackTask, { .value = 0 } },
};

static TmdBone _gActor560800Model40064Skeleton[7] = {
#include "assets/actor_560800_model_40064_skeleton.inc"
};

static u32 _gActor560800Model40064PartVerts[7] = {
#include "assets/actor_560800_model_40064_partVerts.inc"
};

static SVECTOR _gActor560800Model40064Verts[61] = {
#include "assets/actor_560800_model_40064_verts.inc"
};

static SVECTOR _gActor560800Model40064Normals[61] = {
#include "assets/actor_560800_model_40064_normals.inc"
};

static u32 _gActor560800Model40064Stream[578] = {
#include "assets/actor_560800_model_40064_stream.inc"
};

static TmdSource _gActor560800Model40064 = {
    0,
    3072,
    1040,
    7,
    _gActor560800Model40064PartVerts,
    _gActor560800Model40064Verts,
    _gActor560800Model40064Normals,
    _gActor560800Model40064Skeleton,
    _gActor560800Model40064Stream,
};

static TmdBone _gActor560800Model40E78Skeleton[7] = {
#include "assets/actor_560800_model_40E78_skeleton.inc"
};

static u32 _gActor560800Model40E78PartVerts[7] = {
#include "assets/actor_560800_model_40E78_partVerts.inc"
};

static SVECTOR _gActor560800Model40E78Verts[61] = {
#include "assets/actor_560800_model_40E78_verts.inc"
};

static SVECTOR _gActor560800Model40E78Normals[61] = {
#include "assets/actor_560800_model_40E78_normals.inc"
};

static u32 _gActor560800Model40E78Stream[578] = {
#include "assets/actor_560800_model_40E78_stream.inc"
};

static TmdSource _gActor560800Model40E78 = {
    0,
    3072,
    1040,
    7,
    _gActor560800Model40E78PartVerts,
    _gActor560800Model40E78Verts,
    _gActor560800Model40E78Normals,
    _gActor560800Model40E78Skeleton,
    _gActor560800Model40E78Stream,
};

static TmdBone _gActor560800Model41AC4Skeleton[2] = {
#include "assets/actor_560800_model_41AC4_skeleton.inc"
};

static u32 _gActor560800Model41AC4PartVerts[2] = {
#include "assets/actor_560800_model_41AC4_partVerts.inc"
};

static SVECTOR _gActor560800Model41AC4Verts[41] = {
#include "assets/actor_560800_model_41AC4_verts.inc"
};

static SVECTOR _gActor560800Model41AC4Normals[49] = {
#include "assets/actor_560800_model_41AC4_normals.inc"
};

static u32 _gActor560800Model41AC4Stream[282] = {
#include "assets/actor_560800_model_41AC4_stream.inc"
};

static TmdSource _gActor560800Model41AC4 = {
    0,
    1984,
    0,
    2,
    _gActor560800Model41AC4PartVerts,
    _gActor560800Model41AC4Verts,
    _gActor560800Model41AC4Normals,
    _gActor560800Model41AC4Skeleton,
    _gActor560800Model41AC4Stream,
};

static AnimationPackedPose _gActor560800Animation421C4Bank1[30] = {
#include "assets/actor_560800_animation_421C4_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation421C4Bank4[8] = {
#include "assets/actor_560800_animation_421C4_bank4.inc"
};

static AnimationRecord _gActor560800Animation421C4Records[56] = {
#include "assets/actor_560800_animation_421C4_records.inc"
};

static u16 _gActor560800Animation421C4Indices[8] = {
#include "assets/actor_560800_animation_421C4_indices.inc"
};

static AnimationSet _gActor560800Animation421C4 = {
    _gActor560800Animation421C4Records,
    _gActor560800Animation421C4Indices,
    { NULL, _gActor560800Animation421C4Bank1, NULL, NULL, _gActor560800Animation421C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation42474Bank1[30] = {
#include "assets/actor_560800_animation_42474_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation42474Bank4[10] = {
#include "assets/actor_560800_animation_42474_bank4.inc"
};

static AnimationRecord _gActor560800Animation42474Records[58] = {
#include "assets/actor_560800_animation_42474_records.inc"
};

static u16 _gActor560800Animation42474Indices[8] = {
#include "assets/actor_560800_animation_42474_indices.inc"
};

static AnimationSet _gActor560800Animation42474 = {
    _gActor560800Animation42474Records,
    _gActor560800Animation42474Indices,
    { NULL, _gActor560800Animation42474Bank1, NULL, NULL, _gActor560800Animation42474Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation42720Bank1[31] = {
#include "assets/actor_560800_animation_42720_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation42720Bank4[7] = {
#include "assets/actor_560800_animation_42720_bank4.inc"
};

static AnimationRecord _gActor560800Animation42720Records[57] = {
#include "assets/actor_560800_animation_42720_records.inc"
};

static u16 _gActor560800Animation42720Indices[8] = {
#include "assets/actor_560800_animation_42720_indices.inc"
};

static AnimationSet _gActor560800Animation42720 = {
    _gActor560800Animation42720Records,
    _gActor560800Animation42720Indices,
    { NULL, _gActor560800Animation42720Bank1, NULL, NULL, _gActor560800Animation42720Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation429CCBank1[30] = {
#include "assets/actor_560800_animation_429CC_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation429CCBank4[9] = {
#include "assets/actor_560800_animation_429CC_bank4.inc"
};

static AnimationRecord _gActor560800Animation429CCRecords[58] = {
#include "assets/actor_560800_animation_429CC_records.inc"
};

static u16 _gActor560800Animation429CCIndices[8] = {
#include "assets/actor_560800_animation_429CC_indices.inc"
};

static AnimationSet _gActor560800Animation429CC = {
    _gActor560800Animation429CCRecords,
    _gActor560800Animation429CCIndices,
    { NULL, _gActor560800Animation429CCBank1, NULL, NULL, _gActor560800Animation429CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation42C7CBank1[30] = {
#include "assets/actor_560800_animation_42C7C_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation42C7CBank4[10] = {
#include "assets/actor_560800_animation_42C7C_bank4.inc"
};

static AnimationRecord _gActor560800Animation42C7CRecords[58] = {
#include "assets/actor_560800_animation_42C7C_records.inc"
};

static u16 _gActor560800Animation42C7CIndices[8] = {
#include "assets/actor_560800_animation_42C7C_indices.inc"
};

static AnimationSet _gActor560800Animation42C7C = {
    _gActor560800Animation42C7CRecords,
    _gActor560800Animation42C7CIndices,
    { NULL, _gActor560800Animation42C7CBank1, NULL, NULL, _gActor560800Animation42C7CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation42F44Bank1[30] = {
#include "assets/actor_560800_animation_42F44_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation42F44Bank4[12] = {
#include "assets/actor_560800_animation_42F44_bank4.inc"
};

static AnimationRecord _gActor560800Animation42F44Records[62] = {
#include "assets/actor_560800_animation_42F44_records.inc"
};

static u16 _gActor560800Animation42F44Indices[8] = {
#include "assets/actor_560800_animation_42F44_indices.inc"
};

static AnimationSet _gActor560800Animation42F44 = {
    _gActor560800Animation42F44Records,
    _gActor560800Animation42F44Indices,
    { NULL, _gActor560800Animation42F44Bank1, NULL, NULL, _gActor560800Animation42F44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation43200Bank1[30] = {
#include "assets/actor_560800_animation_43200_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation43200Bank4[11] = {
#include "assets/actor_560800_animation_43200_bank4.inc"
};

static AnimationRecord _gActor560800Animation43200Records[60] = {
#include "assets/actor_560800_animation_43200_records.inc"
};

static u16 _gActor560800Animation43200Indices[8] = {
#include "assets/actor_560800_animation_43200_indices.inc"
};

static AnimationSet _gActor560800Animation43200 = {
    _gActor560800Animation43200Records,
    _gActor560800Animation43200Indices,
    { NULL, _gActor560800Animation43200Bank1, NULL, NULL, _gActor560800Animation43200Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor560800Animation434A0Bank1[30] = {
#include "assets/actor_560800_animation_434A0_bank1.inc"
};

static AnimationPackedRotation _gActor560800Animation434A0Bank4[8] = {
#include "assets/actor_560800_animation_434A0_bank4.inc"
};

static AnimationRecord _gActor560800Animation434A0Records[56] = {
#include "assets/actor_560800_animation_434A0_records.inc"
};

static u16 _gActor560800Animation434A0Indices[8] = {
#include "assets/actor_560800_animation_434A0_indices.inc"
};

static AnimationSet _gActor560800Animation434A0 = {
    _gActor560800Animation434A0Records,
    _gActor560800Animation434A0Indices,
    { NULL, _gActor560800Animation434A0Bank1, NULL, NULL, _gActor560800Animation434A0Bank4, NULL, NULL, NULL },
};

enum { ACTOR_560800_CHAIN_PHASE_START = 1 };

/// Double-rate phase counter shared by chain sway and staggered tip effects.
///
/// Starts at 1 and advances by two on each carrier update after initialization,
/// including hidden updates. Carrier-pose placement resets it to 1. Its low bit
/// stays set and enables accelerated ascent; retain the signed 32-bit arithmetic.
static s32 _gActor560800ChainSwayPhase = ACTOR_560800_CHAIN_PHASE_START;

s32 gActor560800ChainGlowTick = ACTOR_560800_CHAIN_PHASE_START;

/// Eight chain animation clips indexed by chain number 1..8.
///
/// Entry 0 is absent. Each seven-part chain borrows the bank in its animation
/// context; clip playback seeds only slots 1..6. Keep the overlay and clip data
/// loaded while any chain context is live.
static AnimationSet* _gActor560800ChainAnimationSets[9] = {
    NULL,
    &_gActor560800Animation421C4,
    &_gActor560800Animation42474,
    &_gActor560800Animation42720,
    &_gActor560800Animation429CC,
    &_gActor560800Animation42C7C,
    &_gActor560800Animation42F44,
    &_gActor560800Animation43200,
    &_gActor560800Animation434A0,
};

/// First arrangement of the eight chain roots under the group coordinate.
///
/// Indexed by chain number minus one (0..7). Positions use whole game units;
/// Euler angles use 4096 units per turn, with a quarter-turn added to root X
/// when bends reset. The extension state also uses these Y values as its limits.
static ActorTransform _gActor560800ChainFirstPlacements[8] = {
    { { 400, -2800, 900, 0 }, { 1360, 2048, 64, 0 } },
    { { 300, -2800, 800, 0 }, { 1430, 2048, 48, 0 } },
    { { 200, -2800, 800, 0 }, { 1070, 2048, 32, 0 } },
    { { 100, -2800, 1100, 0 }, { 1190, 2048, 16, 0 } },
    { { 0, -2800, 900, 0 }, { 1170, 2048, 0, 0 } },
    { { -100, -2800, 1100, 0 }, { 1480, 2048, -16, 0 } },
    { { -200, -2800, 900, 0 }, { 1150, 2048, -32, 0 } },
    { { -300, -2800, 900, 0 }, { 1200, 2048, -48, 0 } },
};

/// Retracted arrangement of the eight chain roots while targeting Eve.
///
/// Indexed by chain number minus one (0..7), in the group coordinate's space.
/// Positions use whole game units; Euler angles use 4096 units per turn,
/// with a quarter-turn added to root X when bends reset.
static ActorTransform _gActor560800ChainSecondPlacements[8] = {
    { { 300, -3700, 900, 0 }, { 1280, 2048, 0, 0 } },
    { { 200, -3700, 800, 0 }, { 1280, 2048, 0, 0 } },
    { { 100, -3700, 800, 0 }, { 1280, 2048, 0, 0 } },
    { { 0, -3700, 1100, 0 }, { 1280, 2048, 0, 0 } },
    { { -100, -3700, 900, 0 }, { 1280, 2048, 0, 0 } },
    { { -200, -3700, 1100, 0 }, { 1280, 2048, 0, 0 } },
    { { -300, -3700, 900, 0 }, { 1280, 2048, 0, 0 } },
    { { -400, -3700, 900, 0 }, { 1280, 2048, 0, 0 } },
};

/// Third arrangement of the eight chain roots under the group coordinate.
///
/// Indexed by chain number minus one (0..7). Positions use whole game units;
/// Euler angles use 4096 units per turn, with a quarter-turn added to root X
/// when bends reset. Selecting this arrangement also sets each chain's
/// otherwise unread pulse-direction flag.
static ActorTransform _gActor560800ChainThirdPlacements[8] = {
    { { 250, -2800, 900, 0 }, { 1280, 2048, -80, 0 } },
    { { 200, -2800, 900, 0 }, { 1280, 2048, 64, 0 } },
    { { 150, -2800, 800, 0 }, { 1280, 2048, 48, 0 } },
    { { 100, -2800, 800, 0 }, { 1280, 2048, 32, 0 } },
    { { 50, -2800, 1100, 0 }, { 1280, 2048, 16, 0 } },
    { { 0, -2800, 900, 0 }, { 1280, 2048, 0, 0 } },
    { { -50, -2800, 1100, 0 }, { 1280, 2048, -16, 0 } },
    { { -100, -2800, 900, 0 }, { 1280, 2048, -32, 0 } },
};

/// Offsets for the eight chain roots detached into view space for clip playback.
///
/// Indexed by chain number minus one (0..7). Each root is reparented to the view
/// coordinate and takes the incoming placement plus the whole-unit table offset;
/// rotations in these records are ignored and the root matrix starts at identity.
static ActorTransform _gActor560800ChainDetachedPlacements[8] = {
    { { 0, -2710, 370, 0 }, { 0, 0, 0, 0 } },
    { { 0, -2710, 370, 0 }, { 0, 0, 0, 0 } },
    { { 0, -2710, 370, 0 }, { 0, 0, 0, 0 } },
    { { 0, -2710, 370, 0 }, { 0, 0, 0, 0 } },
    { { 0, -2710, 370, 0 }, { 0, 0, 0, 0 } },
    { { 0, -2710, 370, 0 }, { 0, 0, 0, 0 } },
    { { 0, -2710, 370, 0 }, { 0, 0, 0, 0 } },
    { { 0, -2710, 370, 0 }, { 0, 0, 0, 0 } },
};

/// Chain-root arrangement parented to the carrier model.
///
/// The eight records use whole game units in carrier space and Euler angles
/// with 4096 units per turn; root X receives an additional quarter-turn.
/// Only records 0..3 are applied: chains 5..8 are removed before placement,
/// leaving the last four records unused.
static ActorTransform _gActor560800ChainCarrierPlacements[8] = {
    { { 0, -1100, -200, 0 }, { 1080, 2048, 0, 0 } },
    { { 0, -1100, -200, 0 }, { 968, 2048, 0, 0 } },
    { { 0, -1100, -200, 0 }, { 1137, 2048, 0, 0 } },
    { { 0, -1100, -200, 0 }, { 911, 2048, 0, 0 } },
    { { 0, -1100, -200, 0 }, { 1024, 2048, 0, 0 } },
    { { 0, -1100, -200, 0 }, { 1024, 2048, 0, 0 } },
    { { 0, -1100, -200, 0 }, { 1024, 2048, 0, 0 } },
    { { 0, -1100, -200, 0 }, { 1024, 2048, 0, 0 } },
};

/// Chain-group messages for drawing, placement and borrowed scene commands.
///
/// Only ids 2005, 2004 and 2006 may be sent: this three-entry table has no end
/// marker. Draw takes a scalar mode, placement an `ActorTransform`, and command
/// an `ActorCommand`; all callbacks ignore the second payload. The cutscene's
/// command, cut-placement and visibility helpers use exactly those ids. The
/// callbacks return void, so message dispatch results must be ignored. The
/// group borrows this table and callback code until its scene tree is released.
static TaskMessageEntry _gActor560800ChainGroupMessages[3] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor560800SetChainGroupDraw },
    { ACTOR_MESSAGE_PLACE, _actor560800PlaceChainGroup },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor560800ApplyChainGroupCommand },
};

/// Positive local-Y movement per extension update for the eight chain children.
///
/// Indexed by zero-based child slot 0..7, in whole coordinate units per group
/// update. Each live child advances Y by its step and clamps to its first-pose Y;
/// the controller borrows these halfwords through scene-tree teardown.
static u16 _gActor560800ChainExtensionStepsY[8] = {
    1,
    2,
    3,
    1,
    2,
    3,
    1,
    2,
};

/// Carrier placement at the start of its first descent (command 2).
///
/// Whole game units under the view coordinate, with zero Euler rotation.
/// The placement callback borrows this complete 24-byte record synchronously.
static ActorTransform _gActor560800CarrierFirstDescentPlacement = { { 8340, -2200, 4000, 0 }, { 0, 0, 0, 0 } };

/// Carrier placement at the start of its second descent (command 5).
///
/// Whole game units under the view coordinate, with zero Euler rotation.
/// The placement callback borrows this complete 24-byte record synchronously.
static ActorTransform _gActor560800CarrierSecondDescentPlacement = { { 6950, -2200, 3650, 0 }, { 0, 0, 0, 0 } };

/// Carrier placement at the start of carrying No. 9 away (command 6).
///
/// Whole game units under the view coordinate, with zero Euler rotation.
/// The placement callback borrows this complete 24-byte record synchronously.
static ActorTransform _gActor560800CarrierCarryNo9Placement = { { 6950, -2500, 3500, 0 }, { 0, 0, 0, 0 } };

/// Messages accepted by the cutscene's carrier model.
///
/// No end marker: send only `ACTOR_MESSAGE_SET_MODEL_DRAW` (integer mode 0 no
/// change, 1 show, 2 hide/skip automatic buffering), `ACTOR_MESSAGE_PLACE`
/// (borrowed `ActorTransform` in the root parent's frame, Y/X/Z Euler order), or
/// `ACTOR_COMMAND_MESSAGE_APPLY` (borrowed `ActorCommand`; only its command
/// halfword is read: 0 relight, 1 hide, 2 first descent, 3 slow descent,
/// 4 rise, 5 second descent, 6 carry No. 9). Payloads are consumed synchronously;
/// all callbacks return void, so senders must ignore the dispatch result.
static TaskMessageEntry _gActor560800CarrierMessages[3] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor560800SetCarrierModelDraw },
    { ACTOR_MESSAGE_PLACE, _actorMsgPlaceYawPitchRollCarrier },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor560800ApplyCarrierCommand },
};

/// Task recipes for the cutscene's chain group, chains and carrier.
///
/// Four entries without a terminator, at priority `TASK_PRIORITY_CONTROL`: 0 coordinate-body group,
/// 1 intact chain, 2 carrier, 3 falling chain copy. The group and carrier borrow
/// the cutscene Task* in spawnArg2. Chains borrow their coordinate-body group
/// in spawnArg2 and use spawnArg1 = 1..8 as their number. A falling copy receives
/// the carrier's whole-unit Y in spawnArg1; its model matrices are copied from
/// the intact chain immediately after spawning. All props join the cutscene
/// teardown tree. Intact chains and the carrier skip automatic model buffering;
/// falling copies use it.
static TaskDesc _gActor560800PropTaskDescriptors[4] = {
    { { { TASK_BODY_COORD, TASK_PRIORITY_CONTROL } }, _actor560800ChainGroupTask, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_CONTROL } }, _actor560800IntactChainTask, { .model = &_gActor560800Model40064 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), TASK_PRIORITY_CONTROL } }, _actor560800CarrierTask, { .model = &_gActor560800Model41AC4 } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_CONTROL } }, _actor560800FallingChainTask, { .model = &_gActor560800Model40E78 } },
};

/// Borrowed singleton task controlling the scene and owning its cast/props.
///
/// Published after work allocation succeeds. Script callbacks borrow its work
/// and child tasks join its teardown tree. Initially NULL; teardown does not
/// clear it, so it may be used only while the controller and its work are live.
static Task* _gActor560800CutsceneTask = NULL;

/// Recorded display ticks elapsed in the first streamed scene phase.
///
/// Written at phase completion from its saved start timestamp; no code reads
/// this retained timing result. Signed subtraction preserves a result one tick
/// shorter when unsigned comparison detects the display counter wrapping.
static s32 _gActor560800FirstPhaseElapsedTicks;

/// Recorded display ticks elapsed in the second streamed scene phase.
///
/// Written at phase completion from its saved start timestamp; no code reads
/// this retained timing result. Signed subtraction preserves a result one tick
/// shorter when unsigned comparison detects the display counter wrapping.
static s32 _gActor560800SecondPhaseElapsedTicks;

/// Display ticks elapsed in the third streamed scene phase.
///
/// Recorded when Kyle enters cut 33 and again at phase completion. Signed
/// subtraction uses the saved third-phase start; if the unsigned counter has
/// wrapped past that start, the retained result is one tick shorter.
static s32 _gActor560800ThirdPhaseElapsedTicks;

/// Display-counter timestamp recorded before first-phase playback is queued.
///
/// A signed word copied from `gDisplayState.frameCount`, used at phase completion
/// to record elapsed display ticks. It must be recorded before that subtraction;
/// wrap detection compares its bit pattern as unsigned.
static s32 _gActor560800FirstPhaseStartTick;

/// Display-counter timestamp recorded before second-phase playback is queued.
///
/// A signed word copied from `gDisplayState.frameCount`, used at phase completion
/// to record elapsed display ticks. It must be recorded before that subtraction;
/// wrap detection compares its bit pattern as unsigned.
static s32 _gActor560800SecondPhaseStartTick;

/// Display-counter timestamp at the start of the third streamed scene phase.
///
/// Recorded immediately before playback is queued. It must be written before
/// third-phase elapsed-time recording. Stored as a signed word but compared
/// as unsigned for wrap detection; it is not a playback-frame index.
static s32 _gActor560800ThirdPhaseStartTick;

/// Random-generator snapshot restored when the cutscene event script ends.
///
/// Successful chain-group initialization saves `gRandomLcgState` after the eight
/// chain spawn attempts, then selects zero for the scripted sequence. The snapshot
/// remains valid until event completion, including completion of the skip script,
/// when gameplay resumes from that saved state.
static u32 _gActor560800SavedRandomState;

/// Borrowed singleton carrier task whose root parents the detached chains.
///
/// Published after model/work initialization succeeds and used for carrier Y
/// and the remaining chains' coordinate parent. Its lifetime belongs to the
/// cutscene teardown tree. Teardown does not clear it; users require a live
/// initialized carrier and must stop before scene teardown.
static Task* _gActor560800CarrierTask;

/// Blend duration used by the scene's player requests and cast-chain changes.
enum { ACTOR_560800_ANIMATION_BLEND_FRAMES = 10 };

/// Pending cue sentinel and Kyle's handgun effect recipe (P229 flare/casing).
enum {
    ACTOR_560800_CUE_NONE        = 0,
    ACTOR_560800_KYLE_GUN_EFFECT = 33,
};

/// The scripted shot removes this many HP without killing the player.
enum { ACTOR_560800_SHOT_DAMAGE_HP = 50 };

/// CAP dialogue uses the scene texture's VRAM origin in 16-bit pixels.
enum {
    ACTOR_560800_CAP_TEXTURE_VRAM_X = 384,
    ACTOR_560800_CAP_TEXTURE_VRAM_Y = 0,
};

/// Cast groups accepted by `_actor560800ShowCastMember`.
enum {
    ACTOR_560800_CAST_PLAYER  = 0,
    ACTOR_560800_CAST_EVE     = 1,
    ACTOR_560800_CAST_KYLE    = 2,
    ACTOR_560800_CAST_NO9     = 3,
    ACTOR_560800_CAST_CHAINS  = 4,
    ACTOR_560800_CAST_CARRIER = 5,
};

/// Carrier scale limits in 12-fractional-bit units and per-update scale direction.
enum {
    ACTOR_560800_CARRIER_HALF_SCALE   = ONE / 2,
    ACTOR_560800_CARRIER_MAX_SCALE    = ONE * 3 / 2,
    ACTOR_560800_CARRIER_SCALE_SHRINK = 0,
    ACTOR_560800_CARRIER_SCALE_GROW   = 1,
    ACTOR_560800_CARRIER_ACCELERATE   = 1,
};

static s32         _actor560800AdvancePlayerAnimChain(Task* task);
static inline void _actor560800BlendCastAnimation(Task* task, u16 animationId, s16 animationRate);
static void        _actor560800ShowCastMember(u32 memberId);
static inline void _actor560800RestartPlayerAnimation(Task* task, u16 animationId);
static inline void _actor560800BlendPlayerAnimation(Task* task, u16 animationId, s32 blendFrames);
static inline void _actor560800RestartPlayerWeaponAnimation(s16 animationId);
static inline void _actor560800BlendPlayerWeaponAnimation(s32 animationId);
static inline void _actor560800SpawnOuterPlayerDecals(Task* task);
static inline void _actor560800SpawnInnerPlayerDecals(Task* task);
static inline void _actor560800RestartCastAnimation(_Actor560800CastWork* work, u16 animationId);
static inline void _actor560800RestartCastAnimationAtRate(Task* task, u16 animationId, u16 animationRate);
static void        _actor560800InitializeCutscene(Task* task);
static void        _actor560800BendChainTowardTarget(Task* task);
static void        _actor560800InitChainModel(Task* task);
static void        _actor560800RaiseCarrier(Task* task);
static void        _actor560800CarryNo9Away(Task* task);

/// Plays the scene movie, permits START to skip it, then restores game presentation.
///
/// A zero spawn argument selects movie 100; nonzero selects movie 101. The
/// selected movie must be loaded for the current room before state 1. Owns the
/// movie workspace and saved VRAM until restoration finishes and kills the task.
/// Restoration reloads sprite images before resuming the game loop.
static void _actor560800MovieTask(Task* task)
{
    enum {
        ACTOR_560800_MOVIE_PREPARE      = 0,
        ACTOR_560800_MOVIE_QUEUE        = 1,
        ACTOR_560800_MOVIE_WAIT_READY   = 2,
        ACTOR_560800_MOVIE_PLAY         = 3,
        ACTOR_560800_MOVIE_WAIT_STOP    = 4,
        ACTOR_560800_MOVIE_RESTORE      = 5,
        ACTOR_560800_MOVIE_DEFAULT_ID   = 0x64,
        ACTOR_560800_MOVIE_ALTERNATE_ID = 0x65,
    };
    u8              streamArgs[sizeof(gCdCmdQueue.entries[0].args)];
    GameLocationKey movieLocation;
    CdCmdQueue*     cdQueue = &gCdCmdQueue;

    switch (task->state) {
        case ACTOR_560800_MOVIE_PREPARE:
            SetDispMask(0);
            streamPrepareMovieWorkspace(true);
            task->state++;
            break;
        case ACTOR_560800_MOVIE_QUEUE:
            movieLocation = gGameSession->location;
            if (task->spawnArg1.value != 0) {
                movieLocation.view = ACTOR_560800_MOVIE_ALTERNATE_ID;
            } else {
                movieLocation.view = ACTOR_560800_MOVIE_DEFAULT_ID;
            }
            // Only the slot byte is interpreted; enqueue copies the full argument block.
            streamArgs[0] = streamFindMovieSlot(&movieLocation, 0, 0);
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, streamArgs);
            task->state++;
            break;
        case ACTOR_560800_MOVIE_WAIT_READY:
            if (cdQueue->movieReady != 0) {
                SetDispMask(1);
                task->state++;
            }
            break;
        case ACTOR_560800_MOVIE_PLAY:
            if (cdCmdIsIdle()) {
                SetDispMask(0);
                task->state++;
            } else if (padIsStartPressed()) {
                SetDispMask(0);
                cdCmdRequestCancel();
                task->state++;
            }
            break;
        // Wait for playback or cancellation to release the CD channel before restoring.
        case ACTOR_560800_MOVIE_WAIT_STOP:
            if (cdCmdIsIdle()) {
                streamResetGameRestore();
                task->state++;
            }
            break;
        case ACTOR_560800_MOVIE_RESTORE:
            if (streamPollGameRestore(false, true)) {
                taskKill(task);
                displayResumeGameLoop();
            }
            break;
    }
}

/// Advances the cutscene player's clip chain by one update.
///
/// Requires cutscene work and a valid scene clip index (0..12). A nonzero hold
/// counts updates in a signed-halfword counter; zero waits for playback to end.
/// Successors blend over ten normal-rate frames with world collision enabled.
/// Returns 1 for an absent player or a reached terminal link, otherwise 0.
/// The borrowed player task and scene animation bank must remain live.
/// A hold of 65535 cannot be reached by the signed counter and keeps its clip.
static s32 _actor560800AdvancePlayerAnimChain(Task* task)
{
    _Actor560800CutsceneWork* work;
    const ActorAnimChainLink* chain;
    const ActorAnimChainLink* link;
    const ActorAnimChainLink* finishedLink;
    AnimationPlayRequest      request;
    u16                       nextAnimationId;
    u16                       finishedAnimationId;

/// Starts a successor player scene clip with blending and clears its hold.
///
/// Requires live scene work and a non-NULL player, loaded player animation sets,
/// a valid u16 clipId and a writable AnimationPlayRequest request lvalue.
/// Arguments repeat and must be stable and side-effect-free. Caches the clip,
/// requests interpolation over ACTOR_560800_ANIMATION_BLEND_FRAMES normal-rate
/// frames with collision enabled, then clears playerAnimHold after synchronous
/// dispatch. The player borrows the set table throughout playback. Invoke within
/// braces; the binding is undefined after this consumer.
#define ACTOR_560800_START_PLAYER_CHAIN_CLIP(work, clipId, request)                                       \
    {                                                                                                     \
        (request).source.sets          = _gActor560800PlayerAnimationSets;                                \
        (work)->playerAnimId           = (clipId);                                                        \
        (request).animationId          = (clipId);                                                        \
        (request).blend                = ANIMATION_BLEND_INTERPOLATE;                                     \
        (request).blendFrames          = ACTOR_560800_ANIMATION_BLEND_FRAMES;                             \
        (request).enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;                                \
        TASK_MESSAGE_DISPATCH_POINTER((work)->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &(request), 0); \
        (work)->playerAnimHold = 0;                                                                       \
    }

    work = task->work;
    if (work->player == NULL) {
        return 1;
    }
    chain = _gActor560800PlayerAnimationChain;
    link  = &chain[work->playerAnimId];
    // Timed links can finish before playback; untimed links wait for the player.
    if (link->holdFrames != 0) {
        if (work->playerAnimHold >= link->holdFrames) {
            if (link->nextAnimId < 0) {
                return 1;
            }
            nextAnimationId = link->nextAnimId;
            ACTOR_560800_START_PLAYER_CHAIN_CLIP(work, nextAnimationId, request);
        } else {
            work->playerAnimHold += 1;
        }
    } else {
        if (taskMessageDispatch(work->player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) != 0) {
            return 0;
        }
        finishedLink = &_gActor560800PlayerAnimationChain[work->playerAnimId];
        if (finishedLink->nextAnimId < 0) {
            return 1;
        }
        // Dispatch may change the player binding; reacquire it before starting a clip.
        work = task->work;
        if (work->player != NULL) {
            finishedAnimationId = finishedLink->nextAnimId;
            ACTOR_560800_START_PLAYER_CHAIN_CLIP(work, finishedAnimationId, request);
        }
    }
#undef ACTOR_560800_START_PLAYER_CHAIN_CLIP
    return 0;
}

/// Blends the cast body's non-root animation slots into a selected clip.
///
/// Requires initialized _Actor560800CastWork, live playback storage and loaded
/// animation data. Slots 1..slotCount-1 are used: 19 for Kyle, 18 for Eve/No. 9.
/// work, animationId and blendFrames must be stable and side-effect-free; they are evaluated
/// per slot. blendFrames is 0..2047 whole normal-rate frames. Each slot seeks to
/// frame zero, retaining its rate. Does not change animId, animRate or animHold;
/// callers maintain those separately. Captures no caller locals.
#define ACTOR_560800_BLEND_CAST_SLOTS(work, animationId, blendFrames)                                  \
    do {                                                                                               \
        u16 slotIndex;                                                                                 \
        for (slotIndex = 1; slotIndex < (work)->slotCount; slotIndex++) {                              \
            animationSeekSlotWithBlend(&(work)->rig.anim, slotIndex, (animationId), 0, (blendFrames)); \
        }                                                                                              \
    } while (0)

/// Clears a cast clip's timed-chain hold count.
///
/// Requires live, non-NULL cast work. The identical branch arms retain a
/// compiled block boundary before subsequent slot playback; the original
/// condition is unproven.
static inline void _actor560800ResetAnimHold(_Actor560800CastWork* work)
{
    if (work != NULL) {
        work->animHold = 0;
    } else {
        work->animHold = 0;
    }
}

/// Blends a cast body's non-root tracks into a new clip over ten frames.
///
/// Requires an initialized `_Actor560800CastWork` with live model and clip data;
/// slots 1 through `slotCount - 1` must fit the rig and selected clip's tracks.
/// Records `animationRate` in sixteenths of a frame for subsequent chain links,
/// but retains each slot's current rate. Restarts the clip's hold counter.
static inline void _actor560800BlendCastAnimation(Task* task, u16 animationId, s16 animationRate)
{
    _Actor560800CastWork* work;
    u16                   slotIndex;

    work           = task->work;
    work->animId   = animationId;
    work->animRate = animationRate;
    _actor560800ResetAnimHold(work);
    for (slotIndex = 1; slotIndex < work->slotCount; slotIndex++) {
        animationSeekSlotWithBlend(&work->rig.anim, slotIndex, animationId, 0, ACTOR_560800_ANIMATION_BLEND_FRAMES);
    }
}

/// Advances a visible cast body's non-root animations and follows its clip chain.
///
/// Requires initialized cast work, live playback storage and one chain link for
/// every selected clip. A timed link waits its whole-frame hold count; an
/// untimed link waits for all active slots to settle. Successors blend over ten
/// frames. Returns 1 at a terminal link (negative successor), otherwise 0.
/// Hidden bodies leave both playback and the hold counter unchanged.
static s32 _actor560800TickCastAnimationChain(Task* task)
{
    _Actor560800CastWork* work;
    u16                   slotIndex;
    u16                   allSettled;

    work = task->work;
    if (task->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) {
        return 0;
    }
    for (slotIndex = 1; slotIndex < work->slotCount; slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
    slotIndex  = 1;
    allSettled = 1;
    for (; slotIndex < work->slotCount; slotIndex++) {
        if (!(work->rig.slots[slotIndex].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
            allSettled = 0;
            break;
        }
    }
    // Timed links may change clip before all slots settle.
    if (work->animChain[work->animId].holdFrames != 0) {
        if (work->animHold >= work->animChain[work->animId].holdFrames) {
            if (work->animChain[work->animId].nextAnimId >= 0) {
                _actor560800BlendCastAnimation(task, work->animChain[work->animId].nextAnimId, work->animRate);
            } else {
                return 1;
            }
        } else {
            work->animHold++;
        }
    } else if (allSettled) {
        if (work->animChain[work->animId].nextAnimId >= 0) {
            _actor560800BlendCastAnimation(task, work->animChain[work->animId].nextAnimId, work->animRate);
        } else {
            return 1;
        }
    }
    return 0;
}

/// Allocates a scene body work block and binds its lighting and placement texture.
///
/// Requires the published scene and the selected area-placement record.
/// Returns 1 on allocation failure; otherwise the task owns the zeroed work
/// until scene-tree teardown and its model borrows that work's light matrices.
static inline u16 _actor560800AllocateBodyWork(Task* task, u8 textureEntryId)
{
    TmdObject*            model         = task->extra.tmd;
    GfxCoord*             root          = model->coords;
    _Actor560800CastWork* allocatedWork = memMalloc(sizeof(*allocatedWork), false);
    AreaPlacement*        placement;
    u8                    entryId;

    task->work = allocatedWork;
    if (allocatedWork == NULL) {
        return 1;
    }
    root->parent = &gGfxViewCoord;
    memFillBytes(task->work, 0, sizeof(*allocatedWork));
    model->lightMtx = &allocatedWork->light;
    model->colorMtx = &allocatedWork->color;
    task->msgTable  = _gActor560800CastMessages;
    placement       = areaGetVariant(&gGameSession->location)->placements;
    entryId         = placement->entryId;
    while (entryId != AREA_PLACEMENT_END) {
        if (entryId == textureEntryId) {
            break;
        }
        placement++;
        entryId = placement->entryId;
    }
    tmdSetTextureOffsets(task->extra.tmd, placement->texturePageOffset, placement->clutRowOffset);
    taskReparent(_gActor560800CutsceneTask, task);
    return 0;
}

/// Seeds a scene body's non-root tracks and animation-chain state at normal rate.
///
/// Requires initialized cast work and a loaded animationId covering slots
/// 1..slotCount - 1 (19 slots for Eve/No. 9, 20 for Kyle). Normal rate is 16
/// sixteenths of a frame per tick, narrowed into the work and each slot.
/// Leaves root slot 0 untouched; owns no storage beyond the body's work.
static inline void _actor560800InitializeBodyAnimation(Task* task, u16 animationId)
{
    _Actor560800CastWork* work = task->work;
    u16                   slotIndex;
    s32                   animationRate = ANIMATION_RATE_ONE;

    work->animId   = animationId;
    work->animRate = animationRate;
    work->animHold = 0;
    for (slotIndex = 1; slotIndex < work->slotCount; slotIndex++) {
        work->rig.slots[slotIndex].rate = animationRate;
        animationResetSlot(&work->rig.anim, slotIndex, animationId);
    }
}

/// Initializes and updates Eve's scene body, including her replacement model.
///
/// Owns zeroed cast work and lends its lighting matrices to the model until
/// cutscene teardown. Requires Eve's area placement texture record and a loaded
/// nineteen-slot rig; starts clip 0 for spawnArg1=0, clip 2 otherwise. A spawn
/// argument of 1 places and relights the replacement on its next update.
/// Ticks non-root tracks, draws the ground shadow and optionally refreshes
/// lighting from the root's cached world translation. Allocation failure kills
/// the task. The model's root is in view space; task lifetime belongs to the scene.
static void _actor560800EveBodyTask(Task* task)
{
    _Actor560800CastWork* work = task->work;
    SVECTOR               groundShadowOffset;
    VECTOR                lightingPosition;

    switch (task->state) {
        case ACTOR_560800_CAST_INITIALIZE: {
            if (_actor560800AllocateBodyWork(task, ACTOR_560800_CHAIN_TARGET_EVE)) {
                taskKill(task);
                return;
            }
            task->extra.tmd->flags &= ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            work                    = task->work;
            {
                TmdObject* lightingModel = task->extra.tmd;
                animationInitContext(&work->rig.anim, _gActor560800EveAnimationSets, lightingModel, work->rig.poses, work->rig.slots);
            }
            work->slotCount = ACTOR_560800_EVE_NO9_RIG_SLOTS;
            work->animChain = _gActor560800EveAnimationChain;
            if (task->spawnArg1.value == 0) {
                _actor560800InitializeBodyAnimation(task, 0);
            } else {
                _actor560800InitializeBodyAnimation(task, 2);
            }
            task->state += 1;
            break;
        }
        case ACTOR_560800_EVE_ACTIVE:
            break;
        case ACTOR_560800_EVE_PLACE_REPLACEMENT: {
            s32 replacementMode = task->spawnArg1.value;
            if (replacementMode == 1) {
                TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gActor560800EveReplacementPlacement, 0);
                work->relit  = replacementMode;
                task->state += 1;
            }
        } break;
    }
    // Draw the shadow before advancing the pose for this update.
    groundShadowOffset.vx = 0;
    groundShadowOffset.vy = ACTOR_560800_GROUND_SHADOW_LOCAL_Y;
    groundShadowOffset.vz = 0;
    actorRenderDrawGroundShadow(&task->extra.tmd->coords[1], ACTOR_560800_GROUND_SHADOW_SIDE, &groundShadowOffset);
    _actor560800TickCastAnimationChain(task);
    if (work->relit != 0) {
        TmdObject* lightingModel = task->extra.tmd;

        lightingPosition.vx = lightingModel->coords->workm.t[0];
        lightingPosition.vy = task->extra.tmd->coords->workm.t[1];
        lightingPosition.vz = task->extra.tmd->coords->workm.t[2];
        worldCoordSetModelLighting(lightingModel, &lightingPosition, 0, ACTOR_560800_MODEL_LIGHT_COUNT);
    }
}

/// Attaches a scene hand or weapon model to its borrowed parent body.
///
/// spawnArg2 is the live parent Task*. spawnArg1 selects 0 free hand on part 12,
/// 1 gun hand on part 8, 2 handgun on part 8, or 3 gunblade on part 8. Hands use
/// Kyle's placement texture offsets, the gun uses zero offsets, and the gunblade
/// uses No. 9's. Owns zeroed cast work and lends its lighting matrices until
/// parent teardown; follows the parent's coordinate without running animation.
/// Initialization returns before optional cached-position relighting begins.
/// Requires the selected placement record; allocation failure kills the task.
static void _actor560800CastAttachmentTask(Task* task)
{
    _Actor560800CastWork* work = task->work;
    VECTOR                lightingPosition;

    if (task->state == ACTOR_560800_CAST_INITIALIZE) {
        TmdObject*            model  = task->extra.tmd;
        Task*                 parent = task->spawnArg2.pointer;
        GfxCoord*             root   = model->coords;
        _Actor560800CastWork* allocatedWork;
        AreaPlacement*        placement;
        u8                    entryId;

        allocatedWork = memMalloc(sizeof(*allocatedWork), false);
        task->work    = allocatedWork;
        if (allocatedWork == NULL) {
            taskKill(task);
            return;
        }
        work = allocatedWork;
        switch (task->spawnArg1.value) {
            case ACTOR_560800_ATTACHMENT_FREE_HAND:
                root->parent = &parent->extra.tmd->coords[12];
                break;
            case ACTOR_560800_ATTACHMENT_GUN_HAND:
            case ACTOR_560800_ATTACHMENT_GUN:
            case ACTOR_560800_ATTACHMENT_GUNBLADE:
                root->parent = &parent->extra.tmd->coords[8];
                break;
        }
        memFillBytes(task->work, 0, sizeof(*allocatedWork));
        model->lightMtx = &work->light;
        model->colorMtx = &work->color;
        if (task->spawnArg1.value < ACTOR_560800_ATTACHMENT_GUN) {
            placement = areaGetVariant(&gGameSession->location)->placements;
            entryId   = placement->entryId;
            while (entryId != AREA_PLACEMENT_END) {
                if (entryId == ACTOR_560800_KYLE_PLACEMENT_ENTRY) {
                    break;
                }
                placement++;
                entryId = placement->entryId;
            }
            tmdSetTextureOffsets(task->extra.tmd, placement->texturePageOffset, placement->clutRowOffset);
        } else if (task->spawnArg1.value == ACTOR_560800_ATTACHMENT_GUN) {
            tmdSetTextureOffsets(task->extra.tmd, 0, 0);
        } else if (task->spawnArg1.value == ACTOR_560800_ATTACHMENT_GUNBLADE) {
            placement = areaGetVariant(&gGameSession->location)->placements;
            entryId   = placement->entryId;
            while (entryId != AREA_PLACEMENT_END) {
                if (entryId == ACTOR_560800_CHAIN_TARGET_NO9) {
                    break;
                }
                placement++;
                entryId = placement->entryId;
            }
            tmdSetTextureOffsets(task->extra.tmd, placement->texturePageOffset, placement->clutRowOffset);
        }
        taskReparent(parent, task);
        task->msgTable = _gActor560800CastMessages;
        task->state   += 1;
        return;
    }
    if (work->relit != 0) {
        TmdObject* lightingModel = task->extra.tmd;

        lightingPosition.vx = lightingModel->coords->workm.t[0];
        lightingPosition.vy = task->extra.tmd->coords->workm.t[1];
        lightingPosition.vz = task->extra.tmd->coords->workm.t[2];
        worldCoordSetModelLighting(lightingModel, &lightingPosition, 0, ACTOR_560800_MODEL_LIGHT_COUNT);
    }
}

/// Initializes and updates Kyle's animated scene body and scripted joint turns.
///
/// Owns zeroed cast work, with a twenty-slot rig starting in clip 0; requires
/// Kyle's area placement texture record and the published scene task. The model
/// borrows its lighting matrices until task teardown. Slots 1..19 advance unless
/// paused. Joint angles use 4096 units per turn; paused angles are composed once
/// and cleared so they do not accumulate on the held pose. Draws the ground
/// shadow and refreshes cached-position lighting when requested. Allocation
/// failure kills the task; its teardown lifetime belongs to the scene.
static void _actor560800KyleBodyTask(Task* task)
{
    _Actor560800CastWork* work = task->work;
    SVECTOR               groundShadowOffset;
    VECTOR                lightingPosition;

    if (task->state == ACTOR_560800_CAST_INITIALIZE) {
        if (_actor560800AllocateBodyWork(task, ACTOR_560800_KYLE_PLACEMENT_ENTRY)) {
            taskKill(task);
            return;
        }
        work = task->work;
        {
            TmdObject* lightingModel = task->extra.tmd;
            animationInitContext(&work->rig.anim, _gActor560800KyleAnimationSets, lightingModel, work->rig.poses, work->rig.slots);
        }
        work->slotCount = ACTOR_560800_KYLE_RIG_SLOTS;
        work->animChain = _gActor560800KyleAnimationChain;
        _actor560800InitializeBodyAnimation(task, 0);
        task->state += 1;
    }
    groundShadowOffset.vx = 0;
    groundShadowOffset.vy = ACTOR_560800_GROUND_SHADOW_LOCAL_Y;
    groundShadowOffset.vz = 0;
    actorRenderDrawGroundShadow(&task->extra.tmd->coords[1], ACTOR_560800_GROUND_SHADOW_SIDE, &groundShadowOffset);
    if (work->animPaused == 0) {
        _actor560800TickCastAnimationChain(task);
    }
    // Compose scripted turns after animation; held poses consume each delta once.
    gfxRotMatrixY(&task->extra.tmd->coords[4].coord, work->part4Yaw, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixX(&task->extra.tmd->coords[4].coord, work->part4Pitch, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixZ(&task->extra.tmd->coords[2].coord, work->part2Roll, GRAPHICS_ROTATION_COMPOSE);
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->animPaused != 0) {
        work->part4Yaw   = 0;
        work->part4Pitch = 0;
        work->part2Roll  = 0;
    }
    if (work->relit != 0) {
        TmdObject* lightingModel = task->extra.tmd;

        lightingPosition.vx = lightingModel->coords->workm.t[0];
        lightingPosition.vy = task->extra.tmd->coords->workm.t[1];
        lightingPosition.vz = task->extra.tmd->coords->workm.t[2];
        worldCoordSetModelLighting(lightingModel, &lightingPosition, 0, ACTOR_560800_MODEL_LIGHT_COUNT);
    }
}

/// Initializes and updates No. 9's animated scene body.
///
/// Owns zeroed cast work and a nineteen-slot rig starting in clip 0; requires
/// No. 9's area placement texture record and the published scene task. The model
/// borrows its lighting matrices until task teardown. Advances non-root tracks,
/// draws the ground shadow only while the model and floor quad are visible, and
/// optionally refreshes lighting from the cached world translation. Allocation
/// failure kills the task; its teardown lifetime belongs to the scene.
static void _actor560800No9BodyTask(Task* task)
{
    _Actor560800CastWork* work = task->work;
    SVECTOR               groundShadowOffset;
    VECTOR                lightingPosition;

    if (task->state == ACTOR_560800_CAST_INITIALIZE) {
        if (_actor560800AllocateBodyWork(task, ACTOR_560800_CHAIN_TARGET_NO9)) {
            taskKill(task);
            return;
        }
        work = task->work;
        {
            TmdObject* lightingModel = task->extra.tmd;
            animationInitContext(&work->rig.anim, _gActor560800No9AnimationSets, lightingModel, work->rig.poses, work->rig.slots);
        }
        work->slotCount = ACTOR_560800_EVE_NO9_RIG_SLOTS;
        work->animChain = _gActor560800No9AnimationChain;
        _actor560800InitializeBodyAnimation(task, 0);
        task->state += 1;
    }
    if (!(task->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) && work->floorQuadHidden == 0) {
        groundShadowOffset.vx = 0;
        groundShadowOffset.vy = ACTOR_560800_GROUND_SHADOW_LOCAL_Y;
        groundShadowOffset.vz = 0;
        actorRenderDrawGroundShadow(&task->extra.tmd->coords[1], ACTOR_560800_GROUND_SHADOW_SIDE, &groundShadowOffset);
    }
    _actor560800TickCastAnimationChain(task);
    if (work->relit != 0) {
        TmdObject* lightingModel = task->extra.tmd;

        lightingPosition.vx = lightingModel->coords->workm.t[0];
        lightingPosition.vy = task->extra.tmd->coords->workm.t[1];
        lightingPosition.vz = task->extra.tmd->coords->workm.t[2];
        worldCoordSetModelLighting(lightingModel, &lightingPosition, 0, ACTOR_560800_MODEL_LIGHT_COUNT);
    }
}

/// Refreshes room lighting on each live scene body, attachment and chain.
///
/// Requires the published cutscene and initialized models in non-null cast
/// slots. Samples cached world translations without composing coordinates.
/// Preserves cast order; the chain command borrows stack storage synchronously
/// and reads only its command halfword. Does not change continuous-relight flags.
static void _actor560800RelightCast(void)
{
    _Actor560800CutsceneWork* work = _gActor560800CutsceneTask->work;
    Task*                     task;
    // Lighting samples and the final borrowed command have disjoint lifetimes.
    union {
        VECTOR       lightingPosition;
        ActorCommand command;
    } scratch;

/// Relights one scene body or attachment from its cached root translation.
///
/// modelTask is a stable live Task* expression with a live model and writable
/// lighting matrices. samplePosition is a writable VECTOR lvalue; both arguments
/// occur repeatedly. Reads XYZ from workm without composition and samples all
/// three light rows. The query borrows samplePosition synchronously, while the
/// model's matrices remain borrowed from its owning work. Invoke within braces.
#define ACTOR_560800_RELIGHT_MODEL(modelTask, samplePosition)                                    \
    {                                                                                            \
        TmdObject* model    = (modelTask)->extra.tmd;                                            \
        (samplePosition).vx = model->coords->workm.t[0];                                         \
        (samplePosition).vy = (modelTask)->extra.tmd->coords->workm.t[1];                        \
        (samplePosition).vz = (modelTask)->extra.tmd->coords->workm.t[2];                        \
        worldCoordSetModelLighting(model, &(samplePosition), 0, ACTOR_560800_MODEL_LIGHT_COUNT); \
    }

    task = work->eve;
    if (task != NULL) {
        ACTOR_560800_RELIGHT_MODEL(task, scratch.lightingPosition);
    }
    task = work->kyle;
    if (task != NULL) {
        ACTOR_560800_RELIGHT_MODEL(task, scratch.lightingPosition);
    }
    task = work->kyleGunHand;
    if (task != NULL) {
        ACTOR_560800_RELIGHT_MODEL(task, scratch.lightingPosition);
    }
    task = work->kyleFreeHand;
    if (task != NULL) {
        ACTOR_560800_RELIGHT_MODEL(task, scratch.lightingPosition);
    }
    task = work->kyleGun;
    if (task != NULL) {
        ACTOR_560800_RELIGHT_MODEL(task, scratch.lightingPosition);
    }
    task = work->no9;
    if (task != NULL) {
        ACTOR_560800_RELIGHT_MODEL(task, scratch.lightingPosition);
    }
    task = work->no9Gunblade;
    if (task != NULL) {
        ACTOR_560800_RELIGHT_MODEL(task, scratch.lightingPosition);
    }
    if (work->chainGroup != NULL) {
        _Actor560800CutsceneWork* dispatchWork = _gActor560800CutsceneTask->work;

        scratch.command.command = ACTOR_560800_CHAIN_COMMAND_RELIGHT;
        TASK_MESSAGE_DISPATCH_POINTER(dispatchWork->chainGroup, ACTOR_COMMAND_MESSAGE_APPLY, &scratch.command, 0);
    }
#undef ACTOR_560800_RELIGHT_MODEL
}

/// Shows a selected scene body or prop group together with its attachments.
///
/// `memberId`: 0 player, 1 Eve, 2 Kyle and hands/gun, 3 No. 9 and gunblade,
/// 4 chain group, 5 carrier; other values do nothing. Requires a published
/// cutscene and a live selected body/group. Only the optional gun and gunblade
/// are checked for absence.
static void _actor560800ShowCastMember(u32 memberId)
{
    _Actor560800CutsceneWork* work = _gActor560800CutsceneTask->work;

    switch (memberId) {
        case ACTOR_560800_CAST_PLAYER:
            taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            break;
        case ACTOR_560800_CAST_EVE:
            taskMessageDispatch(work->eve, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            break;
        case ACTOR_560800_CAST_KYLE:
            taskMessageDispatch(work->kyle, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            taskMessageDispatch(work->kyleGunHand, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            taskMessageDispatch(work->kyleFreeHand, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            if (work->kyleGun != NULL) {
                taskMessageDispatch(work->kyleGun, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            }
            break;
        case ACTOR_560800_CAST_NO9:
            taskMessageDispatch(work->no9, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            if (work->no9Gunblade != NULL) {
                taskMessageDispatch(work->no9Gunblade, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            }
            break;
        case ACTOR_560800_CAST_CHAINS:
            taskMessageDispatch(work->chainGroup, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            break;
        case ACTOR_560800_CAST_CARRIER:
            taskMessageDispatch(work->carrierModel, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            break;
    }
}

/// Hides a scene cast group and excludes its models from automatic buffering.
///
/// Accepts ACTOR_560800_CAST_* selectors; other values do nothing. Requires a
/// published scene and a live selected group, including Kyle's two hands. The
/// gun and gunblade are optional. Hiding the player also releases its primitive
/// buffer; the package model handlers only change draw flags.
static void _actor560800HideCastMember(u32 memberId)
{
    _Actor560800CutsceneWork* work = _gActor560800CutsceneTask->work;

    switch (memberId) {
        case ACTOR_560800_CAST_PLAYER:
            taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE, 0);
            break;
        case ACTOR_560800_CAST_EVE:
            taskMessageDispatch(work->eve, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, 0);
            break;
        case ACTOR_560800_CAST_KYLE:
            taskMessageDispatch(work->kyle, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, 0);
            taskMessageDispatch(work->kyleGunHand, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, 0);
            taskMessageDispatch(work->kyleFreeHand, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, 0);
            if (work->kyleGun != NULL) {
                taskMessageDispatch(work->kyleGun, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, 0);
            }
            break;
        case ACTOR_560800_CAST_NO9:
            taskMessageDispatch(work->no9, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, 0);
            if (work->no9Gunblade != NULL) {
                taskMessageDispatch(work->no9Gunblade, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, 0);
            }
            break;
        case ACTOR_560800_CAST_CHAINS:
            taskMessageDispatch(work->chainGroup, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, 0);
            break;
        case ACTOR_560800_CAST_CARRIER:
            taskMessageDispatch(work->carrierModel, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, 0);
            break;
    }
}

/// Places or hides each live cast group for a cut, cancelling effects unless retained.
///
/// Requires the published scene and a cut id in 1..33; table entry 0 is NULL.
/// Nonzero placement X means show and place in the receiver's coordinate frame;
/// zero means hide. The chain group reuses the last placement selected for a
/// live body, so at least one body must exist when the chain group is present.
static void _actor560800PlaceCastForCut(s32 cutId)
{
    enum { ACTOR_560800_PLACEMENT_HIDDEN_X = 0 };
    _Actor560800CutsceneWork* work;
    const ActorTransform*     placement;

    work = _gActor560800CutsceneTask->work;
    if (work->keepEffects == 0) {
        roomEffectRequestCancelAll();
    }
    if (work->player != NULL) {
        placement = _gActor560800PlayerPlacementsByCut[cutId];
        if (placement->pos.vx != ACTOR_560800_PLACEMENT_HIDDEN_X) {
            _actor560800ShowCastMember(ACTOR_560800_CAST_PLAYER);
            TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, placement, 0);
        } else {
            _actor560800HideCastMember(ACTOR_560800_CAST_PLAYER);
        }
    }
    if (work->kyle != NULL) {
        placement = _gActor560800KylePlacementsByCut[cutId];
        if (placement->pos.vx != ACTOR_560800_PLACEMENT_HIDDEN_X) {
            _actor560800ShowCastMember(ACTOR_560800_CAST_KYLE);
            TASK_MESSAGE_DISPATCH_POINTER(work->kyle, ACTOR_MESSAGE_PLACE, placement, 0);
        } else {
            _actor560800HideCastMember(ACTOR_560800_CAST_KYLE);
        }
    }
    if (work->no9 != NULL) {
        placement = _gActor560800No9PlacementsByCut[cutId];
        if (placement->pos.vx != ACTOR_560800_PLACEMENT_HIDDEN_X) {
            _actor560800ShowCastMember(ACTOR_560800_CAST_NO9);
            TASK_MESSAGE_DISPATCH_POINTER(work->no9, ACTOR_MESSAGE_PLACE, placement, 0);
        } else {
            _actor560800HideCastMember(ACTOR_560800_CAST_NO9);
        }
    }
    if (work->eve != NULL) {
        placement = _gActor560800EvePlacementsByCut[cutId];
        if (placement->pos.vx != ACTOR_560800_PLACEMENT_HIDDEN_X) {
            _actor560800ShowCastMember(ACTOR_560800_CAST_EVE);
            TASK_MESSAGE_DISPATCH_POINTER(work->eve, ACTOR_MESSAGE_PLACE, placement, 0);
        } else {
            _actor560800HideCastMember(ACTOR_560800_CAST_EVE);
        }
    }
    if (work->chainGroup != NULL) {
        // The chains reuse the last live body's placement, normally Eve's.
        if (placement->pos.vx != ACTOR_560800_PLACEMENT_HIDDEN_X) {
            _actor560800ShowCastMember(ACTOR_560800_CAST_CHAINS);
            TASK_MESSAGE_DISPATCH_POINTER(work->chainGroup, ACTOR_MESSAGE_PLACE, placement, 0);
        } else {
            _actor560800HideCastMember(ACTOR_560800_CAST_CHAINS);
        }
    }
}

/// Restarts Aya in a clip from the scene's player-animation bank.
///
/// Requires cutscene work and a loaded clip at animationId (0..5 or 10..12).
/// An absent player is a no-op. Installs the borrowed scene bank, keeps world
/// collision enabled, records the clip for chaining and clears its hold counter.
/// The request is consumed synchronously; the bank must outlive playback.
static inline void _actor560800RestartPlayerAnimation(Task* task, u16 animationId)
{
    _Actor560800CutsceneWork* work;
    AnimationPlayRequest      request;

    work = task->work;
    if (work->player != NULL) {
        request.source.sets          = _gActor560800PlayerAnimationSets;
        work->playerAnimId           = animationId;
        request.animationId          = animationId;
        request.blend                = ANIMATION_BLEND_RESET;
        request.blendFrames          = 0;
        request.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &request, 0);
        work->playerAnimHold = 0;
    }
}

/// Blends Aya into a clip from this scene's player-animation bank.
///
/// `task` owns the cutscene work; an absent player makes this a no-op.
/// `animationId` must name a loaded scene clip, and `blendFrames` counts whole
/// normal-rate frames. Records the clip for chaining and clears its hold count.
/// The player borrows the scene's clip data and keeps world collision enabled.
static inline void _actor560800BlendPlayerAnimation(Task* task, u16 animationId, s32 blendFrames)
{
    _Actor560800CutsceneWork* work;
    AnimationPlayRequest      request;

    work = task->work;
    if (work->player != NULL) {
        request.source.sets          = _gActor560800PlayerAnimationSets;
        work->playerAnimId           = animationId;
        request.animationId          = animationId;
        request.blend                = ANIMATION_BLEND_INTERPOLATE;
        request.blendFrames          = blendFrames;
        request.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &request, 0);
        work->playerAnimHold = 0;
    }
}

/// Restarts the player in a clip from the equipped-weapon animation bank.
///
/// Requires a live player and loaded character/weapon bank. animationId is a
/// signed-halfword clip index in that bank (callers use 1, 3 and 9). Takes scripted
/// control with world collision disabled. Leaves the scene clip and hold count
/// unchanged; the five-word request is borrowed only during dispatch.
static inline void _actor560800RestartPlayerWeaponAnimation(s16 animationId)
{
    AnimationPlayRequest request;
    s32                  weaponId;

    weaponId                     = gPlayerStatus.weapon;
    request.source.index         = PLAYER_ACTOR_SCENE_WEAPON_BANK(weaponId);
    request.animationId          = animationId;
    request.blend                = ANIMATION_BLEND_RESET;
    request.blendFrames          = 0;
    request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &request, 0);
}

/// Blends the player into an equipped-weapon clip over ten frames.
///
/// Requires a live player task and its loaded character/weapon animation bank.
/// `animationId` is an index in that bank. This enters scripted playback and
/// disables the player's world-collision participation; it does not update the
/// cutscene's scene-clip id or hold count.
static inline void _actor560800BlendPlayerWeaponAnimation(s32 animationId)
{
    AnimationPlayRequest request;
    s32                  weaponId;

    weaponId                     = gPlayerStatus.weapon;
    request.source.index         = PLAYER_ACTOR_SCENE_WEAPON_BANK(weaponId);
    request.animationId          = animationId;
    request.blend                = ANIMATION_BLEND_INTERPOLATE;
    request.blendFrames          = ACTOR_560800_ANIMATION_BLEND_FRAMES;
    request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &request, 0);
}

/// Spawns four held ground decals farther from the player root.
///
/// Requires live cutscene work and the player model root. Offsets use the root's
/// local game-coordinate units: positive X and Z=-500..-800.
/// The packed request selects CLUT 0, full brightness and a half-side of
/// 64, 48, 32, 32 units. Placement snapshots the temporary offset; the decal
/// task never follows its retained offset pointer. Effects live until cancelled.
static inline void _actor560800SpawnOuterPlayerDecals(Task* task)
{
    _Actor560800CutsceneWork* work;
    SVECTOR                   offset;

    work      = task->work;
    offset.vx = 0x12C;
    offset.vy = 0;
    offset.vz = -0x1F4;
    effectSpawn(EFFECT_GROUND_DECAL, work->player->extra.tmd->coords, (EFFECT_GROUND_DECAL_START_FULL_BRIGHT | 64), &offset);
    offset.vx = 0x190;
    offset.vy = 0;
    offset.vz = -0x258;
    effectSpawn(EFFECT_GROUND_DECAL, work->player->extra.tmd->coords, (EFFECT_GROUND_DECAL_START_FULL_BRIGHT | 48), &offset);
    offset.vx = 0x12C;
    offset.vy = 0;
    offset.vz = -0x2BC;
    effectSpawn(EFFECT_GROUND_DECAL, work->player->extra.tmd->coords, (EFFECT_GROUND_DECAL_START_FULL_BRIGHT | 32), &offset);
    offset.vx = 0x1C2;
    offset.vy = 0;
    offset.vz = -0x320;
    effectSpawn(EFFECT_GROUND_DECAL, work->player->extra.tmd->coords, (EFFECT_GROUND_DECAL_START_FULL_BRIGHT | 32), &offset);
}

/// Spawns three held ground decals nearer the player root.
///
/// Requires live cutscene work and the player model root. Offsets use the root's
/// local game-coordinate units: positive X and Z=-200..0.
/// The packed request selects CLUT 0, full brightness and a half-side of
/// 64, 32, 32 units. Placement snapshots the temporary offset; the decal
/// task never follows its retained offset pointer. Effects live until cancelled.
static inline void _actor560800SpawnInnerPlayerDecals(Task* task)
{
    _Actor560800CutsceneWork* work;
    SVECTOR                   offset;

    work      = task->work;
    offset.vx = 0x12C;
    offset.vy = 0;
    offset.vz = -0xC8;
    effectSpawn(EFFECT_GROUND_DECAL, work->player->extra.tmd->coords, (EFFECT_GROUND_DECAL_START_FULL_BRIGHT | 64), &offset);
    offset.vx = 0x1F4;
    offset.vy = 0;
    offset.vz = -0x64;
    effectSpawn(EFFECT_GROUND_DECAL, work->player->extra.tmd->coords, (EFFECT_GROUND_DECAL_START_FULL_BRIGHT | 32), &offset);
    offset.vx = 0x1C2;
    offset.vy = 0;
    offset.vz = 0;
    effectSpawn(EFFECT_GROUND_DECAL, work->player->extra.tmd->coords, (EFFECT_GROUND_DECAL_START_FULL_BRIGHT | 32), &offset);
}

/// Advances Aya's animation chain and consumes her pending cutscene cue.
///
/// Requires initialized cutscene work and a live player. Cut cues 1..33 choose
/// clips, move the player along X, or preserve/cancel the ground decals at later
/// cuts. Cue 35 starts a half-rate weapon clip, waits eleven updates, then blends
/// to scene clip 12 over thirty frames. Multi-step cues retain their id while
/// waiting; every completed or unhandled cue clears it. Movement uses game units;
/// cue counters measure distance in cut 18 and updates in cue 35.
static void _actor560800HandlePlayerCue(Task* task)
{
    enum { ACTOR_560800_PLAYER_CUE_DELAYED_WEAPON_BLEND = 35 };
    _Actor560800CutsceneWork* work;

    work = task->work;
    _actor560800AdvancePlayerAnimChain(task);
    switch (work->playerCue.id) {
        case ACTOR_560800_CUE_NONE:
        case 38:
            break;
        case 1:
            _actor560800RestartPlayerWeaponAnimation(PLAYER_ANIMATION_IDLE);
            break;
        case 3:
            _actor560800BlendPlayerWeaponAnimation(PLAYER_ANIMATION_AIM_ENTER);
            break;
        case 9:
            _actor560800BlendPlayerAnimation(task, 0, ACTOR_560800_ANIMATION_BLEND_FRAMES);
            break;
        case 12:
            _actor560800RestartPlayerWeaponAnimation(PLAYER_ANIMATION_AIM_IDLE);
            break;
        case 16:
            _actor560800RestartPlayerAnimation(task, 0xC);
            break;
        case 18:
            switch (work->playerCue.step) {
                case 0:
                    _actor560800RestartPlayerWeaponAnimation(PLAYER_ANIMATION_BACKWARD_WALK);
                    taskMessageDispatch(work->player, ANIMATION_MESSAGE_SET_RATE, ANIMATION_RATE_ONE / 2, 0);
                    work->playerCue.counter = 0;
                    work->playerCue.step++;
                    return;
                case 1:
                    if (work->playerCue.counter < 100) {
                        work->playerCue.counter      += 5;
                        gPlayerStatus.coordMtx->t[0] -= 5;
                        return;
                    }
                    _actor560800BlendPlayerAnimation(task, 0xC, ACTOR_560800_ANIMATION_BLEND_FRAMES);
                    break;
                default:
                    return;
            }
            break;
        case 19:
            _actor560800RestartPlayerAnimation(task, 1);
            work->playerCue.id = ACTOR_560800_CUE_NONE;
            return;
        case 21:
            _actor560800RestartPlayerAnimation(task, 3);
            _actor560800SpawnOuterPlayerDecals(task);
            work->keepEffects = true;
            break;
        case 28:
            _actor560800RestartPlayerAnimation(task, 4);
            work->playerCue.id = ACTOR_560800_CUE_NONE;
            return;
        case 29:
            _actor560800SpawnOuterPlayerDecals(task);
            _actor560800SpawnInnerPlayerDecals(task);
            work->keepEffects = true;
            break;
        case 22:
        case 32:
            work->keepEffects = false;
            break;
        case 33:
            _actor560800SpawnOuterPlayerDecals(task);
            _actor560800SpawnInnerPlayerDecals(task);
            work->keepEffects = true;
            _actor560800RestartPlayerAnimation(task, 5);
            break;
        case ACTOR_560800_PLAYER_CUE_DELAYED_WEAPON_BLEND:
            switch (work->playerCue.step) {
                case 0:
                    _actor560800BlendPlayerWeaponAnimation(PLAYER_ANIMATION_AIM_EXIT);
                    taskMessageDispatch(work->player, ANIMATION_MESSAGE_SET_RATE, ANIMATION_RATE_ONE / 2, 0);
                    work->playerCue.counter = 0;
                    work->playerCue.step++;
                    return;
                case 1:
                    if (++work->playerCue.counter < 11) {
                        return;
                    }
                    _actor560800BlendPlayerAnimation(task, 0xC, 30);
                    work->playerCue.id = ACTOR_560800_CUE_NONE;
                    return;
                default:
                    return;
            }
            break;
    }
    work->playerCue.id = ACTOR_560800_CUE_NONE;
}

/// Restarts a cast body's non-root tracks in a clip at the requested rate.
///
/// Requires live cast work and a loaded clip covering slots 1..slotCount - 1;
/// slotCount is 19 for Eve/No. 9 and 20 for Kyle. Slot 0 stays untouched.
/// animationRate is in sixteenths of a frame (16 normal speed, 8 half speed at
/// current calls); the work keeps its low signed halfword and each slot its low
/// signed byte. Records the clip and rate and clears the timed-chain hold count.
static inline void _actor560800RestartCastAnimationAtRate(Task* task, u16 animationId, u16 animationRate)
{
    _Actor560800CastWork* work;
    u16                   slotIndex;

    work           = task->work;
    slotIndex      = 1;
    work->animId   = animationId;
    work->animRate = animationRate;
    work->animHold = 0;
    if (slotIndex < work->slotCount) {
        do {
            work->rig.slots[slotIndex].rate = animationRate;
            animationResetSlot(&work->rig.anim, slotIndex, animationId);
            slotIndex++;
        } while (slotIndex < work->slotCount);
    }
}

/// Consumes Eve's pending cutscene cue.
///
/// Requires initialized cutscene work and the live body or chain group selected
/// by the cue. Cuts 22/24 hide the chains; cut 28 restarts Eve's clip 3 at normal
/// speed. Cuts 1/28 also write yaw/pause fields that Eve's update leaves unread.
/// All cues are cleared after this update, including unhandled ids.
static void _actor560800HandleEveCue(Task* task)
{
    _Actor560800CutsceneWork* work;
    _Actor560800CastWork*     heldWork;
    _Actor560800CastWork*     releasedWork;
    SVECTOR                   unusedFrameStorage;

    work = task->work;
    switch (work->eveCue.id) {
        case 0:
        case 38:
            break;
        case 1:
            heldWork             = work->eve->work;
            heldWork->part4Yaw   = 0;
            heldWork->animPaused = 1;
            break;
        case 22:
        case 24:
            taskMessageDispatch(work->chainGroup, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, 0);
            break;
        case 28:
            releasedWork             = work->eve->work;
            releasedWork->part4Yaw   = 0;
            releasedWork->animPaused = 0;
            _actor560800RestartCastAnimationAtRate(work->eve, 3, ANIMATION_RATE_ONE);
            break;
    }
    work->eveCue.id = 0;
}

/// Restarts a cast body's non-root tracks at normal playback speed.
///
/// Requires initialized cast work and a loaded clip whose tracks cover slots
/// 1..slotCount - 1, with slotCount 19 for Eve/No. 9 and 20 for Kyle.
/// Records the clip and normal rate in sixteenths of a frame, clears the hold
/// count, and applies that rate to every restarted slot; slot 0 is untouched.
static inline void _actor560800RestartCastAnimation(_Actor560800CastWork* work, u16 animationId)
{
    u16 slotIndex;
    u16 animationRate;

    work->animId   = animationId;
    slotIndex      = 1;
    animationRate  = ANIMATION_RATE_ONE;
    work->animRate = animationRate;
    work->animHold = 0;
    for (; slotIndex < work->slotCount; slotIndex++) {
        work->rig.slots[slotIndex].rate = animationRate;
        animationResetSlot(&work->rig.anim, slotIndex, animationId);
    }
}

/// Consumes No. 9's pending cutscene cue.
///
/// Requires initialized cutscene work and No. 9's live nineteen-slot rig.
/// Cut cues restart non-root tracks at normal rate. Cut 24 holds its cue while
/// waiting 181 updates before blending clip 45 to clip 29; cut 26 also hides the
/// floor quad. Completed and unhandled cues clear the id; waiting cues retain it.
static void _actor560800HandleNo9Cue(Task* task)
{
    _Actor560800CutsceneWork* work;

    work = task->work;
    switch (work->no9Cue.id) {
        case ACTOR_560800_CUE_NONE:
        case 38:
            break;
        case 2:
            _actor560800RestartCastAnimation(work->no9->work, 1);
            break;
        case 4:
            _actor560800RestartCastAnimation(work->no9->work, 5);
            break;
        case 6:
            _actor560800RestartCastAnimation(work->no9->work, 0xc);
            break;
        case 8:
            _actor560800RestartCastAnimation(work->no9->work, 0x28);
            break;
        case 10:
            _actor560800RestartCastAnimation(work->no9->work, 0x17);
            break;
        case 11:
            _actor560800RestartCastAnimation(work->no9->work, 0x18);
            break;
        case 13:
            _actor560800RestartCastAnimation(work->no9->work, 0x19);
            break;
        case 15:
            _actor560800RestartCastAnimation(work->no9->work, 0xc);
            break;
        case 17:
            _actor560800RestartCastAnimation(work->no9->work, 0x29);
            break;
        case 22:
            _actor560800RestartCastAnimation(work->no9->work, 0x1f);
            break;
        case 23:
            _actor560800RestartCastAnimation(work->no9->work, 0x1d);
            break;
        case 24:
            switch (work->no9Cue.step) {
                case 0:
                    _actor560800RestartCastAnimationAtRate(work->no9, 0x2D, ANIMATION_RATE_ONE);
                    work->no9Cue.counter = 0;
                    work->no9Cue.step++;
                    return;
                case 1:
                    if (++work->no9Cue.counter < 181) {
                        return;
                    }
                    _actor560800BlendCastAnimation(work->no9, 0x1D, ANIMATION_RATE_ONE);
                    break;
                default:
                    return;
            }
            break;
        case 26:
            _actor560800RestartCastAnimation(work->no9->work, 0x21);
            ((_Actor560800CastWork*)work->no9->work)->floorQuadHidden = true;
            break;
        case 27:
            _actor560800RestartCastAnimation(work->no9->work, 0x24);
            break;
    }
    work->no9Cue.id = ACTOR_560800_CUE_NONE;
}

/// Restarts Kyle's firing clip and emits his handgun flash, casing and vibration.
///
/// Requires the published cutscene task and Kyle's live twenty-slot rig. Restarts
/// slots 1..19 in clip 32 at normal rate, leaving slot 0 intact; the effect attaches
/// to model part 8. Pulses port 0's variable motor at full strength for two ticks.
/// The event-script argument is ignored.
static void _actor560800FireKyleGun(s32 unusedArg)
{
    _Actor560800CutsceneWork* work;
    // The original stack allocation remains required for the matching frame.
    SVECTOR unusedFrameStorage;

    work = _gActor560800CutsceneTask->work;
    _actor560800RestartCastAnimationAtRate(work->kyle, 0x20, ANIMATION_RATE_ONE);
    effectSpawn(EFFECT_HANDGUN_MUZZLE_FLASH, &work->kyle->extra.tmd->coords[8], ACTOR_560800_KYLE_GUN_EFFECT, NULL);
    padPostVibrationRequest(0, PAD_VIBRATION_MOTOR_VARIABLE, PAD_VIBRATION_INTENSITY_MAX, 2);
}

/// Records display ticks elapsed in the third scene phase.
///
/// Requires the third phase's recorded game-owned vblank timestamp. Writes the
/// elapsed slot used by scene timing; no playback state changes. Compares the
/// counter as unsigned, then retains signed subtraction and the one-tick-short
/// result when the counter has wrapped past its timestamp.
static inline void _actor560800RecordThirdPhaseElapsed(void)
{
    if ((u32)_gActor560800ThirdPhaseStartTick > (u32)gDisplayState.frameCount) {
        _gActor560800ThirdPhaseElapsedTicks = gDisplayState.frameCount - (_gActor560800ThirdPhaseStartTick + 1);
    } else {
        _gActor560800ThirdPhaseElapsedTicks = gDisplayState.frameCount - _gActor560800ThirdPhaseStartTick;
    }
}

/// Consumes Kyle's pending cutscene cue, including movement and both gunshots.
///
/// Requires initialized cutscene work and live bodies/attachments used by the
/// selected cue; the handgun itself may be NULL. Cut cues 1..33 control clips,
/// pose angles, lighting and visibility. Mid-cut cues 35..39 blend clips, shoot
/// No. 9, decrease/restore part 4 yaw, or restart clip 34 at half rate. Angles
/// use 4096 units per turn and translations use game units. Multi-step cues keep
/// their id across updates; completed and unhandled cues clear it. Cut 20's
/// counter measures slide distance first, then update ticks.
static void _actor560800HandleKyleCue(Task* task)
{
    enum {
        ACTOR_560800_KYLE_CUE_DELAYED_CLIP_BLEND = 35,
        ACTOR_560800_KYLE_CUE_SHOOT_NO9          = 36,
        ACTOR_560800_KYLE_CUE_DECREASE_PART4_YAW = 37,
        ACTOR_560800_KYLE_CUE_RESTORE_PART4_YAW  = 38,
        ACTOR_560800_KYLE_CUE_HALF_RATE_CLIP     = 39,
    };
    _Actor560800CutsceneWork* work;
    _Actor560800CastWork*     releasedWork;
    _Actor560800CastWork*     shownWork;
    _Actor560800CastWork*     rolledWork;
    _Actor560800CastWork*     restoredWork;
    _Actor560800CastWork*     pitchWork;
    _Actor560800CastWork*     turnWork;
    _Actor560800CastWork*     returnTurnWork;
    _Actor560800CastWork*     hitWork;
    _Actor560800CastWork*     returnClipWork;
    GfxCoord*                 kyleRoot;

    work = task->work;
    switch (work->kyleCue.id) {
        case ACTOR_560800_CUE_NONE:
            break;
        case 1:
            ((_Actor560800CastWork*)work->kyle->work)->animPaused = true;
            break;
        case 2:
            ((_Actor560800CastWork*)work->kyle->work)->part4Yaw = 0x155;
            break;
        case 4:
            switch (work->kyleCue.step) {
                case 0: {
                    _Actor560800CastWork* blendWork;

                    releasedWork             = work->kyle->work;
                    releasedWork->part4Yaw   = 0;
                    releasedWork->animPaused = false;
                    blendWork                = work->kyle->work;
                    blendWork->animId        = 1;
                    blendWork->animRate      = ANIMATION_RATE_ONE;
                    blendWork->animHold      = 0;
                    ACTOR_560800_BLEND_CAST_SLOTS(blendWork, 1, ACTOR_560800_ANIMATION_BLEND_FRAMES);
                    work->kyleCue.step++;
                    return;
                }
                case 1:
                    ((_Actor560800CastWork*)work->kyle->work)->animPaused = true;
                    break;
                default:
                    return;
            }
            break;
        case 14:
            switch (work->kyleCue.step) {
                case 0:
                    _actor560800RestartCastAnimationAtRate(work->kyle, 0x19, ANIMATION_RATE_ONE);
                    work->kyleCue.step++;
                    return;
                case 1:
                    kyleRoot              = work->kyle->extra.tmd->coords;
                    kyleRoot->coord.t[0] -= 0x1E;
                    if (work->kyle->extra.tmd->coords->coord.t[0] < _gActor560800KyleCutPlacements[ACTOR_560800_KYLE_APPROACH_PLACEMENT].pos.vx) {
                        TASK_MESSAGE_DISPATCH_POINTER(work->kyle, ACTOR_MESSAGE_PLACE, &_gActor560800KyleCutPlacements[ACTOR_560800_KYLE_APPROACH_PLACEMENT], 0);
                        _actor560800BlendCastAnimation(work->kyle, 0x1A, ANIMATION_RATE_ONE);
                        work->kyleCue.id = ACTOR_560800_CUE_NONE;
                    }
                    work->kyle->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                    return;
                default:
                    return;
            }
            break;
        case 15:
            _actor560800RestartCastAnimationAtRate(work->kyle, 5, ANIMATION_RATE_ONE);
            break;
        case 18:
            _actor560800RestartCastAnimationAtRate(work->kyle, 0x1F, ANIMATION_RATE_ONE);
            break;
        // The shot at Aya follows her slide, then a three-update impact delay.
        case 20:
            switch (work->kyleCue.step) {
                case 0:
                    ((_Actor560800CastWork*)work->kyle->work)->relit = true;
                    _actor560800BlendPlayerWeaponAnimation(PLAYER_ANIMATION_BACKWARD_WALK);
                    taskMessageDispatch(work->player, ANIMATION_MESSAGE_SET_RATE, ANIMATION_RATE_ONE / 2, 0);
                    work->kyleCue.counter = 0;
                    work->kyleCue.step++;
                    return;
                case 1:
                    if (work->kyleCue.counter < 300) {
                        work->kyleCue.counter        += 5;
                        gPlayerStatus.coordMtx->t[0] -= 5;
                        return;
                    }
                    _actor560800BlendPlayerAnimation(task, 2, ACTOR_560800_ANIMATION_BLEND_FRAMES);
                    work->kyleCue.counter = 0;
                    work->kyleCue.step++;
                    return;
                case 2:
                    if (++work->kyleCue.counter < 11) {
                        return;
                    }
                    _actor560800FireKyleGun(0);
                    work->kyleCue.counter = 0;
                    work->kyleCue.step++;
                    return;
                case 3:
                    if (++work->kyleCue.counter < 3) {
                        return;
                    }
                    effectSpawn(EFFECT_HIT_PUFF, &work->player->extra.tmd->coords[6], 0, NULL);
                    padPostVibrationRequest(0, PAD_VIBRATION_MOTOR_VARIABLE, PAD_VIBRATION_INTENSITY_MAX, 2);
                    break;
                default:
                    return;
            }
            break;
        case 21:
            ((_Actor560800CastWork*)work->kyle->work)->relit = false;
            _actor560800RestartCastAnimationAtRate(work->kyle, 0xA, ANIMATION_RATE_ONE);
            break;
        case 22:
            _actor560800RestartCastAnimationAtRate(work->kyle, 0xB, ANIMATION_RATE_ONE);
            break;
        case 23:
            taskMessageDispatch(work->kyle, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, 0);
            taskMessageDispatch(work->kyleGunHand, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, 0);
            taskMessageDispatch(work->kyleFreeHand, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, 0);
            if (work->kyleGun != NULL) {
                taskMessageDispatch(work->kyleGun, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, 0);
            }
            break;
        case 24:
            taskMessageDispatch(work->kyle, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            taskMessageDispatch(work->kyleGunHand, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            taskMessageDispatch(work->kyleFreeHand, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            if (work->kyleGun != NULL) {
                taskMessageDispatch(work->kyleGun, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            }
            shownWork           = work->kyle->work;
            shownWork->part4Yaw = 0x155;
            shownWork->relit    = true;
            break;
        case 25:
            rolledWork            = work->kyle->work;
            rolledWork->part4Yaw  = 0;
            rolledWork->part2Roll = -0x71;
            _actor560800RestartCastAnimationAtRate(work->kyle, 0x1F, ANIMATION_RATE_ONE);
            break;
        case 26:
            restoredWork            = work->kyle->work;
            restoredWork->part2Roll = 0;
            restoredWork->relit     = false;
            _actor560800RestartCastAnimationAtRate(work->kyle, 0xD, ANIMATION_RATE_ONE);
            break;
        case 28:
            _actor560800RestartCastAnimationAtRate(work->kyle, 0x1A, ANIMATION_RATE_ONE);
            break;
        case 30:
            pitchWork              = work->kyle->work;
            pitchWork->part4Pitch -= 0x1E;
            if (pitchWork->part4Pitch >= -0x155) {
                return;
            }
            break;
        case 32:
            ((_Actor560800CastWork*)work->kyle->work)->part4Pitch = 0;
            _actor560800RestartCastAnimationAtRate(work->kyle, 0xF, ANIMATION_RATE_ONE);
            break;
        case 33:
            _actor560800RecordThirdPhaseElapsed();
            _actor560800RestartCastAnimationAtRate(work->kyle, 0x10, ANIMATION_RATE_ONE);
            break;
        case ACTOR_560800_KYLE_CUE_DELAYED_CLIP_BLEND:
            switch (work->kyleCue.step) {
                case 0: {
                    _Actor560800CastWork* blendWork;

                    blendWork           = work->kyle->work;
                    blendWork->animId   = 7;
                    blendWork->animRate = ANIMATION_RATE_ONE;
                    blendWork->animHold = 0;
                    ACTOR_560800_BLEND_CAST_SLOTS(blendWork, 7, ACTOR_560800_ANIMATION_BLEND_FRAMES);
                    work->kyleCue.counter = 0;
                    work->kyleCue.step++;
                    return;
                }
                case 1:
                    if (++work->kyleCue.counter < 91) {
                        return;
                    }
                    _actor560800BlendCastAnimation(work->kyle, 0x14, ANIMATION_RATE_ONE);
                    break;
                default:
                    return;
            }
            break;
        // No. 9's recoil clip blends at half rate before the delayed hit puff.
        case ACTOR_560800_KYLE_CUE_SHOOT_NO9:
            switch (work->kyleCue.step) {
                case 0:
                    _actor560800RestartCastAnimationAtRate(work->kyle, 0xE, ANIMATION_RATE_ONE);
                    work->kyleCue.counter = 0;
                    work->kyleCue.step++;
                    return;
                case 1:
                    if (++work->kyleCue.counter < 31) {
                        return;
                    }
                    padPostVibrationRequest(0, PAD_VIBRATION_MOTOR_VARIABLE, PAD_VIBRATION_INTENSITY_MAX, 2);
                    effectSpawn(EFFECT_HANDGUN_MUZZLE_FLASH, &work->kyle->extra.tmd->coords[8], ACTOR_560800_KYLE_GUN_EFFECT, NULL);
                    hitWork           = work->no9->work;
                    hitWork->animId   = 0x20;
                    hitWork->animRate = ANIMATION_RATE_ONE / 2;
                    _actor560800ResetAnimHold(hitWork);
                    ACTOR_560800_BLEND_CAST_SLOTS(hitWork, 0x20, 5);
                    work->kyleCue.counter = 0;
                    work->kyleCue.step++;
                    return;
                case 2:
                    if (++work->kyleCue.counter < 3) {
                        return;
                    }
                    effectSpawn(EFFECT_HIT_PUFF, &work->no9->extra.tmd->coords[4], 0, NULL);
                    break;
                default:
                    return;
            }
            break;
        case ACTOR_560800_KYLE_CUE_DECREASE_PART4_YAW:
            ((_Actor560800CastWork*)work->kyle->work)->animPaused = false;
            turnWork                                              = work->kyle->work;
            turnWork->part4Yaw                                   -= 0x3C;
            if (turnWork->part4Yaw >= -0x200) {
                return;
            }
            break;
        case ACTOR_560800_KYLE_CUE_RESTORE_PART4_YAW:
            returnTurnWork = work->kyle->work;
            switch (work->kyleCue.step) {
                case 0:
                    returnClipWork = work->kyle->work;
                    SOFT_TOUCH_REG(returnClipWork);
                    returnClipWork->animId   = 3;
                    returnClipWork->animRate = ANIMATION_RATE_ONE;
                    returnClipWork->animHold = 0;
                    ACTOR_560800_BLEND_CAST_SLOTS(returnClipWork, 3, ACTOR_560800_ANIMATION_BLEND_FRAMES);
                    work->kyleCue.step++;
                    return;
                case 1:
                    returnTurnWork->part4Yaw += 0x3C;
                    if (returnTurnWork->part4Yaw < 0) {
                        return;
                    }
                    returnTurnWork->part4Yaw = 0;
                    break;
                default:
                    return;
            }
            break;
        case ACTOR_560800_KYLE_CUE_HALF_RATE_CLIP:
            _actor560800RestartCastAnimationAtRate(work->kyle, 0x22, ANIMATION_RATE_ONE / 2);
            break;
    }
    work->kyleCue.id = ACTOR_560800_CUE_NONE;
}
#undef ACTOR_560800_BLEND_CAST_SLOTS

/// Records a phase's elapsed display ticks and finishes its streamed scene.
///
/// Phase ids 1..3 select the matching saved timestamp and elapsed counter.
/// Other ids skip timing but still cancel and finish the stream. Timestamps
/// must have been recorded for the selected phase. A wrapped counter retains
/// the original result, one tick shorter than unsigned modular subtraction.
static void _actor560800FinishScenePhase(s32 phaseId)
{
    /// Records elapsed display ticks, retaining the extra subtraction after wrap.
    ///
    /// Counter must be a writable signed word; timestamp a side-effect-free word.
    /// Evaluates counter once and timestamp twice. Reads the live display counter
    /// for comparison and subtraction. Used only for this phase update.
#define ACTOR_560800_RECORD_PHASE_TICKS(counter, timestamp)           \
    {                                                                 \
        if ((u32)(timestamp) > (u32)gDisplayState.frameCount) {       \
            (counter) = gDisplayState.frameCount - ((timestamp) + 1); \
        } else {                                                      \
            (counter) = gDisplayState.frameCount - (timestamp);       \
        }                                                             \
    }
    if (phaseId == ACTOR_560800_SCENE_PHASE_FIRST) {
        ACTOR_560800_RECORD_PHASE_TICKS(_gActor560800FirstPhaseElapsedTicks, _gActor560800FirstPhaseStartTick);
    } else if (phaseId == ACTOR_560800_SCENE_PHASE_SECOND) {
        ACTOR_560800_RECORD_PHASE_TICKS(_gActor560800SecondPhaseElapsedTicks, _gActor560800SecondPhaseStartTick);
    } else if (phaseId == ACTOR_560800_SCENE_PHASE_THIRD) {
        ACTOR_560800_RECORD_PHASE_TICKS(_gActor560800ThirdPhaseElapsedTicks, _gActor560800ThirdPhaseStartTick);
    }
#undef ACTOR_560800_RECORD_PHASE_TICKS
    // Completion also restores the random generators saved by scene playback.
    cdCmdCancelScene();
    streamFinishScene();
}

/// Allocates the scene controller work and spawns its cast and props.
///
/// Borrows the live player, publishes the controller, then creates Eve, Kyle
/// with hands/gun, No. 9 with gunblade, the chains and the carrier. Child tasks
/// join the scene's teardown tree during their own initialization. Sets the
/// ambient-colour override to 1440 in each colour-matrix translation channel.
/// Allocation failure kills the controller; individual spawn failures remain
/// unchecked here. The caller continues after this void initializer returns.
static void _actor560800InitializeCutscene(Task* task)
{
    enum { ACTOR_560800_SCENE_AMBIENT_LEVEL = 1440 };
    _Actor560800CutsceneWork* work;
    Task*                     kyle;
    Task*                     no9;
    SVECTOR                   ambientColor;

    work       = memMalloc(sizeof(*work), false);
    task->work = work;
    if (work == NULL) {
        taskKill(task);
        return;
    }
    memFillBytes(work, 0, sizeof(*work));
    work->player              = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    _gActor560800CutsceneTask = task;
    work->eve                 = taskSpawnFromTable(gActor560800CutsceneTaskDescriptors, ACTOR_560800_TASK_EVE_MASKED, 0, 0);
    kyle                      = taskSpawnFromTable(gActor560800CutsceneTaskDescriptors, ACTOR_560800_TASK_KYLE, 0, 0);
    work->kyle                = kyle;
    work->kyleGunHand         = taskSpawnFromTable(gActor560800CutsceneTaskDescriptors, ACTOR_560800_TASK_KYLE_GUN_HAND, ACTOR_560800_ATTACHMENT_GUN_HAND, kyle);
    work->kyleFreeHand        = taskSpawnFromTable(gActor560800CutsceneTaskDescriptors, ACTOR_560800_TASK_KYLE_FREE_HAND, ACTOR_560800_ATTACHMENT_FREE_HAND, work->kyle);
    work->kyleGun             = taskSpawnFromTable(gActor560800CutsceneTaskDescriptors, ACTOR_560800_TASK_KYLE_GUN, ACTOR_560800_ATTACHMENT_GUN, work->kyle);
    no9                       = taskSpawnFromTable(gActor560800CutsceneTaskDescriptors, ACTOR_560800_TASK_NO9, 0, 0);
    work->no9                 = no9;
    work->no9Gunblade         = taskSpawnFromTable(gActor560800CutsceneTaskDescriptors, ACTOR_560800_TASK_NO9_GUNBLADE, ACTOR_560800_ATTACHMENT_GUNBLADE, no9);
    work->chainGroup          = taskSpawnFromTable(_gActor560800PropTaskDescriptors, ACTOR_560800_PROP_TASK_CHAIN_GROUP, 0, task);
    work->carrierModel        = taskSpawnFromTable(_gActor560800PropTaskDescriptors, ACTOR_560800_PROP_TASK_CARRIER, 0, task);
    ambientColor.vx           = ACTOR_560800_SCENE_AMBIENT_LEVEL;
    ambientColor.vy           = ACTOR_560800_SCENE_AMBIENT_LEVEL;
    ambientColor.vz           = ACTOR_560800_SCENE_AMBIENT_LEVEL;
    worldCoordSetAmbientColorOverride(&ambientColor);
}

/// Runs the scripted scene, its four actor cues and Eve's model replacement.
///
/// Waits for attachment-wheel and display transitions before allocating cast
/// work and selecting CAP file 0 at VRAM texture origin (384,0). Starts the event
/// script with its skip sequence while retaining HUD state. When the event ends,
/// restores the saved random state, cancels effects, restarts equipped-weapon
/// clip 1 with world collision disabled, and schedules scene-tree teardown.
/// Actor cues run every active update; scene cue 1 replaces Eve after killing
/// her old body. The player and loaded CAP/animation resources remain borrowed.
static void _actor560800CutsceneTask(Task* task)
{
    enum {
        ACTOR_560800_SCENE_WAIT_PRESENTATION = 0,
        ACTOR_560800_SCENE_START_SCRIPT      = 1,
        ACTOR_560800_SCENE_RUN_SCRIPT        = 2,
        ACTOR_560800_SCENE_CUE_REPLACE_EVE   = 1,
    };
    AnimationPlayRequest      request;
    _Actor560800CutsceneWork* work;
    s32                       weaponId;

    switch (task->state) {
        case ACTOR_560800_SCENE_WAIT_PRESENTATION:
            if (gAttachmentState.mode == ATTACHMENT_MODE_WHEEL || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                return;
            }
            _actor560800InitializeCutscene(task);
            gCapFile = 0;
            capSelectLoadedFile(0);
            capSetTexturePage(ACTOR_560800_CAP_TEXTURE_VRAM_X, ACTOR_560800_CAP_TEXTURE_VRAM_Y);
            task->state++;
            // Start the script in the same update that finishes initialization.
        case ACTOR_560800_SCENE_START_SCRIPT:
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            evsStartScriptWithSkip(_gActor560800CutsceneScript, EVENT_SCRIPT_HUD_KEEP, _gActor560800CutsceneSkipScript);
            task->state++;
            break;
        case ACTOR_560800_SCENE_RUN_SCRIPT:
            if (gGameSession->eventState == 0) {
                // Restore the saved random state and weapon pose before scheduling teardown.
                gRandomLcgState = _gActor560800SavedRandomState;
                roomEffectRequestCancelAll();
                weaponId                     = gPlayerStatus.weapon;
                request.source.index         = PLAYER_ACTOR_SCENE_WEAPON_BANK(weaponId);
                request.animationId          = PLAYER_ANIMATION_IDLE;
                request.blend                = ANIMATION_BLEND_RESET;
                request.blendFrames          = 0;
                request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &request, 0);
                taskRequestKill(task, 0);
                return;
            }
            break;
    }
    _actor560800HandlePlayerCue(task);
    _actor560800HandleEveCue(task);
    _actor560800HandleNo9Cue(task);
    _actor560800HandleKyleCue(task);
    work = task->work;
    switch (work->sceneCue.id) {
        case ACTOR_560800_CUE_NONE:
            break;
        case ACTOR_560800_SCENE_CUE_REPLACE_EVE:
            if (work->eve != NULL) {
                taskKill(work->eve);
            }
            displaySpawnTaskFromTable(gActor560800CutsceneTaskDescriptors, ACTOR_560800_TASK_REPLACE_EVE, 0, 0);
            break;
    }
    work->sceneCue.id = ACTOR_560800_CUE_NONE;
}

/// Hands display control to the scene's movie task and releases this launcher.
///
/// Forwards spawnArg1 unchanged (zero selects movie 100, nonzero movie 101).
/// Queues the current camera and packets after switching to task-only display.
/// The spawned task owns playback and restoration; this launcher owns no work.
/// The display handoff also occurs when the spawn fails.
static void _actor560800StartMovieTask(Task* task)
{
    displaySpawnTaskFromTable(gActor560800MovieTaskDescriptors, ACTOR_560800_MOVIE_TASK_PLAYBACK, task->spawnArg1.value, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    viewQueueCurrentCameraAndPackets();
    taskKill(task);
}

/// Darkens the cutscene, then disables display when its subtractive ramp finishes.
///
/// Starts at state 0; low 16 bits of `spawnArg1` add intensity units per update
/// (scene callers use 2, 5 or 8). Owns a `ScreenFadeWork` until task teardown and
/// hangs below the published cutscene. Allocation failure kills this task.
/// Draws red/green/red before each step, including initialization; channels
/// narrow to signed 16 bits without clamping. Zero rate never finishes.
static void _actor560800FadeOutTask(Task* task)
{
    enum { ACTOR_560800_FADE_INITIALIZE = 0,
           ACTOR_560800_FADE_RAMP       = 1 };
    ScreenFadeWork* fade;
    ScreenFadeWork* allocatedFade;

    fade = task->work;
    switch (task->state) {
        case ACTOR_560800_FADE_INITIALIZE:
            allocatedFade = memMalloc(sizeof(*allocatedFade), false);
            task->work    = allocatedFade;
            if (allocatedFade == NULL) {
                taskKill(task);
                return;
            }
            fade    = allocatedFade;
            fade->b = 0;
            fade->g = 0;
            fade->r = 0;
            taskReparent(_gActor560800CutsceneTask, task);
            task->state += 1;
            /* fallthrough */
        case ACTOR_560800_FADE_RAMP:
            fadeDrawOverlay(fade->r, fade->g, fade->r, GPU_BLEND_SUBTRACT);
            _screenFadeStepUp(fade, task);
            if (fade->r >= SCREEN_FADE_OUT_COMPLETE) {
                SetDispMask(0);
                taskKill(task);
            }
            break;
    }
}

/// Reveals the cutscene through a shrinking subtractive overlay and delayed display enable.
///
/// Starts at state 0 with intensity 255; low 16 bits of `spawnArg1` are the
/// per-update decrement (scene callers use 5, 8 or 10). Owns its fade work until
/// teardown and hangs below the published cutscene; allocation failure kills it.
/// Every update draws red/green/red before signed-halfword ramp truncation.
/// State 6 enables display; state 7 keeps ramping until red becomes negative.
/// Zero rate never finishes.
static void _actor560800FadeInTask(Task* task)
{
    enum { ACTOR_560800_FADE_INITIALIZE     = 0,
           ACTOR_560800_FADE_ENABLE_DISPLAY = 6,
           ACTOR_560800_FADE_RAMP           = 7 };
    ScreenFadeWork* fade;
    ScreenFadeWork* allocatedFade;

    fade = task->work;
    switch (task->state) {
        case ACTOR_560800_FADE_INITIALIZE:
            allocatedFade = memMalloc(sizeof(*allocatedFade), false);
            task->work    = allocatedFade;
            if (allocatedFade == NULL) {
                taskKill(task);
                return;
            }
            fade    = allocatedFade;
            fade->b = SCREEN_FADE_FULL_INTENSITY;
            fade->g = SCREEN_FADE_FULL_INTENSITY;
            fade->r = SCREEN_FADE_FULL_INTENSITY;
            taskReparent(_gActor560800CutsceneTask, task);
            task->state += 1;
            break;
        case ACTOR_560800_FADE_ENABLE_DISPLAY:
            SetDispMask(1);
            /* fallthrough */
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
            task->state += 1;
            break;
        case ACTOR_560800_FADE_RAMP:
            break;
        default:
            return;
    }
    fadeDrawOverlay(fade->r, fade->g, fade->r, GPU_BLEND_SUBTRACT);
    _screenFadeStepDown(fade, task);
    if (fade->r < 0) {
        taskKill(task);
    }
}

/// Handles draw commands for a live cast body or attachment model.
///
/// Mode 1 enables active drawing and automatic buffering; mode 2 excludes both.
/// Mode 0 and other values leave every flag unchanged. Ignores the message id
/// and second argument. The callback has no usable return value.
static void _actor560800SetCastModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg)
{
    TmdObject* model;

    model = task->extra.tmd;
    switch (drawMode) {
        case ACTOR_MESSAGE_DRAW_HIDE:
            break;
        case ACTOR_MESSAGE_DRAW_SHOW:
            model->flags = model->flags & (u16) ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            return;
        case ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER:
            model->flags = model->flags | (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            return;
    }
}

#include "../../shared/actor/actor_messages_place_ypr.inc.c"

/// Brightens the cutscene by spawning its subtractive fade-in task.
///
/// Forwards intensityStep to spawnArg1; the task uses its low unsigned halfword
/// as intensity units per update. Requires the published scene; the spawned
/// task joins its teardown tree. Does not check or return spawn success.
static void _actor560800SpawnFadeIn(s32 intensityStep)
{
    taskSpawnFromTable(gActor560800CutsceneTaskDescriptors, ACTOR_560800_TASK_FADE_IN, intensityStep, 0);
}

/// Darkens the cutscene by spawning its subtractive fade-out task.
///
/// Forwards intensityStep to spawnArg1; the task uses its low unsigned halfword
/// as intensity units per update. Requires the published scene; the spawned
/// task joins its teardown tree. Does not check or return spawn success.
static void _actor560800SpawnFadeOut(s32 intensityStep)
{
    taskSpawnFromTable(gActor560800CutsceneTaskDescriptors, ACTOR_560800_TASK_FADE_OUT, intensityStep, 0);
}

/// Sends a synchronous action command to the scene's chain group.
///
/// Requires a published scene and live chain group. Only the low halfword
/// of commandId is sent; the receiver ignores the uninitialized context word.
/// Commands: 0 relight, 1 target Eve/second pose, 2 extend, 3 third pose,
/// 4 first pose, 5 detached clip playback, 6 carrier pose, 7 target No. 9,
/// 8 burst the next remaining chain.
static void _actor560800SendChainCommand(s16 commandId)
{
    _Actor560800CutsceneWork* work = _gActor560800CutsceneTask->work;
    ActorCommand              command;

    command.command = commandId;
    TASK_MESSAGE_DISPATCH_POINTER(work->chainGroup, ACTOR_COMMAND_MESSAGE_APPLY, &command, 0);
}

/// Sends a synchronous action command to the scene's carrier.
///
/// Requires a published scene and live carrier. Only the low halfword
/// of commandId is sent; the receiver ignores the uninitialized context word.
/// Commands: 0 relight, 1 hide, 2 first descent, 3 slow descent,
/// 4 accelerated rise, 5 second descent, 6 carry No. 9.
static void _actor560800SendCarrierCommand(s16 commandId)
{
    _Actor560800CutsceneWork* work = _gActor560800CutsceneTask->work;
    ActorCommand              command;

    command.command = commandId;
    TASK_MESSAGE_DISPATCH_POINTER(work->carrierModel, ACTOR_COMMAND_MESSAGE_APPLY, &command, 0);
}

/// Replaces a pending cue and restarts its handler sequence.
///
/// Requires a writable cue. Stores cueId's low halfword and clears the step;
/// 0 means no pending cue. The counter is left for the selected handler to reset.
/// Cue ids belong to the receiving actor or scene, rather than a shared namespace.
static inline void _actor560800PostCue(ActorCutsceneCue* cue, s16 cueId)
{
    cue->id   = cueId;
    cue->step = 0;
}

/// Posts the player's next scene cue, replacing any pending cue.
///
/// Requires the published scene. Stores cueId's halfword and restarts the
/// sequence step; its counter is left for the selected cue's handler to reset.
/// Cue 0 clears the request; other ids are interpreted by that actor's handler.
static void _actor560800PostPlayerCue(s16 cueId)
{
    _Actor560800CutsceneWork* work = _gActor560800CutsceneTask->work;

    _actor560800PostCue(&work->playerCue, cueId);
}

/// Blends the scene's player into a package clip over ten normal-rate frames.
///
/// Requires the published scene and loaded clip data; an absent player is a
/// no-op. Records the low halfword clip id for chaining, zero-extends it in the
/// request, clears the hold count and enables player world collision. The
/// player borrows the package's animation bank for subsequent playback.
static void _actor560800BlendScenePlayerAnimation(s16 animationId)
{
    _actor560800BlendPlayerAnimation(_gActor560800CutsceneTask, animationId, ACTOR_560800_ANIMATION_BLEND_FRAMES);
}

/// Blends Eve's non-root tracks into a scene clip over ten frames.
///
/// Requires the published scene, a live initialized body and a loaded clip
/// covering slots 1..slotCount - 1. Clears the hold count and records normal
/// chain speed in sixteenths of a frame; the slots keep their current rates.
static void _actor560800BlendEveAnimation(u16 animationId)
{
    _Actor560800CutsceneWork* work = _gActor560800CutsceneTask->work;

    _actor560800BlendCastAnimation(work->eve, animationId, ANIMATION_RATE_ONE);
}

/// Blends No. 9's non-root tracks into a scene clip over ten frames.
///
/// Requires the published scene, a live initialized body and a loaded clip
/// covering slots 1..slotCount - 1. Clears the hold count and records normal
/// chain speed in sixteenths of a frame; the slots keep their current rates.
static void _actor560800BlendNo9Animation(u16 animationId)
{
    _Actor560800CutsceneWork* work = _gActor560800CutsceneTask->work;

    _actor560800BlendCastAnimation(work->no9, animationId, ANIMATION_RATE_ONE);
}

/// Moves a scene texture strip into its saved VRAM region and uploads its replacement.
///
/// Copies 64 VRAM words by 256 rows from (960,0) to (640,256). Requires the
/// source strip and image chunk in resource slot 35 to be loaded. Palette
/// headers at Y=245..255 shift down five rows during upload. Ignores the GPU
/// time limit and upload result, then resets the row shift to zero.
static void _actor560800SwapSceneTextureStrip(void)
{
    enum {
        ACTOR_560800_TEXTURE_STRIP_SOURCE_X        = 0x3C0,
        ACTOR_560800_TEXTURE_STRIP_DESTINATION_X   = 0x280,
        ACTOR_560800_TEXTURE_STRIP_DESTINATION_Y   = 0x100,
        ACTOR_560800_TEXTURE_STRIP_WIDTH_WORDS     = 0x40,
        ACTOR_560800_TEXTURE_STRIP_HEIGHT_ROWS     = 0x100,
        ACTOR_560800_REPLACEMENT_IMAGE_SLOT        = 35,
        ACTOR_560800_REPLACEMENT_PALETTE_ROW_SHIFT = 5,
    };
    RECT sourceRect;

    sourceRect.x = ACTOR_560800_TEXTURE_STRIP_SOURCE_X;
    sourceRect.y = 0;
    sourceRect.w = ACTOR_560800_TEXTURE_STRIP_WIDTH_WORDS;
    sourceRect.h = ACTOR_560800_TEXTURE_STRIP_HEIGHT_ROWS;
    MoveImage(&sourceRect, ACTOR_560800_TEXTURE_STRIP_DESTINATION_X, ACTOR_560800_TEXTURE_STRIP_DESTINATION_Y);
    gFsImageHeaderYShiftRows = ACTOR_560800_REPLACEMENT_PALETTE_ROW_SHIFT;
    fsUploadImageChunk(gFsResourceSlots[ACTOR_560800_REPLACEMENT_IMAGE_SLOT].data, true);
    gFsImageHeaderYShiftRows = 0;
}

/// Posts Kyle's next scene cue, replacing any pending cue.
///
/// Requires the published scene. Stores cueId's halfword and restarts the
/// sequence step; its counter is left for the selected cue's handler to reset.
/// Cue 0 clears the request; other ids are interpreted by that actor's handler.
static void _actor560800PostKyleCue(s16 cueId)
{
    _Actor560800CutsceneWork* work = _gActor560800CutsceneTask->work;

    _actor560800PostCue(&work->kyleCue, cueId);
}

/// Blends Kyle's non-root tracks into a scene clip over ten frames.
///
/// Requires the published scene, a live initialized body and a loaded clip
/// covering slots 1..slotCount - 1. Clears the hold count and records normal
/// chain speed in sixteenths of a frame; the slots keep their current rates.
static void _actor560800BlendKyleAnimation(u16 animationId)
{
    _Actor560800CutsceneWork* work = _gActor560800CutsceneTask->work;

    _actor560800BlendCastAnimation(work->kyle, animationId, ANIMATION_RATE_ONE);
}

/// Starts the scene sound script selected by cueId.
///
/// Requires cueId in 1..18 and the scene's sound bank to be loaded. The table
/// maps each cue to its complete script id; no bounds or availability check
/// occurs. Queues zero pan offset and no extra attenuation; ignores admission
/// failure.
static void _actor560800StartSoundCue(s32 cueId)
{
    sndEvtRequestScriptStart(_gActor560800SoundScriptsByCue[cueId], 0, 0);
}

/// Replaces Eve's texture and body while scene display presentation is held.
///
/// Holds presentation for three task updates, then copies a 64-word by 256-row
/// VRAM strip from (896,0) to (512,256). Requires image resource slot 36; uploads
/// it with an eight-row palette-header shift and ignores the GPU time limit.
/// Clears that shift, kills this display task, resumes game-loop presentation
/// and publishes Eve's replacement task. Spawn argument 1 makes the body place
/// itself on its next update. Requires live cutscene work throughout the swap.
static void _actor560800ReplaceEveBodyTask(Task* task)
{
    enum {
        ACTOR_560800_EVE_SWAP_HOLD           = 0,
        ACTOR_560800_EVE_SWAP_WAIT_FIRST     = 1,
        ACTOR_560800_EVE_SWAP_WAIT_SECOND    = 2,
        ACTOR_560800_EVE_SWAP_UPLOAD         = 3,
        ACTOR_560800_EVE_TEXTURE_SOURCE_X    = 0x380,
        ACTOR_560800_EVE_TEXTURE_DEST_X      = 0x200,
        ACTOR_560800_EVE_TEXTURE_DEST_Y      = 0x100,
        ACTOR_560800_EVE_TEXTURE_WIDTH_WORDS = 0x40,
        ACTOR_560800_EVE_TEXTURE_HEIGHT_ROWS = 0x100,
        ACTOR_560800_EVE_PALETTE_ROW_SHIFT   = 8,
        ACTOR_560800_EVE_IMAGE_SLOT          = 36,
    };
    RECT                      sourceRect;
    _Actor560800CutsceneWork* work;

    work = _gActor560800CutsceneTask->work;
    switch (task->state) {
        case ACTOR_560800_EVE_SWAP_HOLD:
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
            /* fallthrough */
        case ACTOR_560800_EVE_SWAP_WAIT_FIRST:
        case ACTOR_560800_EVE_SWAP_WAIT_SECOND:
            task->state++;
            return;
        case ACTOR_560800_EVE_SWAP_UPLOAD:
            sourceRect.x = ACTOR_560800_EVE_TEXTURE_SOURCE_X;
            sourceRect.y = 0;
            sourceRect.w = ACTOR_560800_EVE_TEXTURE_WIDTH_WORDS;
            sourceRect.h = ACTOR_560800_EVE_TEXTURE_HEIGHT_ROWS;
            MoveImage(&sourceRect, ACTOR_560800_EVE_TEXTURE_DEST_X, ACTOR_560800_EVE_TEXTURE_DEST_Y);
            gFsImageHeaderYShiftRows = ACTOR_560800_EVE_PALETTE_ROW_SHIFT;
            fsUploadImageChunk(gFsResourceSlots[ACTOR_560800_EVE_IMAGE_SLOT].data, true);
            gFsImageHeaderYShiftRows = 0;
            taskKill(task);
            displayResumeGameLoop();
            work->eve = taskSpawnFromTableOnDefaultList(gActor560800CutsceneTaskDescriptors, ACTOR_560800_TASK_EVE_REPLACEMENT, 1, _gActor560800CutsceneTask);
            break;
    }
}

/// Posts the scene's next cue, replacing any pending request.
///
/// Requires the published cutscene. Cue 1 requests Eve's model replacement;
/// 0 clears the request. Restarts the sequence step without clearing its counter.
static void _actor560800PostSceneCue(s16 cueId)
{
    _Actor560800CutsceneWork* work = _gActor560800CutsceneTask->work;

    _actor560800PostCue(&work->sceneCue, cueId);
}

/// Posts one cut id to all four actors and restarts each cue sequence.
///
/// Requires the published cutscene. The event script posts cuts 1..33; each actor
/// handles its own subset. Cue 0 clears every request. Leaves the four counters
/// unchanged and does not post a scene cue.
static void _actor560800PostCastCue(s16 cueId)
{
    _Actor560800CutsceneWork* work = _gActor560800CutsceneTask->work;

    _actor560800PostCue(&work->playerCue, cueId);
    _actor560800PostCue(&work->eveCue, cueId);
    _actor560800PostCue(&work->no9Cue, cueId);
    _actor560800PostCue(&work->kyleCue, cueId);
}

/// Applies the scene's 50-HP shot without killing the player.
///
/// Requires the published cutscene and live player status. Removes the player's
/// equipment/effect tasks, clamps the remaining HP to at least 1, and records that
/// the shot was applied. A repeated call applies the loss again; the skip path
/// checks the latch before performing this operation.
static void _actor560800ApplyShotDamage(void)
{
    _Actor560800CutsceneWork* work         = _gActor560800CutsceneTask->work;
    PlayerStatus*             playerStatus = &gPlayerStatus;
    s16                       remainingHp;

    playerActorRemoveEquipment();

    if (playerStatus->hp < ACTOR_560800_SHOT_DAMAGE_HP + 1) {
        remainingHp = 1;
    } else {
        remainingHp = (u16)playerStatus->hp - ACTOR_560800_SHOT_DAMAGE_HP;
    }
    do {
        playerStatus->hp        = remainingHp;
        work->shotDamageApplied = 1;
    } while (0);
}

/// Stops cutscene activity and preserves the scripted shot's HP loss on skip.
///
/// Requires the published cutscene and selected scene/audio session. Clears all
/// four actor cues and applies the nonfatal shot only if it has not happened.
/// Cancels room effects and scene CD work, then blanks the display; the surrounding
/// skip script owns fades, texture restoration and scene teardown.
static void _actor560800SkipScene(void)
{
    _Actor560800CutsceneWork* work = _gActor560800CutsceneTask->work;
    s16                       remainingHp;

    work->playerCue.id = 0;
    work->kyleCue.id   = 0;
    work->no9Cue.id    = 0;
    work->eveCue.id    = 0;
    if (work->shotDamageApplied == 0) {
        PlayerStatus*             playerStatus = &gPlayerStatus;
        _Actor560800CutsceneWork* damageWork   = _gActor560800CutsceneTask->work;

        playerActorRemoveEquipment();
        if (playerStatus->hp < ACTOR_560800_SHOT_DAMAGE_HP + 1) {
            remainingHp = 1;
        } else {
            remainingHp = (u16)playerStatus->hp - ACTOR_560800_SHOT_DAMAGE_HP;
        }
        do {
            playerStatus->hp              = remainingHp;
            damageWork->shotDamageApplied = 1;
        } while (0);
    }
    roomEffectRequestCancelAll();
    cdCmdCancelScene();
    SetDispMask(0);
}

/// Stages deferred audio start for the script's selected scene session.
///
/// The script calls this after each of its three scene selections. Requires the
/// selected scene's prepared buffers to remain live through deferred CD dispatch;
/// with no selected slot the resident API leaves the previous request intact.
static void _actor560800StageSceneAudioStart(void)
{
    cdCmdStageSceneAudioStart();
}

/// Records a phase's start frame and enqueues selected scene/audio playback.
///
/// phaseId 1..3 selects its matching timestamp, later used to measure elapsed
/// frames. Other values leave timestamps unchanged but still enqueue playback.
/// Requires the prepared scene session and free CD queue capacity.
static void _actor560800StartScenePhasePlayback(s32 phaseId)
{
    if (phaseId == 1) {
        _gActor560800FirstPhaseStartTick = gDisplayState.frameCount;
    } else if (phaseId == 2) {
        _gActor560800SecondPhaseStartTick = gDisplayState.frameCount;
    } else if (phaseId == 3) {
        _gActor560800ThirdPhaseStartTick = gDisplayState.frameCount;
    }
    cdCmdEnqueueScenePlayback();
}

/// Holds display presentation until the queued scene playback work finishes.
///
/// Starts the display task that waits for the CD queue to become idle and then
/// resumes the game loop. Holds frame flipping and queues the current camera
/// and packets after handing control over. Spawn failure is unchecked.
static void _actor560800HoldDisplayUntilScenePlaybackEnds(void)
{
    displaySpawnTaskFromTable(gActor560800CutsceneTaskDescriptors, ACTOR_560800_TASK_AWAIT_PLAYBACK, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
    viewQueueCurrentCameraAndPackets();
}

/// Resumes the game display loop after the scene's queued CD work has finished.
///
/// While the queue remains busy this task stays alive; on idle it kills itself
/// and restores ordinary display scheduling. Installed at task-table index 13.
static void _actor560800AwaitScenePlaybackTask(Task* task)
{
    if (cdCmdIsIdle()) {
        taskKill(task);
        displayResumeGameLoop();
    }
}

/// Selects the scene's second loaded CAP dialogue resource.
///
/// Uses zero-based data-resource ordinal 1, excluding image and empty slots.
/// Clears the current file before selection and restores texture origin (384, 0)
/// in VRAM pixels. Requires loaded, writable CAP storage and its texture; the
/// resource remains borrowed for subsequent dialogue playback.
static void _actor560800SelectSecondCapFile(void)
{
    gCapFile = NULL;
    capSelectLoadedFile(1);
    capSetTexturePage(ACTOR_560800_CAP_TEXTURE_VRAM_X, ACTOR_560800_CAP_TEXTURE_VRAM_Y);
}

/// Selects the scene's third loaded CAP dialogue resource.
///
/// Uses zero-based data-resource ordinal 2, excluding image and empty slots.
/// Clears the current file before selection and restores texture origin (384, 0)
/// in VRAM pixels. Requires loaded, writable CAP storage and its texture; the
/// resource remains borrowed for subsequent dialogue playback.
static void _actor560800SelectThirdCapFile(void)
{
    gCapFile = NULL;
    capSelectLoadedFile(2);
    capSetTexturePage(ACTOR_560800_CAP_TEXTURE_VRAM_X, ACTOR_560800_CAP_TEXTURE_VRAM_Y);
}

/// Immediately kills the scene task-table's bodyless placeholder task.
///
/// Installed at index 1; owns no work and ignores both spawn arguments.
static void _actor560800DiscardTask(Task* task)
{
    taskKill(task);
}

/// Sways the root and bends five chain joints toward the group's target.
///
/// Requires a live seven-coordinate chain and parent group with a valid target
/// translation. Reserves one zeroed scratch-stack block and releases it before
/// returning. Odd-numbered chains turn two angle units per update, even ones one;
/// angles use 4096 units per turn. Copies all seven stored rotations into scratch
/// and back, but rebuilds only parts 0..5, using part 6's translation as the tip.
/// Proximity in Y/Z starts a three-phase root retraction, in game-coordinate units.
static void _actor560800BendChainTowardTarget(Task* task)
{
    enum {
        ACTOR_560800_CHAIN_BEND_END_PART = 6,
        ACTOR_560800_CHAIN_DIP_IDLE      = 0,
        ACTOR_560800_CHAIN_DIP_RETRACT   = 1,
        ACTOR_560800_CHAIN_DIP_RETURN    = 2,
    };
    _Actor560800ChainScratch*   savedScratch;
    _Actor560800PropWork*       work;
    _Actor560800ChainScratch*   scratch;
    _Actor560800ChainGroupWork* group;
    s16                         partIndex;
    s16                         angleStep;
    s32                         targetPitch;

/// Multiplies a chain joint's Q12 basis by the rotation already loaded in the GTE.
///
/// Requires stable halfword-aligned MATRIX pointers result and joint, each used
/// three times. They may alias: each column is read before that column is stored.
/// The caller must load the intended GTE rotation first. Uses signed IR values
/// and GTE saturation for all three columns; translation and composition stamps
/// are untouched. Overwrites GTE vector/arithmetic registers and CPU scratch
/// registers through SDK macros. Invoke within a braced statement body.
#define ACTOR_560800_ROTATE_CHAIN_BASIS(result, joint) \
    {                                                  \
        gte_ldclmv((joint));                           \
        gte_rtir();                                    \
        gte_stclmv((result));                          \
        gte_ldclmv(&(joint)->m[0][1]);                 \
        gte_rtir();                                    \
        gte_stclmv(&(result)->m[0][1]);                \
        gte_ldclmv(&(joint)->m[0][2]);                 \
        gte_rtir();                                    \
        gte_stclmv(&(result)->m[0][2]);                \
    }
    savedScratch = SCRATCH_STACK_CURSOR(_Actor560800ChainScratch);
    work         = task->work;
    scratch = SCRATCH_STACK_CURSOR(_Actor560800ChainScratch) = savedScratch - 1;
    group                                                    = work->parent->work;
    memFillBytes(scratch, 0, sizeof(_Actor560800ChainScratch));
    memCopyBytes(work->rot, scratch->rot, sizeof(scratch->rot));
    if (work->chainNumber & 1) {
        angleStep = 2;
        switch (((_gActor560800ChainSwayPhase + work->swayPhase) * 2) & 0x300) {
            case 0x0:
            case 0x300:
                scratch->rot[0].vx += 4;
                scratch->rot[0].vx %= ACTOR_TRANSFORM_ANGLE_TURN;
                break;
            case 0x100:
            case 0x200:
                scratch->rot[0].vx -= 4;
                if (scratch->rot[0].vx < 0) {
                    scratch->rot[0].vx += ACTOR_TRANSFORM_ANGLE_TURN;
                }
                break;
        }
    } else {
        angleStep = 1;
        switch (((_gActor560800ChainSwayPhase + work->swayPhase) * 2) & 0x700) {
            case 0x0:
            case 0x100:
            case 0x600:
            case 0x700:
                scratch->rot[0].vx += 2;
                scratch->rot[0].vx %= ACTOR_TRANSFORM_ANGLE_TURN;
                break;
            case 0x200:
            case 0x300:
            case 0x400:
            case 0x500:
                scratch->rot[0].vx -= 2;
                if (scratch->rot[0].vx < 0) {
                    scratch->rot[0].vx += ACTOR_TRANSFORM_ANGLE_TURN;
                }
                break;
        }
    }
    // Start at the root with its parent translation; sway the root independently.
    scratch->pos.vx = task->extra.tmd->coords->parent->coord.t[0] + task->extra.tmd->coords->coord.t[0];
    scratch->pos.vy = task->extra.tmd->coords->parent->coord.t[1] + task->extra.tmd->coords->coord.t[1];
    scratch->pos.vz = task->extra.tmd->coords->parent->coord.t[2] + task->extra.tmd->coords->coord.t[2];
    scratch->ang.vx = scratch->rot[0].vx;
    scratch->ang.vy = scratch->rot[0].vy;
    scratch->ang.vz = scratch->rot[0].vz;
    gfxRotMatrixY(&task->extra.tmd->coords->coord, scratch->rot[0].vy, GRAPHICS_ROTATION_REPLACE);
    gfxRotMatrixX(&task->extra.tmd->coords->coord, scratch->rot[0].vx + ACTOR_TRANSFORM_ANGLE_TURN / 4, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixZ(&task->extra.tmd->coords->coord, scratch->rot[0].vz, GRAPHICS_ROTATION_COMPOSE);
    scratch->link  = task->extra.tmd->coords->coord;
    scratch->chain = scratch->link;
    gte_SetRotMatrix(&scratch->chain);
    // Walk five bending joints, measuring the next joint before and after each turn.
    for (partIndex = 1; partIndex < ACTOR_560800_CHAIN_BEND_END_PART; partIndex++) {
        ACTOR_560800_ROTATE_CHAIN_BASIS(&scratch->link, &task->extra.tmd->coords[partIndex].coord);
        scratch->joint.vx = task->extra.tmd->coords[partIndex + 1].coord.t[0];
        scratch->joint.vy = task->extra.tmd->coords[partIndex + 1].coord.t[1];
        scratch->joint.vz = task->extra.tmd->coords[partIndex + 1].coord.t[2];
        gte_SetRotMatrix(&scratch->link);
        gte_ldv0(&scratch->joint);
        gte_rtv0();
        gte_stsv(&scratch->joint);
        scratch->joint.vx += scratch->pos.vx;
        scratch->joint.vy += scratch->pos.vy;
        scratch->joint.vz += scratch->pos.vz;
        scratch->aim.vx    = (scratch->ang.vx + scratch->rot[partIndex].vx) % ACTOR_TRANSFORM_ANGLE_TURN;
        scratch->aim.vy    = (scratch->ang.vy + scratch->rot[partIndex].vy) % ACTOR_TRANSFORM_ANGLE_TURN;
        scratch->aim.vz    = (scratch->ang.vz + scratch->rot[partIndex].vz) % ACTOR_TRANSFORM_ANGLE_TURN;
        if (scratch->rot[0].vy == 0) {
            targetPitch = ratan2(scratch->joint.vy - group->targetWorld.t[1], group->targetWorld.t[2] - scratch->joint.vz) % ACTOR_TRANSFORM_ANGLE_TURN;
            if (targetPitch < 0) {
                targetPitch += ACTOR_TRANSFORM_ANGLE_TURN;
            }
            scratch->aim.vx -= targetPitch;
            if (scratch->aim.vx < 0) {
                scratch->aim.vx += ACTOR_TRANSFORM_ANGLE_TURN;
            }
            if (scratch->aim.vx < (ACTOR_TRANSFORM_ANGLE_TURN / 2)) {
                scratch->rot[partIndex].vx += angleStep;
                scratch->rot[partIndex].vx %= ACTOR_TRANSFORM_ANGLE_TURN;
            } else {
                scratch->rot[partIndex].vx -= angleStep;
                if (scratch->rot[partIndex].vx < 0) {
                    scratch->rot[partIndex].vx += ACTOR_TRANSFORM_ANGLE_TURN;
                }
            }
        } else {
            targetPitch = ratan2(group->targetWorld.t[1] - scratch->joint.vy, group->targetWorld.t[2] - scratch->joint.vz) % ACTOR_TRANSFORM_ANGLE_TURN;
            if (targetPitch < 0) {
                targetPitch += ACTOR_TRANSFORM_ANGLE_TURN;
            }
            scratch->aim.vx -= targetPitch;
            if (scratch->aim.vx < 0) {
                scratch->aim.vx += ACTOR_TRANSFORM_ANGLE_TURN;
            }
            if (scratch->aim.vx > (ACTOR_TRANSFORM_ANGLE_TURN / 2)) {
                scratch->rot[partIndex].vx += angleStep;
                scratch->rot[partIndex].vx %= ACTOR_TRANSFORM_ANGLE_TURN;
            } else {
                scratch->rot[partIndex].vx -= angleStep;
                if (scratch->rot[partIndex].vx < 0) {
                    scratch->rot[partIndex].vx += ACTOR_TRANSFORM_ANGLE_TURN;
                }
            }
        }
        gfxRotMatrixY(&task->extra.tmd->coords[partIndex].coord, scratch->rot[partIndex].vy, GRAPHICS_ROTATION_REPLACE);
        gfxRotMatrixX(&task->extra.tmd->coords[partIndex].coord, scratch->rot[partIndex].vx, GRAPHICS_ROTATION_COMPOSE);
        gfxRotMatrixZ(&task->extra.tmd->coords[partIndex].coord, scratch->rot[partIndex].vz, GRAPHICS_ROTATION_COMPOSE);
        gte_SetRotMatrix(&scratch->chain);
        ACTOR_560800_ROTATE_CHAIN_BASIS(&scratch->chain, &task->extra.tmd->coords[partIndex].coord);
        scratch->joint.vx = task->extra.tmd->coords[partIndex + 1].coord.t[0];
        scratch->joint.vy = task->extra.tmd->coords[partIndex + 1].coord.t[1];
        scratch->joint.vz = task->extra.tmd->coords[partIndex + 1].coord.t[2];
        gte_SetRotMatrix(&scratch->chain);
        gte_ldv0(&scratch->joint);
        gte_rtv0();
        gte_stsv(&scratch->joint);
        scratch->ang.vx += scratch->rot[partIndex].vx;
        scratch->ang.vx %= ACTOR_TRANSFORM_ANGLE_TURN;
        scratch->ang.vy += scratch->rot[partIndex].vy;
        scratch->ang.vy %= ACTOR_TRANSFORM_ANGLE_TURN;
        scratch->ang.vz += scratch->rot[partIndex].vz;
        scratch->ang.vz %= ACTOR_TRANSFORM_ANGLE_TURN;
        scratch->pos.vx += scratch->joint.vx;
        scratch->pos.vy += scratch->joint.vy;
        scratch->pos.vz += scratch->joint.vz;
    }
    memCopyBytes(scratch->rot, work->rot, sizeof(scratch->rot));
    // Near the target, retract the root briefly and ease its offset back to zero.
    switch (work->dipStep) {
        case ACTOR_560800_CHAIN_DIP_IDLE:
            if (abs(scratch->pos.vy - group->targetWorld.t[1]) < 300) {
                if (abs(scratch->pos.vz - group->targetWorld.t[2]) < 200) {
                    work->dipStep = ACTOR_560800_CHAIN_DIP_RETRACT;
                }
            }
            break;
        case ACTOR_560800_CHAIN_DIP_RETRACT:
            work->offset.vy -= 20;
            if (work->offset.vy < -100) {
                work->dipStep = ACTOR_560800_CHAIN_DIP_RETURN;
            }
            break;
        case ACTOR_560800_CHAIN_DIP_RETURN:
            work->offset.vy += 2;
            if (work->offset.vy > 0) {
                work->offset.vy = 0;
                work->dipStep   = ACTOR_560800_CHAIN_DIP_IDLE;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor560800ChainScratch);
#undef ACTOR_560800_ROTATE_CHAIN_BASIS
}

/// Allocates playback and transform storage for a chain or its falling copy.
///
/// Requires a live seven-part model and a live TMD parent task in `spawnArg2`.
/// The root coordinate and teardown lifetime attach to that parent; model
/// lighting borrows matrices from the owned work until task teardown.
/// `spawnArg1` supplies a chain number (1..8) for a live chain; the falling copy
/// supplies a Y coordinate which is stored there but never used for playback.
/// Binds the seven-slot rig without starting a clip. Failure kills the task;
/// callers must stop accessing its work after an allocation failure.
static void _actor560800InitChainModel(Task* task)
{
    _Actor560800PropWork* allocatedWork;
    _Actor560800PropWork* work;
    TmdObject*            model;
    GfxCoord*             root;
    Task*                 parent;

    model         = task->extra.tmd;
    root          = model->coords;
    allocatedWork = memMalloc(sizeof(*allocatedWork), false);
    task->work    = allocatedWork;
    if (allocatedWork == NULL) {
        taskKill(task);
        return;
    }
    memFillBytes(allocatedWork, 0, sizeof(*allocatedWork));
    work            = task->work;
    parent          = task->spawnArg2.pointer;
    work->parent    = parent;
    root->parent    = parent->extra.tmd->coords;
    model->lightMtx = &work->light;
    model->colorMtx = &work->color;
    taskReparent(work->parent, task);
    // Keep all three draws so later scene effects see the same RNG sequence.
    RANDOM_LCG_ADVANCE();
    work->swayPhase = RANDOM_LCG_DRAW();
    RANDOM_LCG_ADVANCE();
    work->field_274 = RANDOM_LCG_DRAW();
    RANDOM_LCG_ADVANCE();
    work->pulseScale = RANDOM_LCG_DRAW() & 0x3FF;
    animationInitContext(&work->rig.anim, _gActor560800ChainAnimationSets, model, work->rig.poses, work->rig.slots);
    work->chainNumber = task->spawnArg1.value;
}

/// Copies the seven local part matrices from an intact chain to its falling copy.
///
/// Both tasks must own live seven-part models. Copies MATRIX values only;
/// coordinate parents, composition stamps and lighting bindings are retained.
static inline void _actor560800CopyChainPose(const Task* intactTask, Task* fallingTask)
{
    u16 partIndex;

    partIndex = 0;
    do {
        memCopyBytes(&intactTask->extra.tmd->coords[partIndex].coord,
                     &fallingTask->extra.tmd->coords[partIndex].coord, sizeof(fallingTask->extra.tmd->coords[partIndex].coord));
        partIndex++;
    } while (partIndex < ARRAY_SIZE(_gActor560800Model40064Skeleton));
}

/// Controls one intact gantry chain through bending, clip playback and detachment.
///
/// spawnArg1 selects chain/clip 1..8; spawnArg2 borrows the live chain-group
/// parent. Initialization owns prop work and binds the seven-part rig. Commands
/// select bending, reset/tick slots 1..6, or copy all seven local part matrices
/// to a falling task before delayed removal. Failed detachment returns to bending.
/// Initialization requires deferred teardown: it still advances the state after
/// allocation failure kills the task, without accessing the failed work allocation.
/// Surviving frames rebuild the parent-space root position and draw the visible
/// tip glow and staggered sprite effect. Hidden chains return without these updates.
static void _actor560800IntactChainTask(Task* task)
{
    enum {
        ACTOR_560800_CHAIN_INITIALIZE        = 0,
        ACTOR_560800_CHAIN_TICK_CLIP         = 3,
        ACTOR_560800_CHAIN_REMOVE_DELAY      = 5,
        ACTOR_560800_CHAIN_REMOVE_TICKS      = 2,
        ACTOR_560800_CHAIN_RESTRICTED_VIEW   = 22,
        ACTOR_560800_CHAIN_FALLING_TASK      = 3,
        ACTOR_560800_CHAIN_TIP_PART          = 6,
        ACTOR_560800_CHAIN_GLOW_RADIUS_SCALE = 256,
        ACTOR_560800_CHAIN_GLOW_PACKED_COLOR = 0x3C36,
        ACTOR_560800_CHAIN_EFFECT_PART       = 2,
        ACTOR_560800_CHAIN_EFFECT_TICK_SHIFT = 7,
        ACTOR_560800_CHAIN_EFFECT_TICK_MASK  = (1 << ACTOR_560800_CHAIN_EFFECT_TICK_SHIFT) - 1,
        ACTOR_560800_CHAIN_EFFECT_INDEX_MASK = 7,
        ACTOR_560800_CHAIN_EFFECT_SIZE       = 2048, // Perspective size numerator in spawnArg1's low twelve bits
    };
    _Actor560800PropWork* work;
    TmdObject*            model;
    GfxCoord*             rootCoord;
    Task*                 fallingTask;
    s32                   partIndex;
    u32                   effectTick;
    TmdObject*            drawModel;
    u32                   taskState;
    u16                   clipIndex;
    SVECTOR               unusedFrameSlot; // Untouched storage retained for the original stack frame

    model     = task->extra.tmd;
    taskState = task->state;
    work      = task->work;
    rootCoord = model->coords;
    drawModel = model;
    switch (taskState) {
        case ACTOR_560800_CHAIN_INITIALIZE:
            _actor560800InitChainModel(task);
            task->state++;
            return;
        case ACTOR_560800_CHAIN_BEND:
            // The view and placement state select which numbered chains remain visible.
            if (viewFindLogicalIndex(gGameSession->location.view) == ACTOR_560800_CHAIN_RESTRICTED_VIEW) {
                switch (work->chainNumber) {
                    case 1:
                    case 3:
                        drawModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                        return;
                    case 4 ... 0x7FFF:
                        break;
                    default:
                        _actor560800BendChainTowardTarget(task);
                        goto done;
                }
            }
            if (work->chainNumber < 8) {
                if (work->chainNumber >= 5) {
                    drawModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    return;
                }
            }
            _actor560800BendChainTowardTarget(task);
            break;
        case ACTOR_560800_CHAIN_START_CLIP: {
            _Actor560800PropWork* clipWork;

            if (work->chainNumber < 4) {
                if (work->chainNumber >= 2) {
                    drawModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    return;
                }
            }
            partIndex = 1;
            clipIndex = work->chainNumber;
            clipWork  = task->work;
            do {
                clipWork->rig.slots[partIndex & 0xFFFF].rate = ANIMATION_RATE_ONE;
                animationResetSlot(&clipWork->rig.anim, partIndex & 0xFFFF, clipIndex);
                partIndex++;
            } while ((u32)(partIndex & 0xFFFF) < ARRAY_SIZE(clipWork->rig.slots));
            task->state++;
            break;
        }
        case ACTOR_560800_CHAIN_TICK_CLIP: {
            _Actor560800PropWork* clipWork;

            clipWork  = task->work;
            partIndex = 1;
            do {
                animationTickSlot(&clipWork->rig.anim, partIndex & 0xFFFF);
                partIndex++;
            } while ((u32)(partIndex & 0xFFFF) < ARRAY_SIZE(clipWork->rig.slots));
            // Retain the scan even though its result does not advance the task state.
            for (partIndex = 1; (u32)(partIndex & 0xFFFF) < ARRAY_SIZE(clipWork->rig.slots); partIndex++) {
                if (!(clipWork->rig.slots[partIndex & 0xFFFF].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
                    break;
                }
            }
            break;
        }
        case ACTOR_560800_CHAIN_BREAK:
            // Transfer the local pose before the falling copy's first update.
            fallingTask = taskSpawnFromTable(_gActor560800PropTaskDescriptors, ACTOR_560800_CHAIN_FALLING_TASK,
                                             (s32)_gActor560800CarrierTask->extra.tmd->coords->coord.t[1],
                                             task->spawnArg2.pointer);
            if (fallingTask == NULL) {
                task->state = ACTOR_560800_CHAIN_BEND;
                return;
            }
            _actor560800CopyChainPose(task, fallingTask);
            task->killCountdown = 0;
            task->state++;
            break;
        case ACTOR_560800_CHAIN_REMOVE_DELAY:
            if (++task->killCountdown >= ACTOR_560800_CHAIN_REMOVE_TICKS) {
                taskKill(task);
                return;
            }
            break;
    }
done:
    rootCoord->coord.t[0]   = work->position.vx + work->offset.vx;
    rootCoord->coord.t[1]   = work->position.vy + work->offset.vy;
    rootCoord->coord.t[2]   = work->position.vz + work->offset.vz;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (!(drawModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        shelterB1PodServiceGantryDrawChainGlow(&task->extra.tmd->coords[ACTOR_560800_CHAIN_TIP_PART], work->chainNumber, ACTOR_560800_CHAIN_GLOW_RADIUS_SCALE, ACTOR_560800_CHAIN_GLOW_PACKED_COLOR);
        if (viewFindLogicalIndex(gGameSession->location.view) != ACTOR_560800_CHAIN_RESTRICTED_VIEW) {
            effectTick = _gActor560800ChainSwayPhase + 1;
            // The retained three-bit selector staggers chains 1..7; it never selects chain 8.
            if (!(effectTick & ACTOR_560800_CHAIN_EFFECT_TICK_MASK) && ((effectTick >> ACTOR_560800_CHAIN_EFFECT_TICK_SHIFT) & ACTOR_560800_CHAIN_EFFECT_INDEX_MASK) == work->chainNumber) {
                effectSpawn(EFFECT_SHELTER_B1_GANTRY_RISING_SPRITE, &task->extra.tmd->coords[ACTOR_560800_CHAIN_EFFECT_PART], ACTOR_560800_CHAIN_EFFECT_SIZE, NULL);
            }
        }
    }
}

/// Animates a detached chain copy falling away from the moving carrier.
///
/// Starts at state 0 with the seven model-part matrices already copied from
/// the intact chain. Requires a live carrier and chain-group parent throughout.
/// `spawnArg1` is the carrier's Y when the copy spawned; the copy owns prop work
/// and updates parts 3..5 in 4096-unit-per-turn angles. Fall speed and distance
/// narrow to signed halfwords each update. Removes the copy once its local
/// root Y exceeds 10000; lighting follows the composed position.
static void _actor560800FallingChainTask(Task* task)
{
    enum {
        ACTOR_560800_FALL_INITIALIZE          = 0,
        ACTOR_560800_FALL_ADVANCE             = 1,
        ACTOR_560800_CHAIN_SWING_X_INCREASING = 1,
        ACTOR_560800_CHAIN_SWING_Z_INCREASING = 2,
        ACTOR_560800_CHAIN_SWING_LIMIT        = 0x154,
        ACTOR_560800_FALL_END_Y               = 10000,
    };
    _Actor560800PropWork* work;
    GfxCoord*             root;
    TmdObject*            model;
    VECTOR                lightingPosition;
    s32                   partIndex;
    s32                   maskedPartIndex;
    u16                   nextSpeed;
    u16                   nextDistance;

    work = task->work;
    root = task->extra.tmd->coords;
    switch (task->state) {
        case ACTOR_560800_FALL_INITIALIZE:
            _actor560800InitChainModel(task);
            root->parent           = _gActor560800CarrierTask->extra.tmd->coords;
            task->extra.tmd->flags = 0;
            work                   = task->work;
            partIndex              = 1;
            do {
                work->swingDir[partIndex & 0xFFFF] = 0;
                partIndex                         += 1;
            } while ((u32)(partIndex & 0xFFFF) < 6U);
            work->fallStartY   = root->coord.t[1];
            root->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state++;
            break;
        case ACTOR_560800_FALL_ADVANCE:
            // Direction bits stay clear: the retained updates keep turning X/Z down.
            partIndex = 3;
            do {
                if (work->swingDir[partIndex & 0xFFFF] & ACTOR_560800_CHAIN_SWING_X_INCREASING) {
                    work->swing[partIndex & 0xFFFF].vx += 20;
                    if (work->swing[partIndex & 0xFFFF].vx >= (ACTOR_560800_CHAIN_SWING_LIMIT + 1)) {
                        work->swingDir[partIndex & 0xFFFF] |= ACTOR_560800_CHAIN_SWING_X_INCREASING;
                    }
                } else {
                    work->swing[partIndex & 0xFFFF].vx -= 20;
                    if (work->swing[partIndex & 0xFFFF].vx < -ACTOR_560800_CHAIN_SWING_LIMIT) {
                        work->swingDir[partIndex & 0xFFFF] &= (u16)~ACTOR_560800_CHAIN_SWING_X_INCREASING;
                    }
                }
                if (work->swingDir[partIndex & 0xFFFF] & ACTOR_560800_CHAIN_SWING_Z_INCREASING) {
                    work->swing[partIndex & 0xFFFF].vz += 20;
                    if (work->swing[partIndex & 0xFFFF].vz >= (ACTOR_560800_CHAIN_SWING_LIMIT + 1)) {
                        work->swingDir[partIndex & 0xFFFF] |= ACTOR_560800_CHAIN_SWING_Z_INCREASING;
                    }
                } else {
                    work->swing[partIndex & 0xFFFF].vz -= 20;
                    if (work->swing[partIndex & 0xFFFF].vz < -ACTOR_560800_CHAIN_SWING_LIMIT) {
                        work->swingDir[partIndex & 0xFFFF] &= (u16)~ACTOR_560800_CHAIN_SWING_Z_INCREASING;
                    }
                }
                maskedPartIndex = partIndex & 0xFFFF;
                gfxRotMatrixY(&task->extra.tmd->coords[maskedPartIndex].coord, work->rot[maskedPartIndex].vy, GRAPHICS_ROTATION_REPLACE);
                gfxRotMatrixX(&task->extra.tmd->coords[maskedPartIndex].coord,
                              work->rot[maskedPartIndex].vx + work->swing[maskedPartIndex].vx, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixZ(&task->extra.tmd->coords[maskedPartIndex].coord,
                              work->rot[maskedPartIndex].vz + work->swing[maskedPartIndex].vz, GRAPHICS_ROTATION_COMPOSE);
                partIndex += 1;
            } while ((u32)(partIndex & 0xFFFF) < 6U);
            // Compensate for the moving carrier parent so the detached chain falls independently.
            nextSpeed          = work->speed + 4;
            nextDistance       = work->fallDistance + nextSpeed;
            work->fallDistance = nextDistance;
            work->speed        = nextSpeed;
            root->coord.t[1]   = work->fallStartY + task->spawnArg1.value -
                               _gActor560800CarrierTask->extra.tmd->coords->coord.t[1] +
                               (s16)nextDistance;
            root->composeStamp = GRAPHICS_COORD_DIRTY;
            if (root->coord.t[1] > ACTOR_560800_FALL_END_Y) {
                taskKill(task);
                return;
            }
            break;
    }
    model               = task->extra.tmd;
    lightingPosition.vx = model->coords->workm.t[0];
    lightingPosition.vy = task->extra.tmd->coords->workm.t[1];
    lightingPosition.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(model, &lightingPosition, 0, 3);
}

/// Resets the six non-root chain rotations and their stored bends.
///
/// The low sixteen bits of chainIndex must select a live group chain (0..7)
/// whose seven coordinates correspond to chainWork->rot. Leaves the root, joint
/// translations and stored root rotation intact; the caller dirties composition.
static inline void _actor560800StraightenChainParts(const _Actor560800ChainGroupWork* group, s32 chainIndex, _Actor560800PropWork* chainWork)
{
    u16 partIndex;

    for (partIndex = 1; (u32)partIndex < ARRAY_SIZE(chainWork->rot); partIndex++) {
        gfxRotMatrixY(&group->chains[chainIndex & 0xFFFF]->extra.tmd->coords[partIndex].coord, 0, GRAPHICS_ROTATION_REPLACE);
        gfxRotMatrixX(&group->chains[chainIndex & 0xFFFF]->extra.tmd->coords[partIndex].coord, 0, GRAPHICS_ROTATION_COMPOSE);
        gfxRotMatrixZ(&group->chains[chainIndex & 0xFFFF]->extra.tmd->coords[partIndex].coord, 0, GRAPHICS_ROTATION_COMPOSE);
        chainWork->rot[partIndex].vx = 0;
        chainWork->rot[partIndex].vy = 0;
        chainWork->rot[partIndex].vz = 0;
    }
}

/// Places the live chains in the group's selected arrangement.
///
/// Requires initialized group work with placeMode 0..4 and seven coordinates
/// in each referenced chain.
/// Consumes only placement's position, in game-coordinate units; angles are
/// ignored and the borrowed payload is not retained. Modes 0..2 place the group
/// root when any chain remains, and table offsets, resetting bends only when
/// requested. Mode 3 detaches
/// each chain into view space and retains the mode. Mode 4 removes chains 5..8,
/// parents the rest to the live carrier and resets their bends. Modes 0..2 and 4
/// clear the placement mode; modes 0..2 also clear the bend-reset request.
/// Ignores messageId/unusedArg; dispatch has no usable return value.
static void _actor560800PlaceChainGroup(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArg)
{
    enum {
        ACTOR_560800_CHAIN_PLACE_FIRST    = 0,
        ACTOR_560800_CHAIN_PLACE_SECOND   = 1,
        ACTOR_560800_CHAIN_PLACE_THIRD    = 2,
        ACTOR_560800_CHAIN_PLACE_DETACHED = 3,
        ACTOR_560800_CHAIN_PLACE_CARRIER  = 4,
        ACTOR_560800_CARRIER_CHAIN_COUNT  = 4,
    };
    _Actor560800ChainGroupWork* work;
    u16                         pulseGrowing;
    const ActorTransform*       poses;
    _Actor560800PropWork*       chainWork;
    GfxCoord*                   root;
    s32                         chainIndex;

    work         = task->work;
    pulseGrowing = 0;
    switch (work->placeMode) {
        case ACTOR_560800_CHAIN_PLACE_FIRST:
            poses = _gActor560800ChainFirstPlacements;
            break;
        case ACTOR_560800_CHAIN_PLACE_SECOND:
            poses = _gActor560800ChainSecondPlacements;
            break;
        case ACTOR_560800_CHAIN_PLACE_THIRD:
            poses        = _gActor560800ChainThirdPlacements;
            pulseGrowing = 1;
            break;
        // Detached chains keep their arrangement independent of the group root.
        case ACTOR_560800_CHAIN_PLACE_DETACHED:
            poses      = _gActor560800ChainDetachedPlacements;
            chainIndex = 0;
            do {
                if (work->chains[chainIndex & 0xFFFF] != NULL) {
                    root = work->chains[chainIndex & 0xFFFF]->extra.tmd->coords;
                    gfxSetRotIdentity(&root->coord);
                    root->parent           = &gGfxViewCoord;
                    chainWork              = work->chains[chainIndex & 0xFFFF]->work;
                    chainWork->position.vx = placement->pos.vx + poses->pos.vx;
                    chainWork->position.vy = placement->pos.vy + poses->pos.vy;
                    chainWork->position.vz = placement->pos.vz + poses->pos.vz;
                    chainWork->offset.vx   = 0;
                    chainWork->offset.vy   = 0;
                    chainWork->offset.vz   = 0;
                }
                chainIndex++;
                poses++;
            } while ((u32)(chainIndex & 0xFFFF) < ARRAY_SIZE(work->chains));
            return;
        // Keep the first four chains and attach their pose to the carrier.
        case ACTOR_560800_CHAIN_PLACE_CARRIER:
            chainIndex = 0;
            do {
                if ((chainIndex & 0xFFFF) >= (u32)ACTOR_560800_CARRIER_CHAIN_COUNT) {
                    taskKill(work->chains[chainIndex & 0xFFFF]);
                    work->chains[chainIndex & 0xFFFF] = NULL;
                }
                chainIndex++;
            } while ((u32)(chainIndex & 0xFFFF) < ARRAY_SIZE(work->chains));
            poses      = _gActor560800ChainCarrierPlacements;
            chainIndex = 0;
            do {
                if (work->chains[chainIndex & 0xFFFF] != NULL) {
                    chainWork              = work->chains[chainIndex & 0xFFFF]->work;
                    root                   = work->chains[chainIndex & 0xFFFF]->extra.tmd->coords;
                    root->parent           = _gActor560800CarrierTask->extra.tmd->coords;
                    chainWork->position.vx = poses->pos.vx;
                    chainWork->position.vy = poses->pos.vy;
                    chainWork->position.vz = poses->pos.vz;
                    gfxRotMatrixY(&root->coord, poses->rot.vy, GRAPHICS_ROTATION_REPLACE);
                    gfxRotMatrixX(&root->coord, poses->rot.vx + ACTOR_TRANSFORM_ANGLE_TURN / 4, GRAPHICS_ROTATION_COMPOSE);
                    gfxRotMatrixZ(&root->coord, poses->rot.vz, GRAPHICS_ROTATION_COMPOSE);
                    chainWork->rot[0].vx    = poses->rot.vx;
                    chainWork->rot[0].vy    = poses->rot.vy;
                    chainWork->rot[0].vz    = poses->rot.vz;
                    chainWork->pulseGrowing = pulseGrowing;
                    _actor560800StraightenChainParts(work, chainIndex, chainWork);
                    root->composeStamp = GRAPHICS_COORD_DIRTY;
                }
                chainIndex++;
                poses++;
            } while ((u32)(chainIndex & 0xFFFF) < ARRAY_SIZE(work->chains));
            work->placeMode             = ACTOR_560800_CHAIN_PLACE_FIRST;
            _gActor560800ChainSwayPhase = ACTOR_560800_CHAIN_PHASE_START;
            gActor560800ChainGlowTick   = ACTOR_560800_CHAIN_PHASE_START;
            return;
    }
    // Ordinary arrangements use the group root and optionally straighten bends.
    chainIndex = 0;
    do {
        if (work->chains[chainIndex & 0xFFFF] != NULL) {
            chainWork                                  = work->chains[chainIndex & 0xFFFF]->work;
            root                                       = work->chains[chainIndex & 0xFFFF]->extra.tmd->coords;
            task->extra.coordBody->coord->coord.t[0]   = placement->pos.vx;
            task->extra.coordBody->coord->coord.t[1]   = placement->pos.vy;
            task->extra.coordBody->coord->coord.t[2]   = placement->pos.vz;
            task->extra.coordBody->coord->composeStamp = GRAPHICS_COORD_DIRTY;
            chainWork->position.vx                     = poses->pos.vx;
            chainWork->position.vy                     = poses->pos.vy;
            chainWork->position.vz                     = poses->pos.vz;
            chainWork->pulseGrowing                    = pulseGrowing;
            if (work->placeResetsBend != 0) {
                gfxRotMatrixY(&root->coord, poses->rot.vy, GRAPHICS_ROTATION_REPLACE);
                gfxRotMatrixX(&root->coord, poses->rot.vx + ACTOR_TRANSFORM_ANGLE_TURN / 4, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixZ(&root->coord, poses->rot.vz, GRAPHICS_ROTATION_COMPOSE);
                chainWork->rot[0].vx = poses->rot.vx;
                chainWork->rot[0].vy = poses->rot.vy;
                chainWork->rot[0].vz = poses->rot.vz;
                _actor560800StraightenChainParts(work, chainIndex, chainWork);
            }
            root->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        chainIndex++;
        poses++;
    } while ((u32)(chainIndex & 0xFFFF) < ARRAY_SIZE(work->chains));
    work->placeResetsBend = 0;
    work->placeMode       = ACTOR_560800_CHAIN_PLACE_FIRST;
}

/// Applies a cutscene command to the eight-chain group.
///
/// Reads only command->command: 0 relight surviving chains, 1 target Eve in the
/// second pose, 2 extend, 3 third pose, 4 first pose, 5 detached clip playback,
/// 6 carrier pose, 7 carrier pose targeting No. 9, 8 break the next chain.
/// Requires initialized group work; commands 5/6 require all eight chains live.
/// Placement commands select the next placement's arrangement, without placing
/// immediately. Borrows the payload synchronously; ignores context tags,
/// messageId and unusedArg. Dispatch callers must ignore its return register.
static void _actor560800ApplyChainGroupCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    enum {
        ACTOR_560800_CHAIN_PLACE_FIRST    = 0,
        ACTOR_560800_CHAIN_PLACE_SECOND   = 1,
        ACTOR_560800_CHAIN_PLACE_THIRD    = 2,
        ACTOR_560800_CHAIN_PLACE_DETACHED = 3,
        ACTOR_560800_CHAIN_PLACE_CARRIER  = 4,
    };
    _Actor560800ChainGroupWork* work;
    Task*                       chainTask;
    TmdObject*                  model;
    VECTOR                      lightingPosition;
    u16                         chainIndex;

    work = task->work;
    switch (command->command) {
        case ACTOR_560800_CHAIN_COMMAND_RELIGHT:
            chainIndex = 0;
            do {
                chainTask = work->chains[chainIndex];
                if (chainTask != NULL) {
                    model               = chainTask->extra.tmd;
                    lightingPosition.vx = model->coords->workm.t[0];
                    lightingPosition.vy = chainTask->extra.tmd->coords->workm.t[1];
                    lightingPosition.vz = chainTask->extra.tmd->coords->workm.t[2];
                    worldCoordSetModelLighting(model, &lightingPosition, 0, ACTOR_560800_MODEL_LIGHT_COUNT);
                }
                chainIndex += 1;
            } while (chainIndex < ARRAY_SIZE(work->chains));
            break;
        case ACTOR_560800_CHAIN_COMMAND_TARGET_EVE:
            work->placeMode       = ACTOR_560800_CHAIN_PLACE_SECOND;
            work->placeResetsBend = 1;
            work->targetEntryId   = ACTOR_560800_CHAIN_TARGET_EVE;
            break;
        case ACTOR_560800_CHAIN_COMMAND_EXTEND:
            task->state    = ACTOR_560800_CHAIN_GROUP_EXTEND;
            work->field_44 = 0;
            break;
        case ACTOR_560800_CHAIN_COMMAND_THIRD_POSE:
            task->state     = ACTOR_560800_CHAIN_GROUP_FOLLOW;
            work->placeMode = ACTOR_560800_CHAIN_PLACE_THIRD;
            break;
        case ACTOR_560800_CHAIN_COMMAND_FIRST_POSE:
            work->placeMode       = ACTOR_560800_CHAIN_PLACE_FIRST;
            work->placeResetsBend = 1;
            break;
        case ACTOR_560800_CHAIN_COMMAND_DETACHED_CLIP:
            task->state     = ACTOR_560800_CHAIN_GROUP_FOLLOW;
            work->placeMode = ACTOR_560800_CHAIN_PLACE_DETACHED;
            chainIndex      = 0;
            do {
                work->chains[chainIndex]->state = ACTOR_560800_CHAIN_START_CLIP;
                chainIndex                     += 1;
            } while (chainIndex < ARRAY_SIZE(work->chains));
            break;
        case ACTOR_560800_CHAIN_COMMAND_CARRIER_POSE:
            task->state     = ACTOR_560800_CHAIN_GROUP_FOLLOW;
            work->placeMode = ACTOR_560800_CHAIN_PLACE_CARRIER;
            chainIndex      = 0;
            do {
                work->chains[chainIndex]->state = ACTOR_560800_CHAIN_BEND;
                chainIndex                     += 1;
            } while (chainIndex < ARRAY_SIZE(work->chains));
            break;
        case ACTOR_560800_CHAIN_COMMAND_TARGET_NO9:
            task->state         = ACTOR_560800_CHAIN_GROUP_FOLLOW;
            work->placeMode     = ACTOR_560800_CHAIN_PLACE_CARRIER;
            work->targetEntryId = ACTOR_560800_CHAIN_TARGET_NO9;
            break;
        case ACTOR_560800_CHAIN_COMMAND_BREAK_NEXT:
            task->state = ACTOR_560800_CHAIN_GROUP_BREAK_NEXT;
            break;
    }
}

/// Spawns and controls the scene's eight chains and their shared body target.
///
/// spawnArg2 borrows the live cutscene task. Owns zeroed group work and child
/// chains until cutscene teardown; saves the random state before selecting zero.
/// State 1 follows the target, 2 raises each surviving chain to its first-pose
/// Y limit, and 3 bursts and releases the first surviving chain, then returns
/// to state 1. Individual spawn failures remain null entries.
/// Target Eve or No. 9 selects body part 9's world transform, lowered by 120
/// game units. Before any target command, the original stores uninitialized
/// translation; allocation failure also falls through after killing the task.
/// Both behaviors are retained.
static void _actor560800ChainGroupTask(Task* task)
{
    enum {
        ACTOR_560800_CHAIN_TARGET_PART        = 9,
        ACTOR_560800_CHAIN_TARGET_Y_OFFSET    = 120,
        ACTOR_560800_CHAIN_BREAK_PART         = 3,
        ACTOR_560800_CHAIN_BREAK_REPEAT_COUNT = 4,
        // Room spray: size 896/1152/1024, 2/3/2 updates per cell; default speed 64.
        ACTOR_560800_CHAIN_BREAK_STATIONARY_SPRAY = 0x10002380,
        ACTOR_560800_CHAIN_BREAK_GRAVITY_SPRAY    = 0x04003480,
        ACTOR_560800_CHAIN_BREAK_SCATTER_SPRAY    = 0x02002400,
        // Gantry drift sprite: size 768, 2 updates per cell, speed 32, palette 2.
        ACTOR_560800_CHAIN_BREAK_DRIFT_SPRITE = 0x02202300,
    };
    _Actor560800ChainGroupWork* work;
    _Actor560800ChainGroupWork* currentWork;
    _Actor560800ChainGroupWork* spawnWork;
    _Actor560800ChainGroupWork* extendingWork;
    _Actor560800PropWork*       chainWork;
    GfxCoord*                   root;
    GfxCoord*                   chainRoot;
    GfxCoord*                   burstCoord;
    GfxCoord*                   targetCoords;
    Task*                       chainTask;
    SVECTOR                     targetPosition;
    u16                         spawnIndex;
    u16                         extendIndex;
    u16                         burstIndex;
    s16                         chainIndex;

    work = task->work;
    switch (task->state) {
        case ACTOR_560800_CHAIN_GROUP_INITIALIZE:
            root        = task->extra.coordBody->coord;
            currentWork = memMalloc(sizeof(*currentWork), false);
            task->work  = currentWork;
            if (currentWork == NULL) {
                taskKill(task);
            } else {
                root->parent = &gGfxViewCoord;
                memFillBytes(task->work, 0, sizeof(*currentWork));
                spawnIndex          = 0;
                spawnWork           = currentWork;
                spawnWork->cutscene = task->spawnArg2.pointer;
                task->msgTable      = _gActor560800ChainGroupMessages;
                taskReparent(spawnWork->cutscene, task);
                do {
                    spawnWork->chains[spawnIndex] =
                        taskSpawnFromTable(_gActor560800PropTaskDescriptors, ACTOR_560800_PROP_TASK_CHAIN, spawnIndex + 1, task);
                    spawnIndex++;
                } while (spawnIndex < ARRAY_SIZE(spawnWork->chains));
                _gActor560800SavedRandomState = gRandomLcgState;
                gRandomLcgState               = 0;
            }
            task->state++;
            break;
        case ACTOR_560800_CHAIN_GROUP_FOLLOW:
            break;
        case ACTOR_560800_CHAIN_GROUP_EXTEND:
            extendingWork = work;
            extendIndex   = 0;
            do {
                chainTask = extendingWork->chains[extendIndex];
                if (chainTask != NULL) {
                    chainWork               = chainTask->work;
                    chainRoot               = chainTask->extra.tmd->coords;
                    chainWork->position.vy += _gActor560800ChainExtensionStepsY[extendIndex];
                    if (_gActor560800ChainFirstPlacements[extendIndex].pos.vy < chainWork->position.vy) {
                        chainWork->position.vy = _gActor560800ChainFirstPlacements[extendIndex].pos.vy;
                    }
                    chainRoot->composeStamp = GRAPHICS_COORD_DIRTY;
                }
                extendIndex++;
            } while (extendIndex < ARRAY_SIZE(extendingWork->chains));
            break;
        case ACTOR_560800_CHAIN_GROUP_BREAK_NEXT:
            for (chainIndex = 0; chainIndex < ARRAY_SIZE(work->chains); chainIndex++) {
                if (work->chains[chainIndex] != NULL) {
                    burstCoord = &work->chains[chainIndex]->extra.tmd->coords[ACTOR_560800_CHAIN_BREAK_PART];
                    effectSpawn(gRoomEffectWaterSprayId, burstCoord, ACTOR_560800_CHAIN_BREAK_STATIONARY_SPRAY, NULL);
                    effectSpawn(gRoomEffectWaterSprayId, burstCoord, ACTOR_560800_CHAIN_BREAK_GRAVITY_SPRAY, NULL);
                    burstIndex = 0;
                    do {
                        effectSpawn(gRoomEffectWaterSprayId, burstCoord, ACTOR_560800_CHAIN_BREAK_SCATTER_SPRAY, NULL);
                        burstIndex++;
                        effectSpawn(EFFECT_1B4, burstCoord, ACTOR_560800_CHAIN_BREAK_DRIFT_SPRITE, NULL);
                    } while (burstIndex < ACTOR_560800_CHAIN_BREAK_REPEAT_COUNT);
                    work->chains[chainIndex]->state = ACTOR_560800_CHAIN_BREAK;
                    work->chains[chainIndex]        = NULL;
                    break;
                }
            }
            task->state = ACTOR_560800_CHAIN_GROUP_FOLLOW;
            break;
    }
    // Compose the selected body joint in world space, then lower the chain target.
    currentWork = task->work;
    if (currentWork->targetEntryId == ACTOR_560800_CHAIN_TARGET_EVE) {
        _Actor560800CutsceneWork* cutsceneWork = currentWork->cutscene->work;
        targetCoords                           = cutsceneWork->eve->extra.tmd->coords;
        gfxComposeNodeWorldTransform(&targetCoords[ACTOR_560800_CHAIN_TARGET_PART], &currentWork->targetWorld, &targetPosition);
    } else if (currentWork->targetEntryId == ACTOR_560800_CHAIN_TARGET_NO9) {
        _Actor560800CutsceneWork* cutsceneWork = currentWork->cutscene->work;
        targetCoords                           = cutsceneWork->no9->extra.tmd->coords;
        gfxComposeNodeWorldTransform(&targetCoords[ACTOR_560800_CHAIN_TARGET_PART], &currentWork->targetWorld, &targetPosition);
    }
    currentWork->targetWorld.t[0] = targetPosition.vx;
    currentWork->targetWorld.t[1] = targetPosition.vy - ACTOR_560800_CHAIN_TARGET_Y_OFFSET;
    currentWork->targetWorld.t[2] = targetPosition.vz;
}

/// Applies a cutscene command to the carrier's motion state.
///
/// Reads only `command->command`: 0 relight, 1 hide, 2 first placement/descent,
/// 3 slow descent, 4 accelerated rise, 5 second placement/descent, 6 carry No. 9.
/// Placement commands show the model first and initialize scale or carry phase.
/// Requires initialized prop work; consumes the command synchronously without
/// retaining it. Ignores its context tags, message id and second argument.
/// The callback has no usable return value.
static void _actor560800ApplyCarrierCommand(Task* task, s32 messageId, ActorCommand* command, s32 unusedArg)
{
    _Actor560800PropWork* work;
    TmdObject*            model;
    VECTOR                lightingPosition;

    work = task->work;
    switch (command->command) {
        case ACTOR_560800_CARRIER_RELIGHT:
            model               = task->extra.tmd;
            lightingPosition.vx = model->coords->workm.t[0];
            lightingPosition.vy = task->extra.tmd->coords->workm.t[1];
            lightingPosition.vz = task->extra.tmd->coords->workm.t[2];
            worldCoordSetModelLighting(model, &lightingPosition, 0, 3);
            break;
        case ACTOR_560800_CARRIER_HIDE:
            task->state = ACTOR_560800_CARRIER_HIDE;
            break;
        case ACTOR_560800_CARRIER_FIRST_DESCENT:
            taskMessageDispatch(task, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gActor560800CarrierFirstDescentPlacement, 0);
            work->pulseScale = ONE;
            task->state      = ACTOR_560800_CARRIER_FIRST_DESCENT;
            break;
        case ACTOR_560800_CARRIER_SLOW_DESCENT:
            task->state = ACTOR_560800_CARRIER_SLOW_DESCENT;
            break;
        case ACTOR_560800_CARRIER_RISE:
            task->state = ACTOR_560800_CARRIER_RISE;
            break;
        case ACTOR_560800_CARRIER_SECOND_DESCENT:
            taskMessageDispatch(task, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gActor560800CarrierSecondDescentPlacement, 0);
            work->pulseScale = ONE;
            task->state      = ACTOR_560800_CARRIER_SECOND_DESCENT;
            break;
        case ACTOR_560800_CARRIER_CARRY_NO9:
            taskMessageDispatch(task, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gActor560800CarrierCarryNo9Placement, 0);
            work->carryStep = 0;
            task->state     = ACTOR_560800_CARRIER_CARRY_NO9;
            break;
    }
}

/// Accelerates the carrier upward and shrinks its second part on X/Z.
///
/// Requires initialized prop work and both model coordinates. Bit 0 of the
/// scene's phase word increments the signed-halfword speed; negative local Y
/// moves upward. While scale is at least half size, each update forces a
/// shrinking step of 50 in 12-fractional-bit scale units. The stored direction
/// reversal is overwritten on the next update, so this phase does not oscillate.
static void _actor560800RaiseCarrier(Task* task)
{
    _Actor560800PropWork* work;
    GfxCoord*             root;
    GfxCoord*             pulseCoord;
    _Actor560800PropWork* pulseWork;
    MATRIX*               pulseMatrix;
    VECTOR                scale;

    work = task->work;
    root = task->extra.tmd->coords;
    if (_gActor560800ChainSwayPhase & ACTOR_560800_CARRIER_ACCELERATE) {
        work->speed++;
    }
    // Retain the alias before the reload; removing it changes register allocation.
    pulseWork         = work;
    root->coord.t[1] -= work->speed;
    if (work->pulseScale >= ACTOR_560800_CARRIER_HALF_SCALE) {
        work->pulseGrowing = ACTOR_560800_CARRIER_SCALE_SHRINK;
        pulseWork          = task->work;
        pulseCoord         = task->extra.tmd->coords;
        pulseMatrix        = &pulseCoord[1].coord;
        gfxSetRotIdentity(pulseMatrix);
        pulseCoord++;
        if (pulseWork->pulseGrowing == ACTOR_560800_CARRIER_SCALE_SHRINK) {
            pulseWork->pulseScale -= 0x32;
            if (pulseWork->pulseScale < ONE) {
                pulseWork->pulseGrowing = ACTOR_560800_CARRIER_SCALE_GROW;
            }
        } else if (pulseWork->pulseGrowing == ACTOR_560800_CARRIER_SCALE_GROW) {
            pulseWork->pulseScale += 0x32;
            if (pulseWork->pulseScale > ACTOR_560800_CARRIER_MAX_SCALE) {
                pulseWork->pulseGrowing = ACTOR_560800_CARRIER_SCALE_SHRINK;
            }
        }
        scale.vx = pulseWork->pulseScale;
        scale.vy = ONE;
        scale.vz = pulseWork->pulseScale;
        ScaleMatrix(&pulseCoord->coord, &scale);
    }
    root->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Lowers and reshapes the carrier, then raises it together with No. 9.
///
/// Requires initialized prop work, two model coordinates, and a live cutscene
/// task in `spawnArg2` with No. 9 present. Phases 0/1 move down by 100 units per
/// update through Y=-3000 and Y=-1200; phase 1 grows X/Z scale by 50 to above
/// 1.5 times size. Phase 2 holds Y while shrinking by 200 to below unit scale.
/// Phase 3 subtracts 20 from both roots' Y each update, carrying them upward.
static void _actor560800CarryNo9Away(Task* task)
{
    enum {
        ACTOR_560800_CARRY_LOWER_FROM_TOP = 0,
        ACTOR_560800_CARRY_LOWER_AND_GROW = 1,
        ACTOR_560800_CARRY_SHRINK         = 2,
        ACTOR_560800_CARRY_RISE           = 3,
    };
    _Actor560800PropWork* work;
    GfxCoord*             root;
    GfxCoord*             pulseCoord;
    GfxCoord*             no9Root;
    _Actor560800PropWork* pulseWork;
    MATRIX*               growMatrix;
    MATRIX*               shrinkMatrix;
    VECTOR                scale;
    s32                   unitScale;

    work = task->work;
    root = task->extra.tmd->coords;
    switch (work->carryStep) {
        case ACTOR_560800_CARRY_LOWER_FROM_TOP:
            if (root->coord.t[1] >= -3000) {
                work->carryStep++;
            }
            break;
        case ACTOR_560800_CARRY_LOWER_AND_GROW:
            if (work->pulseScale <= ACTOR_560800_CARRIER_MAX_SCALE) {
                work->pulseGrowing = ACTOR_560800_CARRIER_SCALE_GROW;
                pulseWork          = task->work;
                pulseCoord         = task->extra.tmd->coords;
                growMatrix         = &pulseCoord[1].coord;
                unitScale          = ONE;
                gfxSetRotIdentity(growMatrix);
                pulseCoord++;
                if (pulseWork->pulseGrowing == ACTOR_560800_CARRIER_SCALE_SHRINK) {
                    pulseWork->pulseScale -= 0x32;
                    if (pulseWork->pulseScale < unitScale) {
                        pulseWork->pulseGrowing = ACTOR_560800_CARRIER_SCALE_GROW;
                    }
                } else if (pulseWork->pulseGrowing == ACTOR_560800_CARRIER_SCALE_GROW) {
                    pulseWork->pulseScale += 0x32;
                    if (pulseWork->pulseScale > ACTOR_560800_CARRIER_MAX_SCALE) {
                        pulseWork->pulseGrowing = ACTOR_560800_CARRIER_SCALE_SHRINK;
                    }
                }
                scale.vx = pulseWork->pulseScale;
                scale.vy = ONE;
                scale.vz = pulseWork->pulseScale;
                ScaleMatrix(&pulseCoord->coord, &scale);
            }
            if (root->coord.t[1] >= -1200) {
                work->carryStep++;
            }
            break;
        case ACTOR_560800_CARRY_SHRINK:
            if (work->pulseScale >= ONE) {
                work->pulseGrowing = ACTOR_560800_CARRIER_SCALE_SHRINK;
                pulseWork          = task->work;
                pulseCoord         = task->extra.tmd->coords;
                shrinkMatrix       = &pulseCoord[1].coord;
                unitScale          = ONE;
                gfxSetRotIdentity(shrinkMatrix);
                pulseCoord++;
                if (pulseWork->pulseGrowing == ACTOR_560800_CARRIER_SCALE_SHRINK) {
                    pulseWork->pulseScale -= 0xC8;
                    if (pulseWork->pulseScale < unitScale) {
                        pulseWork->pulseGrowing = ACTOR_560800_CARRIER_SCALE_GROW;
                    }
                } else if (pulseWork->pulseGrowing == ACTOR_560800_CARRIER_SCALE_GROW) {
                    pulseWork->pulseScale += 0xC8;
                    if (pulseWork->pulseScale > ACTOR_560800_CARRIER_MAX_SCALE) {
                        pulseWork->pulseGrowing = ACTOR_560800_CARRIER_SCALE_SHRINK;
                    }
                }
                scale.vx = pulseWork->pulseScale;
                scale.vy = ONE;
                scale.vz = pulseWork->pulseScale;
                ScaleMatrix(&pulseCoord->coord, &scale);
            } else {
                work->carryStep++;
            }
            root->composeStamp = GRAPHICS_COORD_DIRTY;
            return;
        case ACTOR_560800_CARRY_RISE: {
            Task*                     cutscene     = task->spawnArg2.pointer;
            _Actor560800CutsceneWork* cutsceneWork = cutscene->work;
            no9Root                                = cutsceneWork->no9->extra.tmd->coords;
            root->coord.t[1]                      -= 20;
            no9Root->coord.t[1]                   -= 20;
            root->composeStamp                     = GRAPHICS_COORD_DIRTY;
            no9Root->composeStamp                  = GRAPHICS_COORD_DIRTY;
            return;
        }
    }
    root->composeStamp = GRAPHICS_COORD_DIRTY;
    root->coord.t[1]  += 100;
}

/// Steps the carrier's second-part X/Z scale in its stored direction.
///
/// Requires live prop work and two model coordinates. scaleStep uses twelve
/// fractional bits and narrows the updated scale to a signed halfword. Shrink
/// flips to growth strictly below ONE; growth flips to shrink strictly above
/// the maximum. Overshoot is retained, and other direction values hold scale.
/// Rebuilds only part 1's rotation at X/Z scale, with Y fixed at ONE; retains
/// translation and leaves composition-stamp invalidation to its caller.
static inline void _actor560800StepCarrierScale(Task* task, s16 scaleStep)
{
    GfxCoord*             pulseCoord;
    _Actor560800PropWork* pulseWork;
    VECTOR                scale;
    pulseCoord = task->extra.tmd->coords;
    pulseWork  = task->work;
    gfxSetRotIdentity(&pulseCoord[1].coord);
    pulseCoord++;
    if (pulseWork->pulseGrowing == ACTOR_560800_CARRIER_SCALE_SHRINK) {
        pulseWork->pulseScale -= scaleStep;
        if (pulseWork->pulseScale < ONE) {
            pulseWork->pulseGrowing = ACTOR_560800_CARRIER_SCALE_GROW;
        }
    } else if (pulseWork->pulseGrowing == ACTOR_560800_CARRIER_SCALE_GROW) {
        pulseWork->pulseScale += scaleStep;
        if (pulseWork->pulseScale > ACTOR_560800_CARRIER_MAX_SCALE) {
            pulseWork->pulseGrowing = ACTOR_560800_CARRIER_SCALE_SHRINK;
        }
    }
    scale.vx = pulseWork->pulseScale;
    scale.vy = ONE;
    scale.vz = pulseWork->pulseScale;
    ScaleMatrix(&pulseCoord->coord, &scale);
}

/// Runs the carrier model's descent, scale changes and removal of No. 9.
///
/// Starts at state 0 with a live two-part model and cutscene Task* in spawnArg2.
/// Owns zeroed prop work and lends its lighting matrices to the model until
/// cutscene teardown. Allocation failure kills the task. The message table
/// selects hide, two descents, slow descent, accelerated ascent or the carry
/// sequence. Positive Y descends and negative Y ascends in game units. Scale
/// uses twelve fractional bits, with X/Z changing and Y fixed at ONE; each motion
/// phase forces its scale direction anew. Every post-initialization update, even
/// when hidden, advances the two scene counters by two and one respectively.
static void _actor560800CarrierTask(Task* task)
{
    enum {
        ACTOR_560800_CARRIER_STATE_INITIALIZE     = 0,
        ACTOR_560800_CARRIER_STATE_HIDE           = 1,
        ACTOR_560800_CARRIER_STATE_FIRST_DESCENT  = 2,
        ACTOR_560800_CARRIER_STATE_SLOW_DESCENT   = 3,
        ACTOR_560800_CARRIER_STATE_RISE           = 4,
        ACTOR_560800_CARRIER_STATE_SECOND_DESCENT = 5,
        ACTOR_560800_CARRIER_STATE_CARRY_NO9      = 6,
    };
    _Actor560800PropWork* work;
    _Actor560800PropWork* allocatedWork;
    TmdObject*            model;
    GfxCoord*             motionRoot;
    GfxCoord*             root;

    switch (task->state) {
        // Attach lifetime to the cutscene; the rendered root stays in view space.
        case ACTOR_560800_CARRIER_STATE_INITIALIZE:
            model      = task->extra.tmd;
            root       = model->coords;
            task->work = memMalloc(sizeof(_Actor560800PropWork), false);
            if (task->work == NULL) {
                taskKill(task);
            } else {
                memFillBytes(task->work, 0, sizeof(*allocatedWork));
                allocatedWork         = task->work;
                root->parent          = &gGfxViewCoord;
                allocatedWork->parent = task->spawnArg2.pointer;
                model->lightMtx       = &allocatedWork->light;
                model->colorMtx       = &allocatedWork->color;
                taskReparent(task->spawnArg2.pointer, task);
                task->msgTable           = _gActor560800CarrierMessages;
                _gActor560800CarrierTask = task;
                gfxSetRotIdentity(&root->coord);
            }
            task->state++;
            return;
        case ACTOR_560800_CARRIER_STATE_HIDE:
            task->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        // Positive Y descends; every update forces growth rather than oscillation.
        case ACTOR_560800_CARRIER_STATE_FIRST_DESCENT:
            motionRoot              = task->extra.tmd->coords;
            work                    = task->work;
            motionRoot->coord.t[1] += 5;
            if (work->pulseScale <= ACTOR_560800_CARRIER_MAX_SCALE) {
                work->pulseGrowing = ACTOR_560800_CARRIER_SCALE_GROW;
                _actor560800StepCarrierScale(task, 50);
            }
            motionRoot->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
        // Continue downward slowly while forcing X/Z shrinkage to half size.
        case ACTOR_560800_CARRIER_STATE_SLOW_DESCENT:
            motionRoot              = task->extra.tmd->coords;
            work                    = task->work;
            motionRoot->coord.t[1] += 1;
            if (work->pulseScale >= ACTOR_560800_CARRIER_HALF_SCALE) {
                work->pulseGrowing = ACTOR_560800_CARRIER_SCALE_SHRINK;
                _actor560800StepCarrierScale(task, 10);
            }
            motionRoot->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
        case ACTOR_560800_CARRIER_STATE_RISE:
            _actor560800RaiseCarrier(task);
            break;
        case ACTOR_560800_CARRIER_STATE_SECOND_DESCENT:
            motionRoot              = task->extra.tmd->coords;
            work                    = task->work;
            motionRoot->coord.t[1] += 5;
            if (work->pulseScale <= ACTOR_560800_CARRIER_MAX_SCALE) {
                work->pulseGrowing = ACTOR_560800_CARRIER_SCALE_GROW;
                _actor560800StepCarrierScale(task, 50);
            }
            motionRoot->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
        case ACTOR_560800_CARRIER_STATE_CARRY_NO9:
            _actor560800CarryNo9Away(task);
            break;
    }
    _gActor560800ChainSwayPhase += 2;
    gActor560800ChainGlowTick   += 1;
}

/// Changes active-draw and automatic-buffer flags on every surviving chain.
///
/// Requires initialized group work and live model tasks in non-null entries.
/// Mode 1 permits both drawing and buffering; mode 2 excludes both. Mode 0 and
/// other values leave flags intact. Retains unrelated model flags and allocates
/// no buffers. Ignores messageId/unusedArg; dispatch has no usable return value.
static void _actor560800SetChainGroupDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg)
{
    _Actor560800ChainGroupWork* work;
    TmdObject*                  model;
    Task*                       chainTask;
    s32                         chainIndex;

    work       = task->work;
    chainIndex = 0;
    do {
        chainTask = work->chains[chainIndex & 0xFFFF];
        if (chainTask != NULL) {
            model = chainTask->extra.tmd;
            switch (drawMode) {
                case ACTOR_MESSAGE_DRAW_HIDE:
                    break;
                case ACTOR_MESSAGE_DRAW_SHOW:
                    model->flags = model->flags & (u16) ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
                    break;
                case ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER:
                    model->flags = model->flags | (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
                    break;
            }
        }
        chainIndex += 1;
    } while ((u32)(chainIndex & 0xFFFF) < ARRAY_SIZE(work->chains));
}

/// Handles draw commands for a live carrier model.
///
/// Mode 1 enables active drawing and automatic buffering; mode 2 excludes both.
/// Mode 0 and other values leave every flag unchanged. Ignores the message id
/// and second argument. The callback has no usable return value.
static void _actor560800SetCarrierModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg)
{
    TmdObject* model;

    model = task->extra.tmd;
    switch (drawMode) {
        case ACTOR_MESSAGE_DRAW_HIDE:
            break;
        case ACTOR_MESSAGE_DRAW_SHOW:
            model->flags = model->flags & (u16) ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            return;
        case ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER:
            model->flags = model->flags | (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            return;
    }
}

/// Selects the private Y/X/Z placement callback for the carrier model.
///
/// The value is one static function identifier with signature
/// `void (Task*, s32, const ActorTransform*, s32)`, declared before its message
/// table. The binding applies only to the following fragment inclusion and
/// evaluates no arguments.
#define ACTOR_MESSAGE_PLACE_YAW_PITCH_ROLL_HANDLER _actorMsgPlaceYawPitchRollCarrier
#include "../../shared/actor/actor_messages_place_ypr.inc.c"
#undef ACTOR_MESSAGE_PLACE_YAW_PITCH_ROLL_HANDLER
