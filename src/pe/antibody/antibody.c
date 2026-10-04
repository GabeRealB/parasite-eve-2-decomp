#include "pe/antibody.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/effects.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/glow_draw.h"
/// Signed texture-frame counter for both antibody sprite instances.
#define SPRITE_QUAD_FRAME_T s16
#include "../../shared/sprite_quad.h"

/// Visual tuning of the antibody cast for one Parasite Energy level.
///
/// The cast is a set of rings, an arc and a fan of glow wedges that grow
/// around the caster while motes spawned on a circle close on its centre. The
/// cast task and the mote task both select their row with the level digit of
/// the attachment id, less one, which they keep in `EffectWork::index`.
///
/// `scaleLimit` and `scaleStep` are in the units of the cast's
/// `EffectWork::scale`, which is at once the brightness of the drawing (its
/// low byte is the red and green channel) and, multiplied up, the radius of
/// each ring. The mote sizes are the size argument of the mote's sprite quad.
typedef struct {
    s16 wedgeCount;         // Glow wedges fanned around the ring; the yaw table holds 16
    s16 scaleLimit;         // Scale that ends the growth; the fading rings are drawn at this scale
    s16 scaleStep;          // Scale gained per frame of growth; at level 3 the fading arc and wedges keep gaining it
    s16 moteSpawnSize;      // Size a mote is spawned with
    s16 moteRerollSizeBase; // Low end of the 0x200-wide range a mote's size is re-rolled in; doubled for the larger sprite
    s16 moteSpawnRadius;    // Radius of the horizontal circle each burst of four motes is spawned on
    s16 moteSpawnInterval;  // Frames between mote bursts during the first 0x14 frames of the cast
} _AntibodyLevelTuning;
STATIC_ASSERT_SIZEOF(_AntibodyLevelTuning, 0xE);

/// Per-level tuning for the antibody motes, one row per PE level 1-3,
/// weakest first.
static _AntibodyLevelTuning D_antibody_80130BD4[] = {
    { 0x0008, 0x0090, 0x0005, 0x0200, 0x0080, 0x0600, 0x0008 },
    { 0x000C, 0x00C0, 0x0006, 0x0300, 0x0100, 0x0700, 0x0006 },
    { 0x0010, 0x00F0, 0x0007, 0x0400, 0x0180, 0x0800, 0x0004 },
};

/// The `SndEvt_EnqueueType6` id for each `D_antibody_80130BD4` row, played
/// once when `func_antibody_8012EF34` seeds the cast.
static s32 D_antibody_80130C00[] = { 0xE0290001, 0xE02C0001, 0xE02F0001 };

/// Antibody mote instance of `spriteQuadDraw`.
///
/// `pos` is the effect coordinate, `frame` selects the mote cell, `size` is
/// the perspective numerator and `angle` is the spin in 4096 units per turn.
static void spriteQuadDrawMote(const GfxCoord* pos, s16 frame, s16 size, s16 angle);
static void func_antibody_80130428(GfxCoord* arg0, s16 arg1, s16 arg2);

/// Sixteen wedge yaws, refilled once per cast by `func_antibody_8012EF34`.
/// Entry `i` is `i * (0x1000 / wedgeCount)` plus a 9-bit `gRandomLcgState` draw;
/// states 1 and 2 pass one yaw per frame to `glowDrawWedge`.
static s16 D_antibody_80130C0C[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/// Runs one frame of an antibody cast. `Task::spawnArg2` is the `EffectWork`
/// block and `Task::extra` reaches the effect coordinate. Cancel
/// (`Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD` or `gRoomEffectState->peEffectControl >= 4`) releases the
/// work block.
///
/// State 0 parents the coordinate with an identity rotation at the origin,
/// seeds `index` from the combo counter, refills `D_antibody_80130C0C`
/// with one yaw per wedge, and plays the row's cue. State 1 grows the draw
/// parameter `scale` by the row's `scaleStep`, draws three rings plus the
/// `wedgeCount` wedges (and an arc above the weakest row), and for the first
/// 0x14 ticks spawns four `0x600F5` motes on a `moteSpawnRadius`-radius circle every
/// `moteSpawnInterval` frames, reparenting each onto this task. Once `scale` passes
/// the row's `scaleLimit` cap it spawns the `0x800600AC` burst, latches
/// `period` and moves to state 2, which shrinks `scale` by 0x10 a frame
/// and redraws at the capped radius until it drops below 0x11.

void func_antibody_8012EF34(Task* arg0)
{
    EffectWork*      mem;
    GfxCoord*        coord;
    AttachmentState* state;
    s32              i;
    u8               rgb[3];

    state = &Gp_StateC08;
    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if ((state->effectPhase != ATTACHMENT_EFFECT_HELD) && (gRoomEffectState->peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        mem->age = mem->age + 1;
        switch (arg0->state) {
            case 0: {
                GfxRotationWords* rot;

                rot                 = (GfxRotationWords*)&coord->coord;
                coord->parent       = mem->parent;
                rot->m00M01         = ONE;
                rot->m02M10         = 0;
                rot->m11M12         = ONE;
                rot->m20M21         = 0;
                rot->m22            = ONE;
                coord->coord.t[2]   = 0;
                coord->coord.t[1]   = 0;
                coord->coord.t[0]   = 0;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                gRoomEffectState->peFxFlags &= (u16)~ROOM_EFFECT_PE_ANTIBODY_AURA;
                state->flags                |= ATTACHMENT_FLAG_APPLY_STATS;
                arg0->state                  = 1;
                mem->index                   = (Gp_StateC08.attachId % 10) - 1;
                i                            = 0;
                if (D_antibody_80130BD4[mem->index].wedgeCount > 0) {
                    do {
                        s16* dst;
                        s32  lo;
                        s32  rng;

                        dst             = D_antibody_80130C0C;
                        lo              = i * (0x1000 / D_antibody_80130BD4[mem->index].wedgeCount);
                        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        dst[i]          = lo + (((u32)rng >> 16) & 0x1FF);
                        gRandomLcgState = rng;
                    } while (++i < D_antibody_80130BD4[mem->index].wedgeCount);
                }
                {
                    s32 pan;

                    pan = (s8)worldCoordGetOriginAudioPan(coord);
                    SndEvt_EnqueueType6(D_antibody_80130C00[mem->index], pan,
                                        (s8)worldCoordGetOriginAudioDepth(coord));
                }
                return;
            }
            case 1: {
                _AntibodyLevelTuning* table;
                _AntibodyLevelTuning* t2;
                EffectWork*           eff;
                s32                   rng;
                s16                   ang;
                s16*                  p;
                s16                   count;

                table               = D_antibody_80130BD4;
                mem->scale          = mem->scale + table[mem->index].scaleStep;
                rgb[0]              = (u8)mem->scale;
                rgb[1]              = (u8)mem->scale;
                rgb[2]              = mem->scale >> 1;
                coord->coord.t[1]   = -0x400;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                Gp_DrawRing(coord, (s16)(mem->scale * 4), rgb);
                Gp_DrawRing(coord, (s16)(mem->scale * 8), rgb);
                Gp_DrawRing(coord, (s16)(mem->scale * 0xC), rgb);
                if (mem->index != 0) {
                    rgb[0] >>= 1;
                    rgb[1] >>= 1;
                    rgb[2] >>= 1;
                    Gp_DrawArc(coord, (s16)(mem->scale * 8), 0x80, rgb);
                }
                i     = 0;
                count = table[mem->index].wedgeCount;
                if (count > 0) {
                    t2 = table;
                    p  = D_antibody_80130C0C;
                    do {
                        glowDrawWedge(coord, (s16)(mem->scale * 6), *p, rgb);
                        p += 1;
                    } while (++i < t2[mem->index].wedgeCount);
                }
                coord->coord.t[1]   = 0;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                if (mem->age < 0x14) {
                    if ((mem->age % D_antibody_80130BD4[mem->index].moteSpawnInterval) == 1) {
                        i = 0;
                        do {
                            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            ang             = i + (((u32)rng >> 16) & 0x3FF);
                            gRandomLcgState = rng;
                            mem->angle      = ang;
                            mem->move.vx =
                                (D_antibody_80130BD4[mem->index].moteSpawnRadius * rsin(ang)) >> 12;
                            mem->move.vz = (D_antibody_80130BD4[mem->index].moteSpawnRadius *
                                            rcos(mem->angle)) >>
                                           12;
                            eff = Gp_SpawnEff(EFFECT_ANTIBODY_MOTE, coord, 0, &mem->move);
                            if (eff != NULL) {
                                taskReparent(arg0, eff->task);
                            }
                            i += 0x400;
                        } while (i < 0x1000);
                    }
                }
                if (mem->scale > D_antibody_80130BD4[mem->index].scaleLimit) {
                    Gp_SpawnEff((EFFECT_ANTIBODY_AURA | EFFECT_SPAWN_UNLIMITED), coord, 0, 0);
                    mem->period = mem->scale;
                    arg0->state = 2;
                }
                return;
            }
            case 2: {
                _AntibodyLevelTuning* table;
                _AntibodyLevelTuning* t2;
                s16*                  p;
                s16                   count;

                if (mem->scale < 0x11) {
                    goto release;
                }
                mem->scale          = mem->scale - 0x10;
                rgb[0]              = (u8)mem->scale;
                rgb[1]              = (u8)mem->scale;
                rgb[2]              = mem->scale >> 1;
                coord->coord.t[1]   = -0x400;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                table = D_antibody_80130BD4;
                Gp_DrawRing(coord, (s16)(table[mem->index].scaleLimit * 4), rgb);
                Gp_DrawRing(coord, (s16)(table[mem->index].scaleLimit * 8), rgb);
                Gp_DrawRing(coord, (s16)(table[mem->index].scaleLimit * 0xC), rgb);
                if (mem->index != 0) {
                    if (mem->index == 2) {
                        mem->period = mem->period + table[mem->index].scaleStep;
                    }
                    rgb[0] >>= 1;
                    rgb[1] >>= 1;
                    rgb[2] >>= 1;
                    Gp_DrawArc(coord, (s16)(mem->period * 8), 0x80, rgb);
                }
                i     = 0;
                count = D_antibody_80130BD4[mem->index].wedgeCount;
                if (count > 0) {
                    t2 = D_antibody_80130BD4;
                    p  = D_antibody_80130C0C;
                    do {
                        glowDrawWedge(coord, (s16)(mem->period * 6), *p, rgb);
                        p += 1;
                    } while (++i < t2[mem->index].wedgeCount);
                }
                coord->coord.t[1]   = 0;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                return;
            }
        }
        return;
    }
release:
    effectKillTask(mem, arg0);
}

/// Runs one frame of an antibody mote. State 0 re-bases the effect coordinate
/// on the `EffectWork.parent` parent with an identity rotation and the work
/// block's `pos` offset, then GPF-scales that offset by 0x100
/// (a sixteenth) into `move` as the per-frame step, and seeds
/// the intensity `index` from the combo counter, the draw parameter
/// `scale` from that row's `moteSpawnSize` and the phase `angle` from
/// `gRandomLcgState`. State 1 walks the coordinate back down that step every frame
/// and draws with `spriteQuadDrawMote`; past tick 0x10 it parks a `-0x80`
/// Y drift in `move.vy` and moves to state 2, and one frame in sixteen it
/// jumps straight to state 3 instead. State 2 applies that Y drift and keeps
/// drawing; state 3 draws the larger `spriteQuadDraw` /
/// `func_antibody_80130428` pair. All three re-roll `scale` / `angle`
/// from the row's `moteRerollSizeBase` one frame in eight, and states 2 and 3 release the
/// effect at tick 0x15.
void func_antibody_8012F734(Task* arg0)
{
    EffectWork*       mem;
    GfxCoord*         coord;
    GfxRotationWords* rot;
    s32               rng0;
    s32               rng1a;
    s32               rng1b;
    s32               rng1c;
    s32               rng1d;
    s32               rng2a;
    s32               rng2b;
    s32               rng2c;
    s32               rng3a;
    s32               rng3b;
    s32               rng3c;
    s16               idx;

    mem                 = arg0->spawnArg2.pointer;
    coord               = arg0->extra.coordBody->coord;
    mem->age            = mem->age + 1;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    switch (arg0->state) {
        case 0:
            rot           = (GfxRotationWords*)&coord->coord;
            coord->parent = mem->parent;
            rot->m00M01   = ONE;
            rot->m02M10   = 0;
            rot->m11M12   = ONE;
            rot->m20M21   = 0;
            rot->m22      = ONE;

            coord->coord.t[0] = mem->pos.vx;
            coord->coord.t[1] = mem->pos.vy;
            coord->coord.t[2] = mem->pos.vz;

            gte_lddp(0x100);
            gte_ldsv(&mem->pos);
            gte_gpf12();
            gte_stsv(&mem->move);

            arg0->state     = 1;
            rng0            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rng0;
            idx             = Gp_StateC08.attachId % 10 - 1;
            mem->index      = idx;
            mem->scale      = D_antibody_80130BD4[idx].moteSpawnSize;
            mem->angle      = ((u32)rng0 >> 16) & 0xFFF;
            /* fallthrough */
        case 1:
            rng1a           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rng1a;
            if ((((u32)rng1a >> 16) & 7) == 0) {
                rng1b           = rng1a * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng1b;
                mem->scale =
                    D_antibody_80130BD4[mem->index].moteRerollSizeBase + (((u32)rng1b >> 16) & 0x1FF);
                rng1c           = rng1b * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng1c;
                mem->angle      = ((u32)rng1c >> 16) & 0xFFF;
            }
            coord->coord.t[0]  -= mem->move.vx;
            coord->coord.t[1]  -= mem->move.vy;
            coord->coord.t[2]  -= mem->move.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            spriteQuadDrawMote(coord, mem->age, mem->scale, mem->angle);
            if (mem->age >= 0x10) {
                mem->move.vy = -0x80;
                arg0->state  = 2;
                return;
            }
            rng1d           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rng1d;
            if ((((u32)rng1d >> 16) & 0xF) == 0) {
                arg0->state = 3;
            }
            return;
        case 2:
            rng2a           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rng2a;
            if ((((u32)rng2a >> 16) & 7) == 0) {
                rng2b           = rng2a * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng2b;
                mem->scale =
                    D_antibody_80130BD4[mem->index].moteRerollSizeBase + (((u32)rng2b >> 16) & 0x1FF);
                rng2c           = rng2b * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng2c;
                mem->angle      = ((u32)rng2c >> 16) & 0xFFF;
            }
            coord->coord.t[1]  += mem->move.vy;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            spriteQuadDrawMote(coord, mem->age, mem->scale, mem->angle);
            goto check;
        case 3:
            rng3a           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rng3a;
            if ((((u32)rng3a >> 16) & 7) == 0) {
                rng3b           = rng3a * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng3b;
                mem->scale      = D_antibody_80130BD4[mem->index].moteRerollSizeBase * 2 +
                             (((u32)rng3b >> 16) & 0x1FF);
                rng3c           = rng3b * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng3c;
                mem->angle      = ((u32)rng3c >> 16) & 0xFFF;
            }
            actorRenderComposeCoord(coord);
            spriteQuadDraw(coord, mem->age, mem->scale, mem->angle);
            func_antibody_80130428(coord, mem->age, mem->scale);
        check:
            if (mem->age >= 0x15) {
                effectKillTask(mem, arg0);
            }
            break;
    }
}

/// Defines the mote instance. The next inclusion is unbound and becomes `spriteQuadDraw`.
#define SPRITE_QUAD_FUNC spriteQuadDrawMote
/// Packed additive mote texture page: 4-bit indexed texels at VRAM X=576 words, Y=0 scanlines.
#define SPRITE_QUAD_TEXTURE_PAGE getTPage(0, GPU_BLEND_ADD, 576, 0)
/// Mote palette: VRAM X=96 words, Y=267 scanlines.
#define SPRITE_QUAD_CLUT getClut(96, 267)
/// Texel width and horizontal stride of each cell in the mote's six-cell strip.
#define SPRITE_QUAD_CELL_WIDTH 40
/// Number of cells in the mote texture row, repeated as the effect ages.
#define SPRITE_QUAD_CELLS_PER_ROW 6
/// Inclusive top texel row of the mote strip, relative to its texture page.
///
/// Signed integer constant for the next drawer inclusion; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V 0x50
#define SPRITE_QUAD_V1    0x77
/// Perspective-sizing multiplier for the antibody mote.
///
/// Uses the cell's inclusive 39-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
#include "../../shared/sprite_quad_draw.inc.c"

/// Larger antibody sprite palette: VRAM X=144 words, Y=267 scanlines.
#define SPRITE_QUAD_CLUT getClut(144, 267)
/// Texel width and horizontal stride of each cell in the larger sprite's six-cell strip.
#define SPRITE_QUAD_CELL_WIDTH 40
/// Number of cells in the larger antibody sprite's repeating texture row.
#define SPRITE_QUAD_CELLS_PER_ROW 6
/// Inclusive top texel row of the larger sprite strip, relative to its texture page.
///
/// Signed integer constant for the next drawer inclusion; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V 0x38
#define SPRITE_QUAD_V1    0x5F
/// Perspective-sizing multiplier for the larger antibody sprite.
///
/// Uses the cell's inclusive 39-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
#include "../../shared/sprite_quad_draw.inc.c"

/// Draws the antibody arc between the effect and the player as one
/// semi-transparent raw-tex `POLY_FT4` (tpage 0x28, clut 0x42C8). The effect
/// coordinate's world position and the player's second part coordinate are
/// each projected through `GsWSMATRIX` with one `RTPS`; the quad is laid
/// along the line joining the two projected points, `ratan2` of their screen
/// delta giving the spin applied at that angle and at `+ 0x400`. `arg1`
/// selects the 128-texel UV tile: u = `(arg1 & 1) * 128`, v =
/// `((arg1 & 3) >> 1) * 24 - 0x30`. `arg2` is a signed half-extent, so the
/// on-screen half-width is `arg2 * 23 / depth`. Nothing is drawn if either
/// projection sets a negative `gte_stflg`.
static void func_antibody_80130428(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    EffectStripScratch* block;
    POLY_FT4*           prim;
    GfxCoord*           player;
    s32                 u0;
    s32                 u1;
    s32                 va;
    s32                 vb;
    s16                 ang;

    player               = &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[1];
    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectStripScratch);
    block->worldStart.vx = arg0->workm.t[0];
    block->worldStart.vy = arg0->workm.t[1];
    block->worldStart.vz = arg0->workm.t[2];
    block->worldEnd.vx   = player->workm.t[0];
    block->worldEnd.vy   = player->workm.t[1];
    block->worldEnd.vz   = player->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldStart);
    gte_rtps();
    gte_stsxy(&block->screenStart);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth++;
        gte_ldv0(&block->worldEnd);
        gte_rtps();
        gte_stsxy(&block->screenEnd);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage = 0x28;
            prim->clut  = 0x42C8;
            u0          = (arg1 & 1) << 7;
            u1          = u0 + 0x7F;
            va          = ((arg1 & 3) >> 1) * 24 - 0x30;
            vb          = ((arg1 & 3) >> 1) * 24 - 0x19;
            setUV4(prim, u0, va, u1, va, u0, vb, u1, vb);
            ang                  = ratan2(block->screenEnd.vy - block->screenStart.vy, block->screenEnd.vx - block->screenStart.vx);
            block->cornerOffsetX = (((arg2 * 0x17) / block->depth) * rsin(ang)) >> 12;
            block->cornerOffsetY = (((arg2 * 0x17) / block->depth) * rcos(ang)) >> 12;
            prim->x0             = block->screenStart.vx + block->cornerOffsetX;
            prim->x3             = block->screenEnd.vx - block->cornerOffsetX;
            prim->y0             = block->screenStart.vy - block->cornerOffsetY;
            prim->y3             = block->screenEnd.vy + block->cornerOffsetY;
            block->cornerOffsetX = (((arg2 * 0x17) / block->depth) * rsin(ang + 0x400)) >> 12;
            block->cornerOffsetY = (((arg2 * 0x17) / block->depth) * rcos(ang + 0x400)) >> 12;
            prim->x1             = block->screenEnd.vx + block->cornerOffsetX;
            prim->x2             = block->screenStart.vx - block->cornerOffsetX;
            prim->y1             = block->screenEnd.vy - block->cornerOffsetY;
            prim->y2             = block->screenStart.vy + block->cornerOffsetY;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectStripScratch);
}

#include "../../shared/glow_draw_wedge.inc.c"
