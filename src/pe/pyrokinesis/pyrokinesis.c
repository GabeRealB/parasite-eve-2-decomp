#include "pe/pyrokinesis.h"

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
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/pad_script.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/glow_draw.h"
#include "../../shared/pyro_flame.h"
/// Signed effect-age argument whose low bit selects one of the two flame cells.
#define SPRITE_QUAD_FRAME_T s16
#include "../../shared/sprite_quad.h"
#include "../../shared/jet_cone.h"

/// Collision block of the travelling pyrokinesis flame, allocated zeroed on the
/// cast's first running frame and kept at `Task::work`, whose teardown frees it.
///
/// Both bodies are spheres centred on the origin of the coordinate the flame
/// travels on, and both borrow the one contact entry. The damage sphere is what
/// the flame burns with: it takes pair tests only, and its key is the damage id
/// of the spell being cast - contact category 2 with bit 0x8000 plus that spell
/// and level's row number. A pair contact of category 3 on it bursts the flame
/// into its closing rings and unlinks it. The grid sphere is what stops the
/// flame: collision list 7 is walked only by the room-grid pass, and a contact
/// with a class-0 room surface (`WORLD_COLLISION_CONTACT_GRID` alone) unlinks
/// that sphere and leaves the flame shrinking where it stands, the damage
/// sphere shrinking with it. Running out of range unlinks both. Unlinking a
/// body that is no longer linked does nothing, which the cast relies on when it
/// ends from a state that has already dropped one of them.
typedef struct {
    WorldCollisionBody    damageBody;  // Sphere linked on list 1; pair tests are enabled after the link. Its radius follows the flame's: 0x500 on the launch frame, the level's size while it travels, 0x40 less each frame once it is stopped
    WorldCollisionBody    gridBody;    // Sphere linked on list 7 with a zero key and one eighth of the launch radius; grid tests are enabled after the link, together with `WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT`
    WorldCollisionContact contacts[1]; // One-entry table both bodies borrow. The entry is marked LAST; an occupied contact that neither bursts nor stops the flame is cleared the frame it is found
} _PyrokinesisWork;
STATIC_ASSERT_SIZEOF(_PyrokinesisWork, 0x58);

static void func_pyrokinesis_801304C4(GfxCoord* arg0, s32 arg1);

/// The `sndEvtRequestScriptStart` id of the ignition roar, three per PE level,
/// indexed by `EffectWork.index * 3 + Task::spawnArg1` (level by cast variant).
static s32 D_pyrokinesis_80131DD8[] = {
    0xE00B0002,
    0xE00B0002,
    0xE00B0002,
    0xE00E0002,
    0xE00E0002,
    0xE00E0002,
    0xE0110002,
    0xE0110003,
    0xE0110004,
};

/// Per-flame jitter of the cone, one 8-bit LCG roll each, re-rolled as a block
/// when the cast starts.
static s16 D_pyrokinesis_80131DFC[16] = { 0 };

/// Runs one frame of the pyrokinesis cast: a five-state machine driven by
/// `Task::state`. State 0 copies the player rotation onto the effect
/// coordinate, rotates the combo-scaled launch offset into that frame, rolls
/// the 16 per-flame jitters, plays the roar picked by combo level and cast
/// variant, and links the two spheres of a `_PyrokinesisWork` (list 1 + list
/// 7), the damage sphere keyed with the combo digits plus `0x28000`. State 1
/// walks the coordinate by that offset each frame, redraws the cone and ring,
/// parks the room light slot on it and burns until the `Gp_AttachParams` extent
/// for the combo level runs out. A `0x30000` hit on `damageBody` bursts into
/// three `0x600F6` flames and moves to state 3 (or 4 for cast variant 2); a
/// `WORLD_COLLISION_CONTACT_GRID` contact on `gridBody` means a wall, which
/// drops to state 2 and fades the cone out. States 3 and 4 grow
/// the two rings until they pass the combo radius, state 4 first stepping the
/// brightness down by 8 a frame. Any state releases if the player is dying
/// (`Gp_StateC08.effectPhase` / `Gp_StateC08.effectPhase`) or parasite-energy effects are
/// cancelled (`gRoomEffectState->peEffectControl`).
void func_pyrokinesis_8012EF48(Task* arg0)
{
    EffectWork*                    mem;
    GfxCoord*                      coord;
    _PyrokinesisWork*              work;
    ModelObjectCoordBody*          body;
    GfxCoord*                      player;
    WorldCoordTransientPointLight* lightSlot;
    GfxCoord*                      lightCoord;
    WorldCoordPointLight*          slot;
    GfxRotationWords*              destinationRotation;
    GfxRotationWords*              sourceRotation;
    EffectWork*                    spawned;
    GfxCoord                       ground;
    u8                             rgb[3];
    s32                            i;
    s32                            pan;
    s16                            peEffectControl;
    s32                            tick;
    s32                            radius;
    s32                            next;
    s16                            amp;

    work       = arg0->work;
    mem        = arg0->spawnArg2.pointer;
    body       = arg0->extra.coordBody;
    coord      = body->coord;
    mem->age   = mem->age + 1;
    lightSlot  = gWorldCoordTransientPointLights;
    lightCoord = &lightSlot->light.head.transform.coord;
    slot       = &lightSlot->light;
    switch (arg0->state) {
        case 0:
            if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || ((peEffectControl = gRoomEffectState->peEffectControl), peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                effectKillTask(mem, arg0);
                return;
            }
            if (peEffectControl != 0) {
                mem->age = mem->age - 1;
                return;
            }
            work = memCalloc(sizeof(_PyrokinesisWork), 0);
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
            mem->move.vz = (Gp_StateC08.attachId % 10) * 64 + 0x1C0;
            gte_SetRotMatrix(&player->coord);
            gte_ldv0(&mem->move);
            gte_rtv0();
            gte_stsv(&mem->move);
            for (i = 0; i < 16; i++) {
                gRandomLcgState           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                D_pyrokinesis_80131DFC[i] = (gRandomLcgState >> 16) & 0xFF;
            }
            mem->scale      = 0xC0;
            mem->angle      = 0x500;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->period     = (gRandomLcgState >> 16) & 0xFFF;
            mem->index      = (Gp_StateC08.attachId % 10) - 1;
            pan             = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(D_pyrokinesis_80131DD8[mem->index * 3 + arg0->spawnArg1.value], pan,
                                     (s8)worldCoordGetOriginAudioDepth(coord));
            Gp_SpawnPadLerp((s16)(mem->index * 2 + 8), 0xFF, 8);
            if (mem->index == 1) {
                arg0->spawnArg1.value = 1;
            } else if (arg0->spawnArg1.value == 1) {
                arg0->spawnArg1.value = 0;
            }
            arg0->work                        = work;
            work->damageBody.coord            = coord;
            work->damageBody.context.contacts = work->contacts;
            work->damageBody.key              = ((u16)(Gp_StateC08.attachId / 100) - 1) * 9 +
                                   ((u16)((u16)(Gp_StateC08.attachId % 100) / 10) - 1) * 3 +
                                   (u16)(Gp_StateC08.attachId % 10) + 0x28000;
            work->damageBody.radius = mem->angle;
            work->damageBody.flags  = WORLD_COLLISION_BODY_SPHERE;
            Gp_LinkObj(1, &work->damageBody);
            work->contacts[0].flags         = WORLD_COLLISION_CONTACT_LAST;
            work->gridBody.coord            = coord;
            work->gridBody.context.contacts = work->contacts;
            work->gridBody.key              = 0;
            work->damageBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->gridBody.radius           = (s16)((u16)mem->angle << 16 >> 19);
            work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            Gp_LinkObj(7, &work->gridBody);
            work->gridBody.flags = (work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED)) | (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED);
            Gp_SpawnEff(EFFECT_PYROKINESIS_LAUNCH_CONE, coord, 0, NULL);
            rgb[0] = 0xFF;
            rgb[1] = 0x7F;
            rgb[2] = 0x3F;
            Gp_DrawFadeQuad(rgb, 1);
            arg0->state = 1;
            spriteQuadDraw(coord, mem->age, mem->angle, mem->period);
            glowDrawFlameStar(coord, mem->angle, (s16)((u16)mem->scale << 16 >> 17));
            if (Gp_CountRec18Hi(work->damageBody.context.contacts, 0x30000) != 0) {
                Gp_UnlinkObj(&work->damageBody);
                radius     = (mem->index << 9) + 0x380;
                mem->angle = radius;
                for (i = 0; i < 0x556; i += 0x2AA) {
                    spawned = Gp_SpawnEff(EFFECT_PYROKINESIS_FLAME_RING, coord, i, NULL);
                    if (spawned != NULL) {
                        taskReparent(arg0, spawned->task);
                    }
                }
                next = 3;
                if (arg0->spawnArg1.value == 2) {
                    next = 4;
                }
                arg0->state = next;
                return;
            }
            if (Gp_FindRec18(work->gridBody.context.contacts, WORLD_COLLISION_CONTACT_GRID) != 0) {
                Gp_UnlinkObj(&work->gridBody);
                arg0->state = 2;
                return;
            }
            Gp_ClearRec18Occupied(work->contacts);
            return;
        case 1:
            if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || ((peEffectControl = gRoomEffectState->peEffectControl), peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                Gp_UnlinkObj(&work->damageBody);
                Gp_UnlinkObj(&work->gridBody);
                effectKillTask(mem, arg0);
                return;
            }
            if (peEffectControl != 0) {
                mem->age = mem->age - 1;
                return;
            }
            radius                  = (mem->index << 9) + 0x380;
            mem->angle              = radius;
            work->damageBody.radius = radius;
            coord->coord.t[0]      += mem->move.vx;
            coord->coord.t[1]      += mem->move.vy;
            coord->coord.t[2]      += mem->move.vz;
            coord->composeStamp     = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            spriteQuadDraw(coord, mem->age, mem->angle, mem->period);
            glowDrawFlameStar(coord, mem->angle, (s16)((u16)mem->scale << 16 >> 17));
            if (arg0->spawnArg1.value != 0) {
                jetConeDraw(coord, mem->age, mem->angle, 0);
                jetConeDraw(coord, mem->age, mem->angle, 1);
            }
            if (mem->age < 0x1E) {
                spawned = Gp_SpawnEff(EFFECT_PYROKINESIS_FLAME_PUFF, coord, 0, NULL);
                if (spawned != NULL) {
                    taskReparent(arg0, spawned->task);
                }
            }
            if (gRoomEffectState->groundTraceEnabled != 0) {
                if (Gp_TraceGroundCoord(coord, &ground) == 1) {
                    func_pyrokinesis_801304C4(&ground, mem->angle);
                }
            }
            lightSlot->framesLeft    = 4;
            slot->inner              = (mem->index << 9) + 0x200;
            slot->outer              = slot->inner * 16;
            gRandomLcgState          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            amp                      = ((gRandomLcgState >> 16) & 0x700) + 0x800;
            slot->head.color.r       = amp;
            slot->head.color.g       = (u16)slot->head.color.r >> 1;
            slot->head.color.b       = slot->head.color.r >> 2;
            lightCoord->coord.t[0]   = coord->coord.t[0];
            lightCoord->coord.t[1]   = coord->coord.t[1];
            lightCoord->coord.t[2]   = coord->coord.t[2];
            lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            if (Gp_CountRec18Hi(work->damageBody.context.contacts, 0x30000) != 0) {
                Gp_UnlinkObj(&work->damageBody);
                for (i = 0; i < 0x556; i += 0x2AA) {
                    spawned = Gp_SpawnEff(EFFECT_PYROKINESIS_FLAME_RING, coord, i, NULL);
                    if (spawned != NULL) {
                        taskReparent(arg0, spawned->task);
                    }
                }
                next = 3;
                if (arg0->spawnArg1.value == 2) {
                    next = 4;
                }
                arg0->state = next;
                return;
            }
            if (Gp_FindRec18(work->gridBody.context.contacts, WORLD_COLLISION_CONTACT_GRID) != 0) {
                Gp_UnlinkObj(&work->gridBody);
                arg0->state = 2;
                return;
            }
            tick = mem->age;
            if (tick * 6 > Gp_AttachParams[ATTACHMENT_INDEX_PYROKINESIS][mem->index].area.extent) {
                Gp_UnlinkObj(&work->damageBody);
                Gp_UnlinkObj(&work->gridBody);
                arg0->state = 2;
                return;
            }
            if (tick < 0x1F) {
                Gp_ClearRec18Occupied(work->contacts);
                return;
            }
            Gp_UnlinkObj(&work->damageBody);
            Gp_UnlinkObj(&work->gridBody);
            effectKillTask(mem, arg0);
            return;
        case 2:
            if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || ((peEffectControl = gRoomEffectState->peEffectControl), peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                Gp_UnlinkObj(&work->damageBody);
                effectKillTask(mem, arg0);
                return;
            }
            if (peEffectControl != 0) {
                mem->age = mem->age - 1;
                return;
            }
            actorRenderComposeCoord(coord);
            radius                  = (u16)mem->angle - 0x40;
            mem->angle              = radius;
            work->damageBody.radius = radius;
            spriteQuadDraw(coord, mem->age, mem->angle, mem->period);
            glowDrawFlameStar(coord, mem->angle, (s16)((u16)mem->scale << 16 >> 17));
            if (mem->angle >= 0x81) {
                spawned = Gp_SpawnEff(EFFECT_PYROKINESIS_FLAME_PUFF, coord, 0, NULL);
                if (spawned != NULL) {
                    taskReparent(arg0, spawned->task);
                }
            }
            if (Gp_CountRec18Hi(work->damageBody.context.contacts, 0x30000) != 0) {
                Gp_UnlinkObj(&work->damageBody);
                for (i = 0; i < 0x556; i += 0x2AA) {
                    spawned = Gp_SpawnEff(EFFECT_PYROKINESIS_FLAME_RING, coord, i, NULL);
                    if (spawned != NULL) {
                        taskReparent(arg0, spawned->task);
                    }
                }
                next = 3;
                if (arg0->spawnArg1.value == 2) {
                    next = 4;
                }
                arg0->state = next;
                return;
            }
            if (mem->angle < 0x80) {
                Gp_UnlinkObj(&work->damageBody);
                effectKillTask(mem, arg0);
                return;
            }
            Gp_ClearRec18Occupied(work->contacts);
            return;
        case 3:
            if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || ((peEffectControl = gRoomEffectState->peEffectControl), peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                Gp_UnlinkObj(&work->gridBody);
                effectKillTask(mem, arg0);
                return;
            }
            if (peEffectControl != 0) {
                mem->age = mem->age - 1;
                return;
            }
            actorRenderComposeCoord(coord);
            spriteQuadDraw(coord, mem->age, mem->angle, mem->period);
            glowDrawFlameStar(coord, mem->angle, (s16)((u16)mem->scale << 16 >> 17));
            glowDrawFlameStar(coord, (s16)((u16)mem->angle * 2),
                              (s16)((u16)mem->scale << 16 >> 17));
            mem->angle = mem->angle + 0x40;
            if (mem->angle > ((mem->index << 9) + 0x580)) {
                Gp_UnlinkObj(&work->gridBody);
                effectKillTask(mem, arg0);
                return;
            }
            return;
        case 4:
            if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || ((peEffectControl = gRoomEffectState->peEffectControl), peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                Gp_UnlinkObj(&work->gridBody);
                effectKillTask(mem, arg0);
                return;
            }
            if (peEffectControl != 0) {
                mem->age = mem->age - 1;
                return;
            }
            actorRenderComposeCoord(coord);
            spriteQuadDraw(coord, mem->age, mem->angle, mem->period);
            glowDrawFlameStar(coord, mem->angle, (s16)((u16)mem->scale << 16 >> 17));
            glowDrawFlameStar(coord, (s16)((u16)mem->angle * 2),
                              (s16)((u16)mem->scale << 16 >> 17));
            mem->angle = mem->angle + 0x40;
            if (mem->angle > ((mem->index << 9) + 0x580)) {
                if (mem->scale >= 9) {
                    mem->scale = mem->scale - 8;
                    return;
                }
                Gp_UnlinkObj(&work->gridBody);
                effectKillTask(mem, arg0);
            }
            return;
    }
}

void func_pyrokinesis_8012FAC8(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s16         scene;
    s16         flag;
    s32         state;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (Gp_StateC08.effectPhase != ATTACHMENT_EFFECT_HELD) {
        scene = gRoomEffectState->battleState;
        if (scene == ROOM_EFFECT_BATTLE_ENGAGED) {
            flag = gRoomEffectState->peEffectControl;
            if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
                    return;
                }
                mem->age = mem->age + 1;
                actorRenderComposeCoord(coord);
                state = arg0->state;
                if (state == scene) {
                    goto L_case1;
                }
                if (state < 2) {
                    if (state == 0) {
                        goto L_case0;
                    }
                    return;
                }
                if (state == 2) {
                    goto L_case2;
                }
                if (state == 3) {
                    goto L_release;
                }
                return;
            L_case0:
                Gp_SpawnEff((EFFECT_PYROKINESIS_CAST | EFFECT_SPAWN_UNLIMITED), coord, 0, 0);
                arg0->state = scene;
                return;
            L_case1:
                if (mem->age == 8) {
                    Gp_SpawnEff((EFFECT_PYROKINESIS_CAST | EFFECT_SPAWN_UNLIMITED), coord, 1, 0);
                    arg0->state = 2;
                }
                return;
            L_case2:
                if (mem->age == 0x10) {
                    Gp_SpawnEff(0x80060000 | 0x10, coord, 2, 0);
                    arg0->state = 3;
                }
                return;
            }
        }
    }
L_release:
    effectKillTask(mem, arg0);
}

#include "../../shared/glow_draw_flame_band.inc.c"

#include "../../shared/glow_draw_flame_star.inc.c"

/// Draws the scorch mark the cone leaves on the floor: the unit quad
/// `D_80111E38` is scaled to `arg1` half-size, laid flat into view space with
/// `gGfxViewCoord.workm` (rotation only, translation from `GsWSMATRIX`) and
/// offset by `arg0->workm.t`, then its four corners are projected through
/// `GsWSMATRIX`. On a non-negative `gte_stflg` it queues one semi-transparent
/// `POLY_FT4` (tpage 0x28, clut 0x428C) tinted `(0x30, 0x20, 0x20)`; the frame
/// counter's low bit picks between two 0x1F-wide UV columns at v = 0x38..0x57.
/// Same 0x38 scratch block and body as `Gp_DrawEffSprite7C`.
static void func_pyrokinesis_801304C4(GfxCoord* arg0, s32 arg1)
{
    EffectQuadScratch* quadScratch;
    s32                i;
    POLY_FT4*          prim;
    s32                u;

    quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < ARRAY_SIZE(D_80111E38); i++) {
        quadScratch->vertices[i].vx = (u16)D_80111E38[i].axis0Sign * arg1;
        quadScratch->vertices[i].vy = 0;
        quadScratch->vertices[i].vz = (u16)D_80111E38[i].axis1Sign * arg1;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&quadScratch->vertices[i]);
        gte_rtv0();
        gte_stsv(&quadScratch->vertices[i]);
        quadScratch->vertices[i].vx += arg0->workm.t[0];
        quadScratch->vertices[i].vy += arg0->workm.t[1];
        quadScratch->vertices[i].vz += arg0->workm.t[2];
    }

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    gte_stsxy(&quadScratch->screenCorners[0]);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&quadScratch->screenCorners[1], &quadScratch->screenCorners[2], &quadScratch->screenCorners[3]);
    gte_stflg(&quadScratch->projectionFlags);
    if (quadScratch->projectionFlags >= 0) {
        gte_stszotz(&quadScratch->depth);
        quadScratch->depth++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        setRGB0(prim, 0x30, 0x20, 0x20);
        prim->tpage = 0x28;
        prim->clut  = 0x428C;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v0    = 0x38;
        prim->u0    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v1    = 0x38;
        prim->u1    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v2    = 0x57;
        prim->u2    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v3    = 0x57;
        prim->u3    = u;
        prim->x0    = quadScratch->screenCorners[0].vx;
        prim->y0    = quadScratch->screenCorners[0].vy;
        prim->x1    = quadScratch->screenCorners[1].vx;
        prim->y1    = quadScratch->screenCorners[1].vy;
        prim->x2    = quadScratch->screenCorners[2].vx;
        prim->y2    = quadScratch->screenCorners[2].vy;
        prim->x3    = quadScratch->screenCorners[3].vx;
        prim->y3    = quadScratch->screenCorners[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadScratch);
}

/// Packed additive flame texture page: 4-bit indexed texels at VRAM X=576 words, Y=0 scanlines.
#define SPRITE_QUAD_TEXTURE_PAGE getTPage(0, GPU_BLEND_ADD, 576, 0)
/// Flame-strip palette: VRAM X=192 words, Y=266 scanlines, the rim look in `SPRITE_QUAD_RIM_CELL`.
#define SPRITE_QUAD_CLUT getClut(192, 266)
/// Texel width and horizontal stride of the two flame cells selected by age parity.
#define SPRITE_QUAD_CELL_WIDTH 56
#define SPRITE_QUAD_CELL_MASK  1
/// First flame-cell U coordinate in texels relative to the selected texture page.
///
/// The effect age's low bit selects U = 0x70..0xA7 or 0xA8..0xDF.
/// This integer constant configures the next drawer inclusion, which undefines it.
#define SPRITE_QUAD_U_BASE 0x70
/// Inclusive top texel row of both flame cells, relative to their texture page.
///
/// Signed integer constant for the next drawer inclusion; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V 0xC8
#define SPRITE_QUAD_V1    0xFF
/// Perspective-sizing multiplier for the pyrokinesis flame sprite.
///
/// Uses the cell's inclusive 55-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
#include "../../shared/sprite_quad_draw.inc.c"

void func_pyrokinesis_80130C54(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s16         flag;
    s16         temp_a1;
    s32         y;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (Gp_StateC08.effectPhase != ATTACHMENT_EFFECT_HELD) {
        flag = gRoomEffectState->peEffectControl;
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
                return;
            }
            mem->age = mem->age + 1;
            if (arg0->state == 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy    = -((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->scale      = (gRandomLcgState >> 16) & 0xFFF;
                arg0->state     = 1;
            }
            y                   = coord->coord.t[1] + mem->move.vy;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]   = y;
            actorRenderComposeCoord(coord);
            if (!(mem->age & 1)) {
                mem->index = mem->index + 1;
            }
            temp_a1 = mem->index;
            if (temp_a1 < 8) {
                if (mem->age & 1) {
                    pyroFlameDrawSprite(coord, temp_a1, 0x300, mem->scale);
                }
                return;
            }
        }
    }
    effectKillTask(mem, arg0);
}

#include "../../shared/pyro_flame_draw_sprite.inc.c"

void func_pyrokinesis_801311B8(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s16         flag;
    s32         scale;
    s32         angle;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (Gp_StateC08.effectPhase != ATTACHMENT_EFFECT_HELD) {
        flag = gRoomEffectState->peEffectControl;
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
                return;
            }
            if (arg0->state == 0) {
                gfxRotMatrixZ(&coord->coord, arg0->spawnArg1.value, GRAPHICS_ROTATION_COMPOSE);
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                mem->scale          = 0x80;
                mem->angle          = 0x100;
                arg0->state         = 1;
            }
            actorRenderComposeCoord(coord);
            glowDrawFlameRing(coord, mem->angle, 0x100, mem->scale);
            angle      = (u16)mem->angle;
            scale      = (u16)mem->scale;
            angle     += 0x80;
            scale     -= 8;
            mem->scale = scale;
            mem->angle = angle;
            if ((s16)scale >= 9) {
                return;
            }
        }
    }
    effectKillTask(mem, arg0);
}

#include "../../shared/glow_draw_flame_ring.inc.c"

#define JET_CONE_CLUT         0x4282
#define JET_CONE_RIM_SHORT    0x200
#define JET_CONE_RIM_LONG     0x480
#define JET_CONE_FRAME_JITTER D_pyrokinesis_80131DFC
#include "../../shared/jet_cone_draw.inc.c"

void func_pyrokinesis_80131CE4(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s16         flag;
    s32         scale;
    s32         angle;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (Gp_StateC08.effectPhase != ATTACHMENT_EFFECT_HELD) {
        flag = gRoomEffectState->peEffectControl;
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
                return;
            }
            mem->age = mem->age + 1;
            if (arg0->state == 0) {
                mem->scale  = 0xC0;
                mem->angle  = 0x100;
                arg0->state = 1;
            }
            actorRenderComposeCoord(coord);
            glowDrawFlameBand(arg0->extra.coordBody->coord, mem->angle, mem->scale);
            angle      = (u16)mem->angle;
            scale      = (u16)mem->scale;
            angle     += 0x40;
            scale     -= 0x10;
            mem->scale = scale;
            mem->angle = angle;
            if ((s16)scale >= 0x10) {
                return;
            }
        }
    }
    effectKillTask(mem, arg0);
}
