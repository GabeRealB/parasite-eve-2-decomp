#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/animation.h"
#include "gameplay/enemy.h"
#include "gameplay/loading.h"
#include "gameplay/scene_runtime.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

/// Steps of the palette greying, kept in `_Actor311900Work::clutGreyStep`.
///
/// A figure's tick performs one step a frame and moves on to the next, so the
/// read-back, the conversion and the upload fall on three successive frames.
enum {
    ACTOR_311900_CLUT_GREY_READ    = 0, // Read the figure's two CLUT rows back from VRAM
    ACTOR_311900_CLUT_GREY_CONVERT = 1, // Raise the three channels of every entry to its brightest one
    ACTOR_311900_CLUT_GREY_UPLOAD  = 2, // Write the two rows back to VRAM
    ACTOR_311900_CLUT_GREY_DONE    = 3, // Greyed; the tick leaves the rows alone
};

/// Ticks the advancing figure moves forward for, counted in
/// `_Actor311900Work::advanceFrames`, before its scene is flagged done and its
/// task moves on to its teardown.
enum { ACTOR_311900_ADVANCE_FRAMES = 0x5A };

/// Work block of either figure the package shows, allocated zeroed at its full
/// size by the figure's spawn state and kept at `Task::work` for the task's
/// life.
///
/// It opens with the rig of the figure's twenty-part body model and the
/// animation request its update serves, in the steps an enemy's animation
/// takes (`ACTOR_ENEMY_ANIM_BLEND`, `_RESET`, `_TICK`). Each spawn requests
/// clip 1 from its start and nothing requests another, so the blended step is
/// served but never asked for. The model object borrows `light` and `color`
/// for as long as the block lives.
///
/// What follows the matrices is the figures' own. Each is drawn only while its
/// own view of the room is up and greys its texture palette over its first
/// three ticks. The figure of the package's first state table also moves
/// forward along its own Z axis once its view has been shown, and ends its
/// scene after `ACTOR_311900_ADVANCE_FRAMES` ticks of that; the other figure
/// leaves `advanceFrames` and `advancing` zero.
typedef struct {
    ActorAnimRig20 rig;           // Playback storage of the twenty-part body model; slots 1 to 19 are driven
    s16            animState;     // Step of the animation (0 none, else `ACTOR_ENEMY_ANIM_BLEND`, `_RESET` or `_TICK`)
    s16            appliedAnimId; // Clip the slots were last seeded with; recorded, never read
    s16            animId;        // Clip the next reseed selects, an index into the figure's animation set table
    u16            animFrames;    // Updates since the last reseed; counted, never read
    u8             animRate;      // Rate given every slot ahead of a reseed, in sixteenths of a frame; nothing sets it, so it is 0, which a reseed from the clip's start replaces with `ANIMATION_RATE_ONE`
    byte           pad_47D[0x7];  // Never accessed
    MATRIX         light;         // Light-direction matrix lent to the model object
    MATRIX         color;         // Light-colour matrix lent to the model object
    s16            advanceFrames; // Ticks the figure has moved forward for; reaching `ACTOR_311900_ADVANCE_FRAMES` ends its scene
    s16            advancing;     // Set once the figure's view has been shown, never cleared (0 standing where it was placed, 1 moving forward every tick)
    u8             clutGreyStep;  // Step of the palette greying (`ACTOR_311900_CLUT_GREY_*`)
} _Actor311900Work;
STATIC_ASSERT_SIZEOF(_Actor311900Work, 0x4CC);

/// The animation data `animationInitContext` builds the first setup path's clip
/// context from; the spawn hands it over whole, so it is only ever a byte
/// address here.
extern u8 D_actor_311900_8016EBE8[];

/// The animation data the second setup path builds its clip context from.
extern u8 D_actor_311900_8016EBF4[];

/// The palette rows `func_actor_311900_80161E3C` reads back, greys and uploads.
extern u16 D_actor_311900_8016EC18[][0x100];

static void func_actor_311900_8016228C(Enemy* enemy, Task* task);
static void func_actor_311900_801623B0(Enemy* enemy, Task* task);
static void func_actor_311900_801624F8(Enemy* enemy, Task* task);
static void func_actor_311900_801625F0(Enemy* enemy, Task* task);
static s32  func_actor_311900_80162658(GfxCoord* arg0, s16 arg1);
static void func_actor_311900_8016278C(Task* task);
static void func_actor_311900_8016281C(Task* task);

static TmdSource _gActor311900RupertBroderickBody1;
static TmdSource _gActor311900SwatMember2Body;
void             func_actor_311900_8016222C(Task*);
void             func_actor_311900_8016249C(Task*);

static TmdBone _gActor311900RupertBroderickBody1Skeleton[20] = {
#include "assets/rupert_broderick_body_1_skeleton.inc"
};

static u32 _gActor311900RupertBroderickBody1PartVerts[20] = {
#include "assets/rupert_broderick_body_1_partVerts.inc"
};

static SVECTOR _gActor311900RupertBroderickBody1Verts[386] = {
#include "assets/rupert_broderick_body_1_verts.inc"
};

static SVECTOR _gActor311900RupertBroderickBody1Normals[385] = {
#include "assets/rupert_broderick_body_1_normals.inc"
};

static u32 _gActor311900RupertBroderickBody1Stream[4327] = {
#include "assets/rupert_broderick_body_1_stream.inc"
};

static TmdSource _gActor311900RupertBroderickBody1 = {
    0,
    23980,
    6012,
    20,
    _gActor311900RupertBroderickBody1PartVerts,
    _gActor311900RupertBroderickBody1Verts,
    _gActor311900RupertBroderickBody1Normals,
    _gActor311900RupertBroderickBody1Skeleton,
    _gActor311900RupertBroderickBody1Stream,
};

static TmdBone _gActor311900SwatMember2BodySkeleton[20] = {
#include "assets/swat_member_2_body_skeleton.inc"
};

static u32 _gActor311900SwatMember2BodyPartVerts[20] = {
#include "assets/swat_member_2_body_partVerts.inc"
};

static SVECTOR _gActor311900SwatMember2BodyVerts[360] = {
#include "assets/swat_member_2_body_verts.inc"
};

static SVECTOR _gActor311900SwatMember2BodyNormals[358] = {
#include "assets/swat_member_2_body_normals.inc"
};

static u32 _gActor311900SwatMember2BodyStream[3973] = {
#include "assets/swat_member_2_body_stream.inc"
};

static TmdSource _gActor311900SwatMember2Body = {
    0,
    21176,
    6792,
    20,
    _gActor311900SwatMember2BodyPartVerts,
    _gActor311900SwatMember2BodyVerts,
    _gActor311900SwatMember2BodyNormals,
    _gActor311900SwatMember2BodySkeleton,
    _gActor311900SwatMember2BodyStream,
};

static AnimationPackedPose _gActor311900Animation0C9A8Bank1[11] = {
#include "assets/actor_311900_animation_0C9A8_bank1.inc"
};

static AnimationPackedRotation _gActor311900Animation0C9A8Bank4[201] = {
#include "assets/actor_311900_animation_0C9A8_bank4.inc"
};

static AnimationRecord _gActor311900Animation0C9A8Records[290] = {
#include "assets/actor_311900_animation_0C9A8_records.inc"
};

static u16 _gActor311900Animation0C9A8Indices[20] = {
#include "assets/actor_311900_animation_0C9A8_indices.inc"
};

static AnimationSet _gActor311900Animation0C9A8 = {
    _gActor311900Animation0C9A8Records,
    _gActor311900Animation0C9A8Indices,
    { NULL, _gActor311900Animation0C9A8Bank1, NULL, NULL, _gActor311900Animation0C9A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor311900Animation0CDA0Bank1[6] = {
#include "assets/actor_311900_animation_0CDA0_bank1.inc"
};

static AnimationPackedRotation _gActor311900Animation0CDA0Bank4[75] = {
#include "assets/actor_311900_animation_0CDA0_bank4.inc"
};

static AnimationRecord _gActor311900Animation0CDA0Records[141] = {
#include "assets/actor_311900_animation_0CDA0_records.inc"
};

static u16 _gActor311900Animation0CDA0Indices[20] = {
#include "assets/actor_311900_animation_0CDA0_indices.inc"
};

static AnimationSet _gActor311900Animation0CDA0 = {
    _gActor311900Animation0CDA0Records,
    _gActor311900Animation0CDA0Indices,
    { NULL, _gActor311900Animation0CDA0Bank1, NULL, NULL, _gActor311900Animation0CDA0Bank4, NULL, NULL, NULL },
};

u8 D_actor_311900_8016EBE8[12] = {
    0,
    0,
    0,
    0,
    200,
    231,
    22,
    128,
    0,
    0,
    0,
    0,
};

u8 D_actor_311900_8016EBF4[12] = {
    0,
    0,
    0,
    0,
    192,
    235,
    22,
    128,
    0,
    0,
    0,
    0,
};

TaskDesc D_actor_311900_8016EC00 = { { { TASK_BODY_TMD, 96 } }, func_actor_311900_8016249C, { .model = &_gActor311900SwatMember2Body } };

TaskDesc D_actor_311900_8016EC0C = { { { TASK_BODY_TMD, 96 } }, func_actor_311900_8016222C, { .model = &_gActor311900RupertBroderickBody1 } };

u16 D_actor_311900_8016EC18[4][256];

static void func_actor_311900_80161E3C(Task* task, s32 arg1, s16 arg2);
static void func_actor_311900_80162100(Task* task);

/// Fades the two 256-entry CLUT rows `arg2` / `arg2 + 1` of the palette table
/// to grey, one step of `_Actor311900Work::clutGreyStep` per call:
/// `ACTOR_311900_CLUT_GREY_READ` reads the VRAM rows `arg1 + 0xF5` /
/// `arg1 + 0xF6` back into the table, `ACTOR_311900_CLUT_GREY_CONVERT` sets each
/// entry's three 5-bit channels to their maximum (keeping the STP bit set), and
/// `ACTOR_311900_CLUT_GREY_UPLOAD` uploads the rows again, leaving
/// `ACTOR_311900_CLUT_GREY_DONE`.
static void func_actor_311900_80161E3C(Task* task, s32 arg1, s16 arg2)
{
    RECT              rect;
    _Actor311900Work* work;
    s32               i;
    u16               r;
    u16               g;
    u16               b;

    work = task->work;
    if (work->clutGreyStep == ACTOR_311900_CLUT_GREY_READ) {
        rect.x = 0;
        rect.y = arg1 + 0xF5;
        rect.w = 0x100;
        rect.h = 1;
        StoreImage2(&rect, (u_long*)D_actor_311900_8016EC18[arg2]);
        rect.x = 0;
        rect.y = arg1 + 0xF6;
        rect.w = 0x100;
        rect.h = 1;
        StoreImage2(&rect, (u_long*)D_actor_311900_8016EC18[arg2 + 1]);
        work->clutGreyStep = ACTOR_311900_CLUT_GREY_CONVERT;
    } else if (work->clutGreyStep == ACTOR_311900_CLUT_GREY_CONVERT) {
        for (i = 0; i < 0x100; i++) {
            r = D_actor_311900_8016EC18[arg2][i] & 0x1F;
            g = (D_actor_311900_8016EC18[arg2][i] >> 5) & 0x1F;
            b = (D_actor_311900_8016EC18[arg2][i] >> 10) & 0x1F;
            if (r < g) {
                r = g;
            } else {
                g = r;
            }
            if (g < b) {
                g = b;
            } else {
                b = g;
            }
            if (b < r) {
                b = r;
            } else {
                r = b;
            }
            D_actor_311900_8016EC18[arg2][i] = r | (g << 5) | (b << 10) | 0x8000;
        }
        for (i = 0; i < 0x100; i++) {
            r = D_actor_311900_8016EC18[arg2 + 1][i] & 0x1F;
            g = (D_actor_311900_8016EC18[arg2 + 1][i] >> 5) & 0x1F;
            b = (D_actor_311900_8016EC18[arg2 + 1][i] >> 10) & 0x1F;
            if (r < g) {
                r = g;
            } else {
                g = r;
            }
            if (g < b) {
                g = b;
            } else {
                b = g;
            }
            if (b < r) {
                b = r;
            } else {
                r = b;
            }
            D_actor_311900_8016EC18[arg2 + 1][i] = r | (g << 5) | (b << 10) | 0x8000;
        }
        work->clutGreyStep = ACTOR_311900_CLUT_GREY_UPLOAD;
    } else if (work->clutGreyStep == ACTOR_311900_CLUT_GREY_UPLOAD) {
        rect.x = 0;
        rect.y = arg1 + 0xF5;
        rect.w = 0x100;
        rect.h = 1;
        LoadImage2(&rect, (u_long*)D_actor_311900_8016EC18[arg2]);
        rect.x = 0;
        rect.y = arg1 + 0xF6;
        rect.w = 0x100;
        rect.h = 1;
        LoadImage2(&rect, (u_long*)D_actor_311900_8016EC18[arg2 + 1]);
        work->clutGreyStep = ACTOR_311900_CLUT_GREY_DONE;
    }
}

/// Serves the animation request in `_Actor311900Work::animState` on slots 1..19
/// of the work block's rig. `ACTOR_ENEMY_ANIM_BLEND` seeks every slot to the
/// clip `animId` through `animationSeekSlotWithBlend`, `ACTOR_ENEMY_ANIM_RESET`
/// resets them to it; each first gives the slot the rate in `animRate`, and
/// both then record that clip in `appliedAnimId`, settle on
/// `ACTOR_ENEMY_ANIM_TICK` and clear `animFrames`. That step only ticks the slots
/// and counts frames.
///
/// The two advances are one block in the ROM: jump.c cross-jumps them because
/// both branches name the same local. The tick step reads the block again into an alias
/// of its own -- keeping `start` dead before `work` there is what leaves the
/// slot walk on the `work` register cse2 picks for it.
static void func_actor_311900_80162100(Task* task)
{
    _Actor311900Work* work;
    _Actor311900Work* start;
    _Actor311900Work* tick;
    s32               i;
    s32               j;
    s32               k;

    work = task->work;
    if (work->animState == ACTOR_ENEMY_ANIM_BLEND) {
        start = task->work;
        for (i = 1; i < ARRAY_SIZE(start->rig.slots); i++) {
            start->rig.slots[i].rate = start->animRate;
            animationSeekSlotWithBlend(&start->rig.anim, i, start->animId, 0, 0);
        }
        start->appliedAnimId = start->animId;
        work->animState      = ACTOR_ENEMY_ANIM_TICK;
        work->animFrames     = 0;
        return;
    }
    if (work->animState == ACTOR_ENEMY_ANIM_RESET) {
        start = task->work;
        for (j = 1; j < ARRAY_SIZE(start->rig.slots); j++) {
            start->rig.slots[j].rate = start->animRate;
            animationResetSlot(&start->rig.anim, j, start->animId);
        }
        start->appliedAnimId = start->animId;
        work->animState      = ACTOR_ENEMY_ANIM_TICK;
        work->animFrames     = 0;
        return;
    }
    if (work->animState == ACTOR_ENEMY_ANIM_TICK) {
        work->animFrames++;
        tick = task->work;
        for (k = 1; k < ARRAY_SIZE(tick->rig.slots); k++) {
            animationTickSlot(&tick->rig.anim, k);
        }
    }
}

/// The actor's first state table - `func_actor_311900_8016228C`'s setup,
/// `func_actor_311900_801623B0`'s tick and teardown - dispatched through by
/// `func_actor_311900_8016222C`.
static const EnemyTaskFuncTable3 D_actor_311900_80161E24 = {
    func_actor_311900_8016228C,
    func_actor_311900_801623B0,
    enemyDestroy,
};

/// The actor's second state table - `func_actor_311900_801624F8`'s setup,
/// `func_actor_311900_801625F0`'s tick and teardown - dispatched through by
/// `func_actor_311900_8016249C`.
static const EnemyTaskFuncTable3 D_actor_311900_80161E30 = {
    func_actor_311900_801624F8,
    func_actor_311900_801625F0,
    enemyDestroy,
};

/// Runs the actor's state handler that `Task::state` selects. Copies the
/// table onto the stack first, the same local jump table `Gp_EnemyDispatch`
/// builds for the shared `Gp_EnemyWaitFuncs`, so the call goes through the
/// stack copy rather than the overlay's own `.rodata`.
void func_actor_311900_8016222C(Task* task)
{
    Enemy*              enemy;
    EnemyTaskFuncTable3 sp;

    enemy = task->spawnArg2.pointer;
    sp    = D_actor_311900_80161E24;
    sp.funcs[task->state](enemy, task);
}

/// The actor's first setup path, reached through `D_actor_311900_80161E24`. It
/// tears the enemy down instead while game flag 0xA's nibble 2 -- the bit
/// `func_actor_311900_801623B0` raises once the view reaches 0xA -- is already
/// up, or when the `_Actor311900Work` block cannot be allocated into
/// `Task::work`.
///
/// Otherwise it splats the light / colour pair `func_actor_311900_8016278C`
/// writes onto the model root's `field_1C` / `field_20` slots, points
/// `Enemy::field_4` at the root coordinate's matrix, re-parents that root to
/// `gGfxViewCoord`, builds the animation context `animationInitContext` over the
/// block's slot array and packed-pose run, requests clip 1 from its start
/// (`animState` / `animId`), zeroes `advanceFrames` and `advancing`, and publishes
/// the view-dependent light level exactly as the tick does.
static void func_actor_311900_8016228C(Enemy* enemy, Task* task)
{
    _Actor311900Work* work;
    GfxCoord*         coord;
    TmdObject*        obj;

    obj   = task->extra.tmd;
    coord = obj->coords;
    if ((gameFlagGetNibble(GAME_FLAG_SECURITY_MONITOR_SCENES_SEEN) & 2) ||
        (work = memCalloc(sizeof(_Actor311900Work), 0), task->work = work, work == NULL)) {
        enemyDestroy(enemy, task);
        return;
    }
    func_actor_311900_8016278C(task);
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    obj->flags      = 0;
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_311900_8016EBE8, obj, work->rig.poses,
                         work->rig.slots);
    coord->parent       = &gGfxViewCoord;
    work->animState     = ACTOR_ENEMY_ANIM_RESET;
    work->animId        = 1;
    work->advanceFrames = 0;
    work->advancing     = 0;
    if ((Gp_GetViewIndex() & 0xFF) == 0xA) {
        obj->flags = 0;
    } else {
        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    func_actor_311900_80162100(task);
    task->state += 1;
}

/// The actor's per-frame tick. Publishes the view-dependent light level into
/// `TmdObject::flags` (0 at view 0xA, 0x80 otherwise), and while the work
/// block's `advancing` latch is up, counts frames in `advanceFrames` and nudges the
/// model along the coordinate part `func_actor_311900_80162658` walks. The
/// counter reaching `ACTOR_311900_ADVANCE_FRAMES` raises game flag 0x102 and
/// advances the state.
static void func_actor_311900_801623B0(Enemy* enemy, Task* task)
{
    _Actor311900Work* work;
    GfxCoord*         coord;
    TmdObject*        obj;

    obj   = task->extra.tmd;
    work  = task->work;
    coord = obj->coords;
    func_actor_311900_80161E3C(task, 2, 0);
    if ((Gp_GetViewIndex() & 0xFF) == 0xA) {
        gameFlagSetNibble(GAME_FLAG_SECURITY_MONITOR_SCENES_SEEN, gameFlagGetNibble(GAME_FLAG_SECURITY_MONITOR_SCENES_SEEN) | 2);
        obj->flags      = 0;
        work->advancing = 1;
    } else {
        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (work->advancing == 1) {
        work->advanceFrames++;
        func_actor_311900_80162658(coord, 0x24);
    }
    func_actor_311900_80162100(task);
    if (work->advanceFrames >= ACTOR_311900_ADVANCE_FRAMES) {
        gameFlagSetNibble(GAME_FLAG_SECURITY_MONITOR_CAM_A_SCENE_DONE, 1);
        task->state++;
    }
}

/// Runs the actor's second state table - `func_actor_311900_801624F8`'s setup,
/// `func_actor_311900_801625F0`'s tick and `enemyDestroy` - at the handler
/// `Task::state` selects. The table is copied onto the stack before the call,
/// the same shape as `func_actor_311900_8016222C` for the first table.
void func_actor_311900_8016249C(Task* task)
{
    EnemyTaskFuncTable3 sp;

    sp = D_actor_311900_80161E30;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}

/// The `D_actor_311900_80161E30` spawn handler -- the actor's second setup
/// path, reached through the three-entry table whose tick is
/// `func_actor_311900_801625F0`. It is the same setup `func_actor_311900_8016228C`
/// performs for the first table, under different conditions: the enemy is torn
/// down instead while game flag 1 has already reached nibble 3, and the work
/// block gets the light / colour pair `func_actor_311900_8016281C` splats
/// (rather than `func_actor_311900_8016278C`'s) from a different animation run
/// (`D_actor_311900_8016EBF4`, not `D_actor_311900_8016EBE8`).
///
/// The `_Actor311900Work` block goes into `Task::work`. `Enemy::field_4` takes the
/// model's root coordinate's
/// matrix, the root's `parent` is re-parented to `gGfxViewCoord`, the animation
/// context is built over the block's slot array and packed-pose run, and the
/// `animState` / `animId` request clip 1 from its start. Note this handler,
/// unlike `func_actor_311900_8016228C`, does not touch `advanceFrames` / `advancing`
/// or the model's `field_C`.
static void func_actor_311900_801624F8(Enemy* enemy, Task* task)
{
    _Actor311900Work* work;
    GfxCoord*         coord;
    TmdObject*        obj;

    obj   = task->extra.tmd;
    coord = obj->coords;
    if (gameFlagGetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS) >= 3 ||
        (work = memCalloc(sizeof(_Actor311900Work), 0), task->work = work, work == NULL)) {
        enemyDestroy(enemy, task);
        return;
    }
    func_actor_311900_8016281C(task);
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    obj->flags      = 0;
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_311900_8016EBF4, obj, work->rig.poses,
                         work->rig.slots);
    coord->parent   = &gGfxViewCoord;
    work->animState = ACTOR_ENEMY_ANIM_RESET;
    work->animId    = 1;
    func_actor_311900_80162100(task);
    task->state += 1;
}

static void func_actor_311900_801625F0(Enemy* enemy, Task* task)
{
    TmdObject* obj;

    obj = task->extra.tmd;
    func_actor_311900_80161E3C(task, 4, 2);
    if ((Gp_GetViewIndex() & 0xFF) == 0xB) {
        obj->flags = 0;
    } else {
        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    func_actor_311900_80162100(task);
}

/// Takes `arg1` as a signed 16-bit step, builds a direction vector from
/// `arg0->coord`'s rotation with `gfxReadMatrixZAxis`, normalizes it with
/// `VectorNormalSS`, scales it by the step on the GTE, adds it to
/// `arg0->coord.t` and clears `arg0->composeStamp`. Returns the step, or 0 having
/// touched nothing while the game is paused (`gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1`) or when the
/// step is zero. `arg0` is the per-part `GfxCoord` the caller takes from
/// `TmdObject::coords`.
///
/// The scratch-pad vector is carved out under two names: `vec`, which the
/// frame update stores and the calls normalize, and `gte`, which the GTE round
/// trip reads and writes back. The object keeps them apart, and that is what
/// the copy ahead of the `if` is.
static s32 func_actor_311900_80162658(GfxCoord* arg0, s16 arg1)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* gte;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1) {
        return 0;
    }
    head                          = SCRATCH_STACK_CURSOR(SVECTOR);
    vec                           = head - 1;
    gte                           = head - 1;
    SCRATCH_STACK_CURSOR(SVECTOR) = vec;
    if (arg1 != 0) {
        gfxReadMatrixZAxis(&arg0->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(arg1);
        gte_ldsv(gte);
        gte_gpf12();
        gte_stsv(gte);
        arg0->coord.t[0]  += head[-1].vx;
        arg0->coord.t[1]  += vec->vy;
        arg0->coord.t[2]  += vec->vz;
        arg0->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    return arg1;
}

/// Splats an identity light / colour matrix pair into the work block the spawn
/// state carved out of `Task::work`, republishes both onto the
/// `TmdObject::lightMtx` / `colorMtx` slots that the renderer otherwise reads
/// from `Gp_DefaultMtx` / `Gp_DefaultMtx2`, and then overwrites each 3x3 with
/// the values the actor lights its model with -- the light matrix flat except
/// for `m[1][0]` and `m[2][2]`, the colour matrix fully pass-through.
static void func_actor_311900_8016278C(Task* task)
{
    GfxMatrix*        color;
    GfxMatrix*        light;
    TmdObject*        ext;
    _Actor311900Work* work;

    work  = task->work;
    ext   = task->extra.tmd;
    light = (GfxMatrix*)&work->light;
    color = (GfxMatrix*)&work->color;

    light->rotationWords.m00M01 = ONE;
    light->rotationWords.m02M10 = 0;
    light->rotationWords.m11M12 = ONE;
    light->rotationWords.m20M21 = 0;
    light->rotationWords.m22    = ONE;

    color->rotationWords.m00M01 = ONE;
    color->rotationWords.m02M10 = 0;
    color->rotationWords.m11M12 = ONE;
    color->rotationWords.m20M21 = 0;
    color->rotationWords.m22    = ONE;

    ext->lightMtx = &work->light;

    work->color.m[0][0] = 0x1000;
    work->color.m[0][1] = 0x1000;
    work->color.m[0][2] = 0x1000;
    work->color.m[1][0] = 0x1000;
    work->color.m[1][1] = 0x1000;
    work->color.m[1][2] = 0x1000;
    work->color.m[2][0] = 0x1000;
    work->color.m[2][1] = 0x1000;
    work->color.m[2][2] = 0x1000;

    work->light.m[0][0] = 0x1000;
    work->light.m[0][1] = 0x1000;
    work->light.m[0][2] = 0x1000;
    work->light.m[1][0] = 0;
    work->light.m[1][1] = 0x1000;
    work->light.m[1][2] = 0x1000;
    work->light.m[2][0] = 0x1000;
    work->light.m[2][1] = 0x1000;
    work->light.m[2][2] = 0;

    ext->colorMtx = &work->color;
}

/// Same splat as `func_actor_311900_8016278C`, republishing the light / colour
/// pair onto `TmdObject::lightMtx` / `colorMtx` between the identity seed and
/// the per-actor values: the colour matrix goes fully pass-through, the light
/// matrix flat except for a negated `m[0][0]`.
static void func_actor_311900_8016281C(Task* task)
{
    GfxMatrix*        color;
    GfxMatrix*        light;
    TmdObject*        ext;
    _Actor311900Work* work;

    work  = task->work;
    ext   = task->extra.tmd;
    light = (GfxMatrix*)&work->light;
    color = (GfxMatrix*)&work->color;

    light->rotationWords.m00M01 = ONE;
    light->rotationWords.m02M10 = 0;
    light->rotationWords.m11M12 = ONE;
    light->rotationWords.m20M21 = 0;
    light->rotationWords.m22    = ONE;

    color->rotationWords.m00M01 = ONE;
    color->rotationWords.m02M10 = 0;
    color->rotationWords.m11M12 = ONE;
    color->rotationWords.m20M21 = 0;
    color->rotationWords.m22    = ONE;

    ext->lightMtx = &work->light;

    work->color.m[0][0] = 0x1000;
    work->color.m[0][1] = 0x1000;
    work->color.m[0][2] = 0x1000;
    work->color.m[1][0] = 0x1000;
    work->color.m[1][1] = 0x1000;
    work->color.m[1][2] = 0x1000;
    work->color.m[2][0] = 0x1000;
    work->color.m[2][1] = 0x1000;
    work->color.m[2][2] = 0x1000;

    work->light.m[0][0] = -0x1000;
    work->light.m[0][1] = 0x1000;
    work->light.m[0][2] = 0x1000;
    work->light.m[1][0] = 0x1000;
    work->light.m[1][1] = 0x1000;
    work->light.m[1][2] = 0x1000;
    work->light.m[2][0] = 0x1000;
    work->light.m[2][1] = 0x1000;
    work->light.m[2][2] = 0x1000;

    ext->colorMtx = &work->color;
}
