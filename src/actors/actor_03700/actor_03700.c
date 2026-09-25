#include "common.h"

#include "actors/actor.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

/// Per-actor work block, reached as `(Actor103700Work*)task->work`; this actor
/// keeps its own state in the `Task::work` slot rather than a `TaskIdMap`.
///
/// `field_24E` is the behaviour mode `Actor03700_Fn008D0` dispatches on and
/// `field_250` the phase within it. `field_248` is the animation requested and
/// `field_24A` the one playing, with `field_24C` counting its frames. The
/// movement helpers steer the root coordinate towards the target position
/// `field_23C` at turn rate `field_254` and forward speed `field_252`,
/// remembering the previous position in `field_22C`. `field_25E` and
/// `field_25C` are the bob and sway phases, `field_260` the ambient cue timer,
/// and `field_262` is set while the actor holds the player.
typedef struct Actor103700Work {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[6];
    /* 0x104 */ byte       poses[0x60]; // pose buffer, `func_800B3F84` arg3
    /* 0x164 */ MATRIX     colorMtx;
    /* 0x184 */ MATRIX     lightMtx;
    /* 0x1A4 */ GpObj      obj;
    /* 0x1C4 */ GpRec18    records[4];
    /* 0x224 */ GpEffArg   field_224; // hit-spark record for `func_800FDB18`
    /* 0x22C */ SVECTOR    field_22C;
    /* 0x234 */ SVECTOR    field_234;
    /* 0x23C */ SVECTOR    field_23C;
    /* 0x244 */ s16        field_244;
    /* 0x246 */ s16        field_246;
    /* 0x248 */ s16        field_248;
    /* 0x24A */ s16        field_24A;
    /* 0x24C */ s16        field_24C;
    /* 0x24E */ s16        field_24E;
    /* 0x250 */ s16        field_250;
    /* 0x252 */ s16        field_252;
    /* 0x254 */ s16        field_254;
    /* 0x256 */ u16        field_256;
    /* 0x258 */ u16        field_258;
    /* 0x25A */ s16        field_25A;
    /* 0x25C */ u16        field_25C;
    /* 0x25E */ u16        field_25E;
    /* 0x260 */ u16        field_260;
    /* 0x262 */ s16        field_262;
    /* 0x264 */ s16        field_264;
    /* 0x266 */ s16        field_266;
    /* 0x268 */ s16        field_268;
    /* 0x26A */ s16        field_26A;
    /* 0x26C */ u16        field_26C;
} Actor103700Work;

/// 0x2C-byte scratch from `G_SCRATCH_HEAD` used by `Actor03700_Fn03130`:
/// the 0x3F8 query buffer followed by the `GpAnimArg` it sends as message 0x3FF.
typedef struct Actor103700HoldScratch {
    /* 0x00 */ GpDelayArg query;
    /* 0x18 */ GpAnimArg  anim;
} Actor103700HoldScratch;
STATIC_ASSERT_SIZEOF(Actor103700HoldScratch, 0x2C);

extern u16 Actor03700_D07F7C[];

/// Halfword tables `Actor03700_Fn018C8` indexes by a 4-bit LCG draw:
/// the countdown seeded into `field_258` and `field_256`.
extern u16 Actor03700_D07F3C[];
extern u16 Actor03700_D07F5C[];

/// Pair `Actor03700_Fn01550` packs with `Gp_PackPair` for message 0x3F9.
extern GpU16Pair Actor03700_D07F08;

/// Halfword table `Actor03700_Fn01550` indexes by a 4-bit LCG draw.
extern s16 Actor03700_D07F1C[];

/// Halfword bob table, one row of 15 per `arg1`: the row runs
/// 0, 10, 19, 24, 25, 22, 15, 5, -5, -15, -22, -25 before returning to 0.
/// Every use reads it as a signed halfword through `lh` and adds it to a
/// coordinate's Y translation, so it is the amplitude of an idle bob.
extern s16 Actor03700_D07F98[];

/// Halfword wave table `Actor03700_Fn03320` indexes by `field_25C`.
extern s16 Actor03700_D07FD4[];

/// 8-byte rise step: while `field_24C` is below `threshold` the Y and
/// forward displacements are spread over `steps` frames.
typedef struct Actor103700Rise {
    /* 0x0 */ s16 threshold;
    /* 0x2 */ s16 steps;
    /* 0x4 */ s16 dy;
    /* 0x6 */ s16 dist;
} Actor103700Rise;

extern Actor103700Rise Actor03700_D07FF4[];
extern Actor103700Rise Actor03700_D0802C[];

/// The enemy's pair source; the spawn stores it in `GpEnemy::param` and
/// seeds HP from its `hpMax`, which retail addresses as its own label.
extern GpPairSrcE Actor03700_D07F0C;
extern u16        Actor03700_D07F10;

/// Animation data `func_800B3F84` loads, and the task's `field_24` table.
extern u8    Actor03700_D080E4[];
extern void* Actor03700_D08108;

/// Animation-set table handed to the player as the 0x3FF payload's `animBlock`.
extern GpAnimSet* Actor03700_D080FC[];

/// Halfword table indexed by the low 7 bits of a hit id; 3 cancels the damage.
extern s16 Actor03700_D08074[];

/// 0x18-byte scratch the approach helpers carve off the scratchpad stack: the
/// offset to the target and its `VectorNormalS`. `Actor03700_Fn029C0` never
/// gives it back; `Actor03700_Fn027DC` does.
typedef struct Actor103700SteerScratch {
    /* 0x00 */ VECTOR  delta;
    /* 0x10 */ SVECTOR normal;
} Actor103700SteerScratch;
STATIC_ASSERT_SIZEOF(Actor103700SteerScratch, 0x18);

/* `D_80067704` selects the model stream the next `Gp_SpawnEff` builds its
 * `TmdObject` from. */
extern void* D_80067704[1];

/* The two model streams the death effect picks between, in this overlay's data. */
extern u8 Actor03700_D043FC[];
extern u8 Actor03700_D04600[];

/// `func_800B4114` is deliberately not declared by `gameplay/1BC.h`; its
/// definition takes `arg2` as `u16`, which would add a zero-extension no
/// caller has. See the note in that header.
void func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void Actor03700_Fn000A4(GpEnemy* arg0, Task* task);
void Actor03700_Fn0042C(Task* task, TmdObject* arg1, s32 arg2);
s32  Actor03700_Fn008D0(Task* task);
void Actor03700_Fn00ABC(Task* task);
void Actor03700_Fn00D5C(Task* task);
void Actor03700_Fn00F88(Task* task);
void Actor03700_Fn011B4(Task* task);
void Actor03700_Fn01550(Task* task);
void Actor03700_Fn018C8(Task* task);
void Actor03700_Fn01C94(Task* task);
s32  Actor03700_Fn01DFC(Task* task);
void Actor03700_Fn01F48(Task* task);
void Actor03700_Fn020D4(GpEnemy* enemy, Task* task);
void Actor03700_Fn025C8(Task* task);
void Actor03700_Fn027DC(Task* task);
void Actor03700_Fn029C0(Task* task);
void Actor03700_Fn03004(GpEnemy* enemy, Task* task);
s32  Actor03700_Fn03130(Task* task);
void Actor03700_Fn0321C(Task* task);
void Actor03700_Fn032BC(Task* task, s32 arg1, s32 arg2);
void Actor03700_Fn03320(Task* task, s32 arg1);
void Actor03700_Fn033F0(Task* task);
void Actor03700_Fn034A0(Task* task);
void Actor03700_Fn0355C(Task* task);

/// The bob step of `Actor03700_Fn032BC`, which `Actor03700_Fn029C0` carries
/// expanded in place rather than as a call.
static inline void Actor03700_BobInline(Task* task, s32 arg1, s32 arg2)
{
    Actor103700Work* work;
    GsCOORDINATE2*   coord;
    u16              frame;

    work  = (Actor103700Work*)task->work;
    coord = task->extra.tmd->coords;

    frame           = work->field_25E + 1;
    work->field_25E = frame;
    if (arg2 < (s16)frame) {
        work->field_25E = 0;
    }
    coord->coord.t[1] += Actor03700_D07F98[(arg1 * 15) + (s16)work->field_25E];
}

/// The sway step of `Actor03700_Fn03320`, expanded in place the same way.
static inline void Actor03700_SwayInline(Task* task, s32 arg1)
{
    Actor103700Work* work;
    GsCOORDINATE2*   coord;
    u16              frame;
    s32              amp;

    work  = (Actor103700Work*)task->work;
    coord = task->extra.tmd->coords;

    frame           = work->field_25C + 1;
    work->field_25C = frame;
    if ((s16)frame >= 15) {
        work->field_25C = 0;
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        work->field_25A = arg1 + ((Gp_LcgState >> 16) & 0x3F);
    }
    amp                = (work->field_25A * Actor03700_D07FD4[(s16)work->field_25C] * 16) >> 16;
    coord->coord.t[0] += (amp * coord->coord.m[0][0]) >> 12;
    coord->coord.t[2] += (amp * coord->coord.m[2][0]) >> 12;
}

/// The enemy's state handlers, run by `Actor03700_Fn02FA8` for the task's
/// state: spawn, per-frame tick and death.
const GpEnemyTaskFuncTable3 Actor03700_D00004 = {
    { Actor03700_Fn000A4, Actor03700_Fn03004, Actor03700_Fn020D4 },
};

/// Spawn handler. Allocates the 0x270-byte work block onto the task, points the
/// model at its light/colour matrices and links the enemy node. The model
/// variant (`GpAreaPlace::mode`) picks the mode: tens digit 0 allocates
/// the model buffers and takes the units digit (0..2) as the pose, nudging the
/// root coordinate for poses 1 and 2; 1..3 set model flag 4 and mode 7 or 10.
/// The animation slots then get a shared random phase, and the collision object
/// is linked with its four records before the task moves to state 1.
void Actor03700_Fn000A4(GpEnemy* arg0, Task* task)
{
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    Actor103700Work* work;
    s32              kind;
    s32              i;

    obj   = task->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x270, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, task);
        return;
    }
    task->work     = (TaskIdMap*)work;
    obj->flags     = 0;
    coord->flg     = 0;
    obj->lightMtx  = &work->lightMtx;
    obj->colorMtx  = &work->colorMtx;
    arg0->field_4  = &coord->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->param                = &Actor03700_D07F0C;
    arg0->coord                = coord;
    arg0->node.state.b.flags   = 0;
    arg0->bodyPos.vx           = 0;
    arg0->bodyPos.vy           = 0;
    arg0->bodyPos.vz           = 0;
    arg0->recs                 = work->records;
    work->field_224.coord      = &task->extra.tmd->coords[1];
    work->field_224.spawnArgLo = 0x100;
    work->field_224.spawnArgHi = 1;
    work->field_246            = arg0->place->yaw;
    kind                       = arg0->place->mode;
    switch (kind / 10) {
        case 0:
            Tmd_AllocBuffers(obj);
            if (kind < 3) {
                work->field_24E = kind;
            } else {
                work->field_24E = 0;
            }
            work->field_248 = kind < 3 ? kind + 1 : 1;
            switch (work->field_24E) {
                case 1:
                    work->field_248    = 2;
                    coord->coord.t[1] += 0x50;
                    break;
                case 2:
                    work->field_248    = 3;
                    coord->coord.t[2] -= 0x55;
                    break;
                case 0:
                    work->field_248 = 1;
                    break;
            }
            break;
        case 1:
        case 2:
            obj->flags     |= 4;
            work->field_24E = 7;
            work->field_248 = 1;
            if (kind == 10) {
                work->field_266 = 1;
            }
            break;
        case 3:
            obj->flags     |= 4;
            work->field_24E = 10;
            work->field_248 = 1;
            break;
    }
    arg0->hp        = Actor03700_D07F10;
    work->field_24A = work->field_248;
    task->msgTable  = &Actor03700_D08108;
    func_800B3F84(&work->anim, Actor03700_D080E4, obj, work->poses, work->slots);
    for (i = 1; i < 6; i++) {
        Gp_AnimResetSlot(&work->anim, i, work->field_248);
    }
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    kind        = (Gp_LcgState >> 16) & 3;
    for (i = 1; i < 6; i++) {
        work->slots[i].rate += kind;
    }
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    work->field_234.vx = coord->coord.t[0];
    work->field_234.vy = coord->coord.t[1];
    work->field_234.vz = coord->coord.t[2];
    work->obj.radius   = 0xC8;
    work->obj.coord    = coord;
    work->obj.ctx.recs = work->records;
    work->obj.pos.vx   = 0;
    work->obj.pos.vy   = 0;
    work->obj.pos.vz   = 0;
    work->obj.key      = 0x30025;
    work->obj.flags    = 1;
    Gp_LinkObj(2, &work->obj);
    Gp_InitRec18Table(work->records, 4, 0);
    work->obj.flags |= 0xC000;
    task->state      = 1;
}

/// Collision step. Applies the `func_800E0C10` push-back to the root coordinate
/// (a delta for 1, an absolute reset to `field_22C` for 2), then walks the four
/// records: kind 1 records push the actor out along the deepest overlap, kind 2
/// records are hits from a player slot, which take damage, spawn the hit spark
/// and move the actor into its flinch / knockdown modes.
void Actor03700_Fn0042C(Task* task, TmdObject* arg1, s32 arg2)
{
    ActorPushFrame*  scratch;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   src;
    Actor103700Work* work;
    s32              push;
    s32              reach;
    s32              res;
    s32              i;
    s32              z;
    s32              val;
    s32              ex;
    s32              ey;
    s32              ez;
    s32              broke;
    u32              id;
    u32              damage;

    push    = 0;
    broke   = 0;
    work    = (Actor103700Work*)task->work;
    scratch = (ActorPushFrame*)SCRATCH_PUSH_BYTES(0x58);
    coord   = task->extra.tmd->coords;
    res     = func_800E0C10(work->records, &scratch->delta, 4, NULL);
    if (res == 1)
        goto move_delta;
    if (res < 2)
        goto move_done;
    if (res == 2)
        goto move_absolute;
    goto move_done;
move_delta:
    coord->coord.t[0] += scratch->delta.vx.h.hi;
    coord->coord.t[1] += scratch->delta.vy.h.hi;
    z                  = coord->coord.t[2] + scratch->delta.vz.h.hi;
    goto move_z;
move_absolute:
    coord->coord.t[0] = work->field_22C.vx;
    coord->coord.t[1] = work->field_22C.vy;
    z                 = work->field_22C.vz;
move_z:
    coord->coord.t[2] = z;
move_done:
    i               = 0;
    work->field_264 = 0;
    do {
        id = work->records[i].key;
        switch (id >> 16) {
            case 0:
                break;
            case 1:
                work->field_264     = id >> 16;
                scratch->delta.vx.w = coord->workm.t[0] - work->records[i].point.vx;
                scratch->delta.vy.w = coord->workm.t[1] - work->records[i].point.vy;
                scratch->delta.vz.w = coord->workm.t[2] - work->records[i].point.vz;
                reach               = work->records[i].depth - SquareRoot0(scratch->delta.vx.w * scratch->delta.vx.w + scratch->delta.vy.w * scratch->delta.vy.w + scratch->delta.vz.w * scratch->delta.vz.w);
                val                 = reach;
                if (reach <= 0) {
                    val = 0;
                }
                reach = val;
                if (push < reach) {
                    push = reach;
                    VectorNormal((VECTOR*)&scratch->delta, &scratch->normal);
                    ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm, &scratch->normal, &scratch->dir);
                }
                break;
            case 2:
                src                 = Gp_ActorSlots[(id >> 7) & 1]->extra.tmd->coords;
                ex                  = src->coord.t[0] - coord->coord.t[0];
                scratch->delta.vx.w = ex;
                ey                  = src->coord.t[1] - coord->coord.t[1];
                scratch->delta.vy.w = ey;
                ez                  = src->coord.t[2] - coord->coord.t[2];
                scratch->delta.vz.w = ez;
                damage              = Gp_ComputeDamage(work->records[i].key, SquareRoot0(ex * ex + ey * ey + ez * ez), 0, 0);
                id                  = work->records[i].key;
                if (id & 0x8000) {
                    if (Actor03700_D08074[id & 0x7F] == 3) {
                        broke  = 1;
                        damage = 0;
                    } else {
                        broke = Gp_GetIdParam1(id) & 0xFFFF;
                        if ((u32)(broke - 0xC) < 2) {
                            func_800FDB18(broke, coord, NULL, &work->field_224);
                        }
                        work->field_268 = Actor03700_D08074[work->records[i].key & 0x7F];
                        broke           = 0;
                    }
                } else {
                    work->field_268 = (Gp_GetIdParam1(id) & 0xFFFF) == 7;
                }
                func_800DA6E8(&((GpEnemy*)task->spawnArg2)->node, damage, 0);
                func_800E2C78(task->spawnArg2, work->records[i].key, damage, 0);
                if ((s32)damage > 0) {
                    ((GpEnemy*)task->spawnArg2)->hp = 0;
                    work->field_24E                 = 6;
                    work->field_250                 = 0;
                    task->state                     = 2;
                } else if (((Gp_GetIdParam0(work->records[i].key) & 0xFFFF) == 8 || broke == 1) &&
                           (u16)(work->field_24E - 1) >= 2) {
                    if (work->field_262 == 0) {
                        work->field_24E = 5;
                        work->field_250 = 3;
                    } else {
                        work->field_24E = 11;
                        work->field_250 = 0;
                    }
                }
                break;
        }
    } while (++i < 4);
    if (push > 0) {
        coord->coord.t[0] += (push * scratch->dir.vx) >> 12;
        coord->coord.t[2] += (push * scratch->dir.vz) >> 12;
    }
    Gp_ClearRec18Occupied(work->records);
    SCRATCH_POP_BYTES(0x58);
}

/// The tick's mode dispatcher: runs the handler for the work block's mode
/// `field_24E`. Modes 0, 3, 4, 5, 8 and 9 (and 10 while `field_250` is set)
/// also count `field_260` up and replay the ambient cue from the placement's
/// sound bank every 16 frames. Mode 6 clears `field_250`, mode 7 releases the
/// contact records and runs the descent in `Actor03700_Fn025C8`; both report 1,
/// which tells the caller to skip this frame's movement.
s32 Actor03700_Fn008D0(Task* task)
{
    s16              state;
    GsCOORDINATE2*   object;
    s32              ret;
    u16              timer;
    Actor103700Work* soundWork;
    Actor103700Work* work;

    work  = (Actor103700Work*)task->work;
    state = work->field_24E;
    ret   = 0;
    /* Each sound block needs separate locals to preserve the call scheduling. */
    switch (state) {
        case 0:
            Actor03700_Fn00ABC(task);
            soundWork            = (Actor103700Work*)task->work;
            object               = task->extra.tmd->coords;
            timer                = soundWork->field_260 + 1;
            soundWork->field_260 = timer;
            if ((s16)timer < 0x10) {
                return ret;
            }
            soundWork->field_260 = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = ((GpEnemy*)task->spawnArg2)->placeKey;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)Gp_GetObjPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(object));
            }
            return ret;
        case 1:
            Actor03700_Fn00D5C(task);
            return ret;
        case 2:
            Actor03700_Fn00F88(task);
            return ret;
        case 3:
            Actor03700_Fn011B4(task);
            soundWork            = (Actor103700Work*)task->work;
            object               = task->extra.tmd->coords;
            timer                = soundWork->field_260 + 1;
            soundWork->field_260 = timer;
            if ((s16)timer < 0x10) {
                return ret;
            }
            soundWork->field_260 = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = ((GpEnemy*)task->spawnArg2)->placeKey;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)Gp_GetObjPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(object));
            }
            return ret;
        case 4:
            Actor03700_Fn01550(task);
            soundWork            = (Actor103700Work*)task->work;
            object               = task->extra.tmd->coords;
            timer                = soundWork->field_260 + 1;
            soundWork->field_260 = timer;
            if ((s16)timer < 0x10) {
                return ret;
            }
            soundWork->field_260 = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = ((GpEnemy*)task->spawnArg2)->placeKey;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)Gp_GetObjPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(object));
            }
            return ret;
        case 5:
            Actor03700_Fn018C8(task);
            soundWork            = (Actor103700Work*)task->work;
            object               = task->extra.tmd->coords;
            timer                = soundWork->field_260 + 1;
            soundWork->field_260 = timer;
            if ((s16)timer < 0x10) {
                return ret;
            }
            soundWork->field_260 = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = ((GpEnemy*)task->spawnArg2)->placeKey;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)Gp_GetObjPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(object));
            }
            return ret;
        case 6:
            work->field_250 = 0;
            ret             = 1;
            break;
        case 7:
            Gp_ClearRec18Occupied(work->records);
            if (work->field_266 != 0) {
                Actor03700_Fn0355C(task);
            }
            Actor03700_Fn025C8(task);
            ret = 1;
            break;
        case 8:
            if (work->field_266 != 0) {
                Actor03700_Fn0355C(task);
            }
            Actor03700_Fn027DC(task);
            soundWork            = (Actor103700Work*)task->work;
            object               = task->extra.tmd->coords;
            timer                = soundWork->field_260 + 1;
            soundWork->field_260 = timer;
            if ((s16)timer < 0x10) {
                return ret;
            }
            soundWork->field_260 = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = ((GpEnemy*)task->spawnArg2)->placeKey;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)Gp_GetObjPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(object));
            }
            return ret;
        case 9:
            Actor03700_Fn027DC(task);
            soundWork            = (Actor103700Work*)task->work;
            object               = task->extra.tmd->coords;
            timer                = soundWork->field_260 + 1;
            soundWork->field_260 = timer;
            if ((s16)timer < 0x10) {
                return ret;
            }
            soundWork->field_260 = 0;
            {
                u32 soundId;
                s32 pan;
                soundId   = ((GpEnemy*)task->spawnArg2)->placeKey;
                soundId >>= 0xC;
                soundId <<= 8;
                soundId  |= 0x40250005;
                pan       = (s8)Gp_GetObjPan(object);
                SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(object));
            }
            return ret;
        case 10:
            Actor03700_Fn029C0(task);
            if (work->field_250 != 0) {
                soundWork            = (Actor103700Work*)task->work;
                object               = task->extra.tmd->coords;
                timer                = soundWork->field_260 + 1;
                soundWork->field_260 = timer;
                if ((s16)timer < 0x10) {
                    return ret;
                }
                soundWork->field_260 = 0;
                {
                    u32 soundId;
                    s32 pan;
                    soundId   = ((GpEnemy*)task->spawnArg2)->placeKey;
                    soundId >>= 0xC;
                    soundId <<= 8;
                    soundId  |= 0x40250005;
                    pan       = (s8)Gp_GetObjPan(object);
                    SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(object));
                }
            }
            return ret;
        case 11:
            Actor03700_Fn01C94(task);
            break;
    }
    return ret;
}

void Actor03700_Fn00ABC(Task* task)
{
    Actor103700Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR*         head;
    SVECTOR*         vec;
    s16              angle;
    s32              dist;

    head                  = SCRATCH_HEAD(SVECTOR);
    vec                   = head - 1;
    SCRATCH_HEAD(SVECTOR) = vec;
    work                  = (Actor103700Work*)task->work;
    coord                 = task->extra.tmd->coords;

    switch (work->field_250) {
        case 0:
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_252    = ((Gp_LcgState >> 16) & 0xF) + 20;
            work->field_254    = Actor03700_D07F7C[((GpEnemy*)task->spawnArg2)->place->rowIndex];
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            angle              = (Gp_LcgState >> 16) & 0xFFF;
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            dist               = (Gp_LcgState >> 16) & 0x1FF;
            work->field_23C.vx = work->field_234.vx + ((dist * rsin(angle)) >> 12);
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_23C.vy = work->field_234.vy + ((Gp_LcgState >> 16) & 0x1FF);
            work->field_23C.vz = work->field_234.vz + ((dist * rcos(angle)) >> 12);
            vec->vx            = work->field_23C.vx - coord->coord.t[0];
            vec->vy            = 0;
            vec->vz            = work->field_23C.vz - coord->coord.t[2];
            work->field_244    = ratan2(vec->vx, vec->vz) & 0xFFF;
            work->field_250    = 1;
            break;
        case 1:
            vec->vx = work->field_23C.vx - coord->coord.t[0];
            vec->vz = work->field_23C.vz - coord->coord.t[2];
            if ((s16)SquareRoot0(vec->vx * vec->vx + vec->vz * vec->vz) < 120) {
                work->field_250 = 0;
            }
            break;
    }
    Actor03700_Fn032BC(task, 0, 14);
    Actor03700_Fn03320(task, 20);
    if (Actor03700_Fn01DFC(task) != 0) {
        work->field_24E      = 3;
        work->field_250      = 0;
        Gp_StateF0.field_19 |= 1;
    }
    SCRATCH_POP(SVECTOR);
}

void Actor03700_Fn00D5C(Task* task)
{
    Actor103700Work* work;
    GsCOORDINATE2*   coord;
    s32              dist;
    s32              i;

    work  = (Actor103700Work*)task->work;
    coord = task->extra.tmd->coords;

    switch (work->field_250) {
        case 0:
            if (Actor03700_Fn01DFC(task) != 0) {
                Gp_StateF0.field_19 |= 1;
                Gp_LcgState          = Gp_LcgState * 5 + 0x71357911;
                work->field_250      = 1;
                work->field_256      = (Gp_LcgState >> 16) & 0x3F;
            }
            break;
        case 1:
            if ((s16)--work->field_256 <= 0) {
                work->field_250 = 2;
                work->field_256 = 0;
                work->field_248 = 4;
            }
            break;
        case 2:
            for (i = 0; i < 7; i++) {
                if (Actor03700_D07FF4[i].threshold >= work->field_24C) {
                    coord->coord.t[1] += Actor03700_D07FF4[i].dy / Actor03700_D07FF4[i].steps;
                    dist               = Actor03700_D07FF4[i].dist / Actor03700_D07FF4[i].steps;
                    coord->coord.t[0] += (rsin(work->field_246) * dist) >> 12;
                    coord->coord.t[2] += (rcos(work->field_246) * dist) >> 12;
                    break;
                }
            }
            if (work->field_24C >= 50) {
                work->field_248      = 1;
                work->field_24E      = 3;
                work->field_250      = 0;
                Gp_StateF0.field_19 |= 1;
            }
            break;
    }
}

void Actor03700_Fn00F88(Task* task)
{
    Actor103700Work* work;
    GsCOORDINATE2*   coord;
    s32              dist;
    s32              i;

    work  = (Actor103700Work*)task->work;
    coord = task->extra.tmd->coords;

    switch (work->field_250) {
        case 0:
            if (Actor03700_Fn01DFC(task) != 0) {
                Gp_StateF0.field_19 |= 1;
                Gp_LcgState          = Gp_LcgState * 5 + 0x71357911;
                work->field_250      = 1;
                work->field_256      = (Gp_LcgState >> 16) & 0x3F;
            }
            break;
        case 1:
            if ((s16)--work->field_256 <= 0) {
                work->field_250 = 2;
                work->field_256 = 0;
                work->field_248 = 5;
            }
            break;
        case 2:
            for (i = 0; i < 9; i++) {
                if (Actor03700_D0802C[i].threshold >= work->field_24C) {
                    coord->coord.t[1] += Actor03700_D0802C[i].dy / Actor03700_D0802C[i].steps;
                    dist               = Actor03700_D0802C[i].dist / Actor03700_D0802C[i].steps;
                    coord->coord.t[0] += (rsin(work->field_246) * dist) >> 12;
                    coord->coord.t[2] += (rcos(work->field_246) * dist) >> 12;
                    break;
                }
            }
            if (work->field_24C >= 50) {
                work->field_248      = 1;
                work->field_24E      = 3;
                work->field_250      = 0;
                Gp_StateF0.field_19 |= 1;
            }
            break;
    }
}

void Actor03700_Fn011B4(Task* task)
{
    Actor103700Work* work;
    GsCOORDINATE2*   coord;
    void*            head;
    SVECTOR*         vec;
    s32              i;
    s32              sound;
    s8               slot;

    coord              = task->extra.tmd->coords;
    head               = SCRATCH_HEAD(void);
    SCRATCH_HEAD(void) = (u8*)head - sizeof(SVECTOR);
    work               = (Actor103700Work*)task->work;
    vec                = SCRATCH_HEAD(void);

    switch (work->field_250) {
        case 0:
            work->field_23C.vx = Player_Status.coordMtx->t[0];
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_23C.vy = Player_Status.coordMtx->t[1] - (((Gp_LcgState >> 16) & 0x3FF) + 800);
            work->field_23C.vz = Player_Status.coordMtx->t[2];
            if (work->field_26A == 0) {
                work->field_252 = 5;
            } else {
                work->field_252 = 0;
                work->field_26A = 0;
            }
            work->field_254 = Actor03700_D07F7C[((GpEnemy*)task->spawnArg2)->place->rowIndex];
            Actor03700_Fn03320(task, 20);
            if (work->field_246 == work->field_244) {
                work->field_250 = 1;
                work->field_252 = Actor03700_D07F1C[((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
                coord           = task->extra.tmd->coords;
                vec->vx         = work->field_23C.vx - coord->coord.t[0];
                vec->vy         = work->field_23C.vy - coord->coord.t[1];
                vec->vz         = work->field_23C.vz - coord->coord.t[2];
                work->field_256 = SquareRoot0(vec->vx * vec->vx + vec->vy * vec->vy + vec->vz * vec->vz) / work->field_252;
                slot            = (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 3) + 17;
                for (i = 1; i < 6; i++) {
                    work->slots[i].rate = slot;
                }
                coord = task->extra.tmd->coords;
                sound = ((((GpEnemy*)task->spawnArg2)->placeKey >> 12) << 8) | 0x40250002;
                SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            break;
        case 1:
            Actor03700_Fn03320(task, 40);
            if ((s16)--work->field_256 <= 0) {
                work->field_250 = 0;
                work->field_256 = 0;
                work->field_26A = 1;
            }
            if (work->field_264 != 0) {
                if (Actor03700_Fn03130(task) == 0) {
                    func_800FDB18(1, coord, NULL, &work->field_224);
                    work->field_24E = 5;
                } else {
                    work->field_24E = 4;
                }
                work->field_250 = 0;
            }
            if (Gp_StateF0.field_19 & 2) {
                work->field_24E = 5;
                work->field_248 = 1;
                work->field_250 = 0;
            }
            break;
    }
    Actor03700_Fn032BC(task, 0, 14);
    SCRATCH_POP_BYTES(sizeof(SVECTOR));
}

void Actor03700_Fn01550(Task* task)
{
    Actor103700Work* work;
    GsCOORDINATE2*   obj;
    Task*            player;
    void*            head;
    GpAnimArg*       arg;
    s32              sound;

    work               = (Actor103700Work*)task->work;
    obj                = task->extra.tmd->coords;
    player             = gameGetPtrSlot(3);
    head               = SCRATCH_HEAD(void);
    SCRATCH_HEAD(void) = (u8*)head - 0x1C;
    arg                = (GpAnimArg*)SCRATCH_HEAD(void);

    switch (work->field_250) {
        case 0:
            Gp_DispatchMsg(player, 0x3F9, Gp_PackPair(&Actor03700_D07F08, 0), 0);
            func_800FDB18(1, (GsCOORDINATE2*)obj, NULL, &work->field_224);
            Gp_SpawnPadLerp(5, 0xC0, 8);
            sound = ((((GpEnemy*)task->spawnArg2)->placeKey >> 12) << 8) | 0x40250004;
            SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
            if ((s16)++work->field_26C >= 6) {
                work->field_26C      = 0;
                work->field_250      = 3;
                Gp_StateF0.field_19 |= 2;
            } else {
                work->field_250 = 1;
                work->field_256 = 20;
            }
            break;
        case 1:
            work->field_252 = -20;
            if ((s16)--work->field_256 <= 0) {
                work->field_250 = 2;
            }
            break;
        case 2:
            work->field_23C.vx = Player_Status.coordMtx->t[0];
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_23C.vy = Player_Status.coordMtx->t[1] - (((Gp_LcgState >> 16) & 0x3FF) + 800);
            work->field_23C.vz = Player_Status.coordMtx->t[2];
            work->field_252    = Actor03700_D07F1C[((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
            work->field_254    = Actor03700_D07F7C[((GpEnemy*)task->spawnArg2)->place->rowIndex];
            if (work->field_264 != 0) {
                work->field_250 = 0;
            }
            break;
        case 3:
            arg->animBlock.ptr = Actor03700_D080FC;
            arg->field_4       = 2;
            arg->field_8       = 0;
            arg->field_C       = 0;
            arg->field_10      = 1;
            Gp_DispatchMsg(player, 0x3F4, (s32)arg, 0);
            sound = ((((GpEnemy*)task->spawnArg2)->placeKey >> 12) << 8) | 6;
            SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
            work->field_250 = 4;
            break;
        case 4:
            if (Gp_DispatchMsg(player, 0x3ED, 0, 0) == 0) {
                Gp_DispatchMsg(player, 0x3F1, 0, 0);
                work->field_262      = 0;
                work->field_24E      = 5;
                work->field_250      = 0;
                Gp_StateF0.field_19 &= 1;
            }
            break;
    }
    Actor03700_Fn032BC(task, 1, 14);
    SCRATCH_POP_BYTES(0x1C);
}

void Actor03700_Fn018C8(Task* task)
{
    Actor103700Work* work;
    GsCOORDINATE2*   obj;
    s32              period;
    s32              i;
    s32              slot;
    s32              sound;

    work   = (Actor103700Work*)task->work;
    obj    = task->extra.tmd->coords;
    period = 14;

    switch (work->field_250) {
        case 0:
            work->field_250 = 1;
            work->field_256 = 30;
            work->field_258 = Actor03700_D07F3C[((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
            work->field_26C = 0;
            slot            = (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 3) + 10;
            for (i = 1; i < 6; i++) {
                work->slots[i].rate = slot;
            }
            break;
        case 1:
            work->field_23C.vx = Player_Status.coordMtx->t[0];
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_23C.vy = Player_Status.coordMtx->t[1] - (((Gp_LcgState >> 16) & 0x3FF) + 800);
            work->field_23C.vz = Player_Status.coordMtx->t[2];
            work->field_254    = Actor03700_D07F7C[((GpEnemy*)task->spawnArg2)->place->rowIndex];
            if ((s16)--work->field_258 > 0) {
                work->field_252 = -50;
            } else {
                work->field_252 = 0;
                Actor03700_Fn03320(task, 20);
            }
            if ((s16)--work->field_256 <= 0) {
                work->field_250 = 2;
                work->field_256 = 0;
                work->field_252 = 0;
            }
            break;
        case 2:
            work->field_23C.vx = Player_Status.coordMtx->t[0];
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_23C.vy = Player_Status.coordMtx->t[1] - (((Gp_LcgState >> 16) & 0x1FF) + 800);
            work->field_23C.vz = Player_Status.coordMtx->t[2];
            work->field_254    = Actor03700_D07F7C[((GpEnemy*)task->spawnArg2)->place->rowIndex];
            work->field_252    = 5;
            Actor03700_Fn03320(task, 20);
            if ((s16)++work->field_256 >= 91) {
                work->field_256 = 0;
                work->field_24E = 3;
                work->field_250 = 0;
            }
            break;
        case 3:
            work->field_256 = Actor03700_D07F5C[((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
            work->field_258 = 15;
            work->field_250 = 4;
            work->field_254 = 0;
            sound           = ((((GpEnemy*)task->spawnArg2)->placeKey >> 12) << 8) | 0x40250003;
            SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
            break;
        case 4:
            if ((s16)--work->field_258 > 0) {
                work->field_252 = -125;
            } else {
                work->field_252 = 0;
            }
            if ((s16)--work->field_256 <= 0) {
                work->field_256 = 0;
                work->field_24E = 3;
                work->field_250 = 0;
            }
            Actor03700_Fn03320(task, 80);
            period = 21;
            break;
    }
    Actor03700_Fn032BC(task, 0, period);
}

void Actor03700_Fn01C94(Task* task)
{
    Actor103700Work* work;
    GsCOORDINATE2*   obj;
    Task*            player;
    void*            head;
    GpAnimArg*       arg;
    s32              sound;
    s32              pan;

    work               = (Actor103700Work*)task->work;
    obj                = task->extra.tmd->coords;
    player             = gameGetPtrSlot(3);
    head               = SCRATCH_HEAD(void);
    SCRATCH_HEAD(void) = (u8*)head - sizeof(GpAnimArg);
    arg                = (GpAnimArg*)SCRATCH_HEAD(void);

    switch (work->field_250) {
        case 0:
            arg->animBlock.ptr = Actor03700_D080FC;
            arg->field_4       = 2;
            arg->field_8       = 0;
            arg->field_C       = 0;
            arg->field_10      = 1;
            Gp_DispatchMsg(player, 0x3F4, (s32)arg, 0);
            sound = ((((GpEnemy*)task->spawnArg2)->placeKey >> 12) << 8) | 6;
            pan   = (s8)Gp_GetObjPan(obj);
            SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(obj));
            work->field_250 = 1;
            break;
        case 1:
            if (Gp_DispatchMsg(player, 0x3ED, 0, 0) == 0) {
                Gp_DispatchMsg(player, 0x3F1, 0, 0);
                work->field_262 = 0;
                work->field_24E = 5;
                work->field_250 = 0;
            }
            break;
    }
    SCRATCH_POP_BYTES(sizeof(GpAnimArg));
}

/// Tests whether the actor has noticed the player: true when the player is
/// under 0x708 units away on the XZ plane (the offset is staged on the
/// scratchpad stack), mid-action (`Gp_StateF0.field_2` low nibble) or holding
/// the aim button (`field_19` bit 0). On noticing, it arms `Gp_StateF0` and
/// plays the alert cue from the placement's sound bank. Returns 1 when noticed.
s32 Actor03700_Fn01DFC(Task* task)
{
    void**         scratch;
    u8*            head;
    SVECTOR*       vec;
    GsCOORDINATE2* coord;
    s16            dx;
    s16            dz;
    s32            ret;
    u32            soundId;
    s32            pan;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    vec                            = (SVECTOR*)(head - 8);
    coord                          = task->extra.tmd->coords;
    vec->vx                        = (u16)Player_Status.coordMtx->t[0] - (u16)coord->coord.t[0];
    dz                             = (u16)Player_Status.coordMtx->t[2] - (u16)coord->coord.t[2];
    SCRATCH_HEAD_AT(scratch, void) = vec;
    vec->vz                        = dz;
    dx                             = ((SVECTOR*)(head - 8))->vx;
    ret                            = 0;
    if ((SquareRoot0((dx * dx) + (dz * dz)) < 0x708) || (Gp_StateF0.field_2 & 0xF) || (Gp_StateF0.field_19 & 1)) {
        ret = 1;
        Gp_ArmStateF0(ret);
        soundId   = ((GpEnemy*)task->spawnArg2)->placeKey;
        soundId >>= 0xC;
        soundId <<= 8;
        soundId  |= 0x40250000 | ret;
        pan       = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(coord));
    }
    SCRATCH_POP_BYTES(8);
    return ret;
}

/// Turns the root coordinate towards the target position `field_23C`:
/// `field_244` becomes the heading to the target, and `field_246` steps from
/// the matrix's current heading towards it by at most `field_254`, taking the
/// short way round. The result is written back as a Y rotation.
void Actor03700_Fn01F48(Task* task)
{
    Actor103700Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR*         rot;
    u16              want;
    s16              ang;
    s16              diff;
    s32              adiff;
    s32              step;
    s32              cur;
    s32              next;
    s32              wrapStep;

    coord           = task->extra.tmd->coords;
    work            = (Actor103700Work*)task->work;
    rot             = (SVECTOR*)SCRATCH_PUSH_BYTES(8);
    rot->vx         = work->field_23C.vx - coord->coord.t[0];
    rot->vy         = 0;
    rot->vz         = work->field_23C.vz - coord->coord.t[2];
    work->field_244 = ratan2(rot->vx, rot->vz) & 0xFFF;
    ang             = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]);
    want            = work->field_244;
    ang            &= 0xFFF;
    diff            = want - ang;
    adiff           = diff >= 0 ? diff : -diff;

    work->field_246 = ang;
    if (adiff < 0x800) {
        step = work->field_254;
        if (step >= adiff) {
            work->field_246 = want;
        } else {
            next = ang;
            if (diff <= 0) {
                next -= step;
            } else {
                next += step;
            }
            goto store;
        }
    } else {
        step = work->field_254;
        if (diff > 0) {
            if (step >= 0x1000 - diff) {
                goto snap;
            } else {
                goto turn;
            }
        } else if (step >= 0x1000 + diff) {
            goto snap;
        } else {
            goto turn;
        }
    snap:
        work->field_246 = work->field_244;
        goto done;
    turn:
        wrapStep = work->field_254;
        cur      = work->field_246;
        if (diff > 0) {
            next = cur - wrapStep;
        } else {
            next = cur + wrapStep;
        }
    store:
        work->field_246 = next;
    }
done:
    rot->vx = 0;
    rot->vy = work->field_246;
    rot->vz = 0;
    RotMatrix(rot, &coord->coord);
    SCRATCH_POP_BYTES(8);
}

/// Death handler. Mode 1 of `Gp_StateF0.field_4` only refreshes the actor colour and
/// mode 2 hides the model; otherwise it steps `field_250`: unlink the enemy and
/// play the death cue (releasing the player's hold if `field_262` is set), wait
/// out a short delay, spawn the `field_268` death effect, let the player go,
/// count 60 frames and destroy the enemy.
void Actor03700_Fn020D4(GpEnemy* enemy, Task* task)
{
    TmdObject*       model;
    GsCOORDINATE2*   obj;
    Actor103700Work* work;
    Task*            player;
    GpAnimArg        arg;
    /* One 12-byte frame slot serves both the colour vector and the area key. */
    union {
        VECTOR    color;
        GpAreaKey key;
    } buf;
    GpAreaKey*     sessionKey;
    GpAreaKey*     keyPtr;
    u8             areaByte0;
    GpAreaRec*     rec;
    GpAreaPlace*   entry;
    GpEffWork*     eff;
    TmdObject*     effModel;
    s32            sound;
    s32            sound2;
    s32            idx;
    u32            raw;
    GsCOORDINATE2* coord;

    work   = (Actor103700Work*)task->work;
    obj    = task->extra.tmd->coords;
    model  = task->extra.tmd;
    player = gameGetPtrSlot(3);

    switch (Gp_StateF0.field_4) {
        case 1:
            coord        = task->extra.tmd->coords;
            buf.color.vx = coord->workm.t[0];
            buf.color.vy = coord->workm.t[1];
            buf.color.vz = coord->workm.t[2];
            Gp_UpdateActorColor(task->spawnArg2, &buf.color, 0, 0);
            return;
        case 2:
            task->extra.tmd->flags |= 0x80;
            return;
        case 0:
        default:
            if (work->field_266 != 0) {
                Actor03700_Fn0355C(task);
            }
            switch (work->field_250) {
                case 0:
                    enemy->recs = 0;
                    Gp_UnlinkObj(&work->obj);
                    Gp_UnlinkNode(&enemy->node);
                    Gp_ReleaseStateF0Add(task, 0x25);
                    model->flags = 0x80;
                    sound        = ((((GpEnemy*)task->spawnArg2)->placeKey >> 12) << 8) | 0x40250003;
                    SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
                    if (work->field_262 != 0) {
                        arg.animBlock.ptr = Actor03700_D080FC;
                        arg.field_4       = 2;
                        arg.field_8       = 0;
                        arg.field_C       = 0;
                        arg.field_10      = 1;
                        Gp_DispatchMsg(player, 0x3F4, (s32)&arg, 0);
                        sound2 = ((((GpEnemy*)task->spawnArg2)->placeKey >> 12) << 8) | 6;
                        SndEvt_EnqueueType6(sound2, (s8)Gp_GetObjPan(obj), (s8)gpGetObjDepth(obj));
                    }
                    work->field_250 = 1;
                    work->field_258 = (((GpEnemy*)task->spawnArg2)->placeKey >> 12) % 6 + 2;
                    break;
                case 1:
                    if ((s16)--work->field_258 > 0) {
                        break;
                    }
                    switch (work->field_268) {
                        case 0:
                            Tmd_FreeBuffers(model);
                            model->flags |= 4;
                            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                            if ((Gp_LcgState >> 16) & 1) {
                                D_80067704[0] = Actor03700_D043FC;
                            } else {
                                D_80067704[0] = Actor03700_D04600;
                            }
                            eff = Gp_SpawnEff(0x40007, &task->extra.tmd->coords[4], 0x80, NULL);
                            if (eff != NULL) {
                                sessionKey    = (GpAreaKey*)&gGameSession->at4.loc;
                                raw           = ((GpEnemy*)task->spawnArg2)->placeKey;
                                effModel      = eff->task->extra.tmd;
                                buf.key.stage = sessionKey->stage;
                                buf.key.area  = sessionKey->area;
                                buf.key.room  = sessionKey->room;
                                keyPtr        = &buf.key;
                                TOUCH_REG(keyPtr);
                                areaByte0    = gGameSession->at4.loc.view;
                                idx          = raw >> 12;
                                buf.key.view = areaByte0;
                                Gp_SyncAreaKeyIndex(keyPtr);
                                rec             = Gp_GetNestedAreaRec(&buf.key);
                                entry           = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
                                effModel->tpage = entry->tpage;
                                effModel->clut  = entry->clut;
                                if (effModel->buffer != NULL) {
                                    tmdProcessStream(effModel);
                                    tmdProcessStream(effModel);
                                }
                            }
                            break;
                        case 1:
                            Gp_SpawnEff(0x60080, obj, 0x10280, NULL);
                            break;
                        case 2:
                            Gp_SpawnEff(0x60055, obj, 0x10013380, NULL);
                            Gp_SpawnEff(0x60055, obj, 0x10111300, NULL);
                            break;
                    }
                    work->field_250 = 2;
                    break;
                case 2:
                    if (work->field_262 != 0) {
                        if (Gp_DispatchMsg(player, 0x3ED, 0, 0) == 0) {
                            Gp_DispatchMsg(player, 0x3F1, 0, 0);
                            work->field_250 = 3;
                            work->field_256 = 60;
                            work->field_262 = 0;
                        }
                    } else {
                        work->field_250 = 3;
                        work->field_256 = 60;
                    }
                    break;
                case 3:
                    if ((s16)--work->field_256 < 0) {
                        work->field_250 = 4;
                    }
                    break;
                case 4:
                    if (work->field_266 == 0 || work->field_266 == 2) {
                        Gp_DestroyEnemy(enemy, task);
                    }
                    break;
            }
            break;
    }
}

/// Mode 7, the drop-in: keeps the actor hidden, unlockable and out of the
/// contact passes until `Gp_StateF0.field_1A` reaches the placement's `mode - 9`, then
/// counts `field_256` down from 5 and picks a target position `field_23C` above
/// the root coordinate from `Gp_LcgState` - a lower one and mode 8 for
/// placements below 10, a higher one and mode 9 above - before re-enabling the
/// contacts, rearming a random countdown and allocating the model buffers.
void Actor03700_Fn025C8(Task* task)
{
    TmdObject*       obj;
    TmdObject*       ext;
    Actor103700Work* work;
    GpEnemy*         spawn;
    GsCOORDINATE2*   coord;
    s32              diff;

    ext                       = task->extra.tmd;
    work                      = (Actor103700Work*)task->work;
    coord                     = ext->coords;
    spawn                     = (GpEnemy*)task->spawnArg2;
    obj                       = ext;
    work->obj.flags          &= 0x3FFF;
    obj->flags               |= 0x84;
    spawn->node.state.b.flags = 1;

    switch (work->field_250) {
        case 0:
            diff = spawn->place->mode - 9;
            if (Gp_StateF0.field_1A >= diff) {
                work->field_250 = 1;
                work->field_256 = 5;
            }
            break;
        case 1:
            diff = spawn->place->mode - 9;
            if ((s16)--work->field_256 <= 0) {
                work->obj.flags |= 0xC000;
                if (diff < 10) {
                    work->field_24E    = 8;
                    Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                    work->field_23C.vx = (u16)coord->coord.t[0] + ((Gp_LcgState >> 16) & 0xFF);
                    Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                    work->field_23C.vy = (u16)coord->coord.t[1] - (((Gp_LcgState >> 16) & 0x1FF) + 0x352);
                    Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                    work->field_23C.vz = (u16)coord->coord.t[2] + ((Gp_LcgState >> 16) & 0xFF);
                } else {
                    work->field_24E    = 9;
                    Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                    work->field_23C.vx = (u16)coord->coord.t[0] + ((Gp_LcgState >> 16) & 0xFF);
                    Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                    work->field_23C.vy = (u16)coord->coord.t[1] - (((Gp_LcgState >> 16) & 0x1FF) + 0x73A);
                    Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                    work->field_23C.vz = (u16)coord->coord.t[2] + ((Gp_LcgState >> 16) & 0xFF);
                }
                work->field_250 = 0;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_256 = (Gp_LcgState >> 16) & 0x1F;
                Tmd_AllocBuffers(obj);
                obj->flags &= ~4;
            }
            break;
    }
}

/// Modes 8 and 9, the approach after the drop-in: counts `field_256` down,
/// then moves the root coordinate towards the target position `field_23C` by
/// 75/2048 of the unit direction per frame until it is within 150 on Y, then
/// counts 30 frames and hands over to mode 3, arming `Gp_StateF0`.
void Actor03700_Fn027DC(Task* task)
{
    Actor103700SteerScratch* s;
    Actor103700Work*         work;
    GsCOORDINATE2*           coord;
    s32                      d;

    s     = (Actor103700SteerScratch*)SCRATCH_PUSH_BYTES(sizeof(Actor103700SteerScratch));
    work  = (Actor103700Work*)task->work;
    coord = task->extra.tmd->coords;
    switch (work->field_250) {
        case 0:
            if ((s16)--work->field_256 <= 0) {
                work->field_250 = 1;
            }
            break;
        case 1:
            s->delta.vx = work->field_23C.vx - coord->coord.t[0];
            s->delta.vy = work->field_23C.vy - coord->coord.t[1];
            s->delta.vz = work->field_23C.vz - coord->coord.t[2];
            VectorNormalS(&s->delta, &s->normal);
            coord->coord.t[0] += (s->normal.vx * 75) >> 11;
            coord->coord.t[1] += (s->normal.vy * 75) >> 11;
            coord->coord.t[2] += (s->normal.vz * 75) >> 11;
            d                  = (s32)work->field_23C.vy - coord->coord.t[1];
            if ((d < 0 ? -d : d) < 150) {
                work->field_250 = 2;
                work->field_256 = 30;
            }
            break;
        case 2:
            if ((s16)--work->field_256 <= 0) {
                work->field_24E = 3;
                work->field_250 = 0;
                work->field_256 = 0;
                Gp_ArmStateF0(1);
            }
            break;
    }
    SCRATCH_POP_BYTES(sizeof(Actor103700SteerScratch));
}

void Actor03700_Fn029C0(Task* task)
{
    Actor103700Work*         work;
    TmdObject*               obj;
    GsCOORDINATE2*           coord;
    Actor103700SteerScratch* scratch;
    s32                      mode;
    GpEnemy*                 ctx;

    scratch                               = SCRATCH_HEAD(Actor103700SteerScratch) - 1;
    SCRATCH_HEAD(Actor103700SteerScratch) = scratch;
    obj                                   = task->extra.tmd;
    coord                                 = obj->coords;
    work                                  = (Actor103700Work*)task->work;
    mode                                  = work->field_250;
    ctx                                   = (GpEnemy*)task->spawnArg2;

    switch (mode) {
        case 0:
            work->obj.flags        &= 0x3FFF;
            obj->flags             |= 0x84;
            ctx->node.state.b.flags = 1;
            if (Gp_StateF0.field_1A == 0) {
                work->field_250    = 1;
                work->obj.flags   |= 0xC000;
                Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                work->field_23C.vx = coord->coord.t[0] - (((Gp_LcgState >> 16) & 0x1FF) + 500);
                Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                work->field_23C.vy = coord->coord.t[1] + (((Gp_LcgState >> 16) & 0x1FF) + 3500);
                Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                work->field_23C.vz = coord->coord.t[2] + (((Gp_LcgState >> 16) & 0x1FF) + 2000);
                Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
                work->field_256    = (Gp_LcgState >> 16) & 0x1F;
                Tmd_AllocBuffers(obj);
                obj->flags &= ~4;
            }
            break;
        case 1:
            if ((s16)--work->field_256 <= 0) {
                work->field_250 = 2;
            }
            break;
        case 2:
            scratch->delta.vx = work->field_23C.vx - coord->coord.t[0];
            scratch->delta.vy = work->field_23C.vy - coord->coord.t[1];
            scratch->delta.vz = work->field_23C.vz - coord->coord.t[2];
            VectorNormalS(&scratch->delta, &scratch->normal);
            coord->coord.t[0] += (scratch->normal.vx * 5) >> 9;
            coord->coord.t[1] += (scratch->normal.vy * 5) >> 9;
            coord->coord.t[2] += (scratch->normal.vz * 5) >> 9;
            work->field_254    = Actor03700_D07F7C[((GpEnemy*)task->spawnArg2)->place->rowIndex];
            Actor03700_Fn01F48(task);

            Actor03700_BobInline(task, 0, 21);
            Actor03700_SwayInline(task, 80);

            if (abs(work->field_23C.vy - coord->coord.t[1]) < 40) {
                work->field_250 = 3;
                work->field_256 = 30;
                Gp_ArmStateF0(1);
            }
            break;
        case 3:
            work->field_23C.vx = Player_Status.coordMtx->t[0];
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_23C.vy = Player_Status.coordMtx->t[1] - (((Gp_LcgState >> 16) & 0x3FF) + 800);
            work->field_23C.vz = Player_Status.coordMtx->t[2];
            work->field_254    = Actor03700_D07F7C[((GpEnemy*)task->spawnArg2)->place->rowIndex];
            Actor03700_Fn01F48(task);

            Actor03700_BobInline(task, 0, 21);
            Actor03700_SwayInline(task, 80);

            if ((s16)work->field_256 == 30) {
                Gp_StateF0.field_1A = 2;
            }
            if ((s16)--work->field_256 <= 0) {
                work->field_24E = mode;
                work->field_250 = 0;
                work->field_256 = 0;
            }
            break;
    }
}

/// The actor's per-frame task callback: runs the handler for the task's state
/// from `Actor03700_D00004` (spawn, tick, death), passing the enemy record the
/// task was spawned with. The table is copied onto the stack before the call.
void Actor03700_Fn02FA8(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor03700_D00004;
    sp.funcs[task->state](task->spawnArg2, task);
}

void Actor03700_Fn03004(GpEnemy* enemy, Task* task)
{
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    Actor103700Work* work;
    s32              state;
    s32              one;

    obj   = task->extra.tmd;
    state = Gp_StateF0.field_4;
    work  = (Actor103700Work*)task->work;
    coord = obj->coords;
    one   = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    obj->flags                = 0;
    enemy->node.state.b.flags = 0;
    goto default_body;
case2:
    obj->flags               |= 0x80;
    enemy->node.state.b.flags = one;
    return;
default_body:
    if (work->field_24E < 7) {
        Actor03700_Fn0042C(task, obj, one);
    }
    if (Actor03700_Fn008D0(task) != 0) {
        return;
    }
    if (work->field_254 != 0) {
        Actor03700_Fn01F48(task);
    }
    if (work->field_252 != 0) {
        Actor03700_Fn0321C(task);
    }
    if (work->field_266 != 0) {
        Actor03700_Fn0355C(task);
    }
    Actor03700_Fn033F0(task);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
case1:
    Actor03700_Fn034A0(task);
}

/// Asks the player for the melee hold (message 0x3F8, range 8) and, once it is
/// accepted, starts the grab on the actor's animation slot (message 0x3FF) and
/// flags `Actor103700Work::field_262`. The task's own unit is held for as long
/// as `GameActor::field_954` stays out of mode 2; the two message buffers come
/// from one 0x2C-byte `G_SCRATCH_HEAD` push.
s32 Actor03700_Fn03130(Task* task)
{
    Actor103700Work*        work;
    Task*                   player;
    void*                   head;
    Actor103700HoldScratch* scratch;
    s32                     ret;

    work               = (Actor103700Work*)task->work;
    player             = gameGetPtrSlot(3);
    head               = SCRATCH_HEAD(void);
    SCRATCH_HEAD(void) = (u8*)head - sizeof(Actor103700HoldScratch);
    scratch            = (Actor103700HoldScratch*)SCRATCH_HEAD(void);

    ret = 0;
    if (((GameActor*)player->work)->field_954 != 2) {
        scratch->query.field_14 = 8;
        if (Gp_DispatchMsg(player, 0x3F8, (s32)scratch, 0) == 0) {
            scratch->anim.animBlock.ptr = Actor03700_D080FC;
            scratch->anim.field_4       = 1;
            scratch->anim.field_8       = 0;
            scratch->anim.field_C       = 0;
            scratch->anim.field_10      = 1;
            Gp_DispatchMsg(player, 0x3FF, (s32)&scratch->anim, 0);
            work->field_262 = 1;
            ret             = 1;
        }
    }
    SCRATCH_POP_BYTES(sizeof(Actor103700HoldScratch));
    return ret;
}

/// Moves the actor forward: remembers the root coordinate's position in
/// `field_22C` (where a collision reset returns it), steps it along the
/// matrix's third column scaled by `field_252`, and moves its height 30 units
/// toward the target's `field_23C.vy`.
void Actor03700_Fn0321C(Task* task)
{
    GsCOORDINATE2*   coord;
    Actor103700Work* work;
    s32              y;

    coord = task->extra.tmd->coords;
    work  = (Actor103700Work*)task->work;

    work->field_22C.vx = coord->coord.t[0];
    work->field_22C.vy = coord->coord.t[1];
    work->field_22C.vz = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_252) >> 12;
    y                  = coord->coord.t[1];
    coord->coord.t[1]  = (work->field_23C.vy - y > 0) ? y + 30 : y - 30;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_252) >> 12;
}

void Actor03700_Fn032BC(Task* task, s32 arg1, s32 arg2)
{
    Actor103700Work* work;
    GsCOORDINATE2*   coord;
    u16              frame;

    work  = (Actor103700Work*)task->work;
    coord = task->extra.tmd->coords;

    frame           = work->field_25E + 1;
    work->field_25E = frame;
    if (arg2 < (s16)frame) {
        work->field_25E = 0;
    }
    coord->coord.t[1] += Actor03700_D07F98[(arg1 * 15) + (s16)work->field_25E];
}

void Actor03700_Fn03320(Task* task, s32 arg1)
{
    Actor103700Work* work;
    GsCOORDINATE2*   coord;
    u16              frame;
    s32              amp;

    work  = (Actor103700Work*)task->work;
    coord = task->extra.tmd->coords;

    frame           = work->field_25C + 1;
    work->field_25C = frame;
    if ((s16)frame >= 15) {
        work->field_25C = 0;
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        work->field_25A = arg1 + ((Gp_LcgState >> 16) & 0x3F);
    }
    amp                = (work->field_25A * Actor03700_D07FD4[(s16)work->field_25C] * 16) >> 16;
    coord->coord.t[0] += (amp * coord->coord.m[0][0]) >> 12;
    coord->coord.t[2] += (amp * coord->coord.m[2][0]) >> 12;
}

/// Drives the five animation slots from the requested animation `field_248`.
/// When the request differs from the one playing (`field_24A`), it is
/// latched, the frame counter `field_24C` restarts and every slot is pointed
/// at it with a blend of 4; otherwise the counter ticks and each slot advances.
void Actor03700_Fn033F0(Task* task)
{
    Actor103700Work* work;
    s32              i;

    work = (Actor103700Work*)task->work;
    i    = 1;
    if (work->field_248 != work->field_24A) {
        work->field_24A = (u16)work->field_248;
        work->field_24C = 0;
        do {
            func_800B4114(work, i, work->field_248, 0, 4);
            i++;
        } while (i < 6);
        return;
    }
    TOUCH_REG(i);
    work->field_24C += i;
    do {
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 6);
}

/// Refreshes the actor's colour from the world position of its root
/// coordinate, with no blend parameters.
void Actor03700_Fn034A0(Task* task)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = task->extra.tmd->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(task->spawnArg2, &vec, 0, 0);
}

/// Handler for message 0x7DE. Ignored unless the actor is in one of its
/// active modes (below 7) and the task is in its tick state. While the actor
/// holds the player (`field_262`) it moves the hold to its release phase
/// (`field_250` = 3); otherwise it drops to mode 5 with the base animation
/// requested. Always answers 0.
s32 Actor03700_Fn034F8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    Actor103700Work* work;
    s32              state;

    work = (Actor103700Work*)task->work;
    if (work->field_24E >= 7) {
        return 0;
    }
    state = task->state;
    if (state != 1) {
        return 0;
    }
    if (work->field_262 != 0) {
        work->field_250 = 3;
    } else {
        work->field_24E = 5;
        work->field_248 = state;
        work->field_250 = 0;
    }
    return 0;
}

void Actor03700_Fn0355C(Task* task)
{
    Actor103700Work* work = (Actor103700Work*)task->work;
    s32              state;

    switch (Gp_StateF0.field_1A) {
        case 0:
            break;
        case 1:
            Gp_StateF0.field_1A = 2;
            return;
        case 2:
            if (Gp_StateF0.field_6 < 0x11) {
                Gp_StateF0.field_1A = 3;
                return;
            }
            break;
        case 3:
            if (Gp_StateF0.field_6 < 0xE) {
                Gp_StateF0.field_1A = 4;
                return;
            }
            break;
        case 4:
            if (Gp_StateF0.field_6 < 0xA) {
                Gp_StateF0.field_1A = 5;
                return;
            }
            break;
        case 5:
            state = task->state;
            if (state == 2 && Gp_StateF0.field_6 == 0) {
                work->field_266 = state;
            }
            break;
    }
}
