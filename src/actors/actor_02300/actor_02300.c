#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/gtemac.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area_entry.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/fs.h"
#include "main/random.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/sound_ids.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"
#include "../../shared/actor/player_detection.h"
/// Selects shielded Rook behavior for this Beam Sword GOLEM carrier.
#define GOLEM_PAWN_ROOK_KIND GOLEM_PAWN_ROOK_KIND_ROOK
/// Selects the shared Beam Sword build for this shielded Rook GOLEM carrier.
///
/// Keep this binding through all GOLEM fragments, with matching weapon tasks,
/// behavior callbacks and animation tables.
#define GOLEM_PAWN_ROOK_WEAPON GOLEM_PAWN_ROOK_WEAPON_BEAM_SWORD
#include "../../shared/creatures/golem/golem_pawn_rook.h"

/// Character script bank used by this carrier's cue tables.
enum { GOLEM_PAWN_ROOK_SOUND_BANK = 0x17 };

extern AnimationSet gGolemIdleClip;
extern AnimationSet gGolemWalkClip;
static AnimationSet _gGolemTurnClip;
static AnimationSet _gGolemListenClip;
static AnimationSet _gGolemRookBeamSwordChargeWindupClip;
static AnimationSet _gGolemRookBeamSwordChargeRunClip;
static AnimationSet _gGolemRookBeamSwordChargeSlashClip;
extern AnimationSet gGolemSwingRecoverClip;
extern AnimationSet gGolemSwordSwingClip;
static AnimationSet _gGolemRookScreamChargeClip;
extern AnimationSet gGolemScreamReleaseClip;
extern AnimationSet gGolemStaggerClip;
static AnimationSet _gGolemRecoilFrontClip;
static AnimationSet _gGolemRecoilBehindClip;
static AnimationSet _gGolemBuildupClip;
static AnimationSet _gGolemScreamRecoverClip;
static AnimationSet _gGolemFallBehindClip;
static AnimationSet _gGolemDownedHitBehindClip;
static AnimationSet _gGolemWritheBehindClip;
static AnimationSet _gGolemLieBehindClip;
static AnimationSet _gGolemFallFrontClip;
static AnimationSet _gGolemDownedHitFrontClip;
static AnimationSet _gGolemWritheFrontClip;
static AnimationSet _gGolemLieFrontClip;
static TmdSource    _gGolemRookBody;
static TmdSource    _gGolemBeamSword;
static TmdSource    _gGolemRookShield;
static void         _actor02300SwordTask(Task* sword);
static void         _actor02300ShieldTask(Task* shield);
static void         _actor02300BodyTask(Task* task);

/// Blend-in durations indexed by animation request, in whole frames.
///
/// The 32 signed halfwords belong to this carrier. Launcher bursts change
/// entry 13 to 0 or 3 for successive shots; all bodies in the carrier share it.
static s16 _gGolemPawnRookAnimBlendFrames[32] = {
    0,
    8,
    8,
    0,
    8,
    8,
    0,
    0,
    8,
    0,
    8,
    0,
    0,
    0,
    0,
    0,
    8,
    4,
    4,
    4,
    4,
    4,
    4,
    0,
    0,
    0,
    4,
    0,
    0,
    0,
    0,
    0,
};

#include "assets/enemies/golem/rook/body.inc"

#include "assets/enemies/golem/beam_sword.inc"

#include "assets/enemies/golem/rook/shield.inc"

#include "assets/enemies/golem/rook/beam_sword/package_1_clips.inc"

/// Five category-4 attack rows, storing base HP damage and player reaction.
///
/// Slots 0/1 are sword swing/charge, 2 is the zero-power Silence row,
/// 3 is the grenade and 4 is the launcher strike. Unused weapon rows remain
/// present so every carrier uses the same attack indices.
static DamageAttack _gGolemPawnRookAttacks[5] = {
    { 28, 5 },
    { 24, 5 },
    { 0, GAME_ACTOR_REACTION_SILENCE },
    { 15, GAME_ACTOR_REACTION_PARALYSIS },
    { 5, GAME_ACTOR_REACTION_ORDINARY },
};

/// Beam Sword Rook GOLEM combat parameters and rewards.
///
/// Fresh bodies borrow this record and start at 482 HP. Release credits 250 EXP,
/// 400 BP and 8 MP; the buildup base is six 31-frame steps. Critical chance and
/// damage-over-time chance/count are zero. The attack rows share the GOLEM indices.
static EnemyParams _gActor02300RookGolemParams = {
    .attacks      = _gGolemPawnRookAttacks,
    .hpMax        = 482,
    .exp          = 250,
    .bp           = 400,
    .mp           = 8,
    .buildupSteps = 6,
};

/// Weapon attack rows absorbed by the raised frontal shield (0 bypass, 1 absorb).
///
/// Indexed by bits 0..6 of a bit-15-clear category-2 contact key. The caller
/// must supply a row in 0..45; absorbed damage reduces shield HP instead of
/// body HP. The signed halfword entries preserve the carrier table layout.
static s16 _gGolemPawnRookShieldWeaponRows[46] = {
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
};

/// Parasite Energy rows absorbed by the raised frontal shield (0 bypass, 1 absorb).
///
/// Indexed by bits 0..6 of a bit-15-set category-2 contact key. Damage
/// calculation accepts rows 0..54; stored row 55 is unused by that domain.
/// Absorbed damage reduces shield HP instead of body HP; entries remain s16.
static s16 _gGolemPawnRookShieldPeRows[56] = {
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
};

/// Character-bank script IDs for animation cues and falls, without instance tags.
///
/// Sound sets 1..4 select cue pairs at 2*set-1/2*set, fall impacts at 8+set,
/// and the start of a backward fall at 12+set. Set 0 skips animation pairs
/// but falls still use entries 8/12; entry 0 is a no-op. Callers insert the
/// placement instance into bits 8..15 and supply spatial pan/depth.
static s32 _gGolemPawnRookMotionCues[17] = {
    SOUND_SCRIPT_REQUEST_NO_OP,
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 1),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 2),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 3),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 4),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 5),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 6),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 12),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 13),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 9),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 10),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 11),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 14),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 15),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 16),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 17),
    SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 18),
};

/// Character-bank script ID for a Beam Sword swing or charging strike.
///
/// The instance byte is zero; the caller inserts the enemy placement index.
static s32 _gGolemPawnRookSwingCue = SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 7);

/* The grenade's impact cue, which the Grenade Launcher builds define at this
 * position. The Beam Sword is built without the grenade, so nothing in this
 * package reads it and it holds no id. */
s32 gGolemPawnRookImpactSound = 0;

/// Character-bank script ID for the start of the Rook's Silence charge.
///
/// The instance byte is zero; the caller inserts the enemy placement index.
/// Pawn carriers retain a no-op ID in the same cue slot.
static s32 _gGolemPawnRookScreamCue = SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 19);

/// Character-bank script ID played when the Rook applies Silence.
///
/// The instance byte is zero; the caller inserts the enemy placement index.
/// Pawn carriers retain a no-op ID in the same cue slot.
static s32 _gGolemPawnRookSilenceCue = SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 20);

/// Character-bank script ID for the attached Rook shield breaking.
///
/// The instance byte is zero; the caller inserts the shield placement index.
/// Pawn carriers retain a no-op ID in the same cue slot.
static s32 _gGolemPawnRookShieldBreakCue = SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 21);

/// Rook Beam Sword GOLEM sound-bank selections for Acropolis areas.
///
/// Indexed by the saved one-based area ID; entry 0 is unused. Zero disables
/// motion cues and their bank load. Values 1..4 choose the character-bank file
/// and the footstep/fall cue set.
/// The stage directory lends this complete stored row to spawn setup.
static u16 _gActor02300AcropolisAreaSoundSets[22] = {
    0,
    0,
    0,
    0,
    2,
    0,
    0,
    0,
    0,
    2,
    0,
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
    0,
};

/// Rook Beam Sword GOLEM sound-bank selections for Dryfield areas.
///
/// Indexed by the saved one-based area ID; entry 0 is unused. Zero disables
/// motion cues and their bank load. Values 1..4 choose the character-bank file
/// and the footstep/fall cue set.
/// The stage directory lends this complete stored row to spawn setup.
static u16 _gActor02300DryfieldAreaSoundSets[40] = {
    0,
    0,
    4,
    0,
    0,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
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
    0,
};

/// Rook Beam Sword GOLEM sound-bank selections for Dryfield night areas.
///
/// Indexed by the saved one-based area ID; entry 0 is unused. Zero disables
/// motion cues and their bank load. Values 1..4 choose the character-bank file
/// and the footstep/fall cue set.
/// The stage directory lends this complete stored row to spawn setup.
static u16 _gActor02300DryfieldNightAreaSoundSets[40] = {
    0,
    4,
    0,
    0,
    0,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    4,
    0,
};

/// Rook Beam Sword GOLEM sound-bank selections for Mine/Shelter areas.
///
/// Indexed by the saved one-based area ID; entry 0 is unused. Zero disables
/// motion cues and their bank load. Values 1..4 choose the character-bank file
/// and the footstep/fall cue set.
/// The stage directory lends this complete stored row to spawn setup.
static u16 _gActor02300MineShelterAreaSoundSets[50] = {
    0,
    4,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    3,
    1,
    2,
    1,
    0,
    3,
    2,
    0,
    2,
    0,
    3,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
    3,
    1,
    0,
    1,
    0,
    2,
    2,
    2,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    2,
    2,
    0,
    0,
    0,
    0,
    0,
};

/// Rook Beam Sword GOLEM sound-bank selections for Shelter/Neo Ark areas.
///
/// Indexed by the saved one-based area ID; entry 0 is unused. Zero disables
/// motion cues and their bank load. Values 1..4 choose the character-bank file
/// and the footstep/fall cue set.
/// The stage directory lends this complete stored row to spawn setup.
static u16 _gActor02300ShelterNeoArkAreaSoundSets[34] = {
    0,
    0,
    2,
    2,
    0,
    2,
    0,
    0,
    0,
    0,
    2,
    4,
    0,
    2,
    2,
    0,
    2,
    0,
    4,
    2,
    0,
    2,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    4,
    0,
};

/// Stage directory of Beam Sword Rook GOLEM area sound-bank selections.
///
/// Stage 0 has no row; stages 1..5 select Acropolis, Dryfield day, Dryfield night,
/// Mine/Shelter or Shelter/Neo Ark. Each borrowed row is indexed by the saved area
/// ID. Zero disables cues and bank loading; 1..4 select the character-bank variant.
static const u16* _gActor02300StageAreaSoundSets[GAME_STAGE_SHELTER_NEO_ARK + 1] = {
    [GAME_STAGE_NONE]            = NULL,
    [GAME_STAGE_AKROPOLIS]       = _gActor02300AcropolisAreaSoundSets,
    [GAME_STAGE_DRYFIELD]        = _gActor02300DryfieldAreaSoundSets,
    [GAME_STAGE_DRYFIELD_NIGHT]  = _gActor02300DryfieldNightAreaSoundSets,
    [GAME_STAGE_MINE_SHELTER]    = _gActor02300MineShelterAreaSoundSets,
    [GAME_STAGE_SHELTER_NEO_ARK] = _gActor02300ShelterNeoArkAreaSoundSets,
};

TaskDesc gActor02300RookGolemTasks[3] = {
    { { { TASK_BODY_TMD, 96 } }, _actor02300BodyTask, { .model = &_gGolemRookBody } },
    { { { TASK_BODY_TMD, 96 } }, _actor02300SwordTask, { .model = &_gGolemBeamSword } },
    { { { TASK_BODY_TMD, 96 } }, _actor02300ShieldTask, { .model = &_gGolemRookShield } },
};

/// Beam Sword Rook GOLEM body animation-set directory.
///
/// The body rig borrows this 31-pointer table for its lifetime. Loaded clip slots
/// are 1..11 and 17..29; 0, 12..16 and 30 are NULL and cannot be played.
/// The mutable pointer-table type is required by the animation context interface.
#include "assets/enemies/golem/rook/beam_sword/bank.inc"

/// Combat behavior callbacks indexed by `GOLEM_PAWN_ROOK_BEHAVIOR_*`.
///
/// Indices 0..14 require live body work; unsupported weapon/kind behaviors
/// use the no-op handler. These are separate from the body task lifecycle.
static TaskFunc _gGolemPawnRookBehaviorHandlers[GOLEM_PAWN_ROOK_BEHAVIOR_DOWNED_DEATH + 1] = {
    _golemPawnRookIdleState,
    _golemPawnRookPatrolState,
    _golemPawnRookEngageState,
    _golemPawnRookSwordChargeState,
    _golemPawnRookSwordSwingState,
    _golemPawnRookSilenceScreamState,
    _golemPawnRookNopState,
    _golemPawnRookNopState,
    _golemPawnRookStaggerState,
    _golemPawnRookRecoilState,
    _golemPawnRookBuildupState,
    _golemPawnRookKnockdownState,
    _golemPawnRookDownedHitState,
    _golemPawnRookCollapseState,
    _golemPawnRookDownedDeathState,
};

static void _actor02300SpawnBody(Enemy* enemy, Task* actor);

#include "../../shared/creatures/golem/golem_pawn_rook_inlines.inc.c"

#include "../../shared/creatures/golem/golem_pawn_rook_take_hits.inc.c"

#include "../../shared/creatures/golem/golem_pawn_rook_patrol.inc.c"

#include "../../shared/creatures/golem/golem_pawn_rook_sword.inc.c"

#include "../../shared/creatures/golem/golem_pawn_rook_silence_scream.inc.c"

/// Links a Rook collision body and initializes the contact table it borrows.
///
/// The body's context must reference contacts directly or through its capsule,
/// with contactCount positive and within their writable element extent. Links
/// before clearing the table and marking its final entry; the caller configures
/// grid/pair enables afterwards. No storage is allocated. The body and its
/// contact storage must stay live until the body is unlinked.
static inline void _actor02300LinkBodyAndInitContacts(s32 listIndex, WorldCollisionBody* body, WorldCollisionContact* contacts, s32 contactCount)
{
    worldCollisionLinkBody(listIndex, body);
    worldCollisionInitContacts(contacts, contactCount, 0);
}

/// Initializes the Beam Sword Rook GOLEM body and attaches its shield and sword.
///
/// Allocates body-owned work/lighting and initializes animation slots 1..18.
/// Children inherit placement texture offsets and borrow the body's lighting.
/// A fresh placement links four combat shapes and target tracking, requests the
/// room's sound bank and seeds one or two scream charges. Saved front/rear corpse
/// poses enter teardown without combat links. Work-allocation failure destroys the
/// enemy/task. Requires valid stage/area indices and successful child allocations.
static void _actor02300SpawnBody(Enemy* enemy, Task* actor)
{
    enum {
        ACTOR_02300_ID                       = 23,
        ACTOR_02300_BODY_PART                = 3,
        ACTOR_02300_SIGHT_PART               = 4,
        ACTOR_02300_SWORD_TASK               = 1,
        ACTOR_02300_SHIELD_TASK              = 2,
        ACTOR_02300_HIT_EFFECT_SIZE          = 1280,
        ACTOR_02300_HIT_EFFECT_KIND          = 2,
        ACTOR_02300_PATROL_UNITS_PER_VARIANT = 1000,
        ACTOR_02300_SOUND_FILE_GROUP         = 10,
        ACTOR_02300_SHIELD_HP                = 250,
        ACTOR_02300_SIGHT_LENGTH             = 8000,
        ACTOR_02300_SIGHT_FAR_RADIUS         = 1000,
        ACTOR_02300_SIGHT_NEAR_RADIUS        = 1500,
        ACTOR_02300_HURT_RADIUS              = 400,
        ACTOR_02300_GROUND_RADIUS            = 550,
        ACTOR_02300_STRIKE_RADIUS            = 500,
        ACTOR_02300_RESTORED_CORPSE_STEP     = 2,
        ACTOR_02300_CORPSE_BEHIND_ANIM       = 25,
        ACTOR_02300_CORPSE_FRONT_ANIM        = 29,
        ACTOR_02300_ANIM_IDLE                = 1,
        ACTOR_02300_SPAWN_FRESH              = 0,
        ACTOR_02300_PLACEMENT_PATROLS        = 1
    };

    GolemPawnRookWork* work;
    TmdObject*         model;
    GfxCoord*          rootCoord;
    GfxCoord*          bodyCoords;
    GfxCoord*          sightCoords;
    GfxCoord*          hurtCoords;
    GfxCoord*          groundCoords;
    GfxCoord*          swordCoords;
    Enemy*             shieldEnemy;
    Enemy*             swordEnemy;
    const u16*         areaSoundSets;
    u8                 soundFileKey[8];
    u8                 soundFileArgs[8];
    s32                slotIndex;
    s32                patrolLengthThousands;
    u32                randomDraw;

    model     = actor->extra.tmd;
    rootCoord = model->coords;
    work      = memCalloc(sizeof(GolemPawnRookWork), 0);
    if (work == NULL) {
        enemyDestroy(enemy, actor);
        return;
    }
    actor->work                   = work;
    model->flags                  = 0;
    rootCoord->composeStamp       = GRAPHICS_COORD_DIRTY;
    model->lightMtx               = &work->lightMtx;
    model->colorMtx               = &work->colorMtx;
    work->actorId                 = ACTOR_02300_ID;
    work->taskTable               = gActor02300RookGolemTasks;
    work->hitEffectArg.coord      = &actor->extra.tmd->coords[ACTOR_02300_BODY_PART];
    work->hitEffectArg.spawnArgLo = ACTOR_02300_HIT_EFFECT_SIZE;
    work->hitEffectArg.spawnArgHi = ACTOR_02300_HIT_EFFECT_KIND;
    animationInitContext(&work->rig.anim, _gActor02300RookGolemAnimationSets, model, work->rig.poses, work->rig.slots);
    for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationResetSlot(&work->rig.anim, slotIndex, 1);
    }

    shieldEnemy = enemySpawnFromTable(gActor02300RookGolemTasks, ACTOR_02300_SHIELD_TASK, 0, enemy);
    _actorRenderApplyPlacementTextureOffsets(shieldEnemy->task->extra.tmd, enemy);
    swordEnemy = enemySpawnFromTable(gActor02300RookGolemTasks, ACTOR_02300_SWORD_TASK, 0, enemy);
    _actorRenderApplyPlacementTextureOffsets(swordEnemy->task->extra.tmd, enemy);

    switch (enemy->spawnState) {
        case ACTOR_02300_SPAWN_FRESH:
            enemy->field_4  = &rootCoord->coord;
            enemy->field_48 = 0;
            worldTargetLinkNode(&enemy->node);
            bodyCoords        = actor->extra.tmd->coords;
            enemy->bodyPos.vx = 0;
            enemy->bodyPos.vy = 0;
            enemy->bodyPos.vz = 0;
            enemy->param      = &_gActor02300RookGolemParams;
            enemy->recs       = work->hurtContacts;
            enemy->coord      = &bodyCoords[ACTOR_02300_BODY_PART];
            enemy->hp         = _gActor02300RookGolemParams.hpMax;
            sceneAcquireBattleRef(0);
            work->patrols = enemy->place->mode & ACTOR_02300_PLACEMENT_PATROLS;
            if (work->patrols == 0) {
                work->anim     = ACTOR_02300_ANIM_IDLE;
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_IDLE;
            } else {
                work->anim               = GOLEM_PAWN_ROOK_ANIM_WALK;
                work->behavior           = GOLEM_PAWN_ROOK_BEHAVIOR_PATROL;
                patrolLengthThousands    = enemy->place->variant;
                work->patrolDistanceLeft = patrolLengthThousands * ACTOR_02300_PATROL_UNITS_PER_VARIANT;
            }

            areaSoundSets = _gActor02300StageAreaSoundSets[gGameSession->location.stage];
            if (areaSoundSets != NULL) {
                work->soundSet = areaSoundSets[gGameSession->location.area];
            }
            if (work->soundSet != 0) {
                soundFileKey[3]  = 0;
                soundFileKey[2]  = ACTOR_02300_SOUND_FILE_GROUP;
                soundFileKey[0]  = work->soundSet;
                soundFileArgs[0] = ACTOR_02300_ID;
                soundFileArgs[3] = 0;
                soundFileArgs[2] = 0;
                soundFileArgs[1] = 0;
                cdCmdEnqueue(CD_COMMAND_LOAD_FILE, soundFileKey, soundFileArgs);
            }

            // Register combat shapes before enabling each shape's tests.
            work->shieldHp                  = ACTOR_02300_SHIELD_HP;
            work->sightCapsule.ends[0].vz   = ACTOR_02300_SIGHT_LENGTH;
            work->sightCapsule.end0Radius   = ACTOR_02300_SIGHT_FAR_RADIUS;
            work->sightCapsule.end1Radius   = ACTOR_02300_SIGHT_NEAR_RADIUS;
            work->sightCapsule.ends[0].vx   = 0;
            work->sightCapsule.ends[0].vy   = 0;
            work->sightCapsule.ends[1].vx   = 0;
            work->sightCapsule.ends[1].vy   = 0;
            work->sightCapsule.ends[1].vz   = 0;
            work->sightCapsule.contacts     = work->sightContacts;
            randomDraw                      = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            work->screamCharges             = ((randomDraw >> 16) & 1) + 1;
            gRandomLcgState                 = randomDraw;
            sightCoords                     = actor->extra.tmd->coords;
            work->sightBody.context.capsule = &work->sightCapsule;
            work->sightBody.pos.vx          = 0;
            work->sightBody.pos.vy          = 0;
            work->sightBody.pos.vz          = 0;
            work->sightBody.key             = 0;
            work->sightBody.radius          = 0;
            work->sightBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
            work->sightBody.coord           = &sightCoords[ACTOR_02300_SIGHT_PART];
            _actor02300LinkBodyAndInitContacts(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->sightBody, work->sightContacts, ARRAY_SIZE(work->sightContacts));
            work->sightBody.flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);

            hurtCoords                      = actor->extra.tmd->coords;
            work->hurtBody.context.contacts = work->hurtContacts;
            work->hurtBody.pos.vx           = 0;
            work->hurtBody.pos.vy           = 0;
            work->hurtBody.pos.vz           = 0;
            work->hurtBody.key              = (WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_02300_ID);
            work->hurtBody.radius           = ACTOR_02300_HURT_RADIUS;
            work->hurtBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->hurtBody.coord            = &hurtCoords[ACTOR_02300_BODY_PART];
            _actor02300LinkBodyAndInitContacts(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hurtBody, work->hurtContacts, ARRAY_SIZE(work->hurtContacts));
            work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

            groundCoords                      = actor->extra.tmd->coords;
            work->groundBody.pos.vy           = -ACTOR_02300_GROUND_RADIUS;
            work->groundBody.context.contacts = work->groundContacts;
            work->groundBody.pos.vx           = 0;
            work->groundBody.pos.vz           = 0;
            work->groundBody.key              = 0;
            work->groundBody.radius           = ACTOR_02300_GROUND_RADIUS;
            work->groundBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->groundBody.coord            = groundCoords;
            _actor02300LinkBodyAndInitContacts(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->groundBody, work->groundContacts, ARRAY_SIZE(work->groundContacts));
            work->groundBody.flags |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);

            swordCoords                       = swordEnemy->task->extra.tmd->coords;
            work->strikeBody.context.contacts = work->strikeContacts;
            work->strikeBody.pos.vx           = 0;
            work->strikeBody.pos.vy           = ACTOR_02300_STRIKE_RADIUS;
            work->strikeBody.pos.vz           = 0;
            work->strikeBody.key              = 0;
            work->strikeBody.radius           = ACTOR_02300_STRIKE_RADIUS;
            work->strikeBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->strikeBody.coord            = swordCoords;
            _actor02300LinkBodyAndInitContacts(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->strikeBody, work->strikeContacts, ARRAY_SIZE(work->strikeContacts));
            work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            actor->state            = GOLEM_PAWN_ROOK_TASK_RUNNING;
            break;

        case GOLEM_PAWN_ROOK_DOWNED_BEHIND:
            work->anim   = ACTOR_02300_CORPSE_BEHIND_ANIM;
            work->step   = ACTOR_02300_RESTORED_CORPSE_STEP;
            actor->state = GOLEM_PAWN_ROOK_TASK_TEARDOWN;
            break;

        case GOLEM_PAWN_ROOK_DOWNED_FRONT:
            work->anim   = ACTOR_02300_CORPSE_FRONT_ANIM;
            work->step   = ACTOR_02300_RESTORED_CORPSE_STEP;
            actor->state = GOLEM_PAWN_ROOK_TASK_TEARDOWN;
            break;
    }
}

#include "../../shared/creatures/golem/golem_pawn_rook_frame.inc.c"

#include "../../shared/creatures/golem/golem_pawn_rook_lunge_cycle.inc.c"

#include "../../shared/actor/player_detection_segment.inc.c"

#include "../../shared/creatures/golem/golem_pawn_rook.inc.c"

/// Rook GOLEM Beam Sword callbacks: 0 attach at body part 7, 1 tick, 2 destroy.
///
/// The sword borrows its body's coordinates and lighting. Its tick mirrors drawing
/// and starts the delayed trail; sword teardown also releases that trail.
static const EnemyTaskFuncTable3 _gActor02300SwordTaskStates = {
    .funcs = {
        _golemPawnRookSwordSpawn,
        _golemPawnRookSwordTick,
        enemyDestroy,
    },
};

/// Runs the Rook GOLEM's attached Beam Sword lifecycle.
///
/// Requires a live sword Enemy in `spawnArg2.pointer`, its TMD model and parent
/// body work. The unchecked state is 0 attachment at body part 7, 1 visibility
/// mirroring and delayed trail spawn, or 2 destruction. Lighting is borrowed
/// from the body; sword teardown also releases its successfully spawned trail.
static void _actor02300SwordTask(Task* sword)
{
    const EnemyTaskFuncTable3 stateHandlers = _gActor02300SwordTaskStates;

    stateHandlers.funcs[sword->state](sword->spawnArg2.pointer, sword);
}

#include "../../shared/creatures/golem/golem_pawn_rook_sword_spawn.inc.c"

/// Rook GOLEM shield callbacks: 0 attach at body part 11, 1 tick, 2 destroy.
///
/// The shield borrows its body's coordinates and lighting. Its tick mirrors drawing
/// and processes the shield break before requesting destruction.
static const EnemyTaskFuncTable3 _gActor02300ShieldTaskStates = {
    .funcs = {
        _golemPawnRookShieldSpawn,
        _golemPawnRookShieldTick,
        enemyDestroy,
    },
};

/// Runs the Beam Sword Rook GOLEM's attached shield lifecycle.
///
/// Requires a live shield Enemy in `spawnArg2.pointer`, its model and parent
/// body work. The unchecked state is 0 attachment at body part 11, 1 visibility
/// mirroring and shield-break processing, or 2 destruction. Lighting is borrowed
/// from the body; the break tick requests teardown after spawning its effects.
static void _actor02300ShieldTask(Task* shield)
{
    const EnemyTaskFuncTable3 stateHandlers = _gActor02300ShieldTaskStates;

    stateHandlers.funcs[shield->state](shield->spawnArg2.pointer, shield);
}

#include "../../shared/creatures/golem/golem_pawn_rook_shield_spawn.inc.c"

/// Rook GOLEM body callbacks: 0 spawn, 1 combat frame, 2 persistent corpse.
///
/// Each receives separate Enemy and Task pointers. The corpse state releases combat
/// participation and keeps the model and work alive for pose, lighting and shadow.
static const EnemyTaskFuncTable3 _gActor02300BodyTaskStates = {
    .funcs = {
        _actor02300SpawnBody,
        _golemPawnRookFrameState,
        _golemPawnRookDeadState,
    },
};

#include "../../shared/creatures/golem/golem_pawn_rook_shield_tick.inc.c"

/// Dispatches the Beam Sword Rook body task's spawn, frame or corpse-teardown phase.
///
/// Requires state 0..2, a live TMD body and its Enemy in spawnArg2.pointer.
/// The lifecycle table is copied by value; work is allocated by the spawn phase
/// and borrowed by its shield and sword until teardown.
static void _actor02300BodyTask(Task* task)
{
    EnemyTaskFuncTable3 lifecycleHandlers;

    lifecycleHandlers = _gActor02300BodyTaskStates;
    lifecycleHandlers.funcs[task->state](task->spawnArg2.pointer, task);
}

PACKAGE_ALIASES
