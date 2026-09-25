#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// One step of gameplay's LCG, `state = state * 5 + 0x71357911`, as its high half.
#define ACTOR_341300_RAND() ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16)

/// 0x30 block `func_actor_341300_80162878` and `func_actor_341300_801631D4`
/// allocate into `Task::work`: a tumbling Gouraud triangle shard with its own
/// spin and velocity.
typedef struct {
    SVECTOR rot;
    SVECTOR rotSpeed;
    SVECTOR vel;
    SVECTOR verts[3];
} Actor341300Shard;
STATIC_ASSERT_SIZEOF(Actor341300Shard, 0x30);

extern s8 D_8007272D;

/// Spawn positions `func_actor_341300_80162878`'s shards start from, indexed
/// by `Task::spawnArg1`.
extern SVECTOR D_actor_341300_80165A38[];

/// Spawn positions `func_actor_341300_801631D4`'s shards start from, indexed
/// by `Task::spawnArg1`.
extern SVECTOR D_actor_341300_80165A58[];

/// Placement record the overlay's data table points at, read here only as the
/// target position's x/z pair.
extern VECTOR D_actor_341300_80165330;

extern Task* D_actor_341300_80165A2C;

extern TaskDesc D_actor_341300_80165A68;

extern TaskDesc D_actor_341300_80165208;

extern Task* D_actor_341300_80165AA4;

void func_actor_341300_8016398C(s32 arg0);
void func_actor_341300_801639CC(s32 arg0);

/// Draws the two textured quads at fixed positions: each is four fixed
/// model-space corners projected through `Gfx_ViewWorldMtx`, emitted as a
/// POLY_FT4 at the depth `RotTransPers3` returns, and skipped when the
/// projection flags an error.
void func_actor_341300_80161E84(void)
{
    s16     x[8];
    s16     y[8];
    s32     sxy[8];
    s32     otz[2];
    SVECTOR v[8] = {
        { 0x80C, -0xBC6, 0x150 },
        { 0x83C, -0xBC6, 0x150 },
        { 0x80C, -0xBC6, 0x180 },
        { 0x83C, -0xBC6, 0x180 },
        { 0x747, -0xBC6, 0x150 },
        { 0x777, -0xBC6, 0x150 },
        { 0x747, -0xBC6, 0x180 },
        { 0x777, -0xBC6, 0x180 },
    };
    s32       p;
    s32       flag;
    s32       i;
    s32       j;
    s32       k;
    POLY_FT4* prim;

    SetRotMatrix(&Gfx_ViewWorldMtx);
    SetTransMatrix(&Gfx_ViewWorldMtx);
    for (i = 0; i < 2; i++) {
        k = i * 4;
        RotTransPers(&v[k + 3], &sxy[k + 3], &p, &flag);
        otz[i] = RotTransPers3(&v[k], &v[k + 1], &v[k + 2], &sxy[k], &sxy[k + 1], &sxy[k + 2], &p, &flag);
        if (flag >= 0) {
            for (j = k; j < k + 4; j++) {
                x[j] = sxy[j];
                y[j] = sxy[j] >> 16;
            }
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2D);
            prim->x0 = x[k];
            prim->y0 = y[k];
            prim->x1 = x[k + 1];
            prim->y1 = y[k + 1];
            prim->x2 = x[k + 2];
            prim->y2 = y[k + 2];
            prim->x3 = x[k + 3];
            prim->y3 = y[k + 3];
            setUV4(prim, 0x23, 0xD1, 0x2F, 0xD1, 0x23, 0xDD, 0x2F, 0xDD);
            setRGB0(prim, 0x80, 0x80, 0x80);
            prim->clut  = 0x3E00;
            prim->tpage = 0x97;
            addPrim((u_long*)((((u32)(otz[i] << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
        }
    }
}

/// Per-frame task that turns the player (`gameGetPtrSlot(3)`, whose
/// `Task::work` is the `GameActor` block) to face the object the area work
/// id resolves to, then kills itself once it is close enough.
///
/// The aim angle is `ratan2` of the target's x/z pair minus the player's own
/// coordinate translation; the delta against `GameActor::field_52` is
/// unwrapped into `-0x800..0x800` and stepped by `0x80` per frame, so the
/// player rotates at a fixed rate. Inside `0x80` of the target the facing
/// snaps to the exact angle and the task ends.
///
/// The task's own argument is only ever the `taskKill` target, reached both
/// when the work lookup or `gGameSession::eventState` fails and on the frame the
/// facing settles.
void func_actor_341300_80162278(Task* task)
{
    Task*      player;
    GameActor* actor;
    GpWorkObj* work;
    GpCoord*   self;
    VECTOR*    target;
    s32        angle;
    s32        delta;
    s32        magnitude;
    s32        step;
    s32        wrapped;

    player = gameGetPtrSlot(3);
    actor  = (GameActor*)player->work;
    work   = Gp_FindWorkById(gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8));
    if ((work != NULL) && (gGameSession->eventState != 0)) {
        self      = player->extra.tmd->coords;
        target    = &D_actor_341300_80165330;
        angle     = ratan2(target->vx - self->coord.t[0], target->vz - self->coord.t[2]);
        delta     = angle - actor->field_52;
        magnitude = ABS(delta);
        if (magnitude >= 0x801) {
            wrapped = delta - 0x1000;
            if (delta < 0) {
                wrapped = delta + 0x1000;
            }
            delta = wrapped;
        }
        magnitude = ABS(delta);
        if (magnitude >= 0x81) {
            step = 0x80;
            if (delta < 0) {
                step = -0x80;
            }
            actor->field_52 = (s16)((u16)actor->field_52 + step);
            return;
        }
        actor->field_52 = angle;
    }
    taskKill(task);
}

/// Record handler (opcode 0x0D) of the actor's script data: queues the
/// replacing load of overlay 0x82.
void func_actor_341300_8016239C(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

/// Record handler (opcode 0x0D) of the actor's script data: queues the load
/// of overlay 0x81.
void func_actor_341300_801623BC(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Record handler (opcode 0x0D) of the actor's script data: restores the
/// stream random-number state.
void func_actor_341300_801623DC(void)
{
    Gp_RestoreStreamRng();
}

/// Record handler (opcode 0x0D) of the actor's script data: cancels the queued
/// CD command and restarts the CD queue.
void func_actor_341300_801623FC(void)
{
    CdCmd_CancelReplaceAndActivate();
}

void func_actor_341300_8016241C(void)
{
    D_actor_341300_80165AA4 = Task_SpawnFromTable(&D_actor_341300_80165208, 0, 0, 0);
}

void func_actor_341300_80162450(void)
{
    if (D_actor_341300_80165AA4 != NULL) {
        D_actor_341300_80165AA4->state         = -1;
        D_actor_341300_80165AA4->killCountdown = 0;
        D_actor_341300_80165AA4                = NULL;
    }
}

void func_actor_341300_80162478(Task* arg0)
{
    s16 next;
    s16 count;

    switch (arg0->state) {
        case 0:
            next                = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = next;
            if (next != 0x3D) {
                func_actor_341300_80161E84();
                return;
            }
            return;
        case 1:
            count = arg0->killCountdown;
            if (count < 0x1E) {
                if ((count != 0xA) && (count != 0x14)) {
                    func_actor_341300_80161E84();
                }
                arg0->killCountdown = (u16)arg0->killCountdown + 1;
                return;
            }
        default:
            taskKill(arg0);
            break;
    }
}

void func_actor_341300_80162530(void)
{
    D_actor_341300_80165AA4 = Task_SpawnFromTable(&D_actor_341300_80165208, 1, 0, 0);
}

void func_actor_341300_80162564(s16 arg0)
{
    func_actor_341300_8016398C(arg0);
}

void func_actor_341300_80162588(s16 arg0)
{
    func_actor_341300_801639CC(arg0);
}

/// Offset from the player's third coordinate that `func_actor_341300_801625AC`
/// spawns its four effects at.
const SVECTOR D_actor_341300_80161E64 = { 100, -200, -100, 0 };

void func_actor_341300_801625AC(void)
{
    SVECTOR  vec   = D_actor_341300_80161E64;
    GpCoord* coord = &(gameGetPtrSlot(3))->extra.tmd->coords[2];

    Gp_SpawnEff(0x60055, coord, 0x10013300, &vec);
    Gp_SpawnEff(0x60055, coord, 0x10112280, &vec);
    Gp_SpawnEff(0x60055, coord, 0x10112280, &vec);
    Gp_SpawnEff(0x60055, coord, 0x10112280, &vec);
}

void func_actor_341300_80162680(s8 arg0)
{
    D_8007272D = arg0;
}

void func_actor_341300_8016268C(void)
{
    D_actor_341300_80165AA4 = 0;
}

void func_actor_341300_80162698(Task* arg0)
{
    s16 i;
    s16 next;
    u16 count;

    switch (arg0->state) {
        case 0:
            i = 0;
            do {
                Task_SpawnFromTable(&D_actor_341300_80165A68, 1, 0, (s32)arg0);
                next = i + 1;
                i    = next;
            } while (next < 0xA);
            goto done;
        case 1:
            count               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = count;
            i                   = 0;
            if ((s16)count >= 0x1F) {
                do {
                    Task_SpawnFromTable(&D_actor_341300_80165A68, 1, 0, (s32)arg0);
                    next = i + 1;
                    i    = next;
                } while (next < 0xA);
                goto done;
            }
            break;
        case 2:
            count               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = count;
            i                   = 0;
            if ((s16)count >= 0x10) {
                do {
                    Task_SpawnFromTable(&D_actor_341300_80165A68, 1, 1, (s32)arg0);
                    next = i + 1;
                    i    = next;
                } while (next < 0xA);
                goto done;
            }
            break;
        case 3:
        case 4:
            count               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = count;
            i                   = 0;
            if ((s16)count >= 0x10) {
                do {
                    Task_SpawnFromTable(&D_actor_341300_80165A68, 1, 3, (s32)arg0);
                    Task_SpawnFromTable(&D_actor_341300_80165A68, 1, 1, (s32)arg0);
                    next = i + 1;
                    i    = next;
                } while (next < 0xA);
            done:
                arg0->killCountdown = 0;
                arg0->state         = arg0->state + 1;
            }
            break;
        case 5:
            break;
    }
}

/// Falling debris shard. State 0 allocates the `Actor341300Shard`, parents the
/// actor's coordinate to the view, places it at `D_actor_341300_80165A38
/// [spawnArg1]` and rolls a random velocity, spin and triangle shape. State 1
/// applies gravity and velocity, draws the triangle as a POLY_G3 and advances
/// the spin, killing the task once the shard falls below y 0.
void func_actor_341300_80162878(Task* arg0)
{
    Actor341300Shard* work;
    GpCoord*          coord;
    POLY_G3*          prim;
    s16               x[3];
    s16               y[3];
    s32               sxy;
    s32               otz;
    s16               i;
    s32               v0;
    s32               v1;
    s32               v2;
    s32               v3;

    work  = (Actor341300Shard*)arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (arg0->state) {
        case 0:
            arg0->work = memCalloc(0x30, 0);
            if (arg0->work == NULL) {
                goto kill;
            }
            work       = (Actor341300Shard*)arg0->work;
            coord->sub = &gGfxViewCoord;
            Mem_Set(arg0->work, 0, 0x30);
            Task_Reparent(arg0->spawnArg2, arg0);
            coord->coord.t[0] = D_actor_341300_80165A38[arg0->spawnArg1].vx;
            coord->coord.t[1] = D_actor_341300_80165A38[arg0->spawnArg1].vy;
            coord->coord.t[2] = D_actor_341300_80165A38[arg0->spawnArg1].vz;
            work->vel.vx      = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & 0x1F) : -(ACTOR_341300_RAND() & 0x1F);
            work->vel.vy      = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & 0x1F) : -(ACTOR_341300_RAND() & 0x1F);
            work->vel.vz      = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & 0x1F) : -(ACTOR_341300_RAND() & 0x1F);
            work->rotSpeed.vx = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & 0x7F) : -(ACTOR_341300_RAND() & 0x7F);
            work->rotSpeed.vy = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & 0x7F) : -(ACTOR_341300_RAND() & 0x7F);
            work->rotSpeed.vz = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & 0x7F) : -(ACTOR_341300_RAND() & 0x7F);
            if (work->rotSpeed.vx > 0) {
                work->rotSpeed.vx += 100;
            } else {
                work->rotSpeed.vx -= 100;
            }
            if (work->rotSpeed.vy > 0) {
                work->rotSpeed.vy += 100;
            } else {
                work->rotSpeed.vy -= 100;
            }
            if (work->rotSpeed.vz > 0) {
                work->rotSpeed.vz += 100;
            } else {
                work->rotSpeed.vz -= 100;
            }
            work->verts[0].vx = 0;
            work->verts[0].vy = (ACTOR_341300_RAND() & 1) ? 0x16 : 0x14;
            work->verts[0].vz = 0;
            v0                = rsin(0x2AA) * 20 / 4096;
            if (ACTOR_341300_RAND() & 1) {
                v0 += 2;
            }
            work->verts[1].vx = v0;
            v1                = -(rsin(0x155) * 20 / 4096);
            if (ACTOR_341300_RAND() & 1) {
                v1 -= 2;
            }
            work->verts[1].vy = v1;
            work->verts[1].vz = 0;
            v2                = -(rsin(0x2AA) * 20 / 4096);
            if (ACTOR_341300_RAND() & 1) {
                v2 -= 2;
            }
            work->verts[2].vx = v2;
            v3                = -(rsin(0x155) * 20 / 4096);
            if (ACTOR_341300_RAND() & 1) {
                v3 -= 2;
            }
            work->verts[2].vy = v3;
            work->verts[2].vz = 0;
            arg0->state++;
            break;
        case 1:
            if (coord->coord.t[1] > 0) {
            kill:
                taskKill(arg0);
                break;
            }
            work->vel.vy      += 8;
            coord->coord.t[0] += work->vel.vx;
            coord->coord.t[1] += work->vel.vy;
            coord->coord.t[2] += work->vel.vz;
            Gp_UpdateCoord(coord);
            gte_SetTransMatrix(&coord->workm);
            gte_SetRotMatrix(&coord->workm);
            for (i = 0; i < 3; i++) {
                gte_ldv0(&work->verts[i]);
                gte_rtps();
                gte_stsxy(&sxy);
                gte_stszotz(&otz);
                x[i] = sxy;
                y[i] = sxy >> 16;
            }
            prim           = (POLY_G3*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG3(prim);
            prim->r0 = prim->g0 = prim->b0 = 0x10;
            prim->r1 = prim->g1 = prim->b1 = 0x40;
            prim->r2 = prim->g2 = prim->b2 = 0x80;
            prim->x0                       = x[0];
            prim->y0                       = y[0];
            prim->x1                       = x[1];
            prim->y1                       = y[1];
            prim->x2                       = x[2];
            prim->y2                       = y[2];
            addPrim(&gGpuCurrentOt[otz >> 4], prim);
            work->rot.vx += work->rotSpeed.vx;
            work->rot.vy += work->rotSpeed.vy;
            work->rot.vz += work->rotSpeed.vz;
            Gfx_RotMatrixY(&coord->coord, work->rot.vy, 1);
            Gfx_RotMatrixX(&coord->coord, work->rot.vx, 0);
            Gfx_RotMatrixZ(&coord->coord, work->rot.vz, 0);
            coord->flg = 0;
            break;
    }
}

void func_actor_341300_80163028(Task* arg0)
{
    u16 count;

    switch (arg0->state) {
        case 0:
            arg0->killCountdown = 0;
            arg0->state         = arg0->state + 1;
            break;
        case 1:
            count               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = count;
            if ((s16)count % 3 == 0) {
                Task_SpawnFromTable(&D_actor_341300_80165A68, 3, 0, (s32)arg0);
                Task_SpawnFromTable(&D_actor_341300_80165A68, 3, 1, (s32)arg0);
                Task_SpawnFromTable(&D_actor_341300_80165A68, 3, 1, (s32)arg0);
                arg0->state = arg0->state + 1;
            }
            break;
        case 2:
            count               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = count;
            if ((s16)count % 3 == 0) {
                Task_SpawnFromTable(&D_actor_341300_80165A68, 3, 0, (s32)arg0);
                Task_SpawnFromTable(&D_actor_341300_80165A68, 3, 0, (s32)arg0);
                Task_SpawnFromTable(&D_actor_341300_80165A68, 3, 1, (s32)arg0);
                arg0->state = arg0->state - 1;
            }
            break;
    }
    if (arg0->killCountdown >= 0x1F) {
        arg0->state = 3;
    }
}

void func_actor_341300_801631D4(Task* arg0)
{
    Actor341300Shard* work;
    GpCoord*          coord;
    POLY_G3*          prim;
    s16               x[3];
    s16               y[3];
    s32               sxy;
    s32               otz;
    s16               i;
    s32               v0;
    s32               v1;
    s32               v2;
    s32               v3;

    work  = (Actor341300Shard*)arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (arg0->state) {
        case 0:
            arg0->work = memCalloc(0x30, 0);
            if (arg0->work == NULL) {
                goto kill;
            }
            work       = (Actor341300Shard*)arg0->work;
            coord->sub = &gGfxViewCoord;
            Mem_Set(arg0->work, 0, 0x30);
            Task_Reparent(arg0->spawnArg2, arg0);
            coord->coord.t[0] = D_actor_341300_80165A58[arg0->spawnArg1].vx;
            coord->coord.t[1] = D_actor_341300_80165A58[arg0->spawnArg1].vy;
            coord->coord.t[2] = D_actor_341300_80165A58[arg0->spawnArg1].vz;
            if (arg0->spawnArg1 == 0) {
                work->vel.vx = ACTOR_341300_RAND() & 0x1F;
            } else {
                work->vel.vx = -(ACTOR_341300_RAND() & 0x1F);
            }
            work->vel.vy      = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & 0x1F) : -(ACTOR_341300_RAND() & 0x1F);
            work->vel.vz      = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & 0x1F) : -(ACTOR_341300_RAND() & 0x1F);
            work->rotSpeed.vx = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & 0x7F) : -(ACTOR_341300_RAND() & 0x7F);
            work->rotSpeed.vy = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & 0x7F) : -(ACTOR_341300_RAND() & 0x7F);
            work->rotSpeed.vz = (ACTOR_341300_RAND() & 1) ? (ACTOR_341300_RAND() & 0x7F) : -(ACTOR_341300_RAND() & 0x7F);
            if (work->rotSpeed.vx > 0) {
                work->rotSpeed.vx += 100;
            } else {
                work->rotSpeed.vx -= 100;
            }
            if (work->rotSpeed.vy > 0) {
                work->rotSpeed.vy += 100;
            } else {
                work->rotSpeed.vy -= 100;
            }
            if (work->rotSpeed.vz > 0) {
                work->rotSpeed.vz += 100;
            } else {
                work->rotSpeed.vz -= 100;
            }
            work->verts[0].vx = 0;
            work->verts[0].vy = (ACTOR_341300_RAND() & 1) ? 0x16 : 0x14;
            work->verts[0].vz = 0;
            v0                = rsin(0x2AA) * 20 / 4096;
            if (ACTOR_341300_RAND() & 1) {
                v0 += 2;
            }
            work->verts[1].vx = v0;
            v1                = -(rsin(0x155) * 20 / 4096);
            if (ACTOR_341300_RAND() & 1) {
                v1 -= 2;
            }
            work->verts[1].vy = v1;
            work->verts[1].vz = 0;
            v2                = -(rsin(0x2AA) * 20 / 4096);
            if (ACTOR_341300_RAND() & 1) {
                v2 -= 2;
            }
            work->verts[2].vx = v2;
            v3                = -(rsin(0x155) * 20 / 4096);
            if (ACTOR_341300_RAND() & 1) {
                v3 -= 2;
            }
            work->verts[2].vy = v3;
            work->verts[2].vz = 0;
            arg0->state++;
            break;
        case 1:
            if (coord->coord.t[1] > 0) {
            kill:
                taskKill(arg0);
                break;
            }
            work->vel.vy      += 8;
            coord->coord.t[0] += work->vel.vx;
            coord->coord.t[1] += work->vel.vy;
            coord->coord.t[2] += work->vel.vz;
            Gp_UpdateCoord(coord);
            gte_SetTransMatrix(&coord->workm);
            gte_SetRotMatrix(&coord->workm);
            for (i = 0; i < 3; i++) {
                gte_ldv0(&work->verts[i]);
                gte_rtps();
                gte_stsxy(&sxy);
                gte_stszotz(&otz);
                x[i] = sxy;
                y[i] = sxy >> 16;
            }
            prim           = (POLY_G3*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG3(prim);
            prim->r0 = prim->g0 = prim->b0 = 0x10;
            prim->r1 = prim->g1 = prim->b1 = 0x40;
            prim->r2 = prim->g2 = prim->b2 = 0x80;
            prim->x0                       = x[0];
            prim->y0                       = y[0];
            prim->x1                       = x[1];
            prim->y1                       = y[1];
            prim->x2                       = x[2];
            prim->y2                       = y[2];
            addPrim(&gGpuCurrentOt[1039], prim);
            work->rot.vx += work->rotSpeed.vx;
            work->rot.vy += work->rotSpeed.vy;
            work->rot.vz += work->rotSpeed.vz;
            Gfx_RotMatrixY(&coord->coord, work->rot.vy, 1);
            Gfx_RotMatrixX(&coord->coord, work->rot.vx, 0);
            Gfx_RotMatrixZ(&coord->coord, work->rot.vz, 0);
            coord->flg = 0;
            break;
    }
}

void func_actor_341300_8016398C(s32 arg0)
{
    if ((arg0 << 0x10) == 0) {
        D_actor_341300_80165A2C = Task_SpawnFromTable(&D_actor_341300_80165A68, 0, 0, 0);
    }
}

void func_actor_341300_801639CC(s32 arg0)
{
    if (((arg0 << 0x10) == 0) && (D_actor_341300_80165A2C != NULL)) {
        taskKill(D_actor_341300_80165A2C);
        D_actor_341300_80165A2C = NULL;
    }
}

void func_actor_341300_80163A10(Task* arg0)
{
    s16 i;
    s16 next;
    s32 tmp;
    s32 state;
    u16 count;

    if (arg0->state < 3) {
        if (arg0->state <= 0) {
            if (arg0->state == 0) {
                tmp = arg0->state;
                SOFT_TOUCH_REG(tmp);
                state               = tmp + 1;
                arg0->killCountdown = 0;
                goto store;
            }
        } else {
            count               = (u16)arg0->killCountdown + 1;
            arg0->killCountdown = count;
            i                   = 0;
            if ((s16)count >= 0x10) {
                do {
                    Task_SpawnFromTable(&D_actor_341300_80165A68, 1, 0, (s32)arg0);
                    Task_SpawnFromTable(&D_actor_341300_80165A68, 1, 1, (s32)arg0);
                    next = i + 1;
                    i    = next;
                } while (next < 0xA);
                arg0->killCountdown = 0;
                state               = arg0->state + 1;
            store:
                arg0->state = state;
            }
        }
    }
}
