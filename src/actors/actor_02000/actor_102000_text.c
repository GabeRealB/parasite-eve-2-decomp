#include "actors/actor_02000.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
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
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
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
/// Selects Pawn behavior for this Beam Sword GOLEM carrier.
#define GOLEM_PAWN_ROOK_KIND GOLEM_PAWN_ROOK_KIND_PAWN
/// Selects the shared Beam Sword build for this Pawn GOLEM carrier.
///
/// Keep this binding through all GOLEM fragments, with matching weapon tasks,
/// behavior callbacks and animation tables.
#define GOLEM_PAWN_ROOK_WEAPON GOLEM_PAWN_ROOK_WEAPON_BEAM_SWORD
#include "../../shared/actor/player_detection.h"
#include "../../shared/creatures/golem/golem_pawn_rook.h"

/// Area sound selector that suppresses the group-10 file request and animation cues.
enum { ACTOR_02000_SOUND_SET_NONE = 0 };

/// Character script bank used by this carrier's cue tables.
enum { GOLEM_PAWN_ROOK_SOUND_BANK = 0x14 };

static const EnemyTaskFuncTable3 _gActor02000SwordLifecycleHandlers;
static const EnemyTaskFuncTable3 _gActor02000BodyLifecycleHandlers;

static s16          _gGolemPawnRookShieldWeaponRows[];
static s16          _gGolemPawnRookShieldPeRows[];
extern AnimationSet gGolemIdleClip;

extern AnimationSet gGolemWalkClip;

static AnimationSet _gGolemTurnClip;

static AnimationSet _gGolemListenClip;

extern AnimationSet gGolemPawnBeamSwordChargeWindupClip;

extern AnimationSet gGolemPawnBeamSwordChargeRunClip;

extern AnimationSet gGolemPawnBeamSwordChargeSlashClip;

extern AnimationSet gGolemSwingRecoverClip;

extern AnimationSet gGolemSwordSwingClip;

extern AnimationSet gGolemStaggerClip;

extern AnimationSet gGolemRecoilFrontClip;

extern AnimationSet gGolemRecoilBehindClip;

extern AnimationSet gGolemBuildupClip;

extern AnimationSet gGolemScreamRecoverClip;

static AnimationSet _gGolemFallBehindClip;

static AnimationSet _gGolemDownedHitBehindClip;

static AnimationSet _gGolemWritheBehindClip;

static AnimationSet _gGolemLieBehindClip;

static AnimationSet _gGolemFallFrontClip;

static AnimationSet _gGolemDownedHitFrontClip;

static AnimationSet _gGolemWritheFrontClip;

static AnimationSet _gGolemLieFrontClip;

static TmdSource _gGolemPawnBody;

static TmdSource _gGolemBeamSword;

static void _golemPawnRookEngageState(Task* task);

static void _actor02000SwordTask(Task* sword);

static void _actor02000BodyTask(Task* body);

static void _actor02000SpawnBody(Enemy* enemy, Task* actor);

#include "../../shared/creatures/golem/golem_pawn_rook_take_hits.inc.c"

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

#include "assets/enemies/golem/pawn/body.inc"

#include "assets/enemies/golem/beam_sword.inc"

#include "assets/enemies/golem/pawn/beam_sword/package_1_clips.inc"

/// Five category-4 attack rows, storing base HP damage and player reaction.
///
/// Slots 0/1 are sword swing/charge, 2 is the zero-power Silence row,
/// 3 is the grenade and 4 is the launcher strike. Unused weapon rows remain
/// present so every carrier uses the same attack indices.
static DamageAttack _gGolemPawnRookAttacks[5] = {
    { 28, GAME_ACTOR_REACTION_HIT_FLASHES },
    { 24, GAME_ACTOR_REACTION_HIT_FLASHES },
    { 0, GAME_ACTOR_REACTION_SILENCE },
    { 15, GAME_ACTOR_REACTION_PARALYSIS },
    { 5, GAME_ACTOR_REACTION_ORDINARY },
};

/// Combat parameters shared by the Beam Sword Pawn GOLEM bodies in this carrier.
///
/// Bodies start with 425 HP and borrow the five attack rows for their lifetime.
/// Release awards 125 EXP, 100 BP and 5 MP; critical chance is 50 percent,
/// buildup uses six base steps, and damage-over-time chance/pulse count are zero.
static EnemyParams _gActor02000PawnGolemParams = {
    .attacks              = _gGolemPawnRookAttacks,
    .hpMax                = 425,
    .exp                  = 125,
    .bp                   = 100,
    .mp                   = 5,
    .critChance           = 50,
    .buildupSteps         = 6,
    .damageOverTimeChance = 0,
    .damageOverTimeTicks  = 0,
};

/// Weapon attack rows absorbed by the raised frontal shield (0 bypass, 1 absorb).
///
/// Indexed by bits 0..6 of a bit-15-clear category-2 contact key. The caller
/// must supply a row in 0..45; absorbed damage reduces shield HP instead of
/// body HP. The signed halfword entries preserve the carrier table layout.
static s16 _gGolemPawnRookShieldWeaponRows[46] = {
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
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
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_ABSORB,
    GOLEM_PAWN_ROOK_SHIELD_BYPASS,
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

/* The four cue ids the other builds define at this position: the grenade's
 * impact, and the Rook's scream, silence and shield break. The Pawn's Beam Sword
 * build has none of the code that plays them, so nothing in this package
 * reads these and each holds no id. */
s32 gGolemPawnRookImpactSound = 0;

/// Character-bank script ID for the start of the Rook's Silence charge.
///
/// The instance byte is zero; the caller inserts the enemy placement index.
/// Pawn carriers retain a no-op ID in the same cue slot.
static s32 _gGolemPawnRookScreamCue = SOUND_SCRIPT_REQUEST_NO_OP;

/// Character-bank script ID played when the Rook applies Silence.
///
/// The instance byte is zero; the caller inserts the enemy placement index.
/// Pawn carriers retain a no-op ID in the same cue slot.
static s32 _gGolemPawnRookSilenceCue = SOUND_SCRIPT_REQUEST_NO_OP;

/// Character-bank script ID for the attached Rook shield breaking.
///
/// The instance byte is zero; the caller inserts the shield placement index.
/// Pawn carriers retain a no-op ID in the same cue slot.
static s32 _gGolemPawnRookShieldBreakCue = SOUND_SCRIPT_REQUEST_NO_OP;

/// Beam Sword Pawn GOLEM sound-file selectors for Acropolis/M.I.S.T., indexed by area ID.
///
/// Rows 0..21; 0 disables animation cues, 1..4 select a group-10 sound
/// file and the corresponding motion/fall cue set. Area row 0 is unused.
static u16 _gActor02000AcropolisAreaSoundSets[22] = {
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    2,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    2,
    ACTOR_02000_SOUND_SET_NONE,
    2,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
};

/// Beam Sword Pawn GOLEM sound-file selectors for daytime Dryfield, indexed by area ID.
///
/// Rows 0..39; 0 disables animation cues, 1..4 select a group-10 sound
/// file and the corresponding motion/fall cue set. Area row 0 is unused.
static u16 _gActor02000DryfieldAreaSoundSets[40] = {
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    4,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    4,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    4,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    2,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
};

/// Beam Sword Pawn GOLEM sound-file selectors for nighttime Dryfield, indexed by area ID.
///
/// Rows 0..39; 0 disables animation cues, 1..4 select a group-10 sound
/// file and the corresponding motion/fall cue set. Area row 0 is unused.
static u16 _gActor02000DryfieldNightAreaSoundSets[40] = {
    ACTOR_02000_SOUND_SET_NONE,
    4,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    4,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    4,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    4,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    4,
    ACTOR_02000_SOUND_SET_NONE,
};

/// Beam Sword Pawn GOLEM sound-file selectors for the Mine/Shelter stage, indexed by area ID.
///
/// Rows 0..49; 0 disables animation cues, 1..4 select a group-10 sound
/// file and the corresponding motion/fall cue set. Area row 0 is unused.
static u16 _gActor02000MineShelterAreaSoundSets[50] = {
    ACTOR_02000_SOUND_SET_NONE,
    4,
    4,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    3,
    1,
    2,
    1,
    ACTOR_02000_SOUND_SET_NONE,
    3,
    2,
    ACTOR_02000_SOUND_SET_NONE,
    2,
    ACTOR_02000_SOUND_SET_NONE,
    3,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    1,
    1,
    ACTOR_02000_SOUND_SET_NONE,
    3,
    1,
    ACTOR_02000_SOUND_SET_NONE,
    1,
    ACTOR_02000_SOUND_SET_NONE,
    2,
    2,
    2,
    1,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    2,
    2,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
};

/// Beam Sword Pawn GOLEM sound-file selectors for the Shelter/Neo Ark stage, indexed by area ID.
///
/// Rows 0..33; 0 disables animation cues, 1..4 select a group-10 sound
/// file and the corresponding motion/fall cue set. Area row 0 is unused.
static u16 _gActor02000ShelterNeoArkAreaSoundSets[34] = {
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    2,
    2,
    ACTOR_02000_SOUND_SET_NONE,
    2,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    2,
    4,
    ACTOR_02000_SOUND_SET_NONE,
    2,
    2,
    ACTOR_02000_SOUND_SET_NONE,
    2,
    ACTOR_02000_SOUND_SET_NONE,
    4,
    2,
    ACTOR_02000_SOUND_SET_NONE,
    2,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    1,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    ACTOR_02000_SOUND_SET_NONE,
    4,
    ACTOR_02000_SOUND_SET_NONE,
};

/// Stage-indexed area sound sets for the Beam Sword Pawn GOLEM.
///
/// Stage 0 has no table; stages 1..5 follow `GAME_STAGE_*`. Each selected
/// area ID must fit its stage table. The body caches only the u16 selector.
static u16* _gActor02000StageAreaSoundSets[6] = {
    [GAME_STAGE_NONE]            = NULL,
    [GAME_STAGE_AKROPOLIS]       = _gActor02000AcropolisAreaSoundSets,
    [GAME_STAGE_DRYFIELD]        = _gActor02000DryfieldAreaSoundSets,
    [GAME_STAGE_DRYFIELD_NIGHT]  = _gActor02000DryfieldNightAreaSoundSets,
    [GAME_STAGE_MINE_SHELTER]    = _gActor02000MineShelterAreaSoundSets,
    [GAME_STAGE_SHELTER_NEO_ARK] = _gActor02000ShelterNeoArkAreaSoundSets,
};

TaskDesc gActor02000PawnGolemTasks[2] = {
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _actor02000BodyTask, { .model = &_gGolemPawnBody } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _actor02000SwordTask, { .model = &_gGolemBeamSword } },
};

/// Animation request bank borrowed by the Beam Sword Pawn GOLEM body rig.
///
/// Indices 1..9 and 17..29 have clips; 0, 10..16 and 30 are NULL. Playback
/// requests must select a present clip. The table and clip data stay loaded
/// while the body's nineteen-slot animation context borrows them.
#include "assets/enemies/golem/pawn/beam_sword/bank.inc"

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
    _golemPawnRookNopState,
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

#include "../../shared/creatures/golem/golem_pawn_rook_patrol.inc.c"

#include "../../shared/creatures/golem/golem_pawn_rook_sword.inc.c"

/// Initializes the Beam Sword Pawn GOLEM body and its attached sword.
///
/// Requires a live nineteen-part model, placement and valid stage/area sound
/// indices. Allocates body-owned work and lighting and initializes slots 1..18.
/// Sword spawning must succeed; its texture placement is applied immediately.
/// A fresh placement links targeting and four combat bodies and acquires a
/// battle reference. Saved behind/front downed poses enter the persistent corpse
/// state without relinking combat. Work allocation failure destroys the pair.
static void _actor02000SpawnBody(Enemy* enemy, Task* actor)
{
    /// Links an initialized body, clears its contacts and enables collision tests.
    ///
    /// body is a side-effect-free pointer expression evaluated twice; other
    /// arguments are evaluated once. contactCount counts writable contact elements.
    /// The body already borrows this table, directly or through its capsule;
    /// all storage stays live until unlinking. Expands to a braced statement block.
#define ACTOR_02000_LINK_BODY_CONTACTS(listIndex, body, contacts, contactCount, enabledTests) \
    {                                                                                         \
        worldCollisionLinkBody((listIndex), (body));                                          \
        worldCollisionInitContacts((contacts), (contactCount), 0);                            \
        (body)->flags |= (enabledTests);                                                      \
    }
    enum {
        ACTOR_02000_ID                       = 20,
        ACTOR_02000_BODY_PART                = 3,
        ACTOR_02000_SIGHT_PART               = 4,
        ACTOR_02000_SWORD_TASK               = 1,
        ACTOR_02000_HIT_EFFECT_SIZE          = 1280,
        ACTOR_02000_HIT_EFFECT_HIGH_ARGUMENT = 2,
        ACTOR_02000_PATROL_UNITS_PER_VARIANT = 1000,
        ACTOR_02000_SOUND_FILE_GROUP         = 10,
        ACTOR_02000_SIGHT_LENGTH             = 8000,
        ACTOR_02000_SIGHT_FAR_RADIUS         = 1000,
        ACTOR_02000_SIGHT_NEAR_RADIUS        = 1500,
        ACTOR_02000_HURT_RADIUS              = 400,
        ACTOR_02000_GROUND_RADIUS            = 550,
        ACTOR_02000_STRIKE_OFFSET_Y          = 500,
        ACTOR_02000_STRIKE_RADIUS            = 500,
        ACTOR_02000_RESTORED_CORPSE_STEP     = 2,
        ACTOR_02000_CORPSE_BEHIND_ANIM       = 25,
        ACTOR_02000_CORPSE_FRONT_ANIM        = 29,
        ACTOR_02000_IDLE_ANIM                = 1,
        ACTOR_02000_FRESH_PLACEMENT          = 0,
        ACTOR_02000_PLACEMENT_PATROLS        = 1,
    };
    GolemPawnRookWork* work;
    TmdObject*         model;
    GfxCoord*          root;
    GfxCoord*          bodyCoords;
    GfxCoord*          sightCoords;
    GfxCoord*          hurtCoords;
    GfxCoord*          groundCoords;
    GfxCoord*          swordCoords;
    Enemy*             swordEnemy;
    u16*               areaSoundSets;
    u8                 soundFileKey[8];
    u8                 soundFileArgs[8];
    s32                slotIndex;
    s32                patrolLengthThousands;

    // Body-owned work supplies lighting and animation storage to the attached sword.
    model = actor->extra.tmd;
    root  = model->coords;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyDestroy(enemy, actor);
        return;
    }
    actor->work                   = work;
    model->flags                  = 0;
    root->composeStamp            = GRAPHICS_COORD_DIRTY;
    model->lightMtx               = &work->lightMtx;
    model->colorMtx               = &work->colorMtx;
    work->actorId                 = ACTOR_02000_ID;
    work->taskTable               = gActor02000PawnGolemTasks;
    work->hitEffectArg.coord      = &actor->extra.tmd->coords[ACTOR_02000_BODY_PART];
    work->hitEffectArg.spawnArgLo = ACTOR_02000_HIT_EFFECT_SIZE;
    work->hitEffectArg.spawnArgHi = ACTOR_02000_HIT_EFFECT_HIGH_ARGUMENT;
    animationInitContext(&work->rig.anim, _gActor02000AnimationSets, model, work->rig.poses, work->rig.slots);
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationResetSlot(&work->rig.anim, slotIndex, ACTOR_02000_IDLE_ANIM);
    }
    swordEnemy = enemySpawnFromTable(gActor02000PawnGolemTasks, ACTOR_02000_SWORD_TASK, 0, enemy);
    _actorRenderApplyTaskPlacementTextureOffsets(swordEnemy->task, enemy);

    // Restored corpses retain the sword but do not rejoin combat.
    switch (enemy->spawnState) {
        case ACTOR_02000_FRESH_PLACEMENT:
            enemy->field_4  = &root->coord;
            enemy->field_48 = 0;
            worldTargetLinkNode(&enemy->node);
            bodyCoords        = actor->extra.tmd->coords;
            enemy->bodyPos.vx = 0;
            enemy->bodyPos.vy = 0;
            enemy->bodyPos.vz = 0;
            enemy->param      = &_gActor02000PawnGolemParams;
            enemy->recs       = work->hurtContacts;
            enemy->coord      = &bodyCoords[ACTOR_02000_BODY_PART];
            enemy->hp         = _gActor02000PawnGolemParams.hpMax;
            sceneAcquireBattleRef(0);
            work->patrols = enemy->place->mode & ACTOR_02000_PLACEMENT_PATROLS;
            if (work->patrols == 0) {
                work->anim     = ACTOR_02000_IDLE_ANIM;
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_IDLE;
            } else {
                work->anim               = GOLEM_PAWN_ROOK_ANIM_WALK;
                work->behavior           = GOLEM_PAWN_ROOK_BEHAVIOR_PATROL;
                patrolLengthThousands    = enemy->place->variant;
                work->patrolDistanceLeft = patrolLengthThousands * ACTOR_02000_PATROL_UNITS_PER_VARIANT;
            }

            // The CD request reads key bytes 3/2/0 and only four argument bytes.
            areaSoundSets = _gActor02000StageAreaSoundSets[gGameSession->location.stage];
            if (areaSoundSets != NULL) {
                work->soundSet = areaSoundSets[gGameSession->location.area];
            }
            if (work->soundSet != 0) {
                soundFileKey[3]  = 0;
                soundFileKey[2]  = ACTOR_02000_SOUND_FILE_GROUP;
                soundFileKey[0]  = work->soundSet;
                soundFileArgs[0] = ACTOR_02000_ID;
                soundFileArgs[3] = 0;
                soundFileArgs[2] = 0;
                soundFileArgs[1] = 0;
                cdCmdEnqueue(CD_COMMAND_LOAD_FILE, soundFileKey, soundFileArgs);
            }

            // Four collision bodies borrow contact storage from this work block.
            work->sightCapsule.ends[0].vz   = ACTOR_02000_SIGHT_LENGTH;
            work->sightCapsule.end0Radius   = ACTOR_02000_SIGHT_FAR_RADIUS;
            work->sightCapsule.ends[0].vx   = 0;
            work->sightCapsule.ends[0].vy   = 0;
            work->sightCapsule.ends[1].vx   = 0;
            work->sightCapsule.ends[1].vy   = 0;
            work->sightCapsule.ends[1].vz   = 0;
            work->sightCapsule.end1Radius   = ACTOR_02000_SIGHT_NEAR_RADIUS;
            work->sightCapsule.contacts     = work->sightContacts;
            sightCoords                     = actor->extra.tmd->coords;
            work->sightBody.context.capsule = &work->sightCapsule;
            work->sightBody.pos.vx          = 0;
            work->sightBody.pos.vy          = 0;
            work->sightBody.pos.vz          = 0;
            work->sightBody.key             = 0;
            work->sightBody.radius          = 0;
            work->sightBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
            work->sightBody.coord           = &sightCoords[ACTOR_02000_SIGHT_PART];
            ACTOR_02000_LINK_BODY_CONTACTS(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->sightBody, work->sightContacts,
                                           ARRAY_SIZE(work->sightContacts), (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));

            hurtCoords                      = actor->extra.tmd->coords;
            work->hurtBody.context.contacts = work->hurtContacts;
            work->hurtBody.pos.vx           = 0;
            work->hurtBody.pos.vy           = 0;
            work->hurtBody.pos.vz           = 0;
            work->hurtBody.key              = WORLD_COLLISION_CONTACT_ENEMY_BODY | ACTOR_02000_ID;
            work->hurtBody.radius           = ACTOR_02000_HURT_RADIUS;
            work->hurtBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->hurtBody.coord            = &hurtCoords[ACTOR_02000_BODY_PART];
            ACTOR_02000_LINK_BODY_CONTACTS(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hurtBody, work->hurtContacts,
                                           ARRAY_SIZE(work->hurtContacts), WORLD_COLLISION_BODY_PAIR_ENABLED);

            groundCoords                      = actor->extra.tmd->coords;
            work->groundBody.pos.vy           = -ACTOR_02000_GROUND_RADIUS;
            work->groundBody.context.contacts = work->groundContacts;
            work->groundBody.pos.vx           = 0;
            work->groundBody.pos.vz           = 0;
            work->groundBody.key              = 0;
            work->groundBody.radius           = ACTOR_02000_GROUND_RADIUS;
            work->groundBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->groundBody.coord            = groundCoords;
            ACTOR_02000_LINK_BODY_CONTACTS(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->groundBody, work->groundContacts,
                                           ARRAY_SIZE(work->groundContacts), (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED));

            swordCoords                       = swordEnemy->task->extra.tmd->coords;
            work->strikeBody.context.contacts = work->strikeContacts;
            work->strikeBody.pos.vx           = 0;
            work->strikeBody.pos.vy           = ACTOR_02000_STRIKE_OFFSET_Y;
            work->strikeBody.pos.vz           = 0;
            work->strikeBody.key              = 0;
            work->strikeBody.radius           = ACTOR_02000_STRIKE_RADIUS;
            work->strikeBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->strikeBody.coord            = swordCoords;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->strikeBody);
            worldCollisionInitContacts(work->strikeContacts, ARRAY_SIZE(work->strikeContacts), 0);
            work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            actor->state            = GOLEM_PAWN_ROOK_TASK_RUNNING;
            break;
        case GOLEM_PAWN_ROOK_DOWNED_BEHIND:
            work->anim   = ACTOR_02000_CORPSE_BEHIND_ANIM;
            work->step   = ACTOR_02000_RESTORED_CORPSE_STEP;
            actor->state = GOLEM_PAWN_ROOK_TASK_TEARDOWN;
            break;
        case GOLEM_PAWN_ROOK_DOWNED_FRONT:
            work->anim   = ACTOR_02000_CORPSE_FRONT_ANIM;
            work->step   = ACTOR_02000_RESTORED_CORPSE_STEP;
            actor->state = GOLEM_PAWN_ROOK_TASK_TEARDOWN;
            break;
    }
#undef ACTOR_02000_LINK_BODY_CONTACTS
}
#include "../../shared/creatures/golem/golem_pawn_rook_inlines.inc.c"

#include "../../shared/creatures/golem/golem_pawn_rook_frame_no_sparks.inc.c"

/// Faces and approaches the player, then selects a Beam Sword swing or charge.
///
/// Requires initialized GOLEM Pawn body work, a live model root and the player's
/// matrix in the same coordinate frame. Steps 0..3 track, choose an attack,
/// listen and turn; other steps do nothing. Bearing narrows player offsets to
/// signed halfwords and uses 4096 units per turn; attack range uses full-word
/// X/Z offsets whose squared sum must fit a signed word. Updates movement and
/// animation requests for the enclosing tick rather than moving the root here.
/// Borrows one VECTOR-sized scratch-stack reservation, leaving its Y unused,
/// and restores the cursor on every path. All pointed-to state stays borrowed.
static void _golemPawnRookEngageState(Task* task)
{
    enum {
        GOLEM_PAWN_ROOK_ENGAGE_TRACK           = 0,
        GOLEM_PAWN_ROOK_ENGAGE_PICK_ATTACK     = 1,
        GOLEM_PAWN_ROOK_ENGAGE_LISTEN          = GOLEM_PAWN_ROOK_ENGAGE_LISTEN_STEP,
        GOLEM_PAWN_ROOK_ENGAGE_TURN            = 3,
        GOLEM_PAWN_ROOK_ENGAGE_ANIM_TURN       = 3,
        GOLEM_PAWN_ROOK_ENGAGE_SPEED           = 20,
        GOLEM_PAWN_ROOK_ENGAGE_TURN_THRESHOLD  = 1409,
        GOLEM_PAWN_ROOK_ENGAGE_SIGHT_YAW_LIMIT = 128,
        GOLEM_PAWN_ROOK_ENGAGE_LISTEN_FRAMES   = 96,
        GOLEM_PAWN_ROOK_ENGAGE_TURN_FRAMES     = 35,
        GOLEM_PAWN_ROOK_ENGAGE_TURN_RATE       = 59,
    };
    s16                currentYaw;
    s16                listenYaw;
    s16                step;
    s16                yawDelta;
    s16                listenYawDelta;
    s16                forwardSpeed;
    s32                absoluteYawDelta;
    s32                absoluteListenYawDelta;
    s16                wrappedYawDelta;
    s16                wrappedListenYawDelta;
    s16                yawError;
    s32                playerOffsetX;
    s32                playerOffsetZ;
    u16                sightFlags;
    u16                listenSightFlags;
    VECTOR*            scratchTop;
    VECTOR*            toPlayer;
    GolemPawnRookWork* work;
    GfxCoord*          rootCoord;

    /// Stores player/current headings and measures their nonnegative shortest gap.
    ///
    /// work borrows live GOLEM body work; rootCoord and toPlayer supply facing
    /// and X/Z offset in the same parent frame. Bearing deliberately narrows
    /// offsets to s16. Headings use 4096 units per turn, mask to 0..4095 and
    /// overwrite targetYaw/yaw. yawValue, deltaValue, wrappedValue and errorValue
    /// are distinct writable s16 lvalues; magnitudeValue is a distinct s32.
    /// deltaValue is -4095..4095 and errorValue is 0..2048. At exactly half a
    /// turn the wrap branch runs; only that branch writes wrappedValue.
    /// Arguments are side-effect-free and evaluated repeatedly. Captures no
    /// caller locals. Invoke as a standalone compound statement; undefined below.
#define GOLEM_PAWN_ROOK_MEASURE_PLAYER_YAW_GAP(work, rootCoord, toPlayer, yawValue, deltaValue, magnitudeValue, wrappedValue, errorValue) \
    {                                                                                                                                     \
        (work)->targetYaw = ratan2((s16)(toPlayer)->vx, (s16)(toPlayer)->vz) & ACTOR_TRANSFORM_ANGLE_MASK;                                \
        (yawValue)        = ratan2((rootCoord)->coord.m[0][2], (rootCoord)->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;                  \
        (work)->yaw       = (yawValue);                                                                                                   \
        (deltaValue)      = (work)->targetYaw - (yawValue);                                                                               \
        (magnitudeValue)  = __builtin_abs((deltaValue));                                                                                  \
        if ((magnitudeValue) < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {                                                                         \
            (errorValue) = (magnitudeValue);                                                                                              \
        } else {                                                                                                                          \
            if ((deltaValue) > 0) {                                                                                                       \
                (wrappedValue) = ACTOR_TRANSFORM_ANGLE_TURN - (deltaValue);                                                               \
            } else {                                                                                                                      \
                (wrappedValue) = (deltaValue) + ACTOR_TRANSFORM_ANGLE_TURN;                                                               \
            }                                                                                                                             \
            (errorValue) = (wrappedValue);                                                                                                \
        }                                                                                                                                 \
    }

    scratchTop                   = SCRATCH_STACK_CURSOR(VECTOR);
    toPlayer                     = scratchTop - 1;
    SCRATCH_STACK_CURSOR(VECTOR) = toPlayer;
    work                         = task->work;
    step                         = work->step;
    rootCoord                    = task->extra.tmd->coords;
    switch (step) {
        // Approach after the blend, opening sight only inside the narrow yaw cone.
        case GOLEM_PAWN_ROOK_ENGAGE_TRACK:
            forwardSpeed = 0;
            if (work->animFrame >= _gGolemPawnRookAnimBlendFrames[work->anim]) {
                forwardSpeed = GOLEM_PAWN_ROOK_ENGAGE_SPEED;
            }
            work->forwardSpeed = forwardSpeed;
            work->turnRate     = GOLEM_PAWN_ROOK_ENGAGE_TRACK_TURN_RATE;
            toPlayer->vx       = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
            toPlayer->vz       = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
            GOLEM_PAWN_ROOK_MEASURE_PLAYER_YAW_GAP(work, rootCoord, toPlayer, currentYaw, yawDelta, absoluteYawDelta, wrappedYawDelta, yawError);
            if (yawError >= GOLEM_PAWN_ROOK_ENGAGE_TURN_THRESHOLD) {
                if (work->turnedAround == 0) {
                    work->anim = GOLEM_PAWN_ROOK_ENGAGE_ANIM_TURN;
                    work->step = GOLEM_PAWN_ROOK_ENGAGE_TURN;
                } else {
                    work->anim         = GOLEM_PAWN_ROOK_ANIM_LISTEN;
                    work->step         = GOLEM_PAWN_ROOK_ENGAGE_PICK_ATTACK;
                    work->turnedAround = 0;
                }
            }
            if (yawError < GOLEM_PAWN_ROOK_ENGAGE_SIGHT_YAW_LIMIT) {
                sightFlags            = work->sightBody.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->sightBody.flags = sightFlags;
                if (work->playerSpotted != 0) {
                    work->sightBody.flags = sightFlags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                    work->step            = GOLEM_PAWN_ROOK_ENGAGE_PICK_ATTACK;
                    work->turnedAround    = 0;
                }
            }
            break;
        // Commit to a close swing or a running charge using the full X/Z distance.
        case GOLEM_PAWN_ROOK_ENGAGE_PICK_ATTACK:
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            toPlayer->vx       = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
            playerOffsetZ      = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
            toPlayer->vz       = playerOffsetZ;
            playerOffsetX      = toPlayer->vx;
            if (SquareRoot0((playerOffsetX * playerOffsetX) + (playerOffsetZ * playerOffsetZ)) < GOLEM_PAWN_ROOK_CLOSE_ATTACK_RANGE) {
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_SWORD_SWING;
                work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                work->anim     = GOLEM_PAWN_ROOK_CLOSE_ATTACK_ANIM;
            } else {
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_SWORD_CHARGE;
                work->step     = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
                work->anim     = GOLEM_PAWN_ROOK_WEAPON_APPROACH_ANIM;
                work->timer    = 0;
            }
            break;
        // A missed approach listens for contact, then retries facing after the hold.
        case GOLEM_PAWN_ROOK_ENGAGE_LISTEN:
            work->forwardSpeed    = 0;
            work->turnRate        = 0;
            listenSightFlags      = work->sightBody.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->sightBody.flags = listenSightFlags;
            if (work->playerSpotted != 0) {
                work->sightBody.flags = listenSightFlags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                work->anim            = GOLEM_PAWN_ROOK_ANIM_WALK;
                work->step            = GOLEM_PAWN_ROOK_ENGAGE_TRACK;
            } else if (work->animFrame >= GOLEM_PAWN_ROOK_ENGAGE_LISTEN_FRAMES) {
                toPlayer->vx = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
                toPlayer->vz = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
                GOLEM_PAWN_ROOK_MEASURE_PLAYER_YAW_GAP(work, rootCoord, toPlayer, listenYaw, listenYawDelta, absoluteListenYawDelta, wrappedListenYawDelta, yawError);
                if (yawError >= GOLEM_PAWN_ROOK_ENGAGE_TURN_THRESHOLD) {
                    work->anim = GOLEM_PAWN_ROOK_ENGAGE_ANIM_TURN;
                    work->step = GOLEM_PAWN_ROOK_ENGAGE_TURN;
                } else {
                    work->anim = GOLEM_PAWN_ROOK_ANIM_WALK;
                    work->step = GOLEM_PAWN_ROOK_ENGAGE_TRACK;
                }
            }
            break;
        case GOLEM_PAWN_ROOK_ENGAGE_TURN:
            work->forwardSpeed = 0;
            work->turnRate     = GOLEM_PAWN_ROOK_ENGAGE_TURN_RATE;
            if (work->animFrame >= GOLEM_PAWN_ROOK_ENGAGE_TURN_FRAMES) {
                work->anim         = GOLEM_PAWN_ROOK_ANIM_WALK;
                work->step         = GOLEM_PAWN_ROOK_ENGAGE_TRACK;
                work->turnedAround = 1;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);

#undef GOLEM_PAWN_ROOK_MEASURE_PLAYER_YAW_GAP
}

#include "../../shared/actor/player_detection_segment.inc.c"

#include "../../shared/creatures/golem/golem_pawn_rook.inc.c"

/// Runs the Pawn GOLEM's attached Beam Sword lifecycle.
///
/// Requires a live sword Enemy in `spawnArg2.pointer`, its TMD model and parent
/// body work. The unchecked state is 0 attachment at body part 7, 1 visibility
/// mirroring and delayed trail spawn, or 2 destruction. Lighting is borrowed
/// from the body; sword teardown also releases its successfully spawned trail.
static void _actor02000SwordTask(Task* sword)
{
    const EnemyTaskFuncTable3 stateHandlers = _gActor02000SwordLifecycleHandlers;

    stateHandlers.funcs[sword->state](sword->spawnArg2.pointer, sword);
}

#include "../../shared/creatures/golem/golem_pawn_rook_sword_spawn.inc.c"

/// Runs the Beam Sword Pawn GOLEM body's lifecycle.
///
/// Requires a live body Enemy in `spawnArg2.pointer` and a nineteen-part model.
/// The unchecked state is 0 allocation/setup, 1 combat update, or 2 persistent
/// corpse update. Spawn owns the work shared by attached children; a handler
/// may destroy the Enemy/task pair, so nothing is read after dispatch.
static void _actor02000BodyTask(Task* body)
{
    const EnemyTaskFuncTable3 stateHandlers = _gActor02000BodyLifecycleHandlers;

    stateHandlers.funcs[body->state](body->spawnArg2.pointer, body);
}

/// Attached Beam Sword lifecycle: 0 attach, 1 follow body/spawn trail, 2 destroy.
///
/// Each handler receives the live sword Enemy/task pair. Attachment borrows
/// body work and lighting; destruction releases the attached trail as well.
static const EnemyTaskFuncTable3 _gActor02000SwordLifecycleHandlers = { {
    _golemPawnRookSwordSpawn,
    _golemPawnRookSwordTick,
    enemyDestroy,
} };

/// Beam Sword Pawn GOLEM lifecycle: 0 initialize, 1 combat frame, 2 persistent corpse.
///
/// Each handler receives the live body Enemy/task pair. State 2 advances and
/// records the dead pose; it does not request task destruction.
static const EnemyTaskFuncTable3 _gActor02000BodyLifecycleHandlers = { {
    _actor02000SpawnBody,
    _golemPawnRookFrameStateNoSparks,
    _golemPawnRookDeadState,
} };

PACKAGE_ALIASES
