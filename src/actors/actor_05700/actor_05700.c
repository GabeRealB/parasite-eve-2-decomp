#include "actors/actor_05700.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

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
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/sound_ids.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"
#include "../../shared/actor/player_detection.h"
/// Selects shielded Rook behavior for this Grenade Launcher GOLEM carrier.
#define GOLEM_PAWN_ROOK_KIND GOLEM_PAWN_ROOK_KIND_ROOK
/// Selects the shared Grenade Launcher build for this shielded Rook GOLEM carrier.
///
/// Keep this binding through all GOLEM fragments, with matching weapon tasks,
/// behavior callbacks and animation tables.
#define GOLEM_PAWN_ROOK_WEAPON GOLEM_PAWN_ROOK_WEAPON_GRENADE_LAUNCHER
#include "../../shared/creatures/golem/golem_pawn_rook.h"

/// Character script bank used by this carrier's cue tables.
enum { GOLEM_PAWN_ROOK_SOUND_BANK = 0x39 };

extern AnimationSet gGolemIdleClip;
extern AnimationSet gGolemWalkClip;
static AnimationSet _gGolemTurnClip;
static AnimationSet _gGolemListenClip;
static AnimationSet _gGolemRookScreamChargeClip;
extern AnimationSet gGolemScreamReleaseClip;
static AnimationSet _gGolemRookGrenadeLauncherBurstStartClip;
static AnimationSet _gGolemFireClip;
static AnimationSet _gGolemBurstCancelClip;
static AnimationSet _gGolemRookGrenadeLauncherReloadClip;
static AnimationSet _gGolemLauncherStrikeClip;
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
static AnimationSet _gGolemRookGrenadeLauncherBackAwayClip;
static TmdSource    _gGolemRookBody;
static TmdSource    _gGolemGrenadeLauncher;
static TmdSource    _gGolemRookShield;
extern TmdSource    gGolemGrenade;
static void         _actor05700GrenadeLauncherTask(Task* launcher);
static void         _actor05700GrenadeTask(Task* grenade);
static void         _actor05700ShieldTask(Task* shield);
static void         _actor05700RookGolemTask(Task* golem);

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

#include "assets/enemies/golem/grenade_launcher.inc"

#include "assets/enemies/golem/rook/shield.inc"

#include "assets/enemies/golem/grenade.inc"

#include "assets/enemies/golem/rook/grenade_launcher/package_1_clips.inc"

/// Five category-4 attack rows, storing base HP damage and player reaction.
///
/// Slots 0/1 are sword swing/charge, 2 is the zero-power Silence row,
/// 3 is the grenade and 4 is the launcher strike. Unused weapon rows remain
/// present so every carrier uses the same attack indices.
static DamageAttack _gGolemPawnRookAttacks[5] = {
    { 30, GAME_ACTOR_REACTION_HIT_FLASHES },
    { 20, GAME_ACTOR_REACTION_HIT_FLASHES },
    { 0, GAME_ACTOR_REACTION_SILENCE },
    { 20, GAME_ACTOR_REACTION_PARALYSIS },
    { 30, GAME_ACTOR_REACTION_SPARK_AND_PUFFS },
};

/// Rook GOLEM launcher combat, reaction and release-reward parameters.
///
/// One shared record per loaded carrier, borrowed by newly linked live enemies.
/// Saved corpses bypass combat initialization. Its five attack rows remain live
/// with this record; grenades also pack damage directly from that attack table.
static EnemyParams _gGolemPawnRookParams = {
    .attacks              = _gGolemPawnRookAttacks,
    .hpMax                = 482,
    .exp                  = 250,
    .bp                   = 400,
    .mp                   = 8,
    .critChance           = 0,
    .buildupSteps         = 6,
    .damageOverTimeChance = 0,
    .damageOverTimeTicks  = 0,
};

/// Raised-shield absorption flags for weapon attack rows.
///
/// Signed halfword values are 0 bypass and 1 absorb. The low seven attack-key
/// bits must select a row 0..45; bit 15 is clear for this family.
/// A raised shield covers frontal hits while standing; absorption spends shield HP
/// instead of body HP. Pawn carriers retain the same table without a shield child.
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

/// Raised-shield absorption flags for Parasite Energy attack rows.
///
/// Signed halfword values are 0 bypass and 1 absorb. The low seven attack-key
/// bits must select a row 0..55; bit 15 selects this PE family.
/// A raised shield covers frontal hits while standing; absorption spends shield HP
/// instead of body HP. Pawn carriers retain the same table without a shield child.
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

/// Character-bank script ID played when a grenade detaches from its launcher.
///
/// The stored instance byte is zero; the grenade inserts its placement index
/// in bits 8..15 before requesting the spatialized sound.
static s32 _gGolemPawnRookGrenadeLaunchCue = SOUND_CHARACTER(GOLEM_PAWN_ROOK_SOUND_BANK, 7);

/// Sound id of the cue a grenade plays where its flight ends; the bullet ORs
/// the firing enemy's place index into bits 8 and up before queueing it.
s32 gGolemPawnRookImpactSound = 0x40390008;

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

/// Stage-1 area-indexed motion sound-bank selectors for the launcher Rook.
///
/// Published area IDs 0..20 index directly; the final zero halfword is unused.
/// Zero disables bank loading and motion cues; 1..4 select a sound file and
/// its footstep/fall cue set. All 22 stored halfwords are retained.
static u16 _gActor05700Stage1AreaSoundSets[22] = {
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

/// Stage-2 area-indexed motion sound-bank selectors for the launcher Rook.
///
/// Published area IDs 0..38 index directly; the final zero halfword is unused.
/// Zero disables loading and motion cues; 1..4 select the sound file and
/// footstep/fall cue set. All 40 stored halfwords are retained.
static u16 _gActor05700Stage2AreaSoundSets[40] = {
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

/// Stage-3 area-indexed motion sound-bank selectors for the launcher Rook.
///
/// Published area IDs 0..38 index directly; the final zero halfword is unused.
/// Zero disables loading and motion cues; 1..4 select the sound file and
/// footstep/fall cue set. All 40 stored halfwords are retained.
static u16 _gActor05700Stage3AreaSoundSets[40] = {
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

/// Stage-4 area-indexed motion sound-bank selectors for the launcher Rook.
///
/// Published area IDs 0..49 index the 50 halfwords directly; 0 disables loading and motion
/// cues, and 1..4 select the sound file and footstep/fall cue set.
static u16 _gActor05700Stage4AreaSoundSets[50] = {
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

/// Stage-5 area-indexed motion sound-bank selectors for the launcher Rook.
///
/// Published area IDs 0..33 index the 34 halfwords directly; 0 disables loading and motion
/// cues, and 1..4 select the sound file and footstep/fall cue set.
static u16 _gActor05700Stage5AreaSoundSets[34] = {
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

/// Launcher GOLEM sound-set selectors indexed first by stage, then by area.
///
/// Stage 0 is NULL and leaves the zero-filled work's soundSet at 0 (no bank or
/// animation cues). Stages 1..5 borrow area-indexed halfwords containing 0..4;
/// nonzero values select the sound file and its footstep/fall cue variant.
/// Stages 1..5 have 22, 40, 40, 50 and 34 area entries, respectively; the spawn
/// has no bounds check. Rows remain borrowed from this carrier until the body
/// and its cue playback end.
static u16* _gGolemPawnRookStageAreaSoundSets[6] = {
    NULL,
    _gActor05700Stage1AreaSoundSets,
    _gActor05700Stage2AreaSoundSets,
    _gActor05700Stage3AreaSoundSets,
    _gActor05700Stage4AreaSoundSets,
    _gActor05700Stage5AreaSoundSets,
};

/// Projected vertex indices of the laser sight's two fading ribbon quads.
///
/// Each row gives GPU corners 0..3. Indices 0/1 are centre-line endpoints;
/// 2/3 and 4/5 are their two edges. Every index addresses a six-element X/Y
/// scratch array; the signed halfword representation and two rows are retained.
static s16 _gGolemPawnRookBeamRibbonCorners[2][4] = {
    { 0, 1, 2, 3 },
    { 0, 1, 4, 5 },
};

TaskDesc gActor05700RookGolemTasks[4] = {
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _actor05700RookGolemTask, { .model = &_gGolemRookBody } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _actor05700GrenadeLauncherTask, { .model = &_gGolemGrenadeLauncher } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _actor05700GrenadeTask, { .model = &gGolemGrenade } },
    { { { TASK_BODY_TMD, TASK_PRIORITY_ACTOR } }, _actor05700ShieldTask, { .model = &_gGolemRookShield } },
};

/// Launcher GOLEM body clip bank indexed by animation selectors 0..30.
///
/// Slot 0 and unavailable weapon/kind clips are NULL. The nineteen-part body
/// rig borrows the bank while loaded: idle/walk/turn/listen occupy 1..4,
/// launcher attack clips 12..16/30, hit reactions 17..20, and downed poses 22..29.
/// The Rook plays scream clips 10/11 and recovery clip 21; the Pawn retains
/// clip 21 but has no scream behavior.
#include "assets/enemies/golem/rook/grenade_launcher/bank.inc"

/// Combat behavior callbacks indexed by `GOLEM_PAWN_ROOK_BEHAVIOR_*`.
///
/// Indices 0..14 require live body work; unsupported weapon/kind behaviors
/// use the no-op handler. These are separate from the body task lifecycle.
static TaskFunc _gGolemPawnRookBehaviorHandlers[GOLEM_PAWN_ROOK_BEHAVIOR_DOWNED_DEATH + 1] = {
    _golemPawnRookIdleState,
    _golemPawnRookPatrolState,
    _golemPawnRookEngageState,
    _golemPawnRookNopState,
    _golemPawnRookNopState,
    _golemPawnRookSilenceScreamState,
    _golemPawnRookGrenadeBurstState,
    _golemPawnRookLauncherStrikeState,
    _golemPawnRookStaggerState,
    _golemPawnRookRecoilState,
    _golemPawnRookBuildupState,
    _golemPawnRookKnockdownState,
    _golemPawnRookDownedHitState,
    _golemPawnRookCollapseState,
    _golemPawnRookDownedDeathState,
};

/// Animation bank the work block's animation context is started on.

/// Enemy parameter record the spawn hands to its `Enemy`.

#include "../../shared/creatures/golem/golem_pawn_rook_take_hits.inc.c"

#include "../../shared/creatures/golem/golem_pawn_rook_patrol.inc.c"

#include "../../shared/creatures/golem/golem_pawn_rook_grenade_burst.inc.c"

/// Rook GOLEM launcher lifecycle: 0 attach, 1 mirror flags/fire, 2 destroy.
///
/// The launcher borrows the live body's work, lighting and part-7 coordinate.
/// Dispatch copies all three handlers before selecting an unchecked state.
/// Destruction releases the launcher's Enemy and begins task teardown.
static const EnemyTaskFuncTable3 _gActor05700GrenadeLauncherStates = {
    _golemPawnRookLauncherSpawn,
    _golemPawnRookLauncherTick,
    enemyDestroy,
};

#include "../../shared/creatures/golem/golem_pawn_rook_lunge_strike.inc.c"

#include "../../shared/creatures/golem/golem_pawn_rook_silence_scream.inc.c"

/// Selects the Rook GOLEM's launcher spawn table for the shared spawn code.
///
/// Binds a writable `TaskDesc[4]`: 0 body, 1 launcher, 2 grenade, 3 shield.
/// The body borrows the array for later grenade spawns; keep the array,
/// callbacks and geometry loaded until all spawned tasks, including detached
/// grenades, have been released. Only the following fragment uses this binding.
#define GOLEM_PAWN_ROOK_TASK_DESCRIPTORS gActor05700RookGolemTasks
#include "../../shared/creatures/golem/golem_pawn_rook_spawn.inc.c"
#undef GOLEM_PAWN_ROOK_TASK_DESCRIPTORS

#include "../../shared/creatures/golem/golem_pawn_rook_inlines.inc.c"

#include "../../shared/creatures/golem/golem_pawn_rook_frame.inc.c"

#include "../../shared/creatures/golem/golem_pawn_rook_lunge_cycle.inc.c"

#include "../../shared/actor/player_detection_segment.inc.c"

#include "../../shared/creatures/golem/golem_pawn_rook.inc.c"

/// Dispatches the Rook's attached grenade-launcher task.
///
/// `launcher` is a live TMD task with its owned `Enemy` in `spawnArg2.pointer`.
/// Its unchecked `state` is 0 to attach to body part 7, 1 to mirror the body's
/// visibility and consume a fire request, or 2 to destroy the child. The body
/// must remain alive while its child borrows the body's work and lighting.
/// Destruction may release the task and enemy before returning.
static void _actor05700GrenadeLauncherTask(Task* launcher)
{
    EnemyTaskFuncTable3 lifecycleHandlers = _gActor05700GrenadeLauncherStates;

    lifecycleHandlers.funcs[launcher->state](launcher->spawnArg2.pointer, launcher);
}

#include "../../shared/creatures/golem/golem_pawn_rook_launcher.inc.c"

/// Grenade lifecycle states: 0 launch/detach, 1 flight, 2 delayed destruction.
///
/// The dispatcher copies all three handlers before indexing by task state.
/// State 2 unlinks collision, then keeps the grenade alive for 61 update ticks.
static const EnemyTaskFuncTable3 _gActor05700GrenadeStates = {
    _golemPawnRookGrenadeSpawn,
    _golemPawnRookGrenadeFly,
    _golemPawnRookGrenadeDestroy,
};

/// Dispatches a grenade fired by the Rook's launcher.
///
/// `grenade` is a live TMD task with its owned `Enemy` in `spawnArg2.pointer`.
/// Its unchecked `state` is 0 to launch and detach from the launcher, 1 to fly
/// until impact or timeout, or 2 to unlink collision and wait 61 frames before
/// destruction. State 0 requires a live launcher parent; later states use the
/// grenade's own work and lighting. Destruction may release the task and enemy.
static void _actor05700GrenadeTask(Task* grenade)
{
    EnemyTaskFuncTable3 lifecycleHandlers = _gActor05700GrenadeStates;

    lifecycleHandlers.funcs[grenade->state](grenade->spawnArg2.pointer, grenade);
}

#include "../../shared/creatures/golem/golem_pawn_rook_grenade_destroy.inc.c"

/// Shield lifecycle states: 0 attach, 1 follow/break, 2 destroy.
///
/// The shield attaches to body part 11 and borrows the live body's work and
/// lighting. The dispatcher copies all three handlers before indexing by task state.
static const EnemyTaskFuncTable3 _gActor05700ShieldStates = {
    _golemPawnRookShieldSpawn,
    _golemPawnRookShieldTick,
    enemyDestroy,
};

/// Dispatches the Rook's attached shield task through its break and destruction.
///
/// `shield` is a live TMD task with its owned `Enemy` in `spawnArg2.pointer`.
/// Its unchecked `state` is 0 to attach to body part 11, 1 to mirror the body's
/// visibility and service its shield-break request, or 2 to destroy the child.
/// The body must remain alive while the shield borrows its work and lighting.
/// Destruction may release the task and enemy before returning.
static void _actor05700ShieldTask(Task* shield)
{
    EnemyTaskFuncTable3 lifecycleHandlers = _gActor05700ShieldStates;

    lifecycleHandlers.funcs[shield->state](shield->spawnArg2.pointer, shield);
}

#include "../../shared/creatures/golem/golem_pawn_rook_shield_spawn.inc.c"

/// Launcher Rook body lifecycle states: 0 initialize, 1 combat, 2 persistent corpse.
///
/// The dispatcher copies all three handlers before indexing by task state.
/// The dead state ends combat participation while retaining the body and pose.
static const EnemyTaskFuncTable3 _gActor05700RookGolemStates = {
    _golemPawnRookSpawn,
    _golemPawnRookFrameState,
    _golemPawnRookDeadState,
};

#include "../../shared/creatures/golem/golem_pawn_rook_shield_tick.inc.c"

/// Dispatches the grenade-launcher Rook GOLEM's body task.
///
/// `golem` is a live nineteen-part TMD task with its owned `Enemy` in
/// `spawnArg2.pointer`. Its unchecked `state` is 0 to initialize the body and
/// attached launcher and shield, 1 to run combat behaviour, or 2 to retain and
/// draw its dead body after ending collision and target tracking. Spawn failure
/// may release the task and enemy before returning.
static void _actor05700RookGolemTask(Task* golem)
{
    EnemyTaskFuncTable3 lifecycleHandlers = _gActor05700RookGolemStates;

    lifecycleHandlers.funcs[golem->state](golem->spawnArg2.pointer, golem);
}
