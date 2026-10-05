#include "pe/necrosis.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/effects.h"
#include "gameplay/pad_script.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
/// Signed texture-frame selector; the caller reduces the effect age modulo six.
#define SPRITE_QUAD_FRAME_T s16
#include "../../shared/sprite_quad.h"

/// Size and travel time of the necrosis cloud at one Parasite Energy level.
///
/// The cast launches a cloud along the caster's facing. The cloud carries a
/// collision sphere keyed with the spell's identity and sheds one trail puff
/// per frame, both of which grow as it travels. The cast selects its row by the
/// level digit of the spell being cast (`AttachmentState::attachId % 10 - 1`),
/// so a higher level launches a larger cloud that travels for longer.
///
/// The radius is in game-coordinate units. The frame count is compared with
/// the cast's `EffectWork::age`, which is 1 on the launch frame and is held
/// while parasite-energy effects are paused for as long as the cloud travels.
typedef struct {
    s16 startRadius;  // Radius the keyed collision sphere is launched with, gaining 0x20 a frame from the launch frame on; a trail puff's sprite size is this plus 0x60 for each frame of the cast's age
    s16 travelFrames; // Frames the cloud keeps its collision spheres for. On the frame after, it sheds its last puff and drops them, and the cast ends 0x10 frames later; the controller vibration that fades out under the cast lasts 0xC frames longer than this
} _NecrosisLevelTuning;
STATIC_ASSERT_SIZEOF(_NecrosisLevelTuning, 4);

/// Collision block of the travelling necrosis cloud, allocated zeroed on the
/// cast's first running frame and kept at `Task::work`.
///
/// Both bodies are spheres centred on the origin of the coordinate the cloud
/// travels on, and both borrow the one contact entry. The damage sphere is what
/// the cloud hurts with: it takes pair tests only, and its key is the damage id
/// of the spell being cast - contact category 2 with bit 0x8000, which selects
/// the attachment level table, plus that spell and level's row number. The
/// grid sphere is what stops the cloud: collision list 7 is walked only by the
/// room-grid pass, and a contact with a class-0 room surface
/// (`WORLD_COLLISION_CONTACT_GRID` alone) zeroes the cloud's velocity and
/// unlinks that sphere, leaving the damage sphere growing in place. Both are
/// unlinked when the travel time runs out or the cast ends early.
typedef struct {
    WorldCollisionBody    damageBody;  // Sphere linked on list 1; pair tests are enabled after the link. Launched with the level's `startRadius` and gains 0x20 each travelling frame
    WorldCollisionBody    gridBody;    // Sphere of radius 0x80 linked on list 7 with a zero key; grid tests are enabled after the link, together with `WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT`, which sphere tests do not read
    WorldCollisionContact contacts[1]; // One-entry table both bodies borrow. The entry is marked LAST; occupied contacts are cleared each travelling frame, and only a grid contact is ever looked for
} _NecrosisWork;
STATIC_ASSERT_SIZEOF(_NecrosisWork, 0x58);

/// Per-level tuning for the necrosis cloud, one row per PE level 1-3, weakest
/// first.
static _NecrosisLevelTuning D_necrosis_801306BC[] = {
    { 0x03C0, 0x000A },
    { 0x0480, 0x000F },
    { 0x0540, 0x0014 },
};

/// The `sndEvtRequestScriptStart` id for each `D_necrosis_801306BC` row.
static s32 D_necrosis_801306C8[] = { 0xE0150001, 0xE0180001, 0xE01B0001 };

static void func_necrosis_8012FE64(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3);
static void func_necrosis_80130288(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3);

/// Runs one frame of the necrosis cast. State 0 copies the player rotation onto
/// the effect coordinate, rotates a (0, 0, 0x90) offset into that frame, and
/// links a `_NecrosisWork` collision pair (list 1 + list 7) whose packed id is
/// the combo digits plus `0x28000`. State 1 GPF-scales that offset by 0x1100
/// each frame, walks the coordinate, and spawns `0x80060019`; a `0x100000` hit
/// on `gridBody` zeros the offset and unlinks the list-7 object. State 2 waits
/// `travelFrames + 0x10` ticks. Any state releases if the player is dying
/// (`Gp_StateC08.effectPhase` / `Gp_StateC08.effectPhase`) or parasite-energy effects are
/// cancelled (`gRoomEffectState->peEffectControl`).
void func_necrosis_8012EF34(Task* arg0)
{
    _NecrosisWork*         work;
    EffectWork*            mem;
    GfxCoord*              coord;
    GfxCoord*              player;
    GfxRotationWords*      destinationRotation;
    GfxRotationWords*      sourceRotation;
    WorldCollisionContact* contacts;
    EffectWork*            spawned;
    s32                    pan;
    u16                    old;
    s32                    tick;
    s16                    peEffectControl;

    work     = arg0->work;
    mem      = arg0->spawnArg2.pointer;
    coord    = arg0->extra.coordBody->coord;
    old      = mem->age;
    tick     = old + 1;
    mem->age = tick;
    switch (arg0->state) {
        case 0:
            if (Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) {
                goto release;
            }
            peEffectControl = gRoomEffectState->peEffectControl;
            if (peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                goto release;
            }
            if (peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                mem->age = old;
                return;
            }
            work = memCalloc(sizeof(_NecrosisWork), 0);
            if (work == NULL) {
                mem->age = 0;
                return;
            }
            player                      = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            destinationRotation         = (GfxRotationWords*)&coord->coord;
            sourceRotation              = (GfxRotationWords*)&player->coord;
            destinationRotation->m00M01 = sourceRotation->m00M01;
            destinationRotation->m02M10 = sourceRotation->m02M10;
            destinationRotation->m11M12 = sourceRotation->m11M12;
            destinationRotation->m20M21 = sourceRotation->m20M21;
            destinationRotation->m22    = sourceRotation->m22;
            coord->composeStamp         = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            mem->move.vx = 0;
            mem->move.vy = 0;
            mem->move.vz = 0x90;
            gte_SetRotMatrix(&player->coord);
            gte_ldv0(&mem->move);
            gte_rtv0();
            gte_stsv(&mem->move);
            contacts                          = work->contacts;
            mem->index                        = (Gp_StateC08.attachId % 10) - 1;
            arg0->work                        = work;
            work->damageBody.coord            = coord;
            work->damageBody.context.contacts = contacts;
            work->damageBody.key =
                ((u16)(Gp_StateC08.attachId / 100) - 1) * 9 + ((u16)((u16)(Gp_StateC08.attachId % 100) / 10) - 1) * 3 + (u16)(Gp_StateC08.attachId % 10) + 0x28000;
            work->damageBody.radius = D_necrosis_801306BC[mem->index].startRadius;
            work->damageBody.flags  = WORLD_COLLISION_BODY_SPHERE;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &work->damageBody);
            contacts->flags                 = WORLD_COLLISION_CONTACT_LAST;
            work->gridBody.coord            = coord;
            work->gridBody.context.contacts = contacts;
            work->gridBody.key              = 0;
            work->gridBody.radius           = 0x80;
            work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->damageBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_GRID_ONLY, &work->gridBody);
            work->gridBody.flags = (work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED)) | (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED);
            pan                  = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(D_necrosis_801306C8[(u16)(Gp_StateC08.attachId % 10) - 1], pan,
                                     (s8)worldCoordGetOriginAudioDepth(coord));
            Gp_SpawnPadLerp(D_necrosis_801306BC[mem->index].travelFrames + 0xC, 0xFF, 8);
            arg0->state = 1;
            /* fallthrough */
        case 1:
            if (gRoomEffectState->peEffectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                gte_lddp(0x1100);
                gte_ldsv(&mem->move);
                gte_gpf12();
                gte_stsv(&mem->move);
                coord->coord.t[0]  += mem->move.vx;
                coord->coord.t[1]  += mem->move.vy;
                coord->coord.t[2]  += mem->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                spawned = Gp_SpawnEff((EFFECT_NECROSIS_TRAIL_PUFF | EFFECT_SPAWN_UNLIMITED), coord,
                                      D_necrosis_801306BC[mem->index].startRadius + (mem->age * 0x60),
                                      NULL);
                if (spawned != NULL) {
                    taskReparent(arg0, spawned->task);
                }
                work->damageBody.radius = work->damageBody.radius + 0x20;
            } else {
                mem->age = mem->age - 1;
            }
            if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                worldCollisionUnlinkBody(&work->damageBody);
                worldCollisionUnlinkBody(&work->gridBody);
                goto release;
            }
            if (mem->age > D_necrosis_801306BC[mem->index].travelFrames) {
                worldCollisionUnlinkBody(&work->damageBody);
                worldCollisionUnlinkBody(&work->gridBody);
                arg0->state = 2;
                return;
            }
            if (worldCollisionFindContactIndex(work->gridBody.context.contacts, WORLD_COLLISION_CONTACT_GRID) != 0) {
                mem->move.vx = 0;
                mem->move.vy = 0;
                mem->move.vz = 0;
                worldCollisionUnlinkBody(&work->gridBody);
            }
            worldCollisionClearContacts(work->contacts);
            return;
        case 2:
            if (Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) {
                goto release;
            }
            if (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                goto release;
            }
            tick = (s16)tick;
            if ((D_necrosis_801306BC[mem->index].travelFrames + 0x10) < tick) {
            release:
                effectKillTask(mem, arg0);
            }
            break;
    }
}

void func_necrosis_8012F52C(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    EffectWork* spawned;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (gRoomEffectState->peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        return;
    }

    mem->age = mem->age + 1;
    if (arg0->state == 0) {
        mem->scale      = arg0->spawnArg1.value & 0xFFF;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->angle      = (gRandomLcgState >> 16) & 0xFFF;
        mem->period     = mem->scale - 0x100;
        mem->step       = mem->scale >> 4;
        arg0->state     = 1;
    }
    actorRenderComposeCoord(coord);
    spriteQuadDraw(coord, mem->age % 6, mem->scale, mem->angle);
    mem->scale = mem->scale - mem->step;
    if (mem->scale < mem->step) {
        effectKillTask(mem, arg0);
        return;
    }
    if (mem->age % 3 == 0) {
        spawned = Gp_SpawnEff(EFFECT_NECROSIS_MIST_PUFF, coord, (s32)(mem->period), 0);
        if (spawned != NULL) {
            taskReparent(arg0, spawned->task);
        }
    }
}

/// Trail-puff palette: VRAM X=240 words, Y=266 scanlines.
#define SPRITE_QUAD_CLUT getClut(240, 266)
/// Texel width and horizontal stride of each of the six frames selected by the caller.
#define SPRITE_QUAD_CELL_WIDTH 40
/// Inclusive top texel row of the necrosis strip, relative to its texture page.
///
/// Signed integer constant for the next drawer inclusion; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V 0x38
#define SPRITE_QUAD_V1    0x5F
/// Perspective-sizing multiplier for the necrosis sprite.
///
/// Uses the cell's inclusive 39-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
#include "../../shared/sprite_quad_draw.inc.c"

void func_necrosis_8012FAF8(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s16         tick;
    s32         rng1;
    s32         rng2;
    s32         rng3;
    s32         temp_lo;
    s32         var_v1;
    u16         temp_v0;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (gRoomEffectState->peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        return;
    }

    mem->age = mem->age + 1;
    switch (arg0->state) {
        case 0:
            mem->age        = 0;
            temp_v0         = arg0->spawnArg1.value;
            mem->period     = temp_v0 & 0xFFF;
            rng1            = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            mem->scale      = ((u32)rng1 >> 16) & 0xFFF;
            gRandomLcgState = rng1;
            mem->angle      = mem->period / 20;
            mem->move.vx    = (rsin(mem->scale) * mem->angle) >> 12;
            temp_lo         = rcos(mem->scale) * mem->angle;
            rng2            = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rng2;
            mem->move.vy    = temp_lo >> 12;
            mem->move.vz    = (rsin(((u32)rng2 >> 16) & 0xFFF) * mem->move.vx) >> 12;
            rng3            = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rng3;
            if ((s32)(((u32)rng3 >> 16) & 3) < ((u16)(Gp_StateC08.attachId % 10U) - 1)) {
                mem->step = 0x1000;
            }
            if ((u16)(Gp_StateC08.attachId % 10U) - 1 < 2) {
                arg0->state = 1;
                return;
            }
            var_v1 = 2;
            if (mem->step != 0) {
                var_v1 = 1;
            }
            arg0->state = var_v1;
            return;
        case 1:
            coord->coord.t[0]  += mem->move.vx;
            coord->coord.t[1]  += mem->move.vy;
            coord->coord.t[2]  += mem->move.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            tick       = mem->index + 1;
            mem->index = tick;
            if (tick < 8) {
                func_necrosis_8012FE64(coord, (s16)(tick | mem->step), mem->period,
                                       mem->scale);
                return;
            }
            effectKillTask(mem, arg0);
            return;
        case 2:
            coord->coord.t[0]  += mem->move.vx;
            coord->coord.t[1]  += mem->move.vy;
            coord->coord.t[2]  += mem->move.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            tick       = mem->index + 1;
            mem->index = tick;
            if (tick < 6) {
                func_necrosis_80130288(coord, (s16)(tick | mem->step), mem->period,
                                       mem->scale);
                return;
            }
            effectKillTask(mem, arg0);
            return;
    }
}

/// Draws one frame of the necrosis mist puff. `arg0`'s world position is
/// projected through `GsWSMATRIX` by a single `RTPS` and the quad is dropped
/// when that sets a negative `gte_stflg`. `arg1` is packed by the caller: the
/// low nibble picks one of the 0x20-wide texture cells on row 0x18..0x37, and
/// bit 0x1000 swaps the pale tpage/CLUT pair (0x2A / 0x428F) for the dark one
/// (0x4A / 0x42C2). `arg3` spins the quad and `arg2` sizes it: the corners sit
/// `arg2 * 31 / otz` from the projected centre along `arg3` and `arg3 + 0x400`,
/// so the puff shrinks with depth. Same shape as `spriteQuadDraw`.
static void func_necrosis_8012FE64(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    s32                 u0;
    s32                 u1;
    s32                 ang2;

    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    block->worldPoint.vx = arg0->workm.t[0];
    block->worldPoint.vy = arg0->workm.t[1];
    block->worldPoint.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        if (arg1 & 0x1000) {
            prim->tpage = 0x2A;
            prim->clut  = 0x428F;
        } else {
            prim->tpage = 0x4A;
            prim->clut  = 0x42C2;
        }
        setSemiTrans(prim, 1);
        setShadeTex(prim, 1);
        u0 = (arg1 & 0xF) << 5;
        u1 = u0 + 0x1F;
        setUV4(prim, u0, 0x18, u1, 0x18, u0, 0x37, u1, 0x37);
        block->extent.corner.x = (((arg2 * 31) / block->depth) * rsin(arg3)) >> 12;
        block->extent.corner.y = (((arg2 * 31) / block->depth) * rcos(arg3)) >> 12;
        prim->x0               = block->screenX + block->extent.corner.x;
        prim->x3               = block->screenX - block->extent.corner.x;
        prim->y0               = block->screenY - block->extent.corner.y;
        prim->y3               = block->screenY + block->extent.corner.y;
        ang2                   = arg3 + 0x400;
        block->extent.corner.x = (((arg2 * 31) / block->depth) * rsin(ang2)) >> 12;
        block->extent.corner.y = (((arg2 * 31) / block->depth) * rcos(ang2)) >> 12;
        prim->x1               = block->screenX + block->extent.corner.x;
        prim->x2               = block->screenX - block->extent.corner.x;
        prim->y1               = block->screenY - block->extent.corner.y;
        prim->y2               = block->screenY + block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

/// Draws one frame of the necrosis spore cloud. Same shape as
/// `func_necrosis_8012FE64`: `arg0`'s world position is projected through
/// `GsWSMATRIX` by a single `RTPS` and the quad is dropped when that sets a
/// negative `gte_stflg`. `arg1` is packed by the caller: the low nibble picks
/// one of the 0x28-wide texture cells on row 0x50..0x77, and bit 0x1000 swaps
/// the dark tpage/CLUT pair (0x49 / 0x42C2) for the pale one (0x29 / 0x428F).
/// `arg3` spins the quad and `arg2` sizes it: the corners sit `arg2 * 39 / otz`
/// from the projected centre along `arg3` and `arg3 + 0x400`, so the cloud
/// shrinks with depth.
static void func_necrosis_80130288(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    s32                 u0;
    s32                 u1;
    s32                 ang2;

    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    block->worldPoint.vx = arg0->workm.t[0];
    block->worldPoint.vy = arg0->workm.t[1];
    block->worldPoint.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        if (arg1 & 0x1000) {
            prim->tpage = 0x29;
            prim->clut  = 0x428F;
        } else {
            prim->tpage = 0x49;
            prim->clut  = 0x42C2;
        }
        setSemiTrans(prim, 1);
        setShadeTex(prim, 1);
        u0 = (arg1 & 0xF) * 40;
        u1 = u0 + 0x27;
        setUV4(prim, u0, 0x50, u1, 0x50, u0, 0x77, u1, 0x77);
        block->extent.corner.x = (((arg2 * 39) / block->depth) * rsin(arg3)) >> 12;
        block->extent.corner.y = (((arg2 * 39) / block->depth) * rcos(arg3)) >> 12;
        prim->x0               = block->screenX + block->extent.corner.x;
        prim->x3               = block->screenX - block->extent.corner.x;
        prim->y0               = block->screenY - block->extent.corner.y;
        prim->y3               = block->screenY + block->extent.corner.y;
        ang2                   = arg3 + 0x400;
        block->extent.corner.x = (((arg2 * 39) / block->depth) * rsin(ang2)) >> 12;
        block->extent.corner.y = (((arg2 * 39) / block->depth) * rcos(ang2)) >> 12;
        prim->x1               = block->screenX + block->extent.corner.x;
        prim->x2               = block->screenX - block->extent.corner.x;
        prim->y1               = block->screenY - block->extent.corner.y;
        prim->y2               = block->screenY + block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}
