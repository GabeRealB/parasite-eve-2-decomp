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

static void _pyrokinesisDrawGroundGlow(const GfxCoord* groundCoord, s32 halfSize);

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
            padScriptSpawnVariableMotorRamp((s16)(mem->index * 2 + 8), 0xFF, 8);
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
            worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &work->damageBody);
            work->contacts[0].flags         = WORLD_COLLISION_CONTACT_LAST;
            work->gridBody.coord            = coord;
            work->gridBody.context.contacts = work->contacts;
            work->gridBody.key              = 0;
            work->damageBody.flags         |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->gridBody.radius           = (s16)((u16)mem->angle << 16 >> 19);
            work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_GRID_ONLY, &work->gridBody);
            work->gridBody.flags = (work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED)) | (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED);
            effectSpawn(EFFECT_PYROKINESIS_LAUNCH_CONE, coord, 0, NULL);
            rgb[0] = 0xFF;
            rgb[1] = 0x7F;
            rgb[2] = 0x3F;
            effectDrawScreenTint(rgb, GPU_BLEND_ADD);
            arg0->state = 1;
            spriteQuadDraw(coord, mem->age, mem->angle, mem->period);
            glowDrawFlameDisc(coord, mem->angle, (s16)((u16)mem->scale << 16 >> 17));
            if (worldCollisionCountContactsByKind(work->damageBody.context.contacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                worldCollisionUnlinkBody(&work->damageBody);
                radius     = (mem->index << 9) + 0x380;
                mem->angle = radius;
                for (i = 0; i < 0x556; i += 0x2AA) {
                    spawned = effectSpawn(EFFECT_PYROKINESIS_FLAME_RING, coord, i, NULL);
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
            if (worldCollisionFindContactIndex(work->gridBody.context.contacts, WORLD_COLLISION_CONTACT_GRID) != 0) {
                worldCollisionUnlinkBody(&work->gridBody);
                arg0->state = 2;
                return;
            }
            worldCollisionClearContacts(work->contacts);
            return;
        case 1:
            if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || ((peEffectControl = gRoomEffectState->peEffectControl), peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                worldCollisionUnlinkBody(&work->damageBody);
                worldCollisionUnlinkBody(&work->gridBody);
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
            glowDrawFlameDisc(coord, mem->angle, (s16)((u16)mem->scale << 16 >> 17));
            if (arg0->spawnArg1.value != 0) {
                _jetConeDraw(coord, mem->age, mem->angle, 0);
                _jetConeDraw(coord, mem->age, mem->angle, 1);
            }
            if (mem->age < 0x1E) {
                spawned = effectSpawn(EFFECT_PYROKINESIS_FLAME_PUFF, coord, 0, NULL);
                if (spawned != NULL) {
                    taskReparent(arg0, spawned->task);
                }
            }
            if (gRoomEffectState->groundTraceEnabled != 0) {
                if (worldCollisionProjectGroundCoord(coord, &ground) == 1) {
                    _pyrokinesisDrawGroundGlow(&ground, mem->angle);
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
            if (worldCollisionCountContactsByKind(work->damageBody.context.contacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                worldCollisionUnlinkBody(&work->damageBody);
                for (i = 0; i < 0x556; i += 0x2AA) {
                    spawned = effectSpawn(EFFECT_PYROKINESIS_FLAME_RING, coord, i, NULL);
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
            if (worldCollisionFindContactIndex(work->gridBody.context.contacts, WORLD_COLLISION_CONTACT_GRID) != 0) {
                worldCollisionUnlinkBody(&work->gridBody);
                arg0->state = 2;
                return;
            }
            tick = mem->age;
            if (tick * 6 > Gp_AttachParams[ATTACHMENT_INDEX_PYROKINESIS][mem->index].area.extent) {
                worldCollisionUnlinkBody(&work->damageBody);
                worldCollisionUnlinkBody(&work->gridBody);
                arg0->state = 2;
                return;
            }
            if (tick < 0x1F) {
                worldCollisionClearContacts(work->contacts);
                return;
            }
            worldCollisionUnlinkBody(&work->damageBody);
            worldCollisionUnlinkBody(&work->gridBody);
            effectKillTask(mem, arg0);
            return;
        case 2:
            if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || ((peEffectControl = gRoomEffectState->peEffectControl), peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                worldCollisionUnlinkBody(&work->damageBody);
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
            glowDrawFlameDisc(coord, mem->angle, (s16)((u16)mem->scale << 16 >> 17));
            if (mem->angle >= 0x81) {
                spawned = effectSpawn(EFFECT_PYROKINESIS_FLAME_PUFF, coord, 0, NULL);
                if (spawned != NULL) {
                    taskReparent(arg0, spawned->task);
                }
            }
            if (worldCollisionCountContactsByKind(work->damageBody.context.contacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                worldCollisionUnlinkBody(&work->damageBody);
                for (i = 0; i < 0x556; i += 0x2AA) {
                    spawned = effectSpawn(EFFECT_PYROKINESIS_FLAME_RING, coord, i, NULL);
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
                worldCollisionUnlinkBody(&work->damageBody);
                effectKillTask(mem, arg0);
                return;
            }
            worldCollisionClearContacts(work->contacts);
            return;
        case 3:
            if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || ((peEffectControl = gRoomEffectState->peEffectControl), peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                worldCollisionUnlinkBody(&work->gridBody);
                effectKillTask(mem, arg0);
                return;
            }
            if (peEffectControl != 0) {
                mem->age = mem->age - 1;
                return;
            }
            actorRenderComposeCoord(coord);
            spriteQuadDraw(coord, mem->age, mem->angle, mem->period);
            glowDrawFlameDisc(coord, mem->angle, (s16)((u16)mem->scale << 16 >> 17));
            glowDrawFlameDisc(coord, (s16)((u16)mem->angle * 2),
                              (s16)((u16)mem->scale << 16 >> 17));
            mem->angle = mem->angle + 0x40;
            if (mem->angle > ((mem->index << 9) + 0x580)) {
                worldCollisionUnlinkBody(&work->gridBody);
                effectKillTask(mem, arg0);
                return;
            }
            return;
        case 4:
            if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || ((peEffectControl = gRoomEffectState->peEffectControl), peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                worldCollisionUnlinkBody(&work->gridBody);
                effectKillTask(mem, arg0);
                return;
            }
            if (peEffectControl != 0) {
                mem->age = mem->age - 1;
                return;
            }
            actorRenderComposeCoord(coord);
            spriteQuadDraw(coord, mem->age, mem->angle, mem->period);
            glowDrawFlameDisc(coord, mem->angle, (s16)((u16)mem->scale << 16 >> 17));
            glowDrawFlameDisc(coord, (s16)((u16)mem->angle * 2),
                              (s16)((u16)mem->scale << 16 >> 17));
            mem->angle = mem->angle + 0x40;
            if (mem->angle > ((mem->index << 9) + 0x580)) {
                if (mem->scale >= 9) {
                    mem->scale = mem->scale - 8;
                    return;
                }
                worldCollisionUnlinkBody(&work->gridBody);
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
                switch (arg0->state) {
                    case 0:
                        effectSpawn((EFFECT_PYROKINESIS_CAST | EFFECT_SPAWN_UNLIMITED), coord, 0, 0);
                        arg0->state = scene;
                        return;
                    case 1:
                        if (mem->age == 8) {
                            effectSpawn((EFFECT_PYROKINESIS_CAST | EFFECT_SPAWN_UNLIMITED), coord, 1, 0);
                            arg0->state = 2;
                        }
                        return;
                    case 2:
                        if (mem->age == 0x10) {
                            effectSpawn(0x80060000 | 0x10, coord, 2, 0);
                            arg0->state = 3;
                        }
                        return;
                    case 3:
                        break;
                    default:
                        return;
                }
            }
        }
    }
    effectKillTask(mem, arg0);
}

#include "../../shared/glow_draw_flame_band.inc.c"

#include "../../shared/glow_draw_flame_star.inc.c"

/// Projects the ground glow's four staged corners, retaining the final GTE FLAG.
///
/// Borrows a live `EffectQuadScratch`; the caller has set the translation
/// matrix. Saves corner 0 before RTPT replaces the screen FIFO, then leaves
/// corner 3's depth in SZ3 for the caller to capture before another transform.
static inline void _pyrokinesisProjectGroundGlow(EffectQuadScratch* quadScratch)
{
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    gte_stsxy(&quadScratch->screenCorners[0]);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&quadScratch->screenCorners[1], &quadScratch->screenCorners[2], &quadScratch->screenCorners[3]);
    gte_stflg(&quadScratch->projectionFlags);
}

/// Queues the alternating additive ground glow beneath a travelling flame.
///
/// Borrows a ground-hit coordinate with a composed translation. `halfSize`
/// is the square's half-side in coordinate units before view-frame rotation;
/// staged corners and translated positions narrow to signed 16 bits. The two
/// 32-by-32 texture cells alternate with the display animation frame. Requires
/// initialized GTE projection, a word-aligned scratch stack with room for one
/// `EffectQuadScratch`, and arena space for one `POLY_FT4`. A negative final
/// GTE FLAG rejects the quad; scratch storage is released on either path.
static void _pyrokinesisDrawGroundGlow(const GfxCoord* groundCoord, s32 halfSize)
{
    enum {
        PYROKINESIS_GROUND_GLOW_TEXTURE_DEPTH_4BIT = 0,
        PYROKINESIS_GROUND_GLOW_CELL_SIZE          = 32,
        PYROKINESIS_GROUND_GLOW_LEFT_U             = 192,
        PYROKINESIS_GROUND_GLOW_TOP_V              = 56,
        PYROKINESIS_GROUND_GLOW_UV_SPAN            = PYROKINESIS_GROUND_GLOW_CELL_SIZE - 1,
    };

    EffectQuadScratch* quadScratch;
    s32                cornerIndex;
    POLY_FT4*          quad;
    s32                textureU;

    quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    // Rotate the ground-plane offsets, then centre them on the composed hit.
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(D_80111E38); cornerIndex++) {
        quadScratch->vertices[cornerIndex].vx = (u16)D_80111E38[cornerIndex].axis0Sign * halfSize;
        quadScratch->vertices[cornerIndex].vy = 0;
        quadScratch->vertices[cornerIndex].vz = (u16)D_80111E38[cornerIndex].axis1Sign * halfSize;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&quadScratch->vertices[cornerIndex]);
        gte_rtv0();
        gte_stsv(&quadScratch->vertices[cornerIndex]);
        quadScratch->vertices[cornerIndex].vx += groundCoord->workm.t[0];
        quadScratch->vertices[cornerIndex].vy += groundCoord->workm.t[1];
        quadScratch->vertices[cornerIndex].vz += groundCoord->workm.t[2];
    }

    _pyrokinesisProjectGroundGlow(quadScratch);
    if (quadScratch->projectionFlags >= 0) {
        gte_stszotz(&quadScratch->depth);
        quadScratch->depth++;
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setRGB0(quad, 0x30, 0x20, 0x20);
        quad->tpage = getTPage(PYROKINESIS_GROUND_GLOW_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, 512, 0);
        quad->clut  = getClut(192, 266);
        textureU    = (gDisplayState.animFrame & 1) * PYROKINESIS_GROUND_GLOW_CELL_SIZE + PYROKINESIS_GROUND_GLOW_LEFT_U;
        quad->v0    = PYROKINESIS_GROUND_GLOW_TOP_V;
        quad->u0    = textureU;
        textureU    = (gDisplayState.animFrame & 1) * PYROKINESIS_GROUND_GLOW_CELL_SIZE + PYROKINESIS_GROUND_GLOW_LEFT_U + PYROKINESIS_GROUND_GLOW_UV_SPAN;
        quad->v1    = PYROKINESIS_GROUND_GLOW_TOP_V;
        quad->u1    = textureU;
        textureU    = (gDisplayState.animFrame & 1) * PYROKINESIS_GROUND_GLOW_CELL_SIZE + PYROKINESIS_GROUND_GLOW_LEFT_U;
        quad->v2    = PYROKINESIS_GROUND_GLOW_TOP_V + PYROKINESIS_GROUND_GLOW_UV_SPAN;
        quad->u2    = textureU;
        textureU    = (gDisplayState.animFrame & 1) * PYROKINESIS_GROUND_GLOW_CELL_SIZE + PYROKINESIS_GROUND_GLOW_LEFT_U + PYROKINESIS_GROUND_GLOW_UV_SPAN;
        quad->v3    = PYROKINESIS_GROUND_GLOW_TOP_V + PYROKINESIS_GROUND_GLOW_UV_SPAN;
        quad->u3    = textureU;
        quad->x0    = quadScratch->screenCorners[0].vx;
        quad->y0    = quadScratch->screenCorners[0].vy;
        quad->x1    = quadScratch->screenCorners[1].vx;
        quad->y1    = quadScratch->screenCorners[1].vy;
        quad->x2    = quadScratch->screenCorners[2].vx;
        quad->y2    = quadScratch->screenCorners[2].vy;
        quad->x3    = quadScratch->screenCorners[3].vx;
        quad->y3    = quadScratch->screenCorners[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
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

void pyrokinesisFlamePuffTask(Task* task)
{
    enum {
        PYROKINESIS_FLAME_PUFF_INITIALIZE,
        PYROKINESIS_FLAME_PUFF_ACTIVE,
        PYROKINESIS_FLAME_PUFF_RISE_SPEED_COUNT = 32,
        PYROKINESIS_FLAME_PUFF_SIZE_FACTOR      = 768,
    };

    EffectWork* work;
    GfxCoord*   coord;
    s16         peEffectControl;
    s16         textureFrame;
    s32         nextY;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_StateC08.effectPhase != ATTACHMENT_EFFECT_HELD) {
        peEffectControl = gRoomEffectState->peEffectControl;
        if (peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                return;
            }
            work->age++;
            if (task->state == PYROKINESIS_FLAME_PUFF_INITIALIZE) {
                // Choose a fixed rise velocity and screen rotation once per puff.
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vy   = -((gRandomLcgState >> 16) & (PYROKINESIS_FLAME_PUFF_RISE_SPEED_COUNT - 1));
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->scale     = (gRandomLcgState >> 16) & (PYRO_FLAME_FULL_TURN - 1);
                task->state     = PYROKINESIS_FLAME_PUFF_ACTIVE;
            }
            nextY               = coord->coord.t[1] + work->move.vy;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]   = nextY;
            actorRenderComposeCoord(coord);
            // Each cell lasts two running ticks, but only its odd tick is drawn.
            if (!(work->age & 1)) {
                work->index++;
            }
            textureFrame = work->index;
            if (textureFrame < PYRO_FLAME_FRAME_COUNT) {
                if (work->age & 1) {
                    _pyroFlameDrawSprite(coord, textureFrame, PYROKINESIS_FLAME_PUFF_SIZE_FACTOR, work->scale);
                }
                return;
            }
        }
    }
    effectKillTask(work, task);
}

#include "../../shared/pyro_flame_draw_sprite.inc.c"

void pyrokinesisFlameRingTask(Task* task)
{
    enum {
        PYROKINESIS_FLAME_RING_INITIALIZE,
        PYROKINESIS_FLAME_RING_ACTIVE,
        PYROKINESIS_FLAME_RING_INITIAL_INTENSITY = 128,
        PYROKINESIS_FLAME_RING_INITIAL_RADIUS    = 256,
        PYROKINESIS_FLAME_RING_WIDTH             = 256,
        PYROKINESIS_FLAME_RING_RADIUS_STEP       = 128,
        PYROKINESIS_FLAME_RING_INTENSITY_STEP    = 8,
        PYROKINESIS_FLAME_RING_MIN_INTENSITY     = 9,
    };

    EffectWork* work;
    GfxCoord*   coord;
    s16         peEffectControl;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_StateC08.effectPhase != ATTACHMENT_EFFECT_HELD) {
        peEffectControl = gRoomEffectState->peEffectControl;
        if (peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                return;
            }
            if (task->state == PYROKINESIS_FLAME_RING_INITIALIZE) {
                // Tilt the local XZ ring by the spawn angle once; expansion keeps that plane.
                gfxRotMatrixZ(&coord->coord, task->spawnArg1.value, GRAPHICS_ROTATION_COMPOSE);
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                work->scale         = PYROKINESIS_FLAME_RING_INITIAL_INTENSITY;
                work->angle         = PYROKINESIS_FLAME_RING_INITIAL_RADIUS;
                task->state         = PYROKINESIS_FLAME_RING_ACTIVE;
            }
            actorRenderComposeCoord(coord);
            glowDrawFlameRing(coord, work->angle, PYROKINESIS_FLAME_RING_WIDTH, work->scale);
            work->angle += PYROKINESIS_FLAME_RING_RADIUS_STEP;
            work->scale -= PYROKINESIS_FLAME_RING_INTENSITY_STEP;
            if (work->scale >= PYROKINESIS_FLAME_RING_MIN_INTENSITY) {
                return;
            }
        }
    }
    effectKillTask(work, task);
}

#include "../../shared/glow_draw_flame_ring.inc.c"

#define JET_CONE_CLUT         0x4282
#define JET_CONE_RIM_SHORT    0x200
#define JET_CONE_RIM_LONG     0x480
#define JET_CONE_FRAME_JITTER D_pyrokinesis_80131DFC
#include "../../shared/jet_cone_draw.inc.c"

void pyrokinesisLaunchConeTask(Task* task)
{
    enum {
        PYROKINESIS_LAUNCH_CONE_INITIALIZE,
        PYROKINESIS_LAUNCH_CONE_ACTIVE,
        PYROKINESIS_LAUNCH_CONE_INITIAL_INTENSITY = 192,
        PYROKINESIS_LAUNCH_CONE_INITIAL_RADIUS    = 256,
        PYROKINESIS_LAUNCH_CONE_RADIUS_STEP       = 64,
        PYROKINESIS_LAUNCH_CONE_INTENSITY_STEP    = 16,
    };

    EffectWork* work;
    GfxCoord*   coord;
    s16         peEffectControl;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_StateC08.effectPhase != ATTACHMENT_EFFECT_HELD) {
        peEffectControl = gRoomEffectState->peEffectControl;
        if (peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                return;
            }
            work->age++;
            if (task->state == PYROKINESIS_LAUNCH_CONE_INITIALIZE) {
                work->scale = PYROKINESIS_LAUNCH_CONE_INITIAL_INTENSITY;
                work->angle = PYROKINESIS_LAUNCH_CONE_INITIAL_RADIUS;
                task->state = PYROKINESIS_LAUNCH_CONE_ACTIVE;
            }
            actorRenderComposeCoord(coord);
            // Draw the current ring pair before advancing its radius and intensity.
            glowDrawFlameCone(task->extra.coordBody->coord, work->angle, work->scale);
            work->angle += PYROKINESIS_LAUNCH_CONE_RADIUS_STEP;
            work->scale -= PYROKINESIS_LAUNCH_CONE_INTENSITY_STEP;
            if (work->scale >= PYROKINESIS_LAUNCH_CONE_INTENSITY_STEP) {
                return;
            }
        }
    }
    effectKillTask(work, task);
}
