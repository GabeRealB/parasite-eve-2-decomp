#include "world_targets.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>
#include <psyq/stdio.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor_render.h"
#include "gameplay/area_entry.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/enemy.h"
#include "geometry.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/item_pickup.h"
#include "item_use.h"
#include "gameplay/items.h"
#include "items.h"
#include "lighting_work.h"
#include "gameplay/enemy_params.h"
#include "gameplay/scene.h"
#include "gameplay/world_collision.h"
#include "world_coords.h"
#include "gameplay/world_state.h"
#include "world_state.h"

#include "main/display.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/text.h"
#include "main/ui.h"

/// 0x38-byte scratch from the scratch stack used by `Gp_ScanLockNodes`.
/// `src` is the actor's `coord.t` (lowered by 1000 on Y) before
/// `gGfxViewCoord.workm` rotates it into `self`, the world-space aim origin.
/// `node` is the candidate `gWorldTargetListHead` node's world position; both are
/// handed to `func_800E0308` as the line-of-sight segment.
typedef struct _GpLockScanScratch {
    /* 0x00 */ SVECTOR self;
    /* 0x08 */ SVECTOR node;
    /* 0x10 */ SVECTOR src;
    /* 0x18 */ byte    pad_18[0x20];
} GpLockScanScratch;
STATIC_ASSERT_SIZEOF(GpLockScanScratch, 0x38);

/* Define BSS before API headers to preserve first-declaration order. */
GpSlot70 Gp_LockSlots[32];

SceneCombatState gSceneCombatState;

#include "gameplay/scene_combat.h"
#include "gameplay/world_targets.h"

static __inline__ void project_slot(s32* sxy, GpSlot70* slot);

static void* Gp_ScanLockNodes(Task* arg0, VECTOR3* out, s32 flag);

static void Gp_UpdateLockSlots(void);

static void* Gp_FindLockNodeAt(Task* arg0, VECTOR3* pos);

static void Gp_ClearLockSlots(void);

static s32 Gp_ProjectToSxy(WorldTargetNode* arg0, s32* sxy);

static __inline__ void project_slot(s32* sxy, GpSlot70* slot)
{
    WorldTargetNode* src;
    GpPerspScratch*  block;

    src = slot->field_0;
    SCRATCH_STACK_RESERVE_BLOCK(GpPerspScratch);
    block         = SCRATCH_STACK_CURSOR(GpPerspScratch);
    block->vec.vx = GP_NODE_ENEMY(src)->bodyPos.vx;
    block->vec.vy = GP_NODE_ENEMY(src)->bodyPos.vy;
    block->vec.vz = GP_NODE_ENEMY(src)->bodyPos.vz;
    gte_SetRotMatrix(&GP_NODE_ENEMY(src)->coord->workm);
    gte_SetTransMatrix(&GP_NODE_ENEMY(src)->coord->workm);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(sxy);
    gte_stdp(&block->p);
    gte_stflg(&block->flag);
    gte_stszotz(&block->otz);
    SCRATCH_STACK_RELEASE_BLOCK(GpPerspScratch);
}

void Gp_DrawTargetCursor(void)
{
    WorldTargetNode*             node;
    GameSession*                 sess;
    WorldCoordProjectionScratch* projection;
    POLY_FT4*                    prim;
    s32                          easing;
    s32                          frame;
    s32                          u;
    s32                          v;

    node = gWorldTargetListHead;
    if (Pad_RemapState->field_A != 0) {
        return;
    }
    Gp_UpdateLockSlots();
    sess = gGameSession;
    if (sess->sceneUpdatesPaused == 1) {
        return;
    }
    if (Gp_StateC08.field_A == 2 || Gp_StateC08.field_A == 3) {
        return;
    }
    if (Gp_StateC08.field_A == 1) {
        return;
    }
    if (sess->eventState != 0) {
        return;
    }
    if (sess->hideHud != 0) {
        return;
    }
    for (; node != NULL; node = node->next) {
        if (node->state.parts.targeted != 0 && !(node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
            easing               = 0;
            projection           = SCRATCH_STACK_RESERVE_BLOCK(WorldCoordProjectionScratch);
            projection->point.vx = GP_NODE_ENEMY(node)->bodyPos.vx;
            projection->point.vy = GP_NODE_ENEMY(node)->bodyPos.vy;
            projection->point.vz = GP_NODE_ENEMY(node)->bodyPos.vz;
            Gp_UpdateCoord(GP_NODE_ENEMY(node)->coord);
            gte_SetRotMatrix(&GP_NODE_ENEMY(node)->coord->workm);
            gte_SetTransMatrix(&GP_NODE_ENEMY(node)->coord->workm);
            gte_RotTransPers(&projection->point, &projection->screen, &projection->depthCue,
                             &projection->projectionFlags, &projection->orderingDepth);
            if (D_80115260 != node) {
                if (D_80115260 == NULL) {
                    D_80115264 = 0xFF;
                } else {
                    D_80115264 = 0;
                }
                D_80115260 = node;
            }
            if (D_80115264 < 5) {
                D_8010F9EC += ((projection->screen.vx << 8) - D_8010F9EC) >> 1;
                D_8010F9F0 += ((projection->screen.vy << 8) - D_8010F9F0) >> 1;
                if (projection->screen.vx == (D_8010F9EC >> 8) && projection->screen.vy == (D_8010F9F0 >> 8)) {
                    D_80115264 = 0xFF;
                } else {
                    D_80115264++;
                }
                easing                = 1;
                projection->screen.vx = D_8010F9EC >> 8;
                projection->screen.vy = D_8010F9F0 >> 8;
            } else {
                D_8010F9EC = projection->screen.vx << 8;
                D_8010F9F0 = projection->screen.vy << 8;
            }
            // Convert the eased projection to the cursor's display coordinates.
            projection->screen.vy -= gDisplayState.vramYOffset;
            frame                  = gDisplayState.animFrame % 24 / 3;
            prim                   = gGpuPrimCursor;
            gGpuPrimCursor         = prim + 1;
            if (easing == 1) {
                prim->x0 = prim->x2 = projection->screen.vx - 8;
                prim->x1 = prim->x3 = projection->screen.vx + 8;
                prim->y0 = prim->y1 = projection->screen.vy - 8;
                prim->y2 = prim->y3 = projection->screen.vy + 8;
            } else {
                prim->x0 = prim->x2 = projection->screen.vx - 0x10;
                prim->x1 = prim->x3 = projection->screen.vx + 0x10;
                prim->y0 = prim->y1 = projection->screen.vy - 0x10;
                prim->y2 = prim->y3 = projection->screen.vy + 0x10;
            }
            u = (frame & 3) << 5;
            v = (frame >> 2) << 5;
            setUV4(prim, u + 0x40, v, u + 0x60, v, u + 0x40, v + 0x20, u + 0x60, v + 0x20);
            prim->clut  = 0x3C81;
            prim->tpage = 0x3E;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            addPrim(gGpuCurrentOt, prim);
            SCRATCH_STACK_RELEASE_BLOCK(WorldCoordProjectionScratch);
            break;
        }
    }
    if (node == NULL) {
        D_80115260 = NULL;
        D_80115264 = 0;
    }
}

static void* Gp_ScanLockNodes(Task* arg0, VECTOR3* out, s32 flag)
{
    GpLockScanScratch* block;
    GameActor*         actor;
    GfxCoord*          coord;
    GfxCoord*          nodeCoord;
    WorldTargetNode*   node;
    WorldTargetNode*   best;
    s32                bestAngle;
    u32                bestDist;
    s32                baseAngle;
    s32                angle;
    u32                dist;
    s32                sub;
    SVECTOR            tmp;
    SVECTOR*           srcp;

    best = NULL;
    SCRATCH_STACK_RESERVE_BLOCK(GpLockScanScratch);
    block         = SCRATCH_STACK_CURSOR(GpLockScanScratch);
    actor         = arg0->work;
    coord         = arg0->extra.tmd->coords;
    block->src.vx = coord->coord.t[0];
    block->src.vy = coord->coord.t[1] - 1000;
    block->src.vz = coord->coord.t[2];
    Gp_UpdateCoord(&gGfxViewCoord);
    srcp = &block->src;
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(srcp);
    gte_rtv0();
    gte_stsv(&block->self);
    block->self.vx += gGfxViewCoord.workm.t[0];
    block->self.vy += gGfxViewCoord.workm.t[1];
    block->self.vz += gGfxViewCoord.workm.t[2];

    if (actor->targetNode != NULL && flag != 0) {
        node      = actor->targetNode;
        baseAngle = ratan2(GP_NODE_ENEMY(node)->playerRelPos.vx, GP_NODE_ENEMY(node)->playerRelPos.vz);
    } else {
        baseAngle = 0;
    }
    bestAngle = 0x3000;
    bestDist  = 0x7FFFFFFF;
    dist      = 0;
    for (node = gWorldTargetListHead; node != NULL; node = node->next) {
        if (node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE) {
            continue;
        }
        angle = ratan2(GP_NODE_ENEMY(node)->playerRelPos.vx, GP_NODE_ENEMY(node)->playerRelPos.vz);
        if (flag == 0) {
            dist = GP_NODE_ENEMY(node)->playerRelPos.vz * GP_NODE_ENEMY(node)->playerRelPos.vz + GP_NODE_ENEMY(node)->playerRelPos.vx * GP_NODE_ENEMY(node)->playerRelPos.vx + GP_NODE_ENEMY(node)->playerRelPos.vy * GP_NODE_ENEMY(node)->playerRelPos.vy;
            if (angle < 0) {
                angle = -angle;
            }
            if (dist <= 0x300000) {
                sub    = 0x300000 - dist;
                sub  >>= 13;
                sub   *= 3;
                angle -= sub;
                if (angle < 0) {
                    angle = 0;
                }
                dist += sub / 3;
            }
            angle >>= 10;
            if (node == actor->targetNode) {
                angle += 0x1000;
            }
            if (angle == bestAngle && dist > bestDist) {
                angle = 0x2000;
            }
        } else {
            angle -= baseAngle;
            if (angle < 0) {
                angle += 0x1000;
            }
            if (angle >= 0x1000) {
                angle -= 0x1000;
            }
            if (flag == 1) {
                angle = -angle;
            }
            if (node == actor->targetNode) {
                angle += 0x1000;
            }
        }
        if (angle > bestAngle) {
            continue;
        }
        Gp_UpdateCoord(GP_NODE_ENEMY(node)->coord);
        block->node.vx = GP_NODE_ENEMY(node)->bodyPos.vx;
        block->node.vy = GP_NODE_ENEMY(node)->bodyPos.vy;
        block->node.vz = GP_NODE_ENEMY(node)->bodyPos.vz;
        nodeCoord      = GP_NODE_ENEMY(node)->coord;
        tmp            = block->node;
        gte_SetRotMatrix(&nodeCoord->workm);
        gte_ldv0(&tmp);
        gte_rtv0();
        gte_stsv(&block->node);
        block->node.vx += GP_NODE_ENEMY(node)->coord->workm.t[0];
        block->node.vy += GP_NODE_ENEMY(node)->coord->workm.t[1];
        block->node.vz += GP_NODE_ENEMY(node)->coord->workm.t[2];
        if (func_800E0308(&block->node, &block->self) != 1) {
            bestAngle = angle;
            best      = node;
            bestDist  = dist;
        }
    }
    if (best != NULL) {
        Gp_GetLockPos(best, out);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpLockScanScratch);
    return best;
}

void func_800DA6E8(void* arg0, s32 arg1, s32 arg2)
{
    GpSlot70* found;
    s32       i;
    GpSlot70* p;

    found = NULL;
    i     = 0;
    p     = Gp_LockSlots;
loop:
    if (p->field_0 == arg0) {
        if (arg1 >= 0) {
            if (p->field_4 >= 0) {
                found = p;
                goto done;
            }
            p++;
        } else if (p->field_4 < 0) {
            found = p;
            goto done;
        } else {
            p++;
        }
    } else {
        p++;
    }
    i++;
    if (i < 0x20) {
        goto loop;
    }
done:
    if (found == NULL) {
        i = 0;
        p = Gp_LockSlots;
    loop2:
        if (p->field_0 == NULL) {
            found          = p;
            p->field_0     = arg0;
            found->field_4 = 0;
        } else {
            i++;
            p++;
            if (i < 0x20) {
                goto loop2;
            }
        }
        if (found != NULL) {
            goto update;
        }
    } else {
    update:
        found->field_6  = 0x14;
        found->field_4 += arg1;
    }
}

static void Gp_UpdateLockSlots(void)
{
    RECT             rect;
    u8               buf[16];
    TextDrawReq      req;
    s32              i;
    GpSlot70*        slot;
    u8*              bufp;
    TextDrawReq*     reqp;
    s32              x;
    s32              y;
    s32              val;
    s32              x14;
    s32              ot;
    void*            obj;
    WorldTargetNode* node;
    s32              found;

    slot = Gp_LockSlots;
    i    = 0;
    bufp = buf;
    reqp = &req;
    ot   = -0xA;
    do {
        obj = slot->field_0;
        if (obj == NULL) {
            goto empty;
        }
        node  = gWorldTargetListHead;
        found = 0;
        if (node != NULL) {
            do {
                if (obj == node) {
                    found = 1;
                    goto check_found;
                }
                node = node->next;
            } while (node != NULL);
        }
    check_found:
        if (found != 0) {
            project_slot(&slot->screen.packed, slot);
        } else {
            slot->field_0 = (void*)4;
        }

        val = slot->field_4;
        if (val >= 0) {
            x = slot->screen.xy.vx + 0xA;
            y = slot->screen.xy.vy + 4;
        } else {
            x = slot->screen.xy.vx - 0xA;
            y = slot->screen.xy.vy - 0x10;
        }
        if (x < -0x88) {
            x = (x & 7) - 0x8F;
        }
        if (x >= 0x89) {
            x = -(x & 7) + 0x8F;
        }
        if (y >= 0x55) {
            y = (y & 7) + 0x4D;
        }
        if (y < -0x64) {
            y = -(y & 7) - 0x5D;
        }

        x14            = x + 0xE;
        req.x          = x14;
        req.y          = y;
        req.otIndex    = ot;
        req.colorRgb   = 0x37A78;
        req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
        req.alignment  = TEXT_ALIGNMENT_RIGHT;
        req.drawMode   = TEXT_DRAW_OUTLINED;

        val = slot->field_4;
        if (val < 0) {
            req.colorRgb = 0x808008;
            val          = -val;
        }
        if (val >= 0x2710) {
            val = 0x270F;
        }

        req.x        = x14;
        req.drawMode = TEXT_DRAW_QUEUED;
        Text_DrawString(reqp, Text_ItoaSigned(bufp, val));
        req.x        = x14;
        req.drawMode = TEXT_DRAW_OUTLINE_ONLY;
        Text_DrawString(reqp, Text_ItoaSigned(bufp, val));

        rect.x = x - 0x10;
        rect.y = y - 8;
        rect.w = 0x20;
        rect.h = 0xC;
        if (val >= 0x3E8) {
            rect.x = x - 0x18;
            rect.w = 0x28;
        } else if (val < 0x64) {
            rect.x = x - 8;
            rect.w = 0x18;
        }
        Ui_DrawTextInRect(&rect, -0xA, 2, NULL);

        {
            s16 timer;
            timer = slot->field_6;
            timer--;
            slot->field_6 = timer;
            if (timer > 0) {
                goto next;
            }
        }
        slot->field_4 = 0;
        slot->field_6 = 0;
        slot->field_0 = NULL;
        goto next;

    empty:
        slot->field_4 = 0;
        slot->field_6 = 0;
    next:
        i++;
        slot++;
    } while (i < 0x20);
}

void Gp_UnlinkNode(WorldTargetNode* node)
{
    s32               i;
    Task* volatile*   p;
    Task*             work;
    GameActor*        actor;
    WorldTargetNode** list;

    i = 0;
    p = gPlayerActorTasks;
    do {
        work = *p;
        if (work != NULL) {
            actor = work->work;
            if (actor->targetNode == node) {
                actor->targetNode = NULL;
            }
        }
        i++;
        p++;
    } while (i < PLAYER_ACTOR_TASK_COUNT);

    if (node->state.parts.onList == 1) {
        list = &gWorldTargetListHead;
        if (gWorldTargetListHead != node) {
            do {
                if (*list == NULL) {
                    goto done;
                }
                list = &(*list)->next;
            } while (*list != node);
        }
        if (*list != NULL) {
            *list = node->next;
        }
    done:
        node->state.parts.onList   = 0;
        node->state.parts.targeted = 0;
    }
}

void Gp_LinkNode(WorldTargetNode* node)
{
    WorldTargetNode** p;

    if (node->state.parts.onList == 0) {
        p = &gWorldTargetListHead;
        while (*p != NULL) {
            p = &(*p)->next;
        }
        *p                         = node;
        node->next                 = NULL;
        node->state.parts.targeted = 0;
        node->state.parts.onList   = 1;
        node->state.parts.flags   &= ~WORLD_TARGET_NOT_LOCKABLE;
    } else {
        node->state.parts.flags &= ~WORLD_TARGET_NOT_LOCKABLE;
    }
}

s32 Gp_NodeSlotMask(WorldTargetNode* node)
{
    s32             mask;
    s32             i;
    s32             one;
    Task* volatile* p;
    Task*           work;

    mask = 0;
    i    = mask;
    one  = 1;
    p    = gPlayerActorTasks;
    do {
        work = *p;
        if (work != NULL) {
            if (((GameActor*)work->work)->targetNode == node) {
                mask |= one << i;
            }
        }
        i++;
        p++;
    } while (i < PLAYER_ACTOR_TASK_COUNT);
    return mask;
}

void Gp_AssignNodeSlot0(WorldTargetNode* node)
{
    Task*            work;
    GameActor*       actor;
    WorldTargetNode* previous;
    u8               val;

    work = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    if (work != NULL) {
        actor    = work->work;
        previous = actor->targetNode;
        if (previous != NULL) {
            previous->state.parts.targeted = 0;
        }
        actor->targetNode = node;
    }
    val                        = node->state.parts.flags;
    node->state.parts.targeted = 1;
    node->state.parts.flags    = val & WORLD_TARGET_NOT_LOCKABLE_CLEAR;
}

void Gp_ClearNodeSlots(WorldTargetNode* node)
{
    s32             i;
    Task* volatile* p;
    Task*           work;
    GameActor*      actor;
    u8              val;

    i = 0;
    p = gPlayerActorTasks;
    do {
        work = *p;
        if (work != NULL) {
            actor = work->work;
            if (actor->targetNode == node) {
                actor->targetNode = NULL;
            }
        }
        i++;
        p++;
    } while (i < PLAYER_ACTOR_TASK_COUNT);
    val                        = node->state.parts.flags;
    node->state.parts.targeted = 0;
    node->state.parts.flags    = val | WORLD_TARGET_NOT_LOCKABLE;
}

void* Gp_FindLockNode(Task* arg0)
{
    VECTOR3 pos;

    return Gp_ScanLockNodes(arg0, &pos, 0);
}

void* Gp_FindLockNodePad(Task* arg0)
{
    VECTOR3  pos;
    VECTOR3* p;
    s32      flag;

    p = &pos;
    if (Pad_CheckButtons(0, 0, 0x8000) != 0) {
        flag = 1;
    } else if (Pad_CheckButtons(0, 0, 0x2000) != 0) {
        flag = -1;
    } else {
        flag = 0;
    }
    return Gp_ScanLockNodes(arg0, p, flag);
}

static void* Gp_FindLockNodeAt(Task* arg0, VECTOR3* pos)
{
    s32 flag;

    if (Pad_CheckButtons(0, 0, 0x8000) != 0) {
        flag = 1;
    } else if (Pad_CheckButtons(0, 0, 0x2000) != 0) {
        flag = -1;
    } else {
        flag = 0;
    }
    return Gp_ScanLockNodes(arg0, pos, flag);
}

void Gp_GetLockPos(WorldTargetNode* arg0, VECTOR3* out)
{
    GfxCoord* world;
    GfxCoord* coord;
    u8*       head;
    MATRIX*   mat;

    if (arg0 == NULL) {
        printf(Gp_StrGetLockPosNull);
        out->vx = 0;
        out->vy = 0;
        out->vz = 0;
        return;
    }

    coord = GP_NODE_ENEMY(arg0)->coord;
    world = &gGfxViewCoord;
    if (coord == world) {
        out->vx = GP_NODE_ENEMY(arg0)->bodyPos.vx;
        out->vy = GP_NODE_ENEMY(arg0)->bodyPos.vy;
        out->vz = GP_NODE_ENEMY(arg0)->bodyPos.vz;
        return;
    }

    head                       = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(void) = head - 0x28;
    Gp_UpdateCoord(coord);
    mat = (MATRIX*)(head - 0x20);
    Gp_WorldToLocal(&world->workm, &coord->workm, mat);
    gte_SetRotMatrix(mat);
    gte_SetTransMatrix(mat);
    gte_ldlvl(&GP_NODE_ENEMY(arg0)->bodyPos);
    gte_rtirtr();
    gte_stlvl(out);
    SCRATCH_STACK_RELEASE_BYTES(0x28);
}

static void Gp_ClearLockSlots(void)
{
    s32       i;
    GpSlot70* p;

    p = Gp_LockSlots;
    i = 0;
    do {
        i++;
        p->field_0 = NULL;
        p->field_4 = 0;
        p->field_6 = 0;
        p++;
    } while (i < 0x20);
}

void Gp_ResetLinkState(void)
{
    gWorldTargetListHead = NULL;
    Gp_ClearLockSlots();
    D_8010F9F0 = 0xFFF00000;
    D_8010F9EC = 0xFFF00000;
}

static s32 Gp_ProjectToSxy(WorldTargetNode* arg0, s32* sxy)
{
    GpPerspScratch* block;
    s32             ret;

    block         = SCRATCH_STACK_RESERVE_BLOCK(GpPerspScratch);
    block->vec.vx = GP_NODE_ENEMY(arg0)->bodyPos.vx;
    block->vec.vy = GP_NODE_ENEMY(arg0)->bodyPos.vy;
    block->vec.vz = GP_NODE_ENEMY(arg0)->bodyPos.vz;
    gte_SetRotMatrix(&GP_NODE_ENEMY(arg0)->coord->workm);
    gte_SetTransMatrix(&GP_NODE_ENEMY(arg0)->coord->workm);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(sxy);
    gte_stdp(&block->p);
    gte_stflg(&block->flag);
    gte_stszotz(&block->otz);
    ret = block->otz;
    SCRATCH_STACK_RELEASE_BLOCK(GpPerspScratch);
    return ret;
}

void Gp_ClearSlotNodeFlags(void)
{
    s32              i;
    Task* volatile*  p;
    Task*            work;
    WorldTargetNode* node;

    i = 0;
    p = gPlayerActorTasks;
    do {
        work = *p;
        if (work != NULL) {
            node = ((GameActor*)work->work)->targetNode;
            if (node != NULL) {
                node->state.parts.targeted = 0;
            }
        }
        i++;
        p++;
    } while (i < PLAYER_ACTOR_TASK_COUNT);
}

s32 Gp_GrantLocationItems(InventoryItemRange* arg0)
{
    GameLocationKey* loc;
    GpGiveRec*       rec;
    s32              key;
    s32              ret;
    s32              i;
    u16              item;
    s8               mode;
    u8               stage;
    u8               area;
    u8               sub;

    ret   = 0;
    loc   = &gGameSession->location.loc;
    stage = loc->stage;
    area  = loc->area;
    sub   = loc->variant;
    key   = (stage << 24) | (area << 16) | (sub << 8);
    mode  = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode;
    if ((mode == 0) || (mode == 2)) {
        rec = D_8010F9F4[stage];
    } else {
        rec = D_8010FA0C[stage];
    }
    if (rec->field_0 != -1) {
        do {
            if (rec->field_0 == key) {
                for (i = 0; i < 4; i++) {
                    item = rec->items[i];
                    if (item != 0) {
                        if ((i != 3) || (func_800B9D80(0x80000) != 0)) {
                            if (func_800B7420(item) == 0) {
                                ret = 1;
                                if (i == 3) {
                                    ret = 2;
                                }
                                Gp_GiveItem(arg0, item, -1);
                            }
                        }
                    }
                }
                return ret;
            }
            rec++;
        } while (rec->field_0 != -1);
    }
    return ret;
}

s32 Gp_LoadActorImage(Task* arg0, GpImgRec* arg1, RECT* arg2)
{
    s32        ret;
    TmdObject* extra;
    s32        x;

    extra = arg0->extra.tmd;
    ret   = 0;
    if (arg1 != NULL) {
        arg1->rect.x = (extra->texturePageOffset << 6) + (x = (arg2->x + 1) / 2 + 0x180);
        arg1->rect.y = arg2->y + 0x100;
        arg1->rect.w = arg2->w;
        arg1->rect.h = arg2->h;
        Gp_LoadImages(arg1);
    } else {
        ret = 1;
    }
    return ret;
}

void Gp_LoadImages(GpImgRec* arg0)
{
    RECT* dest;
    s32   done;

    done = 0;
    dest = SCRATCH_STACK_RESERVE_BLOCK(RECT);

    do {
        switch (arg0->field_0) {
            case 0:
                dest->x = arg0->rect.x;
                dest->y = arg0->rect.y;
                dest->w = arg0->rect.w;
                dest->h = arg0->rect.h;
                LoadImage(dest, arg0->data);
                break;
            case GP_IMG_REC_END:
                done = 1;
                break;
            default:
                done = 1;
                break;
        }
        arg0++;
    } while (done == 0);

    SCRATCH_STACK_RELEASE_BLOCK(RECT);
}

void Gp_InitStateF0(void)
{
    SceneCombatState* combat;
    McSaveData*       save;
    u8                difficulty;

    combat                                      = &gSceneCombatState;
    gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_IDLE;
    combat->signals.bytes.endDelayFrames        = 0;
    combat->signals.bytes.actionFlags           = 0;
    combat->signals.bytes.enemyAlert            = 0;
    combat->actorControl                        = SCENE_COMBAT_ACTORS_RUNNING;
    combat->peTargetCount                       = 0;
    combat->battleRefs                          = 0;
    combat->expReward                           = 0;
    combat->bpReward                            = 0;
    combat->mpReward                            = 0;
    combat->lifeDrainHp                         = 0;
    combat->actor00700DeathAlert                = 0;
    combat->actor03700Flags                     = 0;
    combat->actor03700Wave                      = 0;
    combat->actor02400Alert                     = 0;
    combat->actor01600Wave                      = 0;
    combat->pairedEnemySignals                  = 0;
    combat->spiderEntranceReady                 = 0;
    combat->hopperAlertOwner                    = 0;
    combat->shrineEnemyPhase                    = SCENE_COMBAT_SHRINE_HIDDEN;
    combat->actor02500EntranceReady             = 0;
    combat->spiderAmbushReady                   = 0;
    combat->actor00400HideRequested             = 0;
    combat->bruteGroupPhase                     = SCENE_COMBAT_BRUTE_WAITING;
    combat->enemySoundBankQueued                = 0;
    combat->podDeathStarted                     = 0;
    combat->bruteDeathAlert                     = 0;
    combat->actor00300AttackAlert               = 0;
    combat->lungerDeathAlert                    = 0;
    combat->field_2A                            = 0;
    if (Gp_IsDebugAttachRoom() == 1) {
        combat->difficulty = SCENE_COMBAT_DIFFICULTY_NORMAL;
    } else {
        save               = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        difficulty         = (u8)save->state.gameMode;
        combat->difficulty = difficulty;
        if (difficulty == SCENE_COMBAT_DIFFICULTY_NORMAL) {
            if (save->state.clearCount != 0) {
                combat->difficulty = SCENE_COMBAT_DIFFICULTY_REPLAY;
            }
        }
    }
}

void Gp_ArmStateF0(s32 arg0)
{
    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_IDLE) {
        gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_ENGAGED;
    }
}

void Gp_SetStateF0Bit(s32 arg0)
{
    if (arg0 != 0) {
        gSceneCombatState.signals.bytes.actionFlags |= 1 << (arg0 - 1);
    }
}

void Gp_SetStateF0Byte3(s32 arg0)
{
    gSceneCombatState.signals.bytes.enemyAlert = arg0;
}

void Gp_IncStateF0Ref(s32 arg0)
{
    gSceneCombatState.battleRefs++;
}

void Gp_ReleaseStateF0Add(Task* arg0, s32 arg1)
{
    SceneCombatState* combat;
    SceneCombatState* rewards;
    EnemyParams*      params;

    combat = &gSceneCombatState;
    if (combat->battleRefs != 0) {
        combat->battleRefs--;
        if (combat->battleRefs == 0) {
            gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_FINISHED;
            combat->signals.bytes.actionFlags           = 0;
            combat->signals.bytes.enemyAlert            = 0;
            combat->signals.bytes.endDelayFrames        = SCENE_COMBAT_END_DELAY_FRAMES;
            if (!(gGameSession->flowFlags & GAME_SESSION_FLOW_SKIP_AREA_MUSIC)) {
                SndEvt_EnqueueType2(0, 0xB4);
            }
        }
        params = ((Enemy*)arg0->spawnArg2.pointer)->param;
        if (params != NULL) {
            rewards             = &gSceneCombatState;
            rewards->expReward += params->exp;
            rewards->bpReward  += params->bp;
            rewards->mpReward  += params->mp;
        }
    }
}

void Gp_ReleaseStateF0Clear(Task* unusedTask, s32 unusedArg)
{
    SceneCombatState* combat;

    combat = &gSceneCombatState;
    if (combat->battleRefs != 0) {
        combat->battleRefs--;
        if (combat->battleRefs == 0) {
            gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_FINISHED;
            combat->signals.bytes.actionFlags           = 0;
            combat->signals.bytes.enemyAlert            = 0;
            combat->signals.bytes.endDelayFrames        = SCENE_COMBAT_END_DELAY_FRAMES;
            combat->expReward                           = 0;
            combat->bpReward                            = 0;
            combat->mpReward                            = 0;
            if (!(gGameSession->flowFlags & GAME_SESSION_FLOW_SKIP_AREA_MUSIC)) {
                SndEvt_EnqueueType2(0, 0xB4);
            }
        }
    }
}

void Gp_ReleaseStateF0(Task* arg0, s32 arg1)
{
    SceneCombatState* combat;

    combat = &gSceneCombatState;
    if (combat->battleRefs != 0) {
        combat->battleRefs--;
        if (combat->battleRefs == 0) {
            gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_FINISHED;
            combat->signals.bytes.actionFlags           = 0;
            combat->signals.bytes.enemyAlert            = 0;
            combat->signals.bytes.endDelayFrames        = SCENE_COMBAT_END_DELAY_FRAMES;
            if (!(gGameSession->flowFlags & GAME_SESSION_FLOW_SKIP_AREA_MUSIC)) {
                SndEvt_EnqueueType2(0, 0xB4);
            }
        }
    }
}
