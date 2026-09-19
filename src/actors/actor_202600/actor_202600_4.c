#include "common.h"

#include "actors/actor_202600.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/task.h"
#include <psyq/libgs.h>

/// Spawn handler: allocates the actor's work block, links the four `GpObj`
/// nodes (two collision-record tables, the coordinates and the effect arg) and
/// seeds the initial state from the spawn parameters. The spawn mode splits
/// into a tens digit (unused) and a units digit: 0 and 1 either place the actor
/// on a table row and rotate it to face that row's angle, or fall through to
/// the "walk to the player" state, 2 and 3 lift the coordinate and arm a
/// timer. Modes >= 10 re-read the variant index from the parameters.
void func_actor_202600_8014CE1C(GpEnemy* ctx, Task* actor)
{
    SVECTOR            rot;
    GpRec18*           rec0;
    GpRec18*           rec1;
    GpRec18*           rec2;
    GpRec18*           rec3;
    SVECTOR*           positions;
    MATRIX*            matrix;
    Actor202600Work*   work;
    s32                variant;
    s32                quotient;
    s32                i;
    s32                mode;
    Actor202600Params* params;
    GsCOORDINATE2*     coord;
    Actor202600Obj2C*  obj;

    obj   = actor->extra;
    coord = obj->field_8;
    work  = memCalloc(sizeof(Actor202600Work), 0);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->work     = (void*)work;
    obj->field_C    = 0;
    coord->flg      = 0;
    obj->field_1C   = &work->field_1F4;
    obj->field_20   = &work->field_1D4;
    matrix          = &coord->coord;
    work->field_3C0 = 0;
    work->field_36C = &D_actor_202600_801528D4;
    ctx->field_4    = matrix;
    ctx->field_48   = 0;
    Gp_LinkNode(&ctx->node);
    ctx->field_18           = ((Actor202600Obj2C*)actor->extra)->field_8 + 1;
    ctx->field_1C.vy        = -0x64;
    ctx->field_54           = (s32)work->field_2B4;
    ctx->field_1C.vx        = 0;
    ctx->field_1C.vz        = 0;
    ctx->field_50           = &D_actor_202600_80152788;
    ctx->field_40           = (s16)D_actor_202600_80152788.field_4;
    work->field_354.field_0 = coord;
    work->field_354.field_4 = 0x100;
    work->field_354.field_6 = 1;
    work->field_3C4         = (s16)((Actor202600Params*)ctx->field_3C)->field_1;
    params                  = ctx->field_3C;
    mode                    = params->field_2;
    if (mode < 10) {
        switch (mode) {
            case 0:
                work->field_392 = 0xC;
                work->field_39A = 0;
                work->field_3A8 = 0x80;
                work->field_3C6 = 0;
                break;
            case 1:
                work->field_39A = mode;
                work->field_392 = mode;
                work->field_3A8 = 0x80;
                work->field_3C6 = 0;
                break;
            case 2:
                work->field_39A    = mode;
                work->field_392    = 6;
                work->field_3A8    = 0;
                work->field_3C6    = 0;
                work->field_3C8    = 1;
                coord->coord.t[1] += 0x3E8;
                break;
            case 3:
                work->field_39A    = 2;
                work->field_392    = 6;
                work->field_3A8    = 0;
                work->field_3C6    = 1;
                work->field_3C8    = 1;
                coord->coord.t[1] += 0x3E8;
                break;
        }
    } else {
        work->field_3C4 = (s16)params->field_1;
        quotient        = mode / 10;
        variant         = mode - quotient * 10;
        switch (variant) {
            case 0:
                if (GameFlag_GetNibble(0xCC) == 1) {
                    work->field_392 = 0xC;
                    work->field_39A = 0;
                    work->field_3A8 = 0x80;
                    rot.vx          = 0;
                    rot.vy          = D_actor_202600_801527F0[work->field_3C4];
                    rot.vz          = 0;
                    RotMatrix(&rot, matrix);
                    positions         = D_actor_202600_801527D0;
                    coord->coord.t[0] = positions[work->field_3C4].vx;
                    coord->coord.t[1] = positions[work->field_3C4].vy;
                    coord->coord.t[2] = positions[work->field_3C4].vz;
                } else {
                    work->field_3C2 = 0;
                    work->field_39A = 8;
                    work->field_392 = 1;
                    work->field_3A8 = 0;
                }
                break;
            case 1:
                if (GameFlag_GetNibble(0xCB) == 2) {
                    work->field_392 = 0xC;
                    work->field_39A = 0;
                    work->field_3A8 = 0x80;
                    rot.vx          = 0;
                    rot.vy          = D_actor_202600_80152828[work->field_3C4];
                    rot.vz          = 0;
                    RotMatrix(&rot, matrix);
                    positions         = D_actor_202600_80152808;
                    coord->coord.t[0] = positions[work->field_3C4].vx;
                    coord->coord.t[1] = positions[work->field_3C4].vy;
                    coord->coord.t[2] = positions[work->field_3C4].vz;
                    break;
                }
                work->field_39A    = 8;
                work->field_3C2    = variant;
                work->field_392    = 6;
                work->field_3A8    = 0;
                work->field_3C8    = variant;
                coord->coord.t[1] += 0x3E8;
        }
    }
    func_800B3F84((GpAnimCtx*)work, D_actor_202600_801528EC, (TmdObject*)obj, work->field_154, ((Actor202600Anim*)work)->slots);
    for (i = 1; i < 8; i++) {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, 1);
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    rec0                     = work->field_234;
    work->field_214.coord    = coord;
    work->field_214.ctx.recs = rec0;
    work->field_214.pos.vx   = 0;
    work->field_214.pos.vy   = -0x12C;
    work->field_214.pos.vz   = 0;
    work->field_214.key      = 0x3001A;
    work->field_214.radius   = 0x12C;
    work->field_214.flags    = 1;
    Gp_LinkObj(2, &work->field_214);
    Gp_InitRec18Table(rec0, 4, 0);
    work->field_214.flags   |= 0x4200;
    work->field_294.coord    = ((Actor202600Obj2C*)actor->extra)->field_8 + 1;
    rec1                     = work->field_2B4;
    work->field_294.ctx.recs = rec1;
    work->field_294.pos.vx   = 0;
    work->field_294.pos.vy   = -0x64;
    work->field_294.pos.vz   = 0;
    work->field_294.key      = 0x3001A;
    work->field_294.radius   = 0x12C;
    work->field_294.flags    = 1;
    Gp_LinkObj(2, &work->field_294);
    Gp_InitRec18Table(rec1, 2, 0);
    work->field_294.flags   |= 0x8000;
    work->field_2E4.coord    = ((Actor202600Obj2C*)actor->extra)->field_8 + 4;
    rec2                     = work->field_304;
    work->field_2E4.ctx.recs = rec2;
    work->field_2E4.pos.vx   = 0;
    work->field_2E4.pos.vy   = 0;
    work->field_2E4.pos.vz   = 0;
    work->field_2E4.key      = 0;
    work->field_2E4.radius   = 0xC8;
    work->field_2E4.flags    = 1;
    Gp_LinkObj(3, &work->field_2E4);
    Gp_InitRec18Table(rec2, 1, 0);
    work->field_2E4.flags   &= 0x7FFF;
    work->field_31C.coord    = ((Actor202600Obj2C*)actor->extra)->field_8 + 4;
    rec3                     = work->field_33C;
    work->field_31C.ctx.recs = rec3;
    work->field_31C.pos.vx   = 0;
    work->field_31C.pos.vy   = 0;
    work->field_31C.pos.vz   = 0;
    work->field_31C.key      = 0x22424;
    work->field_31C.radius   = 0x1F4;
    work->field_31C.flags    = 1;
    Gp_LinkObj(1, &work->field_31C);
    Gp_InitRec18Table(rec3, 1, 0);
    work->field_31C.flags &= 0x7FFF;
    actor->state           = 1;
}
