#include "common.h"

#include "actors/actor_356100.h"
#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include <psyq/inline_c.h>

/// `gpf 12`; the `inline_c.h` macro of that name assembles to a different word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80162258);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_801625A0);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80162AEC);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80162C90);

void func_actor_356100_801633DC(Actor356100* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    GpAnimCtx*           anim;
    s16                  weight;
    s16                  i;
    Actor356100AnimWork* work;

    work   = (Actor356100AnimWork*)arg0->field_1C;
    weight = work->field_98C;
    anim   = &work->anim;
    for (i = 1; i < 0x15; i++) {
        if (i < 0xB) {
            work->blendSlots[i].field_9 = (u8)work->field_98A;
            work->slots[i].field_9      = (u8)(work->field_982 - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].field_9 = (u8)(work->field_982 - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80163508);

/// Initialisation for the state-0x10 clip run: allocates the work block, binds
/// the model's light / colour matrices, re-seeds the enemy descriptor and both
/// animation contexts, copies the model root's XZ pair into the work block and
/// rebuilds the root's Y rotation as a uniform 0x1194 scale. The `field_36`
/// sub-type picks the clip and the spawn argument the re-entry pair, and the
/// finished entry advances the state. Same body as `Actor01900_Fn02018`.
void func_actor_356100_8016382C(GpEnemy* enemy, Actor356100* actor)
{
    SVECTOR          dir;
    SVECTOR*         v;
    VECTOR           pos;
    TmdObject*       obj;
    GsCOORDINATE2*   root;
    Actor356100Work* work;
    s32              kind;

    root            = actor->field_2C->field_8;
    obj             = actor->field_2C;
    work            = Mem_Calloc(0xBC0, 0);
    actor->field_1C = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, (Task*)actor);
        return;
    }
    actor->field_18 = func_actor_356100_8016A158;
    Actor356100_BindMatrices(actor);
    enemy->field_4     = &actor->field_2C->field_8->coord;
    enemy->field_48    = 0;
    enemy->field_1C.vx = 0;
    enemy->field_1C.vy = 0;
    enemy->field_1C.vz = 0;
    enemy->field_18    = &actor->field_2C->field_8[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.field_4 = 1;
    enemy->field_4C     = 0;
    enemy->field_40     = (s16)D_actor_356100_8016A984.field_4;
    enemy->field_50     = &D_actor_356100_8016A984;
    enemy->field_54     = (s32)&work->field_9C0;
    func_800B3F84(&((Actor356100AnimWork*)work)->anim, D_actor_356100_801730B8, (GpAnimObj*)obj,
                  &((Actor356100AnimWork*)work)->slots[21], ((Actor356100AnimWork*)work)->slots);
    func_800B3F84(&((Actor356100AnimWork*)work)->blendAnim, D_actor_356100_801730B8, (GpAnimObj*)obj,
                  &((Actor356100AnimWork*)work)->blendSlots[21], ((Actor356100AnimWork*)work)->blendSlots);
    work->field_978 = 2;
    work->field_97E = 1;
    work->field_97A = 0;
    work->field_990 = 0;
    work->field_98E = 0;
    work->field_984 = 0x10;
    work->field_982 = 0x10;
    func_actor_356100_80163508(actor);
    work->field_14     = 0;
    work->field_C[0].x = actor->field_2C->field_8->coord.t[0];
    work->field_C[0].z = actor->field_2C->field_8->coord.t[2];
    Gfx_MatrixCol2(&actor->field_2C->field_8->coord, &dir);
    dir.vy = 0;
    v      = &dir;
    VectorNormalSS(v, v);
    gte_lddp(2000);
    gte_ldsv(v);
    gte_gpf12_real();
    gte_stsv(v);
    work->field_C[1].x = actor->field_2C->field_8->coord.t[0] + dir.vx;
    work->field_C[1].z = actor->field_2C->field_8->coord.t[2] + dir.vz;
    actor->field_24    = &D_actor_356100_80173258;
    root->sub          = &Gfx_ViewCoord;
    root->flg          = 0;
    Gp_UpdateCoord(root);
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    D_actor_356100_801732A8.field_0 = actor->field_2C->field_8;
    D_actor_356100_801732A8.field_4 = 0x100;
    D_actor_356100_801732A8.field_6 = 2;
    kind                            = actor->field_36;
    switch (kind & 0xF) {
        case 2:
            work->field_2 = -1;
            work->field_0 = 0;
            break;
        case 4:
            work->field_2 = -1;
            work->field_0 = 0x16;
            break;
        default:
            work->field_2 = -1;
            work->field_0 = 0x18;
            Tmd_AllocBuffers(obj);
            break;
    }
    switch (((Task*)actor)->spawnArg1 & 0xF) {
        case 2:
            work->field_B54 = D_actor_356100_8016A994[0].field_0;
            work->field_B56 = D_actor_356100_8016A994[0].field_2;
            break;
        case 1:
            work->field_B54 = D_actor_356100_8016A994[2].field_0;
            work->field_B56 = D_actor_356100_8016A994[2].field_2;
            break;
        case 0:
        default:
            work->field_B54 = D_actor_356100_8016A994[1].field_0;
            work->field_B56 = D_actor_356100_8016A994[2].field_2;
            break;
    }
    Actor356100_RescaleYaw(actor->field_2C->field_8, 0x1194);
    work->field_BBC = 0;
    actor->field_30++;
}

/// Runs the clip the work block's `field_978` halfword selects and holds this
/// state until it ends: while the actor is live, reset the model (`node.field_4`
/// / `obj->field_C`, `Tmd_AllocBuffers`), start clip 2 at speed 0x10, and tick
/// until clip 0xB has reached frame 6 or clip 0xC frame 9, then park `field_982`
/// at 0x20. Once the actor is no longer live the same slot is halved per frame as
/// a scale ramp that bounces between 0x10 and -0x10 — ending the state with
/// `field_0 = 0x11` when `Gp_TickObjFlag2` reports the flag has expired.
void func_actor_356100_80163CD4(Actor356100* arg0)
{
    Actor356100Work* work;
    GpEnemy*         ctx;
    TmdObject*       obj;
    s16              animA;
    s16              animB;
    s32              value;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4 != 0) {
        animA             = 0xB;
        animB             = 0xC;
        obj               = arg0->field_2C;
        ctx->node.field_4 = 0;
        obj->field_C      = 0;
        Tmd_AllocBuffers(obj);
        work->field_978 = 2;
        work->field_982 = 0x10;
        do {
            func_actor_356100_80163508(arg0);
        } while (((work->field_97E != animA) || ((u32)(work->field_5A & 0x3FF) < 6U)) &&
                 ((work->field_97E != animB) || ((u32)(work->field_5A & 0x3FF) < 9U)));
        work->field_982 = 0x20;
        return;
    }
    arg0->field_2C->field_8->flg = 0;
    value                        = (s16)work->field_982 / 2;
    work->field_982              = (u16)value;
    if (value == 1) {
        work->field_982 = -0x10U;
    }
    if ((s16)work->field_982 == -1) {
        work->field_982 = 0x10;
    }
    func_actor_356100_80163508(arg0);
    if (Gp_TickObjFlag2((GpObj5D*)ctx) == 1) {
        ctx->field_4C &= 0xFD;
        work->field_0  = 0x11;
    }
}

/// Turns the actor's facing onto the player in one step and rescales the root
/// coordinate to 0x1194: the live branch resets the model and starts clip 1 at
/// speed 0x10 with the 9 state parked in `field_97E`, otherwise the aim scratch
/// takes the player offset, `Actor356100_PositionYaw` gives the wrapped turn,
/// `field_98E` snapshots it, it is clamped to [-0x10, 0x10] and the root yaw is
/// re-derived from it. Same body as `func_actor_401300_8013AAE8`.
void func_actor_356100_80163E2C(Actor356100* arg0)
{
    Actor356100Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor356100AimScratch* aim;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_978 = 1;
        work->field_982 = 0x10;
        work->field_97A = 0;
        work->field_97E = 9;
        func_actor_356100_80163508(arg0);
        work->field_9BC = 0x180;
        Gp_ArmStateF0(1);
        return;
    }
    *(Actor356100AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor356100AimScratch**)G_SCRATCH_HEAD;
    arg0->field_2C->field_8->flg              = 0;
    if (work->field_68 & 1) {
        work->field_0 = 7;
    }
    aim->angle      = Actor356100_PositionYaw(arg0, &aim->delta, &Wip_SysConfig);
    work->field_98E = aim->angle;
    if (aim->angle >= 0x11) {
        aim->angle = 0x10;
    }
    if (aim->angle < -0x10) {
        aim->angle = -0x10;
    }
    coord       = arg0->field_2C->field_8;
    aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, aim->angle, 1);
    Actor356100_RescaleYaw(arg0->field_2C->field_8, 0x1194);
    func_actor_356100_80163508(arg0);
    *(Actor356100AimScratch**)G_SCRATCH_HEAD += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80164158);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80164ACC);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_801653F4);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80165B30);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80166018);

void func_actor_356100_801666B4(Actor356100* arg0)
{
    Actor356100Work* work;
    GpEnemy*         enemy;
    Task*            player;
    SVECTOR*         vecp;
    SVECTOR          vec;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        player                                    = Game_GetPtrSlot(3);
        work->field_9BC                           = 0x180;
        enemy->node.field_4                       = 0;
        work->field_978                           = 1;
        work->field_982                           = 0x10;
        work->field_97E                           = 5;
        ((TmdObject*)player->extra)->field_8->flg = 0;
        Gp_UpdateCoord(((TmdObject*)player->extra)->field_8);
        D_actor_356100_801732B0.x = ((TmdObject*)player->extra)->field_8->coord.t[0];
        D_actor_356100_801732B0.y = ((TmdObject*)player->extra)->field_8->coord.t[1];
        D_actor_356100_801732B0.z = ((TmdObject*)player->extra)->field_8->coord.t[2];
        vecp                      = &vec;
        /* Order matters: the vy store must follow the vx loads in RTL, or
           sched1 fills its anti-dependency chain from the earlier stores and
           hoists it above the D.z store. */
        vec.vx = ((GpCoordXZ*)arg0->field_2C->field_8)->field_18 - ((GpCoordXZ*)((TmdObject*)player->extra)->field_8)->field_18;
        vec.vy = 0;
        vec.vz = ((GpCoordXZ*)arg0->field_2C->field_8)->field_20 - ((GpCoordXZ*)((TmdObject*)player->extra)->field_8)->field_20;
        VectorNormalSS(vecp, vecp);
        gte_lddp(0x3E8);
        gte_ldsv(vecp);
        gte_gpf12_real();
        gte_stsv(vecp);
        arg0->field_2C->field_8->coord.t[0] = ((TmdObject*)player->extra)->field_8->coord.t[0] + vec.vx;
        arg0->field_2C->field_8->coord.t[2] = ((TmdObject*)player->extra)->field_8->coord.t[2] + vec.vz;
        arg0->field_2C->field_8->flg        = 0;
        D_actor_356100_801732B0.field_10    = 0;
        D_actor_356100_801732B0.field_12    = ratan2(vec.vx, vec.vz);
        D_actor_356100_801732B0.field_14    = 0;
        Gp_DispatchMsg(player, 0x3E9, (s32)&D_actor_356100_801732B0, 0);
    }
    func_actor_356100_80163508(arg0);
    if (work->field_97E == 5 && (work->field_68 & 1)) {
        work->field_0 = 0xD;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_801668FC);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80166CF0);

/// Rotation-collapse tick: going live clears the model's `field_C`, flags the
/// enemy's link node and re-seeds `field_6`. Each frame then bumps `field_6`
/// and fires its milestone — 0x18 releases state F0 (arg 0xA), 0x1D switches
/// light mode 1 and spawns effect 0x600A5 at model coordinate 2, 0x29 sets
/// `field_C` to 2, 0x2F switches light mode 2 and 0x33 sets `field_C` to 0x80.
/// From 0x1A on, the root rotation is rebuilt in the 0x34-byte scratch block
/// as a uniform 0x1194 scale whose Y shrinks by 0xB per frame past 0x14, and
/// written back into the root coordinate with `flg` cleared. Same body as
/// `Actor01900_Fn06904`.
void func_actor_356100_80167358(Actor356100* arg0)
{
    Actor356100Work*       work;
    GpEnemy*               enemy;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor356100RotScratch* blk;
    u8*                    head;
    u8*                    tail;
    void*                  scratch_base;
    s16                    temp_v0;
    s16                    ang;
    s16                    cur;
    s32                    k;
    s32                    sy;
    u16                    temp_v1;
    u16                    m22;

    work  = arg0->field_1C;
    obj   = arg0->field_2C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj->field_C        = 0;
        enemy->node.field_4 = 1;
        work->field_6       = 0;
    }
    temp_v1      = (u16)work->field_6;
    scratch_base = PSX_SCRATCH;
    if (work->field_6 < 0x401) {
        work->field_6 = (s16)(temp_v1 + 1);
        temp_v0       = temp_v1 - 0x18;
        switch (temp_v0) {
            case 0:
                Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0xA);
                break;
            case 5:
                Gp_SetLightMode((GpObj4C*)enemy, 1);
                Gp_SpawnEff(0x600A5, arg0->field_2C->field_8 + 2, 3, NULL);
                break;
            case 23:
                arg0->field_2C->field_C = 2;
                break;
            case 17:
                Gp_SetLightMode((GpObj4C*)enemy, 2);
                break;
            case 39:
                arg0->field_2C->field_C = 0x80;
                break;
        }
        cur = work->field_6;
        if (cur >= 0x1A) {
            k                                        = 0x1194;
            head                                     = scratch_base;
            head                                     = *(u8**)(head + 0x3FC);
            coord                                    = arg0->field_2C->field_8;
            blk                                      = (Actor356100RotScratch*)(head - 0x34);
            sy                                       = k - (cur - 0x14) * 0xB;
            *(Actor356100RotScratch**)G_SCRATCH_HEAD = blk;
            ang                                      = ratan2((s32)-coord->coord.m[2][0], (s32)coord->coord.m[2][2]);
            blk->angle                               = ang;
            Gfx_RotMatrixY(&blk->m, (s32)ang, 1);
            blk->scale.vx = k;
            blk->scale.vy = (s32)(s16)sy;
            blk->scale.vz = k;
            ScaleMatrix(&blk->m, &((Actor356100RotScratch*)(head - 0x34))->scale);
            coord->coord.m[0][0] = *(u16*)&((Actor356100RotScratch*)(head - 0x34))->m.m[0][0];
            coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
            coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
            coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
            coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
            coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
            coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
            coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
            __asm__ volatile("lui %0, 0x1F80" : "=r"(tail));
            tail       = *(u8**)(tail + 0x3FC);
            m22        = *(u16*)&blk->m.m[2][2];
            coord->flg = 0;
            tail       = tail + 0x34;
            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(tail) : "memory");
            coord->coord.m[2][2] = m22;
        }
    }
}

/// Range tick, and the sibling of `func_actor_356100_80167818` below it. Going
/// live clears the model's `field_C`, reallocates its buffers, clears the
/// enemy's link node, saves the `field_AF8` colour matrix into `field_B18` and
/// starts clip 0xE at speed 1 with `field_9BC` forced to 0x180. Each frame then
/// bumps `field_6` until it passes 0x960, after which a 4-bit `Gp_LcgState`
/// draw thins the tick to one frame in 16. A tick that runs drops to state 6
/// while the player is still within 3000 of the actor, then flips the clip
/// between 0xE and 0xF on a 50/50 draw gated by bits 2 and 1 of `field_68`.
/// Same shape as `func_actor_401300_80139520`.
void func_actor_356100_80167584(Actor356100* arg0)
{
    Actor356100Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR          delta;
    SVECTOR*         d;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj          = arg0->field_2C;
        enemy        = arg0->field_20;
        obj->field_C = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC     = 0x180;
        enemy->node.field_4 = 0;
        work->field_6       = 0;
        work->field_B18     = work->field_AF8;
        work->field_97E     = 0xE;
        work->field_978     = 1;
        work->field_982     = work->field_984;
    }
    if (work->field_6 > 0x960) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 0xF)) {
            return;
        }
    } else {
        work->field_6 = (s16)((u16)work->field_6 + 1);
    }
    coord = arg0->field_2C->field_8;
    d     = &delta;
    Actor356100_PositionDelta(coord, d);
    if (!Actor356100_OutOfRange(d, 3000)) {
        work->field_0 = 6;
    }
    func_actor_356100_80163508(arg0);
    if (work->field_97E == 0xE && (work->field_68 & 2)) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            work->field_97E = 0xF;
            work->field_978 = 1;
            func_actor_356100_80163508(arg0);
        }
    }
    if (work->field_97E == 0xF && (work->field_68 & 1)) {
        work->field_97E = 0xE;
        work->field_978 = 1;
        func_actor_356100_80163508(arg0);
    }
}

void func_actor_356100_80167818(Actor356100* arg0)
{
    Actor356100Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR          delta;
    SVECTOR*         d;
    s32              sound;
    s32              pan;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj                     = arg0->field_2C;
        D_actor_356100_801731B0 = 0;
        work->field_97E         = 0x10;
        work->field_978         = 2;
        obj->field_C            = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC     = 0x180;
        enemy->node.field_4 = 0;
        work->field_990     = 0;
        work->field_982     = 0x10;
        work->field_98E     = 0;
        work->field_6       = 0;
    } else if (work->field_6 == 0) {
        sound = ((enemy->field_8 >> 0xC) << 8) | 0x51030008;
        pan   = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
        work->field_6 = 1;
    }
    func_actor_356100_80163508(arg0);
    if ((work->field_5A & 0x3FF) == 4 && work->field_994 != (work->field_5A & 0x3FF)) {
        D_actor_356100_801732A8.field_0 = arg0->field_2C->field_8;
        D_actor_356100_801732A8.field_4 = 0x100;
        D_actor_356100_801732A8.field_6 = 2;
        func_800FDB18((u16)Gp_GetIdParam1(0x1001), arg0->field_2C->field_8 + 5, NULL,
                      &D_actor_356100_801732A8);
    }
    work->field_994 = work->field_5A & 0x3FF;
    coord           = arg0->field_2C->field_8;
    d               = &delta;
    Actor356100_PositionDelta(coord, d);
    if (!Actor356100_OutOfRange(d, 3000)) {
        SndEvt_EnqueueType7(0x51030008, 1);
        work->field_0 = 6;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80167A7C);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_8016804C);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_801684F0);

/// Turn the actor's facing onto the player in one step and rescale the root
/// coordinate to 0x1194: the live branch resets the model and starts clip 1 at
/// speed 0x10 with the 0x13 state parked in `field_97E`, otherwise `field_6`
/// ticks over for the 0xB-frame transition, the aim scratch takes the player
/// offset, `Actor356100_PositionYaw` gives the wrapped turn, `field_98E`
/// snapshots it, the turn is clamped to [-0x20, 0x20] and the root yaw is
/// re-derived from it before `field_0` moves to `state` once the count-down
/// expires. Same body as `func_actor_401300_8013AAE8`.
void func_actor_356100_80168AFC(Actor356100* arg0)
{
    Actor356100Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor356100AimScratch* aim;
    int                    state;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC = 0x180;
        work->field_978 = 1;
        work->field_982 = 0x10;
        work->field_97A = 0;
        work->field_97E = 0x13;
        func_actor_356100_80163508(arg0);
        work->field_6 = 0;
        return;
    }
    work->field_6                             = (s16)((u16)work->field_6 + 1);
    state                                     = 0xB;
    *(Actor356100AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor356100AimScratch**)G_SCRATCH_HEAD;
    arg0->field_2C->field_8->flg              = 0;
    if ((work->field_68 & 1) || ((s16)work->field_6 >= state)) {
        work->field_0 = state;
    }
    aim->angle      = Actor356100_PositionYaw(arg0, &aim->delta, &Wip_SysConfig);
    work->field_98E = aim->angle;
    if (aim->angle >= 0x21) {
        aim->angle = 0x20;
    }
    if (aim->angle < -0x20) {
        aim->angle = -0x20;
    }
    coord      = arg0->field_2C->field_8;
    aim->angle = (u16)aim->angle + ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, aim->angle, 1);
    Actor356100_RescaleYaw(arg0->field_2C->field_8, 0x1194);
    func_actor_356100_80163508(arg0);
    *(Actor356100AimScratch**)G_SCRATCH_HEAD += 1;
}

/// Turn the actor's facing onto the player in 0x28 steps and rescale the root
/// coordinate to 0x1194: the live branch resets the model and starts clip 2 at
/// speed 0x10 with the 0x13 state parked in `field_97E`, otherwise the aim
/// scratch takes the player offset, `Actor356100_PositionYaw` gives the wrapped
/// turn, `field_98E` walks toward it by at most 0x28 and the state flips to 0xB
/// once it has caught up. Same body as `func_actor_401300_8013AE48`.
void func_actor_356100_80168E44(Actor356100* arg0)
{
    Actor356100Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor356100AimScratch* aim;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 1;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_9BC = 0x180;
        work->field_978 = 2;
        work->field_982 = 0x10;
        work->field_97A = 0;
        work->field_97E = 0x13;
        func_actor_356100_80163508(arg0);
        func_actor_356100_80163508(arg0);
        work->field_6   = 0;
        work->field_990 = 0;
        return;
    }
    *(Actor356100AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor356100AimScratch**)G_SCRATCH_HEAD;
    aim->angle                                = Actor356100_PositionYaw(arg0, &aim->delta, &Wip_SysConfig);
    if (work->field_98E < aim->angle) {
        if (aim->angle - work->field_98E > 0x28) {
            work->field_98E += 0x28;
        } else {
            work->field_98E = aim->angle;
        }
    } else if (work->field_98E - aim->angle > 0x28) {
        work->field_98E -= 0x28;
    } else {
        work->field_98E = aim->angle;
    }
    if (work->field_98E == aim->angle) {
        work->field_0 = 0xB;
    }
    coord      = arg0->field_2C->field_8;
    aim->angle = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, aim->angle, 1);
    Actor356100_RescaleYaw(arg0->field_2C->field_8, 0x1194);
    work->field_978 = 2;
    func_actor_356100_80163508(arg0);
    *(Actor356100AimScratch**)G_SCRATCH_HEAD += 1;
}

/// The overlay's death-throes tick, the sibling of `func_actor_356100_80168E44`:
/// going live re-seeds the model (the enemy's link node, `obj->field_C`, the
/// 0x978..0x982 animation slots) and queues sound 0x550B0007 against the root
/// part, whose coordinate the live arm clears outright. Each frame then bumps
/// `field_6`, runs the clip and walks part 1's coordinate by a fixed 0x1044 /
/// 0x4AA per frame. Four frames each fire their own sound (0x550B0008 with the
/// 6/0xFF/0x80 pad rumble, 0x400D0002 with 8/0x7F/0x30, 0x400D0001 with
/// 6/0x7F/0x30, 0x550B0009 bare), and `field_6` 0x29..0x2D drives a 16-effect
/// 0x600FB burst over the model's part coordinates — 0x2E..0x31 the same burst
/// with six effects, alternating on the frame's parity.
///
/// The parity test re-reads `field_6` from memory rather than reusing the range
/// test's value (the two reads are what the original emits), so the read is
/// spelled volatile.
void func_actor_356100_80169180(Actor356100* arg0)
{
    Actor356100Work* work;
    GpEnemy*         ctx;
    GsCOORDINATE2*   coord;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        s32 pan;

        ctx                     = arg0->field_20;
        arg0->field_2C->field_C = 0;
        Tmd_AllocBuffers(arg0->field_2C);
        ctx->node.field_4 = 1;
        work->field_97E   = 1;
        work->field_978   = 2;
        Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, 0x800, 1);
        coord                        = arg0->field_2C->field_8;
        coord->coord.t[2]            = 0;
        coord->coord.t[1]            = 0;
        coord->coord.t[0]            = 0;
        arg0->field_2C->field_8->flg = 0;
        Gp_UpdateCoord(arg0->field_2C->field_8);
        work->field_6 = 0;
        pan           = (s8)Gp_GetObjPan((GpObj38*)&arg0->field_2C->field_8[1]);
        SndEvt_EnqueueType6(0x550B0007, pan, (s8)Gp_GetObjDepth((GpObj38*)&arg0->field_2C->field_8[1]));
    }
    work->field_6 = (s16)((u16)work->field_6 + 1);
    func_actor_356100_80163508(arg0);
    arg0->field_2C->field_8[1].coord.t[0] += 0x1044;
    arg0->field_2C->field_8[1].coord.t[2] += 0x4AA;
    arg0->field_2C->field_8[1].flg         = 0;
    Gp_UpdateCoord(&arg0->field_2C->field_8[1]);
    if (work->field_6 == 0x31) {
        s32 pan;

        pan = (s8)Gp_GetObjPan((GpObj38*)&arg0->field_2C->field_8[1]);
        SndEvt_EnqueueType6(0x550B0008, pan, (s8)Gp_GetObjDepth((GpObj38*)&arg0->field_2C->field_8[1]));
        Gp_SpawnPadLerp(6, 0xFF, 0x80);
    }
    if (work->field_6 == 0x4D) {
        s32 pan;

        pan = (s8)Gp_GetObjPan((GpObj38*)&arg0->field_2C->field_8[1]);
        SndEvt_EnqueueType6(0x400D0002, pan, (s8)Gp_GetObjDepth((GpObj38*)&arg0->field_2C->field_8[1]));
        Gp_SpawnPadLerp(8, 0x7F, 0x30);
    }
    if (work->field_6 == 0x58) {
        s32 pan;

        pan = (s8)Gp_GetObjPan((GpObj38*)&arg0->field_2C->field_8[1]);
        SndEvt_EnqueueType6(0x400D0001, pan, (s8)Gp_GetObjDepth((GpObj38*)&arg0->field_2C->field_8[1]));
        Gp_SpawnPadLerp(6, 0x7F, 0x30);
    }
    if (work->field_6 == 0xCE) {
        s32 pan;

        pan = (s8)Gp_GetObjPan((GpObj38*)&arg0->field_2C->field_8[1]);
        SndEvt_EnqueueType6(0x550B0009, pan, (s8)Gp_GetObjDepth((GpObj38*)&arg0->field_2C->field_8[1]));
    }
    if ((u32)((u16)work->field_6 - 0x29) < 5U) {
        Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[3], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[0x10], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[1], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[0x12], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[2], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[0x11], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[3], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[4], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[5], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[0x10], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[1], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[0x13], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[0x11], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[0x10], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[5], 0, 0);
        Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[0x12], 0, 0);
    }
    if ((u32)((u16)work->field_6 - 0x2E) < 4U) {
        if (!(*(volatile u16*)&work->field_6 & 1)) {
            Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[2], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[0x11], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[3], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[4], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[5], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[0x10], 0, 0);
        } else {
            Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[1], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[0x13], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[0x11], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[0x10], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[5], 0, 0);
            Gp_SpawnEff(0x600FB, &arg0->field_2C->field_8[0x12], 0, 0);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100", func_actor_356100_80169854);

s32 func_actor_356100_80169E5C(void)
{
    return 0;
}
