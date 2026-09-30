#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/rand.h>

#include "common.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/direction_input.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gamemain.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/shelter_b3_dumping_hole.h"
#include "../../shared/screen_wave.h"

/// Work block of the overlay's event/controller task -- the one
/// `D_actor_342100_80164BB8` points at.
///
/// `func_actor_342100_801630A4` allocates it with `Mem_Malloc(0x44, 0)`,
/// `Mem_Set`s the same 0x44 bytes over it and stores it in that task's
/// `Task::work` slot (0x1C), which is not a `TaskIdMap` here, then publishes
/// the task in `D_actor_342100_80164BB8`. Every leaf helper reaches the block
/// that way, `(Actor342100Work*)D_actor_342100_80164BB8->work`.
///
/// `field_2C` is the `gameGetPtrSlot(3)` task the overlay aims its messages
/// at. `field_30` and `field_34` are further message targets, both sent
/// 0x7DB, and `field_38` is a task the overlay spawns itself: with a non-zero
/// argument `func_actor_342100_80163454` writes 1 into its
/// `Task::spawnArg1`. `field_3C` takes `arg0 + 0x2F` from
/// `func_actor_342100_8016334C`'s integer argument, the same value that
/// function forwards as the animation message's second word.
///
/// `wave` is the ramp of the screen-wave task `screenWaveGridTask`:
/// `func_actor_342100_80163408` seeds its span and scale and spawns the task
/// on it, and the fade task `func_actor_342100_80162748`, which reaches this
/// block through `Task::spawnArg2`, ends the wave by setting its ramp state to
/// 2 once the screen has been blanked white.
///
/// shelter_b3_garbage_incinerator carries the same encounter with a smaller
/// block that shares the leading bytes and `wave` but keeps one task pointer
/// fewer, with the child task and the animation fields in other places, so
/// the two are different types.
typedef struct Actor342100Work {
    /* 0x00 */ byte           pad_0[0x20];
    /* 0x20 */ OverlayWaveCtx wave;
    /* 0x2C */ Task*          field_2C; // gameGetPtrSlot(3)
    /* 0x30 */ Task*          field_30;
    /* 0x34 */ Task*          field_34;
    /* 0x38 */ Task*          field_38;
    /* 0x3C */ s16            field_3C;
    /* 0x3E */ s16            field_3E;
    /* 0x40 */ byte           pad_40[0x4];
} Actor342100Work;
STATIC_ASSERT_SIZEOF(Actor342100Work, 0x44);

/// The overlay's event/controller task, published by
/// `func_actor_342100_801630A4`.
extern Task* D_actor_342100_80164BB8;

/// Single-entry spawn table `func_actor_342100_80163454` starts as entry 3.
extern TaskDesc D_actor_342100_80164B78[];

void func_actor_342100_80163344(Task* arg0, s32 arg1, s32 arg2);

void func_actor_342100_8016334C(s32 arg0);

void func_actor_342100_801633D0(s32 arg0);

void func_actor_342100_80163408(void);

void func_actor_342100_80163454(s32 arg0);

void func_actor_342100_80163518(void);

// Message-table callbacks use the argument views required by this TU.
typedef struct {
    s32 id;
    union {
        void (*call0)(Task*, s32, s32);
    } handler;
} Actor342100MessageEntry;
STATIC_ASSERT_SIZEOF(Actor342100MessageEntry, 8);

extern Actor342100MessageEntry D_actor_342100_801648F8[1];

/// Main-executable global with no module header yet: the remaining-enemy count.

/// Main-executable globals with no module header yet: `Player_Status.weapon` is the base
/// weapon id records are numbered from, and `Mc_SaveData[0].state.characterId` selects the alternate
/// set -- 1 means the second block, anything else the `+0x22` one.

/// Single-entry spawn table of the screen-wave task
/// `screenWaveGridTask`: `func_actor_342100_80163408` starts entry 0
/// and hands it the address of `Actor342100Work::wave` as its ramp.
extern TaskDesc D_actor_342100_801648DC[];

/// Null-terminated table of the overlay's per-state message tables, counted
/// and reported by `func_actor_342100_80162F54` when it arms the encounter:
/// three live entries and the null word that ends them.
extern AnimationSet* D_actor_342100_80164900[4];

/// Animation step table `func_actor_342100_801629B8` walks: `s16` entries
/// holding the anim id one step on from `field_3C`, sent as the message's
/// second word with `0x2F` added; the first three entries are `-1`, which ends
/// the chain, and only the fourth is live. Sits directly after
/// `D_actor_342100_80164900`'s null word, and its first element is the address
/// `func_actor_342100_80162F54`'s encounter table of a different size would
/// have started at, so splat cut it out as a symbol of its own.
extern s16 D_actor_342100_80164910[];

/// Placement tables the overlay's spawn task picks between by
/// `gGameSession->location.loc.view`: 0x1D, 0x1E, 0x1F, 0x23 and 0x24 select the 0x80164930
/// / 0x80164918 / 0x80164948 / 0x80164960 / 0x80164980 table respectively, and
/// the values in between select none. Each is a zero-`vx`-terminated `SVECTOR`
/// list of two to three placements -- the terminator is an all-zero entry -- and
/// `func_actor_342100_80162C88` drops one effect task on every live entry.
extern SVECTOR D_actor_342100_80164918[];
extern SVECTOR D_actor_342100_80164930[];
extern SVECTOR D_actor_342100_80164948[];
extern SVECTOR D_actor_342100_80164960[];
extern SVECTOR D_actor_342100_80164980[];

/// Model/animation set `func_actor_342100_80162F54` installs with
/// `func_800E8614` on the same arm; a byte address is all the installer sees.
extern GpEvsCmd D_actor_342100_801649C8[];

/// Effect record `func_actor_342100_80162DDC` hands `func_800FDB18` together
/// with one part of the player's model: `field_0` is that part's coordinate
/// and `field_4` the scale that goes with it (0x100 for the wide pick, 0x10
/// for the narrow one). Ships as `{ NULL, 0, 1 }` in the data blob, directly
/// before the part table below.
extern GpEffArg D_actor_342100_801649A0;

/// The player-model parts the effect record above is aimed at, as indices into
/// the player's coordinate array (`TmdObject::coords`): sixteen `u16`s
/// running 1..0x12, of which `func_actor_342100_80162DDC` takes the first four
/// (2, 4, 6, 0xA) when it masks the LCG draw with 3 and all sixteen when it
/// masks with 0xF.
extern u16 D_actor_342100_801649A8[];

/// Frame counter the narrow arm of `func_actor_342100_80162DDC`'s state 1 is
/// gated on: it aims the effect only on the frames where the low nibble (or,
/// for the other arm, the low three bits) of this global is clear.

/// Distortion amplitude of the screen wave, `frame * scale / span` of the
/// running ramp, recomputed every frame.
extern s32 gScreenWaveRamp;

/// The ramp the running wave task was spawned with, parked at spawn so the
/// tick reads it back every frame.
extern OverlayWaveCtx* gScreenWaveCtx;

/// Per-column and per-row phase records: each is seeded with a random offset
/// and speed at spawn and advanced by its speed on every frame
/// `Gp_StateF0.field_4` is clear.
extern OverlayWaveRec gScreenWaveColumns[10];
extern OverlayWaveRec gScreenWaveRows[30];

/// The two frame buffers' 8 by 30 meshes of textured quads, one grid per
/// buffer, indexed by the current buffer.
// The task starts at row 1 and draws rows -1 through 28.
extern POLY_FT4 gScreenWaveGrid[2][30][8];

void func_actor_342100_80162748(Task*);
void func_actor_342100_80162AB0(Task*);
void func_actor_342100_80162C88(void);
void func_actor_342100_80162DDC(Task*);
void func_actor_342100_801630A4(Task*);

AnimationPackedPose D_actor_342100_80163534[6] = {
#include "assets/actor_342100_animation_019F0_bank1.inc"
};

AnimationPackedRotation D_actor_342100_8016357C[46] = {
#include "assets/actor_342100_animation_019F0_bank4.inc"
};

AnimationRecord D_actor_342100_80163634[109] = {
#include "assets/actor_342100_animation_019F0_records.inc"
};

u16 D_actor_342100_801637E8[20] = {
#include "assets/actor_342100_animation_019F0_indices.inc"
};

AnimationSet D_actor_342100_80163810 = {
    D_actor_342100_80163634,
    D_actor_342100_801637E8,
    { NULL, D_actor_342100_80163534, NULL, NULL, D_actor_342100_8016357C, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342100_80163838[16] = {
#include "assets/actor_342100_animation_0242C_bank1.inc"
};

AnimationPackedRotation D_actor_342100_801638F8[246] = {
#include "assets/actor_342100_animation_0242C_bank4.inc"
};

AnimationRecord D_actor_342100_80163CD0[341] = {
#include "assets/actor_342100_animation_0242C_records.inc"
};

u16 D_actor_342100_80164224[20] = {
#include "assets/actor_342100_animation_0242C_indices.inc"
};

AnimationSet D_actor_342100_8016424C = {
    D_actor_342100_80163CD0,
    D_actor_342100_80164224,
    { NULL, D_actor_342100_80163838, NULL, NULL, D_actor_342100_801638F8, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_342100_80164274[14] = {
#include "assets/actor_342100_animation_02A94_bank1.inc"
};

AnimationPackedRotation D_actor_342100_8016431C[148] = {
#include "assets/actor_342100_animation_02A94_bank4.inc"
};

AnimationRecord D_actor_342100_8016456C[200] = {
#include "assets/actor_342100_animation_02A94_records.inc"
};

u16 D_actor_342100_8016488C[20] = {
#include "assets/actor_342100_animation_02A94_indices.inc"
};

AnimationSet D_actor_342100_801648B4 = {
    D_actor_342100_8016456C,
    D_actor_342100_8016488C,
    { NULL, D_actor_342100_80164274, NULL, NULL, D_actor_342100_8016431C, NULL, NULL, NULL },
};

TaskDesc D_actor_342100_801648DC[2] = {
    { 0, 192, screenWaveGridTask, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

s32 gScreenWaveRamp = 256;

Actor342100MessageEntry D_actor_342100_801648F8[1] = {
    { 2011, { .call0 = func_actor_342100_80163344 } },
};

AnimationSet* D_actor_342100_80164900[4] = {
    &D_actor_342100_80163810,
    &D_actor_342100_801648B4,
    &D_actor_342100_8016424C,
    NULL,
};

s16 D_actor_342100_80164910[4] = {
    -1,
    -1,
    -1,
    0,
};

SVECTOR D_actor_342100_80164918[3] = {
    { 0x4650, -100, -2500, 0 },
    { 0x4650, -700, -8700, 0 },
    { 0, 0, 0, 0 },
};

SVECTOR D_actor_342100_80164930[3] = {
    { 0x4268, -600, -1500, 0 },
    { 0x4268, -100, -0x2710, 0 },
    { 0, 0, 0, 0 },
};

SVECTOR D_actor_342100_80164948[3] = {
    { 9000, -100, -2500, 0 },
    { 5000, -100, -2500, 0 },
    { 0x2710, 0, -0x2710, 0 },
};

SVECTOR D_actor_342100_80164960[4] = {
    { 0x2710, -200, -9000, 0 },
    { 0x2710, -200, -2000, 0 },
    { 6000, -200, -3000, 0 },
    { 0, 0, 0, 0 },
};

SVECTOR D_actor_342100_80164980[4] = {
    { 3000, -200, -5000, 0 },
    { 5000, -200, -7500, 0 },
    { 3000, -200, -8000, 0 },
    { 0, 0, 0, 0 },
};

GpEffArg D_actor_342100_801649A0 = { NULL, 0, 1 };

u16 D_actor_342100_801649A8[16] = {
    2,
    4,
    6,
    10,
    1,
    3,
    5,
    7,
    8,
    9,
    11,
    12,
    13,
    15,
    16,
    18,
};

GpEvsCmd D_actor_342100_801649C8[18] = {
    { 13, { .callback = func_actor_342100_8016334C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_342100_80163454 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_342100_801633D0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_342100_8016334C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_342100_80162C88 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_342100_80163408 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_342100_8016334C }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_342100_80163454 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 75 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_342100_801633D0 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callback = func_actor_342100_801633D0 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_actor_342100_80163518 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TaskDesc D_actor_342100_80164B78[5] = {
    { 0, 192, func_actor_342100_801630A4, { .model = NULL } },
    { 0, 192, taskKill, { .model = NULL } },
    { 0, 192, func_actor_342100_80162748, { .model = NULL } },
    { 0, 192, func_actor_342100_80162DDC, { .model = NULL } },
    { TASK_BODY_COORD, 192, func_actor_342100_80162AB0, { .model = NULL } },
};

OverlayWaveCtx* gScreenWaveCtx = NULL;

Task* D_actor_342100_80164BB8 = NULL;

// Nine active columns and one retained zero entry.
OverlayWaveRec gScreenWaveColumns[10] = { 0 };

OverlayWaveRec gScreenWaveRows[30] = { 0 };

POLY_FT4 gScreenWaveGrid[2][30][8] = { 0 };

static s32 func_actor_342100_801629B8(Task* arg0);
static s32 func_actor_342100_80162F54(Task* arg0);

#include "../../shared/screen_wave_grid.inc.c"

/// Fade-to-white driver of the encounter, six states over the eight-byte
/// channel block it allocates into its own `Task::work` and hands the parent
/// work block through `Task::spawnArg2`.
///
/// State 0 allocates the ramp, zeroes the three channels and parks the
/// message record `D_actor_342100_801648F8` in `Task::msgTable`. States 2 and
/// 3 step `r` -- the first by 0xA up to 0x50, the second by 1 up to
/// 0xFF -- and each hands the state machine back to 1 when it clamps, so the
/// two ramps run back to back. State 4 steps `g` / `b` by 8; once
/// `g` passes 0xFF the display mode is switched, `Fs_ImgBuffers` is
/// filled white, the parent work block's wave ramp is sent to state 2, and state 5
/// draws the full-screen white `TILE` + `DR_TPAGE` packed into
/// `gGpuPrimCursor` before returning without the fade call. Every other state
/// -- 1, 6 and up -- only draws the fade.
void func_actor_342100_80162748(Task* arg0)
{
    OverlayFadeWork* work;
    OverlayFadeWork* alloc;
    Actor342100Work* parent;
    TILE*            tile;
    DR_TPAGE*        dr;

    work = (OverlayFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (OverlayFadeWork*)Mem_Malloc(8, 0);
            arg0->work = alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work           = alloc;
            work->b        = 0;
            work->g        = 0;
            work->r        = 0;
            arg0->msgTable = D_actor_342100_801648F8;
            arg0->state   += 1;
            break;
        case 2:
            work->r += 0xA;
            if ((s16)work->r >= 0x51) {
                work->r     = 0x50;
                arg0->state = 1;
            }
            break;
        case 3:
            work->r += 1;
            if ((s16)work->r >= 0x100) {
                work->r     = 0xFF;
                arg0->state = 1;
            }
            break;
        case 4:
            work->g += 8;
            work->b += 8;
            if ((s16)work->g >= 0x100) {
                parent             = (Actor342100Work*)((Task*)arg0->spawnArg2.pointer)->work;
                parent->wave.state = 2;
                Display_SetMode(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
                Mem_Set(Fs_ImgBuffers, 0xFF, 0x25800);
                work->b     = 0xFF;
                work->g     = 0xFF;
                arg0->state = 5;
            }
            break;
        case 5:
            tile           = gGpuPrimCursor;
            gGpuPrimCursor = tile + 1;
            setlen(tile, 3);
            setcode(tile, 0x60);
            tile->r0 = 0xFF;
            tile->g0 = 0xFF;
            tile->b0 = 0xFF;
            tile->x0 = -0xA0;
            tile->y0 = -0x78;
            tile->w  = 0x140;
            tile->h  = 0xF0;
            addPrim(gGpuCurrentOt - 16, tile);

            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE1000200;
            addPrim(gGpuCurrentOt - 16, dr);
            return;
    }
    Fade_DrawOverlay((u8)work->r, (u8)work->g, (u8)work->b, 1);
}

/// Advance the encounter's animation one step: the work block's `field_2C` is
/// queried with 0x3ED and a non-zero answer stops the chain with 0; `field_3C`
/// is range-checked against 0x2F (the first anim id the table can name) and the
/// table's entry shifted up by 0x2F, a negative entry ending it with 1 as well.
/// The step that survives re-sends `AnimationPlayRequest {setId, anim, 1, 0xA, ANIMATION_WORLD_COLLISION_DISABLE}` as
/// message 0x3E8 -- `func_actor_342100_8016334C`'s tail with `field_C` = 0xA --
/// to the same target, and reports 1.
static s32 func_actor_342100_801629B8(Task* arg0)
{
    Actor342100Work*     work;
    Actor342100Work*     w;
    AnimationPlayRequest msg;
    s16                  anim;
    s32                  weaponId;
    s32                  setId;

    work = (Actor342100Work*)arg0->work;
    if (work->field_2C == NULL) {
    ret1:
        return 1;
    }
    if (Gp_DispatchMsg(work->field_2C, 0x3ED, 0, 0) != 0) {
        return 0;
    }
    if (work->field_3C < 0x2F) {
        goto ret1;
    }
    if (D_actor_342100_80164910[work->field_3C - 0x2F] < 0) {
        goto ret1;
    }
    anim                     = D_actor_342100_80164910[work->field_3C - 0x2F] + 0x2F;
    w                        = (Actor342100Work*)arg0->work;
    weaponId                 = Player_Status.weapon;
    setId                    = (Mc_SaveData[0].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.source.index         = setId;
    w->field_3C              = anim;
    msg.animationId          = anim;
    msg.blend                = ANIMATION_BLEND_INTERPOLATE;
    msg.blendFrames          = 0xA;
    msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    Gp_DispatchMsgPtr(w->field_2C, ANIMATION_MESSAGE_PLAY, &msg, 0);
    goto ret1;
}

/// State 0 allocates the overlay's effect record -- eight bytes, scale 0x100,
/// count 1, aimed at the model's root coordinate -- through `arg0->work`,
/// which is also where the null check reads it back: that is what leaves the
/// copy into `eff` after the branch instead of before it. State 1 waits out
/// `spawnArg1` and steps to 2. State 2 runs on every fourth frame, and builds
/// the effect's offset vector out of five LCG rolls: two per signed component
/// (the value from one roll, its sign from the next) plus a third that is
/// always negative. Only the three rolls whose value goes into `Gp_LcgState`
/// are stored, so the two temporary rolls are separate variables -- one `rng`
/// would be a single long-lived pseudo and take a register the constant needs.
///
/// Where `vec.vx = vx` sits is load-bearing. Placed with the last roll it is
/// scheduled past the argument setup, which lengthens `vx`'s live range enough
/// that global-alloc prefers the `0x71357911` constant and hands the component
/// $a2 (99.49%); between the third roll and the `vec.vy` store it stays short
/// and takes $a1, the constant falling to $a2 (100.00%).
void func_actor_342100_80162AB0(Task* arg0)
{
    GpEffArg* eff;
    GfxCoord* coord;
    SVECTOR   vec;
    s32       rng;
    s32       rng2;
    s32       vx;
    s32       vz;

    eff   = (GpEffArg*)arg0->work;
    coord = arg0->extra.coordBody->coord;
    switch (arg0->state) {
        case 0:
            arg0->work = Mem_Malloc(8, 0);
            if (arg0->work == NULL) {
                taskKill(arg0);
                return;
            }
            eff = (GpEffArg*)arg0->work;
            Mem_Set(eff, 0, 8);
            eff->spawnArgLo = 0x100;
            eff->coord      = arg0->extra.coordBody->coord;
            eff->spawnArgHi = 1;
            arg0->state++;
            return;
        case 1:
            if (arg0->spawnArg1.value <= 0) {
                arg0->state = 2;
                return;
            }
            arg0->spawnArg1.value--;
            return;
        case 2:
            if (gDisplayState.animFrame & 0xF) {
                return;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            rng         = Gp_LcgState * 5 + 0x71357911;
            vx          = ((u32)rng >> 16) & 0x3F;
            Gp_LcgState = rng * 5 + 0x71357911;
            if (((u32)Gp_LcgState >> 16) & 1) {
                vx = -vx;
            }
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            vec.vx      = vx;
            vec.vy      = -(((u32)Gp_LcgState >> 16) & 0x3F);
            rng2        = Gp_LcgState * 5 + 0x71357911;
            vz          = ((u32)rng2 >> 16) & 0x3F;
            Gp_LcgState = rng2 * 5 + 0x71357911;
            if (((u32)Gp_LcgState >> 16) & 1) {
                vz = -vz;
            }
            vec.vz = vz;
            func_800FDB18(3, coord, &vec, eff);
            return;
    }
}

/// Spawn the encounter's effect tasks: `gGameSession->location.loc.view` selects one of
/// the overlay's placement tables, and every entry in it rolls the LCG once,
/// starts spawn entry 4 (`func_actor_342100_80162AB0`) with the roll's masked
/// high half as its `spawnArg1` -- the lifetime that task's state 1 counts down
/// -- and lays the entry onto the model the new task displays: identity rotation
/// at scale 0x1000 through the `GpMtxWords` view of `coord`, the entry's `vx` /
/// `vy` / `vz` written to `coord.t[0..2]`. The walk is `while (pos->vx != 0)`,
/// so a table is as many entries as it has non-zero `vx`s and a table whose
/// first entry is zero spawns nothing.
///
/// The table pointer is deliberately uninitialised: `gGameSession->location.loc.view`
/// values 0x20..0x22 -- and anything outside the jump table -- leave it holding
/// whatever the caller left in `$s1`, which is the target's shape.
///
/// Referenced from the `0x0D` entry of the command table in
/// `D_actor_342100_801649C8` (+0x90), next to the same-shaped entries naming
/// `func_actor_342100_8016334C` / `func_actor_342100_801633D0` /
/// `func_actor_342100_80163408` / `func_actor_342100_80163454`. That entry
/// passes it no arguments, which is why the declaration is `(void)`.
void func_actor_342100_80162C88(void)
{
    GfxCoord*   coord;
    GpMtxWords* rot;
    SVECTOR*    pos;
    Task*       task;
    u32         rng;

    switch (gGameSession->location.loc.view) {
        case 29:
            pos = D_actor_342100_80164930;
            break;
        case 30:
            pos = D_actor_342100_80164918;
            break;
        case 31:
            pos = D_actor_342100_80164948;
            break;
        case 35:
            pos = D_actor_342100_80164960;
            break;
        case 36:
            pos = D_actor_342100_80164980;
            break;
    }
    while (pos->vx != 0) {
        rng               = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState       = rng;
        task              = Task_SpawnFromTable(D_actor_342100_80164B78, 4, (rng >> 16) & 0x1F, 0);
        coord             = task->extra.tmd->coords;
        rot               = (GpMtxWords*)&coord->coord;
        rot->m00_m01      = 0x1000;
        rot->m02_m10      = 0;
        rot->m11_m12      = 0x1000;
        rot->m20_m21      = 0;
        rot->m22          = 0x1000;
        coord->coord.t[0] = pos->vx;
        coord->coord.t[1] = pos->vy;
        coord->coord.t[2] = pos->vz;
        pos++;
    }
}

/// Spawn task of the overlay's spawn table (`func_actor_342100_80162748`'s
/// neighbour entry, started with the encounter): each tick rolls the LCG and
/// aims the overlay's effect record at one part of the player's model, taken
/// from the coordinate array `gameGetPtrSlot(3)`'s display object owns.
///
/// State 0 fires unconditionally -- the wide pick, scale 0x100 -- and steps to
/// state 1. State 1 fires only on a frame the `gDisplayState.animFrame` gate lets through,
/// and which pick that is depends on the task's `spawnArg1`: the zero arm
/// takes the same four parts as state 0 at scale 0x10, the non-zero arm the
/// whole table at scale 0x100.
///
/// The three arms each spell the aim-and-fire sequence out. That is what the
/// target's shape is: the two state-1 arms are byte-for-byte equal from the
/// table-base `lui` on, so `jump.c`'s cross-jumping (the `jump_optimize` that
/// runs after reload) merges that suffix into one block and leaves each arm
/// its own copy of the address and scale in front of the jump -- the address
/// and scale cannot merge because the scale differs. Folding the arms into one
/// `goto`-shared block instead compiles them into a single copy with a live
/// scale value, which is a different object (95.02%).
void func_actor_342100_80162DDC(Task* arg0)
{
    Task* slot;
    s32   idx;

    slot        = gameGetPtrSlot(3);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    idx         = Gp_LcgState >> 16;

    switch (arg0->state) {
        case 0:
            idx                               &= 3;
            D_actor_342100_801649A0.spawnArgLo = 0x100;
            D_actor_342100_801649A0.coord      = &slot->extra.tmd->coords[D_actor_342100_801649A8[idx]];
            func_800FDB18(3, slot->extra.tmd->coords, NULL, &D_actor_342100_801649A0);
            arg0->state++;
            return;
        case 1:
            if (arg0->spawnArg1.value == 0) {
                if (gDisplayState.animFrame & 0xF) {
                    return;
                }
                idx                               &= 3;
                D_actor_342100_801649A0.spawnArgLo = 0x10;
                D_actor_342100_801649A0.coord      = &slot->extra.tmd->coords[D_actor_342100_801649A8[idx]];
                func_800FDB18(3, slot->extra.tmd->coords, NULL, &D_actor_342100_801649A0);
                return;
            }
            if (gDisplayState.animFrame & 7) {
                return;
            }
            idx                               &= 0xF;
            D_actor_342100_801649A0.spawnArgLo = 0x100;
            D_actor_342100_801649A0.coord      = &slot->extra.tmd->coords[D_actor_342100_801649A8[idx]];
            func_800FDB18(3, slot->extra.tmd->coords, NULL, &D_actor_342100_801649A0);
            return;
    }
}

/// First tick of the overlay's event/controller task, the one that arms the
/// encounter as state 0 and then waits for the player's arrival as state 1.
///
/// State 0 counts the live entries of the overlay's message-table list and
/// hands slot 3 that list with message 0x3F7, lets the player's weapon into
/// the message stream (`Gp_MsgPlayerWeapon`), raises the `Gp_StateC08` flag
/// `func_800A7DB8` gates on, installs the model set and hands slot 6 the
/// 0xFA4 that starts the encounter, then starts spawn entry 2 with the task
/// itself and steps to state 1. State 1 ticks the child and reports 1 to keep
/// the task alive until `gGameSession->eventState` is set.
static s32 func_actor_342100_80162F54(Task* arg0)
{
    Actor342100Work* work = (Actor342100Work*)arg0->work;
    Actor342100Work* msgWork;
    GpCopyArg        msg;
    s32              n;

    switch (work->field_3E) {
        case 0:
            msgWork = (Actor342100Work*)arg0->work;
            n       = 0;
            while (D_actor_342100_80164900[n & 0xFFFF] != 0) {
                n += 1;
            }
            msg.source.sets = &D_actor_342100_80164900[0];
            msg.count       = n & 0xFFFF;
            Gp_DispatchMsgPtr(msgWork->field_2C, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &msg, 0);
            Gp_MsgPlayerWeapon(0);
            Gp_StateC08.field_6 |= 1;
            func_800E8614(D_actor_342100_801649C8, 0);
            Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA4, 0, 0);
            work->field_34 = Task_SpawnFromTable(D_actor_342100_80164B78, 2, 0, arg0);
            work->field_3E = work->field_3E + 1;
            break;
        case 1:
            if (gGameSession->eventState != 0) {
                break;
            }
            return 1;
    }
    func_actor_342100_801629B8(arg0);
    return 0;
}

/// The overlay's event/controller task. Idles while the session or any of
/// the global pause flags hold it. State 0 allocates the work block and
/// publishes the task, then picks state 1 or 2 from
/// `gGameSession->spawnPhase[0]`; state 1 waits on flag 0x11E and pending
/// object 5, and states 1 and 2 both move to 3 once `field_120` has dropped
/// to zero while the player still has HP. State 3 ticks
/// `func_actor_342100_80162F54` until it reports done.
///
/// `work` is read from `work` before state 0 replaces it, so the two
/// `field_30` stores go through the block the task held on entry.
void func_actor_342100_801630A4(Task* arg0)
{
    u16              id;
    s8               kind;
    u8               extra;
    Actor342100Work* work;
    Actor342100Work* newWork;
    s32              ready;
    PlayerStatus*    cfg;

    work = (Actor342100Work*)arg0->work;
    if (gGameSession->sceneUpdatesPaused != 0 || Gp_StateC08.field_9 != 0 || Gp_StateF0.field_4 != 0 || D_80114CF8 != 0) {
        return;
    }
    switch (arg0->state) {
        case 0:
            if (Gp_StateC08.field_A == 1 || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                break;
            }
            newWork    = Mem_Malloc(0x44, 0);
            arg0->work = newWork;
            if (newWork == NULL) {
                taskKill(arg0);
            } else {
                Mem_Set(newWork, 0, 0x44);
                newWork->field_2C       = gameGetPtrSlot(3);
                D_actor_342100_80164BB8 = arg0;
            }
            Task_SpawnFromTable(D_shelter_b3_dumping_hole_8018B57C, 0, 0xD0, 0);
            SndEvt_EnqueueType6(0x54270007, 0, 0);
            switch (gGameSession->spawnPhase[0]) {
                case GAME_SESSION_SPAWN_IDLE:
                    arg0->state++;
                    break;
                case GAME_SESSION_SPAWN_ARMED:
                    work->field_30 = Task_SpawnFromTable(D_shelter_b3_dumping_hole_8018B83C, 0, 1, 0);
                default:
                    arg0->state = 2;
                    break;
            }
            break;
        case 1:
            if (GameFlag_GetNibble(0x11E) != 0) {
                if (Gp_TakePendingObj4C(&id, (u8*)&kind, &extra) != 0 && (id & 0x7FFF) == 5 && kind == 1) {
                    work->field_30 = Task_SpawnFromTable(D_shelter_b3_dumping_hole_8018B83C, 0, 0, 0);
                    arg0->state++;
                }
            }
            cfg = &Player_Status;
            if (gGameSession->sceneClock > 0 || cfg->hp <= 0) {
                ready = 0;
            } else {
                ready = 1;
            }
            if (ready) {
                arg0->state = 3;
            }
            break;
        case 2:
            cfg = &Player_Status;
            if (gGameSession->sceneClock > 0 || cfg->hp <= 0) {
                ready = 0;
            } else {
                ready = 1;
            }
            if (ready) {
                arg0->state = 3;
            }
            break;
        case 3:
            if ((s16)func_actor_342100_80162F54(arg0) != 0) {
                arg0->state++;
            }
            break;
        case 4:
            break;
    }
}

void func_actor_342100_80163344(Task* arg0, s32 arg1, s32 arg2)
{
    arg0->state = arg2;
}

/// Point the overlay's slot-3 task at the animation set `arg0 + 0x2F` and hand
/// the work block's `field_3C` the same value, then install the set with
/// message 0x3E8. The set's block is `Player_Status.weapon + 1` under the alternate
/// weapon configuration and `Player_Status.weapon + 0x22` otherwise; its `field_4` is the
/// same halfword the block keeps, `field_8` is 1 and `field_C` 0xF.
void func_actor_342100_8016334C(s32 arg0)
{
    Actor342100Work*     work;
    AnimationPlayRequest msg;
    s16                  anim;
    s32                  weaponId;
    s32                  setId;

    work                     = (Actor342100Work*)D_actor_342100_80164BB8->work;
    anim                     = arg0 + 0x2F;
    weaponId                 = Player_Status.weapon;
    setId                    = (Mc_SaveData[0].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.source.index         = setId;
    work->field_3C           = anim;
    msg.animationId          = anim;
    msg.blend                = ANIMATION_BLEND_INTERPOLATE;
    msg.blendFrames          = 0xF;
    msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    Gp_DispatchMsgPtr(work->field_2C, ANIMATION_MESSAGE_PLAY, &msg, 0);
}

void func_actor_342100_801633D0(s32 arg0)
{
    Actor342100Work* work = (Actor342100Work*)D_actor_342100_80164BB8->work;

    Gp_DispatchMsg(work->field_34, 0x7DB, arg0, 0);
}

/// Seed the spawn entry's two parameters and start the task that consumes
/// them, passing the block itself as `Task::spawnArg2`.
void func_actor_342100_80163408(void)
{
    Actor342100Work* work = (Actor342100Work*)D_actor_342100_80164BB8->work;

    work->wave.span  = 0x258;
    work->wave.scale = 0x100;
    Task_SpawnFromTable(D_actor_342100_801648DC, 0, 0, &work->wave);
}

/// Entry/exit of the overlay's spawned child. A zero arm plays the cue, asks
/// slot 4 to forward message 0x7DB with the `{ 0, 0x2C, 4 }` record, passes the
/// same record on to `field_30` if that target exists, and starts the child at
/// entry 3; a non-zero arm tells the already-spawned child so through its
/// `Task::spawnArg1`.
void func_actor_342100_80163454(s32 arg0)
{
    Actor342100Work* work = (Actor342100Work*)D_actor_342100_80164BB8->work;
    ActorCommand     msg;

    if (arg0 == 0) {
        SndEvt_EnqueueType6(0x54270005, 0, 0);
        Gp_PulseState1C();
        msg.context.loc.area  = 0x2C;
        msg.context.loc.stage = 0;
        msg.command           = 4;
        // The message ABI carries the borrowed record's address in one word.
        Gp_DispatchMsg(gameGetPtrSlot(4), SCENE_MESSAGE_BROADCAST_TO_ACTORS, (s32)&msg, ACTOR_COMMAND_MESSAGE_APPLY);
        if (work->field_30 != NULL) {
            Gp_DispatchMsgPtr(work->field_30, ACTOR_COMMAND_MESSAGE_APPLY, &msg, 0);
        }
        work->field_38 = Task_SpawnFromTable(D_actor_342100_80164B78, 3, 0, 0);
        return;
    }
    work->field_38->spawnArg1.value = 1;
}

void func_actor_342100_80163518(void)
{
    Player_Status.hp          = 0;
    gGameSession->restartMode = GAME_SESSION_RESTART_PRESERVE_DISPLAY;
}
