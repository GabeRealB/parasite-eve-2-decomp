#include "common.h"

#include "main/mem.h"
#include "main/gfx.h"
#include "main/task.h"

#include "gameplay/1BC.h"
#include "gameplay/3FB8.h"

#include "actors/actor_107600.h"

/* The controller task this actor is reparented to is the Mist shooting
 * gallery's, so the counter at +0xE of its work block is that room's. */
#include "rooms/mist_shooting_gallery.h"

/* Leading-rodata state table. A local initializer would emit the pool at this
 * function's .rodata instead of at D_actor_107600_80131E24. */
extern const TaskFuncTable4 D_actor_107600_80131E24;

/* Second table out of the same leading-rodata block, run by
 * `func_actor_107600_801348A0`. */
extern const TaskFuncTable4 D_actor_107600_80131E74;

/* Third table out of that block, run by `func_actor_107600_80132CD4` and
 * holding one entry per `Actor107600Work.field_144` phase. */
extern const TaskFuncTable3 D_actor_107600_80131E34;

/* Table `func_actor_107600_80132DF0` spawns from, indexed with `arg1 + 1`; it
 * is the trailing animation/data blob, not the leading rodata. */
extern TaskDesc D_actor_107600_80134F94;

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_80131F10);

INCLUDE_RODATA("actors/nonmatchings/actor_107600/actor_107600", D_actor_107600_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_107600/actor_107600", D_actor_107600_80131E24);

INCLUDE_RODATA("actors/nonmatchings/actor_107600/actor_107600", D_actor_107600_80131E34);

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_80132160);

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_80132514);

void func_actor_107600_801328CC(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = D_actor_107600_80131E24;
    sp.funcs[arg0->state](arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_80132930);

/// State 2 of the `D_actor_107600_80131E24` table: drops this instance from the
/// spawning gallery's live-target count unless it was spawned already counted
/// (phase 2, the `0x200D` cursor target), then advances to the exit state.
void func_actor_107600_80132A7C(Task* arg0)
{
    Task*            parent;
    Actor107600Work* work;

    parent = arg0->parent;
    work   = (Actor107600Work*)arg0->idMap;
    if (work->field_144 != 2) {
        ((MistShootingGalleryWork*)parent->idMap)->field_0E--;
    }
    arg0->state++;
}

/// Enemy exit callback: releases the shared state slot unless the work block
/// has already reached phase 2, then hands the enemy back for destruction.
void func_actor_107600_80132AC0(Task* arg0)
{
    Actor107600Work* work = (Actor107600Work*)arg0->idMap;

    if (work->field_144 != 2) {
        Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    }
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

/// Copies the world position of the model's first attach coordinate onto a
/// 0x10-byte `VECTOR` carved off `G_SCRATCH_HEAD` and hands it to
/// `Gp_UpdateActorColor` for the enemy in `Task::spawnArg2` with no blend
/// parameters. Same shape as `func_actor_107600_801349E0`, a different callee.
void func_actor_107600_80132B0C(Task* arg0)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;
    void*          obj;

    obj       = arg0->spawnArg2;
    coord     = ((TmdObject*)arg0->extra)->field_8;
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    Gp_UpdateActorColor(obj, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}

/// Rebuilds the model root's rotation from the work block's three angles: wrap
/// each to 12 bits, build the rotation in a scratch matrix carved off
/// `G_SCRATCH_HEAD`, then copy its 3x3 into the part's `GsCOORDINATE2::coord`.
/// The same body as `ActorsShared80139948`, with the copy left as a call to
/// `func_actor_107600_80132C4C`.
void func_actor_107600_80132B7C(Task* arg0)
{
    Actor107600Work* work  = (Actor107600Work*)arg0->idMap;
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->field_8;
    MATRIX*          m;

    work->pitch              &= 0xFFF;
    work->yaw                &= 0xFFF;
    work->roll               &= 0xFFF;
    m                         = (MATRIX*)(*(u8**)G_SCRATCH_HEAD - 0x20);
    *(s32*)&m->m[0][0]        = 0x1000;
    *(s32*)&m->m[0][2]        = 0;
    *(s32*)&m->m[1][1]        = 0x1000;
    *(s32*)&m->m[2][0]        = 0;
    m->m[2][2]                = 0x1000;
    *(MATRIX**)G_SCRATCH_HEAD = m;
    RotMatrixZ((s16)work->roll, m);
    RotMatrixX((s16)work->pitch, m);
    func_8004BFF8((s16)work->yaw, m);
    func_actor_107600_80132C4C(m, &coord->coord);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x20;
}

/// Copies the 3x3 rotation of the scratch matrix `func_actor_107600_80132B7C`
/// just built into the part's `GsCOORDINATE2::coord`, leaving the translation
/// row of the destination alone.
void func_actor_107600_80132C4C(MATRIX* src, MATRIX* dst)
{
    dst->m[0][0] = src->m[0][0];
    dst->m[0][1] = src->m[0][1];
    dst->m[0][2] = src->m[0][2];
    dst->m[1][0] = src->m[1][0];
    dst->m[1][1] = src->m[1][1];
    dst->m[1][2] = src->m[1][2];
    dst->m[2][0] = src->m[2][0];
    dst->m[2][1] = src->m[2][1];
    dst->m[2][2] = src->m[2][2];
}

void func_actor_107600_80132CB8(Actor107600* arg0)
{
    Actor107600Work* work = arg0->field_1C;

    work->field_13E++;
}

/// Runs the `D_actor_107600_80131E34` entry for the work block's `field_144`
/// phase through the same stack-copied table idiom as
/// `func_actor_107600_801328CC`, then advances the model's yaw by 0x20 once the
/// spawn flag at `field_14A` says this instance is rotating.
void func_actor_107600_80132CD4(Task* arg0)
{
    TaskFuncTable3   sp;
    Actor107600Work* work = (Actor107600Work*)arg0->idMap;

    sp = D_actor_107600_80131E34;
    sp.funcs[work->field_144](arg0);
    if (work->field_14A != 0) {
        work->yaw += 0x20;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_80132D54);

/// Spawns the next instance of this actor's own `D_actor_107600_80134F94`
/// table (`arg1 + 1` is the index) and adopts it as a child of `arg0`: the new
/// task is reparented and its root coordinate's `sub` link is pointed at
/// `arg0`'s own root coordinate, `Task::spawnArg1` is packed from the two
/// arguments, and the spawned model takes `arg1`'s texture page - dropping the
/// CLUT row to 0 once `arg1` reaches 10 - before the stream is processed twice
/// (one half-buffer per call) and the enemy's light is set to 0x900.
void func_actor_107600_80132DF0(GpEnemy* arg0, s32 arg1, s32 arg2)
{
    GpEnemy*    enemy;
    GpCoordExt* coord;
    TmdObject*  obj;

    enemy = Gp_SpawnEnemyFromTable(&D_actor_107600_80134F94, arg1 + 1, arg2, arg0);
    if (enemy != NULL) {
        Task_Reparent(arg0->task, enemy->task);
        coord                  = (GpCoordExt*)((TmdObject*)enemy->task->extra)->field_8;
        coord->sub             = ((TmdObject*)arg0->task->extra)->field_8;
        enemy->task->spawnArg1 = arg1 | (arg2 << 16);
        obj                    = (TmdObject*)enemy->task->extra;
        obj->field_24          = 0;
        if (arg1 < 10) {
            obj->field_25 = 2;
        } else {
            obj->field_25 = 0;
        }
        Tmd_ProcessStream(obj);
        Tmd_ProcessStream(obj);
        enemy->field_A = 0x900;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_80132ED0);

INCLUDE_RODATA("actors/nonmatchings/actor_107600/actor_107600", D_actor_107600_80131E74);

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_80133024);

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_801332D4);

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_80133668);

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_801337FC);

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_801339A4);

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_80133DC4);

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_80133FA8);

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_80134248);

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_801344E8);

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_80134608);

void func_actor_107600_801348A0(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = D_actor_107600_80131E74;
    sp.funcs[arg0->state](arg0);
}

void func_actor_107600_80134904(Task* arg0)
{
    TmdObject* obj = arg0->extra;

    obj->field_C |= 0x80;
}

void func_actor_107600_80134920(Task* arg0)
{
    Gp_UnlinkObj(&((Actor107600Work*)arg0->idMap)->obj);
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

/// Links this actor's display node the way `func_8010C980` does for the
/// gameplay objects: the node's collision table is the `GpRec18` run at
/// `work->rec18` (count 8), and its `field_1C` payload is the spawn variant's
/// height, 0x220 for the `field_162 == 1` variant and 0x190 otherwise.
void func_actor_107600_80134958(Task* arg0)
{
    Actor107600Work* work  = (Actor107600Work*)arg0->idMap;
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->field_8;
    GpRec18*         rec   = work->rec18;

    work->obj.field_8  = coord;
    work->obj.field_C  = rec;
    work->obj.field_10 = 0;
    work->obj.field_12 = -0x250;
    work->obj.field_14 = 0;
    work->obj.field_18 = 0x3004C;
    work->obj.field_1C = (work->field_162 == 1) ? 0x220 : 0x190;
    work->obj.flags    = 1;
    Gp_LinkObj(2, &work->obj);
    Gp_InitRec18Table(rec, 8, 0);
}

/// Copies the world position of the model's first attach coordinate onto a
/// 0x10-byte `VECTOR` carved off `G_SCRATCH_HEAD` and hands it to
/// `func_actor_107600_80134608` with no blend parameters.
void func_actor_107600_801349E0(Task* arg0)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;
    void*          obj;

    obj       = arg0->spawnArg2;
    coord     = ((TmdObject*)arg0->extra)->field_8;
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    func_actor_107600_80134608(obj, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}

/// Builds a second rotation from the work block's angle trio at +0x50: wrap
/// each to 12 bits, lay an unscaled `MATRIX` down at the scratchpad head, apply
/// `Gfx_RotMatrixZ`/`X`/`Y` to it in that order, copy its 3x3 into the model's
/// `GsCOORDINATE2::coord` through `func_actor_107600_80134B2C`, and hand the
/// scratch block back.
void func_actor_107600_80134A50(Task* arg0)
{
    Actor107600Work* work  = (Actor107600Work*)arg0->idMap;
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->field_8;
    MATRIX*          m;

    work->field_50           &= 0xFFF;
    work->field_52           &= 0xFFF;
    work->field_54           &= 0xFFF;
    m                         = (MATRIX*)(*(u8**)G_SCRATCH_HEAD - 0x20);
    *(s32*)&m->m[0][0]        = 0x1000;
    *(s32*)&m->m[0][2]        = 0;
    *(s32*)&m->m[1][1]        = 0x1000;
    *(s32*)&m->m[2][0]        = 0;
    m->m[2][2]                = 0x1000;
    *(MATRIX**)G_SCRATCH_HEAD = m;
    Gfx_RotMatrixZ(m, (s16)work->field_54, 0);
    Gfx_RotMatrixX(m, (s16)work->field_50, 0);
    Gfx_RotMatrixY(m, (s16)work->field_52, 0);
    func_actor_107600_80134B2C(m, &coord->coord);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x20;
}

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_80134B2C);

void func_actor_107600_80134B98(Actor107600* arg0, s16 arg1)
{
    Actor107600Work* work = arg0->field_1C;

    work->field_158 = arg1;
    work->field_15A = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_80134BAC);

/// Opens the spawn variant `work->field_162` selects. The rolling variants
/// (0 and 1) start the state at 1, kick the +0x50 rotation trio off at 0x400,
/// roll `Gp_LcgState` into `field_16A` beside the 10 percent scale pair
/// `func_actor_107600_80134EF4` divides the model root's rotation by, rebuild
/// that rotation through `func_actor_107600_80134A50`, and put the spawned
/// object's light into mode 2 with its blend timer cleared. Variant 2 only
/// starts the state at 8 and leaves the scale pair at 100 percent.
void func_actor_107600_80134C54(Task* arg0)
{
    Actor107600Work* work = (Actor107600Work*)arg0->idMap;
    GpObj4C*         obj  = arg0->spawnArg2;

    switch (work->field_162) {
        case 0:
        case 1:
            work->field_158 = 1;
            work->field_50  = 0x400;
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_16A = (Gp_LcgState >> 16) & 7;
            work->field_168 = 10;
            work->field_169 = 10;
            func_actor_107600_80134A50(arg0);
            Gp_SetLightMode(obj, 2);
            obj->field_4F = 0;
            break;
        case 2:
            work->field_158 = 8;
            work->field_168 = 100;
            work->field_169 = 100;
            break;
    }
}

void func_actor_107600_80134D10(Actor107600* arg0)
{
    func_actor_107600_80134B98(arg0, 1);
}

void func_actor_107600_80134D30(Actor107600* arg0)
{
    func_actor_107600_80134B98(arg0, 1);
}

void func_actor_107600_80134D50(Actor107600* arg0)
{
    func_actor_107600_80134B98(arg0, 1);
}

void func_actor_107600_80134D70(Actor107600* arg0)
{
    arg0->field_1C->field_16B = 3;
    func_actor_107600_80134B98(arg0, 7);
}

INCLUDE_ASM("actors/nonmatchings/actor_107600/actor_107600", func_actor_107600_80134D9C);

/// Rotates a fixed 0x10-byte offset by the coordinate's own `coord` matrix and
/// leaves the result in that matrix's translation row. The offset is carved off
/// `G_SCRATCH_HEAD` the way `func_actor_107600_80132B0C` carves its VECTOR, but
/// is filled with (0, -0x180, 0) and rotated in place by `ApplyMatrixLV`, which
/// also folds in the matrix's existing translation. `func_actor_107600_80132ED0`
/// calls this on the coordinate it then hands to `Gp_UpdateCoord`.
void func_actor_107600_80134E5C(GsCOORDINATE2* arg0)
{
    void**  scratch;
    u8*     head;
    VECTOR* block;

    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    *scratch  = block;
    block->vx = 0;
    block->vy = -0x180;
    block->vz = 0;
    ApplyMatrixLV(&arg0->coord, block, block);
    arg0->coord.t[0] = block->vx;
    arg0->coord.t[1] = block->vy;
    *scratch         = (u8*)*scratch + 0x10;
    arg0->coord.t[2] = block->vz;
}

/// Scales the model root's rotation by the two percent factors
/// `func_actor_107600_80134C54` rolls into the work block: the diagonal
/// `coord.m[0][0]` and `coord.m[2][1]` halves, each read as a raw 16-bit value
/// and re-signed before the divide so the scale stays signed.
void func_actor_107600_80134EF4(Task* arg0)
{
    Actor107600Work* work  = (Actor107600Work*)arg0->idMap;
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->field_8;
    u16              x     = coord->coord.m[0][0];
    u16              y     = coord->coord.m[2][1];

    coord->coord.m[0][0] = (s16)x / 100 * work->field_168;
    coord->coord.m[2][1] = (s16)y / 100 * work->field_169;
}
