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
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
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
#include "../../shared/actor_messages.h"

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

extern ActorTransform D_actor_560800_80175314[];
extern ActorTransform D_actor_560800_801753D4[];
extern ActorTransform D_actor_560800_80175494[];
extern ActorTransform D_actor_560800_80175554[];
extern ActorTransform D_actor_560800_80175614[];

/// Controller task of this overlay, published by `func_actor_560800_80135BD8`
/// and read by the sub-task handlers.
extern Task* D_actor_560800_8017578C;

/// Animation block `func_actor_560800_80136378` points the `source.sets` of its
/// `AnimationPlayRequest` at when it sends message 0x3F4 - the same role
/// `D_actor_400600_80151A48` plays in that overlay.
extern AnimationSet* D_actor_560800_8016EA40[13];

extern ActorAnimChainLink D_actor_560800_8016EBE8[];
extern ActorTransform     D_actor_560800_8016F1CC[6];

/// Animation bank `_actor560800InitChainModel` hands `animationInitContext` as its
/// second argument: a null entry then one animation set per chain, indexed by
/// `_Actor560800PropWork::chainNumber`.
extern AnimationSet* D_actor_560800_801752F0[];

/// Elapsed frames, one per phase id 1..3, written by
/// `func_actor_560800_80135AEC` as the frames since that phase's timestamp.
/// The counter has wrapped if the timestamp is ahead of `gDisplayState.frameCount`,
/// which is the one case the elapsed count is short by one.
extern s32 D_actor_560800_80175790;
extern s32 D_actor_560800_80175794;
extern s32 D_actor_560800_80175798;

/// Phase timestamps, one per phase id 1..3: `func_actor_560800_80136930`
/// stamps `gDisplayState.frameCount` (the frame counter) into the slot its argument
/// selects, and `func_actor_560800_80135AEC` reads it back per phase and stores
/// the elapsed frames in the matching slot of `D_actor_560800_80175790`.
extern s32 D_actor_560800_8017579C;
extern s32 D_actor_560800_801757A0;
extern s32 D_actor_560800_801757A4;

/// Seed `func_actor_560800_80135D54` loads into `gRandomLcgState` before it hands
/// control back to gameplay.
extern u32 D_actor_560800_801757A8;

/// Pair of blocks `func_actor_560800_80135D54` passes to `evsStartScriptWithSkip`.
extern EvsCommand D_actor_560800_8016F5E0[];
extern EvsCommand D_actor_560800_80171800[];

/// Flag word whose bit 0 gates `_actor560800RaiseCarrier`'s sink step.
extern s32 D_actor_560800_801752E8;

/// Frame counter `func_actor_560800_80138FC8` raises by one per tick.
extern s32 D_actor_560800_801752EC;

extern Task* D_actor_560800_801757AC;

/// Task descriptor table the actor spawns most of its sub-tasks from, by
/// index.
extern TaskDesc D_actor_560800_801718F0[];

static void func_actor_560800_80133970(Task* arg0);
static void func_actor_560800_80134258(Task* arg0);
static void func_actor_560800_80134384(Task* arg0);
static void func_actor_560800_80134BFC(Task* arg0);

extern TaskDesc           D_actor_560800_8016EA28[];
extern TaskDesc           D_actor_560800_8017575C[];
extern AnimationSet*      D_actor_560800_8016EA74[];
extern AnimationSet*      D_actor_560800_8016EB04[];
extern AnimationSet*      D_actor_560800_8016EB30[];
extern ActorAnimChainLink D_actor_560800_8016EC1C[36];
extern ActorAnimChainLink D_actor_560800_8016ECAC[6];
extern ActorAnimChainLink D_actor_560800_8016ECC4[46];
extern ActorTransform     D_actor_560800_8016F154;
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_560800_8016F34C[2];
extern s32              D_actor_560800_8016F57C[];

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
void             func_actor_560800_80137820(Task*);
static void      _actor560800FallingChainTask(Task* task);
void             func_actor_560800_80137F58(Task*, s32, VECTOR*, s32);
void             func_actor_560800_801384EC(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);
void             func_actor_560800_801386D4(Task*);
static void      _actor560800ApplyCarrierCommand(Task* task, s32 messageId, ActorCommand* command, s32 unusedArg);
void             func_actor_560800_80138FC8(Task*);
void             func_actor_560800_80139360(Task*, s32, s32, s32);
static void      _actor560800SetCarrierModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg);
void             func_actor_560800_80139440(Task* task, s32 msgId, ActorTransform* placement, s32 arg3);

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
static TmdSource    _gActor560800EveBreaMaskedBody;
static TmdSource    _gActor560800AyaBreaBody;
static TmdSource    _gActor560800KyleMadiganBody;
static TmdSource    _gActor560800No9GolemDryfieldBody;
static TmdSource    _gActor560800KyleMadiganLeft;
static TmdSource    _gActor560800KyleMadiganHandRight;
static TmdSource    _gActor560800No9GolemDryfieldGunblade;
static TmdSource    _gActor560800KyleMadiganGun;
void                func_actor_560800_801326C4(Task*);
void                func_actor_560800_80132A14(Task*);
void                func_actor_560800_80132C60(Task*);
void                func_actor_560800_80132F64(Task*);
void                func_actor_560800_80133204(void);
void                func_actor_560800_80133648(u32);
void                func_actor_560800_80133750(s32);
void                func_actor_560800_80134B14(s32);
void                func_actor_560800_80135AEC(s32);
void                func_actor_560800_80135D54(Task*);
static void         _actor560800FadeOutTask(Task* task);
static void         _actor560800FadeInTask(Task* task);
static void         _actor560800SetCastModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg);
void                func_actor_560800_80136280(s32);
void                func_actor_560800_801362B0(s32);
void                func_actor_560800_801362E0(s16);
void                func_actor_560800_8013631C(s16);
void                func_actor_560800_80136358(s16);
void                func_actor_560800_80136378(s16);
void                func_actor_560800_801363F8(u16);
void                func_actor_560800_801364A0(u16);
void                func_actor_560800_80136548(void);
void                func_actor_560800_801365B0(s16);
void                func_actor_560800_801365D0(u16);
void                func_actor_560800_80136678(s32);
void                func_actor_560800_801366B0(Task*);
void                func_actor_560800_801367C0(s16);
void                func_actor_560800_801367E0(s16);
void                func_actor_560800_80136818(void);
void                func_actor_560800_80136878(void);
void                func_actor_560800_80136910(void);
void                func_actor_560800_80136930(s32);
void                func_actor_560800_801369A0(void);
static void         _actor560800AwaitScenePlaybackTask(Task* task);
void                func_actor_560800_80136A20(void);
void                func_actor_560800_80136A54(void);
static void         _actor560800DiscardTask(Task* task);

void func_actor_560800_801321A0(Task*);
void func_actor_560800_80135F50(Task*);

static TmdBone _gActor560800EveBreaMaskedBodySkeleton[19] = {
#include "assets/eve_brea_masked_body_skeleton.inc"
};

static u32 _gActor560800EveBreaMaskedBodyPartVerts[19] = {
#include "assets/eve_brea_masked_body_partVerts.inc"
};

static SVECTOR _gActor560800EveBreaMaskedBodyVerts[312] = {
#include "assets/eve_brea_masked_body_verts.inc"
};

static SVECTOR _gActor560800EveBreaMaskedBodyNormals[338] = {
#include "assets/eve_brea_masked_body_normals.inc"
};

static u32 _gActor560800EveBreaMaskedBodyStream[3463] = {
#include "assets/eve_brea_masked_body_stream.inc"
};

static TmdSource _gActor560800EveBreaMaskedBody = {
    0,
    18392,
    6232,
    19,
    _gActor560800EveBreaMaskedBodyPartVerts,
    _gActor560800EveBreaMaskedBodyVerts,
    _gActor560800EveBreaMaskedBodyNormals,
    _gActor560800EveBreaMaskedBodySkeleton,
    _gActor560800EveBreaMaskedBodyStream,
};

static TmdBone _gActor560800AyaBreaBodySkeleton[19] = {
#include "assets/aya_brea_body_skeleton.inc"
};

static u32 _gActor560800AyaBreaBodyPartVerts[19] = {
#include "assets/aya_brea_body_partVerts.inc"
};

static SVECTOR _gActor560800AyaBreaBodyVerts[365] = {
#include "assets/aya_brea_body_verts.inc"
};

static SVECTOR _gActor560800AyaBreaBodyNormals[385] = {
#include "assets/aya_brea_body_normals.inc"
};

static u32 _gActor560800AyaBreaBodyStream[3923] = {
#include "assets/aya_brea_body_stream.inc"
};

static TmdSource _gActor560800AyaBreaBody = {
    0,
    21760,
    5992,
    19,
    _gActor560800AyaBreaBodyPartVerts,
    _gActor560800AyaBreaBodyVerts,
    _gActor560800AyaBreaBodyNormals,
    _gActor560800AyaBreaBodySkeleton,
    _gActor560800AyaBreaBodyStream,
};

static TmdBone _gActor560800KyleMadiganBodySkeleton[20] = {
#include "assets/kyle_madigan_body_skeleton.inc"
};

static u32 _gActor560800KyleMadiganBodyPartVerts[20] = {
#include "assets/kyle_madigan_body_partVerts.inc"
};

static SVECTOR _gActor560800KyleMadiganBodyVerts[300] = {
#include "assets/kyle_madigan_body_verts.inc"
};

static SVECTOR _gActor560800KyleMadiganBodyNormals[298] = {
#include "assets/kyle_madigan_body_normals.inc"
};

static u32 _gActor560800KyleMadiganBodyStream[3412] = {
#include "assets/kyle_madigan_body_stream.inc"
};

static TmdSource _gActor560800KyleMadiganBody = {
    0,
    18224,
    5696,
    20,
    _gActor560800KyleMadiganBodyPartVerts,
    _gActor560800KyleMadiganBodyVerts,
    _gActor560800KyleMadiganBodyNormals,
    _gActor560800KyleMadiganBodySkeleton,
    _gActor560800KyleMadiganBodyStream,
};

static TmdBone _gActor560800No9GolemDryfieldBodySkeleton[19] = {
#include "assets/no9_golem_dryfield_body_skeleton.inc"
};

static u32 _gActor560800No9GolemDryfieldBodyPartVerts[19] = {
#include "assets/no9_golem_dryfield_body_partVerts.inc"
};

static SVECTOR _gActor560800No9GolemDryfieldBodyVerts[432] = {
#include "assets/no9_golem_dryfield_body_verts.inc"
};

static SVECTOR _gActor560800No9GolemDryfieldBodyNormals[444] = {
#include "assets/no9_golem_dryfield_body_normals.inc"
};

static u32 _gActor560800No9GolemDryfieldBodyStream[4749] = {
#include "assets/no9_golem_dryfield_body_stream.inc"
};

static TmdSource _gActor560800No9GolemDryfieldBody = {
    0,
    26564,
    6624,
    19,
    _gActor560800No9GolemDryfieldBodyPartVerts,
    _gActor560800No9GolemDryfieldBodyVerts,
    _gActor560800No9GolemDryfieldBodyNormals,
    _gActor560800No9GolemDryfieldBodySkeleton,
    _gActor560800No9GolemDryfieldBodyStream,
};

static TmdBone _gActor560800KyleMadiganLeftSkeleton[1] = {
#include "assets/kyle_madigan_left_skeleton.inc"
};

static u32 _gActor560800KyleMadiganLeftPartVerts[1] = {
#include "assets/kyle_madigan_left_partVerts.inc"
};

static SVECTOR _gActor560800KyleMadiganLeftVerts[23] = {
#include "assets/kyle_madigan_left_verts.inc"
};

static SVECTOR _gActor560800KyleMadiganLeftNormals[23] = {
#include "assets/kyle_madigan_left_normals.inc"
};

static u32 _gActor560800KyleMadiganLeftStream[166] = {
#include "assets/kyle_madigan_left_stream.inc"
};

static TmdSource _gActor560800KyleMadiganLeft = {
    0,
    1148,
    0,
    1,
    _gActor560800KyleMadiganLeftPartVerts,
    _gActor560800KyleMadiganLeftVerts,
    _gActor560800KyleMadiganLeftNormals,
    _gActor560800KyleMadiganLeftSkeleton,
    _gActor560800KyleMadiganLeftStream,
};

static TmdBone _gActor560800KyleMadiganHandRightSkeleton[1] = {
#include "assets/kyle_madigan_hand_right_skeleton.inc"
};

static u32 _gActor560800KyleMadiganHandRightPartVerts[1] = {
#include "assets/kyle_madigan_hand_right_partVerts.inc"
};

static SVECTOR _gActor560800KyleMadiganHandRightVerts[23] = {
#include "assets/kyle_madigan_hand_right_verts.inc"
};

static SVECTOR _gActor560800KyleMadiganHandRightNormals[23] = {
#include "assets/kyle_madigan_hand_right_normals.inc"
};

static u32 _gActor560800KyleMadiganHandRightStream[166] = {
#include "assets/kyle_madigan_hand_right_stream.inc"
};

static TmdSource _gActor560800KyleMadiganHandRight = {
    0,
    1148,
    0,
    1,
    _gActor560800KyleMadiganHandRightPartVerts,
    _gActor560800KyleMadiganHandRightVerts,
    _gActor560800KyleMadiganHandRightNormals,
    _gActor560800KyleMadiganHandRightSkeleton,
    _gActor560800KyleMadiganHandRightStream,
};

static TmdBone _gActor560800No9GolemDryfieldGunbladeSkeleton[1] = {
#include "assets/no9_golem_dryfield_gunblade_skeleton.inc"
};

static u32 _gActor560800No9GolemDryfieldGunbladePartVerts[1] = {
#include "assets/no9_golem_dryfield_gunblade_partVerts.inc"
};

static SVECTOR _gActor560800No9GolemDryfieldGunbladeVerts[43] = {
#include "assets/no9_golem_dryfield_gunblade_verts.inc"
};

static SVECTOR _gActor560800No9GolemDryfieldGunbladeNormals[41] = {
#include "assets/no9_golem_dryfield_gunblade_normals.inc"
};

static u32 _gActor560800No9GolemDryfieldGunbladeStream[326] = {
#include "assets/no9_golem_dryfield_gunblade_stream.inc"
};

static TmdSource _gActor560800No9GolemDryfieldGunblade = {
    0,
    2300,
    0,
    1,
    _gActor560800No9GolemDryfieldGunbladePartVerts,
    _gActor560800No9GolemDryfieldGunbladeVerts,
    _gActor560800No9GolemDryfieldGunbladeNormals,
    _gActor560800No9GolemDryfieldGunbladeSkeleton,
    _gActor560800No9GolemDryfieldGunbladeStream,
};

static TmdBone _gActor560800KyleMadiganGunSkeleton[1] = {
#include "assets/kyle_madigan_gun_skeleton.inc"
};

static u32 _gActor560800KyleMadiganGunPartVerts[1] = {
#include "assets/kyle_madigan_gun_partVerts.inc"
};

static SVECTOR _gActor560800KyleMadiganGunVerts[22] = {
#include "assets/kyle_madigan_gun_verts.inc"
};

static SVECTOR _gActor560800KyleMadiganGunNormals[24] = {
#include "assets/kyle_madigan_gun_normals.inc"
};

static u32 _gActor560800KyleMadiganGunStream[162] = {
#include "assets/kyle_madigan_gun_stream.inc"
};

static TmdSource _gActor560800KyleMadiganGun = {
    0,
    1108,
    0,
    1,
    _gActor560800KyleMadiganGunPartVerts,
    _gActor560800KyleMadiganGunVerts,
    _gActor560800KyleMadiganGunNormals,
    _gActor560800KyleMadiganGunSkeleton,
    _gActor560800KyleMadiganGunStream,
};

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

TaskDesc D_actor_560800_8016EA28[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_actor_560800_80135F50, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_560800_801321A0, { .value = 0 } },
};

AnimationSet* D_actor_560800_8016EA40[13] = {
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

AnimationSet* D_actor_560800_8016EA74[36] = {
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

AnimationSet* D_actor_560800_8016EB04[11] = {
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

AnimationSet* D_actor_560800_8016EB30[46] = {
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

ActorAnimChainLink D_actor_560800_8016EBE8[13] = {
    { 0, -1 },
    { 0, 10 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 90, -1 },
    { 0, -1 },
    { 65535, -1 },
};

ActorAnimChainLink D_actor_560800_8016EC1C[36] = {
    { 65535, -1 },
    { 0, 2 },
    { 0, -1 },
    { 0, 4 },
    { 0, -1 },
    { 100, 6 },
    { 0, 26 },
    { 0, -1 },
    { 65535, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, 13 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 40, 28 },
    { 0, 26 },
    { 0, 26 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, 26 },
    { 60, 35 },
    { 0, 26 },
};

ActorAnimChainLink D_actor_560800_8016ECAC[6] = {
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
};

ActorAnimChainLink D_actor_560800_8016ECC4[46] = {
    { 65535, -1 },
    { 65535, -1 },
    { 0, 3 },
    { 65535, -1 },
    { 65535, -1 },
    { 0, 6 },
    { 65535, -1 },
    { 0, 8 },
    { 65535, -1 },
    { 65535, -1 },
    { 65535, -1 },
    { 0, 12 },
    { 65535, -1 },
    { 0, 14 },
    { 65535, -1 },
    { 0, 14 },
    { 0, -1 },
    { 0, 18 },
    { 65535, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, 22 },
    { 0, 18 },
    { 0, -1 },
    { 0, 15 },
    { 0, 26 },
    { 65535, -1 },
    { 0, 14 },
    { 0, 15 },
    { 0, 30 },
    { 0, 31 },
    { 65535, -1 },
    { 0, -1 },
    { 0, 34 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 0, -1 },
    { 65535, -1 },
    { 65535, -1 },
    { 65535, -1 },
    { 0, -1 },
    { 0, -1 },
    { 65535, -1 },
};

ActorTransform D_actor_560800_8016ED7C = { 0 };

ActorTransform D_actor_560800_8016ED94 = { 0 };

ActorTransform D_actor_560800_8016EDAC = { { 6400, 0, 3350, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_560800_8016EDC4 = { 0 };

ActorTransform D_actor_560800_8016EDDC = { { 5400, 0, 3350, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_560800_8016EDF4 = { { 6000, 0, 3200, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_560800_8016EE0C = { 0 };

ActorTransform D_actor_560800_8016EE24 = { { 5850, 0, 3200, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_560800_8016EE3C = { 0 };

ActorTransform D_actor_560800_8016EE54 = { 0 };

ActorTransform D_actor_560800_8016EE6C = { 0 };

ActorTransform D_actor_560800_8016EE84 = { 0 };

ActorTransform D_actor_560800_8016EE9C[2] = { { { 5850, 0, 3200, 0 }, { 0, 1024, 0, 0 } }, { { 5850, 0, 3200, 0 }, { 0, 1024, 0, 0 } } };

ActorTransform D_actor_560800_8016EECC = { { 5850, 0, 3200, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_560800_8016EEE4 = { { 5000, 0, 3200, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_560800_8016EEFC = { { 7500, 0, 3000, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_560800_8016EF14 = { { 7850, 0, 3400, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_560800_8016EF2C = { 0 };

ActorTransform D_actor_560800_8016EF44 = { { 7500, 0, 3400, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_560800_8016EF5C = { 0 };

ActorTransform D_actor_560800_8016EF74 = { 0 };

ActorTransform D_actor_560800_8016EF8C = { { 9150, 0, 3750, 0 }, { 0, -170, 0, 0 } };

ActorTransform D_actor_560800_8016EFA4 = { 0 };

ActorTransform D_actor_560800_8016EFBC = { { 8050, 0, 3700, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_560800_8016EFD4 = { 0 };

ActorTransform D_actor_560800_8016EFEC = { { 6950, 0, 3650, 0 }, { 0, 1536, 0, 0 } };

ActorTransform D_actor_560800_8016F004 = { { 6950, 0, 3650, 0 }, { 0, 1365, 0, 0 } };

ActorTransform D_actor_560800_8016F01C = { 0 };

ActorTransform D_actor_560800_8016F034 = { 0 };

ActorTransform D_actor_560800_8016F04C = { 0 };

ActorTransform D_actor_560800_8016F064 = { { 8350, 0, 4000, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_560800_8016F07C = { { 8350, 0, 4000, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_560800_8016F094 = { 0 };

ActorTransform D_actor_560800_8016F0AC = { { 8350, 0, 4000, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_560800_8016F0C4 = { 0 };

ActorTransform D_actor_560800_8016F0DC = { 0 };

ActorTransform D_actor_560800_8016F0F4 = { 0 };

ActorTransform D_actor_560800_8016F10C = { 0 };

ActorTransform D_actor_560800_8016F124 = { { 8350, 0, 4300, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_560800_8016F13C = { { 8340, 0, 4000, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_560800_8016F154 = { { 8130, 0, 2850, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_560800_8016F16C = { 0 };

ActorTransform D_actor_560800_8016F184 = { { 8700, 0, 3200, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_560800_8016F19C = { 0 };

ActorTransform D_actor_560800_8016F1B4 = { { 8600, 0, 3500, 0 }, { 0, -512, 0, 0 } };

ActorTransform D_actor_560800_8016F1CC[6] = {
    { { 9400, 0, 2600, 0 }, { 0, -512, 0, 0 } },
    { { 9400, 0, 2600, 0 }, { 0, -1024, 0, 0 } },
    { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } },
    { { 9400, 0, 2600, 0 }, { 0, -1024, 0, 0 } },
    { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } },
    { { 7800, 0, 3200, 0 }, { 0, -1024, 0, 0 } },
};

ActorTransform D_actor_560800_8016F25C = { { 7800, 0, 3200, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_560800_8016F274 = { { 7800, 0, 3200, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_560800_8016F28C = { { 6350, 0, 3200, 0 }, { 0, -967, 0, 0 } };

ActorTransform D_actor_560800_8016F2A4 = { { 6350, 0, 3200, 0 }, { 0, 796, 0, 0 } };

ActorTransform D_actor_560800_8016F2BC = { { 8450, 0, 2850, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_560800_8016F2D4 = { 0 };

ActorTransform D_actor_560800_8016F2EC = { { 8450, 0, 2850, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_560800_8016F304 = { 0 };

ActorTransform D_actor_560800_8016F31C = { { 8450, 0, 2850, 0 }, { 0, -512, 0, 0 } };

ActorTransform D_actor_560800_8016F334 = { { 10800, 0, 3200, 0 }, { 0, -1024, 0, 0 } };

TaskMessageEntry D_actor_560800_8016F34C[2] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor560800SetCastModelDraw },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceYawPitchRoll },
};

ActorTransform* D_actor_560800_8016F35C[34] = {
    NULL,
    &D_actor_560800_8016ED7C,
    &D_actor_560800_8016ED94,
    &D_actor_560800_8016EDAC,
    &D_actor_560800_8016ED7C,
    &D_actor_560800_8016EDAC,
    &D_actor_560800_8016EDC4,
    &D_actor_560800_8016EDDC,
    &D_actor_560800_8016EDC4,
    &D_actor_560800_8016EDAC,
    &D_actor_560800_8016ED7C,
    &D_actor_560800_8016EDC4,
    &D_actor_560800_8016EDAC,
    &D_actor_560800_8016ED7C,
    &D_actor_560800_8016EDF4,
    &D_actor_560800_8016EE0C,
    &D_actor_560800_8016EDF4,
    &D_actor_560800_8016EE0C,
    &D_actor_560800_8016EDF4,
    &D_actor_560800_8016EDDC,
    &D_actor_560800_8016EDF4,
    &D_actor_560800_8016EE24,
    &D_actor_560800_8016EE3C,
    &D_actor_560800_8016ED94,
    &D_actor_560800_8016EE3C,
    &D_actor_560800_8016EE54,
    &D_actor_560800_8016EE6C,
    &D_actor_560800_8016EE84,
    D_actor_560800_8016EE9C,
    &D_actor_560800_8016EEE4,
    D_actor_560800_8016EE9C,
    &D_actor_560800_8016EEE4,
    D_actor_560800_8016EE9C,
    &D_actor_560800_8016EECC,
};

ActorTransform* D_actor_560800_8016F3E4[34] = {
    NULL,
    &D_actor_560800_8016EEFC,
    &D_actor_560800_8016EF14,
    &D_actor_560800_8016EF2C,
    &D_actor_560800_8016EEFC,
    &D_actor_560800_8016EF2C,
    &D_actor_560800_8016EF44,
    &D_actor_560800_8016EF5C,
    &D_actor_560800_8016EF44,
    &D_actor_560800_8016EF2C,
    &D_actor_560800_8016EEFC,
    &D_actor_560800_8016EF44,
    &D_actor_560800_8016EF2C,
    &D_actor_560800_8016EEFC,
    &D_actor_560800_8016EF74,
    &D_actor_560800_8016EF8C,
    &D_actor_560800_8016EF74,
    &D_actor_560800_8016EF8C,
    &D_actor_560800_8016EF74,
    &D_actor_560800_8016EF5C,
    &D_actor_560800_8016EF74,
    &D_actor_560800_8016EFA4,
    &D_actor_560800_8016EFBC,
    &D_actor_560800_8016EF14,
    &D_actor_560800_8016EFBC,
    &D_actor_560800_8016EFD4,
    &D_actor_560800_8016EFEC,
    &D_actor_560800_8016F004,
    &D_actor_560800_8016F01C,
    &D_actor_560800_8016F034,
    &D_actor_560800_8016F01C,
    &D_actor_560800_8016F034,
    &D_actor_560800_8016F01C,
    &D_actor_560800_8016F04C,
};

ActorTransform* D_actor_560800_8016F46C[34] = {
    NULL,
    D_actor_560800_8016F1CC,
    &D_actor_560800_8016F1CC[1],
    &D_actor_560800_8016F1CC[2],
    D_actor_560800_8016F1CC,
    &D_actor_560800_8016F1CC[2],
    &D_actor_560800_8016F1CC[3],
    &D_actor_560800_8016F1CC[4],
    &D_actor_560800_8016F1CC[3],
    &D_actor_560800_8016F1CC[2],
    D_actor_560800_8016F1CC,
    &D_actor_560800_8016F1CC[3],
    &D_actor_560800_8016F1CC[2],
    D_actor_560800_8016F1CC,
    &D_actor_560800_8016F334,
    &D_actor_560800_8016F25C,
    &D_actor_560800_8016F1CC[5],
    &D_actor_560800_8016F25C,
    &D_actor_560800_8016F1CC[5],
    &D_actor_560800_8016F1CC[4],
    &D_actor_560800_8016F1CC[5],
    &D_actor_560800_8016F274,
    &D_actor_560800_8016F28C,
    &D_actor_560800_8016F1CC[1],
    &D_actor_560800_8016F28C,
    &D_actor_560800_8016F2A4,
    &D_actor_560800_8016F2BC,
    &D_actor_560800_8016F2D4,
    &D_actor_560800_8016F2EC,
    &D_actor_560800_8016F304,
    &D_actor_560800_8016F2EC,
    &D_actor_560800_8016F304,
    &D_actor_560800_8016F2EC,
    &D_actor_560800_8016F31C,
};

ActorTransform* D_actor_560800_8016F4F4[34] = {
    NULL,
    &D_actor_560800_8016F064,
    &D_actor_560800_8016F07C,
    &D_actor_560800_8016F094,
    &D_actor_560800_8016F064,
    &D_actor_560800_8016F094,
    &D_actor_560800_8016F0AC,
    &D_actor_560800_8016F0C4,
    &D_actor_560800_8016F0AC,
    &D_actor_560800_8016F094,
    &D_actor_560800_8016F064,
    &D_actor_560800_8016F0AC,
    &D_actor_560800_8016F094,
    &D_actor_560800_8016F064,
    &D_actor_560800_8016F0DC,
    &D_actor_560800_8016F0F4,
    &D_actor_560800_8016F0DC,
    &D_actor_560800_8016F0F4,
    &D_actor_560800_8016F0DC,
    &D_actor_560800_8016F0C4,
    &D_actor_560800_8016F0DC,
    &D_actor_560800_8016F10C,
    &D_actor_560800_8016F124,
    &D_actor_560800_8016F07C,
    &D_actor_560800_8016F124,
    &D_actor_560800_8016F13C,
    &D_actor_560800_8016F154,
    &D_actor_560800_8016F16C,
    &D_actor_560800_8016F184,
    &D_actor_560800_8016F19C,
    &D_actor_560800_8016F184,
    &D_actor_560800_8016F19C,
    &D_actor_560800_8016F184,
    &D_actor_560800_8016F1B4,
};

s32 D_actor_560800_8016F57C[19] = {
    0,
    0x404D0001,
    0x404D0002,
    0x404D0003,
    0x404D0004,
    0x404D0005,
    0x404D0006,
    0x404D0007,
    0x404D0008,
    0x404D0009,
    0x404D000A,
    0x404D000B,
    0x404D000C,
    0x404D000D,
    0x404D000E,
    0x404D000F,
    0x404D0010,
    0x404D0011,
    0x404D0012,
};

EvsSceneKey D_actor_560800_8016F5C8 = { 6, 8, 11 };

EvsSceneKey D_actor_560800_8016F5D0 = { 6, 8, 21 };

EvsSceneKey D_actor_560800_8016F5D8 = { 6, 8, 31 };

EvsCommand D_actor_560800_8016F5E0[364] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801362E0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_560800_8016F5C8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80136910 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136930 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136280 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801362E0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801362E0 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136678 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801362E0 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136678 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136678 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136678 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136678 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136678 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136678 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136678 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136678 }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136678 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136678 }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136678 }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136678 }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136678 }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136678 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136678 }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136678 }, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136678 }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 27 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801365B0 }, { .value = 37 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801365B0 }, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_801362B0 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80135AEC }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_801369A0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80136A20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_560800_8016F5D0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80136910 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136280 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80136548 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136930 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_80136358 }, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_80136378 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801365D0 }, { .value = 27 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801365D0 }, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801365D0 }, { .value = 34 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801365D0 }, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801365D0 }, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801365B0 }, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801365D0 }, { .value = 27 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_80136378 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801365D0 }, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801365D0 }, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801365D0 }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_80136378 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80135AEC }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_801369A0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80136A54 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80136818 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136280 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_560800_8016F5D8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80136910 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136930 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801365D0 }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801362E0 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 24 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 24 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801365B0 }, { .value = 36 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_8013631C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_8013631C }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801362E0 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80134B14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801362E0 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80134B14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801363F8 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801362E0 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_8013631C }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80134B14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801362E0 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80134B14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801362E0 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_8013631C }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_801362B0 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367C0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_8013631C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU32 = func_actor_560800_80133648 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136280 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_8013631C }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 27 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_8013631C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 27 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 37 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_8013631C }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801364A0 }, { .value = 39 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_801362B0 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU32 = func_actor_560800_80133648 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80136280 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801365B0 }, { .value = 39 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 31 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 31 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801363F8 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801363F8 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80133750 }, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80133204 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_560800_801367E0 }, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU16 = func_actor_560800_801365D0 }, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_801362B0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_560800_80135AEC }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_560800_80171800[10] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80136878 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_560800_80136548 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_actor_560800_801718F0[14] = {
    { { { TASK_BODY_NONE, 192 } }, func_actor_560800_80135D54, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor560800DiscardTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor560800FadeInTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor560800FadeOutTask, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_560800_801326C4, { .model = &_gActor560800EveBreaMaskedBody } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_560800_80132C60, { .model = &_gActor560800KyleMadiganBody } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_560800_80132F64, { .model = &_gActor560800No9GolemDryfieldBody } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_560800_80132A14, { .model = &_gActor560800KyleMadiganLeft } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_560800_80132A14, { .model = &_gActor560800KyleMadiganHandRight } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_560800_80132A14, { .model = &_gActor560800KyleMadiganGun } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_560800_80132A14, { .model = &_gActor560800No9GolemDryfieldGunblade } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_560800_801326C4, { .model = &_gActor560800AyaBreaBody } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_560800_801366B0, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor560800AwaitScenePlaybackTask, { .value = 0 } },
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

s32 D_actor_560800_801752E8 = 1;

s32 D_actor_560800_801752EC = 1;

AnimationSet* D_actor_560800_801752F0[9] = {
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

ActorTransform D_actor_560800_80175314[8] = {
    { { 400, -2800, 900, 0 }, { 1360, 2048, 64, 0 } },
    { { 300, -2800, 800, 0 }, { 1430, 2048, 48, 0 } },
    { { 200, -2800, 800, 0 }, { 1070, 2048, 32, 0 } },
    { { 100, -2800, 1100, 0 }, { 1190, 2048, 16, 0 } },
    { { 0, -2800, 900, 0 }, { 1170, 2048, 0, 0 } },
    { { -100, -2800, 1100, 0 }, { 1480, 2048, -16, 0 } },
    { { -200, -2800, 900, 0 }, { 1150, 2048, -32, 0 } },
    { { -300, -2800, 900, 0 }, { 1200, 2048, -48, 0 } },
};

ActorTransform D_actor_560800_801753D4[8] = {
    { { 300, -3700, 900, 0 }, { 1280, 2048, 0, 0 } },
    { { 200, -3700, 800, 0 }, { 1280, 2048, 0, 0 } },
    { { 100, -3700, 800, 0 }, { 1280, 2048, 0, 0 } },
    { { 0, -3700, 1100, 0 }, { 1280, 2048, 0, 0 } },
    { { -100, -3700, 900, 0 }, { 1280, 2048, 0, 0 } },
    { { -200, -3700, 1100, 0 }, { 1280, 2048, 0, 0 } },
    { { -300, -3700, 900, 0 }, { 1280, 2048, 0, 0 } },
    { { -400, -3700, 900, 0 }, { 1280, 2048, 0, 0 } },
};

ActorTransform D_actor_560800_80175494[8] = {
    { { 250, -2800, 900, 0 }, { 1280, 2048, -80, 0 } },
    { { 200, -2800, 900, 0 }, { 1280, 2048, 64, 0 } },
    { { 150, -2800, 800, 0 }, { 1280, 2048, 48, 0 } },
    { { 100, -2800, 800, 0 }, { 1280, 2048, 32, 0 } },
    { { 50, -2800, 1100, 0 }, { 1280, 2048, 16, 0 } },
    { { 0, -2800, 900, 0 }, { 1280, 2048, 0, 0 } },
    { { -50, -2800, 1100, 0 }, { 1280, 2048, -16, 0 } },
    { { -100, -2800, 900, 0 }, { 1280, 2048, -32, 0 } },
};

ActorTransform D_actor_560800_80175554[8] = {
    { { 0, -2710, 370, 0 }, { 0, 0, 0, 0 } },
    { { 0, -2710, 370, 0 }, { 0, 0, 0, 0 } },
    { { 0, -2710, 370, 0 }, { 0, 0, 0, 0 } },
    { { 0, -2710, 370, 0 }, { 0, 0, 0, 0 } },
    { { 0, -2710, 370, 0 }, { 0, 0, 0, 0 } },
    { { 0, -2710, 370, 0 }, { 0, 0, 0, 0 } },
    { { 0, -2710, 370, 0 }, { 0, 0, 0, 0 } },
    { { 0, -2710, 370, 0 }, { 0, 0, 0, 0 } },
};

ActorTransform D_actor_560800_80175614[8] = {
    { { 0, -1100, -200, 0 }, { 1080, 2048, 0, 0 } },
    { { 0, -1100, -200, 0 }, { 968, 2048, 0, 0 } },
    { { 0, -1100, -200, 0 }, { 1137, 2048, 0, 0 } },
    { { 0, -1100, -200, 0 }, { 911, 2048, 0, 0 } },
    { { 0, -1100, -200, 0 }, { 1024, 2048, 0, 0 } },
    { { 0, -1100, -200, 0 }, { 1024, 2048, 0, 0 } },
    { { 0, -1100, -200, 0 }, { 1024, 2048, 0, 0 } },
    { { 0, -1100, -200, 0 }, { 1024, 2048, 0, 0 } },
};

TaskMessageEntry D_actor_560800_801756D4[3] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_560800_80139360 },
    { ACTOR_MESSAGE_PLACE, func_actor_560800_80137F58 },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_560800_801384EC },
};

u16 D_actor_560800_801756EC[8] = {
    1,
    2,
    3,
    1,
    2,
    3,
    1,
    2,
};

s32 D_actor_560800_801756FC[6] = {
    8340,
    -2200,
    4000,
    0,
    0,
    0,
};

s32 D_actor_560800_80175714[6] = {
    6950,
    -2200,
    3650,
    0,
    0,
    0,
};

s32 D_actor_560800_8017572C[6] = {
    6950,
    -2500,
    3500,
    0,
    0,
    0,
};

TaskMessageEntry D_actor_560800_80175744[3] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor560800SetCarrierModelDraw },
    { ACTOR_MESSAGE_PLACE, func_actor_560800_80139440 },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor560800ApplyCarrierCommand },
};

TaskDesc D_actor_560800_8017575C[4] = {
    { { { TASK_BODY_COORD, 192 } }, func_actor_560800_801386D4, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_560800_80137820, { .model = &_gActor560800Model40064 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_560800_80138FC8, { .model = &_gActor560800Model41AC4 } },
    { { { TASK_BODY_TMD, 192 } }, _actor560800FallingChainTask, { .model = &_gActor560800Model40E78 } },
};

Task* D_actor_560800_8017578C = NULL;

s32 D_actor_560800_80175790;

s32 D_actor_560800_80175794;

s32 D_actor_560800_80175798;

s32 D_actor_560800_8017579C;

s32 D_actor_560800_801757A0;

s32 D_actor_560800_801757A4;

u32 D_actor_560800_801757A8;

Task* D_actor_560800_801757AC;

extern ActorTransform* D_actor_560800_8016F35C[];

extern ActorTransform* D_actor_560800_8016F3E4[];

extern ActorTransform* D_actor_560800_8016F46C[];

extern ActorTransform* D_actor_560800_8016F4F4[];

extern TaskMessageEntry D_actor_560800_801756D4[3];

extern u16 D_actor_560800_801756EC[];

extern s32 D_actor_560800_801756FC[];

extern s32 D_actor_560800_80175714[];

extern s32 D_actor_560800_8017572C[];

/// Per-frame handler of a model task: state 0 allocates its
/// `_Actor560800PropWork`, parents the root coordinate to `gGfxViewCoord`,
/// publishes the task as `D_actor_560800_801757AC` and resets the root matrix
/// to identity. States 2/5 lift the root
/// by 5 while pulsing the second coordinate's X/Z scale in steps of 0x32, state
/// 3 by 1 in steps of 0xA; 4 and 6 hand off to `_actor560800RaiseCarrier` /
/// `_actor560800CarryNo9Away`. Every state but 0 advances the two frame
/// counters. Each case needs its own matrix pointer (and case 0 its own work
/// pointer): a pointer shared across cases is a global pseudo, so the local
/// 0x1000 constant takes `$v0` from it.
extern TaskMessageEntry D_actor_560800_80175744[3];

/// Blend duration used by the scene's player requests and cast-chain changes.
enum { ACTOR_560800_ANIMATION_BLEND_FRAMES = 10 };

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

static s32         func_actor_560800_80132340(Task* arg0);
static inline void _actor560800BlendCastAnimation(Task* task, u16 animationId, s16 animationRate);
static void        _actor560800ShowCastMember(u32 memberId);
static inline void Actor560800_PlayAnim(Task* task, u16 anim);
static inline void _actor560800BlendPlayerAnimation(Task* task, u16 animationId, s32 blendFrames);
static inline void Actor560800_PlaySe(s16 arg4);
static inline void _actor560800BlendPlayerWeaponAnimation(s32 animationId);
static inline void Actor560800_SpawnSparksA(Task* task);
static inline void Actor560800_SpawnSparksB(Task* task);
static inline void Actor560800_ResetAnimSlots(_Actor560800CastWork* anim, s16 clip);
static inline void Actor560800_BlendSlotsFirst(Task* task, u16 id, s16 rate);
static inline void Actor560800_ResetSlots(Task* task, u16 id, u16 rate);
static void        func_actor_560800_80135BD8(Task* arg0);
static void        func_actor_560800_80136AA8(Task* arg0);
static void        _actor560800InitChainModel(Task* task);
static void        _actor560800RaiseCarrier(Task* task);
static void        _actor560800CarryNo9Away(Task* task);

void func_actor_560800_801321A0(Task* task)
{
    u8          slotParam[4];
    GameLoc     key;
    CdCmdQueue* queue = &gCdCmdQueue;

    switch (task->state) {
        case 0:
            SetDispMask(0);
            Mem_AllocAuxWithImages(1);
            task->state++;
            break;
        case 1:
            key = gGameSession->location;
            if (task->spawnArg1.value != 0) {
                key.loc.view = 0x65;
            } else {
                key.loc.view = 0x64;
            }
            slotParam[0] = streamFindMovieSlot(&key.loc, 0, 0);
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
            task->state++;
            break;
        case 2:
            if (queue->movieReady != 0) {
                SetDispMask(1);
                task->state++;
            }
            break;
        case 3:
            if (cdCmdIsIdle()) {
                SetDispMask(0);
                task->state++;
            } else if (Pad_CheckFlag800()) {
                SetDispMask(0);
                cdCmdRequestCancel();
                task->state++;
            }
            break;
        case 4:
            if (cdCmdIsIdle()) {
                Stream_ResetRestoreState();
                task->state++;
            }
            break;
        case 5:
            if (Stream_RestoreAfterLoad(0, 1)) {
                taskKill(task);
                displayResumeGameLoop();
            }
            break;
    }
}

static s32 func_actor_560800_80132340(Task* arg0)
{
    _Actor560800CutsceneWork* work;
    ActorAnimChainLink*       table;
    ActorAnimChainLink*       entry;
    ActorAnimChainLink*       entry2;
    AnimationPlayRequest      msg;
    u16                       anim;
    u16                       anim2;

    work = arg0->work;
    if (work->player == NULL) {
        return 1;
    }
    table = D_actor_560800_8016EBE8;
    entry = &table[work->playerAnimId];
    if (entry->holdFrames != 0) {
        if (work->playerAnimHold >= entry->holdFrames) {
            if (entry->nextAnimId < 0) {
                return 1;
            }
            anim                     = entry->nextAnimId;
            msg.source.sets          = D_actor_560800_8016EA40;
            work->playerAnimId       = anim;
            msg.animationId          = anim;
            msg.blend                = ANIMATION_BLEND_INTERPOLATE;
            msg.blendFrames          = 0xA;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(work->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            work->playerAnimHold = 0;
        } else {
            work->playerAnimHold += 1;
        }
    } else {
        if (taskMessageDispatch(work->player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) != 0) {
            return 0;
        }
        entry2 = &D_actor_560800_8016EBE8[work->playerAnimId];
        if (entry2->nextAnimId < 0) {
            return 1;
        }
        work = arg0->work;
        if (work->player != NULL) {
            anim2                    = entry2->nextAnimId;
            msg.source.sets          = D_actor_560800_8016EA40;
            work->playerAnimId       = anim2;
            msg.animationId          = anim2;
            msg.blend                = ANIMATION_BLEND_INTERPOLATE;
            msg.blendFrames          = 0xA;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(work->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            work->playerAnimHold = 0;
        }
    }
    return 0;
}

/// Cross-fades slots 1..`slotCount` - 1 of `work`'s animation context to animation
/// `id` over `frames` frames.
#define _ACTOR560800_BLEND_SLOTS(work, id, frames)                                \
    do {                                                                          \
        u16 _i;                                                                   \
        for (_i = 1; _i < (work)->slotCount; _i++) {                              \
            animationSeekSlotWithBlend(&(work)->rig.anim, _i, (id), 0, (frames)); \
        }                                                                         \
    } while (0)

/// Restarts the animation clip's hold counter.
#define _actor560800ResetAnimHold(work) \
    do {                                \
        (work)->animHold = 0;           \
    } while (0)

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

/// Cross-fades animation slots 1..`slotCount` - 1 of `work`'s rig to animation
/// `id` over `frames` frames.
#define _ACTOR560800_BLEND_SLOTS(work, id, frames)                                \
    do {                                                                          \
        u16 _i;                                                                   \
        for (_i = 1; _i < (work)->slotCount; _i++) {                              \
            animationSeekSlotWithBlend(&(work)->rig.anim, _i, (id), 0, (frames)); \
        }                                                                         \
    } while (0)

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

/// Spawn handler of the floor-quad model task: state 0 allocates its
/// `_Actor560800CastWork`, seeds animation 0 (or 2 when `spawnArg1` is set),
/// and state 1 sends message 0x7D4 once when `spawnArg1` is 1. Every frame
/// draws the floor quad and ticks the animation script.
void func_actor_560800_801326C4(Task* arg0)
{
    _Actor560800CastWork* work = arg0->work;
    SVECTOR               shadowOffset;
    VECTOR                pos;

    switch (arg0->state) {
        case 0: {
            u16 failed;
            {
                TmdObject*            tmd   = arg0->extra.tmd;
                GfxCoord*             coord = tmd->coords;
                _Actor560800CastWork* block = memMalloc(sizeof(*block), false);
                AreaPlacement*        place;
                u8                    id;

                arg0->work = block;
                if (block == NULL) {
                    failed = 1;
                } else {
                    coord->parent = &gGfxViewCoord;
                    memFillBytes(arg0->work, 0, sizeof(_Actor560800CastWork));
                    tmd->lightMtx  = &block->light;
                    tmd->colorMtx  = &block->color;
                    arg0->msgTable = D_actor_560800_8016F34C;
                    place          = Gp_GetNestedAreaRec(&gGameSession->location.loc)->placements;
                    id             = place->entryId;
                    while (id != AREA_PLACEMENT_END) {
                        if (id == 0x83) {
                            break;
                        }
                        place++;
                        id = place->entryId;
                    }
                    tmdSetTextureOffsets(arg0->extra.tmd, place->texturePageOffset, place->clutRowOffset);
                    taskReparent(D_actor_560800_8017578C, arg0);
                    failed = 0;
                }
            }
            if (failed) {
                taskKill(arg0);
                return;
            }
            arg0->extra.tmd->flags &= ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            work                    = arg0->work;
            {
                TmdObject* obj = arg0->extra.tmd;
                animationInitContext(&work->rig.anim, D_actor_560800_8016EB04, obj, work->rig.poses, work->rig.slots);
            }
            work->slotCount = 0x13;
            work->animChain = D_actor_560800_8016ECAC;
            if (arg0->spawnArg1.value == 0) {
                _Actor560800CastWork* w = arg0->work;
                u16                   i;
                s32                   fade = ANIMATION_RATE_ONE;

                w->animId   = 0;
                w->animRate = fade;
                w->animHold = 0;
                for (i = 1; i < w->slotCount; i++) {
                    w->rig.slots[i].rate = fade;
                    animationResetSlot(&w->rig.anim, i, 0);
                }
            } else {
                _Actor560800CastWork* w = arg0->work;
                u16                   i;
                s32                   fade = ANIMATION_RATE_ONE;

                w->animId   = 2;
                w->animRate = fade;
                w->animHold = 0;
                for (i = 1; i < w->slotCount; i++) {
                    w->rig.slots[i].rate = fade;
                    animationResetSlot(&w->rig.anim, i, 2);
                }
            }
            arg0->state += 1;
            break;
        }
        case 2: // an empty case: GCC then roots the case tree at 1
            break;
        case 1: {
            s32 arg = arg0->spawnArg1.value;
            if (arg == 1) {
                TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_actor_560800_8016F154, 0);
                work->relit  = arg;
                arg0->state += 1;
            }
        } break;
    }
    shadowOffset.vx = 0;
    shadowOffset.vy = 0x380;
    shadowOffset.vz = 0;
    actorRenderDrawGroundShadow(&arg0->extra.tmd->coords[1], 0x300, &shadowOffset);
    _actor560800TickCastAnimationChain(arg0);
    if (work->relit != 0) {
        TmdObject* obj = arg0->extra.tmd;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = arg0->extra.tmd->coords->workm.t[1];
        pos.vz = arg0->extra.tmd->coords->workm.t[2];
        worldCoordSetModelLighting(obj, &pos, 0, 3);
    }
}

void func_actor_560800_80132A14(Task* arg0)
{
    _Actor560800CastWork* work = arg0->work;
    VECTOR                pos;

    if (arg0->state == 0) {
        TmdObject*            tmd    = arg0->extra.tmd;
        Task*                 parent = arg0->spawnArg2.pointer;
        GfxCoord*             coord  = tmd->coords;
        _Actor560800CastWork* block;
        AreaPlacement*        place;
        u8                    id;

        block      = memMalloc(sizeof(*block), false);
        arg0->work = block;
        if (block == NULL) {
            taskKill(arg0);
            return;
        }
        work = block;
        switch (arg0->spawnArg1.value) {
            case 0:
                coord->parent = &parent->extra.tmd->coords[12];
                break;
            case 1:
            case 2:
            case 3:
                coord->parent = &parent->extra.tmd->coords[8];
                break;
        }
        memFillBytes(arg0->work, 0, sizeof(_Actor560800CastWork));
        tmd->lightMtx = &work->light;
        tmd->colorMtx = &work->color;
        if (arg0->spawnArg1.value < 2) {
            place = Gp_GetNestedAreaRec(&gGameSession->location.loc)->placements;
            id    = place->entryId;
            while (id != AREA_PLACEMENT_END) {
                if (id == 0x65) {
                    break;
                }
                place++;
                id = place->entryId;
            }
            tmdSetTextureOffsets(arg0->extra.tmd, place->texturePageOffset, place->clutRowOffset);
        } else if (arg0->spawnArg1.value == 2) {
            tmdSetTextureOffsets(arg0->extra.tmd, 0, 0);
        } else if (arg0->spawnArg1.value == 3) {
            place = Gp_GetNestedAreaRec(&gGameSession->location.loc)->placements;
            id    = place->entryId;
            while (id != AREA_PLACEMENT_END) {
                if (id == 0x22) {
                    break;
                }
                place++;
                id = place->entryId;
            }
            tmdSetTextureOffsets(arg0->extra.tmd, place->texturePageOffset, place->clutRowOffset);
        }
        taskReparent(parent, arg0);
        arg0->msgTable = D_actor_560800_8016F34C;
        arg0->state   += 1;
        return;
    }
    if (work->relit != 0) {
        TmdObject* obj = arg0->extra.tmd;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = arg0->extra.tmd->coords->workm.t[1];
        pos.vz = arg0->extra.tmd->coords->workm.t[2];
        worldCoordSetModelLighting(obj, &pos, 0, 3);
    }
}

void func_actor_560800_80132C60(Task* arg0)
{
    _Actor560800CastWork* work = arg0->work;
    SVECTOR               shadowOffset;
    VECTOR                pos;

    if (arg0->state == 0) {
        u16 failed;
        {
            TmdObject*            tmd   = arg0->extra.tmd;
            GfxCoord*             coord = tmd->coords;
            _Actor560800CastWork* block = memMalloc(sizeof(*block), false);
            AreaPlacement*        place;
            u8                    id;

            arg0->work = block;
            if (block == NULL) {
                failed = 1;
            } else {
                coord->parent = &gGfxViewCoord;
                memFillBytes(arg0->work, 0, sizeof(_Actor560800CastWork));
                tmd->lightMtx  = &block->light;
                tmd->colorMtx  = &block->color;
                arg0->msgTable = D_actor_560800_8016F34C;
                place          = Gp_GetNestedAreaRec(&gGameSession->location.loc)->placements;
                id             = place->entryId;
                while (id != AREA_PLACEMENT_END) {
                    if (id == 0x65) {
                        break;
                    }
                    place++;
                    id = place->entryId;
                }
                tmdSetTextureOffsets(arg0->extra.tmd, place->texturePageOffset, place->clutRowOffset);
                taskReparent(D_actor_560800_8017578C, arg0);
                failed = 0;
            }
        }
        if (failed) {
            taskKill(arg0);
            return;
        }
        work = arg0->work;
        {
            TmdObject* obj = arg0->extra.tmd;
            animationInitContext(&work->rig.anim, D_actor_560800_8016EA74, obj, work->rig.poses, work->rig.slots);
        }
        work->slotCount = 0x14;
        work->animChain = D_actor_560800_8016EC1C;
        {
            _Actor560800CastWork* w = arg0->work;
            u16                   i;
            s32                   fade = ANIMATION_RATE_ONE;

            w->animId   = 0;
            w->animRate = fade;
            w->animHold = 0;
            for (i = 1; i < w->slotCount; i++) {
                w->rig.slots[i].rate = fade;
                animationResetSlot(&w->rig.anim, i, 0);
            }
        }
        arg0->state += 1;
    }
    shadowOffset.vx = 0;
    shadowOffset.vy = 0x380;
    shadowOffset.vz = 0;
    actorRenderDrawGroundShadow(&arg0->extra.tmd->coords[1], 0x300, &shadowOffset);
    if (work->animPaused == 0) {
        _actor560800TickCastAnimationChain(arg0);
    }
    gfxRotMatrixY(&arg0->extra.tmd->coords[4].coord, work->part4Yaw, 0);
    gfxRotMatrixX(&arg0->extra.tmd->coords[4].coord, work->part4Pitch, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixZ(&arg0->extra.tmd->coords[2].coord, work->part2Roll, GRAPHICS_ROTATION_COMPOSE);
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->animPaused != 0) {
        work->part4Yaw   = 0;
        work->part4Pitch = 0;
        work->part2Roll  = 0;
    }
    if (work->relit != 0) {
        TmdObject* obj = arg0->extra.tmd;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = arg0->extra.tmd->coords->workm.t[1];
        pos.vz = arg0->extra.tmd->coords->workm.t[2];
        worldCoordSetModelLighting(obj, &pos, 0, 3);
    }
}

void func_actor_560800_80132F64(Task* arg0)
{
    _Actor560800CastWork* work = arg0->work;
    SVECTOR               shadowOffset;
    VECTOR                pos;

    if (arg0->state == 0) {
        u16 failed;
        {
            TmdObject*            tmd   = arg0->extra.tmd;
            GfxCoord*             coord = tmd->coords;
            _Actor560800CastWork* block = memMalloc(sizeof(*block), false);
            AreaPlacement*        place;
            u8                    id;

            arg0->work = block;
            if (block == NULL) {
                failed = 1;
            } else {
                coord->parent = &gGfxViewCoord;
                memFillBytes(arg0->work, 0, sizeof(_Actor560800CastWork));
                tmd->lightMtx  = &block->light;
                tmd->colorMtx  = &block->color;
                arg0->msgTable = D_actor_560800_8016F34C;
                place          = Gp_GetNestedAreaRec(&gGameSession->location.loc)->placements;
                id             = place->entryId;
                while (id != AREA_PLACEMENT_END) {
                    if (id == 0x22) {
                        break;
                    }
                    place++;
                    id = place->entryId;
                }
                tmdSetTextureOffsets(arg0->extra.tmd, place->texturePageOffset, place->clutRowOffset);
                taskReparent(D_actor_560800_8017578C, arg0);
                failed = 0;
            }
        }
        if (failed) {
            taskKill(arg0);
            return;
        }
        work = arg0->work;
        {
            TmdObject* obj = arg0->extra.tmd;
            animationInitContext(&work->rig.anim, D_actor_560800_8016EB30, obj, work->rig.poses, work->rig.slots);
        }
        work->slotCount = 0x13;
        work->animChain = D_actor_560800_8016ECC4;
        {
            _Actor560800CastWork* w = arg0->work;
            u16                   i;
            s32                   fade = ANIMATION_RATE_ONE;

            w->animId   = 0;
            w->animRate = fade;
            w->animHold = 0;
            for (i = 1; i < w->slotCount; i++) {
                w->rig.slots[i].rate = fade;
                animationResetSlot(&w->rig.anim, i, 0);
            }
        }
        arg0->state += 1;
    }
    if (!(arg0->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) && work->floorQuadHidden == 0) {
        shadowOffset.vx = 0;
        shadowOffset.vy = 0x380;
        shadowOffset.vz = 0;
        actorRenderDrawGroundShadow(&arg0->extra.tmd->coords[1], 0x300, &shadowOffset);
    }
    _actor560800TickCastAnimationChain(arg0);
    if (work->relit != 0) {
        TmdObject* obj = arg0->extra.tmd;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = arg0->extra.tmd->coords->workm.t[1];
        pos.vz = arg0->extra.tmd->coords->workm.t[2];
        worldCoordSetModelLighting(obj, &pos, 0, 3);
    }
}

void func_actor_560800_80133204(void)
{
    _Actor560800CutsceneWork* work = D_actor_560800_8017578C->work;
    Task*                     task;
    VECTOR                    pos;

    task = work->eve;
    if (task != NULL) {
        TmdObject* obj = task->extra.tmd;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = task->extra.tmd->coords->workm.t[1];
        pos.vz = task->extra.tmd->coords->workm.t[2];
        worldCoordSetModelLighting(obj, &pos, 0, 3);
    }
    task = work->kyle;
    if (task != NULL) {
        TmdObject* obj = task->extra.tmd;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = task->extra.tmd->coords->workm.t[1];
        pos.vz = task->extra.tmd->coords->workm.t[2];
        worldCoordSetModelLighting(obj, &pos, 0, 3);
    }
    task = work->kyleGunHand;
    if (task != NULL) {
        TmdObject* obj = task->extra.tmd;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = task->extra.tmd->coords->workm.t[1];
        pos.vz = task->extra.tmd->coords->workm.t[2];
        worldCoordSetModelLighting(obj, &pos, 0, 3);
    }
    task = work->kyleFreeHand;
    if (task != NULL) {
        TmdObject* obj = task->extra.tmd;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = task->extra.tmd->coords->workm.t[1];
        pos.vz = task->extra.tmd->coords->workm.t[2];
        worldCoordSetModelLighting(obj, &pos, 0, 3);
    }
    task = work->kyleGun;
    if (task != NULL) {
        TmdObject* obj = task->extra.tmd;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = task->extra.tmd->coords->workm.t[1];
        pos.vz = task->extra.tmd->coords->workm.t[2];
        worldCoordSetModelLighting(obj, &pos, 0, 3);
    }
    task = work->no9;
    if (task != NULL) {
        TmdObject* obj = task->extra.tmd;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = task->extra.tmd->coords->workm.t[1];
        pos.vz = task->extra.tmd->coords->workm.t[2];
        worldCoordSetModelLighting(obj, &pos, 0, 3);
    }
    task = work->no9Gunblade;
    if (task != NULL) {
        TmdObject* obj = task->extra.tmd;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = task->extra.tmd->coords->workm.t[1];
        pos.vz = task->extra.tmd->coords->workm.t[2];
        worldCoordSetModelLighting(obj, &pos, 0, 3);
    }
    if (work->chainGroup != NULL) {
        _Actor560800CutsceneWork* w = D_actor_560800_8017578C->work;

        ((SVECTOR*)&pos)->vy = 0;
        TASK_MESSAGE_DISPATCH_POINTER(w->chainGroup, ACTOR_COMMAND_MESSAGE_APPLY, &pos, 0);
    }
}

/// Shows a selected scene body or prop group together with its attachments.
///
/// `memberId`: 0 player, 1 Eve, 2 Kyle and hands/gun, 3 No. 9 and gunblade,
/// 4 chain group, 5 carrier; other values do nothing. Requires a published
/// cutscene and a live selected body/group. Only the optional gun and gunblade
/// are checked for absence.
static void _actor560800ShowCastMember(u32 memberId)
{
    _Actor560800CutsceneWork* work = D_actor_560800_8017578C->work;

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

void func_actor_560800_80133648(u32 arg0)
{
    _Actor560800CutsceneWork* work = D_actor_560800_8017578C->work;

    switch (arg0) {
        case 0:
            taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            break;
        case 1:
            taskMessageDispatch(work->eve, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            break;
        case 2:
            taskMessageDispatch(work->kyle, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            taskMessageDispatch(work->kyleGunHand, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            taskMessageDispatch(work->kyleFreeHand, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            if (work->kyleGun != NULL) {
                taskMessageDispatch(work->kyleGun, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            }
            break;
        case 3:
            taskMessageDispatch(work->no9, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            if (work->no9Gunblade != NULL) {
                taskMessageDispatch(work->no9Gunblade, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            }
            break;
        case 4:
            taskMessageDispatch(work->chainGroup, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            break;
        case 5:
            taskMessageDispatch(work->carrierModel, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            break;
    }
}

void func_actor_560800_80133750(s32 arg0)
{
    _Actor560800CutsceneWork* work;
    ActorTransform*           msg;

    work = D_actor_560800_8017578C->work;
    if (work->keepEffects == 0) {
        roomEffectRequestCancelAll();
    }
    if (work->player != NULL) {
        msg = D_actor_560800_8016F35C[arg0];
        if (msg->pos.vx != 0) {
            _actor560800ShowCastMember(ACTOR_560800_CAST_PLAYER);
            TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, msg, 0);
        } else {
            func_actor_560800_80133648(0);
        }
    }
    if (work->kyle != NULL) {
        msg = D_actor_560800_8016F46C[arg0];
        if (msg->pos.vx != 0) {
            _actor560800ShowCastMember(ACTOR_560800_CAST_KYLE);
            TASK_MESSAGE_DISPATCH_POINTER(work->kyle, ACTOR_MESSAGE_PLACE, msg, 0);
        } else {
            func_actor_560800_80133648(2);
        }
    }
    if (work->no9 != NULL) {
        msg = D_actor_560800_8016F3E4[arg0];
        if (msg->pos.vx != 0) {
            _actor560800ShowCastMember(ACTOR_560800_CAST_NO9);
            TASK_MESSAGE_DISPATCH_POINTER(work->no9, ACTOR_MESSAGE_PLACE, msg, 0);
        } else {
            func_actor_560800_80133648(3);
        }
    }
    if (work->eve != NULL) {
        msg = D_actor_560800_8016F4F4[arg0];
        if (msg->pos.vx != 0) {
            _actor560800ShowCastMember(ACTOR_560800_CAST_EVE);
            TASK_MESSAGE_DISPATCH_POINTER(work->eve, ACTOR_MESSAGE_PLACE, msg, 0);
        } else {
            func_actor_560800_80133648(1);
        }
    }
    if (work->chainGroup != NULL) {
        // No table of its own: reuses the payload picked for `eve`.
        if (msg->pos.vx != 0) {
            _actor560800ShowCastMember(ACTOR_560800_CAST_CHAINS);
            TASK_MESSAGE_DISPATCH_POINTER(work->chainGroup, ACTOR_MESSAGE_PLACE, msg, 0);
        } else {
            func_actor_560800_80133648(4);
        }
    }
}

static inline void Actor560800_PlayAnim(Task* task, u16 anim)
{
    _Actor560800CutsceneWork* work;
    AnimationPlayRequest      msg;

    work = task->work;
    if (work->player != NULL) {
        msg.source.sets          = D_actor_560800_8016EA40;
        work->playerAnimId       = anim;
        msg.animationId          = anim;
        msg.blend                = ANIMATION_BLEND_RESET;
        msg.blendFrames          = 0;
        msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
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
        request.source.sets          = D_actor_560800_8016EA40;
        work->playerAnimId           = animationId;
        request.animationId          = animationId;
        request.blend                = ANIMATION_BLEND_INTERPOLATE;
        request.blendFrames          = blendFrames;
        request.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &request, 0);
        work->playerAnimHold = 0;
    }
}

static inline void Actor560800_PlaySe(s16 arg4)
{
    s32 msg[5];
    s32 val;

    val    = gPlayerStatus.weapon;
    msg[0] = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? val + 1 : val + 0x22;
    msg[1] = arg4;
    msg[2] = 0;
    msg[3] = 0;
    msg[4] = 0;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, msg, 0);
}

/// Blends the player into an equipped-weapon clip over ten frames.
///
/// Requires a live player task and its loaded character/weapon animation bank.
/// `animationId` is an index in that bank. This enters scripted playback and
/// disables the player's world-collision participation; it does not update the
/// cutscene's scene-clip id or hold count.
static inline void _actor560800BlendPlayerWeaponAnimation(s32 animationId)
{
    enum {
        ACTOR_560800_PRIMARY_CHARACTER          = 1,
        ACTOR_560800_PRIMARY_WEAPON_BANK_BASE   = 1,
        ACTOR_560800_ALTERNATE_WEAPON_BANK_BASE = 0x22,
    };
    AnimationPlayRequest request;
    s32                  weaponId;

    weaponId                     = gPlayerStatus.weapon;
    request.source.index         = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACTOR_560800_PRIMARY_CHARACTER) ? weaponId + ACTOR_560800_PRIMARY_WEAPON_BANK_BASE : weaponId + ACTOR_560800_ALTERNATE_WEAPON_BANK_BASE;
    request.animationId          = animationId;
    request.blend                = ANIMATION_BLEND_INTERPOLATE;
    request.blendFrames          = ACTOR_560800_ANIMATION_BLEND_FRAMES;
    request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &request, 0);
}

static inline void Actor560800_SpawnSparksA(Task* task)
{
    _Actor560800CutsceneWork* work;
    SVECTOR                   vec;

    work   = task->work;
    vec.vx = 0x12C;
    vec.vy = 0;
    vec.vz = -0x1F4;
    effectSpawn(EFFECT_GROUND_DECAL, work->player->extra.tmd->coords, 0x20000040, &vec);
    vec.vx = 0x190;
    vec.vy = 0;
    vec.vz = -0x258;
    effectSpawn(EFFECT_GROUND_DECAL, work->player->extra.tmd->coords, 0x20000030, &vec);
    vec.vx = 0x12C;
    vec.vy = 0;
    vec.vz = -0x2BC;
    effectSpawn(EFFECT_GROUND_DECAL, work->player->extra.tmd->coords, 0x20000020, &vec);
    vec.vx = 0x1C2;
    vec.vy = 0;
    vec.vz = -0x320;
    effectSpawn(EFFECT_GROUND_DECAL, work->player->extra.tmd->coords, 0x20000020, &vec);
}

static inline void Actor560800_SpawnSparksB(Task* task)
{
    _Actor560800CutsceneWork* work;
    SVECTOR                   vec;

    work   = task->work;
    vec.vx = 0x12C;
    vec.vy = 0;
    vec.vz = -0xC8;
    effectSpawn(EFFECT_GROUND_DECAL, work->player->extra.tmd->coords, 0x20000040, &vec);
    vec.vx = 0x1F4;
    vec.vy = 0;
    vec.vz = -0x64;
    effectSpawn(EFFECT_GROUND_DECAL, work->player->extra.tmd->coords, 0x20000020, &vec);
    vec.vx = 0x1C2;
    vec.vy = 0;
    vec.vz = 0;
    effectSpawn(EFFECT_GROUND_DECAL, work->player->extra.tmd->coords, 0x20000020, &vec);
}

/// Requests driven by `playerCue.id`, cleared once handled: the inline helpers play
/// an animation on the task at `player` (0x3F4), post a sound through
/// `gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)` (0x3E8) or spawn the 0x60046 spark effects on its part
/// coordinates. 18 and 35 are two-step sequences on `playerCue.step` / `playerCue.counter`.
///
/// Shape notes, all needed for the match: helpers take only the arguments that
/// vary, because an inlined parameter is copied to a pseudo even when constant
/// and CSE would then share it; `gPlayerStatus.coordMtx` is read as a struct member so the load is
/// in-struct and schedules after the `playerCue.counter` store; the explicit clears in 19,
/// 28 and the last step of 35 decide which anim tails cross-jump together.
static void func_actor_560800_80133970(Task* arg0)
{
    _Actor560800CutsceneWork* work;

    work = arg0->work;
    func_actor_560800_80132340(arg0);
    switch (work->playerCue.id) {
        case 0:
        case 38:
            break;
        case 1:
            Actor560800_PlaySe(1);
            break;
        case 3:
            _actor560800BlendPlayerWeaponAnimation(7);
            break;
        case 9:
            _actor560800BlendPlayerAnimation(arg0, 0, ACTOR_560800_ANIMATION_BLEND_FRAMES);
            break;
        case 12:
            Actor560800_PlaySe(9);
            break;
        case 16:
            Actor560800_PlayAnim(arg0, 0xC);
            break;
        case 18:
            switch (work->playerCue.step) {
                case 0:
                    Actor560800_PlaySe(3);
                    taskMessageDispatch(work->player, ANIMATION_MESSAGE_SET_RATE, 8, 0);
                    work->playerCue.counter = 0;
                    work->playerCue.step++;
                    return;
                case 1:
                    if (work->playerCue.counter < 100) {
                        work->playerCue.counter      += 5;
                        gPlayerStatus.coordMtx->t[0] -= 5;
                        return;
                    }
                    _actor560800BlendPlayerAnimation(arg0, 0xC, ACTOR_560800_ANIMATION_BLEND_FRAMES);
                    break;
                default:
                    return;
            }
            break;
        case 19:
            Actor560800_PlayAnim(arg0, 1);
            work->playerCue.id = 0;
            return;
        case 21:
            Actor560800_PlayAnim(arg0, 3);
            Actor560800_SpawnSparksA(arg0);
            work->keepEffects = 1;
            break;
        case 28:
            Actor560800_PlayAnim(arg0, 4);
            work->playerCue.id = 0;
            return;
        case 29:
            Actor560800_SpawnSparksA(arg0);
            Actor560800_SpawnSparksB(arg0);
            work->keepEffects = 1;
            break;
        case 22:
        case 32:
            work->keepEffects = 0;
            break;
        case 33:
            Actor560800_SpawnSparksA(arg0);
            Actor560800_SpawnSparksB(arg0);
            work->keepEffects = 1;
            Actor560800_PlayAnim(arg0, 5);
            break;
        case 35:
            switch (work->playerCue.step) {
                case 0:
                    _actor560800BlendPlayerWeaponAnimation(8);
                    taskMessageDispatch(work->player, ANIMATION_MESSAGE_SET_RATE, 8, 0);
                    work->playerCue.counter = 0;
                    work->playerCue.step++;
                    return;
                case 1:
                    if (++work->playerCue.counter < 11) {
                        return;
                    }
                    _actor560800BlendPlayerAnimation(arg0, 0xC, 0x1E);
                    work->playerCue.id = 0;
                    return;
                default:
                    return;
            }
            break;
    }
    work->playerCue.id = 0;
}

/// Handles the pending request in `eveCue.id` and clears it: 1 and 28 store to
/// `part4Yaw` / `animPaused` in Eve's work block, which her task never reads,
/// 28 also restarts her slots in clip 3 at `ANIMATION_RATE_ONE`, and 22 / 24
/// send 0x7D5 to `chainGroup`.
static void func_actor_560800_80134258(Task* task)
{
    _Actor560800CutsceneWork* work;
    _Actor560800CastWork*     anim;
    _Actor560800CastWork*     ctx;
    _Actor560800CastWork*     ctx2;
    SVECTOR                   unused;
    u16                       i;
    u16                       rate;

    work = task->work;
    switch (work->eveCue.id) {
        case 0:
        case 38:
            break;
        case 1:
            ctx             = work->eve->work;
            ctx->part4Yaw   = 0;
            ctx->animPaused = 1;
            break;
        case 22:
        case 24:
            taskMessageDispatch(work->chainGroup, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            break;
        case 28:
            ctx2             = work->eve->work;
            ctx2->part4Yaw   = 0;
            ctx2->animPaused = 0;
            anim             = work->eve->work;
            i                = 1;
            anim->animId     = 3;
            rate             = ANIMATION_RATE_ONE;
            anim->animRate   = rate;
            anim->animHold   = 0;
            if (i < anim->slotCount) {
                do {
                    anim->rig.slots[i].rate = rate;
                    animationResetSlot(&anim->rig.anim, i, 3);
                    i++;
                } while (i < anim->slotCount);
            }
            break;
    }
    work->eveCue.id = 0;
}

static inline void Actor560800_ResetAnimSlots(_Actor560800CastWork* anim, s16 clip)
{
    u16 i;
    u16 rate;

    anim->animId   = clip;
    i              = 1;
    rate           = ANIMATION_RATE_ONE;
    anim->animRate = rate;
    anim->animHold = 0;
    if (i < anim->slotCount) {
        do {
            anim->rig.slots[i].rate = rate;
            animationResetSlot(&anim->rig.anim, i, clip);
            i++;
        } while (i < anim->slotCount);
    }
}

static void func_actor_560800_80134384(Task* task)
{
    _Actor560800CutsceneWork* work;
    _Actor560800CastWork*     anim;
    u16                       i;
    u16                       rate;

    work = task->work;
    switch (work->no9Cue.id) {
        case 0:
        case 38:
            break;
        case 2:
            Actor560800_ResetAnimSlots(work->no9->work, 1);
            break;
        case 4:
            Actor560800_ResetAnimSlots(work->no9->work, 5);
            break;
        case 6:
            Actor560800_ResetAnimSlots(work->no9->work, 0xc);
            break;
        case 8:
            Actor560800_ResetAnimSlots(work->no9->work, 0x28);
            break;
        case 10:
            Actor560800_ResetAnimSlots(work->no9->work, 0x17);
            break;
        case 11:
            Actor560800_ResetAnimSlots(work->no9->work, 0x18);
            break;
        case 13:
            Actor560800_ResetAnimSlots(work->no9->work, 0x19);
            break;
        case 15:
            Actor560800_ResetAnimSlots(work->no9->work, 0xc);
            break;
        case 17:
            Actor560800_ResetAnimSlots(work->no9->work, 0x29);
            break;
        case 22:
            Actor560800_ResetAnimSlots(work->no9->work, 0x1f);
            break;
        case 23:
            Actor560800_ResetAnimSlots(work->no9->work, 0x1d);
            break;
        case 24:
            switch (work->no9Cue.step) {
                case 0:
                    anim           = work->no9->work;
                    anim->animId   = 0x2D;
                    i              = 1;
                    rate           = ANIMATION_RATE_ONE;
                    anim->animRate = rate;
                    anim->animHold = 0;
                    if (i >= anim->slotCount) {
                        work->no9Cue.counter = 0;
                        work->no9Cue.step++;
                        return;
                    }
                    for (;;) {
                        anim->rig.slots[i].rate = rate;
                        animationResetSlot(&anim->rig.anim, i, 0x2D);
                        i++;
                        if (i < anim->slotCount) {
                            continue;
                        }
                        work->no9Cue.counter = 0;
                        work->no9Cue.step++;
                        return;
                    }
                case 1:
                    if (++work->no9Cue.counter < 0xB5) {
                        return;
                    }
                    _actor560800BlendCastAnimation(work->no9, 0x1D, ANIMATION_RATE_ONE);
                    break;
                default:
                    return;
            }
            break;
        case 26:
            Actor560800_ResetAnimSlots(work->no9->work, 0x21);
            ((_Actor560800CastWork*)work->no9->work)->floorQuadHidden = 1;
            break;
        case 27:
            Actor560800_ResetAnimSlots(work->no9->work, 0x24);
            break;
    }
    work->no9Cue.id = 0;
}

/// Restarts Kyle's animation slots in clip 0x20 -- writing `animRate` and every
/// slot's `rate` with `ANIMATION_RATE_ONE` -- then spawns
/// effect 0x6002B on the ninth per-part coordinate of the task at `kyle`
/// and posts a brief full-strength pulse to port 0's variable vibration motor.
///
/// The rate is held in a local rather than written as two literals: both uses
/// have to reach the same register, and the rate is live across the loop's
/// `animationResetSlot` call. `unused` is declared and never referenced - the
/// ROM's frame is 0x30 and the local is what reserves its 8 bytes.
void func_actor_560800_80134B14(s32 arg0)
{
    _Actor560800CutsceneWork* work;
    _Actor560800CastWork*     anim;
    SVECTOR                   unused;
    u16                       i;
    u16                       rate;

    work = D_actor_560800_8017578C->work;
    anim = work->kyle->work;

    anim->animId   = 0x20;
    rate           = ANIMATION_RATE_ONE;
    anim->animRate = rate;
    anim->animHold = 0;
    i              = 1;
    if (i < anim->slotCount) {
        do {
            anim->rig.slots[i].rate = rate;
            animationResetSlot(&anim->rig.anim, i, 0x20);
            i++;
        } while (i < anim->slotCount);
    }
    effectSpawn(EFFECT_HANDGUN_MUZZLE_FLASH, &work->kyle->extra.tmd->coords[8], 0x21, NULL);
    padPostVibrationRequest(0, PAD_VIBRATION_MOTOR_VARIABLE, PAD_VIBRATION_INTENSITY_MAX, 2);
}

static inline void Actor560800_BlendSlotsFirst(Task* task, u16 id, s16 rate)
{
    _Actor560800CastWork* w;
    u16                   i;
    u32                   first;
    u32                   count;

    w           = task->work;
    w->animId   = id;
    w->animRate = rate;
    _actor560800ResetAnimHold(w);
    count = w->slotCount;
    __asm__("" : "=r"(first) : "0"((u16)1));
    if (first < count) {
        i = 1;
        do {
            animationSeekSlotWithBlend(&w->rig.anim, i, id, 0, 10);
            i++;
        } while (i < w->slotCount);
    }
}

static inline void Actor560800_ResetSlots(Task* task, u16 id, u16 rate)
{
    _Actor560800CastWork* anim;
    u16                   i;

    anim           = task->work;
    i              = 1;
    anim->animId   = id;
    anim->animRate = rate;
    anim->animHold = 0;
    if (i < anim->slotCount) {
        do {
            anim->rig.slots[i].rate = rate;
            animationResetSlot(&anim->rig.anim, i, id);
            i++;
        } while (i < anim->slotCount);
    }
}

static void func_actor_560800_80134BFC(Task* arg0)
{
    _Actor560800CutsceneWork* work;
    _Actor560800CastWork*     ctx;
    _Actor560800CastWork*     ctx2;
    _Actor560800CastWork*     ctx3;
    _Actor560800CastWork*     ctx4;
    _Actor560800CastWork*     ctx5;
    _Actor560800CastWork*     ctx6;
    _Actor560800CastWork*     ctx7;
    _Actor560800CastWork*     blend;
    _Actor560800CastWork*     anim;
    GfxCoord*                 coord;
    u32                       first;
    u32                       count;
    u16                       i;

    work = arg0->work;
    switch (work->kyleCue.id) {
        case 0:
            break;
        case 1:
            ((_Actor560800CastWork*)work->kyle->work)->animPaused = 1;
            break;
        case 2:
            ((_Actor560800CastWork*)work->kyle->work)->part4Yaw = 0x155;
            break;
        case 4:
            switch (work->kyleCue.step) {
                case 0: {
                    _Actor560800CastWork* reseed;

                    ctx              = work->kyle->work;
                    ctx->part4Yaw    = 0;
                    ctx->animPaused  = 0;
                    reseed           = work->kyle->work;
                    reseed->animId   = 1;
                    reseed->animRate = ANIMATION_RATE_ONE;
                    reseed->animHold = 0;
                    _ACTOR560800_BLEND_SLOTS(reseed, 1, 10);
                    work->kyleCue.step++;
                    return;
                }
                case 1:
                    ((_Actor560800CastWork*)work->kyle->work)->animPaused = 1;
                    break;
                default:
                    return;
            }
            break;
        case 14:
            switch (work->kyleCue.step) {
                case 0:
                    Actor560800_ResetSlots(work->kyle, 0x19, ANIMATION_RATE_ONE);
                    work->kyleCue.step++;
                    return;
                case 1:
                    coord              = work->kyle->extra.tmd->coords;
                    coord->coord.t[0] -= 0x1E;
                    if (work->kyle->extra.tmd->coords->coord.t[0] < D_actor_560800_8016F1CC[5].pos.vx) {
                        TASK_MESSAGE_DISPATCH_POINTER(work->kyle, ACTOR_MESSAGE_PLACE, &D_actor_560800_8016F1CC[5], 0);
                        Actor560800_BlendSlotsFirst(work->kyle, 0x1A, ANIMATION_RATE_ONE);
                        work->kyleCue.id = 0;
                    }
                    work->kyle->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                    return;
                default:
                    return;
            }
            break;
        case 15:
            Actor560800_ResetSlots(work->kyle, 5, ANIMATION_RATE_ONE);
            break;
        case 18:
            Actor560800_ResetSlots(work->kyle, 0x1F, ANIMATION_RATE_ONE);
            break;
        case 20:
            switch (work->kyleCue.step) {
                case 0:
                    ((_Actor560800CastWork*)work->kyle->work)->relit = 1;
                    _actor560800BlendPlayerWeaponAnimation(3);
                    taskMessageDispatch(work->player, ANIMATION_MESSAGE_SET_RATE, 8, 0);
                    work->kyleCue.counter = 0;
                    work->kyleCue.step++;
                    return;
                case 1:
                    if (work->kyleCue.counter < 300) {
                        work->kyleCue.counter        += 5;
                        gPlayerStatus.coordMtx->t[0] -= 5;
                        return;
                    }
                    _actor560800BlendPlayerAnimation(arg0, 2, ACTOR_560800_ANIMATION_BLEND_FRAMES);
                    work->kyleCue.counter = 0;
                    work->kyleCue.step++;
                    return;
                case 2:
                    if (++work->kyleCue.counter < 11) {
                        return;
                    }
                    func_actor_560800_80134B14(0);
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
            ((_Actor560800CastWork*)work->kyle->work)->relit = 0;
            Actor560800_ResetSlots(work->kyle, 0xA, ANIMATION_RATE_ONE);
            break;
        case 22:
            Actor560800_ResetSlots(work->kyle, 0xB, ANIMATION_RATE_ONE);
            break;
        case 23:
            taskMessageDispatch(work->kyle, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            taskMessageDispatch(work->kyleGunHand, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            taskMessageDispatch(work->kyleFreeHand, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            if (work->kyleGun != NULL) {
                taskMessageDispatch(work->kyleGun, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            }
            break;
        case 24:
            taskMessageDispatch(work->kyle, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            taskMessageDispatch(work->kyleGunHand, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            taskMessageDispatch(work->kyleFreeHand, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            if (work->kyleGun != NULL) {
                taskMessageDispatch(work->kyleGun, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            }
            ctx2           = work->kyle->work;
            ctx2->part4Yaw = 0x155;
            ctx2->relit    = 1;
            break;
        case 25:
            ctx3            = work->kyle->work;
            ctx3->part4Yaw  = 0;
            ctx3->part2Roll = -0x71;
            Actor560800_ResetSlots(work->kyle, 0x1F, ANIMATION_RATE_ONE);
            break;
        case 26:
            ctx4            = work->kyle->work;
            ctx4->part2Roll = 0;
            ctx4->relit     = 0;
            Actor560800_ResetSlots(work->kyle, 0xD, ANIMATION_RATE_ONE);
            break;
        case 28:
            Actor560800_ResetSlots(work->kyle, 0x1A, ANIMATION_RATE_ONE);
            break;
        case 30:
            ctx5              = work->kyle->work;
            ctx5->part4Pitch -= 0x1E;
            if (ctx5->part4Pitch >= -0x155) {
                return;
            }
            break;
        case 32:
            ((_Actor560800CastWork*)work->kyle->work)->part4Pitch = 0;
            Actor560800_ResetSlots(work->kyle, 0xF, ANIMATION_RATE_ONE);
            break;
        case 33:
            if ((u32)D_actor_560800_801757A4 > (u32)gDisplayState.frameCount) {
                D_actor_560800_80175798 = gDisplayState.frameCount - (D_actor_560800_801757A4 + 1);
            } else {
                D_actor_560800_80175798 = gDisplayState.frameCount - D_actor_560800_801757A4;
            }
            Actor560800_ResetSlots(work->kyle, 0x10, ANIMATION_RATE_ONE);
            break;
        case 35:
            switch (work->kyleCue.step) {
                case 0: {
                    _Actor560800CastWork* reseed;

                    reseed           = work->kyle->work;
                    reseed->animId   = 7;
                    reseed->animRate = ANIMATION_RATE_ONE;
                    reseed->animHold = 0;
                    _ACTOR560800_BLEND_SLOTS(reseed, 7, 10);
                    work->kyleCue.counter = 0;
                    work->kyleCue.step++;
                    return;
                }
                case 1:
                    if (++work->kyleCue.counter < 0x5B) {
                        return;
                    }
                    Actor560800_BlendSlotsFirst(work->kyle, 0x14, ANIMATION_RATE_ONE);
                    break;
                default:
                    return;
            }
            break;
        case 36:
            switch (work->kyleCue.step) {
                case 0:
                    Actor560800_ResetSlots(work->kyle, 0xE, ANIMATION_RATE_ONE);
                    work->kyleCue.counter = 0;
                    work->kyleCue.step++;
                    return;
                case 1:
                    if (++work->kyleCue.counter < 0x1F) {
                        return;
                    }
                    padPostVibrationRequest(0, PAD_VIBRATION_MOTOR_VARIABLE, PAD_VIBRATION_INTENSITY_MAX, 2);
                    effectSpawn(EFFECT_HANDGUN_MUZZLE_FLASH, &work->kyle->extra.tmd->coords[8], 0x21, NULL);
                    blend           = work->no9->work;
                    blend->animId   = 0x20;
                    blend->animRate = ANIMATION_RATE_ONE / 2;
                    blend->animHold = 0;
                    SOFT_BARRIER();
                    count = blend->slotCount;
                    SOFT_BARRIER();
                    __asm__("" : "=r"(first) : "0"((u16)1));
                    if (first < count) {
                        i = 1;
                        do {
                            animationSeekSlotWithBlend(&blend->rig.anim, i, 0x20, 0, 5);
                            i++;
                        } while (i < blend->slotCount);
                    }
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
        case 37:
            ((_Actor560800CastWork*)work->kyle->work)->animPaused = 0;
            ctx6                                                  = work->kyle->work;
            ctx6->part4Yaw                                       -= 0x3C;
            if (ctx6->part4Yaw >= -0x200) {
                return;
            }
            break;
        case 38:
            ctx7 = work->kyle->work;
            switch (work->kyleCue.step) {
                case 0:
                    anim = work->kyle->work;
                    SOFT_TOUCH_REG(anim);
                    anim->animId   = 3;
                    anim->animRate = ANIMATION_RATE_ONE;
                    anim->animHold = 0;
                    _ACTOR560800_BLEND_SLOTS(anim, 3, 10);
                    work->kyleCue.step++;
                    return;
                case 1:
                    ctx7->part4Yaw += 0x3C;
                    if (ctx7->part4Yaw < 0) {
                        return;
                    }
                    ctx7->part4Yaw = 0;
                    break;
                default:
                    return;
            }
            break;
        case 39:
            Actor560800_ResetSlots(work->kyle, 0x22, ANIMATION_RATE_ONE / 2);
            break;
    }
    work->kyleCue.id = 0;
}

void func_actor_560800_80135AEC(s32 arg0)
{
    if (arg0 == 1) {
        if ((u32)D_actor_560800_8017579C > (u32)gDisplayState.frameCount) {
            D_actor_560800_80175790 = gDisplayState.frameCount - (D_actor_560800_8017579C + 1);
        } else {
            D_actor_560800_80175790 = gDisplayState.frameCount - D_actor_560800_8017579C;
        }
    } else if (arg0 == 2) {
        if ((u32)D_actor_560800_801757A0 > (u32)gDisplayState.frameCount) {
            D_actor_560800_80175794 = gDisplayState.frameCount - (D_actor_560800_801757A0 + 1);
        } else {
            D_actor_560800_80175794 = gDisplayState.frameCount - D_actor_560800_801757A0;
        }
    } else if (arg0 == 3) {
        if ((u32)D_actor_560800_801757A4 > (u32)gDisplayState.frameCount) {
            D_actor_560800_80175798 = gDisplayState.frameCount - (D_actor_560800_801757A4 + 1);
        } else {
            D_actor_560800_80175798 = gDisplayState.frameCount - D_actor_560800_801757A4;
        }
    }
    CdCmd_CancelReplaceAndActivate();
    streamFinishScene();
}

static void func_actor_560800_80135BD8(Task* arg0)
{
    _Actor560800CutsceneWork* work;
    Task*                     sub5;
    Task*                     sub6;
    SVECTOR                   vec;

    work       = memMalloc(sizeof(*work), false);
    arg0->work = work;
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    memFillBytes(work, 0, sizeof(*work));
    work->player            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    D_actor_560800_8017578C = arg0;
    work->eve               = taskSpawnFromTable(D_actor_560800_801718F0, 4, 0, 0);
    sub5                    = taskSpawnFromTable(D_actor_560800_801718F0, 5, 0, 0);
    work->kyle              = sub5;
    work->kyleGunHand       = taskSpawnFromTable(D_actor_560800_801718F0, 7, 1, sub5);
    work->kyleFreeHand      = taskSpawnFromTable(D_actor_560800_801718F0, 8, 0, work->kyle);
    work->kyleGun           = taskSpawnFromTable(D_actor_560800_801718F0, 9, 2, work->kyle);
    sub6                    = taskSpawnFromTable(D_actor_560800_801718F0, 6, 0, 0);
    work->no9               = sub6;
    work->no9Gunblade       = taskSpawnFromTable(D_actor_560800_801718F0, 0xA, 3, sub6);
    work->chainGroup        = taskSpawnFromTable(D_actor_560800_8017575C, 0, 0, arg0);
    work->carrierModel      = taskSpawnFromTable(D_actor_560800_8017575C, 2, 0, arg0);
    vec.vx                  = 0x5A0;
    vec.vy                  = 0x5A0;
    vec.vz                  = 0x5A0;
    worldCoordSetAmbientColorOverride(&vec);
}

void func_actor_560800_80135D54(Task* arg0)
{
    s32                       msg[5];
    _Actor560800CutsceneWork* work;
    s32                       val;

    switch (arg0->state) {
        case 0:
            if (Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                return;
            }
            func_actor_560800_80135BD8(arg0);
            Gp_CapFile = 0;
            Gp_LoadCapFile(0);
            capSetTexturePage(0x180, 0);
            arg0->state++;
        case 1:
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
            evsStartScriptWithSkip(D_actor_560800_8016F5E0, EVENT_SCRIPT_HUD_KEEP, D_actor_560800_80171800);
            arg0->state++;
            break;
        case 2:
            if (gGameSession->eventState == 0) {
                gRandomLcgState = D_actor_560800_801757A8;
                roomEffectRequestCancelAll();
                val    = gPlayerStatus.weapon;
                msg[0] = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? val + 1 : val + 0x22;
                msg[1] = 1;
                msg[2] = 0;
                msg[3] = 0;
                msg[4] = 0;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, msg, 0);
                taskRequestKill(arg0, 0);
                return;
            }
            break;
    }
    func_actor_560800_80133970(arg0);
    func_actor_560800_80134258(arg0);
    func_actor_560800_80134384(arg0);
    func_actor_560800_80134BFC(arg0);
    work = arg0->work;
    switch (work->sceneCue.id) {
        case 0:
            break;
        case 1:
            if (work->eve != NULL) {
                taskKill(work->eve);
            }
            Display_SpawnWithOt(D_actor_560800_801718F0, 0xC, 0, 0);
            break;
    }
    work->sceneCue.id = 0;
}

void func_actor_560800_80135F50(Task* arg0)
{
    Display_SpawnWithOt(D_actor_560800_8016EA28, 1, arg0->spawnArg1.value, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    Gp_SpawnViewTasks();
    taskKill(arg0);
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
    enum { ACTOR_560800_FADE_INITIALIZE    = 0,
           ACTOR_560800_FADE_RAMP          = 1,
           ACTOR_560800_FADE_END_INTENSITY = 256 };
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
            taskReparent(D_actor_560800_8017578C, task);
            task->state += 1;
            /* fallthrough */
        case ACTOR_560800_FADE_RAMP:
            fadeDrawOverlay(fade->r, fade->g, fade->r, GPU_BLEND_SUBTRACT);
            fade->r += task->spawnArg1.halves.low;
            fade->g += task->spawnArg1.halves.low;
            fade->b += task->spawnArg1.halves.low;
            if (fade->r >= ACTOR_560800_FADE_END_INTENSITY) {
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
           ACTOR_560800_FADE_RAMP           = 7,
           ACTOR_560800_FADE_MAX_INTENSITY  = 255 };
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
            fade->b = ACTOR_560800_FADE_MAX_INTENSITY;
            fade->g = ACTOR_560800_FADE_MAX_INTENSITY;
            fade->r = ACTOR_560800_FADE_MAX_INTENSITY;
            taskReparent(D_actor_560800_8017578C, task);
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
    fade->r -= task->spawnArg1.halves.low;
    fade->g -= task->spawnArg1.halves.low;
    fade->b -= task->spawnArg1.halves.low;
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

#include "../../shared/actor_messages_place_ypr.inc.c"

/// Spawns entry 2 of the actor's task descriptor table with `arg0` as its
/// spawn argument. Script tables in the actor's data call it.
void func_actor_560800_80136280(s32 arg0)
{
    taskSpawnFromTable(D_actor_560800_801718F0, 2, arg0, 0);
}

void func_actor_560800_801362B0(s32 arg0)
{
    taskSpawnFromTable(D_actor_560800_801718F0, 3, arg0, 0);
}

void func_actor_560800_801362E0(s16 arg0)
{
    _Actor560800CutsceneWork* work = D_actor_560800_8017578C->work;
    ActorCommand              msg;

    msg.command = arg0;
    TASK_MESSAGE_DISPATCH_POINTER(work->chainGroup, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
}

void func_actor_560800_8013631C(s16 arg0)
{
    _Actor560800CutsceneWork* work = D_actor_560800_8017578C->work;
    ActorCommand              msg;

    msg.command = arg0;
    TASK_MESSAGE_DISPATCH_POINTER(work->carrierModel, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
}

void func_actor_560800_80136358(s16 arg0)
{
    _Actor560800CutsceneWork* work = D_actor_560800_8017578C->work;

    work->playerCue.id   = arg0;
    work->playerCue.step = 0;
}

/// Latches the animation id in `playerAnimId` and plays that animation on the
/// task at `player`: message 0x3F4 with `field_8` 1, `field_C` 0xA and
/// `field_10` 1. The zero-extended id goes into the message while the store
/// keeps the raw halfword argument, so the two uses do not share a register.
void func_actor_560800_80136378(s16 arg0)
{
    _Actor560800CutsceneWork* work;
    AnimationPlayRequest      msg;
    u16                       anim;

    work = D_actor_560800_8017578C->work;
    if (work->player != NULL) {
        anim                     = arg0;
        msg.source.sets          = D_actor_560800_8016EA40;
        work->playerAnimId       = arg0;
        msg.animationId          = anim;
        msg.blend                = ANIMATION_BLEND_INTERPOLATE;
        msg.blendFrames          = 0xA;
        msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
        work->playerAnimHold = 0;
    }
}

/// The same animation reseed as `func_actor_560800_801364A0`, reached through
/// `eve` instead of `no9`: the id goes to `animId` with `ANIMATION_RATE_ONE`
/// in `animRate`, `animHold` is cleared, and slots 1..`slotCount` - 1 are
/// blended through `animationSeekSlotWithBlend`.
void func_actor_560800_801363F8(u16 arg0)
{
    _Actor560800CutsceneWork* work;
    _Actor560800CastWork*     anim;

    work = D_actor_560800_8017578C->work;
    anim = work->eve->work;

    anim->animId   = arg0;
    anim->animRate = ANIMATION_RATE_ONE;
    anim->animHold = 0;
    _ACTOR560800_BLEND_SLOTS(anim, arg0, 10);
}

/// Reseeds the animation slots of the sub-task at `no9` from `arg0`: the
/// id goes to `animId` with `ANIMATION_RATE_ONE` in `animRate`,
/// `animHold` is cleared, and slots 1..`slotCount` - 1 are blended through
/// `animationSeekSlotWithBlend`. `func_actor_560800_801363F8` is the same body reached
/// through `eve`.
void func_actor_560800_801364A0(u16 arg0)
{
    _Actor560800CutsceneWork* work;
    _Actor560800CastWork*     anim;

    work = D_actor_560800_8017578C->work;
    anim = work->no9->work;

    anim->animId   = arg0;
    anim->animRate = ANIMATION_RATE_ONE;
    anim->animHold = 0;
    _ACTOR560800_BLEND_SLOTS(anim, arg0, 10);
}

/// Copies a 64x256 strip of VRAM to (0x280, 0x100), then re-loads the chunk at
/// `D_8006C338[35].data` with `D5B498_8006C234` set to 5 for the duration (that byte is
/// the image mode `Fs_LoadImageChunk` reads for chunks whose second halfword is
/// in 0xF5..0xFF), restoring it to 0 afterwards.
void func_actor_560800_80136548(void)
{
    RECT rect;

    rect.x = 0x3C0;
    rect.y = 0;
    rect.w = 0x40;
    rect.h = 0x100;
    MoveImage(&rect, 0x280, 0x100);
    D5B498_8006C234 = 5;
    Fs_LoadImageChunk(D_8006C338[35].data, 1);
    D5B498_8006C234 = 0;
}

void func_actor_560800_801365B0(s16 arg0)
{
    _Actor560800CutsceneWork* work = D_actor_560800_8017578C->work;

    work->kyleCue.id   = arg0;
    work->kyleCue.step = 0;
}

void func_actor_560800_801365D0(u16 arg0)
{
    _Actor560800CutsceneWork* work;
    _Actor560800CastWork*     anim;

    work = D_actor_560800_8017578C->work;
    anim = work->kyle->work;

    anim->animId   = arg0;
    anim->animRate = ANIMATION_RATE_ONE;
    anim->animHold = 0;
    _ACTOR560800_BLEND_SLOTS(anim, arg0, 10);
}

void func_actor_560800_80136678(s32 arg0)
{
    sndEvtRequestScriptStart(D_actor_560800_8016F57C[arg0], 0, 0);
}

/// Task state handler for the second spawn mode: states 1 and 2 — and state 0,
/// which first parks `gDisplayState.control.flags.flipMode` at 2 — only step the state, and state 3 runs
/// the hand-off. That hand-off copies a 64x256 VRAM strip from (0x380, 0) to
/// (0x200, 0x100), the same shape `func_actor_560800_80136548` uses for the
/// other strip, then re-loads the chunk at `D_8006C338[36].data` with
/// `D5B498_8006C234` at 8 for the duration, kills this task, restores session
/// image memory and game-loop presentation, and spawns `D_actor_560800_801718F0` index 0xB into the work
/// block's `eve`. Like `func_actor_310100_801620FC`, state 3 hands the
/// finished work over rather than leaving the task alive.
void func_actor_560800_801366B0(Task* arg0)
{
    RECT                      rect;
    _Actor560800CutsceneWork* work;

    work = D_actor_560800_8017578C->work;
    switch (arg0->state) {
        case 0:
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
            /* fallthrough */
        case 1:
        case 2:
            arg0->state++;
            return;
        case 3:
            rect.x = 0x380;
            rect.y = 0;
            rect.w = 0x40;
            rect.h = 0x100;
            MoveImage(&rect, 0x200, 0x100);
            D5B498_8006C234 = 8;
            Fs_LoadImageChunk(D_8006C338[36].data, 1);
            D5B498_8006C234 = 0;
            taskKill(arg0);
            displayResumeGameLoop();
            work->eve = taskSpawnFromTableOnDefaultList(D_actor_560800_801718F0, 0xB, 1, D_actor_560800_8017578C);
            break;
    }
}

void func_actor_560800_801367C0(s16 arg0)
{
    _Actor560800CutsceneWork* work = D_actor_560800_8017578C->work;

    work->sceneCue.id   = arg0;
    work->sceneCue.step = 0;
}

void func_actor_560800_801367E0(s16 arg0)
{
    _Actor560800CutsceneWork* work = D_actor_560800_8017578C->work;

    work->playerCue.id   = arg0;
    work->playerCue.step = 0;
    work->eveCue.id      = arg0;
    work->eveCue.step    = 0;
    work->no9Cue.id      = arg0;
    work->no9Cue.step    = 0;
    work->kyleCue.id     = arg0;
    work->kyleCue.step   = 0;
}

void func_actor_560800_80136818(void)
{
    _Actor560800CutsceneWork* work = D_actor_560800_8017578C->work;
    PlayerStatus*             cfg  = &gPlayerStatus;
    s16                       hp;

    Gp_KillPlayerEffs();

    if (cfg->hp < 0x33) {
        hp = 1;
    } else {
        hp = (u16)cfg->hp - 0x32;
    }
    do {
        cfg->hp                 = hp;
        work->shotDamageApplied = 1;
    } while (0);
}

/// Clears the four actors' pending cues, and the first time it runs
/// (`shotDamageApplied` still zero) kills the player effects and drops the current HP by 50,
/// then latches `shotDamageApplied`. Ends by pulsing gameplay state 0x1C, cancelling the
/// pending CD command and blanking the display.
void func_actor_560800_80136878(void)
{
    _Actor560800CutsceneWork* work = D_actor_560800_8017578C->work;
    s16                       hp;

    work->playerCue.id = 0;
    work->kyleCue.id   = 0;
    work->no9Cue.id    = 0;
    work->eveCue.id    = 0;
    if (work->shotDamageApplied == 0) {
        PlayerStatus*             cfg   = &gPlayerStatus;
        _Actor560800CutsceneWork* work2 = D_actor_560800_8017578C->work;

        Gp_KillPlayerEffs();
        if (cfg->hp < 0x33) {
            hp = 1;
        } else {
            hp = (u16)cfg->hp - 0x32;
        }
        do {
            cfg->hp                  = hp;
            work2->shotDamageApplied = 1;
        } while (0);
    }
    roomEffectRequestCancelAll();
    CdCmd_CancelReplaceAndActivate();
    SetDispMask(0);
}

/// Script callback in the actor's data tables: queues the replacement of
/// overlay 0x82.
void func_actor_560800_80136910(void)
{
    cdCmdStageSceneAudioStart();
}

void func_actor_560800_80136930(s32 arg0)
{
    if (arg0 == 1) {
        D_actor_560800_8017579C = gDisplayState.frameCount;
    } else if (arg0 == 2) {
        D_actor_560800_801757A0 = gDisplayState.frameCount;
    } else if (arg0 == 3) {
        D_actor_560800_801757A4 = gDisplayState.frameCount;
    }
    cdCmdEnqueueScenePlayback();
}

void func_actor_560800_801369A0(void)
{
    Display_SpawnWithOt(D_actor_560800_801718F0, 0xD, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
    Gp_SpawnViewTasks();
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

void func_actor_560800_80136A20(void)
{
    Gp_CapFile = 0;
    Gp_LoadCapFile(1);
    capSetTexturePage(0x180, 0);
}

void func_actor_560800_80136A54(void)
{
    Gp_CapFile = 0;
    Gp_LoadCapFile(2);
    capSetTexturePage(0x180, 0);
}

/// Immediately kills the scene task-table's bodyless placeholder task.
///
/// Installed at index 1; owns no work and ignores both spawn arguments.
static void _actor560800DiscardTask(Task* task)
{
    taskKill(task);
}

/// Bends a chain toward its group's target: while parts 1..5 are walked on the
/// scratchpad stack, each part's X rotation steps by the local `speed` toward
/// the bearing of the group's `targetWorld` translation seen from the part's
/// joint, and the joint positions accumulate through the GTE.
/// Part 0's X rotation oscillates on a `D_actor_560800_801752E8` phase. The
/// closing switch drives `dipStep`: close enough in Y/Z starts the dip in
/// `offset.vy`, which then returns to zero.
static void func_actor_560800_80136AA8(Task* arg0)
{
    _Actor560800ChainScratch*   top;
    _Actor560800PropWork*       work;
    _Actor560800ChainScratch*   s;
    _Actor560800ChainGroupWork* group;
    s16                         i;
    s16                         speed;
    s32                         a;

    top  = SCRATCH_STACK_CURSOR(_Actor560800ChainScratch);
    work = arg0->work;
    s = SCRATCH_STACK_CURSOR(_Actor560800ChainScratch) = top - 1;
    group                                              = work->parent->work;
    memFillBytes(s, 0, sizeof(_Actor560800ChainScratch));
    memCopyBytes(work->rot, s->rot, sizeof(s->rot));
    if (work->chainNumber & 1) {
        speed = 2;
        switch (((D_actor_560800_801752E8 + work->swayPhase) * 2) & 0x300) {
            case 0x0:
            case 0x300:
                s->rot[0].vx += 4;
                s->rot[0].vx %= 0x1000;
                break;
            case 0x100:
            case 0x200:
                s->rot[0].vx -= 4;
                if (s->rot[0].vx < 0) {
                    s->rot[0].vx += 0x1000;
                }
                break;
        }
    } else {
        speed = 1;
        switch (((D_actor_560800_801752E8 + work->swayPhase) * 2) & 0x700) {
            case 0x0:
            case 0x100:
            case 0x600:
            case 0x700:
                s->rot[0].vx += 2;
                s->rot[0].vx %= 0x1000;
                break;
            case 0x200:
            case 0x300:
            case 0x400:
            case 0x500:
                s->rot[0].vx -= 2;
                if (s->rot[0].vx < 0) {
                    s->rot[0].vx += 0x1000;
                }
                break;
        }
    }
    s->pos.vx = arg0->extra.tmd->coords->parent->coord.t[0] + arg0->extra.tmd->coords->coord.t[0];
    s->pos.vy = arg0->extra.tmd->coords->parent->coord.t[1] + arg0->extra.tmd->coords->coord.t[1];
    s->pos.vz = arg0->extra.tmd->coords->parent->coord.t[2] + arg0->extra.tmd->coords->coord.t[2];
    s->ang.vx = s->rot[0].vx;
    s->ang.vy = s->rot[0].vy;
    s->ang.vz = s->rot[0].vz;
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, s->rot[0].vy, 1);
    gfxRotMatrixX(&arg0->extra.tmd->coords->coord, s->rot[0].vx + 0x400, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixZ(&arg0->extra.tmd->coords->coord, s->rot[0].vz, GRAPHICS_ROTATION_COMPOSE);
    s->link  = arg0->extra.tmd->coords->coord;
    s->chain = s->link;
    gte_SetRotMatrix(&s->chain);
    for (i = 1; i < 6; i++) {
        gte_ldclmv(&arg0->extra.tmd->coords[i].coord);
        gte_rtir();
        gte_stclmv(&s->link);
        gte_ldclmv(&arg0->extra.tmd->coords[i].coord.m[0][1]);
        gte_rtir();
        gte_stclmv(&s->link.m[0][1]);
        gte_ldclmv(&arg0->extra.tmd->coords[i].coord.m[0][2]);
        gte_rtir();
        gte_stclmv(&s->link.m[0][2]);
        s->joint.vx = arg0->extra.tmd->coords[i + 1].coord.t[0];
        s->joint.vy = arg0->extra.tmd->coords[i + 1].coord.t[1];
        s->joint.vz = arg0->extra.tmd->coords[i + 1].coord.t[2];
        gte_SetRotMatrix(&s->link);
        gte_ldv0(&s->joint);
        gte_rtv0();
        gte_stsv(&s->joint);
        s->joint.vx += s->pos.vx;
        s->joint.vy += s->pos.vy;
        s->joint.vz += s->pos.vz;
        s->aim.vx    = (s->ang.vx + s->rot[i].vx) % 0x1000;
        s->aim.vy    = (s->ang.vy + s->rot[i].vy) % 0x1000;
        s->aim.vz    = (s->ang.vz + s->rot[i].vz) % 0x1000;
        if (s->rot[0].vy == 0) {
            a = ratan2(s->joint.vy - group->targetWorld.t[1], group->targetWorld.t[2] - s->joint.vz) % 0x1000;
            if (a < 0) {
                a += 0x1000;
            }
            s->aim.vx -= a;
            if (s->aim.vx < 0) {
                s->aim.vx += 0x1000;
            }
            if (s->aim.vx < 0x800) {
                s->rot[i].vx += speed;
                s->rot[i].vx %= 0x1000;
            } else {
                s->rot[i].vx -= speed;
                if (s->rot[i].vx < 0) {
                    s->rot[i].vx += 0x1000;
                }
            }
        } else {
            a = ratan2(group->targetWorld.t[1] - s->joint.vy, group->targetWorld.t[2] - s->joint.vz) % 0x1000;
            if (a < 0) {
                a += 0x1000;
            }
            s->aim.vx -= a;
            if (s->aim.vx < 0) {
                s->aim.vx += 0x1000;
            }
            if (s->aim.vx > 0x800) {
                s->rot[i].vx += speed;
                s->rot[i].vx %= 0x1000;
            } else {
                s->rot[i].vx -= speed;
                if (s->rot[i].vx < 0) {
                    s->rot[i].vx += 0x1000;
                }
            }
        }
        gfxRotMatrixY(&arg0->extra.tmd->coords[i].coord, s->rot[i].vy, 1);
        gfxRotMatrixX(&arg0->extra.tmd->coords[i].coord, s->rot[i].vx, GRAPHICS_ROTATION_COMPOSE);
        gfxRotMatrixZ(&arg0->extra.tmd->coords[i].coord, s->rot[i].vz, GRAPHICS_ROTATION_COMPOSE);
        gte_SetRotMatrix(&s->chain);
        gte_ldclmv(&arg0->extra.tmd->coords[i].coord);
        gte_rtir();
        gte_stclmv(&s->chain);
        gte_ldclmv(&arg0->extra.tmd->coords[i].coord.m[0][1]);
        gte_rtir();
        gte_stclmv(&s->chain.m[0][1]);
        gte_ldclmv(&arg0->extra.tmd->coords[i].coord.m[0][2]);
        gte_rtir();
        gte_stclmv(&s->chain.m[0][2]);
        s->joint.vx = arg0->extra.tmd->coords[i + 1].coord.t[0];
        s->joint.vy = arg0->extra.tmd->coords[i + 1].coord.t[1];
        s->joint.vz = arg0->extra.tmd->coords[i + 1].coord.t[2];
        gte_SetRotMatrix(&s->chain);
        gte_ldv0(&s->joint);
        gte_rtv0();
        gte_stsv(&s->joint);
        s->ang.vx += s->rot[i].vx;
        s->ang.vx %= 0x1000;
        s->ang.vy += s->rot[i].vy;
        s->ang.vy %= 0x1000;
        s->ang.vz += s->rot[i].vz;
        s->ang.vz %= 0x1000;
        s->pos.vx += s->joint.vx;
        s->pos.vy += s->joint.vy;
        s->pos.vz += s->joint.vz;
    }
    memCopyBytes(s->rot, work->rot, sizeof(s->rot));
    switch (work->dipStep) {
        case 0:
            if (abs(s->pos.vy - group->targetWorld.t[1]) < 300) {
                if (abs(s->pos.vz - group->targetWorld.t[2]) < 200) {
                    work->dipStep = 1;
                }
            }
            break;
        case 1:
            work->offset.vy -= 20;
            if (work->offset.vy < -100) {
                work->dipStep = 2;
            }
            break;
        case 2:
            work->offset.vy += 2;
            if (work->offset.vy > 0) {
                work->offset.vy = 0;
                work->dipStep   = 0;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_Actor560800ChainScratch);
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
    gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->swayPhase  = gRandomLcgState >> 16;
    gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->field_274  = gRandomLcgState >> 16;
    gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->pulseScale = (gRandomLcgState >> 16) & 0x3FF;
    animationInitContext(&work->rig.anim, D_actor_560800_801752F0, model, work->rig.poses, work->rig.slots);
    work->chainNumber = task->spawnArg1.value;
}

/// Per-frame handler of a chain `_actor560800InitChainModel` sets up. State 1
/// hides the chain (`TmdObject::flags` bit 0x80) for the chain numbers the
/// current view excludes and otherwise runs `func_actor_560800_80136AA8`;
/// state 2 restarts the rig's slots 1 to 6 in the clip `chainNumber` names,
/// state 3 ticks them, state 4 spawns the falling copy from
/// `D_actor_560800_8017575C` and gives it this chain's part coordinates, and
/// state 5 kills the task a frame later. Every frame that survives rebuilds the root translation and, while
/// visible, drives `func_shelter_b1_pod_service_gantry_8017F450` and the periodic `effectSpawn`.
void func_actor_560800_80137820(Task* arg0)
{
    _Actor560800PropWork* work;
    TmdObject*            extra;
    GfxCoord*             coord;
    _Actor560800PropWork* anim;
    Task*                 child;
    s32                   i;
    u32                   tick;
    TmdObject*            obj;
    u32                   state;
    u16                   id;
    SVECTOR               unused; // never touched; only reserves the frame slot

    extra = arg0->extra.tmd;
    state = arg0->state;
    work  = arg0->work;
    coord = extra->coords;
    obj   = extra;
    switch (state) {
        case 0:
            _actor560800InitChainModel(arg0);
            arg0->state++;
            return;
        case 1:
            if (Gp_FindViewIndex(gGameSession->location.loc.view) == 0x16) {
                switch (work->chainNumber) {
                    case 1:
                    case 3:
                        obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                        return;
                    case 4 ... 0x7FFF:
                        break;
                    default:
                        func_actor_560800_80136AA8(arg0);
                        goto done;
                }
            }
            if (work->chainNumber < 8) {
                if (work->chainNumber >= 5) {
                    obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    return;
                }
            }
            func_actor_560800_80136AA8(arg0);
            break;
        case 2:
            if (work->chainNumber < 4) {
                if (work->chainNumber >= 2) {
                    obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    return;
                }
            }
            i    = 1;
            id   = work->chainNumber;
            anim = arg0->work;
            do {
                anim->rig.slots[i & 0xFFFF].rate = ANIMATION_RATE_ONE;
                animationResetSlot(&anim->rig.anim, i & 0xFFFF, id);
                i++;
            } while ((u32)(i & 0xFFFF) < ARRAY_SIZE(anim->rig.slots));
            arg0->state++;
            break;
        case 3:
            anim = arg0->work;
            i    = 1;
            do {
                animationTickSlot(&anim->rig.anim, i & 0xFFFF);
                i++;
            } while ((u32)(i & 0xFFFF) < ARRAY_SIZE(anim->rig.slots));
            for (i = 1; (u32)(i & 0xFFFF) < ARRAY_SIZE(anim->rig.slots); i++) {
                if (!(anim->rig.slots[i & 0xFFFF].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
                    break;
                }
            }
            break;
        case 4:
            child = taskSpawnFromTable(D_actor_560800_8017575C, 3,
                                       (s32)D_actor_560800_801757AC->extra.tmd->coords->coord.t[1],
                                       arg0->spawnArg2.pointer);
            if (child == NULL) {
                arg0->state = 1;
                return;
            }
            i = 0;
            do {
                memCopyBytes(&arg0->extra.tmd->coords[i & 0xFFFF].coord,
                             &child->extra.tmd->coords[i & 0xFFFF].coord, sizeof(child->extra.tmd->coords[i & 0xFFFF].coord));
                i++;
            } while ((u32)(i & 0xFFFF) < ARRAY_SIZE(work->rot));
            arg0->killCountdown = 0;
            arg0->state++;
            break;
        case 5:
            if (++arg0->killCountdown >= 2) {
                taskKill(arg0);
                return;
            }
            break;
    }
done:
    coord->coord.t[0]   = work->position.vx + work->offset.vx;
    coord->coord.t[1]   = work->position.vy + work->offset.vy;
    coord->coord.t[2]   = work->position.vz + work->offset.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (!(obj->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        func_shelter_b1_pod_service_gantry_8017F450(&arg0->extra.tmd->coords[6], work->chainNumber, 0x100, 0x3C36);
        if (Gp_FindViewIndex(gGameSession->location.loc.view) != 0x16) {
            tick = D_actor_560800_801752E8 + 1;
            if (!(tick & 0x7F) && ((tick >> 7) & 7) == work->chainNumber) {
                effectSpawn(EFFECT_SHELTER_B1_GANTRY_RISING_SPRITE, &arg0->extra.tmd->coords[2], 0x800, NULL);
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
            root->parent           = D_actor_560800_801757AC->extra.tmd->coords;
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
                               D_actor_560800_801757AC->extra.tmd->coords->coord.t[1] +
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

/// Placement handler of the chain group, which loads a pose table into all
/// eight chains, chosen by `_Actor560800ChainGroupWork::placeMode`: 0-2 place each
/// chain at its table position and, while `placeResetsBend` is set, rebuild its
/// rotation from the table with the other parts straightened; 3 resets each
/// chain's root matrix, hangs it off `gGfxViewCoord` and offsets it from the
/// message position; 4 kills chains 4-7, reparents the rest to
/// `D_actor_560800_801757AC`'s model and raises the
/// `D_actor_560800_801752E8` / `801752EC` flags.
void func_actor_560800_80137F58(Task* task, s32 msgId, VECTOR* msg, s32 arg3)
{
    _Actor560800ChainGroupWork* work;
    u16                         flag;
    ActorTransform*             pose;
    _Actor560800PropWork*       chain;
    GfxCoord*                   coord;
    GfxMatrix*                  mat;
    s32                         i;
    s32                         j;

    work = task->work;
    flag = 0;
    switch (work->placeMode) {
        case 0:
            pose = D_actor_560800_80175314;
            break;
        case 1:
            pose = D_actor_560800_801753D4;
            break;
        case 2:
            pose = D_actor_560800_80175494;
            flag = 1;
            break;
        case 3:
            pose = D_actor_560800_80175554;
            i    = 0;
            do {
                if (work->chains[i & 0xFFFF] != NULL) {
                    coord                     = work->chains[i & 0xFFFF]->extra.tmd->coords;
                    mat                       = (GfxMatrix*)&coord->coord;
                    mat->rotationWords.m00M01 = ONE;
                    mat->rotationWords.m02M10 = 0;
                    mat->rotationWords.m11M12 = ONE;
                    mat->rotationWords.m20M21 = 0;
                    mat->rotationWords.m22    = ONE;
                    coord->parent             = &gGfxViewCoord;
                    chain                     = work->chains[i & 0xFFFF]->work;
                    chain->position.vx        = msg->vx + pose->pos.vx;
                    chain->position.vy        = msg->vy + pose->pos.vy;
                    chain->position.vz        = msg->vz + pose->pos.vz;
                    chain->offset.vx          = 0;
                    chain->offset.vy          = 0;
                    chain->offset.vz          = 0;
                }
                i++;
                pose++;
            } while ((u32)(i & 0xFFFF) < ARRAY_SIZE(work->chains));
            return;
        case 4:
            i = 0;
            do {
                if ((i & 0xFFFF) >= 4U) {
                    taskKill(work->chains[i & 0xFFFF]);
                    work->chains[i & 0xFFFF] = NULL;
                }
                i++;
            } while ((u32)(i & 0xFFFF) < ARRAY_SIZE(work->chains));
            pose = D_actor_560800_80175614;
            i    = 0;
            do {
                if (work->chains[i & 0xFFFF] != NULL) {
                    chain              = work->chains[i & 0xFFFF]->work;
                    coord              = work->chains[i & 0xFFFF]->extra.tmd->coords;
                    coord->parent      = D_actor_560800_801757AC->extra.tmd->coords;
                    chain->position.vx = pose->pos.vx;
                    chain->position.vy = pose->pos.vy;
                    chain->position.vz = pose->pos.vz;
                    gfxRotMatrixY(&coord->coord, pose->rot.vy, 1);
                    gfxRotMatrixX(&coord->coord, pose->rot.vx + 0x400, GRAPHICS_ROTATION_COMPOSE);
                    gfxRotMatrixZ(&coord->coord, pose->rot.vz, GRAPHICS_ROTATION_COMPOSE);
                    chain->rot[0].vx    = pose->rot.vx;
                    chain->rot[0].vy    = pose->rot.vy;
                    chain->rot[0].vz    = pose->rot.vz;
                    chain->pulseGrowing = flag;
                    j                   = 1;
                    do {
                        gfxRotMatrixY(&work->chains[i & 0xFFFF]->extra.tmd->coords[j & 0xFFFF].coord, 0, 1);
                        gfxRotMatrixX(&work->chains[i & 0xFFFF]->extra.tmd->coords[j & 0xFFFF].coord, 0, GRAPHICS_ROTATION_COMPOSE);
                        gfxRotMatrixZ(&work->chains[i & 0xFFFF]->extra.tmd->coords[j & 0xFFFF].coord, 0, GRAPHICS_ROTATION_COMPOSE);
                        chain->rot[j & 0xFFFF].vx = 0;
                        chain->rot[j & 0xFFFF].vy = 0;
                        chain->rot[j & 0xFFFF].vz = 0;
                        j++;
                    } while ((u32)(j & 0xFFFF) < ARRAY_SIZE(chain->rot));
                    coord->composeStamp = GRAPHICS_COORD_DIRTY;
                }
                i++;
                pose++;
            } while ((u32)(i & 0xFFFF) < ARRAY_SIZE(work->chains));
            work->placeMode         = 0;
            D_actor_560800_801752E8 = 1;
            D_actor_560800_801752EC = 1;
            return;
    }
    i = 0;
    do {
        if (work->chains[i & 0xFFFF] != NULL) {
            chain                                 = work->chains[i & 0xFFFF]->work;
            coord                                 = work->chains[i & 0xFFFF]->extra.tmd->coords;
            task->extra.tmd->coords->coord.t[0]   = msg->vx;
            task->extra.tmd->coords->coord.t[1]   = msg->vy;
            task->extra.tmd->coords->coord.t[2]   = msg->vz;
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            chain->position.vx                    = pose->pos.vx;
            chain->position.vy                    = pose->pos.vy;
            chain->position.vz                    = pose->pos.vz;
            chain->pulseGrowing                   = flag;
            if (work->placeResetsBend != 0) {
                gfxRotMatrixY(&coord->coord, pose->rot.vy, 1);
                gfxRotMatrixX(&coord->coord, pose->rot.vx + 0x400, GRAPHICS_ROTATION_COMPOSE);
                gfxRotMatrixZ(&coord->coord, pose->rot.vz, GRAPHICS_ROTATION_COMPOSE);
                chain->rot[0].vx = pose->rot.vx;
                chain->rot[0].vy = pose->rot.vy;
                chain->rot[0].vz = pose->rot.vz;
                j                = 1;
                do {
                    gfxRotMatrixY(&work->chains[i & 0xFFFF]->extra.tmd->coords[j & 0xFFFF].coord, 0, 1);
                    gfxRotMatrixX(&work->chains[i & 0xFFFF]->extra.tmd->coords[j & 0xFFFF].coord, 0, GRAPHICS_ROTATION_COMPOSE);
                    gfxRotMatrixZ(&work->chains[i & 0xFFFF]->extra.tmd->coords[j & 0xFFFF].coord, 0, GRAPHICS_ROTATION_COMPOSE);
                    chain->rot[j & 0xFFFF].vx = 0;
                    chain->rot[j & 0xFFFF].vy = 0;
                    chain->rot[j & 0xFFFF].vz = 0;
                    j++;
                } while ((u32)(j & 0xFFFF) < ARRAY_SIZE(chain->rot));
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        i++;
        pose++;
    } while ((u32)(i & 0xFFFF) < ARRAY_SIZE(work->chains));
    work->placeResetsBend = 0;
    work->placeMode       = 0;
}

/// Command handler of the chain group (`D_actor_560800_801756D4`): command 0
/// relights each chain from its world translation, 5 and 6 put all eight
/// chains into state 2 / 1, and the rest set this task's state and the
/// group's `placeMode`, `placeResetsBend` and `targetEntryId`.
void func_actor_560800_801384EC(Task* task, s32 msgId, ActorCommand* msg, s32 arg3)
{
    _Actor560800ChainGroupWork* work;
    Task*                       part;
    TmdObject*                  extra;
    VECTOR                      vec;
    s32                         i;

    work = task->work;
    switch (msg->command) {
        case 0:
            i = 0;
            do {
                part = work->chains[i & 0xFFFF];
                if (part != NULL) {
                    extra  = part->extra.tmd;
                    vec.vx = extra->coords->workm.t[0];
                    vec.vy = part->extra.tmd->coords->workm.t[1];
                    vec.vz = part->extra.tmd->coords->workm.t[2];
                    worldCoordSetModelLighting(extra, &vec, 0, 3);
                }
                i += 1;
            } while ((u32)(i & 0xFFFF) < ARRAY_SIZE(work->chains));
            break;
        case 1:
            work->placeMode       = 1;
            work->placeResetsBend = 1;
            work->targetEntryId   = ACTOR_560800_CHAIN_TARGET_EVE;
            break;
        case 2:
            task->state    = 2;
            work->field_44 = 0;
            break;
        case 3:
            task->state     = 1;
            work->placeMode = 2;
            break;
        case 4:
            work->placeMode       = 0;
            work->placeResetsBend = 1;
            break;
        case 5:
            task->state     = 1;
            work->placeMode = 3;
            i               = 0;
            do {
                work->chains[i & 0xFFFF]->state = 2;
                i                              += 1;
            } while ((u32)(i & 0xFFFF) < ARRAY_SIZE(work->chains));
            break;
        case 6:
            task->state     = 1;
            work->placeMode = 4;
            i               = 0;
            do {
                work->chains[i & 0xFFFF]->state = 1;
                i                              += 1;
            } while ((u32)(i & 0xFFFF) < ARRAY_SIZE(work->chains));
            break;
        case 7:
            task->state         = 1;
            work->placeMode     = 4;
            work->targetEntryId = ACTOR_560800_CHAIN_TARGET_NO9;
            break;
        case 8:
            task->state = 3;
            break;
    }
}

/// Handler of the chain group. State 0 allocates its `_Actor560800ChainGroupWork`,
/// roots its coordinate at `gGfxViewCoord`, hangs the task below the cutscene's, spawns the
/// eight chains and swaps `gRandomLcgState` out for a zero seed; state 2 grows
/// each chain's `position.vy` up to its `D_actor_560800_80175314` limit; state 3
/// bursts effects on the first remaining chain, puts it into state 4 and drops
/// it. Every frame `targetWorld` follows part 9 of the body
/// `targetEntryId` selects.
void func_actor_560800_801386D4(Task* task)
{
    _Actor560800ChainGroupWork* work;
    _Actor560800ChainGroupWork* w;
    _Actor560800ChainGroupWork* spawned;
    _Actor560800ChainGroupWork* grow;
    _Actor560800PropWork*       chain;
    GfxCoord*                   root;
    GfxCoord*                   partCoord;
    GfxCoord*                   effCoord;
    GfxCoord*                   c;
    Task*                       part;
    SVECTOR                     pos;
    s32                         i;
    s32                         n;
    s16                         k;

    work = task->work;
    switch (task->state) {
        case 0:
            root       = task->extra.coordBody->coord;
            w          = memMalloc(sizeof(*w), false);
            task->work = w;
            if (w == NULL) {
                taskKill(task);
            } else {
                root->parent = &gGfxViewCoord;
                memFillBytes(task->work, 0, sizeof(*w));
                i                 = 0;
                spawned           = w;
                spawned->cutscene = task->spawnArg2.pointer;
                task->msgTable    = D_actor_560800_801756D4;
                taskReparent(spawned->cutscene, task);
                do {
                    spawned->chains[i & 0xFFFF] =
                        taskSpawnFromTable(D_actor_560800_8017575C, 1, (i & 0xFFFF) + 1, task);
                    i++;
                } while ((u32)(i & 0xFFFF) < ARRAY_SIZE(spawned->chains));
                D_actor_560800_801757A8 = gRandomLcgState;
                gRandomLcgState         = 0;
            }
            task->state++;
            break;
        case 1:
            break;
        case 2:
            grow = work;
            n    = 0;
            do {
                part = grow->chains[n & 0xFFFF];
                if (part != NULL) {
                    chain               = part->work;
                    partCoord           = part->extra.tmd->coords;
                    chain->position.vy += D_actor_560800_801756EC[n & 0xFFFF];
                    if (D_actor_560800_80175314[n & 0xFFFF].pos.vy < chain->position.vy) {
                        chain->position.vy = D_actor_560800_80175314[n & 0xFFFF].pos.vy;
                    }
                    partCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                }
                n++;
            } while ((u32)(n & 0xFFFF) < ARRAY_SIZE(grow->chains));
            break;
        case 3:
            for (k = 0; k < ARRAY_SIZE(work->chains); k++) {
                if (work->chains[k] != NULL) {
                    effCoord = &work->chains[k]->extra.tmd->coords[3];
                    effectSpawn(gRoomEffectWaterSprayId, effCoord, 0x10002380, 0);
                    effectSpawn(gRoomEffectWaterSprayId, effCoord, 0x04003480, 0);
                    i = 0;
                    do {
                        effectSpawn(gRoomEffectWaterSprayId, effCoord, 0x02002400, 0);
                        i++;
                        effectSpawn(EFFECT_1B4, effCoord, 0x02202300, 0);
                    } while ((u32)(i & 0xFFFF) < 4U);
                    work->chains[k]->state = 4;
                    work->chains[k]        = NULL;
                    break;
                }
            }
            task->state = 1;
            break;
    }
    w = task->work;
    if (w->targetEntryId == ACTOR_560800_CHAIN_TARGET_EVE) {
        c = ((_Actor560800CutsceneWork*)w->cutscene->work)->eve->extra.tmd->coords;
        gfxComposeNodeWorldTransform(&c[9], &w->targetWorld, &pos);
    } else if (w->targetEntryId == ACTOR_560800_CHAIN_TARGET_NO9) {
        c = ((_Actor560800CutsceneWork*)w->cutscene->work)->no9->extra.tmd->coords;
        gfxComposeNodeWorldTransform(&c[9], &w->targetWorld, &pos);
    }
    w->targetWorld.t[0] = pos.vx;
    w->targetWorld.t[1] = pos.vy - 0x78;
    w->targetWorld.t[2] = pos.vz;
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
    enum {
        ACTOR_560800_CARRIER_RELIGHT        = 0,
        ACTOR_560800_CARRIER_HIDE           = 1,
        ACTOR_560800_CARRIER_FIRST_DESCENT  = 2,
        ACTOR_560800_CARRIER_SLOW_DESCENT   = 3,
        ACTOR_560800_CARRIER_RISE           = 4,
        ACTOR_560800_CARRIER_SECOND_DESCENT = 5,
        ACTOR_560800_CARRIER_CARRY_NO9      = 6,
    };
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
            TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, D_actor_560800_801756FC, 0);
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
            TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, D_actor_560800_80175714, 0);
            work->pulseScale = ONE;
            task->state      = ACTOR_560800_CARRIER_SECOND_DESCENT;
            break;
        case ACTOR_560800_CARRIER_CARRY_NO9:
            taskMessageDispatch(task, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, D_actor_560800_8017572C, 0);
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
    if (D_actor_560800_801752E8 & ACTOR_560800_CARRIER_ACCELERATE) {
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

void func_actor_560800_80138FC8(Task* task)
{
    _Actor560800PropWork* work;
    _Actor560800PropWork* w;
    _Actor560800PropWork* mem;
    TmdObject*            obj;
    GfxCoord*             coord;
    GfxCoord*             c;
    MATRIX*               m0;
    MATRIX*               m2;
    MATRIX*               m3;
    MATRIX*               m5;
    VECTOR                scale;
    GfxCoord*             root;

    switch (task->state) {
        case 0:
            obj        = task->extra.tmd;
            root       = obj->coords;
            task->work = memMalloc(sizeof(_Actor560800PropWork), false);
            if (task->work == NULL) {
                taskKill(task);
            } else {
                memFillBytes(task->work, 0, sizeof(*mem));
                mem           = task->work;
                root->parent  = &gGfxViewCoord;
                mem->parent   = task->spawnArg2.pointer;
                obj->lightMtx = &mem->light;
                obj->colorMtx = &mem->color;
                taskReparent(task->spawnArg2.pointer, task);
                task->msgTable          = D_actor_560800_80175744;
                D_actor_560800_801757AC = task;
                m0                      = &root->coord;
                MATRIX_PAIR(m0, 0, 0)   = 0x1000;
                MATRIX_PAIR(m0, 0, 2)   = 0;
                MATRIX_PAIR(m0, 1, 1)   = 0x1000;
                MATRIX_PAIR(m0, 2, 0)   = 0;
                m0->m[2][2]             = 0x1000;
            }
            task->state++;
            return;
        case 1:
            task->extra.tmd->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case 2:
            coord              = task->extra.tmd->coords;
            work               = task->work;
            coord->coord.t[1] += 5;
            if (work->pulseScale <= 0x1800) {
                work->pulseGrowing    = 1;
                c                     = task->extra.tmd->coords;
                w                     = task->work;
                m2                    = &c[1].coord;
                MATRIX_PAIR(m2, 0, 0) = 0x1000;
                MATRIX_PAIR(m2, 0, 2) = 0;
                MATRIX_PAIR(m2, 1, 1) = 0x1000;
                MATRIX_PAIR(m2, 2, 0) = 0;
                m2->m[2][2]           = 0x1000;
                c++;
                if (w->pulseGrowing == 0) {
                    w->pulseScale -= 0x32;
                    if (w->pulseScale < 0x1000) {
                        w->pulseGrowing = 1;
                    }
                } else if (w->pulseGrowing == 1) {
                    w->pulseScale += 0x32;
                    if (w->pulseScale > 0x1800) {
                        w->pulseGrowing = 0;
                    }
                }
                scale.vx = w->pulseScale;
                scale.vy = 0x1000;
                scale.vz = w->pulseScale;
                ScaleMatrix(&c->coord, &scale);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
        case 3:
            coord              = task->extra.tmd->coords;
            work               = task->work;
            coord->coord.t[1] += 1;
            if (work->pulseScale >= 0x800) {
                work->pulseGrowing    = 0;
                c                     = task->extra.tmd->coords;
                w                     = task->work;
                m3                    = &c[1].coord;
                MATRIX_PAIR(m3, 0, 0) = 0x1000;
                MATRIX_PAIR(m3, 0, 2) = 0;
                MATRIX_PAIR(m3, 1, 1) = 0x1000;
                MATRIX_PAIR(m3, 2, 0) = 0;
                m3->m[2][2]           = 0x1000;
                c++;
                if (w->pulseGrowing == 0) {
                    w->pulseScale -= 0xA;
                    if (w->pulseScale < 0x1000) {
                        w->pulseGrowing = 1;
                    }
                } else if (w->pulseGrowing == 1) {
                    w->pulseScale += 0xA;
                    if (w->pulseScale > 0x1800) {
                        w->pulseGrowing = 0;
                    }
                }
                scale.vx = w->pulseScale;
                scale.vy = 0x1000;
                scale.vz = w->pulseScale;
                ScaleMatrix(&c->coord, &scale);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
        case 4:
            _actor560800RaiseCarrier(task);
            break;
        case 5:
            coord              = task->extra.tmd->coords;
            work               = task->work;
            coord->coord.t[1] += 5;
            if (work->pulseScale <= 0x1800) {
                work->pulseGrowing    = 1;
                c                     = task->extra.tmd->coords;
                w                     = task->work;
                m5                    = &c[1].coord;
                MATRIX_PAIR(m5, 0, 0) = 0x1000;
                MATRIX_PAIR(m5, 0, 2) = 0;
                MATRIX_PAIR(m5, 1, 1) = 0x1000;
                MATRIX_PAIR(m5, 2, 0) = 0;
                m5->m[2][2]           = 0x1000;
                c++;
                if (w->pulseGrowing == 0) {
                    w->pulseScale -= 0x32;
                    if (w->pulseScale < 0x1000) {
                        w->pulseGrowing = 1;
                    }
                } else if (w->pulseGrowing == 1) {
                    w->pulseScale += 0x32;
                    if (w->pulseScale > 0x1800) {
                        w->pulseGrowing = 0;
                    }
                }
                scale.vx = w->pulseScale;
                scale.vy = 0x1000;
                scale.vz = w->pulseScale;
                ScaleMatrix(&c->coord, &scale);
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
        case 6:
            _actor560800CarryNo9Away(task);
            break;
    }
    D_actor_560800_801752E8 += 2;
    D_actor_560800_801752EC += 1;
}

/// Message 0x7D5 handler of the task `D_actor_560800_801756D4` belongs to: the
/// visibility switch `_actor560800SetCarrierModelDraw` performs on a single model,
/// applied to every chain its `_Actor560800ChainGroupWork` still holds. `arg2` is
/// the sub-command - 1 clears the 0x84 pair of bits in the chain's
/// `TmdObject::flags` and 2 sets it, anything else leaves the chains alone.
void func_actor_560800_80139360(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    _Actor560800ChainGroupWork* work;
    TmdObject*                  obj;
    Task*                       part;
    s32                         i;

    work = task->work;
    i    = 0;
    do {
        part = work->chains[i & 0xFFFF];
        if (part != NULL) {
            obj = part->extra.tmd;
            switch (arg2) {
                case 0:
                    break;
                case 1:
                    obj->flags = obj->flags & (u16) ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
                    break;
                case 2:
                    obj->flags = obj->flags | (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
                    break;
            }
        }
        i += 1;
    } while ((u32)(i & 0xFFFF) < ARRAY_SIZE(work->chains));
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

/// A second copy of the handler, under this file's own name.
#define actorMsgPlaceYawPitchRoll func_actor_560800_80139440
#include "../../shared/actor_messages_place_ypr.inc.c"
#undef actorMsgPlaceYawPitchRoll
