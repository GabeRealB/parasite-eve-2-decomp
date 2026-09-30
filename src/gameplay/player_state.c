#include "gameplay/player_state.h"

#include <psyq/libgte.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "damage.h"
#include "gameplay/effects.h"
#include "items.h"
#include "loading.h"
#include "gameplay/message.h"
#include "gameplay/object_fields.h"
#include "gameplay/player_actor.h"
#include "player_actor.h"
#include "player_state.h"
#include "gameplay/room_effects.h"
#include "room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "actors/companion.h"

#include "main/display.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/wipsys.h"

/// 0x14-byte scratch from the scratch stack used by `func_8010BD88`.
/// `vx`/`vy`/`vz` overlay a `VECTOR3` for `func_80103C74`; `angle` holds
/// the `ratan2` result and the clamped turn delta applied to
/// `GameActor.field_52`.
typedef struct _GpTurnScratch {
    /* 0x00 */ s32 vx;
    /* 0x04 */ s32 vy;
    /* 0x08 */ s32 vz;
    /* 0x0C */ s32 pad;
    /* 0x10 */ s32 angle;
} GpTurnScratch;
STATIC_ASSERT_SIZEOF(GpTurnScratch, 0x14);

/// 0x40-byte scratch from the scratch stack used by `func_80109BB4`.
/// `pos` is the world position of the colliding `WorldCollisionBody` (`pos` rotated by
/// `coord->workm`, plus that matrix's translation), later
/// reused to save the actor's pre-push `coord.t[0]` / `t[2]`. `delta` is
/// `pos` minus the contact point, `unit` its `VectorNormal`, and `local`
/// that direction in grid space via `Gp_GridParams->field_0->workm`.
typedef struct _GpPushBackScratch {
    /* 0x00 */ VECTOR pos;
    /* 0x10 */ VECTOR delta;
    /* 0x20 */ VECTOR local;
    /* 0x30 */ VECTOR unit;
} GpPushBackScratch;
STATIC_ASSERT_SIZEOF(GpPushBackScratch, 0x40);

/// Temporary origin and target delta for adjusting an actor's aim yaw.
typedef struct {
    VECTOR3  vec;        // Target minus the aim origin, in game coordinates
    byte     field_C[4]; // Unused by this helper; purpose unproven
    SVECTOR  offset;     // Local position offset from model part 4; zero for this helper
    GfxCoord coord;      // Aim origin, parented to the view node
} _PlayerActorAimScratch;
STATIC_ASSERT_SIZEOF(_PlayerActorAimScratch, 0x68);

extern u16 D_80113F9C[70];

extern const TaskFuncTable4 D_80097AB0;

static inline void _gpResumeBaseState(Task* arg0);

static void func_8010AAB4(Task* arg0);

static void func_8010AB70(Task* arg0);

static void func_8010AE98(Task* arg0);

static void func_8010AF04(Task* arg0);

static void func_8010AF6C(Task* arg0);

static void func_8010AFC0(Task* arg0);

static void func_8010B010(Task* arg0);

static void func_8010B060(Task* arg0);

static void func_8010B0C8(Task* arg0);

static s32 Gp_TestHpDamage(s32 arg0);

static void func_8010B2D4(Task* arg0, WorldCollisionContact* arg1, s32 arg2);

static void func_8010B348(Task* arg0, WorldCollisionContact* arg1, s32 arg2);

static void func_8010B590(Task* arg0);

static void func_8010B5C0(Task* arg0);

static void func_8010B5E4(Task* arg0);

static void func_8010B5F0(Task* arg0);

s32 func_8010C30C(Task* arg0);

static void func_8010C46C(Task* arg0);

s32 func_8010C4F0(Task* task, s32 msgId, AnimationPlayRequest* request);

s32 func_8010C648(Task* task, s32 msgId, AnimationPlayRequest* request);

s32 func_8010C688(Task* arg0, s32 arg1, ActorTransform* transform, s32 arg3);

s32 func_8010C6C8(Task* arg0, s32 arg1, ActorTransform* transform, GpOverrideArg* arg3);

s32 func_8010C708(Task* arg0, s32 arg1, ActorTransform* transform, GpOverrideArg* arg3);

s32 func_8010C75C(Task* arg0, s32 arg1, GpDelayArg* arg2);

void Gp_MoveActorByKeep(Task* arg0, s32 arg1, GpMoveArg* arg2);

s32 Gp_CopyAllyAnim(Task* arg0, s32 arg1, GpCopyArg* arg2);

static inline void _gpResumeBaseState(Task* arg0)
{
    GameActor* inner;

    inner = arg0->work;
    func_8010B210(arg0);
    inner->field_97A = 0x12;
    if (inner->field_956 != 0) {
        func_8010870C(arg0, 0xC);
    } else {
        func_801066DC(arg0, 0);
    }
}

u16 D_80113F9C[70] = {
    0,
    0,
    0,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    1,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    1,
    1,
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
    0,
    0,
    1,
    1,
    1,
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
};

void func_80109BB4(Task* arg0, WorldCollisionContact* arg1)
{
    u8*                    head;
    GpPushBackScratch*     s;
    GameActor*             actor;
    GfxCoord*              coord;
    WorldCollisionContact* rec;
    WorldCollisionBody*    obj;
    VECTOR*                delta;
    s32                    i;
    s32                    best;
    s32                    push;
    s32                    id;
    s32                    val;

    rec                        = arg1;
    best                       = 0;
    i                          = 0;
    head                       = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(void) = head - 0x40;
    s                          = (GpPushBackScratch*)(head - 0x40);
    actor                      = arg0->work;
    coord                      = arg0->extra.tmd->coords;

    for (i = 0; i < 0x12; rec++, i++) {
        delta = &s->delta;
        if (rec->flags & 1) {
            switch (rec->key.parts.kind) {
                case 0:
                case 1:
                case 2:
                    break;
                case 3:
                    if ((s8)actor->field_992 != 0) {
                        break;
                    }
                    id = rec->key.parts.id;
                    if (id < 0x46 && D_80113F9C[id] == 1) {
                        obj = &((WorldCollisionBody*)actor->field_AC)[(u8)rec->flags >> 4];
                        gte_SetRotMatrix(&obj->coord->workm);
                        gte_ldv0(&obj->pos);
                        gte_rtv0();
                        gte_stlvnl(&s->delta);
                        s->pos.vx = (obj->coord)->workm.t[0] +
                                    s->delta.vx;
                        s->pos.vy =
                            (obj->coord)->workm.t[1] + s->delta.vy;
                        s->pos.vz =
                            (obj->coord)->workm.t[2] + s->delta.vz;
                        s->delta.vx = s->pos.vx - rec->point.vx;
                        s->delta.vy = s->pos.vy - rec->point.vy;
                        s->delta.vz = s->pos.vz - rec->point.vz;
                        push        = rec->distance - SquareRoot0(s->delta.vx * s->delta.vx +
                                                                  s->delta.vy * s->delta.vy +
                                                                  s->delta.vz * s->delta.vz);
                        val         = push;
                        if (push < 0) {
                            val = 0;
                        }
                        push = val;
                        if (best < push) {
                            best = push;
                            VectorNormal(delta, &s->unit);
                            ApplyTransposeMatrixLV(&Gp_GridParams->field_0->workm,
                                                   &s->unit, &s->local);
                        }
                    }
                    break;
                case 4:
                    func_8010B2D4(arg0, rec, (u8)rec->flags >> 4);
                    break;
                case 5:
                    func_8010B348(arg0, rec, (u8)rec->flags >> 4);
                    break;
            }
        }
    }

    if (best > 0) {
        actor->field_986    = 1;
        actor->field_30.vx  = coord->workm.t[0];
        actor->field_30.vy  = coord->workm.t[1];
        actor->field_30.vz  = coord->workm.t[2];
        s->pos.vx           = coord->coord.t[0];
        s->pos.vz           = coord->coord.t[2];
        coord->coord.t[0]  += (best * s->local.vx) >> 12;
        coord->coord.t[2]  += (best * s->local.vz) >> 12;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(coord);
        actor->field_30.vx = coord->workm.t[0] - actor->field_30.vx;
        actor->field_30.vy = coord->workm.t[1] - actor->field_30.vy;
        actor->field_30.vz = coord->workm.t[2] - actor->field_30.vz;
        VectorNormal(&actor->field_30, &actor->field_30);
        coord->coord.t[0]   = s->pos.vx + ((best * s->local.vx) >> 14);
        coord->coord.t[2]   = s->pos.vz + ((best * s->local.vz) >> 14);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(coord);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x40);
}

void func_80109FC4(Task* arg0)
{
    s32        flags;
    GameActor* actor;
    s32        temp;
    s32        mode;

    flags = Player_Status.statusFlags;
    actor = arg0->work;
    if (flags != 0) {
        if (flags & PLAYER_STATUS_DARKNESS) {
            temp             = (u16)actor->field_944 - 1;
            actor->field_944 = temp;
            if ((s16)temp <= 0) {
                flags &= ~PLAYER_STATUS_DARKNESS;
            }
        }
        if (flags & PLAYER_STATUS_PARALYSIS) {
            temp             = (u16)actor->field_946 - 1;
            actor->field_946 = temp;
            if ((s16)temp <= 0) {
                flags &= ~PLAYER_STATUS_PARALYSIS;
            }
        }
        if (flags & PLAYER_STATUS_POISON) {
            temp             = actor->field_98D - 1;
            actor->field_98D = temp;
            if ((s8)temp <= 0) {
                Gp_ApplyHpDamage(1);
                mode = (u16)actor->field_958;
                if (mode == 0) {
                    actor->field_98D = 0x78;
                } else if (mode == 3) {
                    actor->field_98D = 0x14;
                } else {
                    actor->field_98D = 0x3C;
                }
            }
            temp             = (u16)actor->field_948 - 1;
            actor->field_948 = temp;
            if ((s16)temp <= 0) {
                flags &= ~PLAYER_STATUS_POISON;
            }
        }
        if (flags & PLAYER_STATUS_SILENCE) {
            temp             = (u16)actor->field_94A - 1;
            actor->field_94A = temp;
            if ((s16)temp <= 0) {
                flags &= ~PLAYER_STATUS_SILENCE;
            }
        }
        if (flags & 0x20) {
            temp             = (u16)actor->field_94C - 1;
            actor->field_94C = temp;
            if ((s16)temp <= 0) {
                flags &= ~0x20;
            }
        }
        if (flags & PLAYER_STATUS_CONFUSION) {
            temp             = (u16)actor->field_94E - 1;
            actor->field_94E = temp;
            if ((s16)temp <= 0) {
                flags &= ~PLAYER_STATUS_CONFUSION;
            }
        }
        if (flags & PLAYER_STATUS_BERSERKER) {
            if ((u32)((u8)Gp_StateC08.field_A - 2) >= 2U) {
                temp             = (u16)actor->field_950 - 1;
                actor->field_950 = temp;
                if ((s16)temp <= 0) {
                    flags &= ~PLAYER_STATUS_BERSERKER;
                }
            }
        }
        Player_Status.statusFlags = flags;
    }
}

void Gp_TriggerPeState(s32 arg0, s32 arg1)
{
    Task*      work;
    GameActor* inner;
    s32        mask;

    mask = arg1;
    if (arg0 == 0) {
        work = gameGetPtrSlot(3);
        if (arg1 & PLAYER_STATUS_DARKNESS) {
            inner = work->work;
            if (func_800B9D80(0x101) == 0) {
                Player_Status.statusFlags |= PLAYER_STATUS_DARKNESS;
                inner->field_944           = 0x258;
                func_800EC9C8();
                Gp_DetachLinkNode(work);
                Gp_SetState1CPe(1);
            }
        }
        if (mask & PLAYER_STATUS_PARALYSIS) {
            inner = work->work;
            if (func_800B9D80(0x102) == 0) {
                Player_Status.statusFlags |= PLAYER_STATUS_PARALYSIS;
                inner->field_946           = 0x258;
                inner->field_98E           = 0;
                func_8010B210(work);
                Gp_SetState1CPe(2);
            }
        }
        if (mask & PLAYER_STATUS_POISON) {
            inner = work->work;
            if (func_800B9D80(0x104) == 0) {
                Player_Status.statusFlags |= PLAYER_STATUS_POISON;
                inner->field_948           = 0x258;
                inner->field_98D           = 0;
                Gp_SetState1CPe(4);
            }
        }
        if (mask & PLAYER_STATUS_SILENCE) {
            inner = work->work;
            if (func_800B9D80(0x108) == 0) {
                Player_Status.statusFlags |= PLAYER_STATUS_SILENCE;
                inner->field_94A           = 0x258;
                Gp_SetState1CPe(0x10);
            }
        }
        if (mask & 0x20) {
            inner = work->work;
            if (func_800B9D80(0x110) == 0) {
                Player_Status.statusFlags |= 0x20;
                inner->field_94C           = 0x258;
                Gp_SetState1CPe(0x20);
            }
        }
        if (mask & PLAYER_STATUS_CONFUSION) {
            inner = work->work;
            if (func_800B9D80(0x120) == 0) {
                Player_Status.statusFlags |= PLAYER_STATUS_CONFUSION;
                inner->field_94E           = 0x258;
                inner->field_990           = (rand() & 0x1F) + 0xA;
                inner->field_970           = 0;
                Gp_SetState1CPe(0x40);
            }
        }
        if (mask & PLAYER_STATUS_BERSERKER) {
            inner = work->work;
            if (func_800B9D80(0x140) == 0) {
                Player_Status.statusFlags |= PLAYER_STATUS_BERSERKER;
                inner->field_950           = 0x258;
                Gp_SetState1CPe(0x80);
                func_800ECA54();
            }
        }
    } else {
        Player_Status.statusFlags &= ~arg1;
    }
}

void func_8010A42C(Task* arg0, s32 arg1)
{
    u8 kind;

    kind = arg1;
    if (kind != 0) {
        switch (kind) {
            case 0:
                break;
            case 1: {
                GameActor* inner;

                inner = arg0->work;
                if (func_800B9D80(0x101) != 0) {
                    return;
                }
                Player_Status.statusFlags |= PLAYER_STATUS_DARKNESS;
                inner->field_944           = 0x258;
                func_800EC9C8();
                Gp_DetachLinkNode(arg0);
                Gp_SetState1CPe(1);
                break;
            }
            case 2: {
                GameActor* inner;

                inner = arg0->work;
                if (func_800B9D80(0x102) != 0) {
                    return;
                }
                Player_Status.statusFlags |= PLAYER_STATUS_PARALYSIS;
                inner->field_946           = 0x258;
                inner->field_98E           = 0;
                func_8010B210(arg0);
                Gp_SetState1CPe(2);
                break;
            }
            case 3: {
                GameActor* inner;

                inner = arg0->work;
                if (func_800B9D80(0x104) != 0) {
                    return;
                }
                Player_Status.statusFlags |= PLAYER_STATUS_POISON;
                inner->field_948           = 0x258;
                inner->field_98D           = 0;
                Gp_SetState1CPe(4);
                break;
            }
            case 4:
                Gp_SetState1CPe(8);
                break;
            case 8: {
                GameActor* inner;

                inner = arg0->work;
                if (func_800B9D80(0x108) != 0) {
                    return;
                }
                Player_Status.statusFlags |= PLAYER_STATUS_SILENCE;
                inner->field_94A           = 0x258;
                Gp_SetState1CPe(0x10);
                break;
            }
            case 9: {
                GameActor* inner;

                inner = arg0->work;
                if (func_800B9D80(0x110) != 0) {
                    return;
                }
                Player_Status.statusFlags |= 0x20;
                inner->field_94C           = 0x258;
                Gp_SetState1CPe(0x20);
                break;
            }
            case 10: {
                GameActor* inner;

                inner = arg0->work;
                if (func_800B9D80(0x120) != 0) {
                    return;
                }
                Player_Status.statusFlags |= PLAYER_STATUS_CONFUSION;
                inner->field_94E           = 0x258;
                inner->field_990           = (rand() & 0x1F) + 0xA;
                inner->field_970           = 0;
                Gp_SetState1CPe(0x40);
                break;
            }
            case 11: {
                GameActor* inner;

                inner = arg0->work;
                if (func_800B9D80(0x140) != 0) {
                    return;
                }
                Player_Status.statusFlags |= PLAYER_STATUS_BERSERKER;
                inner->field_950           = 0x258;
                Gp_SetState1CPe(0x80);
                func_800ECA54();
                break;
            }
        }
    }
}

void func_8010A670(Task* arg0)
{
    GameActor*       inner;
    WorldTargetNode* node;
    s32              left;
    s32              right;
    s32              pad;
    s32              bits;
    s32              timer;
    s32              next;
    s32              mode;
    s32              dir;

    inner            = arg0->work;
    timer            = inner->field_990 - 1;
    inner->field_990 = timer;
    if ((s8)timer == 0) {
        left             = 0x8000;
        next             = (rand() & 0x1F) + 0xA;
        pad              = inner->field_962;
        inner->field_990 = next;
        bits             = pad & 0xF000;
        if (bits == left || bits == (right = 0x2000)) {
            if (!(inner->field_970 & 0x5000)) {
                if (rand() & 1) {
                    dir = 0x4000;
                } else {
                    dir = 0x1000;
                }
                inner->field_970 = dir;
            }
        } else {
            bits = pad & 0x5000;
            if (bits) {
                if (rand() & 4) {
                    inner->field_970 &= 0xAFFF;
                } else if (rand() & 1) {
                    inner->field_970 = left;
                } else {
                    inner->field_970 = right;
                }
            }
        }
        if (Gp_StateF0.prefix.bytes.field_0 == 1) {
            if (inner->field_90C != NULL) {
                if (rand() & 3) {
                    Gp_DetachLinkNode(arg0);
                }
            } else {
                mode = inner->field_956;
                if (mode == 2 && !(Player_Status.statusFlags & PLAYER_STATUS_DARKNESS) && (rand() & 3)) {
                    node = Gp_FindLockNode(arg0);
                    if (node != NULL) {
                        inner->field_97E = mode;
                        func_80108E0C(arg0, node);
                    }
                }
            }
        }
    }
    if (gGameSession->padHeld & 0xF000) {
        inner->field_962 |= inner->field_970;
    }
}

s32 Gp_ApplyHpDamage(s16 arg0)
{
    s16           amount;
    s32           ret;
    PlayerStatus* p;
    Task*         slot;
    GfxCoord*     coords;

    amount = arg0;
    ret    = 0;
    if (func_800B9D80(0x40000) != 0) {
        amount = arg0 - (arg0 >> 2);
    }
    if (func_800B9D80(0x800) != 0) {
        Player_Status.mp += amount / 5;
        if (Player_Status.mpMax < Player_Status.mp) {
            Player_Status.mp = Player_Status.mpMax;
        }
    }
    if (func_800B9D80(0x200) != 0) {
        p = &Player_Status;
        if (p->hp >= 5 && amount >= p->hp) {
            slot   = gameGetPtrSlot(3);
            coords = slot->extra.tmd->coords;
            p->hp  = 1;
            Gp_SpawnEff(0x6009C, coords + 1, 5, 0);
            return 0;
        }
    }
    Player_Status.hp -= amount;
    if (Player_Status.hp > 0) {
        return ret;
    }
    if (gGameSession->eventState != 0) {
        Player_Status.hp = 1;
    } else {
        ret = 1;
        Display_AcquireRef();
    }
    return ret;
}

void func_8010A9D0(Task* arg0)
{
    GameActor* inner;
    s32        mode;

    inner = arg0->work;
    func_8010AAB4(arg0);
    if ((u16)inner->field_96C == 1) {
        mode = 0x10;
    } else {
        mode = 0x11;
    }
    Gp_AnimPlayChildSlotsEx(arg0, mode, 0, 3);
}

void Gp_StopPlayerAnim(Task* arg0, s32 arg1)
{
    GameActor* inner;

    inner            = arg0->work;
    inner->field_954 = 1;
    inner->field_958 = 0;
    inner->field_95A = 0;
    inner->field_95C = 0;
    inner->field_95E = 0;
    inner->field_96C = 3;
    if (arg1 == 0) {
        Gp_AnimResetChildSlots(arg0, 0x12);
    } else {
        Gp_AnimPlayChildSlotsEx(arg0, 0x12, 0, arg1);
    }
    Gp_DetachLinkNode(arg0);
    inner->field_983 |= 0x18;
}

static void func_8010AAB4(Task* arg0)
{
    GameActor*    inner;
    PlayerStatus* p;

    p                    = &Player_Status;
    inner                = arg0->work;
    Gp_StateC08.field_6 |= 1;
    func_80106350(arg0, p->weapon, 0);
    if (p->hp > 0) {
        inner->field_954 = 1;
        inner->field_958 = 0;
        inner->field_95A = 0;
        inner->field_95C = 7;
        inner->field_95E = 0;
        inner->field_973 = 0;
        Gp_ApplyHpDamage(inner->field_96E);
        inner->field_12A &= 0x3FFF;
        if ((s8)inner->field_97E == 2) {
            inner->field_97E = 1;
        }
        func_8010A42C(arg0, inner->field_972);
    }
}

static void func_8010AB70(Task* arg0)
{
    _gpResumeBaseState(arg0);
}

void func_8010ABD4(Task* arg0)
{
    GameActor* inner;

    inner = arg0->work;
    if (inner->field_95E != 0) {
        if (inner->field_95E == 1) {
            func_8010B210(arg0);
            inner->field_97A = 0x12;
            if (inner->field_956 != 0) {
                func_8010870C(arg0, 0xC);
            } else {
                func_801066DC(arg0, 0);
            }
        }
    }
}

void func_8010AC54(Task* arg0)
{
    GameActor* inner;
    GameActor* inner2;

    inner = arg0->work;
    if (inner->field_95E == 0) {
        inner->field_95E = 1;
        inner->field_934 = 0;
        inner->field_93E = 0;
    }
    if (inner->field_934 == 0) {
        inner->field_93E++;
        if (inner->field_93E == 3) {
            inner2 = arg0->work;
            func_8010B210(arg0);
            inner2->field_97A = 0x12;
            if (inner2->field_956 != 0) {
                func_8010870C(arg0, 0xC);
            } else {
                func_801066DC(arg0, 0);
            }
        } else {
            inner->field_934 = 5;
        }
        Gp_SpawnEff(
            0x600E0, &arg0->extra.tmd->coords[4 - inner->field_93E], 0x320, 0);
    } else {
        inner->field_934--;
    }
}

void func_8010AD64(Task* arg0)
{
    void**          scratch;
    u8*             head;
    SVECTOR*        vec;
    GameActor*      inner;
    EffectSpawnArg* params;
    GfxCoord*       coord;
    s32             val;
    s32             idx;

    inner                             = arg0->work;
    scratch                           = SCRATCH_HEAD_ADDR;
    head                              = SCRATCH_HEAD_AT(scratch, u8);
    params                            = &D_80113358;
    vec                               = (SVECTOR*)(head - 8);
    SCRATCH_HEAD_AT(scratch, SVECTOR) = vec;
    switch (inner->field_95E) {
        case 0:
            idx                     = (s8)inner->field_993;
            inner->field_95E        = 1;
            coord                   = ((WorldCollisionBody*)inner->field_AC)[idx].coord;
            params->spawnArgLo      = 0xC0;
            params->spawnArgHi      = 2;
            D_80113358.coord        = coord;
            ((SVECTOR*)head)[-1].vx = 0;
            val                     = 0;
            if ((s8)inner->field_993 == 0) {
                val = -0x190;
            }
            vec->vy = val;
            vec->vz = 0;
            func_800FDB18(2, D_80113358.coord, vec, params);
            break;
        case 1:
            break;
        case 2:
            _gpResumeBaseState(arg0);
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}

static void func_8010AE98(Task* arg0)
{
    GameActor* inner;

    inner = arg0->work;
    if (func_800B9D80(0x101) != 0) {
        return;
    }
    Player_Status.statusFlags |= PLAYER_STATUS_DARKNESS;
    inner->field_944           = 0x258;
    func_800EC9C8();
    Gp_DetachLinkNode(arg0);
    Gp_SetState1CPe(1);
}

static void func_8010AF04(Task* arg0)
{
    GameActor* inner;

    inner = arg0->work;
    if (func_800B9D80(0x102) != 0) {
        return;
    }
    Player_Status.statusFlags |= PLAYER_STATUS_PARALYSIS;
    inner->field_946           = 0x258;
    inner->field_98E           = 0;
    func_8010B210(arg0);
    Gp_SetState1CPe(2);
}

static void func_8010AF6C(Task* arg0)
{
    GameActor* inner;

    inner = arg0->work;
    if (func_800B9D80(0x104) != 0) {
        return;
    }
    Player_Status.statusFlags |= PLAYER_STATUS_POISON;
    inner->field_948           = 0x258;
    inner->field_98D           = 0;
    Gp_SetState1CPe(4);
}

static void func_8010AFC0(Task* arg0)
{
    GameActor* inner;

    inner = arg0->work;
    if (func_800B9D80(0x108) != 0) {
        return;
    }
    Player_Status.statusFlags |= PLAYER_STATUS_SILENCE;
    inner->field_94A           = 0x258;
    Gp_SetState1CPe(0x10);
}

static void func_8010B010(Task* arg0)
{
    GameActor* inner;

    inner = arg0->work;
    if (func_800B9D80(0x110) != 0) {
        return;
    }
    Player_Status.statusFlags |= 0x20;
    inner->field_94C           = 0x258;
    Gp_SetState1CPe(0x20);
}

static void func_8010B060(Task* arg0)
{
    GameActor* inner;

    inner = arg0->work;
    if (func_800B9D80(0x120) != 0) {
        return;
    }
    Player_Status.statusFlags |= PLAYER_STATUS_CONFUSION;
    inner->field_94E           = 0x258;
    inner->field_990           = (rand() & 0x1F) + 0xA;
    inner->field_970           = 0;
    Gp_SetState1CPe(0x40);
}

static void func_8010B0C8(Task* arg0)
{
    GameActor* inner;

    inner = arg0->work;
    if (func_800B9D80(0x140) != 0) {
        return;
    }
    Player_Status.statusFlags |= PLAYER_STATUS_BERSERKER;
    inner->field_950           = 0x258;
    Gp_SetState1CPe(0x80);
    func_800ECA54();
}

void Gp_PlayerStepSfx(Task* arg0)
{
    GameActor* inner;
    GameActor* inner2;
    GfxCoord*  obj;
    s32        mode;
    s32        snd;
    s32        temp;
    s32        temp2;

    inner = arg0->work;
    obj   = arg0->extra.tmd->coords;
    if (Mc_SaveData[0].state.cheatMode != 0) {
        return;
    }
    if ((s8)inner->field_97A != 0) {
        return;
    }
    func_80109BB4(arg0, inner->field_17C);
    if ((u16)inner->field_96C == 0) {
        return;
    }
    inner2 = arg0->work;
    func_8010AAB4(arg0);
    mode = 0x11;
    if ((u16)inner2->field_96C == 1) {
        mode = 0x10;
    }
    Gp_AnimPlayChildSlotsEx(arg0, mode, 0, 3);
    temp  = (s8)Gp_GetObjPan(obj);
    temp2 = (s8)gpGetObjDepth(obj);
    snd   = 7;
    if ((u16)inner->field_96C == 1) {
        snd = 6;
    }
    SndEvt_EnqueueType6(snd, temp, temp2);
}

void func_8010B210(Task* arg0)
{
    GameActor* inner;

    inner            = arg0->work;
    inner->field_96C = 0;
    inner->field_972 = 0;
    inner->field_96E = 0;
}

static s32 Gp_TestHpDamage(s32 arg0)
{
    PlayerStatus* p;
    u16           saved18;
    u16           saved1c;
    s32           out;
    s32           ret;

    p       = &Player_Status;
    saved18 = p->hp;
    saved1c = p->mp;
    ret     = Gp_ApplyHpDamage(Gp_ScaleDamage(arg0, 0, &out, 0));
    p->hp   = saved18;
    p->mp   = saved1c;
    if (ret != 0) {
        Display_ReleaseRef();
    }
    return ret;
}

void func_8010B2A0(s32 arg0, s32 arg1)
{
    Task_SpawnFromTable(D_80113340, arg0, arg1, 0);
}

static void func_8010B2D4(Task* arg0, WorldCollisionContact* arg1, s32 arg2)
{
    GameActor* inner;
    s32        out;
    s32        flag;

    inner = arg0->work;
    flag  = inner->field_910 != 0;
    if ((u16)inner->field_96C == 0) {
        inner->field_993 = arg2;
        arg2             = (u16)arg2;
        if (arg2 == 1) {
            inner->field_96C = arg2;
        } else {
            inner->field_96C = 2;
        }
        inner->field_96E = Gp_ScaleDamage(arg1->key.value, 0, &out, flag);
        inner->field_972 = out;
    }
}

static void func_8010B348(Task* arg0, WorldCollisionContact* arg1, s32 arg2)
{
    GameActor* inner;
    u32        kind;

    inner = arg0->work;
    kind  = (u16)arg1->key.value;
    if ((u16)inner->field_96C == 0) {
        inner->field_993 = arg2;
        if (kind == 2) {
            goto case24;
        }
        if (kind < 3) {
            goto do_call;
        }
        if (kind == 3) {
            goto case3;
        }
        if (kind != 4) {
            goto do_call;
        }
    case24:
        arg2 = (u16)arg2;
        if (arg2 == 1) {
            inner->field_96C = arg2;
        } else {
            inner->field_96C = 2;
        }
        inner->field_972 = 5;
        goto do_call;
    case3:
        arg2 = (u16)arg2;
        if (arg2 == 1) {
            inner->field_96C = arg2;
        } else {
            inner->field_96C = 2;
        }
        inner->field_972 = 0;
    do_call:
        inner->field_96E = Gp_LookupIdField(arg1->key.value, 0);
    }
}

void func_8010B3F8(Task* arg0)
{
    Task*           slot;
    EffectSpawnArg* params;
    GfxCoord*       coords;
    s32             argLo;
    s32             idx;
    u16             count;
    s16             next;

    slot = gameGetPtrSlot(3);
    switch (arg0->state) {
        case 0:
            arg0->state         = 1;
            arg0->killCountdown = 0;
            /* fallthrough */
        case 1:
            count = arg0->killCountdown;
            if ((count & 0xF) == 0) {
                next                = count + 0x100;
                params              = &D_80113358;
                arg0->killCountdown = next;
                idx                 = arg0->spawnArg1.value & 3;
                if (next >= 0x300) {
                    taskKill(arg0);
                } else {
                    arg0->killCountdown = next | 6;
                }
                argLo              = (idx * 0x60) + 0xC0;
                coords             = &slot->extra.tmd->coords[((arg0->killCountdown & 0xF00) >> 8) + 1];
                params->spawnArgLo = argLo;
                params->spawnArgHi = idx + 1;
                params->coord      = coords;
                func_800FDB18(3, coords, 0, params);
            } else {
                arg0->killCountdown = count - 1;
            }
            break;
    }
}

void func_8010B520(Task* arg0)
{
    GfxCoord*       raw;
    Task*           slot;
    TmdObject*      extra;
    EffectSpawnArg* params;
    GfxCoord*       coords;

    params             = &D_80113358;
    slot               = gameGetPtrSlot(3);
    extra              = slot->extra.tmd;
    raw                = extra->coords;
    params->spawnArgLo = 0xC0;
    coords             = &raw[3];
    params->coord      = coords;
    params->spawnArgHi = (u16)arg0->spawnArg1.value + 1;
    func_800FDB18(2, coords, 0, params);
    taskKill(arg0);
}

static void func_8010B590(Task* arg0)
{
    TmdObject* extra;
    GfxCoord*  coord;

    extra = arg0->extra.tmd;
    coord = extra->coords;
    arg0->state++;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (coord->param.clearFlags != 0) {
        extra->flags = 0;
    }
}

static void func_8010B5C0(Task* arg0)
{
    Task*      parent;
    TmdObject* extra;

    parent                      = arg0->parent;
    extra                       = arg0->extra.tmd;
    extra->flags                = parent->extra.tmd->flags;
    extra->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

static void func_8010B5E4(Task* arg0)
{
    arg0->state = 3;
}

static void func_8010B5F0(Task* arg0)
{
    taskKill(arg0);
}

void func_8010B610(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = D_80097AB0;
    sp.funcs[arg0->state](arg0);
}

void Gp_EndPlayerActorTask(Task* arg0)
{
    GameActor* actor;
    GameActor* inner;
    GameActor* next;
    TmdObject* extra;
    Task*      task;

    actor = arg0->work;
    task  = actor->field_91C;
    if (task != NULL) {
        taskKill(task);
        actor->field_91C     = NULL;
        extra                = arg0->extra.tmd;
        inner                = arg0->work;
        inner->field_93A     = Gp_AllyIdBase[Mc_SaveData[0].state.companionType - 1] + Mc_SaveData[0].state.companionVariant;
        inner->animationSets = Gp_AnimBlkTbl[inner->field_93A]->table.sets;
        func_800B3F84((AnimationContext*)inner->field_424, inner->animationSets, extra, &inner->field_7A8,
                      inner->field_438);
        Gp_AnimResetChildSlots(arg0, 1);
        next            = arg0->work;
        next->field_954 = 0;
        next->field_956 = 0;
        next->field_958 = 0;
        next->field_95A = 0;
        next->field_95C = 0;
        next->field_95E = 0;
        next->field_942 = 0;
        next->field_93E = 0;
        next->field_973 = 0;
        next->field_975 = 0;
        Gp_AnimPlayChildSlotsEx(arg0, 1, 0, 4);
    }
    task = actor->field_914;
    if (task != NULL) {
        taskKill(task);
        actor->field_914 = NULL;
    }
}

Task* Gp_SetupAllyWeapon(void)
{
    Task*       work;
    GameActor*  actor;
    GameActor*  inner;
    GameActor*  next;
    Task*       task;
    McSaveData* save;
    GpActorD4*  block;
    s16         val1;
    s16         val2;
    GpEffWork*  eff;
    TmdObject*  extra;
    Task*       ret;

    work  = gameGetPtrSlot(0xA);
    actor = work->work;
    if (!work | !actor) {
        return 0;
    }

    if (actor->field_924 != NULL) {
        save             = &Mc_SaveData[0];
        task             = func_80104364(actor->field_924, save->state.companionType + 1, save->state.companionVariant, 0);
        actor->field_91C = task;
        if (task != NULL) {
            block = actor->field_910;
            val1  = D_80167218[save->state.companionVariant];
            val2  = D_80167224[save->state.companionVariant];
            Gp_AttachActorObj(work, val1, val2);
            actor->field_124  |= 0x80;
            block->actionCount = D_80167230[save->state.companionVariant];
            if ((u8)save->state.companionVariant == 4 && actor->field_914 == NULL) {
                eff = Gp_SpawnEff(
                    0x80060180, actor->field_91C->extra.tmd->coords, (s32)(val1), 0);
                if (eff != NULL) {
                    actor->field_914 = eff->task;
                    func_80106350(work, val1, 0);
                }
            }
        }
    }

    inner                = work->work;
    extra                = work->extra.tmd;
    inner->field_93A     = Gp_AllyIdBase[Mc_SaveData[0].state.companionType - 1] + Mc_SaveData[0].state.companionVariant;
    inner->animationSets = Gp_AnimBlkTbl[inner->field_93A]->table.sets;
    func_800B3F84((AnimationContext*)inner->field_424, inner->animationSets, extra, &inner->field_7A8,
                  inner->field_438);
    next            = work->work;
    next->field_954 = 0;
    next->field_956 = 0;
    next->field_958 = 0;
    next->field_95A = 0;
    next->field_95C = 0;
    next->field_95E = 0;
    next->field_942 = 0;
    next->field_93E = 0;
    next->field_973 = 0;
    next->field_975 = 0;
    Gp_AnimResetChildSlots(work, 1);
    ret              = actor->field_91C;
    actor->field_983 = 7;
    return ret;
}

void func_8010B9A4(Task* arg0)
{
    GameActor*  actor;
    McSaveData* save;
    s32         field13;
    u16         temp;
    u16         anim;

    actor            = arg0->work;
    actor->field_954 = 1;
    actor->field_95C = 7;
    save             = &Mc_SaveData[0];
    actor->field_958 = 0;
    actor->field_95A = 0;
    actor->field_95E = 0;
    actor->field_960 = 0;
    actor->field_973 = 0;
    actor->field_975 = 0;
    if (save->state.cheatMode == 0 && (field13 = save->state.companionType) == 1) {
        temp                    = save->state.companionHp - actor->field_96E;
        save->state.companionHp = temp;
        if ((s16)temp <= 0 && gGameSession->eventState != 0) {
            save->state.companionHp = field13;
        }
    }
    actor->field_12A &= 0x3FFF;
    if ((s8)actor->field_97E == 2) {
        actor->field_97E = 1;
    }
    func_80106350(arg0, D_80167218[Mc_SaveData[0].state.companionVariant], 0);
    anim = 0x11;
    if ((u16)actor->field_96C == 1) {
        anim = 0x10;
    }
    Gp_AnimPlayChildSlotsEx(arg0, anim, 0, 3);
}

Task* Gp_SpawnAlly(GpActorArg* arg0, u16 arg1, s32 arg2, u16* arg3)
{
    Task*      task;
    GameActor* actor;
    GpActorD4* block;
    GfxCoord*  coord;
    s32        type;

    if (arg1 == 1) {
        type = Mc_SaveData[0].state.companionVariant + 0x7F;
    } else {
        type = arg1 + 0x82;
    }
    task = Task_Spawn(7, type, arg2, arg3);
    if (task != NULL) {
        goto have_task;
    }
    return NULL;

have_task:
    actor = memCalloc(0x998, 0);
    if (actor != NULL) {
        goto have_actor;
    }
fail:
    taskKill(task);
    return NULL;

have_actor:
    block = memCalloc(0xD4, 0);
    if (block == NULL) {
        goto fail;
    }
    Game_SetPtrSlot(task, 0xA);
    Mem_Set(actor, 0, 0x998);
    Mem_Set(block, 0, 0xD4);
    task->work       = actor;
    actor->field_910 = block;
    Gp_PumpTmdStream(task);
    actor->field_93C  = *arg3;
    actor->field_52   = arg0->field_0;
    coord             = task->extra.tmd->coords;
    coord->coord.t[0] = arg0->field_4;
    coord->coord.t[1] = arg0->field_8;
    coord->coord.t[2] = arg0->field_C;
    return task;
}

void Gp_ResetActorMove(Task* arg0, s16 arg1)
{
    GameActor* inner;

    inner            = arg0->work;
    inner->field_954 = 0;
    inner->field_956 = 0;
    inner->field_958 = 0;
    inner->field_95A = 0;
    inner->field_95C = 0;
    inner->field_95E = 0;
    inner->field_942 = 0;
    inner->field_93E = 0;
    inner->field_973 = 0;
    inner->field_975 = 0;
    if (arg1 != 0) {
        Gp_AnimResetChildSlots(arg0, 1);
    } else {
        Gp_AnimPlayChildSlotsEx(arg0, 1, 0, 4);
    }
}

s32 func_8010BC70(GfxCoord* arg0)
{
    u8*        head;
    VECTOR3*   vec;
    TmdObject* extra;
    s32        ret;

    extra                         = (gameGetPtrSlot(3))->extra.tmd;
    head                          = SCRATCH_STACK_CURSOR(u8);
    vec                           = (VECTOR3*)(head - 0x10);
    SCRATCH_STACK_CURSOR(VECTOR3) = vec;
    func_80103C74(arg0, (VECTOR3*)(extra->coords)->coord.t, vec);
    ret = func_80103D8C(((VECTOR3*)(head - 0x10))->vx, vec->vz);
    SCRATCH_STACK_RELEASE_BYTES(0x10);
    return ret;
}

s32 func_8010BCF4(Task* arg0, VECTOR3* arg1)
{
    GfxCoord*  coords;
    VECTOR3*   vec;
    GameActor* actor;
    s16        ret;

    coords = arg0->extra.tmd->coords;
    vec    = SCRATCH_PUSH_BYTES(0x10);
    actor  = arg0->work;
    func_80103C74(coords, arg1, vec);
    ret = func_80103E7C(actor->field_52, ratan2(vec->vx, vec->vz));
    SCRATCH_STACK_RELEASE_BYTES(0x10);
    return ret;
}

void func_8010BD88(Task* arg0, VECTOR3* arg1)
{
    u8*            head;
    GpTurnScratch* vec;
    TmdObject*     extra;
    GameActor*     actor;
    s32            val;

    extra = arg0->extra.tmd;
    head  = SCRATCH_STACK_CURSOR(u8);
    vec = SCRATCH_STACK_CURSOR(GpTurnScratch) = (GpTurnScratch*)(head - 0x14);
    actor                                     = arg0->work;
    func_80103C74(extra->coords, arg1, (VECTOR3*)vec);
    vec->angle = ratan2(((GpTurnScratch*)(head - 0x14))->vx, vec->vz);
    val        = func_80103E7C(actor->field_52, vec->angle);
    vec->angle = val;
    if (val > 0x40) {
        vec->angle = 0x40;
    } else if (val < -0x40) {
        vec->angle = -0x40;
    }
    actor->field_52 = (actor->field_52 + vec->angle) & 0xFFF;
    SCRATCH_STACK_RELEASE_BYTES(0x14);
}

void func_8010BE5C(Task* task, VECTOR3* targetPoint)
{
    enum {
        PLAYER_ACTOR_AIM_YAW_STEP  = 0x20,
        PLAYER_ACTOR_AIM_YAW_LIMIT = 0x1A0
    };
    _PlayerActorAimScratch* head;
    _PlayerActorAimScratch* block;
    GfxCoord*               coord;
    SVECTOR*                offset;
    TmdObject*              extra;
    register GfxCoord*      parts asm("v0");
    GameActor*              actor;
    s32                     yawStep;

    head   = SCRATCH_STACK_CURSOR(_PlayerActorAimScratch);
    extra  = task->extra.tmd;
    actor  = task->work;
    coord  = &head[-1].coord;
    offset = &head[-1].offset;
    // Retain the array pointer's register without treating it as a model object.
    parts = extra->coords;
    block = SCRATCH_STACK_CURSOR(_PlayerActorAimScratch) = head - 1;
    block->offset.vx                                     = 0;
    block->offset.vy                                     = 0;
    block->offset.vz                                     = 0;
    Gp_PlaceCoordOffset(parts + 4, coord, offset);
    func_80103C74(coord, targetPoint, &block->vec);
    // Turn toward the target relative to body facing, preserving the strict aim limit.
    yawStep = ratan2(head[-1].vec.vx, block->vec.vz) - actor->field_52;
    yawStep = func_80103E7C(actor->field_6A, yawStep);
    if (yawStep > PLAYER_ACTOR_AIM_YAW_STEP) {
        yawStep = PLAYER_ACTOR_AIM_YAW_STEP;
    } else if (yawStep < -PLAYER_ACTOR_AIM_YAW_STEP) {
        yawStep = -PLAYER_ACTOR_AIM_YAW_STEP;
    }
    if (ABS(actor->field_6A + yawStep) < PLAYER_ACTOR_AIM_YAW_LIMIT) {
        actor->field_6A += yawStep;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_PlayerActorAimScratch);
}

void func_8010BF7C(Task* arg0, s32 arg1, s32 arg2)
{
    ((GameActor*)arg0->work)->field_910->decisionTimer = arg1 + (arg2 & rand());
}

void func_8010BFCC(Task* arg0)
{
    GameActor* actor;
    TmdObject* extra;

    actor                = arg0->work;
    extra                = arg0->extra.tmd;
    actor->field_93A     = Gp_AllyIdBase[Mc_SaveData[0].state.companionType - 1] + Mc_SaveData[0].state.companionVariant;
    actor->animationSets = Gp_AnimBlkTbl[actor->field_93A]->table.sets;
    func_800B3F84((AnimationContext*)actor->field_424, actor->animationSets, extra, &actor->field_7A8,
                  actor->field_438);
}

s32 func_8010C058(void)
{
    s32 ret;

    if ((Mc_SaveData[0].state.companionHpMax >> 1) < Mc_SaveData[0].state.companionHp) {
        ret = 0;
    } else if ((Mc_SaveData[0].state.companionHpMax >> 2) >= Mc_SaveData[0].state.companionHp) {
        ret = 2;
    } else {
        ret = 1;
    }
    return ret;
}

void Gp_TrackAllyLockTarget(Task* arg0, s32 arg1)
{
    GameActor*       actor;
    WorldTargetNode* node;
    s32              val;

    actor = arg0->work;
    node  = actor->field_90C;
    if (node == NULL || (node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
        actor->field_90C = NULL;
        actor->field_97E = 1;
    } else if ((s8)actor->field_97E == 2) {
        if (arg1 & 1) {
            val = 0;
            if (arg1 != 1) {
                val = 0x380;
            }
            Gp_AimYawToLock(arg0, val);
        }
        if (arg1 & 2) {
            if (D_80113388[Mc_SaveData[0].state.companionVariant] != 0) {
                Gp_AimPitchToLock(arg0);
            } else {
                Gp_AimPitchRec(arg0, D_80167218[Mc_SaveData[0].state.companionVariant], 0x380);
            }
        }
    }
}

void func_8010C180(Task* arg0)
{
    GameActor* inner;
    GameActor* actor;

    inner = arg0->work;
    func_8010B210(arg0);
    inner->field_97A = 0x12;
    actor            = arg0->work;
    actor->field_954 = 0;
    actor->field_956 = 0;
    actor->field_958 = 0;
    actor->field_95A = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_942 = 0;
    actor->field_93E = 0;
    actor->field_973 = 0;
    actor->field_975 = 0;
    Gp_AnimPlayChildSlotsEx(arg0, 1, 0, 4);
}

void Gp_BindActorD4(Task* arg0, SVECTOR3* arg1, s32 arg2)
{
    GfxCoord*              src;
    GpActorD4*             block;
    WorldCollisionBody*    obj;
    WorldCollisionCapsule* rec;
    s16                    vz;

    block                = ((GameActor*)arg0->work)->field_910;
    src                  = arg0->extra.tmd->coords;
    obj                  = &block->obj;
    rec                  = &block->shape;
    block->coord         = *src;
    obj->coord           = &block->coord;
    obj->pos.vz          = -0xA0;
    obj->key             = 0x60000;
    obj->context.capsule = rec;
    obj->pos.vx          = 0;
    obj->pos.vy          = 0;
    obj->flags           = WORLD_COLLISION_BODY_CAPSULE;
    rec->ends[1].vx      = arg1->vx;
    rec->ends[1].vy      = arg1->vy;
    vz                   = arg1->vz;
    rec->ends[0].vz      = arg2;
    rec->ends[0].vx      = rec->ends[1].vx;
    rec->end1Radius      = 0x80;
    rec->end0Radius      = 0x80;
    rec->contacts        = &block->contact;
    rec->ends[1].vz      = vz;
    rec->ends[0].vy      = rec->ends[1].vy;
    Gp_LinkObj(1, obj);
    Gp_InitRec18Table(rec->contacts, 1, 0);
    obj->flags |= (WORLD_COLLISION_BODY_SINGLE_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
}

s32 func_8010C30C(Task* arg0)
{
    TmdObject*     extra;
    GfxCoord*      coord;
    GfxCoord*      next;
    GameActor*     actor;
    VECTOR         vec;
    AnimationSet** prev;
    AnimationSet** anim;
    s32            changed;

    extra  = arg0->extra.tmd;
    coord  = extra->coords;
    actor  = arg0->work;
    next   = coord + 1;
    vec.vx = next->coord.t[0];
    vec.vy = next->coord.t[1];
    vec.vz = next->coord.t[2];
    ApplyMatrixLV(&coord->coord, &vec, &vec);
    coord->coord.t[0]   += vec.vx;
    coord->coord.t[2]   += vec.vz;
    next->coord.t[0]     = 0;
    next->coord.t[2]     = 0;
    actor->field_10      = coord->coord.t[0];
    actor->field_14      = coord->coord.t[1];
    actor->field_18      = coord->coord.t[2];
    prev                 = actor->animationSets;
    actor->field_93A     = Gp_AllyIdBase[Mc_SaveData[0].state.companionType - 1] + Mc_SaveData[0].state.companionVariant;
    anim                 = Gp_AnimBlkTbl[actor->field_93A]->table.sets;
    changed              = prev != anim;
    actor->animationSets = anim;
    func_800B3F84((AnimationContext*)actor->field_424, actor->animationSets, extra, &actor->field_7A8,
                  actor->field_438);
    actor->field_985 = 0x10;
    actor->field_983 = 7;
    actor->field_95E = 0;
    actor->field_960 = 0;
    Gp_ResetActorMove(arg0, changed);
    return 0;
}

static void func_8010C46C(Task* arg0)
{
    GameActor* actor;

    actor             = arg0->work;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, D_80167218[Mc_SaveData[0].state.companionVariant], 0);
}

s32 func_8010C4F0(Task* task, s32 msgId, AnimationPlayRequest* request)
{
    GameActor* actor;
    TmdObject* extra;

    actor             = task->work;
    extra             = task->extra.tmd;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(task, D_80167218[Mc_SaveData[0].state.companionVariant], 0);
    // Select the animation table before resetting or blending its slots.
    actor->field_956 = 1;
    if (actor->animationSets != Gp_AnimBlkTbl[request->source.index]->table.sets) {
        actor->animationSets = Gp_AnimBlkTbl[request->source.index]->table.sets;
        func_800B3F84((AnimationContext*)actor->field_424, actor->animationSets, extra, &actor->field_7A8,
                      actor->field_438);
        actor->field_93A = (u16)request->source.index;
    }
    actor->field_985 = ANIMATION_RATE_ONE;
    if (request->blend == ANIMATION_BLEND_RESET) {
        Gp_AnimResetChildSlots(task, request->animationId);
    } else {
        Gp_AnimPlayChildSlotsEx(task, request->animationId, 1, request->blendFrames);
    }
    if (request->enableWorldCollision == ANIMATION_WORLD_COLLISION_DISABLE) {
        actor->field_983 = PLAYER_ACTOR_WORLD_COLLISION_DISABLE;
    } else {
        actor->field_983 = PLAYER_ACTOR_WORLD_COLLISION_ENABLE;
    }
    return 0;
}

s32 func_8010C648(Task* task, s32 msgId, AnimationPlayRequest* request)
{
    PlayerStatus* playerStatus;
    u8            savedInteractionPressed;

    playerStatus            = &Player_Status;
    savedInteractionPressed = playerStatus->interactionPressed;
    func_80104B54(task, msgId, request);
    playerStatus->interactionPressed = savedInteractionPressed;
    return 0;
}

s32 func_8010C688(Task* arg0, s32 arg1, ActorTransform* transform, s32 arg3)
{
    PlayerStatus* p;
    u8            savedInteractionPressed;

    p                       = &Player_Status;
    savedInteractionPressed = p->interactionPressed;
    func_80104E00(arg0, arg1, transform, arg3);
    p->interactionPressed = savedInteractionPressed;
    return 0;
}

s32 func_8010C6C8(Task* arg0, s32 arg1, ActorTransform* transform, GpOverrideArg* arg3)
{
    PlayerStatus* p;
    u8            savedInteractionPressed;

    p                       = &Player_Status;
    savedInteractionPressed = p->interactionPressed;
    Gp_SetActorDest(arg0, arg1, transform, arg3);
    p->interactionPressed = savedInteractionPressed;
    return 0;
}

s32 func_8010C708(Task* arg0, s32 arg1, ActorTransform* transform, GpOverrideArg* arg3)
{
    PlayerStatus* p;
    u8            savedInteractionPressed;
    GameActor*    actor;

    p                       = &Player_Status;
    actor                   = arg0->work;
    savedInteractionPressed = p->interactionPressed;
    Gp_SetActorDest(arg0, arg1, transform, arg3);
    p->interactionPressed = savedInteractionPressed;
    actor->field_956      = 8;
    return 0;
}

s32 func_8010C75C(Task* arg0, s32 arg1, GpDelayArg* arg2)
{
    GameActor* actor;

    actor = arg0->work;
    if ((s8)actor->field_97A != 0) {
        return 1;
    }
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, D_80167218[Mc_SaveData[0].state.companionVariant], 0);
    actor->field_956 = 6;
    actor->field_934 = arg2->field_14;
    actor->field_93E = 0;
    return 0;
}

void Gp_MoveActorByKeep(Task* arg0, s32 arg1, GpMoveArg* arg2)
{
    PlayerStatus* p;
    u8            savedInteractionPressed;

    p                       = &Player_Status;
    savedInteractionPressed = p->interactionPressed;
    Gp_MoveActorBy(arg0, arg1, arg2);
    p->interactionPressed = savedInteractionPressed;
}

s32 Gp_CopyAllyAnim(Task* arg0, s32 arg1, GpCopyArg* arg2)
{
    union {
        GpAnimBlk* block;
        s32*       words;
    } dest;
    s32* src;
    s32  i;
    s32  count;

    dest.block = Gp_AnimBlkTbl[Gp_AllyIdBase[Mc_SaveData[0].state.companionType - 1] + Mc_SaveData[0].state.companionVariant];
    src        = arg2->source.words;
    count      = arg2->count;
    if (count >= ANIMATION_BANK_EXTENSION_CAPACITY + 1) {
        return 1;
    }
    dest.words = &dest.block->table.addresses[ANIMATION_BANK_BASE_SET_COUNT];
    for (i = 0; i < arg2->count; i++) {
        dest.words[i] = src[i];
    }
    return 0;
}

s32 Gp_HurtAlly(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32 ret;

    ret = 0;
    if (Mc_SaveData[0].state.cheatMode == 0) {
        Mc_SaveData[0].state.companionHp -= Gp_ScaleDamage(arg2, 0, 0, 1);
        if (Mc_SaveData[0].state.companionHp <= 0) {
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, 0, 0x7DE);
            ret = 1;
        }
    }
    return ret;
}

void func_8010C980(void* arg0, WorldCollisionBody* arg1, WorldCollisionContact* arg2, s32 arg3, s32 arg4, s32 arg5)
{
    arg1->coord            = arg0;
    arg1->context.contacts = arg2;
    arg1->pos.vx           = 0;
    arg1->pos.vy           = 0;
    arg1->pos.vz           = 0;
    arg1->flags            = WORLD_COLLISION_BODY_SPHERE;
    arg1->key              = arg4 | 0x30000;
    arg1->radius           = arg5;
    Gp_LinkObj(2, arg1);
    arg1->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_InitRec18Table(arg1->context.contacts, (s16)arg3, 0);
}

const TaskFuncTable4 D_80097AB0 = { {
    func_8010B590,
    func_8010B5C0,
    func_8010B5E4,
    func_8010B5F0,
} };
