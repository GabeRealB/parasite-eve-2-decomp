#include "common.h"

#include "actors/actor_403200.h"
#include "actors/actor_403200_view.h"
#include "actors/actors_shared_801433b8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include <psyq/inline_c.h>

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).
#define SCRATCH_SP (*(u32*)0x1F8003FC)

extern s16 D_actor_403200_80141C58;

/// Cleared by both halves of the launch state below; `actor_403200_5.c` exposes
/// it through the setter / getter pair `func_actor_403200_80141108` and
/// `func_actor_403200_80141114`.
extern s16 D_actor_403200_80141C5A;

/// Non-zero once the launch state has published the enemy's position to the
/// player, and cleared again when it restarts.
extern s8 D_actor_403200_8015F8E0;

/// Shared 0x7DA payload buffer, also used by the other states of this overlay.
extern Actor403200Msg7DA D_actor_403200_8015F8F4;

/// The halfword at `D_actor_403200_8015F8F4 + 2` under its own label: the
/// escort-spawn tick reaches the action selector both ways, so both names are
/// declared (see DECOMPILATION_LEARNINGS.md, "A second label on the same run").
extern s16 D_actor_403200_8015F8F6;

/// Pair descriptors the host and its escorts publish as `GpEnemy::param`;
/// `hpMax` is the hit-point pool each one starts with.
extern GpPairSrcE D_actor_403200_80141C00;
extern GpPairSrcE D_actor_403200_80141C20;
extern GpPairSrcE D_actor_403200_80141C30;
extern GpPairSrcE D_actor_403200_80141C40;

/// Animation-set tables: the host's two blocks, escort 0's two and escort 1's.
extern GpAnimSet* D_actor_403200_8015E484[];
extern GpAnimSet* D_actor_403200_8015E53C[];
extern GpAnimSet* D_actor_403200_8015E5F4[];
/// Spawn table of the seven escorts, indexed 0..6.
extern TaskDesc D_actor_403200_8015E72C;
/// The enemy task's message-handler table, parked in `Task::msgTable`.
extern void* D_actor_403200_8015F770;
/// The enemy task itself, published for the overlay's other files.
extern Task* D_actor_403200_8015F8F0;

void func_8010C980(GsCOORDINATE2* arg0, GpObj* arg1, GpRec18* arg2, s32 arg3, s32 arg4, s32 arg5);

/// World point the launch tick hands the player as message 0x3E9, built from
/// the host model's root coordinate.
extern VECTOR3 D_actor_403200_8015F9C0;

/// Non-zero while the overlay is shutting down, which is what makes the
/// state-selecting tick below hold `field_6` at zero and re-roll its sub-state.
extern s16 D_actor_403200_80141C50;

/// LCG state the state-selecting tick below rolls its sub-state out of.
extern s32 Gp_LcgState;

/// The script pair the death sequence's frame-0x1C cue spawns.
extern s32 D_actor_403200_80141C5C;
extern s32 D_actor_403200_80141C64;

extern s32                  D_actor_403200_80141C6C;
extern s32                  D_actor_403200_80141C74;
extern Actor403200DropCoord D_actor_403200_8015F970;

/// Enemy spawn table the three launch states of `func_actor_403200_8013D9EC`
/// draw from.
extern TaskDesc D_actor_403200_8015E858;

/// Three formations of nine positions, and each member's model/spawn argument.
extern SVECTOR             D_actor_403200_8015F7B0[3][9];
extern Actor403200SpawnRec D_actor_403200_8015F888[9];

/// The scratch coordinate the debris effect of `func_actor_403200_8013DC3C` is
/// built on: `F920` is the whole `GsCOORDINATE2` and `F924` its `coord` matrix,
/// which splat names separately because the code takes that address directly.
extern GsCOORDINATE2 D_actor_403200_8015F920;
extern MATRIX        D_actor_403200_8015F924;

/// Animation table the stand-up tick publishes to the player in its message
/// 0x3FF, the same role `D_actor_444000_80161670` has for the arena tick.
extern GpAnimSet* D_actor_403200_8015E6AC[];

/// Absolute gate the hit handlers share, read as the halfword array its readers
/// index: the byte at +0 is the mode flag the rest of the game writes, and the
/// halfword at +2 is the "player hold is armed" gate the group 3-5 and 6-8 hit
/// handlers test before they let a landed hit spawn its effect.
extern u16 D_801153F4[];

/// Gameplay's escort `TaskDesc` table; entry 3 is the pair this boss spawns.
extern TaskDesc D_80174D58;

/// Reply buffer the stand-up tick passes with its message 0x3F8 before it asks
/// the player for the hold. Same shape as `D_actor_444000_80161928`.
extern Actor403200Msg3F8 D_actor_403200_8015FA00;

/// This overlay's three task states -- spawn/setup, per-frame tick and
/// teardown -- dispatched through by state, the same shape as the sibling
/// enemy actors' tables.
extern GpEnemyTaskFuncTable3 D_actor_403200_801321B8;

/// Handwritten overlay-local follow helper. `arg1`/`arg2` select the axis pair
/// and `arg3` the mode; takes the task, not the work block.
void func_actor_403200_801408D8(Task* arg0, s16 arg1, s16 arg2, s16 arg3);

/// Overlay-local hit-effect spawner (`actor_403200_3.c`): picks a rotation from
/// the attack id's param 0, hands `func_800FDB18` the part's coordinate with the
/// effect id from param 1, and returns. Only the coordinate and the id are read;
/// the caller's `a2` / `a3` are left over from the scan.
void func_actor_403200_80134044(GsCOORDINATE2* coord, s32 id);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_80137CCC);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_80137EB4);

/// Screen-shake driver for the enemy task: `func_actor_403200_8013FB54` writes a
/// level into `field_EAC`, and a change from the armed level in `field_EAD`
/// starts a shake of 5, 10 or 22 frames -- any other level is ignored. Each tick
/// spends one frame and drives `Display_ClampField126` off the frame counter's
/// low bits, so level 1 alternates 0 / 2, level 2 walks a four-frame 0 / 2 / 3 / 2
/// pattern and level 3 an eight-frame ramp that peaks at 4. The shake clears
/// itself once the counter runs out. Same body as
/// `func_actor_444000_8013A77C`, plus the null test on the work block.
void func_actor_403200_80138284(Task* arg0)
{
    Actor403200Work* work;
    s32              phase;

    work = (Actor403200Work*)arg0->work;
    if (work == NULL) {
        return;
    }

    if (work->field_EAC != work->field_EAD) {
        switch (work->field_EAC) {
            case 1:
                work->field_EAE = 5;
                break;
            case 2:
                work->field_EAE = 0xA;
                break;
            case 3:
                work->field_EAE = 0x16;
                break;
            case 0:
            default:
                return;
        }
        work->field_EAD = work->field_EAC;
    }

    if (work->field_EAE == 0) {
        Display_ClampField126(0);
        work->field_EAC = 0;
        work->field_EAD = 0;
        return;
    }
    work->field_EAE--;

    switch (work->field_EAC) {
        case 1:
            phase = work->field_EAE;
            if ((phase & 1) == 0) {
                work->field_EAF = 0;
            } else {
                work->field_EAF = 2;
            }
            Display_ClampField126(work->field_EAF);
            break;

        case 2:
            phase = work->field_EAE;
            switch (phase & 3) {
                case 0:
                    work->field_EAF = 0;
                    break;
                case 1:
                    work->field_EAF = 2;
                    break;
                case 2:
                    work->field_EAF = 3;
                    break;
                case 3:
                    work->field_EAF = 2;
                    break;
            }
            Display_ClampField126(work->field_EAF);
            break;

        case 3:
            phase = work->field_EAE;
            switch (phase & 7) {
                case 3:
                case 4:
                    work->field_EAF = 4;
                    break;
                case 2:
                case 5:
                    work->field_EAF = 3;
                    break;
                case 1:
                case 6:
                    work->field_EAF = 1;
                    break;
                case 0:
                case 7:
                    work->field_EAF = 0;
                    break;
            }
            Display_ClampField126(work->field_EAF);
            break;

        case 0:
        default:
            Display_ClampField126(0);
            break;
    }
}

/// The escort-group reset the enemy runs whenever its state changes: it turns
/// the host model's flag word around and pushes it onto all seven escorts'
/// models, differing in what the word becomes and whether the model buffers are
/// (re)allocated first. `work->field_7F3` is cleared on every path, and the two
/// that end with the work block's state index reset are the ones that set the
/// word to 0x80.
///
/// Same body as `func_actor_444000_8013A958` without that sibling's
/// `TmdObject::buffer` buffer tests, so every escort is re-allocated
/// unconditionally.
s32 func_actor_403200_80138468(Task* task, s32 arg1, s32 arg2)
{
    Actor403200Work* work;
    Actor403200Work* buffers;
    Actor403200Work* escorts;
    Actor403200Work* rebuilt;
    s16              i;
    s16              j;

    work = (Actor403200Work*)task->work;
    switch (arg2) {
        case 0:
            buffers = (Actor403200Work*)task->work;
            Tmd_AllocBuffers((TmdObject*)task->extra);
            for (j = 0; j < 7; j++) {
                if (buffers->field_ECC[j] != NULL) {
                    Tmd_AllocBuffers((TmdObject*)buffers->field_ECC[j]->task->extra);
                }
            }
            escorts                          = (Actor403200Work*)task->work;
            escorts->field_7F3               = 0;
            ((TmdObject*)task->extra)->flags = 0x80;
            for (i = 0; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)task->extra)->flags;
                }
            }
            work->field_0 = 0;
            break;
        case 1:
            escorts                          = (Actor403200Work*)task->work;
            escorts->field_7F3               = 0;
            ((TmdObject*)task->extra)->flags = 0;
            for (i = 0; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)task->extra)->flags;
                }
            }
            rebuilt = (Actor403200Work*)task->work;
            Tmd_AllocBuffers((TmdObject*)task->extra);
            for (j = 0; j < 7; j++) {
                if (rebuilt->field_ECC[j] != NULL) {
                    Tmd_AllocBuffers((TmdObject*)rebuilt->field_ECC[j]->task->extra);
                }
            }
            break;
        case 2:
            work->field_7F3                  = 0;
            escorts                          = (Actor403200Work*)work;
            ((TmdObject*)task->extra)->flags = 0x80;
            for (i = 0; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)task->extra)->flags;
                }
            }
            work->field_0 = 0;
            break;
        case 3:
            i                                = 0;
            escorts                          = (Actor403200Work*)task->work;
            escorts->field_7F3               = 0;
            ((TmdObject*)task->extra)->flags = 0x80;
            for (; i < 7; i++) {
                if (escorts->field_ECC[i] != NULL) {
                    ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags = ((TmdObject*)task->extra)->flags;
                }
            }
            break;
    }
    return 0;
}

/// Handles message 0x7DB: records the payload and dispatches the sender's
/// action to reset the escorts, select an attack, or finish the return pose.
s32 func_actor_403200_80138748(Task* task, s32 msgId, Actor403200Msg7DB* msg)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* rebuilt;
    GpEnemy*         temp_enemy;
    s16              i;
    s16              j;
    s32              sound;
    s32              pan;
    s32              action;

    work       = (Actor403200Work*)task->work;
    temp_enemy = (GpEnemy*)task->spawnArg2;

    work->field_EC4 = msg->b[0];
    work->field_EC5 = msg->b[1];
    work->field_EC6 = msg->b[2];

    if (msg->h.id == 0x2704) {
        action = msg->h.action;
        switch (action) {
            case 0:
                work->field_0   = 0;
                work->field_E96 = 0xFA0;
                break;

            case 2:
                work->field_0   = 5;
                work->field_7B3 = 0x14;
                work->field_7B0 = 2;
                sound           = ((temp_enemy->placeKey >> 0xC) << 8) | 0x40200002;
                pan             = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
                SndEvt_EnqueueType6(sound, pan,
                                    (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
                break;

            case 3:
                work->field_0   = 5;
                work->field_2   = -1;
                work->field_7B3 = 0xD;
                work->field_7B0 = 1;
                break;

            case 5:
                work->field_0                    = 0xA;
                escorts                          = (Actor403200Work*)task->work;
                escorts->field_7F3               = 0;
                ((TmdObject*)task->extra)->flags = 0;
                for (i = 0; i < 7; i++) {
                    if (escorts->field_ECC[i] != NULL) {
                        ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags =
                            ((TmdObject*)task->extra)->flags;
                    }
                }
                rebuilt = (Actor403200Work*)task->work;
                Tmd_AllocBuffers((TmdObject*)task->extra);
                for (j = 0; j < 7; j++) {
                    if (rebuilt->field_ECC[j] != NULL) {
                        Tmd_AllocBuffers((TmdObject*)rebuilt->field_ECC[j]->task->extra);
                    }
                }
                work->field_F06 = 0;
                work->field_F04 = 0;
                break;

            case 10:
                work->field_0   = 0xC;
                work->field_2   = -1;
                work->field_7B3 = 0x12;
                work->field_7B0 = 2;
                work->field_F06 = 7;
                work->field_F14 = 0;
                work->field_F04 = 1;
                break;

            case 11:
                work->field_0   = 0xC;
                work->field_2   = -1;
                work->field_7B3 = 0x12;
                work->field_F14 = 0x96;
                work->field_7B0 = 2;
                work->field_F04 = 1;
                break;

            case 12:
                work->field_F06 = 7;
                work->field_F04 = 0;
                work->field_0   = 1;

            case 19:
                work->field_0           = 0xC;
                work->field_F06         = 4;
                work->field_2           = -1;
                work->field_7B3         = 0x12;
                work->field_F14         = 0x258;
                work->field_7B0         = 2;
                work->field_F04         = 0;
                D_actor_403200_80141C58 = 0x640;
                break;
        }
    }

    if (msg->h.id == 0x2804) {
        switch (msg->h.action) {
            case 0:
                work->field_0 = 0;
                break;

            case 1:
                work->field_7B3 = 0xA;
                work->field_7B0 = 2;
                work->field_7B6 = 0x7F;
                func_actor_403200_80133DD8(task);
                while (work->field_58 & 1) {
                    func_actor_403200_80133DD8(task);
                }
                work->field_7B6                               = 0x10;
                ((TmdObject*)task->extra)->coords->coord.t[0] = -0xBB8;
                ((TmdObject*)task->extra)->coords->coord.t[1] = 0;
                ((TmdObject*)task->extra)->coords->coord.t[2] = -0x992;
                ((TmdObject*)task->extra)->coords->flg        = 0;
                work->field_0                                 = 6;
                break;
        }
    }
    return 1;
}

/// Complete layout of the 0xF24 work block for the spawn state. `Actor403200Work`
/// still names the fields later ticks use; this view adds the animation blocks
/// and collision extras that only the spawn writes.
typedef struct Actor403200InitWork {
    s16                  field_0;
    s16                  field_2;
    s16                  field_4;
    s16                  field_6;
    byte                 pad_8[4];
    GpAnimCtx            anim0;
    GpAnimSlot           slots0[8];
    byte                 aux0[0x80];
    GpAnimCtx            anim1;
    GpAnimSlot           slots1[8];
    byte                 aux1[0x80];
    GpAnimCtx            anim2;
    GpAnimSlot           slots2[4];
    byte                 aux2[0x40];
    GpAnimCtx            anim3;
    GpAnimSlot           slots3[4];
    byte                 aux3[0x40];
    GpAnimCtx            anim4;
    GpAnimSlot           slots4[4];
    byte                 aux4[0x40];
    GpAnimCtx            anim5;
    GpAnimSlot           slots5[4];
    byte                 aux5[0x40];
    byte                 pad_784[0x2C];
    s8                   field_7B0;
    s8                   field_7B1;
    byte                 pad_7B2;
    s8                   field_7B3;
    byte                 pad_7B4[2];
    s16                  field_7B6;
    s16                  field_7B8;
    byte                 pad_7BA[0xA];
    s16                  field_7C4;
    byte                 pad_7C6[2];
    s16                  field_7C8;
    byte                 pad_7CA[0x28];
    s8                   field_7F2;
    u8                   field_7F3;
    Actor403200HitGroup  hits[9];
    GpObj                obj;
    GpActorD4Rec         d4rec;
    GpRec18              recs2[5];
    MATRIX               lightMtx;
    MATRIX               colorMtx;
    Actor403200DropCoord field_E3C;
    byte                 pad_E8C[8];
    s16                  field_E94;
    s16                  field_E96;
    s16                  field_E98;
    byte                 pad_E9A[0x16];
    GpAnimArg            field_EB0;
    byte                 pad_EC4[8];
    GpEnemy*             field_ECC[7];
    byte                 pad_EE8[0x10];
    s16                  field_EF8;
    byte                 pad_EFA[0xA];
    s16                  field_F04;
    s16                  field_F06;
    s16                  field_F08;
    u16                  field_F0A;
    u16                  field_F0C;
    u16                  field_F0E;
    s16                  field_F10;
    s16                  field_F12;
    byte                 pad_F14[7];
    s8                   field_F1B;
    s8                   field_F1C;
    byte                 pad_F1D[7];
} Actor403200InitWork;
STATIC_ASSERT_SIZEOF(Actor403200InitWork, 0xF24);

static __inline__ void Actor403200_SeedRootCoord(Task* task, Actor403200InitWork* work)
{
    GsCOORDINATE2*         coord = ((TmdObject*)task->extra)->coords;
    Actor403200RotScratch* sc;
    s16                    ang;

    sc                                       = (Actor403200RotScratch*)(*(u8**)G_SCRATCH_HEAD - sizeof(Actor403200RotScratch));
    *(Actor403200RotScratch**)G_SCRATCH_HEAD = sc;

    ang       = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle = ang;
    Gfx_RotMatrixY(&sc->m, ang, 1);
    sc->scale.vx = sc->scale.vy = sc->scale.vz = 0x1000;
    ScaleMatrix(&sc->m, &sc->scale);

    coord->coord.m[0][0] = sc->m.m[0][0];
    coord->coord.m[0][1] = sc->m.m[0][1];
    coord->coord.m[0][2] = sc->m.m[0][2];
    coord->coord.m[1][0] = sc->m.m[1][0];
    coord->coord.m[1][1] = sc->m.m[1][1];
    coord->coord.m[1][2] = sc->m.m[1][2];
    coord->coord.m[2][0] = sc->m.m[2][0];
    coord->coord.m[2][1] = sc->m.m[2][1];
    coord->coord.m[2][2] = sc->m.m[2][2];
    coord->flg           = 0;

    work->field_0                    = 0;
    ((TmdObject*)task->extra)->flags = 0;
    *(u8**)G_SCRATCH_HEAD            = *(u8**)G_SCRATCH_HEAD + sizeof(Actor403200RotScratch);
}

/// Spawn state of the arena boss: allocate its work block, wire the host enemy
/// up to the model's root coordinate and its nine collision objects, then spawn
/// the seven escorts that make up the rest of the creature.
void func_actor_403200_80138AFC(GpEnemy* enemy, Task* task)
{
    Actor403200InitWork* work;
    Actor403200InitWork* buffers;
    Actor403200InitWork* escorts;
    Actor403200Matrix*   mtx;
    TmdObject*           tmd;
    GsCOORDINATE2*       coord;
    GsCOORDINATE2*       freeCoord;
    GpEnemy*             esc;
    Task*                escTask;
    GpRec18*             recs2;
    SVECTOR              dir;
    SVECTOR*             gteDir;
    VECTOR               pos;
    s16                  i;
    s16                  j;

    tmd   = (TmdObject*)task->extra;
    coord = tmd->coords;

    work       = memCalloc(0xF24, 0);
    task->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    task->exitCallback = ActorsShared801433b8;

    enemy->field_4    = &((TmdObject*)task->extra)->coords->coord;
    enemy->field_48   = 0;
    enemy->bodyPos.vx = 0;
    enemy->bodyPos.vy = -0xC8;
    enemy->bodyPos.vz = 0;
    enemy->coord      = &((TmdObject*)task->extra)->coords[4];
    Gp_LinkNode(&enemy->node);
    enemy->reactionFlags = 0;
    enemy->hp            = D_actor_403200_80141C00.hpMax;
    enemy->param         = &D_actor_403200_80141C00;
    enemy->recs          = work->hits[0].recs;

    func_800B3F84(&work->anim0, D_actor_403200_8015E484, tmd, work->aux0, work->slots0);
    func_800B3F84(&work->anim1, D_actor_403200_8015E484, tmd, work->aux1, work->slots1);

    work->field_7B0 = 2;
    work->field_7B3 = 2;
    work->field_EF8 = 1;
    work->field_7B1 = 0;
    work->field_7C4 = work->field_7C8 = 0;
    work->field_7B6 = work->field_7B8 = 0x10;

    func_8010C980(&((TmdObject*)task->extra)->coords[4], &work->hits[1].obj, work->hits[1].recs, 5, 0x20, 0x300);
    func_8010C980(&((TmdObject*)task->extra)->coords[4], &work->hits[0].obj, work->hits[0].recs, 5, 0x20, 0x300);
    func_8010C980(&((TmdObject*)task->extra)->coords[1], &work->hits[2].obj, work->hits[2].recs, 5, 0x20, 0xBB8);

    work->hits[1].obj.pos.vz = -0x100;
    work->hits[2].obj.pos.vy = 0x400;
    work->hits[1].obj.pos.vx = 0;
    work->hits[1].obj.pos.vy = 0;
    work->hits[2].obj.pos.vx = 0;
    work->hits[2].obj.pos.vz = -0x400;

    Gfx_MatrixCol2(&((TmdObject*)task->extra)->coords->coord, &dir);
    dir.vy = 0;
    gteDir = &dir;
    VectorNormalSS(gteDir, gteDir);
    gte_lddp(0x1388);
    gte_ldsv(gteDir);
    gte_gpf12_real();
    gte_stsv(gteDir);

    work->field_EB0.field_0  = NULL;
    work->field_EB0.field_4  = 1;
    work->field_EB0.field_8  = 0;
    work->field_EB0.field_C  = 3;
    work->field_EB0.field_10 = 1;
    work->field_F12          = 0;
    task->msgTable           = &D_actor_403200_8015F770;
    coord->sub               = &gGfxViewCoord;
    coord->flg               = 0;
    Gp_UpdateCoord(coord);

    work->field_2 = -1;
    buffers       = task->work;
    Tmd_AllocBuffers((TmdObject*)task->extra);
    for (i = 0; i < 7; i++) {
        if (buffers->field_ECC[i] != NULL) {
            Tmd_AllocBuffers((TmdObject*)buffers->field_ECC[i]->task->extra);
        }
    }

    Actor403200_SeedRootCoord(task, work);

    esc                                                               = Gp_SpawnEnemyFromTable(&D_actor_403200_8015E72C, 0, 0, task->spawnArg2);
    work->field_ECC[0]                                                = esc;
    ((TmdObject*)esc->task->extra)->coords->sub                       = ((TmdObject*)task->extra)->coords;
    ((TmdObject*)work->field_ECC[0]->task->extra)->coords->coord.t[0] = 0;
    ((TmdObject*)work->field_ECC[0]->task->extra)->coords->coord.t[1] = 0;
    ((TmdObject*)work->field_ECC[0]->task->extra)->coords->coord.t[2] = 0;
    ((TmdObject*)work->field_ECC[0]->task->extra)->flags              = 0;
    func_800B3F84(&work->anim2, D_actor_403200_8015E53C, work->field_ECC[0]->task->extra, work->aux2,
                  work->slots2);
    func_800B3F84(&work->anim3, D_actor_403200_8015E53C, work->field_ECC[0]->task->extra, work->aux3,
                  work->slots3);
    work->field_ECC[0]->field_4    = &((TmdObject*)task->extra)->coords->coord;
    work->field_ECC[0]->field_48   = 0;
    work->field_ECC[0]->bodyPos.vx = 0xC8;
    work->field_ECC[0]->bodyPos.vy = 0;
    work->field_ECC[0]->bodyPos.vz = 0x3E8;
    work->field_ECC[0]->coord      = &((TmdObject*)work->field_ECC[0]->task->extra)->coords[1];
    Gp_LinkNode(&work->field_ECC[0]->node);
    work->field_ECC[0]->reactionFlags = 0;
    work->field_ECC[0]->hp            = D_actor_403200_80141C00.hpMax;
    work->field_F0A                   = D_actor_403200_80141C20.hpMax;
    work->field_ECC[0]->param         = &D_actor_403200_80141C20;
    work->field_ECC[0]->recs          = work->hits[3].recs;
    func_8010C980(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1], &work->hits[3].obj, work->hits[3].recs, 5,
                  0x20, 0x300);
    func_8010C980(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[2], &work->hits[4].obj, work->hits[4].recs, 5,
                  0x20, 0x300);
    func_8010C980(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[3], &work->hits[5].obj, work->hits[5].recs, 5,
                  0x20, 0x300);

    esc                                                               = Gp_SpawnEnemyFromTable(&D_actor_403200_8015E72C, 1, 0, task->spawnArg2);
    work->field_ECC[1]                                                = esc;
    ((TmdObject*)esc->task->extra)->coords->sub                       = ((TmdObject*)task->extra)->coords;
    ((TmdObject*)work->field_ECC[1]->task->extra)->coords->coord.t[0] = 0;
    ((TmdObject*)work->field_ECC[1]->task->extra)->coords->coord.t[1] = 0;
    ((TmdObject*)work->field_ECC[1]->task->extra)->coords->coord.t[2] = 0;
    ((TmdObject*)work->field_ECC[1]->task->extra)->flags              = 0;
    func_800B3F84(&work->anim4, D_actor_403200_8015E5F4, work->field_ECC[1]->task->extra, work->aux4,
                  work->slots4);
    func_800B3F84(&work->anim5, D_actor_403200_8015E5F4, work->field_ECC[1]->task->extra, work->aux5,
                  work->slots5);
    work->field_ECC[1]->field_4    = &((TmdObject*)task->extra)->coords->coord;
    work->field_ECC[1]->field_48   = 0;
    work->field_ECC[1]->bodyPos.vx = -0xC8;
    work->field_ECC[1]->bodyPos.vy = 0;
    work->field_ECC[1]->bodyPos.vz = 0x3E8;
    work->field_ECC[1]->coord      = &((TmdObject*)work->field_ECC[1]->task->extra)->coords[1];
    Gp_LinkNode(&work->field_ECC[1]->node);
    work->field_ECC[1]->reactionFlags = 0;
    work->field_ECC[1]->hp            = D_actor_403200_80141C00.hpMax;
    work->field_F0C                   = D_actor_403200_80141C30.hpMax;
    work->field_ECC[1]->param         = &D_actor_403200_80141C30;
    work->field_ECC[1]->recs          = work->hits[6].recs;
    func_8010C980(&((TmdObject*)work->field_ECC[1]->task->extra)->coords[1], &work->hits[6].obj, work->hits[6].recs, 5,
                  0x20, 0x300);
    func_8010C980(&((TmdObject*)work->field_ECC[1]->task->extra)->coords[2], &work->hits[7].obj, work->hits[7].recs, 5,
                  0x20, 0x300);
    func_8010C980(&((TmdObject*)work->field_ECC[1]->task->extra)->coords[3], &work->hits[8].obj, work->hits[8].recs, 5,
                  0x20, 0x300);

    esc                = Gp_SpawnEnemyFromTable(&D_actor_403200_8015E72C, 2, 0, task->spawnArg2);
    work->field_ECC[2] = esc;
    if (esc != NULL) {
        ((TmdObject*)esc->task->extra)->coords->sub                       = &((TmdObject*)task->extra)->coords[4];
        ((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord.t[0] = 0;
        ((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord.t[1] = 0x59;
        ((TmdObject*)work->field_ECC[2]->task->extra)->coords->coord.t[2] = -0x64;
        ((TmdObject*)work->field_ECC[2]->task->extra)->flags              = 0;
    }

    esc                = Gp_SpawnEnemyFromTable(&D_actor_403200_8015E72C, 3, 0, task->spawnArg2);
    work->field_ECC[3] = esc;
    if (esc != NULL) {
        ((TmdObject*)esc->task->extra)->coords->sub                       = &((TmdObject*)task->extra)->coords[3];
        ((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord.t[0] = 0;
        ((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord.t[1] = 0;
        ((TmdObject*)work->field_ECC[3]->task->extra)->coords->coord.t[2] = 0;
        ((TmdObject*)work->field_ECC[3]->task->extra)->flags              = 0;
        work->field_ECC[3]->field_4                                       = &((TmdObject*)task->extra)->coords->coord;
        work->field_ECC[3]->field_48                                      = 0;
        work->field_ECC[3]->bodyPos.vx                                    = 0;
        work->field_ECC[3]->bodyPos.vy                                    = 0x1F4;
        work->field_ECC[3]->bodyPos.vz                                    = 0x384;
        work->field_ECC[3]->coord                                         = ((TmdObject*)work->field_ECC[3]->task->extra)->coords;
        Gp_LinkNode(&work->field_ECC[3]->node);
        work->field_ECC[3]->reactionFlags = 0;
        work->field_ECC[3]->hp            = D_actor_403200_80141C00.hpMax;
        work->field_F0E                   = D_actor_403200_80141C40.hpMax;
        work->field_ECC[3]->param         = &D_actor_403200_80141C40;
        work->field_ECC[3]->recs          = work->hits[1].recs;
    }

    esc                = Gp_SpawnEnemyFromTable(&D_actor_403200_8015E72C, 4, 0, task->spawnArg2);
    work->field_ECC[4] = esc;
    if (esc != NULL) {
        ((TmdObject*)esc->task->extra)->coords->sub                       = &((TmdObject*)task->extra)->coords[4];
        ((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord.t[0] = 0;
        ((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord.t[1] = 0;
        ((TmdObject*)work->field_ECC[4]->task->extra)->coords->coord.t[2] = 0x14;
        ((TmdObject*)work->field_ECC[4]->task->extra)->flags              = 0;
    }

    esc                = Gp_SpawnEnemyFromTable(&D_actor_403200_8015E72C, 5, 0, task->spawnArg2);
    work->field_ECC[5] = esc;
    if (esc != NULL) {
        ((TmdObject*)esc->task->extra)->coords->sub                       = &((TmdObject*)task->extra)->coords[2];
        ((TmdObject*)work->field_ECC[5]->task->extra)->coords->coord.t[0] = 0;
        ((TmdObject*)work->field_ECC[5]->task->extra)->coords->coord.t[1] = 0x67C;
        ((TmdObject*)work->field_ECC[5]->task->extra)->coords->coord.t[2] = 0xC8;
        ((TmdObject*)work->field_ECC[5]->task->extra)->flags              = 0;
    }

    esc                = Gp_SpawnEnemyFromTable(&D_actor_403200_8015E72C, 6, 0, task->spawnArg2);
    work->field_ECC[6] = esc;
    if (esc != NULL) {
        ((TmdObject*)esc->task->extra)->coords->sub                       = &((TmdObject*)task->extra)->coords[1];
        ((TmdObject*)work->field_ECC[6]->task->extra)->coords->coord.t[0] = 0;
        ((TmdObject*)work->field_ECC[6]->task->extra)->coords->coord.t[1] = 0x62C;
        ((TmdObject*)work->field_ECC[6]->task->extra)->coords->coord.t[2] = 0x5DC;
        ((TmdObject*)work->field_ECC[6]->task->extra)->flags              = 0;
    }

    work->field_F0C    = 0x3C;
    freeCoord          = &work->field_E3C.c;
    work->field_ECC[6] = NULL;
    work->field_F04    = 0;
    work->field_F06    = 0;
    work->field_F08    = 0;
    work->field_F0A    = 0x32;
    work->field_7F2    = 0;

    work->field_E3C.c.sub         = ((TmdObject*)task->extra)->coords;
    work->field_E3C.ident.m00_m01 = 0x1000;
    mtx                           = (Actor403200Matrix*)&work->field_E3C.c.coord;
    mtx->ident.m02_m10            = 0;
    mtx->ident.m11_m12            = 0x1000;
    mtx->ident.m20_m21            = 0;
    mtx->ident.m22                = 0x1000;
    work->field_E3C.c.coord.t[0] = work->field_E3C.c.coord.t[1] = work->field_E3C.c.coord.t[2] = 0;
    work->field_E3C.c.flg                                                                      = 0;
    Gp_UpdateCoord(freeCoord);

    work->d4rec.end1.vz    = 0x1B58;
    recs2                  = work->recs2;
    work->d4rec.end0Radius = 0x258;
    work->d4rec.end1Radius = 0x258;
    work->d4rec.end0.vx    = 0;
    work->d4rec.end0.vy    = 0;
    work->d4rec.end0.vz    = 0;
    work->d4rec.end1.vx    = 0;
    work->d4rec.end1.vy    = 0;
    work->d4rec.recs       = recs2;
    work->obj.coord        = freeCoord;
    work->obj.ctx.d4rec    = &work->d4rec;
    work->obj.pos.vx       = 0;
    work->obj.pos.vy       = -0xFA;
    work->obj.pos.vz       = 0x25F;
    work->obj.key          = 0x30000 | 0x20;
    work->obj.radius       = 0;
    work->obj.flags        = 3;
    Gp_LinkObj(2, &work->obj);
    Gp_InitRec18Table(recs2, 5, 0);
    work->obj.flags &= 0x7FFF;

    escorts                             = task->work;
    ((TmdObject*)task->extra)->lightMtx = &escorts->lightMtx;
    ((TmdObject*)task->extra)->colorMtx = &escorts->colorMtx;
    for (j = 0; j < 7; j++) {
        esc = escorts->field_ECC[j];
        if (esc != NULL) {
            escTask                                = esc->task;
            ((TmdObject*)escTask->extra)->lightMtx = &escorts->lightMtx;
            ((TmdObject*)escTask->extra)->colorMtx = &escorts->colorMtx;
        }
    }

    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    func_actor_403200_80133DD8(task);

    D_actor_403200_8015F8F4.field_0 = 0;
    D_actor_403200_8015F8F4.field_1 = 0x2C;
    D_actor_403200_8015F8F4.field_2 = 0;
    Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&D_actor_403200_8015F8F4, 0x7DB);

    work->field_E94 = work->field_E96 = 0x9C4;
    work->field_E98                   = 0x190;
    D_actor_403200_8015F8F0           = task;
    work->field_F1B = work->field_F1C = 0;
    task->state                      += 1;
}

/// The group-0 hit handler: takes at most one hit this frame and turns it into
/// damage.
///
/// It carves a 0x30-byte `Actor403200HitScratch` off the scratchpad stack and
/// scans the five `GpRec18` records of `hits[0]` for the first whose `key`
/// high halfword is attack kind 2 -- the contact point goes into the frame's
/// `pos` and the id is kept. A record with `key` 0 ends the scan with no
/// hit. The scan is written with labels rather than a `for` so `loop.c` parks
/// the match arm out of line; the same shape as
/// `func_actor_444000_8013C060`'s.
///
/// A hit spawns the impact effect on the part's coordinate, publishes
/// `Gp_GetIdParam2` of the attack id to all four per-group slots at 0xE8C, and
/// then takes the damage off the host: the player-relative offset to the part
/// gives the range `Gp_ComputeDamage` scales `damage` by, quadrupled when
/// `Gp_RollEnemyChance` fires. The contact point is re-read relative to the
/// part's world translation and `ratan2` of the pair against the part's facing
/// gives the yaw `angle`, wrapped to +/-0x800. The damage is doubled, applied
/// through `func_800E2C78` and `func_800DA6E8`, and the host's remaining HP is
/// mirrored onto the three escorts sharing its pool.
///
/// The second arm runs the same tick on the `field_4C` bits 0x2/0x8 hit, which
/// `Gp_TickObjFlag4` turns into damage of its own; that one only comes off the
/// host.
///
/// `esc3` / `esc0` / `esc1` and the `hp` load are not spare: read as three
/// separate assignments the loaded pointers all share one register, and the
/// stores then interleave with their loads (the scheduler cannot hoist a load
/// past a store through an unknown pointer). Evaluating the three addresses
/// first is what puts them in `a0` / `a1` / `v1`, and the `hp` load has to sit
/// between the escort 3 and escort 0 ones to land where the original has it.
void func_actor_403200_80139A60(Task* arg0)
{
    Actor403200HitScratch* sc;
    Actor403200Work*       work;
    GpEnemy*               enemy;
    GpRec18*               recs;
    PlayerStatus*          cfg;
    SVECTOR*               pos;
    s32                    mask;
    s32                    kind;
    s32                    id;
    s32                    dx2;
    s32                    dy2;
    s32                    dz2;
    s16                    angle;
    s16                    i;
    s16                    param;
    u16                    hp;
    GpEnemy*               esc3;
    GpEnemy*               esc0;
    GpEnemy*               esc1;

    cfg   = &Player_Status;
    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor403200Work*)arg0->work;
    sc    = (Actor403200HitScratch*)(SCRATCH_SP -= sizeof(Actor403200HitScratch));
    pos   = &sc->pos;
    recs  = work->hits[0].recs;
    i     = 0;
    mask  = 0xFFFF0000;
    kind  = 0x20000;
scan:
    if (recs[i].key == 0) {
        goto missed;
    }
    if ((recs[i].key & mask) == kind) {
        pos->vx = recs[i].point.vx;
        pos->vy = recs[i].point.vy;
        pos->vz = recs[i].point.vz;
        id      = recs[i].key;
        goto found;
    }
    i++;
    if (i < 5) {
        goto scan;
    }
missed:
    id = 0;
found:
    sc->id = id;

    if (id != 0) {
        func_actor_403200_80134044(work->hits[0].obj.coord, id);
        param           = Gp_GetIdParam2(sc->id);
        work->field_E90 = param;
        work->field_E8E = param;
        work->field_E8C = param;
        work->field_E92 = param;
        Gp_GetIdParam0(sc->id);

        sc->delta.vx = cfg->coordMtx->t[0] - ((TmdObject*)arg0->extra)->coords->coord.t[0];
        dx2          = sc->delta.vx * sc->delta.vx;
        sc->delta.vy = cfg->coordMtx->t[1] - ((TmdObject*)arg0->extra)->coords->coord.t[1];
        dy2          = sc->delta.vy * sc->delta.vy;
        sc->delta.vz = cfg->coordMtx->t[2] - ((TmdObject*)arg0->extra)->coords->coord.t[2];
        dz2          = sc->delta.vz * sc->delta.vz;
        sc->dist     = SquareRoot0(dx2 + dy2 + dz2);
        sc->damage   = Gp_ComputeDamage(sc->id, sc->dist, 0, 0);
        if (Gp_RollEnemyChance(enemy, sc->id, 0) != 0) {
            sc->damage *= 4;
        }
        if (sc->damage != 0) {
            sc->rot.vy = 0x320;
            sc->rot.vx = 0;
            sc->rot.vz = 0x3E8;
            Gp_SpawnEff(0x6009C, &((TmdObject*)enemy->task->extra)->coords[3], 3, &sc->rot);
        }
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
        sc->rot.vx = sc->pos.vx - ((TmdObject*)arg0->extra)->coords->workm.t[0];
        sc->rot.vy = sc->pos.vy - ((TmdObject*)arg0->extra)->coords->workm.t[1];
        sc->rot.vz = sc->pos.vz - ((TmdObject*)arg0->extra)->coords->workm.t[2];
        angle      = ratan2(sc->rot.vx, sc->rot.vz) -
                ratan2(-((TmdObject*)arg0->extra)->coords->workm.m[2][0],
                       ((TmdObject*)arg0->extra)->coords->workm.m[2][2]);
        sc->angle = angle;
        if (angle < 0) {
        wrapUp:
            if (angle < -0x800) {
                angle += 0x1000;
                goto wrapUp;
            }
        } else {
        wrapDown:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto wrapDown;
            }
        }
        sc->angle = angle;

        work->field_7C8 = 0;
        work->field_7C4 = 0;
        sc->damage     *= 2;
        func_800E2C78((GpObj40*)enemy, sc->id, sc->damage, 0);
        enemy->hp -= sc->damage;
        func_800DA6E8(&enemy->node, sc->damage, 0);
        esc3     = work->field_ECC[3];
        hp       = enemy->hp;
        esc0     = work->field_ECC[0];
        esc1     = work->field_ECC[1];
        esc3->hp = hp;
        esc1->hp = hp;
        esc0->hp = hp;
    }

    if (enemy->reactionFlags & 0xC) {
        sc->damage = Gp_TickObjFlag4((GpObj5C*)enemy);
        if (Gp_ObjFlag4Expired((GpObj5C*)enemy) != 0) {
            enemy->reactionFlags &= 0xF3;
        }
        if (sc->damage != 0) {
            func_800E2C78((GpObj40*)enemy, sc->id, sc->damage, 0);
            enemy->hp -= sc->damage;
        }
    }

    SCRATCH_SP += sizeof(Actor403200HitScratch);
}

/// The hit handler for collision groups 1 and 2 -- the same scan
/// `func_actor_403200_80139A60` runs for group 0, done twice: group 1 first, and
/// group 2 only if nothing landed on group 1. The second scan carries its own
/// `recs2` / `pos2` / `i2`, because sharing `recs` / `pos` / `i` with the first
/// gives both loops one pseudo each and the wrong registers. Both scans are
/// written as real `for` loops rather than the group-0 handler's labels so
/// `find_and_verify_loops` parks the match arm out of line.
///
/// A hit spawns the impact effect on the part's coordinate, publishes
/// `Gp_GetIdParam2` of the attack id to all four per-group slots at 0xE8C and
/// then takes the damage off the host: the player-relative offset to the part
/// gives the range `Gp_ComputeDamage` scales `damage` by, quadrupled when
/// `Gp_RollEnemyChance` fires, and zeroed unless the attack kind came back 2.
/// The damage also comes off the work block's `field_F0E` pool and the host's
/// remaining HP is mirrored onto the three escorts sharing its pool.
/// `sc->angle` is the yaw of the contact point relative to the fourth escort's
/// facing, wrapped to +/-0x800.
///
/// The attack kind drives a sub-state change: kinds 4 and 6 roll `Gp_LcgState`
/// and take the boss out of state 3 into 8 one time in six, kind 2 does it
/// outright, and both are gated on the `field_F1C` re-arm countdown.
///
/// `esc3` / `esc0` / `esc1` and the `hp` load are not spare: read as three
/// separate assignments the loaded pointers all share one register, and the
/// stores then interleave with their loads. Evaluating the three addresses
/// first is what puts them in `a0` / `a1` / `v1`, and the `hp` load has to sit
/// between the escort 3 and escort 0 ones to land where the original has it.
///
/// The `do` / `while (0)` around the angle wrap is load-bearing, not stylistic.
/// Its body sits at loop depth 1, so `flow.c`'s `REG_N_REFS (regno) +=
/// loop_depth` gives `sc` one reference more than the unwrapped form (37
/// against 36, `work` sitting at 37 on a longer live range). That is what ranks
/// `sc` above `work` in global-alloc and puts it in `$s1`; unwrapped the two
/// exchange registers and the function stops at 99.06%.
void func_actor_403200_80139E94(Task* arg0)
{
    Actor403200HitScratch* sc;
    Actor403200Work*       work;
    GpEnemy*               host;
    PlayerStatus*          cfg;
    GsCOORDINATE2*         coord;
    GpRec18*               recs;
    GpRec18*               recs2;
    SVECTOR*               pos;
    SVECTOR*               pos2;
    s32                    id;
    s32                    dx2;
    s32                    dy2;
    s32                    dz2;
    s16                    angle;
    s16                    state;
    s16                    i;
    s16                    i2;
    s16                    param;
    u16                    roll;
    u16                    hp;
    GpEnemy*               esc3;
    GpEnemy*               esc0;
    GpEnemy*               esc1;

    cfg  = &Player_Status;
    host = (GpEnemy*)arg0->spawnArg2;
    work = (Actor403200Work*)arg0->work;
    sc   = (Actor403200HitScratch*)(SCRATCH_SP -= sizeof(Actor403200HitScratch));
    pos  = &sc->pos;
    recs = work->hits[1].recs;
    for (i = 0; i < 5; i++) {
        if (recs[i].key == 0) {
            goto missed1;
        }
        if ((recs[i].key & 0xFFFF0000) == 0x20000) {
            pos->vx = recs[i].point.vx;
            pos->vy = recs[i].point.vy;
            pos->vz = recs[i].point.vz;
            id      = recs[i].key;
            goto found1;
        }
    }
missed1:
    id = 0;
found1:
    sc->id = id;
    if (id != 0) {
        coord = work->hits[1].obj.coord;
        goto hit;
    }

    pos2  = &sc->pos;
    recs2 = work->hits[2].recs;
    for (i2 = 0; i2 < 5; i2++) {
        if (recs2[i2].key == 0) {
            goto missed2;
        }
        if ((recs2[i2].key & 0xFFFF0000) == 0x20000) {
            pos2->vx = recs2[i2].point.vx;
            pos2->vy = recs2[i2].point.vy;
            pos2->vz = recs2[i2].point.vz;
            id       = recs2[i2].key;
            goto found2;
        }
    }
missed2:
    id = 0;
found2:
    sc->id = id;
    if (id == 0) {
        goto out;
    }
    coord = work->hits[2].obj.coord;
hit:
    func_actor_403200_80134044(coord, id);
    if (sc->id != 0) {
        param           = Gp_GetIdParam2(sc->id);
        work->field_E90 = param;
        work->field_E8E = param;
        work->field_E8C = param;
        work->field_E92 = param;
        switch (Gp_GetIdParam0(sc->id) & 0xFFFF) {
            case 0:
            case 1:
            case 3:
            case 5:
            case 7:
            case 8:
            case 9:
                break;

            case 4:
            case 6:
                state = work->field_0;
                if (state != 3) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    roll        = ((u32)Gp_LcgState >> 16) % 6;
                    if (roll == 0 && work->field_F1C == 0) {
                        work->field_0 = 8;
                        work->field_2 = -1;
                    }
                }
                break;

            case 2:
                if (work->field_F1C == 0 && (state = work->field_0, state != 3)) {
                    work->field_0 = 8;
                    work->field_2 = -1;
                }
                break;
        }

        sc->delta.vx = cfg->coordMtx->t[0] - ((TmdObject*)arg0->extra)->coords->coord.t[0];
        dx2          = sc->delta.vx * sc->delta.vx;
        sc->delta.vy = cfg->coordMtx->t[1] - ((TmdObject*)arg0->extra)->coords->coord.t[1];
        dy2          = sc->delta.vy * sc->delta.vy;
        sc->delta.vz = cfg->coordMtx->t[2] - ((TmdObject*)arg0->extra)->coords->coord.t[2];
        dz2          = sc->delta.vz * sc->delta.vz;
        sc->dist     = SquareRoot0(dx2 + dy2 + dz2);
        sc->damage   = Gp_ComputeDamage(sc->id, sc->dist, 0, 0);

        if (Gp_RollEnemyChance(work->field_ECC[3], sc->id, 0) != 0 && (state = work->field_0, state != 0xD) && state != 3 &&
            state != 9 && state != 0xE && state != 0xF) {
            sc->rot.vy = 0;
            sc->rot.vx = 0;
            sc->rot.vz = 0x3E8;
            Gp_SpawnEff(0x6009C, ((TmdObject*)work->field_ECC[3]->task->extra)->coords, 0, &sc->rot);
            if (work->field_0 != 9 && work->field_F1C == 0) {
                work->field_0 = 8;
                work->field_2 = -1;
            }
            sc->damage *= 4;
        } else if ((Gp_GetIdParam0(sc->id) & 0xFFFF) != 2) {
            sc->damage = 0;
        }

        func_800E2C78((GpObj40*)host, sc->id, sc->damage, 0);
        host->hp -= sc->damage;
        func_800DA6E8(&work->field_ECC[3]->node, sc->damage, 0);
        work->field_F0E                                           -= sc->damage;
        esc3                                                       = work->field_ECC[3];
        hp                                                         = host->hp;
        esc0                                                       = work->field_ECC[0];
        esc1                                                       = work->field_ECC[1];
        esc3->hp                                                   = hp;
        esc1->hp                                                   = hp;
        esc0->hp                                                   = hp;
        ((TmdObject*)work->field_ECC[3]->task->extra)->coords->flg = 0;
        Gp_UpdateCoord(((TmdObject*)work->field_ECC[3]->task->extra)->coords);
        sc->rot.vx = sc->pos.vx - ((TmdObject*)work->field_ECC[3]->task->extra)->coords->workm.t[0];
        sc->rot.vy = sc->pos.vy - ((TmdObject*)work->field_ECC[3]->task->extra)->coords->workm.t[1];
        sc->rot.vz = sc->pos.vz - ((TmdObject*)work->field_ECC[3]->task->extra)->coords->workm.t[2];
        angle      = ratan2(sc->rot.vx, sc->rot.vz) -
                ratan2(-((TmdObject*)arg0->extra)->coords->workm.m[2][0],
                       ((TmdObject*)arg0->extra)->coords->workm.m[2][2]);
        do {
            sc->angle = angle;
            if (angle < 0) {
            wrapUp:
                if (angle < -0x800) {
                    angle += 0x1000;
                    goto wrapUp;
                }
            } else {
            wrapDown:
                if (angle > 0x800) {
                    angle -= 0x1000;
                    goto wrapDown;
                }
            }
        } while (0);
        sc->angle = angle;

        work->field_7C8 = 0;
        work->field_7C4 = 0;
    }
out:
    SCRATCH_SP += sizeof(Actor403200HitScratch);
}

/// The hit handler for collision groups 3, 4 and 5 -- `func_actor_403200_80139E94`
/// done three times over the parts it does not cover, each group only scanned
/// when the previous one landed nothing and the part it hit reported no attack
/// id back. Like the sibling actor's `func_actor_444000_8013CA60`, this one runs
/// no `Gp_GetIdParam0` switch: the call is made and its kind thrown away, so
/// every hit is treated alike, and the group 3 arm is the one that gives up and
/// leaves the frame once it comes back empty.
///
/// The damage is the distance-scaled hit quadrupled when `Gp_RollEnemyChance`
/// fires, then divided by six (never down to zero unless it already was), and
/// comes off the host, the two escorts sharing its pool and `field_F0A`.
/// Emptying that pool spawns the same effect again and refills it to 0x32. Both
/// effect spawns and the state change to 0xE are skipped while the boss is in
/// one of the seven states that ignore hits, while the player hold is armed, or
/// while `D_801153F4[1]` is clear.
///
/// `pos` / `pos2` / `pos3` are all `&sc->pos`, and are not spare: each group's
/// scan writes the contact point through its own pointer, which is what keeps
/// the three `sh` pairs in `a3` then `a2` twice. `esc3` / `esc0` / `esc1` and
/// the `hp` load are the sibling's arrangement, but evaluated before
/// `func_800DA6E8` so `host->field_40` is still in a register and the three
/// stores reuse it; the pool subtraction after them carries the same `field_40`
/// value for the same reason.
void func_actor_403200_8013A4A0(Task* arg0)
{
    Actor403200HitScratch* sc;
    Actor403200Work*       work;
    GpEnemy*               host;
    PlayerStatus*          cfg;
    GpRec18*               recs;
    GpRec18*               recs2;
    GpRec18*               recs3;
    SVECTOR*               pos;
    SVECTOR*               pos2;
    SVECTOR*               pos3;
    s32                    id;
    s32                    dx2;
    s32                    dy2;
    s32                    dz2;
    u32                    dmg;
    s16                    angle;
    s16                    state;
    s16                    i;
    s16                    i2;
    s16                    i3;
    s16                    param;
    u16                    hp;
    GpEnemy*               esc3;
    GpEnemy*               esc0;
    GpEnemy*               esc1;

    cfg  = &Player_Status;
    host = (GpEnemy*)arg0->spawnArg2;
    work = (Actor403200Work*)arg0->work;
    sc   = (Actor403200HitScratch*)(SCRATCH_SP -= sizeof(Actor403200HitScratch));
    pos  = &sc->pos;
    recs = work->hits[3].recs;
    for (i = 0; i < 5; i++) {
        if (recs[i].key == 0) {
            goto missed1;
        }
        if ((recs[i].key & 0xFFFF0000) == 0x20000) {
            pos->vx = recs[i].point.vx;
            pos->vy = recs[i].point.vy;
            pos->vz = recs[i].point.vz;
            id      = recs[i].key;
            goto found1;
        }
    }
missed1:
    id = 0;
found1:
    sc->id = id;
    if (id != 0) {
        func_actor_403200_80134044(work->hits[3].obj.coord, id);
        if (sc->id != 0) {
            goto body;
        }
    }

    pos2  = &sc->pos;
    recs2 = work->hits[4].recs;
    for (i2 = 0; i2 < 5; i2++) {
        if (recs2[i2].key == 0) {
            goto missed2;
        }
        if ((recs2[i2].key & 0xFFFF0000) == 0x20000) {
            pos2->vx = recs2[i2].point.vx;
            pos2->vy = recs2[i2].point.vy;
            pos2->vz = recs2[i2].point.vz;
            id       = recs2[i2].key;
            goto found2;
        }
    }
missed2:
    id = 0;
found2:
    sc->id = id;
    if (id != 0) {
        func_actor_403200_80134044(work->hits[4].obj.coord, id);
        if (sc->id != 0) {
            goto body;
        }
    }

    pos3  = &sc->pos;
    recs3 = work->hits[5].recs;
    for (i3 = 0; i3 < 5; i3++) {
        if (recs3[i3].key == 0) {
            goto missed3;
        }
        if ((recs3[i3].key & 0xFFFF0000) == 0x20000) {
            pos3->vx = recs3[i3].point.vx;
            pos3->vy = recs3[i3].point.vy;
            pos3->vz = recs3[i3].point.vz;
            id       = recs3[i3].key;
            goto found3;
        }
    }
missed3:
    id = 0;
found3:
    sc->id = id;
    if (id == 0) {
        goto out;
    }
    func_actor_403200_80134044(work->hits[5].obj.coord, id);
    if (sc->id == 0) {
        goto out;
    }
body:
    param           = Gp_GetIdParam2(sc->id);
    work->field_E90 = param;
    work->field_E8E = param;
    work->field_E8C = param;
    work->field_E92 = param;
    Gp_GetIdParam0(sc->id);

    sc->delta.vx = (cfg->coordMtx->t[0] - ((TmdObject*)arg0->extra)->coords->coord.t[0]) + 0x51F;
    dx2          = sc->delta.vx * sc->delta.vx;
    sc->delta.vy = (cfg->coordMtx->t[1] - ((TmdObject*)arg0->extra)->coords->coord.t[1]) - 0xFA;
    dy2          = sc->delta.vy * sc->delta.vy;
    sc->delta.vz = (cfg->coordMtx->t[2] - ((TmdObject*)arg0->extra)->coords->coord.t[2]) + 0x25F;
    dz2          = sc->delta.vz * sc->delta.vz;
    sc->dist     = SquareRoot0(dx2 + dy2 + dz2);
    sc->damage   = Gp_ComputeDamage(sc->id, sc->dist, 0, 0);

    if (Gp_RollEnemyChance(work->field_ECC[0], sc->id, 0) != 0 && (state = work->field_0, state != 0xD) && state != 3 &&
        state != 9 && state != 0xE && state != 0xF && state != 8 && state != 0xB && work->field_EC8 != 1 &&
        D_801153F4[1] == 1) {
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vz = 0x320;
        Gp_SpawnEff(0x6009C, &((TmdObject*)work->field_ECC[0]->task->extra)->coords[1], 0, &sc->rot);
        sc->damage   *= 4;
        work->field_0 = 0xE;
    }

    dmg = sc->damage / 6;
    if (dmg == 0) {
        dmg = 1;
        if (sc->damage == 0) {
            sc->damage = 0;
            goto stored;
        }
    }
    sc->damage = dmg;
stored:
    func_800E2C78((GpObj40*)host, sc->id, sc->damage, 0);
    host->hp        -= sc->damage;
    esc3             = work->field_ECC[3];
    hp               = host->hp;
    esc0             = work->field_ECC[0];
    esc1             = work->field_ECC[1];
    esc3->hp         = hp;
    esc1->hp         = hp;
    esc0->hp         = hp;
    work->field_F0A -= sc->damage;
    if ((s16)work->field_F0A <= 0 && (state = work->field_0, state != 0xD) && state != 3 && state != 9 && state != 0xE &&
        state != 0xF && state != 8 && state != 0xB && work->field_EC8 != 1 && D_801153F4[1] == 1) {
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vz = 0x320;
        Gp_SpawnEff(0x6009C, &((TmdObject*)work->field_ECC[0]->task->extra)->coords[1], 0, &sc->rot);
        work->field_0   = 0xE;
        work->field_F0A = 0x32;
    }

    func_800DA6E8(&work->field_ECC[0]->node, sc->damage, 0);
    ((TmdObject*)work->field_ECC[0]->task->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)work->field_ECC[0]->task->extra)->coords);
    sc->rot.vx = sc->pos.vx - ((TmdObject*)work->field_ECC[0]->task->extra)->coords->workm.t[0];
    sc->rot.vy = sc->pos.vy - ((TmdObject*)work->field_ECC[0]->task->extra)->coords->workm.t[1];
    sc->rot.vz = sc->pos.vz - ((TmdObject*)work->field_ECC[0]->task->extra)->coords->workm.t[2];
    angle      = ratan2(sc->rot.vx, sc->rot.vz) -
            ratan2(-((TmdObject*)arg0->extra)->coords->workm.m[2][0],
                   ((TmdObject*)arg0->extra)->coords->workm.m[2][2]);
    do {
        sc->angle = angle;
        if (angle < 0) {
        wrapUp:
            if (angle < -0x800) {
                angle += 0x1000;
                goto wrapUp;
            }
        } else {
        wrapDown:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto wrapDown;
            }
        }
    } while (0);
    sc->angle = angle;

    work->field_7C8 = 0;
    work->field_7C4 = 0;
out:
    SCRATCH_SP += sizeof(Actor403200HitScratch);
}

/// The hit handler for collision groups 6, 7 and 8 -- the same three-scan shape
/// as `func_actor_403200_8013A4A0` runs for groups 3, 4 and 5, with the next
/// group only scanned when the previous one landed nothing and the part it hit
/// reported no attack id back. Like the sibling actor's
/// `func_actor_444000_8013D128`, the first two groups share one call site
/// through `coord`, and `Gp_GetIdParam0` is called and its kind thrown away.
///
/// Damage is the distance-scaled hit -- measured from an offset point rather
/// than the model origin -- quadrupled when `Gp_RollEnemyChance` fires, then
/// divided by six (never down to zero unless it already was), and comes off the
/// host, the two escorts sharing its pool and `field_F0C`. Emptying that pool
/// spawns the same effect again and refills it to 0x3C. Both effect spawns and
/// the state change to 0xE are skipped while the boss is in one of the seven
/// states that ignore hits, while the player hold is armed, or while
/// `D_801153F4[1]` is clear.
///
/// The second escort carries the damage and the effect, but `sc->angle` is the
/// yaw of the contact point relative to the first escort's facing. `pos` /
/// `pos2` / `pos3` are all `&sc->pos` and are not spare: each group's scan
/// writes the contact point through its own pointer. `esc3` / `esc0` / `esc1`
/// and the `hp` load sit after `func_800DA6E8`, unlike the group 3-5 handler.
void func_actor_403200_8013AB70(Task* arg0)
{
    Actor403200HitScratch* sc;
    Actor403200Work*       work;
    GpEnemy*               host;
    PlayerStatus*          cfg;
    GsCOORDINATE2*         coord;
    GpRec18*               recs;
    GpRec18*               recs2;
    GpRec18*               recs3;
    SVECTOR*               pos;
    SVECTOR*               pos2;
    SVECTOR*               pos3;
    s32                    id;
    s32                    dx2;
    s32                    dy2;
    s32                    dz2;
    u32                    dmg;
    s16                    angle;
    s16                    state;
    s16                    i;
    s16                    i2;
    s16                    i3;
    s16                    param;
    u16                    hp;
    GpEnemy*               esc3;
    GpEnemy*               esc0;
    GpEnemy*               esc1;

    cfg  = &Player_Status;
    host = (GpEnemy*)arg0->spawnArg2;
    work = (Actor403200Work*)arg0->work;
    sc   = (Actor403200HitScratch*)(SCRATCH_SP -= sizeof(Actor403200HitScratch));
    pos  = &sc->pos;
    recs = work->hits[6].recs;
    for (i = 0; i < 5; i++) {
        if (recs[i].key == 0) {
            goto missed1;
        }
        if ((recs[i].key & 0xFFFF0000) == 0x20000) {
            pos->vx = recs[i].point.vx;
            pos->vy = recs[i].point.vy;
            pos->vz = recs[i].point.vz;
            id      = recs[i].key;
            goto found1;
        }
    }
missed1:
    id = 0;
found1:
    sc->id = id;
    if (id != 0) {
        coord = work->hits[6].obj.coord;
        goto hit;
    }

    pos2  = &sc->pos;
    recs2 = work->hits[7].recs;
    for (i2 = 0; i2 < 5; i2++) {
        if (recs2[i2].key == 0) {
            goto missed2;
        }
        if ((recs2[i2].key & 0xFFFF0000) == 0x20000) {
            pos2->vx = recs2[i2].point.vx;
            pos2->vy = recs2[i2].point.vy;
            pos2->vz = recs2[i2].point.vz;
            id       = recs2[i2].key;
            goto found2;
        }
    }
missed2:
    id = 0;
found2:
    sc->id = id;
    if (id != 0) {
        coord = work->hits[7].obj.coord;
    hit:
        func_actor_403200_80134044(coord, id);
        if (sc->id != 0) {
            goto body;
        }
    }

    pos3  = &sc->pos;
    recs3 = work->hits[8].recs;
    for (i3 = 0; i3 < 5; i3++) {
        if (recs3[i3].key == 0) {
            goto missed3;
        }
        if ((recs3[i3].key & 0xFFFF0000) == 0x20000) {
            pos3->vx = recs3[i3].point.vx;
            pos3->vy = recs3[i3].point.vy;
            pos3->vz = recs3[i3].point.vz;
            id       = recs3[i3].key;
            goto found3;
        }
    }
missed3:
    id = 0;
found3:
    sc->id = id;
    if (id == 0) {
        goto out;
    }
    func_actor_403200_80134044(work->hits[8].obj.coord, id);
    if (sc->id == 0) {
        goto out;
    }
body:
    param           = Gp_GetIdParam2(sc->id);
    work->field_E90 = param;
    work->field_E8E = param;
    work->field_E8C = param;
    work->field_E92 = param;
    Gp_GetIdParam0(sc->id);

    sc->delta.vx = (cfg->coordMtx->t[0] - ((TmdObject*)arg0->extra)->coords->coord.t[0]) - 0x51F;
    dx2          = sc->delta.vx * sc->delta.vx;
    sc->delta.vy = (cfg->coordMtx->t[1] - ((TmdObject*)arg0->extra)->coords->coord.t[1]) - 0xFA;
    dy2          = sc->delta.vy * sc->delta.vy;
    sc->delta.vz = (cfg->coordMtx->t[2] - ((TmdObject*)arg0->extra)->coords->coord.t[2]) + 0x25F;
    dz2          = sc->delta.vz * sc->delta.vz;
    sc->dist     = SquareRoot0(dx2 + dy2 + dz2);
    sc->damage   = Gp_ComputeDamage(sc->id, sc->dist, 0, 0);

    if (Gp_RollEnemyChance(work->field_ECC[1], sc->id, 0) != 0 && (state = work->field_0, state != 0xD) && state != 3 &&
        state != 9 && state != 0xE && state != 0xF && state != 8 && state != 0xB && work->field_EC8 != 1 &&
        D_801153F4[1] == 1) {
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vz = 0x320;
        Gp_SpawnEff(0x6009C, &((TmdObject*)work->field_ECC[1]->task->extra)->coords[1], 0, &sc->rot);
        sc->damage   *= 4;
        work->field_0 = 0xE;
    }

    dmg = sc->damage / 6;
    if (dmg == 0) {
        dmg = 1;
        if (sc->damage == 0) {
            sc->damage = 0;
            goto stored;
        }
    }
    sc->damage = dmg;
stored:
    func_800E2C78((GpObj40*)host, sc->id, sc->damage, 0);
    host->hp        -= sc->damage;
    work->field_F0C -= sc->damage;
    if ((s16)work->field_F0C <= 0 && (state = work->field_0, state != 0xD) && state != 3 && state != 9 && state != 0xE &&
        state != 0xF && state != 8 && state != 0xB && work->field_EC8 != 1 && D_801153F4[1] == 1) {
        sc->rot.vy = 0;
        sc->rot.vx = 0;
        sc->rot.vz = 0x320;
        Gp_SpawnEff(0x6009C, &((TmdObject*)work->field_ECC[1]->task->extra)->coords[1], 0, &sc->rot);
        work->field_0   = 0xE;
        work->field_F0C = 0x3C;
    }

    func_800DA6E8(&work->field_ECC[1]->node, sc->damage, 0);
    esc3                                                       = work->field_ECC[3];
    hp                                                         = host->hp;
    esc0                                                       = work->field_ECC[0];
    esc1                                                       = work->field_ECC[1];
    esc3->hp                                                   = hp;
    esc1->hp                                                   = hp;
    esc0->hp                                                   = hp;
    ((TmdObject*)work->field_ECC[1]->task->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)work->field_ECC[1]->task->extra)->coords);
    sc->rot.vx = sc->pos.vx - ((TmdObject*)work->field_ECC[0]->task->extra)->coords->workm.t[0];
    sc->rot.vy = sc->pos.vy - ((TmdObject*)work->field_ECC[0]->task->extra)->coords->workm.t[1];
    sc->rot.vz = sc->pos.vz - ((TmdObject*)work->field_ECC[0]->task->extra)->coords->workm.t[2];
    angle      = ratan2(sc->rot.vx, sc->rot.vz) -
            ratan2(-((TmdObject*)arg0->extra)->coords->workm.m[2][0],
                   ((TmdObject*)arg0->extra)->coords->workm.m[2][2]);
    do {
        sc->angle = angle;
        if (angle < 0) {
        wrapUp:
            if (angle < -0x800) {
                angle += 0x1000;
                goto wrapUp;
            }
        } else {
        wrapDown:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto wrapDown;
            }
        }
    } while (0);
    sc->angle = angle;

    work->field_7C8 = 0;
    work->field_7C4 = 0;
out:
    SCRATCH_SP += sizeof(Actor403200HitScratch);
}

/// Reset handler: pushes the host model's `field_C` onto each of the seven
/// escorts, and once the sub-state counter has reached 2 releases the host's and
/// every escort's model buffers. Same shape as
/// `func_actor_403200_80141B40` with a second arm keyed on `field_6`.
///
/// The `modelFlag` copy is not redundant: the second arm's `0x80` has to reach
/// the store as a 32-bit value of its own, or the two arms merge it into the
/// first arm's constant and the second `li $v0, 0x80` disappears.
void func_actor_403200_8013B23C(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    TmdObject*       tmd;
    s32              flag;
    s32              modelFlag;
    s16              i;
    s16              j;

    work = (Actor403200Work*)arg0->work;
    tmd  = (TmdObject*)arg0->extra;
    if (work->field_4 != 0) {
        tmd->flags                       = 0x80;
        escorts                          = (Actor403200Work*)arg0->work;
        i                                = 0;
        escorts->field_7F3               = 0;
        ((TmdObject*)arg0->extra)->flags = (flag = 0x80);
        for (; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags =
                    ((TmdObject*)arg0->extra)->flags;
            }
        }
        work->field_6 = 0;
        return;
    }
    if (work->field_6 == 2) {
        tmd->flags                       = 0x80;
        escorts                          = (Actor403200Work*)arg0->work;
        modelFlag                        = 0x80;
        i                                = 0;
        escorts->field_7F3               = 0;
        ((TmdObject*)arg0->extra)->flags = (flag = modelFlag);
        for (; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags =
                    ((TmdObject*)arg0->extra)->flags;
            }
        }
        dying = (Actor403200Work*)arg0->work;
        Tmd_FreeBuffers((TmdObject*)arg0->extra);
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
                Tmd_FreeBuffers((TmdObject*)dying->field_ECC[j]->task->extra);
            }
        }
    }
}

/// State-change reset for the enemy's launch state: `func_actor_403200_8013B23C`'s
/// reset half with a yaw servo in the middle. It arms the stand-up pair
/// (`field_F1D` 2, `field_7B3` 3), turns animation slot 2 on, clears the host
/// model's flag word and walks the seven escorts pushing that word onto each of
/// their models, allocates the host's and every escort's buffers, and only then
/// turns the enemy to face the player -- the host root part's position made
/// relative to the player's root coordinate, `ratan2` of that pair less the
/// enemy's own facing, wrapped to +/-0x800 into `field_7C4`. On the way out it
/// runs the per-frame body, re-arms the state to 0xA on the animation slot's
/// flag, and latches `field_F06` once the state counter is past 0x14.
///
/// The switch is on the state counter and spawns from
/// `D_actor_403200_8015E858`, each of the eight counter values picking its own
/// table index; the spawned enemy is dropped, unlike the arena reset's. The
/// `state` copy is what keeps the switch index 16-bit, as in
/// `func_actor_403200_8013D9EC`.
void func_actor_403200_8013B3C8(Task* arg0)
{
    Actor403200Work*        work;
    Actor403200Work*        escorts;
    Actor403200Work*        dying;
    GsCOORDINATE2*          model;
    GsCOORDINATE2*          facing;
    Actor403200TurnScratch* sc;
    s16                     i;
    s16                     j;
    s16                     state;
    s16                     ang;

    sc   = (Actor403200TurnScratch*)(SCRATCH_SP -= sizeof(Actor403200TurnScratch));
    work = (Actor403200Work*)arg0->work;
    if (work->field_4 != 0) {
        work->field_F1D                  = 2;
        work->field_7B3                  = 3;
        work->field_7B0                  = 2;
        escorts                          = (Actor403200Work*)arg0->work;
        escorts->field_7F3               = 0;
        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags =
                    ((TmdObject*)arg0->extra)->flags;
            }
        }
        dying = (Actor403200Work*)arg0->work;
        Tmd_AllocBuffers((TmdObject*)arg0->extra);
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
                Tmd_AllocBuffers((TmdObject*)dying->field_ECC[j]->task->extra);
            }
        }
        work->field_EF6 = 1;
        work->field_EF4 = 1;
        work->field_EFA = 0;
    }
    state = work->field_6 - 0x13;
    switch (state) {
        case 0:
            Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 1, 0, arg0->spawnArg2)->workType = 0x900;
            break;
        case 7:
            Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 1, 1, arg0->spawnArg2)->workType = 0x900;
            break;
        case 9:
            Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 1, 2, arg0->spawnArg2)->workType = 0x900;
            break;
        case 0x10:
            Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 1, 3, arg0->spawnArg2)->workType = 0x900;
            break;
        case 0x1F:
            Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 1, 4, arg0->spawnArg2)->workType = 0x900;
            break;
        case 0x37:
            Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 1, 5, arg0->spawnArg2)->workType = 0x900;
            break;
        case 0x3B:
            Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 1, 6, arg0->spawnArg2)->workType = 0x900;
            break;
        case 0x3F:
            Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 1, 7, arg0->spawnArg2)->workType = 0x900;
            break;
    }
    func_actor_403200_80133DD8(arg0);
    if (work->field_58 & 1) {
        work->field_0 = 0xA;
    }
    if (work->field_6 >= 0x15) {
        work->field_F06 = 1;
    }
    model      = ((TmdObject*)arg0->extra)->coords;
    sc->dir.vx = Player_Status.coordMtx->t[0] - model->coord.t[0];
    sc->dir.vy = Player_Status.coordMtx->t[1] - model->coord.t[1];
    sc->dir.vz = Player_Status.coordMtx->t[2] - model->coord.t[2];
    facing     = ((TmdObject*)arg0->extra)->coords;
    ang        = ratan2(sc->dir.vx, sc->dir.vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    if (ang < 0) {
    wrapUp:
        if (ang < -0x800) {
            ang += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (ang > 0x800) {
            ang -= 0x1000;
            goto wrapDown;
        }
    }
    work->field_7C4 = ang;
    SCRATCH_SP     += sizeof(Actor403200TurnScratch);
}

/// Spawns up to nine enemies in a randomly selected formation, stopping when
/// a spawn fails. Each member's index becomes the high nibble of its place key.
void func_actor_403200_8013B740(Task* arg0)
{
    Actor403200Work*     work;
    TaskDesc*            desc;
    Actor403200SpawnRec* entries;
    SVECTOR(*positions)
    [9];
    SVECTOR* row;
    s32      offset;
    s32      rowOffset;
    GpEnemy* enemy;
    s16      i;
    s16      formation;

    work = (Actor403200Work*)arg0->work;

    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    formation   = ((u32)Gp_LcgState >> 16) & 3;
    if (formation == 3) {
        formation = 0;
    }

    i         = 0;
    desc      = &D_actor_403200_8015E858;
    entries   = D_actor_403200_8015F888;
    positions = D_actor_403200_8015F7B0;

spawnNext:
    offset = i * 8;
    SOFT_USE_REG(offset);
    desc[4].arg.model = ((Actor403200SpawnRec*)(offset + (u32)entries))->model;
    enemy             = Gp_SpawnEnemyFromTable(desc, 4, ((Actor403200SpawnRec*)(offset + (u32)entries))->spawnArg, NULL);
    work->field_EF0   = enemy;
    if (enemy != NULL) {
        rowOffset                                                      = ((formation * 8) | formation) * 8;
        row                                                            = (SVECTOR*)((offset + rowOffset) + (u32)positions);
        ((TmdObject*)enemy->task->extra)->coords->coord.t[0]           = row->vx;
        ((TmdObject*)work->field_EF0->task->extra)->coords->coord.t[1] = row->vy;
        ((TmdObject*)work->field_EF0->task->extra)->coords->coord.t[2] = row->vz;
        work->field_EF0->workType                                      = 0x900;
        work->field_EF0->placeKey                                     |= i << 12;
        i++;
        if (i < 9) {
            goto spawnNext;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_8013B8C4);

/// State-change reset for the enemy's launch state, and the tick that walks it
/// out of sub-state 0xF into 0xE.
///
/// The reset half is `func_actor_403200_8013B23C`'s with a yaw servo in the
/// middle: it tells the scene (message 0x7DA, action 0x2C), arms sub-state 0xF
/// with animation 2, clears the host model's flag word and walks the seven
/// escorts pushing that word onto each of their models, allocates the host's and
/// every escort's buffers, and only then turns the enemy to face the player --
/// the fourth model part's position carried into view space, made relative to
/// the player's root coordinate with y zeroed, `ratan2` of that pair less the
/// enemy's own facing, wrapped to +/-0x800 into `field_7C4`. It tells the scene
/// a second time, arms `field_E96`, raises bit 0 of `Gp_StateC08.field_6`,
/// pulses the state and clears the node slot of the host and of escorts 3, 0
/// and 1.
///
/// The tick runs the per-frame body, steps 0xF to 0xE on the second animation
/// slot's flag, and while still in 0xF hands the player the launch message
/// (0x3F9) with `Player_Status.hp` as its gate: the two arms either side
/// of that dispatch write the ramp timings into `gGameSession` and stamp escort
/// 3. The four one-shot cues all latch on the third animation slot's frame,
/// masked to ten bits, against the frame `field_7A8` saw last, and once the
/// state counter is past 0x18 the type-7 cue and the 0x3FF animation message go
/// out together.
///
/// Three things here are load-bearing. The yaw's arguments are read through
/// `posp` and the matrix half through `coord`: read straight off `view` the
/// stores would be forwarded into both arguments (two `sll`/`sra` pairs),
/// while through the pointer each stays a load out of the struct, which is what
/// the target does -- the second is reloaded from its slot, the first is folded
/// back onto `a0`, and `coord` is what keeps `field_8` in `s0` across the call.
/// The cue locals are declared inside each arm so local-alloc colours them per
/// block; hoisted to the top of the function they become one global pseudo and
/// the id and pan come out in each other's registers. And in the second 0x7DA
/// block `D_actor_403200_8015F8E0` is cleared before the `field_7C4` store, so
/// its address is the one computed first.
void func_actor_403200_8013C84C(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    GpEnemy*         enemy;
    Task*            task;
    PlayerStatus*    cfg;
    SVECTOR          view;
    SVECTOR*         posp;
    GsCOORDINATE2*   coord;
    s16              i;
    s16              j;
    s16              yaw;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2;
    task  = gameGetPtrSlot(3);
    cfg   = &Player_Status;
    if (work->field_4 != 0) {
        D_actor_403200_8015F8F4.field_0 = 0;
        D_actor_403200_8015F8F4.field_1 = 0x2C;
        D_actor_403200_8015F8F4.field_2 = 3;
        Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&D_actor_403200_8015F8F4, 0x7DB);
        D_actor_403200_80141C5A = 0;
        SndEvt_EnqueueType7((((u16)enemy->placeKey >> 12) << 8) | 0x4020000A, 1);
        work->field_7B3                  = 0xF;
        work->field_7B0                  = 2;
        escorts                          = (Actor403200Work*)arg0->work;
        escorts->field_7F3               = 0;
        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags =
                    ((TmdObject*)arg0->extra)->flags;
            }
        }
        dying = (Actor403200Work*)arg0->work;
        Tmd_AllocBuffers((TmdObject*)arg0->extra);
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
                Tmd_AllocBuffers((TmdObject*)dying->field_ECC[j]->task->extra);
            }
        }
        work->field_EF6 = 1;
        work->field_F06 = 6;
        work->field_EFE = 0;
        work->field_EF4 = 0;
        work->field_EFA = 0;
        view.vz         = 0;
        view.vy         = 0;
        view.vx         = 0;
        Actor403200_LocalToView(&((TmdObject*)arg0->extra)->coords[4], &view);
        view.vx = ((TmdObject*)task->extra)->coords[0].coord.t[0] - view.vx;
        view.vy = 0;
        view.vz = ((TmdObject*)task->extra)->coords[0].coord.t[2] - view.vz;
        posp    = &view;
        coord   = ((TmdObject*)arg0->extra)->coords;
        yaw     = ratan2(posp->vx, posp->vz) -
              ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        if (yaw < 0) {
        wrapUp:
            if (yaw < -0x800) {
                yaw += 0x1000;
                goto wrapUp;
            }
        } else {
        wrapDown:
            if (yaw > 0x800) {
                yaw -= 0x1000;
                goto wrapDown;
            }
        }
        D_actor_403200_8015F8E0         = 0;
        work->field_7C4                 = yaw;
        D_actor_403200_8015F8F4.field_0 = 0;
        D_actor_403200_8015F8F4.field_1 = 0x2C;
        D_actor_403200_8015F8F4.field_2 = 3;
        Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&D_actor_403200_8015F8F4, 0x7DB);
        work->field_E96         = 0x9C4;
        D_actor_403200_80141C5A = 0;
        Gp_StateC08.field_6    |= 1;
        Gp_PulseState1C();
        Gp_ClearNodeSlots(&enemy->node);
        Gp_ClearNodeSlots(&work->field_ECC[3]->node);
        Gp_ClearNodeSlots(&work->field_ECC[0]->node);
        Gp_ClearNodeSlots(&work->field_ECC[1]->node);
        return;
    }

    SCRATCH_SP -= 0x3C;
    func_actor_403200_80133DD8(arg0);
    if ((work->field_58 & 1) && (work->field_7B3 == 0xF)) {
        work->field_7B0 = 2;
        work->field_7B3 = 0xE;
    }
    if (work->field_7B3 == 0xF) {
        if (cfg->hp > 0) {
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 3), 0);
            if (cfg->hp <= 0) {
                ((GameActor*)task->work)->field_956 = 0xA;
                gGameSession->areaBgmCountdown      = 0x1E;
                gGameSession->field_12E             = 0x36;
                gGameSession->deathRestartDelay     = 0x5A;
            }
        }
        if (((work->field_9A & 0x3FF) == 0x19) && (work->field_7A8 != (work->field_9A & 0x3FF))) {
            s32 sfx;
            s32 pan;
            s32 depth;

            sfx   = (((u16)enemy->placeKey >> 12) << 8) | 0x40200011;
            pan   = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
            depth = (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords);
            SndEvt_EnqueueType6(sfx, pan, depth);
        }
        work->field_7A8 = work->field_9A & 0x3FF;
    }
    if (work->field_7B3 == 0xE) {
        if (((work->field_9A & 0x3FF) == 0x1E) && (work->field_7A8 != (work->field_9A & 0x3FF))) {
            Gp_SpawnPadLerp(4, 0xFF, 8);
        }
        if (((work->field_9A & 0x3FF) == 0x23) && (work->field_7A8 != (work->field_9A & 0x3FF))) {
            s32 sfx;
            s32 pan;

            sfx = (((u16)enemy->placeKey >> 12) << 8) | 0x40200012;
            pan = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
            SndEvt_EnqueueType6(sfx, pan,
                                (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
            Gp_SpawnPadLerp(4, 0xFF, 8);
        }
        if (((work->field_9A & 0x3FF) == 0x27) && (work->field_7A8 != (work->field_9A & 0x3FF))) {
            s32 sfx;
            s32 pan;

            sfx = (((u16)enemy->placeKey >> 12) << 8) | 0x40200012;
            pan = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
            SndEvt_EnqueueType6(sfx, pan,
                                (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
            Gp_SpawnPadLerp(4, 0xFF, 8);
        }
        work->field_7A8 = work->field_9A & 0x3FF;
    }
    if ((Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) && (cfg->hp > 0)) {
        D_actor_403200_8015F9C0.vx = ((TmdObject*)arg0->extra)->coords[0].coord.t[0];
        D_actor_403200_8015F9C0.vy = ((TmdObject*)arg0->extra)->coords[0].coord.t[1];
        D_actor_403200_8015F9C0.vz = ((TmdObject*)arg0->extra)->coords[0].coord.t[2];
        Gp_DispatchMsg(task, 0x3E9, (s32)&D_actor_403200_8015F9C0, 0);
        D_actor_403200_8015F8E0 = 1;
    }
    if (work->field_6 < 0x18) {
        SndEvt_EnqueueType7((((u16)enemy->placeKey >> 12) << 8) | 0x4020000A, 1);
        Gp_DispatchMsg(task, 0x3FF, (s32)&work->field_EB0, 0);
        work->field_7CA = 0;
    }
    SCRATCH_SP += 0x3C;
}

/// State-change reset for the enemy's stand-up, plus the swipe tick that runs
/// on every step afterwards. The reset half is `func_actor_403200_8013B23C`'s
/// with the buffer allocator on the second walk in place of the release: it
/// clears the host model's flag word, walks the seven escorts pushing that word
/// onto each of their models, allocates the host's and every escort's buffers,
/// rebuilds the free coordinate `field_E3C` from `field_7C8` and arms `field_E96`
/// at 0xC80 before playing the entry cue.
///
/// Every step then clears that coordinate's flag and updates it, and each of the
/// two swipe sub-states watches one animation slot's frame: sub-state 4 raises
/// bit 0x8000 of the tenth collision object's `flags` and fires its two cues once
/// `slots0[1]` reaches frame 0xC, sub-state 5 clears `field_EFA` and fires its
/// single cue on `slots0[2]` frame 0x1C. Both cues are positioned on the first
/// escort's second coordinate at half depth, and whichever sub-state is live is
/// the one whose frame `field_7AC` is refreshed from -- the shared mask is what
/// makes the pair one-shot. The switch on `field_6` arms the escort pose index
/// `field_7A4` for seven states, 0x14 and 0xDC also seeding the shared countdown
/// `D_actor_403200_80141C58` and re-arming `field_0`, and the 0x29..0x2E window
/// raises that countdown by 0x258 while it is still under 0x1770.
///
/// The tail runs the per-frame body, scans the tenth collision object's five
/// `recs2` records for one whose high half is 0x10000, and -- when it finds one,
/// the enemy's HP is positive and the player's 0x3F8 query comes back zero --
/// asks the player for the hold (0x3F9) and re-sends it the animation, stamping
/// the player's `field_956` when the hold was taken. Past frame 0x39 the shared
/// countdown is walked down 0x1E, or 0xC8 once it is past 0xBB9, and past 0x15
/// the state arms `field_F06`.
///
/// The countdown's two arms are load-bearing: the `>= 0xBB9` test reads the
/// halfword signed (`lh`) while each arm subtracts from it zero-extended
/// (`lhu`), and writing the pair as one assignment off a shared temp lets CSE
/// fold the compare onto the earlier zero-extended load, which costs an
/// `sll`/`sra` re-extension pair the original does not have.
void func_actor_403200_8013D028(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    GpRec18*         recs;
    GpEnemy*         enemy;
    Task*            task;
    Task*            target;
    s16              i;
    s16              j;
    s16              k;
    s16              frame;
    s16              frame2;
    s16              reply;
    s32              found;
    s32              resetId;
    s32              resetPan;
    s32              swipeId;
    s32              swipePan;
    s32              swipe2Id;
    s32              swipe2Pan;
    s32              hitId;
    s32              hitPan;
    s32              cueId;
    s32              cuePan;

    work        = (Actor403200Work*)arg0->work;
    enemy       = arg0->spawnArg2;
    task        = gameGetPtrSlot(3);
    SCRATCH_SP -= 0x30;

    if (work->field_4 != 0) {
        work->field_F1D                  = 0xB;
        work->field_7B3                  = 4;
        work->field_7B0                  = 2;
        escorts                          = (Actor403200Work*)arg0->work;
        escorts->field_7F3               = 0;
        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags =
                    ((TmdObject*)arg0->extra)->flags;
            }
        }
        dying = (Actor403200Work*)arg0->work;
        Tmd_AllocBuffers((TmdObject*)arg0->extra);
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
                Tmd_AllocBuffers((TmdObject*)dying->field_ECC[j]->task->extra);
            }
        }
        work->field_EF6 = 1;
        work->field_EF4 = 0;
        work->field_EFA = 1;
        work->field_EF8 = 1;
        Gfx_RotMatrixY(&work->field_E3C.coord, work->field_7C8, 1);
        work->field_E3C.flg = 0;
        Gp_UpdateCoord(&work->field_E3C);
        work->field_E96 = 0xC80;
        resetId         = (((u16)enemy->placeKey >> 12) << 8) | 0x40200017;
        resetPan        = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(resetId, resetPan,
                            (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }

    work->field_E3C.flg = 0;
    Gp_UpdateCoord(&work->field_E3C);

    if (work->field_7B3 == 4 && (frame = work->field_4A & 0x3FF) == 0xC &&
        work->field_7AC != frame) {
        work->field_EAC  = 3;
        work->obj.flags |= 0x8000;
        Gp_SpawnPadLerp(0x30, 0xFF, 8);
        swipeId  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200019;
        swipePan = (s8)Gp_GetObjPan(
            &((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]);
        SndEvt_EnqueueType6(
            swipeId, swipePan,
            (s8)(gpGetObjDepth(
                     &((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]) /
                 2));
        swipe2Id  = (((u16)enemy->placeKey >> 12) << 8) | 0x4020001A;
        swipe2Pan = (s8)Gp_GetObjPan(
            &((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]);
        SndEvt_EnqueueType6(
            swipe2Id, swipe2Pan,
            (s8)(gpGetObjDepth(
                     &((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]) /
                 2));
    } else {
        work->obj.flags &= 0x7FFF;
    }

    if (work->field_7B3 == 5 && (frame2 = work->field_72 & 0x3FF) == 0x1C &&
        work->field_7AC != frame2) {
        work->field_EFA = 0;
        work->field_EAC = 3;
        Gp_SpawnPadLerp(0x20, 0x7F, 8);
        hitId  = (((u16)enemy->placeKey >> 12) << 8) | 0x4020001B;
        hitPan = (s8)Gp_GetObjPan(
            &((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]);
        SndEvt_EnqueueType6(
            hitId, hitPan,
            (s8)(gpGetObjDepth(
                     &((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]) /
                 2));
    }

    if (work->field_7B3 == 4) {
        work->field_7AC = work->field_4A & 0x3FF;
    } else {
        work->field_7AC = work->field_72 & 0x3FF;
    }

    switch (work->field_6) {
        case 0x14:
            D_actor_403200_80141C58 = 0x640;
            work->field_7A4         = 0;
            break;
        case 0x22:
            work->field_7A4 = 1;
            break;
        case 0x2B:
            work->field_7A4 = 5;
            cueId           = (((u16)enemy->placeKey >> 12) << 8) | 0x40200018;
            cuePan          = (s8)Gp_GetObjPan(
                &((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]);
            SndEvt_EnqueueType6(
                cueId, cuePan,
                (s8)(gpGetObjDepth(
                         &((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]) /
                     2));
            break;
        case 0x2D:
            work->field_7A4 = 2;
            break;
        case 0x38:
            work->field_7A4 = 4;
            break;
        case 0x44:
            work->field_7A4 = 3;
            break;
        case 0xDC:
            work->field_0 = 0xA;
            break;
    }

    if ((u32)((u16)work->field_6 - 0x29) < 6 && D_actor_403200_80141C58 < 0x1770) {
        D_actor_403200_80141C58 = (u16)D_actor_403200_80141C58 + 0x258;
    }

    func_actor_403200_80133DD8(arg0);

    recs = work->recs2;
    for (k = 0; k < 5; k++) {
        if (recs[k].key == 0) {
            goto missed;
        }
        if ((recs[k].key & 0xFFFF0000) == 0x10000) {
            found = 1;
            goto scanned;
        }
    }
missed:
    found = 0;
scanned:
    if (found != 0 && enemy->hp > 0 &&
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F8, (s32)&D_actor_403200_8015FA00, 0) == 0) {
        target          = gameGetPtrSlot(3);
        reply           = Gp_DispatchMsg(target, 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 4), 0);
        work->field_ECA = reply;
        if (reply == 1) {
            ((GameActor*)task->work)->field_956 = 0xA;
        }
        work->field_EB0.field_0 = D_actor_403200_8015E6AC;
        work->field_EC8         = 1;
        work->field_EB0.field_4 = 2;
        work->field_EB0.field_8 = 0;
        work->field_EB0.field_C = 0;
        Gp_DispatchMsg(task, 0x3FF, (s32)&work->field_EB0, 0);
        work->field_7CA = 0;
    }

    if (work->field_6 == 0x3C && work->field_7B3 == 4) {
        work->field_7B3 = 5;
        work->field_7B0 = 1;
    }

    if (work->field_6 >= 0x39) {
        if (D_actor_403200_80141C58 >= 0xBB9) {
            D_actor_403200_80141C58 = (u16)D_actor_403200_80141C58 - 0xC8;
        } else {
            D_actor_403200_80141C58 = (u16)D_actor_403200_80141C58 - 0x1E;
        }
    }

    if (work->field_6 >= 0x15) {
        work->field_F06 = 4;
    }

    SCRATCH_SP += 0x30;
}

/// State-change reset for the enemy's stand-up, and the height servo that runs
/// on every tick afterwards. The reset half is `func_actor_403200_8013B23C`'s
/// with the buffer allocator on the second walk in place of the release: it
/// clears the host model's flag word, walks the seven escorts pushing that word
/// onto each of their models, allocates the host's and every escort's buffers
/// and then parks the root coordinate at x 0x1068, y 0x7D0, z -0x1770, arming
/// `field_E96` at 0xFA0.
///
/// The servo steps that root y by +0x50 / -0x64 while `field_6` is at or past
/// 0x3D, and by the gentler +0x14 / -0x1E while it is between 0x15 and 0x3D, so
/// the enemy eases back to the ground as it finishes standing up; below 0x15 it
/// stops moving. Which way each step goes is the frame's position inside its
/// group of four -- `frame % 4 < 2` on the `s16` local, whose 16-bit
/// truncation is what puts the `sll 16` / `sra 16` pair in front of the `slti`.
void func_actor_403200_8013D78C(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    s16              i;
    s16              j;
    s16              frame;

    work = (Actor403200Work*)arg0->work;
    if (work->field_4 != 0) {
        work->field_7B3                  = 1;
        work->field_7B0                  = 2;
        escorts                          = (Actor403200Work*)arg0->work;
        escorts->field_7F3               = 0;
        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags =
                    ((TmdObject*)arg0->extra)->flags;
            }
        }
        dying = (Actor403200Work*)arg0->work;
        Tmd_AllocBuffers((TmdObject*)arg0->extra);
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
                Tmd_AllocBuffers((TmdObject*)dying->field_ECC[j]->task->extra);
            }
        }
        ((TmdObject*)arg0->extra)->coords->coord.t[1] = 0x7D0;
        ((TmdObject*)arg0->extra)->coords->coord.t[0] = 0x1068;
        ((TmdObject*)arg0->extra)->coords->coord.t[2] = -0x1770;
        work->field_E96                               = 0xFA0;
    }
    func_actor_403200_80133DD8(arg0);
    if (work->field_58 & 1) {
        work->field_0 = 1;
    }
    if (((TmdObject*)arg0->extra)->coords->coord.t[1] > 0) {
        frame = work->field_6;
        if (frame >= 0x3D) {
            ((TmdObject*)arg0->extra)->coords->coord.t[1] +=
                ((frame % 4) < 2) ? 0x50 : -0x64;
        } else if (frame >= 0x15) {
            ((TmdObject*)arg0->extra)->coords->coord.t[1] +=
                ((frame % 4) < 2) ? 0x14 : -0x1E;
        }
    }
    if (((TmdObject*)arg0->extra)->coords->coord.t[1] < 0) {
        ((TmdObject*)arg0->extra)->coords->coord.t[1] = 0;
    }
    ((TmdObject*)arg0->extra)->coords->flg = 0;
}

/// State-change reset for the enemy's stand-up. It clears the host model's flag
/// word, walks the seven escorts pushing that word onto each of their models,
/// allocates every escort's model buffers and then arms the block -- `field_EF6`
/// and `field_EF4` at 1, `field_EFA` at 0, `field_E96` at 0xC80 -- before
/// playing the type-6 cue built from the spawn record's `field_8`. Same shape as
/// `func_actor_403200_8013B23C`'s reset half, with the buffer allocator on the
/// second walk in place of the release.
///
/// The state then writes its two cue frames, and the three states at 0x39, 0x45
/// and 0x4C spawn `field_EF0` from `D_actor_403200_8015E858`; every other state
/// in the 0x39..0x4C window falls through to the dispatcher.
///
/// The `state` copy is what keeps the switch index 16-bit: switched on
/// `field_6 - 0x39` directly the index is an `int`, and the `lh` the load
/// becomes carries the sign extension the original does with a separate
/// `sll`/`sra` pair (dropping 2 instructions and 2.8% of the match).
void func_actor_403200_8013D9EC(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    GpEnemy*         enemy;
    GpEnemy*         spawned;
    s16              i;
    s16              j;
    s16              state;
    s32              sfx;
    s32              pan;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        work->field_F1D                  = 6;
        work->field_7B3                  = 6;
        work->field_7B0                  = 2;
        escorts                          = (Actor403200Work*)arg0->work;
        escorts->field_7F3               = 0;
        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags =
                    ((TmdObject*)arg0->extra)->flags;
            }
        }
        dying = (Actor403200Work*)arg0->work;
        Tmd_AllocBuffers((TmdObject*)arg0->extra);
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
                Tmd_AllocBuffers((TmdObject*)dying->field_ECC[j]->task->extra);
            }
        }
        work->field_EF6 = 1;
        work->field_EF4 = 1;
        work->field_EFA = 0;
        work->field_E96 = 0xC80;
        sfx             = (((u16)enemy->placeKey >> 12) << 8) | 0x40200017;
        pan             = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(sfx, pan,
                            (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    state = work->field_6 - 0x39;
    switch (state) {
        case 6:
            work->field_7B3 = 0xC;
            work->field_7B0 = 1;
            break;
        case 13:
            work->field_7B3 = 0xC;
            work->field_7B0 = 2;
            break;
        case 0:
        case 12:
        case 19:
            spawned           = Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 0, 0, arg0->spawnArg2);
            spawned->workType = 0x900;
            work->field_EF0   = spawned;
            break;
    }
    func_actor_403200_80133DD8(arg0);
    if (work->field_58 & 1) {
        work->field_0 = 7;
    }
    if (work->field_6 >= 0x15) {
        work->field_F06 = 3;
    }
}

/// The state that rains debris on the arena `func_actor_403200_8013D9EC` opens.
///
/// A reset request re-arms the block on animation 0xB, clears the host model's
/// flag word and pushes it onto each of the seven escorts' models, makes sure
/// the host and every escort has its model buffers allocated, and plays the
/// entry cue.
///
/// Sub-states 0x3B and 0x3C each fire a one-shot cue positioned on part 1 of
/// the first escort. From 0x3D on the state also drops debris: every third step
/// the shared scratch coordinate is rebuilt on that same part -- its rotation
/// accumulated up the parent chain, its origin carried into view space, then
/// turned a quarter turn each way so `Gfx_MatrixCol2` yields the launch
/// direction, which is normalised and scaled to 0x320 before being added to the
/// origin -- and an effect is spawned on it. Every tenth step a fresh enemy is
/// spawned from `D_actor_403200_8015E858` and remembered in `field_EF0`.
///
/// The tick then runs the per-frame body and hands over to state 0xA once the
/// second animation slot raises its flag.
void func_actor_403200_8013DC3C(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    GpEnemy*         enemy;
    GpEnemy*         spawned;
    SVECTOR          pos;
    SVECTOR*         posp;
    s16              i;
    s16              j;
    s32              resetId;
    s32              resetPan;
    s32              cueId;
    s32              cuePan;
    s32              hitId;
    s32              hitPan;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        work->field_F1D                  = 7;
        work->field_7B3                  = 0xB;
        work->field_7B0                  = 2;
        escorts                          = (Actor403200Work*)arg0->work;
        escorts->field_7F3               = 0;
        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags =
                    ((TmdObject*)arg0->extra)->flags;
            }
        }
        dying = (Actor403200Work*)arg0->work;
        Tmd_AllocBuffers((TmdObject*)arg0->extra);
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
                Tmd_AllocBuffers((TmdObject*)dying->field_ECC[j]->task->extra);
            }
        }
        work->field_EF4 = 1;
        work->field_EF6 = 1;
        work->field_EFA = 0;
        work->field_E96 = 0xC80;
        resetId         = (((u16)enemy->placeKey >> 12) << 8) | 0x40200017;
        resetPan        = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(resetId, resetPan,
                            (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }

    if (work->field_6 == 0x3B) {
        cueId  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200016;
        cuePan = (s8)Gp_GetObjPan(
            &((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]);
        SndEvt_EnqueueType6(
            cueId, cuePan,
            (s8)gpGetObjDepth(
                &((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]));
    }

    if (work->field_6 == 0x3C) {
        hitId  = (((u16)enemy->placeKey >> 12) << 8) | 0x4020000D;
        hitPan = (s8)Gp_GetObjPan(
            &((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]);
        SndEvt_EnqueueType6(
            hitId, hitPan,
            (s8)gpGetObjDepth(
                &((TmdObject*)work->field_ECC[0]->task->extra)->coords[1]));
    }

    if ((s16)(u16)work->field_6 >= 0x3D) {
        if ((s16)((s16)(u16)work->field_6 % 3) == 0) {
            Actor403200_AccumulateRotation(
                &((TmdObject*)work->field_ECC[0]->task->extra)->coords[1],
                &D_actor_403200_8015F924);

            pos.vz = 0;
            pos.vy = 0;
            pos.vx = 0;
            Actor403200_LocalToView(&((TmdObject*)work->field_ECC[0]->task->extra)->coords[1],
                                    &pos);

            D_actor_403200_8015F920.sub        = &gGfxViewCoord;
            D_actor_403200_8015F920.coord.t[0] = pos.vx;
            D_actor_403200_8015F920.coord.t[1] = pos.vy;
            D_actor_403200_8015F920.coord.t[2] = pos.vz;
            Gfx_RotMatrixY(&D_actor_403200_8015F920.coord, 0x80, 0);
            Gfx_RotMatrixX(&D_actor_403200_8015F920.coord, -0x80, 0);
            Gfx_MatrixCol2(&D_actor_403200_8015F920.coord, &pos);

            posp   = &pos;
            pos.vy = 0;
            VectorNormalSS(posp, posp);

            gte_lddp(0x320);
            gte_ldsv(posp);
            gte_gpf12_real();
            gte_stsv(posp);

            D_actor_403200_8015F920.coord.t[0] += pos.vx;
            D_actor_403200_8015F920.coord.t[1] += pos.vy;
            D_actor_403200_8015F920.coord.t[2] += pos.vz;
            D_actor_403200_8015F920.flg         = 0;
            Gp_UpdateCoord(&D_actor_403200_8015F920);
            Gp_SpawnEff(0x60199, &D_actor_403200_8015F920, 0x97A0D680, NULL);
        }
        if ((s16)((s16)(u16)work->field_6 % 10) == 4) {
            spawned           = Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 2, 0, arg0->spawnArg2);
            spawned->workType = 0x900;
            work->field_EF0   = spawned;
        }
    }

    func_actor_403200_80133DD8(arg0);

    if (work->field_58 & 1) {
        work->field_0 = 0xA;
        SndEvt_EnqueueType7((((u16)enemy->placeKey >> 12) << 8) | 0x4020000D, 1);
    }

    if (work->field_6 >= 0x15) {
        work->field_F06 = 5;
    }
}

void func_actor_403200_8013E2FC(Task* arg0)
{
    Actor403200Work*   work;
    Actor403200Work*   escorts;
    Actor403200Work*   dying;
    Actor403200Matrix* mtx;
    GsCOORDINATE2*     coords;
    s32                state;
    s32                frame;
    s16                i;
    s16                j;

    work = (Actor403200Work*)arg0->work;
    if (work->field_4 != 0) {
        escorts                          = (Actor403200Work*)arg0->work;
        work->field_7F3                  = 0;
        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags =
                    ((TmdObject*)arg0->extra)->flags;
            }
        }
        dying = (Actor403200Work*)arg0->work;
        Tmd_AllocBuffers((TmdObject*)arg0->extra);
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
                Tmd_AllocBuffers((TmdObject*)dying->field_ECC[j]->task->extra);
            }
        }
        work->field_7B6 = 0x10;
        work->field_F04 = 1;
        work->field_7A4 = 0;
        work->field_E96 = 0xFA0;
    }
    if (work->field_7B3 == 0xD) {
        frame = work->field_72 & 0x3FF;
        if (frame == 0x15 && work->field_7D8 != frame) {
            work->field_EAC = 3;
            Gp_SpawnScript18((s32)&D_actor_403200_80141C6C, (s32)&D_actor_403200_80141C74);
        }
        work->field_7D8 = work->field_72 & 0x3FF;
    }
    if (work->field_7B3 == 9 && work->field_6 == 0x2D) {
        coords                                = ((TmdObject*)arg0->extra)->coords;
        D_actor_403200_8015F970.ident.m00_m01 = 0x1000;
        mtx                                   = (Actor403200Matrix*)&D_actor_403200_8015F970.c.coord;
        mtx->ident.m02_m10                    = 0;
        mtx->ident.m11_m12                    = 0x1000;
        mtx->ident.m20_m21                    = 0;
        mtx->ident.m22                        = 0x1000;
        D_actor_403200_8015F970.c.coord.t[1]  = -0x64;
        D_actor_403200_8015F970.c.coord.t[0]  = 0;
        D_actor_403200_8015F970.c.coord.t[2]  = 0x64;
        D_actor_403200_8015F970.c.flg         = 0;
        D_actor_403200_8015F970.c.sub         = &coords[4];
        Gp_UpdateCoord(&D_actor_403200_8015F970.c);
    }
    state = work->field_7B3;
    if (state == 0x14) {
        if (work->field_58 & 1) {
            work->field_7B3 = 0xD;
            work->field_7B0 = 1;
        }
        if (work->field_7B3 == state && work->field_7B0 == 2) {
            work->field_7B6 = 0x60;
            func_actor_403200_80133DD8(arg0);
            while ((u32)(work->field_4A & 0x3FF) < 0x34) {
                func_actor_403200_80133DD8(arg0);
            }
            work->field_7B6 = 0x10;
        }
    }
    if (D_actor_403200_80141C58 >= 0x191) {
        D_actor_403200_80141C58 = (u16)D_actor_403200_80141C58 - 0xC8;
    }
    func_actor_403200_80133DD8(arg0);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
}

/// State 0x12, the enemy's death sequence: the model is torn down and rebuilt
/// so the collapse animation can run on it.
///
/// A reset request clears the host model's `field_C` and pushes the cleared
/// word onto each of the seven escorts' own model objects, allocates the host's
/// and every escort's model buffers, forces `field_F14 / 4` extra per-frame
/// steps -- stopping early once `field_58` bit 0 is set -- and then re-arms the
/// animation slot at 0x10, plays the type-7 death cue and leaves the yaw target
/// at 0xFA0 and the escort pose cleared.
///
/// The rest of the tick winds the shared `D_actor_403200_80141C58` counter down
/// by 0xC8 once it has passed 0x1F4, runs the per-frame body, clears the host
/// coordinate's rebuild flag, and on frame 0x1C of `field_72` arms the screen
/// shake at level 3 and spawns the `D_actor_403200_80141C5C` script pair. While
/// `field_7B3` is still 0x12 four one-shot cues fire on frames 0x33, 0x3D, 0x4E
/// and 0x71 of `field_9A`, each latching the frame it saw in `field_7A8`.
void func_actor_403200_8013E5A8(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    GpEnemy*         enemy;
    GpEnemy*         obj;
    s16              i;
    s16              j;
    s32              state;
    s32              frame;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        obj                              = arg0->spawnArg2;
        escorts                          = (Actor403200Work*)arg0->work;
        work->field_7F3                  = 0;
        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags =
                    ((TmdObject*)arg0->extra)->flags;
            }
        }
        dying = (Actor403200Work*)arg0->work;
        Tmd_AllocBuffers((TmdObject*)arg0->extra);
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
                Tmd_AllocBuffers((TmdObject*)dying->field_ECC[j]->task->extra);
            }
        }
        work->field_7B6 = 0x40;
        work->field_EF4 = 0;
        work->field_EF6 = 0;
        j               = 0;
        while (j < work->field_F14 / 4) {
            func_actor_403200_80133DD8(arg0);
            j++;
            if (work->field_58 & 1) {
                break;
            }
        }
        work->field_F06 = 7;
        work->field_7B6 = 0x10;
        SndEvt_EnqueueType7((((u16)obj->placeKey >> 12) << 8) | 0x4020000A, 1);
        work->field_7A4 = 0;
        work->field_E96 = 0xFA0;
    }
    if (D_actor_403200_80141C58 >= 0x1F5) {
        D_actor_403200_80141C58 = (u16)D_actor_403200_80141C58 - 0xC8;
    }
    func_actor_403200_80133DD8(arg0);
    ((TmdObject*)arg0->extra)->coords->flg = 0;
    state                                  = work->field_72 & 0x3FF;
    if (state == 0x1C && work->field_7D8 != state) {
        work->field_EAC = 3;
        Gp_SpawnScript18((s32)&D_actor_403200_80141C5C, (s32)&D_actor_403200_80141C64);
    }
    work->field_7D8 = work->field_72 & 0x3FF;
    if (work->field_7B3 == 0x12) {
        frame = work->field_9A & 0x3FF;
        if (frame == 0x33 && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200013;
            pan = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
            SndEvt_EnqueueType6(id, pan,
                                (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        }
        frame = work->field_9A & 0x3FF;
        if (frame == 0x3D && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200003;
            pan = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
            SndEvt_EnqueueType6(id, pan,
                                (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        }
        frame = work->field_9A & 0x3FF;
        if (frame == 0x4E && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200014;
            pan = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
            SndEvt_EnqueueType6(id, pan,
                                (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        }
        frame = work->field_9A & 0x3FF;
        if (frame == 0x71 && work->field_7A8 != frame) {
            s32 id;
            s32 pan;

            id  = (((u16)enemy->placeKey >> 12) << 8) | 0x40200015;
            pan = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
            SndEvt_EnqueueType6(id, pan,
                                (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        }
        work->field_7A8 = work->field_9A & 0x3FF;
    }
}

/// The enemy's attack-launch body: when the dispatcher has flagged the state
/// change it re-arms the work block (`field_7A4` at 3, `field_E96` at 0xC80) and
/// plays the two launch cues -- a type-6 with the object's pan and depth, then
/// type-7s for ids 0x0D and 0x09 -- and otherwise runs the per-frame body,
/// winding the shared `D_actor_403200_80141C58` counter down by 0xC8 once it has
/// passed 0x190 and clearing `field_F06` once `field_6` has passed 0x14.
void func_actor_403200_8013E9C0(Task* arg0)
{
    Actor403200Work* work;
    GpEnemy*         obj;
    s32              state;
    s32              id;
    s32              pan;

    work = (Actor403200Work*)arg0->work;
    if (work->field_4 != 0) {
        obj             = arg0->spawnArg2;
        state           = work->field_7B3;
        work->field_EF4 = 0;
        work->field_EF6 = 0;
        work->field_EFA = 1;
        if (state != 0xD) {
            work->field_7B0 = 1;
            work->field_7B3 = 0xD;
        } else {
            work->field_7B0 = 2;
            work->field_7B3 = state;
        }
        id  = (((u16)obj->placeKey >> 12) << 8) | 0x40200004;
        pan = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(id, pan,
                            (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        work->field_7A4 = 3;
        work->field_E96 = 0xC80;
        SndEvt_EnqueueType7((((u16)obj->placeKey >> 12) << 8) | 0x4020000D, 1);
        SndEvt_EnqueueType7((((u16)obj->placeKey >> 12) << 8) | 0x40200009, 1);
        return;
    }
    SCRATCH_SP -= 0xC;
    func_actor_403200_80133DD8(arg0);
    if (D_actor_403200_80141C58 >= 0x191) {
        D_actor_403200_80141C58 = (u16)D_actor_403200_80141C58 - 0xC8;
        work->field_7A4         = 0;
    }
    if (work->field_58 & 1) {
        work->field_0 = 0xA;
    }
    if (work->field_6 >= 0x15) {
        work->field_F06 = 0;
    }
    SCRATCH_SP += 0xC;
}

/// State-selecting tick of the enemy's approach: on the tick the dispatcher has
/// flagged a state change it re-arms the work block -- the two flags, the
/// stagger countdown at 0x28, the yaw target at 0xE10 and the animation slot at
/// 0x10 -- and winds the shared `D_actor_403200_80141C58` counter down by 0xC8
/// once it has passed 0x190.
///
/// It then runs the per-frame body and aims the enemy at the camera: the
/// camera's translation minus the part's own translation gives the pair
/// `ratan2` turns into a yaw, taken relative to the part's facing the same way
/// the group-0 hit handler does it, and the result is wrapped to +/-0x800 into
/// `field_7C4`. `D_actor_403200_80141C50` holding `field_6` at zero makes the
/// per-frame body's animation re-arm win the next tick.
///
/// Once the `field_F10` stagger countdown has run out it walks the three
/// `field_F08` sub-states, in which the player-relative range and the enemy's
/// remaining HP pick the next state, and a roll of `Gp_LcgState` breaks the tie
/// between the two strafing states; the state already in `field_F1D` is never
/// re-selected twice in a row. A positive heal counter in `field_F1A` overrides
/// all of it with the heal state 0xF.
///
/// The x range that sub-state 0 tests is the player-relative offset read back
/// out of the frame, not `dist`: the two share only the frame, and the y test
/// carries the -0xFA the z one carries +0x25F, the offsets the hit handler puts
/// on the same pair.
void func_actor_403200_8013EB64(Task* arg0)
{
    Actor403200ApproachScratch* sc;
    Actor403200Work*            work;
    GpEnemy*                    enemy;
    Task*                       player;
    GsCOORDINATE2*              coord;
    GsCOORDINATE2*              facing;
    SVECTOR*                    view;
    s16                         angle;

    work   = (Actor403200Work*)arg0->work;
    enemy  = arg0->spawnArg2;
    player = gameGetPtrSlot(3);

    if (work->field_4 != 0) {
        work->field_EF6 = 1;
        work->field_EF4 = 1;
        work->field_EFE = 0;
        if (work->field_F10 == 0) {
            work->field_F10 = 0x28;
        }
        work->field_E96 = 0xE10;
        work->field_7B3 = 1;
        work->field_7B0 = 1;
        work->field_EFA = 0;
        work->field_7B6 = 0x10;
    }
    if (D_actor_403200_80141C58 >= 0x191) {
        D_actor_403200_80141C58 = (u16)D_actor_403200_80141C58 - 0xC8;
        work->field_7A4         = 0;
    }
    sc = (Actor403200ApproachScratch*)(SCRATCH_SP -= sizeof(Actor403200ApproachScratch));
    func_actor_403200_80133DD8(arg0);

    coord    = ((TmdObject*)arg0->extra)->coords;
    view     = &sc->view;
    view->vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    view->vy = Player_Status.coordMtx->t[1] - coord->coord.t[1];
    view->vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    facing   = ((TmdObject*)arg0->extra)->coords;
    angle    = ratan2(view->vx, view->vz) -
            ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    if (angle < 0) {
    wrapUp:
        if (angle < -0x800) {
            angle += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto wrapDown;
        }
    }
    work->field_7C4 = angle;
    if (D_actor_403200_80141C50 == 1) {
        work->field_6 = 0;
    }
    if (work->field_F10 <= work->field_6) {
        sc->delta.vx = ((TmdObject*)player->extra)->coords->coord.t[0] -
                       ((TmdObject*)arg0->extra)->coords->coord.t[0];
        sc->delta.vy = (((TmdObject*)player->extra)->coords->coord.t[1] -
                        ((TmdObject*)arg0->extra)->coords->coord.t[1]) -
                       0xFA;
        sc->delta.vz = (((TmdObject*)player->extra)->coords->coord.t[2] -
                        ((TmdObject*)arg0->extra)->coords->coord.t[2]) +
                       0x25F;
        sc->dist = SquareRoot0(sc->delta.vx * sc->delta.vx + sc->delta.vy * sc->delta.vy +
                               sc->delta.vz * sc->delta.vz);
        switch (work->field_F08) {
            case 0:
                if (enemy->hp < 0x5DC) {
                    work->field_0 = 9;
                } else if (((TmdObject*)player->extra)->coords->coord.t[0] -
                               ((TmdObject*)arg0->extra)->coords->coord.t[0] >=
                           0x2711) {
                    work->field_0 = 2;
                } else if (work->field_F1D != 3) {
                    work->field_0 = 3;
                } else {
                    work->field_0 = 2;
                }
                break;
            case 1:
                if (enemy->hp < 0x320) {
                    work->field_0 = 9;
                } else {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((((u32)Gp_LcgState >> 16) & 0xF) == 0) {
                        work->field_0 = 3;
                    } else if (sc->dist >= 0x20D1) {
                        if (work->field_F1D == 6) {
                            work->field_0 = 2;
                        } else {
                            work->field_0 = 6;
                        }
                    } else if (sc->dist >= 0x189D) {
                        if (work->field_F1D == 7) {
                            work->field_0 = 2;
                        } else {
                            work->field_0 = 7;
                        }
                    } else {
                        work->field_0 = 2;
                    }
                }
                break;
            case 2:
                if (sc->dist >= 0x2329) {
                    if (work->field_F1D == 2) {
                        work->field_0 = 6;
                    } else {
                        work->field_0 = 2;
                    }
                } else if (work->field_F1D == 2) {
                    work->field_0 = 0xB;
                } else {
                    work->field_0 = 2;
                }
                break;
        }
        if ((s8)work->field_F1A > 0) {
            work->field_0 = 0xF;
        }
    }
    SCRATCH_SP += sizeof(Actor403200ApproachScratch);
}

/// Escort-spawn tick of the arena fight. While `D_actor_403200_80141C50` is 1
/// the whole body is skipped; otherwise it carves an
/// `Actor403200SpawnScratch` off `SCRATCH_SP`.
///
/// On the dispatcher's re-arm tick it tops the two `field_EE8` slots back up
/// to two live escorts (`field_F1C` < 2 and `field_F1B` < 8), dresses each
/// model from the current area record's fourth placement, stamps the slot
/// index into `GpEnemy::placeKey`, and plays the two type-7 launch cues.
/// Every later tick yaws the host at the player, and at `field_6` 0x46 / 0x78
/// it sends escort 0 or 1 a 0x7DB order whose action is picked from
/// `field_F08` and a coin flip of `Gp_LcgState`.
void func_actor_403200_8013EF6C(Task* arg0)
{
    Actor403200SpawnScratch* sc;
    Actor403200Work*         work;
    GpEnemy*                 host;
    GpEnemy*                 escort;
    PlayerStatus*            cfg;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           facing;
    TmdObject*               model;
    GpAreaPlace*             entry;
    GpAreaKey                key;
    GpAreaKey*               sessionKey;
    s32                      cueId;
    s32                      cuePan;
    s32                      blastId;
    s32                      blastPan;
    s32                      rnd;
    s32                      state;
    s16                      angle;
    s16                      sel;
    u32                      frame;

    work = (Actor403200Work*)arg0->work;
    host = arg0->spawnArg2;
    if (D_actor_403200_80141C50 != 1) {
        sc = (Actor403200SpawnScratch*)(SCRATCH_SP -= sizeof(Actor403200SpawnScratch));
        if (work->field_4 != 0) {
            state           = work->field_7B3;
            work->field_EF4 = 0;
            work->field_EF6 = 1;
            work->field_EFA = 0;
            if (state != 0x13) {
                work->field_7B3 = 0x13;
                work->field_7B0 = 1;
            } else {
                work->field_7B0 = 2;
                work->field_7B3 = state;
            }
            work->field_7B6 = 0x10;
            for (sc->i = 0; sc->i < 2; sc->i++) {
                if (work->field_EE8[sc->i] == NULL && work->field_F1C < 2 && (u8)work->field_F1B < 8) {
                    work->field_EE8[sc->i] = Gp_SpawnEnemyFromTable(&D_80174D58, 3, 2, NULL);
                    if (work->field_EE8[sc->i] != NULL) {
                        work->field_F1B++;
                        model      = (TmdObject*)work->field_EE8[sc->i]->task->extra;
                        sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
                        key.stage  = sessionKey->stage;
                        key.area   = sessionKey->area;
                        key.room   = sessionKey->room;
                        key.view   = sessionKey->view;
                        Gp_SyncAreaKeyIndex(&key);
                        entry        = (GpAreaPlace*)((s32)Gp_GetNestedAreaRec(&key)->field_0 + 0x30);
                        model->tpage = entry->tpage;
                        model->clut  = entry->clut;
                        if (model->buffer != NULL) {
                            tmdProcessStream(model);
                            tmdProcessStream(model);
                        }
                        work->field_EE8[sc->i]->workType = 0x900;
                        escort                           = work->field_EE8[sc->i];
                        escort->placeKey                |= sc->i << 12;
                        work->field_F1C++;
                    }
                }
            }
            work->field_E96 = 0xC80;
            SndEvt_EnqueueType7((((u16)host->placeKey >> 12) << 8) | 0x4020000D, 1);
            SndEvt_EnqueueType7((((u16)host->placeKey >> 12) << 8) | 0x40200009, 1);
        }
        if (D_actor_403200_80141C58 >= 0x191) {
            D_actor_403200_80141C58 = (u16)D_actor_403200_80141C58 - 0xC8;
        }
        func_actor_403200_80133DD8(arg0);
        if (work->field_7B3 == 0x13 && (frame = work->field_4A & 0x3FF) >= 4 && frame < 0xD) {
            work->field_EFA = 1;
        } else {
            work->field_EFA = 0;
        }
        cfg          = &Player_Status;
        coord        = ((TmdObject*)arg0->extra)->coords;
        sc->delta.vx = cfg->coordMtx->t[0] - coord->coord.t[0];
        sc->delta.vy = cfg->coordMtx->t[1] - coord->coord.t[1];
        sc->delta.vz = cfg->coordMtx->t[2] - coord->coord.t[2];
        facing       = ((TmdObject*)arg0->extra)->coords;
        angle        = ratan2(sc->delta.vx, sc->delta.vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        if (angle < 0) {
        wrapUp:
            if (angle < -0x800) {
                angle += 0x1000;
                goto wrapUp;
            }
        } else {
        wrapDown:
            if (angle > 0x800) {
                angle -= 0x1000;
                goto wrapDown;
            }
        }
        work->field_7C4 = angle;
        if ((work->field_58 & 1) && work->field_7B3 == 0x13) {
            work->field_7B3 = 1;
            work->field_7B0 = 1;
        }
        if (work->field_6 >= 0x14B || (work->field_7B3 == 1 && D_801153F4[1] == 1)) {
            work->field_0 = 3;
        }
        if (work->field_6 == 6) {
            cueId  = (((u16)host->placeKey >> 12) << 8) | 0x40200004;
            cuePan = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
            SndEvt_EnqueueType6(cueId, cuePan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
        }
        if (work->field_6 == 0x3B) {
            blastId  = (((u16)host->placeKey >> 12) << 8) | 0x40200010;
            blastPan = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
            SndEvt_EnqueueType6(blastId, blastPan, (s8)(gpGetObjDepth(((TmdObject*)arg0->extra)->coords) / 2));
            work->field_EAC = 3;
            Gp_SpawnScript18((s32)&D_actor_403200_80141C5C, (s32)&D_actor_403200_80141C64);
        }
        if (work->field_6 == 0x23) {
            work->field_F06 = 0;
        }
        if (work->field_6 == 0x46) {
            sc->i = 0;
            goto dispatch;
        }
        if (work->field_6 == 0x78) {
            sc->i = 1;
        dispatch:
            if (work->field_EE8[sc->i] != NULL) {
                D_actor_403200_8015F8F4.field_0 = 0;
                D_actor_403200_8015F8F4.field_1 = 0x2C;
                sel                             = work->field_F08;
                if (sel == 1) {
                    goto L_case1;
                }
                if (sel >= 2) {
                    goto L_default;
                }
                if (sel != 0) {
                    goto L_default;
                }
                if (sc->i == 0) {
                    Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
                    if (!(((u32)Gp_LcgState >> 16) & 1)) {
                        D_actor_403200_8015F8F4.field_2 = 3;
                    } else {
                        D_actor_403200_8015F8F4.field_2 = 4;
                    }
                } else {
                    Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
                    if (!(((u32)Gp_LcgState >> 16) & 1)) {
                        D_actor_403200_8015F8F4.field_2 = 9;
                    } else {
                        D_actor_403200_8015F8F4.field_2 = 0xA;
                    }
                }
                goto L_join;
            L_case1:
                if (sc->i == 0) {
                    Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
                    if (!(((u32)Gp_LcgState >> 16) & 1)) {
                        D_actor_403200_8015F8F4.field_2 = 0xA;
                    } else {
                        D_actor_403200_8015F8F4.field_2 = 0xB;
                    }
                } else {
                    Gp_LcgState = (Gp_LcgState * 5) + 0x71357911;
                    if (!(((u32)Gp_LcgState >> 16) & 1)) {
                        D_actor_403200_8015F8F4.field_2 = 4;
                    } else {
                        D_actor_403200_8015F8F4.field_2 = 5;
                    }
                }
                goto L_join;
            L_default:
                if (sc->i == 0) {
                    D_actor_403200_8015F8F6 = 5;
                } else {
                    D_actor_403200_8015F8F6 = 0xB;
                }
            L_join:
                D_actor_403200_8015F8F4.field_2  = (u16)D_actor_403200_8015F8F4.field_2 << 8;
                rnd                              = (Gp_LcgState * 5) + 0x71357911;
                D_actor_403200_8015F8F4.field_2 |= ((((u32)rnd >> 16) % 3) * 0x10) | 1;
                Gp_LcgState                      = rnd;
                Gp_DispatchMsg(work->field_EE8[sc->i]->task, 0x7DB, (s32)&D_actor_403200_8015F8F4, 0);
            }
        }
    out:
        SCRATCH_SP += sizeof(Actor403200SpawnScratch);
    }
}

void func_actor_403200_8013F700(Task* arg0)
{
    Actor403200Work* work;
    Actor403200Work* escorts;
    Actor403200Work* dying;
    GpEnemy*         enemy;
    GpEnemy*         spawned;
    GpAreaKey*       sessionKey;
    GpAreaKey*       keyp;
    u8               areaByte0;
    SVECTOR          vec;
    SVECTOR*         v;
    GpAreaKey        key;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   rot;
    GpAreaPlace*     entry;
    TmdObject*       model;
    s16              i;
    s16              j;
    s16              angle;
    s32              sfx;
    s32              pan;

    work  = (Actor403200Work*)arg0->work;
    enemy = arg0->spawnArg2;
    if (work->field_4 != 0) {
        work->field_7B3                  = 0xE;
        work->field_7B0                  = 1;
        escorts                          = (Actor403200Work*)arg0->work;
        escorts->field_7F3               = 0;
        ((TmdObject*)arg0->extra)->flags = 0;
        for (i = 0; i < 7; i++) {
            if (escorts->field_ECC[i] != NULL) {
                ((TmdObject*)escorts->field_ECC[i]->task->extra)->flags =
                    ((TmdObject*)arg0->extra)->flags;
            }
        }
        dying = (Actor403200Work*)arg0->work;
        Tmd_AllocBuffers((TmdObject*)arg0->extra);
        for (j = 0; j < 7; j++) {
            if (dying->field_ECC[j] != NULL) {
                Tmd_AllocBuffers((TmdObject*)dying->field_ECC[j]->task->extra);
            }
        }
        work->field_EF6 = 1;
        work->field_EF4 = 0;
        work->field_EFA = 0;
        work->field_EFE = 0;
        work->field_E96 = 0xC80;
    }
    switch (work->field_6) {
        case 0x64:
        case 0x104:
            if ((s8)work->field_F1A > 0) {
                work->field_7B3 = 0x10;
                work->field_7B0 = 1;
                work->field_EF4 = 1;
                work->field_F1A--;
            } else {
                work->field_0   = 0xA;
                work->field_EFE = 0;
            }
            break;
        case 0x74:
            sfx = (((u16)enemy->placeKey >> 12) << 8) | 0x40200017;
            pan = (s8)Gp_GetObjPan((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords);
            SndEvt_EnqueueType6(
                sfx, pan, (s8)gpGetObjDepth((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords));
            break;
        case 0x1A4:
            work->field_0   = 0xA;
            work->field_EFE = 0;
            work->field_F1A = 0;
            break;
        case 0x9B:
        case 0x113:
            work->field_EFE = 0x80;
            break;
        case 0xAF:
        case 0x145:
            spawned           = Gp_SpawnEnemyFromTable(&D_actor_403200_8015E858, 3, 0, arg0->spawnArg2);
            spawned->workType = 0x900;
            work->field_EF0   = spawned;
            if (spawned != NULL) {
                model      = (TmdObject*)spawned->task->extra;
                sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
                key.stage  = sessionKey->stage;
                key.area   = sessionKey->area;
                key.room   = sessionKey->room;
                areaByte0  = sessionKey->view;
                SOFT_BARRIER();
                keyp = &key;
                TOUCH_REG(keyp);
                key.view = areaByte0;
                Gp_SyncAreaKeyIndex(keyp);
                entry        = (GpAreaPlace*)(0x20 + (s32)Gp_GetNestedAreaRec(&key)->field_0);
                model->tpage = entry->tpage;
                model->clut  = entry->clut;
                if (model->buffer != NULL) {
                    tmdProcessStream(model);
                    tmdProcessStream(model);
                }
                work->field_EFE = 0;
            }
            break;
    }
    coord = ((TmdObject*)arg0->extra)->coords;
    v     = &vec;
    v->vx = D_80073B8C->t[0] - coord->coord.t[0];
    v->vy = D_80073B8C->t[1] - coord->coord.t[1];
    v->vz = D_80073B8C->t[2] - coord->coord.t[2];
    rot   = ((TmdObject*)arg0->extra)->coords;
    angle = ratan2(v->vx, v->vz) - ratan2(-rot->coord.m[2][0], rot->coord.m[2][2]);
    if (angle < 0) {
    wrapUp:
        if (angle < -0x800) {
            angle += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto wrapDown;
        }
    }
    work->field_7C4 = angle;
    func_actor_403200_80133DD8(arg0);
    if (work->field_7B3 == 0x10 && (work->field_58 & 1)) {
        work->field_7B3 = 0xE;
        work->field_7B0 = 1;
    }
    if (work->field_6 >= 0x15) {
        work->field_F06 = 3;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_4", func_actor_403200_8013FB54);

void func_actor_403200_801408D8(Task* task, s16 scale, s16 drop, s16 index)
{
    SVECTOR     dir;
    SVECTOR     normal;
    SVECTOR*    pool  = Gp_GridParams->field_4;
    SVECTOR*    verts = Gp_GridParams->field_8;
    GpGridFace* faces = Gp_GridParams->field_C;
    GpGridFace  face  = {
        { index * 4, index * 4 + 1, index * 4 + 2, index * 4 + 3 }, index, 3
    };
    GpGridFace face2 = {
        { (index + 1) * 4, (index + 1) * 4 + 1, (index + 1) * 4 + 2, (index + 1) * 4 + 3 }, index + 1, 3
    };
    SVECTOR* d;

    Gfx_MatrixCol2(&((TmdObject*)task->extra)->coords->coord, &normal);
    Gfx_MatrixCol0(&((TmdObject*)task->extra)->coords->coord, &dir);
    d = &dir;
    VectorNormalSS(d, d);
    VectorNormalSS(&normal, &normal);
    gte_lddp(scale);
    gte_ldsv(&normal);
    gte_gpf12_real();
    gte_stsv(&normal);
    gte_lddp(0xBB8);
    gte_ldsv(d);
    gte_gpf12_real();
    gte_stsv(d);

    verts[index * 4].vx = verts[index * 4 + 2].vx =
        ((TmdObject*)task->extra)->coords->coord.t[0] + dir.vx + normal.vx;
    verts[index * 4].vy = verts[index * 4 + 2].vy =
        ((TmdObject*)task->extra)->coords->coord.t[1] + dir.vy + normal.vy;
    verts[index * 4].vz = verts[index * 4 + 2].vz =
        ((TmdObject*)task->extra)->coords->coord.t[2] + dir.vz + normal.vz;

    verts[index * 4 + 1].vx = verts[index * 4 + 3].vx =
        ((TmdObject*)task->extra)->coords->coord.t[0] + normal.vx;
    verts[index * 4 + 1].vy = verts[index * 4 + 3].vy =
        ((TmdObject*)task->extra)->coords->coord.t[1] + normal.vy;
    verts[index * 4 + 1].vz = verts[index * 4 + 3].vz =
        ((TmdObject*)task->extra)->coords->coord.t[2] + normal.vz;

    verts[(index + 1) * 4].vx = verts[(index + 1) * 4 + 2].vx =
        ((TmdObject*)task->extra)->coords->coord.t[0] + normal.vx;
    verts[(index + 1) * 4].vy = verts[(index + 1) * 4 + 2].vy =
        ((TmdObject*)task->extra)->coords->coord.t[1] + normal.vy;
    verts[(index + 1) * 4].vz = verts[(index + 1) * 4 + 2].vz =
        ((TmdObject*)task->extra)->coords->coord.t[2] + normal.vz;

    verts[(index + 1) * 4 + 1].vx = verts[(index + 1) * 4 + 3].vx =
        ((TmdObject*)task->extra)->coords->coord.t[0] + normal.vx - dir.vx;
    verts[(index + 1) * 4 + 1].vy = verts[(index + 1) * 4 + 3].vy =
        ((TmdObject*)task->extra)->coords->coord.t[1] + normal.vy - dir.vy;
    verts[(index + 1) * 4 + 1].vz = verts[(index + 1) * 4 + 3].vz =
        ((TmdObject*)task->extra)->coords->coord.t[2] + normal.vz - dir.vz;

    gte_lddp(0x3E8);
    gte_ldsv(&normal);
    gte_gpf12_real();
    gte_stsv(&normal);

    verts[index * 4].vx = verts[index * 4 + 2].vx += normal.vx;
    verts[index * 4].vy = verts[index * 4 + 2].vy += normal.vy;
    verts[index * 4].vz = verts[index * 4 + 2].vz += normal.vz;
    verts[index * 4 + 5].vx = verts[index * 4 + 7].vx += normal.vx;
    verts[index * 4 + 5].vy = verts[index * 4 + 7].vy += normal.vy;
    verts[index * 4 + 5].vz = verts[index * 4 + 7].vz += normal.vz;

    pool[index].vz = verts[index * 4].vx - verts[index * 4 + 1].vx;
    pool[index].vy = verts[index * 4 + 1].vy - verts[index * 4].vy;
    pool[index].vx = verts[index * 4 + 1].vz - verts[index * 4].vz;
    VectorNormalSS(&pool[index], &pool[index]);

    (&pool[index])[1].vz = verts[(index + 1) * 4].vx - verts[(index + 1) * 4 + 1].vx;
    (&pool[index])[1].vy = verts[(index + 1) * 4 + 1].vy - verts[(index + 1) * 4].vy;
    (&pool[index])[1].vx = verts[(index + 1) * 4 + 1].vz - verts[(index + 1) * 4].vz;
    VectorNormalSS(&(&pool[index])[1], &(&pool[index])[1]);

    verts[index * 4].vy     -= drop;
    verts[index * 4 + 1].vy -= drop;
    verts[index * 4 + 4].vy -= drop;
    verts[index * 4 + 5].vy -= drop;

    face.field_A     = 3;
    face2.field_A    = 3;
    faces[index]     = face;
    faces[index + 1] = face2;
}

/// The enemy's upkeep tick, run by the dispatcher through the same
/// `D_actor_403200_801321B8` table the other tasks in this overlay use. It drops
/// each of the two escort slots whose HP has run out, then walks the work
/// block's `field_E94` toward `field_E96` by 0x32 a tick -- snapping once the
/// two are within 0x33 -- calls the follow helper with the new value, and
/// finally lifts the host's own X up to the escort's so the party never sinks
/// below the enemy. The tick ends by dispatching on `state` through the local
/// copy of the handler table.
void func_actor_403200_80140E6C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;
    Actor403200Work*      work;
    Task*                 player;
    GpEnemy*              enemy;
    s32                   diff;
    s32                   y;
    GsCOORDINATE2*        playerCoord;
    GsCOORDINATE2*        selfCoord;

    sp     = D_actor_403200_801321B8;
    player = gameGetPtrSlot(3);
    work   = (Actor403200Work*)arg0->work;
    enemy  = arg0->spawnArg2;
    if (work != NULL) {
        if (work->field_EE8[0] != NULL && work->field_EE8[0]->hp <= 0) {
            work->field_EE8[0] = NULL;
        }
        if (work->field_EE8[1] != NULL && work->field_EE8[1]->hp <= 0) {
            work->field_EE8[1] = NULL;
        }
        diff = work->field_E96 - work->field_E94;
        if (diff < 0) {
            diff = -diff;
        }
        if (diff >= 0x33) {
            if (work->field_E94 < work->field_E96) {
                work->field_E94 = (u16)work->field_E94 + 0x32;
            } else {
                work->field_E94 = (u16)work->field_E94 - 0x32;
            }
        } else {
            work->field_E94 = (u16)work->field_E96;
        }
        func_actor_403200_801408D8(arg0, work->field_E94, work->field_E98, 0);
        playerCoord = ((TmdObject*)player->extra)->coords;
        selfCoord   = ((TmdObject*)arg0->extra)->coords;
        y           = selfCoord->coord.t[0] + work->field_E94;
        if (playerCoord->coord.t[0] < y) {
            playerCoord->coord.t[0] = y;
        }
    }
    sp.funcs[arg0->state](enemy, arg0);
}

/* `migrate_rodata_to_functions` folds this run into the `.s` of
 * `func_actor_403200_80140E6C`, whose body is C here, so its bytes have to be
 * emitted in this unit: the twelve bytes that follow the table
 * `func_actor_403200_8013FB54` carries. */
#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "nonmatching D_actor_403200_801321B8\n"
        "dlabel D_actor_403200_801321B8\n"
        "    .word func_actor_403200_80138AFC\n"
        "    .word func_actor_403200_8013FB54\n"
        "    .word Gp_DestroyEnemy\n"
        "enddlabel D_actor_403200_801321B8\n"
        ".section .text");
#endif
