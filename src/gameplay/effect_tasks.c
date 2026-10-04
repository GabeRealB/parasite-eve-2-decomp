#include "gameplay/effect_tasks.h"

#include <psyq/sys/types.h>
#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/memory.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/display.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/tmd_types.h"

/// Flag in `_EffectCriticalHitStyle::color` that adds four radial spikes, each
/// at a random angle within its own quarter turn, to the ring.
#define EFFECT_CRITICAL_HIT_STYLE_SPIKES 0x1000

/// Packs the channel weights of `_EffectCriticalHitStyle::color`, each 0 to 15.
#define EFFECT_CRITICAL_HIT_STYLE_COLOR(red, green, blue) (((red) << 8) | ((green) << 4) | (blue))

/// One look of the `EFFECT_CRITICAL_HIT` burst, selected by the effect's spawn
/// argument.
///
/// The burst is an additive ring that grows for eight frames while it fades
/// from its colour to black. Each colour channel is its weight multiplied by a
/// brightness that falls from 14 to 0. The ring's radius starts at 0x20 and is
/// in units of 256 / depth pixels at its outer edge; the inner edge sits at
/// half that distance.
typedef struct {
    u16 color;      // `EFFECT_CRITICAL_HIT_STYLE_COLOR` channel weights, with `EFFECT_CRITICAL_HIT_STYLE_SPIKES` where the burst has spikes
    u16 radiusStep; // amount the ring's radius grows each frame
} _EffectCriticalHitStyle;
STATIC_ASSERT_SIZEOF(_EffectCriticalHitStyle, 4);

/// One frame of the `EFFECT_IMPACT_FLASH` animation: a square texture cell with
/// the texture page and palette it is drawn from.
///
/// The flash shows one frame per tick, and consecutive frames can come from
/// different texture pages. `u` and `v` are texels within the page at `tpageX`;
/// the record holds no page Y, and its pages sit on the top row of VRAM. The
/// page and palette coordinates are unencoded VRAM coordinates for `getTPage`
/// and `getClut`.
///
/// `uvSpan` is the distance between the cell's outermost texels, not its side,
/// so a cell ends at `u + uvSpan` and `v + uvSpan`, which must stay within the
/// page (0..255). It is also the distance from the drawn quad's centre to each
/// corner, in the units the effect's scale and the perspective divide act on,
/// so a larger cell is drawn larger.
///
/// Texture coordinates are eight bits wide when drawn. The high byte of `u` and
/// `v` is zero in every frame and has no proven role of its own.
typedef struct {
    u16 uvSpan; // Texels from the cell's first column or row to its last: its side minus one
    u16 u;      // Left texture column in texels (0..255)
    u16 v;      // Top texture row in texels (0..255)
    u16 clutX;  // Palette X in VRAM words, aligned to 16 words
    u16 clutY;  // Palette Y in VRAM scanlines
    u16 tpageX; // Texture page X in VRAM words, aligned to 64 words
} _EffectImpactFlashFrame;
STATIC_ASSERT_SIZEOF(_EffectImpactFlashFrame, 0xC);

/// Scratch-stack workspace for projecting one `EFFECT_PIXEL_SPARK` onto its tile.
///
/// The spark's task stages its coordinate's world position, narrowed to signed
/// 16-bit coordinate units, and one perspective transform through the
/// world-to-screen matrix supplies the screen position, the GTE status and the
/// depth. A rejected projection queues no tile and leaves `depth` unwritten.
///
/// Reserve one complete, word-aligned block on the scratch stack and release
/// it in reverse order after drawing. The block is not cleared; the SVECTOR's
/// unused fourth halfword has no established value. No pointer into this
/// workspace may survive its release.
typedef struct {
    SVECTOR worldPoint;      // Spark position in world coordinates, narrowed to s16
    s32     depth;           // SZ3 / 4 plus one, used for ordering-table placement and blend setup
    s32     projectionFlags; // GTE FLAG word; bit 31 makes it negative and rejects the projection
    DVECTOR screenPoint;     // Projected position in pixels, the tile's top-left corner; one GTE word store fills both halves
} _EffectPixelSparkScratch;
STATIC_ASSERT_SIZEOF(_EffectPixelSparkScratch, 0x14);

extern _EffectCriticalHitStyle D_8011291C[];

extern _EffectImpactFlashFrame Gp_EffSprRecs[];

extern u32 D_80111EF4[1];

extern SVECTOR D_80111EF8[6];

extern SVECTOR D_80111F28[4];

extern u32 D_80111F48[32];

extern u32 D_80112010[1];

extern SVECTOR D_80112014[6];

extern SVECTOR D_80112044[4];

extern u32 D_80112064[32];

extern u32 D_8011212C[1];

extern SVECTOR D_80112130[6];

extern SVECTOR D_80112160[4];

extern u32 D_80112180[32];

extern u32 D_80112248[1];

extern SVECTOR D_8011224C[6];

extern SVECTOR D_8011227C[4];

extern u32 D_8011229C[32];

extern u32 D_80112364[1];

extern SVECTOR D_80112368[8];

extern SVECTOR D_801123A8[10];

extern u32 D_801123F8[48];

extern SVECTOR D_801124DC[34];

extern SVECTOR D_801125EC[34];

extern SVECTOR D_801126FC[34];

extern SVECTOR D_8011280C[34];

static void Gp_DrawEffSprite6C();

static void Gp_DrawEffSprite3B(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3);

static void Gp_DrawEffShard(GfxCoord* arg0, s16 arg1, s16 arg2, u16 arg3);

EffectUnitQuadCorner D_80111E38[4] = {
    { -1, 1 },
    { 1, 1 },
    { -1, -1 },
    { 1, -1 },
};
// Preserve the initialized-data placement of this shared read-only table.
const EffectSpriteTextureFrame gEffectSpriteAtlasFrames[12] __attribute__((section(".data"))) = {
    { 0, 0, 0, 0, 112, 265 },
    { EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, 0, 0, 128, 265 },
    { 2 * EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, 0, 0, 144, 265 },
    { 3 * EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, 0, 0, 160, 265 },
    { 4 * EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, 0, 0, 176, 265 },
    { 5 * EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, 0, 0, 192, 265 },
    { 0, 0, EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, 208, 265 },
    { EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, 224, 265 },
    { 2 * EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, 240, 265 },
    { 3 * EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, 256, 265 },
    { 4 * EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, 272, 265 },
    { 5 * EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, EFFECT_SPRITE_ATLAS_CELL_SIZE, 0, 288, 265 },
};
u16 Gp_QuadClutX[6] = {
    32,
    48,
    192,
    208,
    224,
    240,
};
u16 D_80111EB4[6] = {
    80,
    176,
    256,
    272,
    288,
    304,
};
u16 Gp_FadeQuadColors[8] = {
    0x2CCC,
    4924,
    9155,
    9068,
    0x2C63,
    7267,
    9020,
    7222,
};

void Gp_EffCtlTask2B(Task* arg0)
{
    EffectWork*                    mem;
    GfxCoord*                      coord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          slot;
    s32                            temp;
    s32                            idx;
    s32                            t2;
    s32                            rng;
    s32                            count;

    mem       = arg0->spawnArg2.pointer;
    coord     = arg0->extra.coordBody->coord;
    lightSlot = gWorldCoordTransientPointLights;
    slot      = &lightSlot->light;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        mem->age++;
        switch (arg0->state) {
            case 0:
                temp                                               = arg0->spawnArg1.halves.high;
                mem->index                                         = temp;
                arg0->spawnArg1.value                              = (u8)arg0->spawnArg1.value;
                slot->head.transform.coord.coord.t[0]              = coord->coord.t[0];
                slot->head.transform.coord.coord.t[1]              = coord->coord.t[1];
                t2                                                 = coord->coord.t[2];
                lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
                slot->head.color.r                                 = 0xC00;
                slot->head.color.g                                 = 0xC00;
                slot->head.color.b                                 = 0xC00;
                slot->inner                                        = 0xFA0;
                slot->outer                                        = 0x12C0;
                slot->head.transform.coord.coord.t[2]              = t2;
                coord->parent                                      = mem->parent;
                coord->coord.t[0]                                  = D_801124DC[arg0->spawnArg1.value].vx;
                coord->coord.t[1]                                  = D_801124DC[arg0->spawnArg1.value].vy;
                coord->coord.t[2]                                  = D_801124DC[arg0->spawnArg1.value].vz;
                coord->composeStamp                                = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                switch (arg0->spawnArg1.value) {
                    case 1:
                    default:
                        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        gRandomLcgState = rng;
                        Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, (((u32)rng >> 16) & 0x1FF) | 0x200, 0);
                        idx         = arg0->spawnArg1.value;
                        arg0->state = 1;
                        Gp_SpawnEff(EFFECT_BULLET_CASING, coord, idx, &D_801125EC[idx]);
                        mem->scale            = 4;
                        lightSlot->framesLeft = 4;
                        break;
                    case 2:
                    case 3:
                        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        gRandomLcgState = rng;
                        Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, (((u32)rng >> 16) & 0x1FF) + 0x300, 0);
                        idx         = arg0->spawnArg1.value;
                        arg0->state = 1;
                        Gp_SpawnEff(EFFECT_BULLET_CASING, coord, idx, &D_801125EC[idx]);
                        mem->scale            = 4;
                        lightSlot->framesLeft = 4;
                        break;
                    case 30:
                    case 31:
                    case 32:
                        arg0->state     = 2;
                        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        gRandomLcgState = rng;
                        Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, (((u32)rng >> 16) & 0x1FF) + 0x300, 0);
                        idx = arg0->spawnArg1.value;
                        Gp_SpawnEff(EFFECT_BULLET_CASING, coord, idx, &D_801125EC[idx]);
                        mem->scale            = 2;
                        lightSlot->framesLeft = 2;
                        break;
                    case 5:
                        idx         = arg0->spawnArg1.value;
                        arg0->state = 1;
                        Gp_SpawnEff(EFFECT_P229_SHELL_CASING, coord, idx, &D_801125EC[idx]);
                        mem->scale            = 4;
                        lightSlot->framesLeft = WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE;
                        break;
                    case 33:
                        mem->index      = 1;
                        arg0->state     = 1;
                        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        gRandomLcgState = rng;
                        Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, (((u32)rng >> 16) & 0x1FF) + 0x300, 0);
                        idx = arg0->spawnArg1.value;
                        Gp_SpawnEff(EFFECT_P229_SHELL_CASING, coord, idx, &D_801125EC[idx]);
                        mem->scale            = 4;
                        lightSlot->framesLeft = WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE;
                        break;
                }
                if (mem->index == 0) {
                    gRoomEffectState->burstRequest = true;
                }
                break;
            case 1:
                rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng;
                Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, (((u32)rng >> 16) & 0x1FF) | 0x200, 0);
                arg0->state++;
                break;
        }
        if (slot->inner >= 0x191) {
            slot->inner -= 0x190;
        }
        count = mem->age;
        if (mem->scale < count) {
            effectKillTask(mem, arg0);
        }
    }
}

void Gp_EffCtlTask6A(Task* arg0)
{
    EffectWork*                    mem;
    GfxCoord*                      coord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          slot;
    s32                            t2;

    lightSlot = gWorldCoordTransientPointLights;
    mem       = arg0->spawnArg2.pointer;
    coord     = arg0->extra.coordBody->coord;
    slot      = &lightSlot->light;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        mem->age++;
        switch (arg0->state) {
            case 0:
                slot->head.transform.coord.coord.t[0]              = coord->coord.t[0];
                slot->head.transform.coord.coord.t[1]              = coord->coord.t[1];
                t2                                                 = coord->coord.t[2];
                lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
                slot->head.color.b                                 = 0xC00;
                slot->head.color.g                                 = 0xC00;
                slot->head.color.r                                 = 0xC00;
                slot->inner                                        = 0xFA0;
                slot->outer                                        = 0x12C0;
                gWorldCoordTransientPointLights->framesLeft        = 4;
                slot->head.transform.coord.coord.t[2]              = t2;
                coord->parent                                      = mem->parent;
                coord->coord.t[0]                                  = D_801124DC[arg0->spawnArg1.value].vx;
                coord->coord.t[1]                                  = D_801124DC[arg0->spawnArg1.value].vy;
                coord->coord.t[2]                                  = D_801124DC[arg0->spawnArg1.value].vz;
                coord->composeStamp                                = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->scale      = (gRandomLcgState >> 16) & 0x1FF;
                mem->move.vx    = 0;
                mem->move.vy    = 0;
                mem->move.vz    = -(mem->scale >> 1);
                Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, mem->scale + 0x600, &mem->move);
                arg0->state                    = 1;
                gRoomEffectState->burstRequest = true;
                break;
            case 1:
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x11280,
                            &mem->move);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x21280,
                            &mem->move);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x31280,
                            &mem->move);
                arg0->state++;
                break;
        }
        if (slot->inner >= 0x191) {
            slot->inner -= 0x190;
        }
        if (mem->age >= 5) {
            effectKillTask(mem, arg0);
        }
    }
}

void Gp_EffCtlTask6B(Task* arg0)
{
    EffectWork*                    mem;
    GfxCoord*                      coord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          slot;
    RoomEffectState*               effectState;
    s32                            temp;
    s32                            idx;
    s32                            t2;
    s32                            count;

    lightSlot   = gWorldCoordTransientPointLights;
    slot        = &lightSlot->light;
    mem         = arg0->spawnArg2.pointer;
    coord       = arg0->extra.coordBody->coord;
    effectState = gRoomEffectState;
    if (effectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        mem->age++;
        if (arg0->state == 0) {
            temp                                               = arg0->spawnArg1.halves.high;
            mem->index                                         = temp;
            arg0->spawnArg1.value                              = (u8)arg0->spawnArg1.value;
            slot->head.transform.coord.coord.t[0]              = coord->coord.t[0];
            slot->head.transform.coord.coord.t[1]              = coord->coord.t[1];
            t2                                                 = coord->coord.t[2];
            lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
            slot->head.color.r                                 = 0xC00;
            slot->head.color.g                                 = 0xC00;
            slot->head.color.b                                 = 0xC00;
            slot->inner                                        = 0xFA0;
            slot->outer                                        = 0x12C0;
            slot->head.transform.coord.coord.t[2]              = t2;
            coord->parent                                      = mem->parent;
            coord->coord.t[0]                                  = D_801124DC[arg0->spawnArg1.value].vx;
            coord->coord.t[1]                                  = D_801124DC[arg0->spawnArg1.value].vy;
            coord->coord.t[2]                                  = D_801124DC[arg0->spawnArg1.value].vz;
            coord->composeStamp                                = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->scale      = (gRandomLcgState >> 16) & 0x1FF;
            mem->move.vx    = 0;
            mem->move.vy    = 0;
            mem->move.vz    = -(mem->scale >> 1);
            Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, mem->scale + 0x380, &mem->move);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            Gp_SpawnEff(EFFECT_MUZZLE_FLARE_ADDITIVE, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x380, 0);
            idx         = arg0->spawnArg1.value;
            arg0->state = 1;
            Gp_SpawnEff(EFFECT_RIFLE_MUZZLE_FLASH_MODEL, coord, idx, &D_801125EC[idx]);
            if (arg0->spawnArg1.value == 0x11) {
                mem->scale                                  = 1;
                gWorldCoordTransientPointLights->framesLeft = 1;
            } else {
                mem->scale                                  = 4;
                gWorldCoordTransientPointLights->framesLeft = 4;
            }
            if (mem->index == 0) {
                gRoomEffectState->burstRequest = true;
            }
        }
        if (slot->inner >= 0x191) {
            slot->inner -= 0x190;
        }
        count = mem->age;
        if (mem->scale < count) {
            effectKillTask(mem, arg0);
        }
    }
}

void func_800ED42C(Task* arg0)
{
    EffectWork*                    mem;
    GfxCoord*                      coord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          slot;
    SVECTOR*                       vec;
    s32                            temp;
    s32                            t2;
    s32                            count;
    s32                            i;

    mem       = arg0->spawnArg2.pointer;
    coord     = arg0->extra.coordBody->coord;
    lightSlot = gWorldCoordTransientPointLights;
    slot      = &lightSlot->light;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        mem->age++;
        switch (arg0->state) {
            case 0:
                temp                                               = arg0->spawnArg1.halves.high;
                mem->index                                         = temp;
                arg0->spawnArg1.value                              = (u8)arg0->spawnArg1.value;
                slot->head.transform.coord.coord.t[0]              = coord->coord.t[0];
                slot->head.transform.coord.coord.t[1]              = coord->coord.t[1];
                t2                                                 = coord->coord.t[2];
                lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
                slot->head.color.r                                 = 0xE00;
                slot->head.color.g                                 = 0xA00;
                slot->head.color.b                                 = 0xA00;
                slot->inner                                        = 0xFA0;
                slot->outer                                        = 0x12C0;
                slot->head.transform.coord.coord.t[2]              = t2;
                coord->parent                                      = mem->parent;
                coord->coord.t[0]                                  = D_801124DC[arg0->spawnArg1.value].vx;
                coord->coord.t[1]                                  = D_801124DC[arg0->spawnArg1.value].vy;
                coord->coord.t[2]                                  = D_801124DC[arg0->spawnArg1.value].vz;
                coord->composeStamp                                = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                switch (arg0->spawnArg1.value) {
                    case 1:
                    default:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->scale      = (gRandomLcgState >> 16) & 0x1FF;
                        if (mem->index == 0xD) {
                            mem->move.vx = 0;
                            mem->move.vy = 0;
                            mem->move.vz = -((s32)((u16)mem->scale << 16) >> 18);
                            Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, mem->scale + 0x200, &mem->move);
                            for (i = 0; i < 0xC; i++) {
                                Gp_SpawnEff(EFFECT_PIXEL_SPARK, coord, 0, 0);
                            }
                            for (i = 0; i < 0xC; i++) {
                                Gp_SpawnEff(EFFECT_SHOTGUN_SPARK_LINE, coord, 0, 0);
                            }
                            mem->scale = 0x18;
                        } else {
                            mem->move.vx = 0;
                            mem->move.vy = 0;
                            mem->move.vz = -((s32)((u16)mem->scale << 16) >> 17);
                            Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, mem->scale + 0x380, &mem->move);
                            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            Gp_SpawnEff(EFFECT_MUZZLE_FLARE_ADDITIVE, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x380, 0);
                            if (mem->index == 0xF) {
                                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                                Gp_DrawEffSprite6C(coord, (s16)(mem->scale + 0x280),
                                                   (gRandomLcgState >> 16) & 0xFFF);
                            }
                            mem->scale = 4;
                        }
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x20300, 0);
                        for (i = 0; i < 4; i++) {
                            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            Gp_SpawnEff(EFFECT_MUZZLE_SPARK_THROWN, coord, ((gRandomLcgState >> 16) & 0x1FF) | 0x200, 0);
                        }
                        arg0->state = 1;
                        break;
                    case 15:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->scale      = (gRandomLcgState >> 16) & 0x1FF;
                        if (mem->index == 0xD) {
                            mem->move.vx = 0;
                            mem->move.vy = 0;
                            mem->move.vz = -((s32)((u16)mem->scale << 16) >> 18);
                            Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, mem->scale + 0x200, &mem->move);
                            for (i = 0; i < 0xC; i++) {
                                Gp_SpawnEff(EFFECT_PIXEL_SPARK, coord, 0, 0);
                            }
                            for (i = 0; i < 0xC; i++) {
                                Gp_SpawnEff(EFFECT_SHOTGUN_SPARK_LINE, coord, 0, 0);
                            }
                        } else {
                            mem->move.vx = 0;
                            mem->move.vy = 0;
                            mem->move.vz = -((s32)((u16)mem->scale << 16) >> 17);
                            Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, mem->scale + 0x380, &mem->move);
                            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            Gp_SpawnEff(EFFECT_MUZZLE_FLARE_ADDITIVE, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x380, 0);
                            if (mem->index == 0xF) {
                                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                                Gp_DrawEffSprite6C(coord, (s16)(mem->scale + 0x280),
                                                   (gRandomLcgState >> 16) & 0xFFF);
                            }
                        }
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x20300, 0);
                        for (i = 0; i < 4; i++) {
                            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            Gp_SpawnEff(EFFECT_MUZZLE_SPARK_THROWN, coord, ((gRandomLcgState >> 16) & 0x1FF) | 0x200, 0);
                        }
                        Gp_SpawnEff(EFFECT_SHOTGUN_SHELL_CASING, coord, arg0->spawnArg1.value, &D_801125EC[arg0->spawnArg1.value]);
                        arg0->state = 2;
                        mem->scale  = 4;
                        break;
                    case 23:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->scale      = (gRandomLcgState >> 16) & 0x1FF;
                        if (mem->index == 0xD) {
                            mem->move.vx = 0;
                            mem->move.vy = 0;
                            mem->move.vz = (s32)((u16)mem->scale << 16) >> 18;
                            gte_SetTransMatrix(&GsWSMATRIX);
                            gte_SetRotMatrix(&coord->coord);
                            vec = &mem->move;
                            gte_ldv0(vec);
                            gte_rtv0();
                            gte_stsv(vec);
                            Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, mem->scale + 0x200, vec);
                            i = 0;
                            gfxRotMatrixX(&coord->coord, 0x400, i);
                            coord->composeStamp = GRAPHICS_COORD_DIRTY;
                            for (; i < 0xC; i++) {
                                Gp_SpawnEff(EFFECT_PIXEL_SPARK, coord, 0, 0);
                            }
                            for (i = 0; i < 0xC; i++) {
                                Gp_SpawnEff(EFFECT_SHOTGUN_SPARK_LINE, coord, 0, 0);
                            }
                        } else {
                            mem->move.vx = 0;
                            mem->move.vy = 0;
                            mem->move.vz = (s32)((u16)mem->scale << 16) >> 17;
                            gte_SetTransMatrix(&GsWSMATRIX);
                            gte_SetRotMatrix(&coord->coord);
                            vec = &mem->move;
                            gte_ldv0(vec);
                            gte_rtv0();
                            gte_stsv(vec);
                            Gp_SpawnEff(EFFECT_MUZZLE_FLARE, coord, mem->scale + 0x380, vec);
                            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            Gp_SpawnEff(EFFECT_MUZZLE_FLARE_ADDITIVE, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x380, 0);
                            if (mem->index == 0xF) {
                                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                                Gp_DrawEffSprite6C(coord, (s16)(mem->scale + 0x280),
                                                   (gRandomLcgState >> 16) & 0xFFF);
                            }
                            gfxRotMatrixX(&coord->coord, 0x400, GRAPHICS_ROTATION_COMPOSE);
                            coord->composeStamp = GRAPHICS_COORD_DIRTY;
                        }
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x20300, 0);
                        for (i = 0; i < 4; i++) {
                            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                            Gp_SpawnEff(EFFECT_MUZZLE_SPARK_THROWN, coord, ((gRandomLcgState >> 16) & 0x1FF) | 0x200, 0);
                        }
                        Gp_SpawnEff(EFFECT_SHOTGUN_SHELL_CASING, mem->parent, arg0->spawnArg1.value, &D_801125EC[arg0->spawnArg1.value]);
                        arg0->state = 2;
                        mem->scale  = 4;
                }
                lightSlot->framesLeft          = 4;
                gRoomEffectState->burstRequest = true;
                break;
            case 1:
                if (mem->age == mem->scale) {
                    Gp_SpawnEff(EFFECT_SHOTGUN_SHELL_CASING, coord, arg0->spawnArg1.value, &D_801125EC[arg0->spawnArg1.value]);
                    arg0->state = 2;
                }
                break;
        }
        if (slot->inner >= 0x191) {
            slot->inner -= 0x190;
        }
        count = mem->age;
        if (mem->scale < count) {
            effectKillTask(mem, arg0);
        }
    }
}

void Gp_EffCtlTask6C(Task* arg0)
{
    EffectWork*                    mem;
    GfxCoord*                      coord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          slot;
    s32                            temp;
    s32                            idx;
    s32                            t2;
    s32                            rng;
    s32                            rng2;
    s32                            count;
    s32                            i;

    mem       = arg0->spawnArg2.pointer;
    coord     = arg0->extra.coordBody->coord;
    lightSlot = gWorldCoordTransientPointLights;
    slot      = &lightSlot->light;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        mem->age++;
        switch (arg0->state) {
            case 0:
                temp                                               = arg0->spawnArg1.halves.high;
                mem->index                                         = temp;
                arg0->spawnArg1.value                              = (u8)arg0->spawnArg1.value;
                slot->head.transform.coord.coord.t[0]              = coord->coord.t[0];
                slot->head.transform.coord.coord.t[1]              = coord->coord.t[1];
                t2                                                 = coord->coord.t[2];
                lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
                slot->head.color.r                                 = 0xC00;
                slot->head.color.g                                 = 0xC00;
                slot->head.color.b                                 = 0xC00;
                slot->inner                                        = 0xFA0;
                slot->outer                                        = 0x12C0;
                slot->head.transform.coord.coord.t[2]              = t2;
                coord->parent                                      = mem->parent;
                coord->coord.t[0]                                  = D_801126FC[arg0->spawnArg1.value].vx;
                coord->coord.t[1]                                  = D_801126FC[arg0->spawnArg1.value].vy;
                coord->coord.t[2]                                  = D_801126FC[arg0->spawnArg1.value].vz;
                coord->composeStamp                                = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                idx             = ((u32)rng >> 16) & 0x1FF;
                rng2            = rng * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng;
                mem->scale      = idx;
                gRandomLcgState = rng2;
                Gp_DrawEffSprite6C(coord, idx | 0x400, ((u32)rng2 >> 16) & 0xFFF, idx);
                for (i = 0; i < 4; i++) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_MUZZLE_SPARK_THROWN, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x380, 0);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x21380, 0);
                }
                switch (arg0->spawnArg1.value) {
                    case 1:
                    default:
                        arg0->state = 1;
                        mem->scale  = 0x10;
                        break;
                    case 12:
                        arg0->state = 2;
                        mem->scale  = 4;
                        break;
                    case 27:
                        mem->angle  = 0xA;
                        arg0->state = 1;
                        mem->scale  = 0x18;
                        break;
                }
                lightSlot->framesLeft = 4;
                if (mem->index == 0) {
                    gRoomEffectState->burstRequest = true;
                }
                break;
            case 1:
                if (mem->age == mem->scale) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, ((gRandomLcgState >> 16) & 0xFF) + 0x12180,
                                &D_8011280C[arg0->spawnArg1.value]);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, ((gRandomLcgState >> 16) & 0xFF) + 0x22180,
                                &D_8011280C[arg0->spawnArg1.value]);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, ((gRandomLcgState >> 16) & 0xFF) + 0x32180,
                                &D_8011280C[arg0->spawnArg1.value]);
                    Gp_SpawnEff(EFFECT_091, coord, arg0->spawnArg1.value + mem->angle,
                                &D_8011280C[arg0->spawnArg1.value]);
                    arg0->state = 2;
                }
                break;
        }
        if (slot->inner >= 0x191) {
            slot->inner -= 0x190;
        }
        count = mem->age;
        if (mem->scale < count) {
            effectKillTask(mem, arg0);
        }
    }
}

void Gp_EffSprTask34(Task* arg0)
{
    EffectWork*         mem;
    GfxCoord*           coord;
    EffectShapeScratch* head;
    EffectShapeScratch* block;
    EffectShapeScratch* projectionScratch;
    POLY_FT4*           prim;
    s16                 flag;
    s32                 rng;
    u16                 vz;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag < ROOM_EFFECT_CONTROL_HIDDEN) {
        actorRenderComposeCoord(coord);
        if (mem->age == 0) {
            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->scale      = ((u32)rng >> 16) & 0xFFF;
            gRandomLcgState = rng;
            mem->angle      = arg0->spawnArg1.halves.low;
        }
        head                                     = SCRATCH_STACK_CURSOR(EffectShapeScratch);
        (head - 1)->worldPoint.vx                = (u16)coord->workm.t[0];
        block                                    = head - 1;
        block->worldPoint.vy                     = (u16)coord->workm.t[1];
        vz                                       = (u16)coord->workm.t[2];
        SCRATCH_STACK_CURSOR(EffectShapeScratch) = block;
        block->worldPoint.vz                     = vz;
        projectionScratch                        = block;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&projectionScratch->worldPoint);
        gte_rtps();
        gte_stsxy(&(head - 1)->screenX);
        gte_stflg(&(head - 1)->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&(head - 1)->depth);
            block->depth   = block->depth + 1;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage            = 0x2A;
            prim->clut             = 0x4340;
            prim->u0               = mem->age * 40;
            prim->v0               = 0xD8;
            prim->u1               = mem->age * 40 + 0x27;
            prim->v1               = 0xD8;
            prim->u2               = mem->age * 40;
            prim->v2               = 0xFF;
            prim->u3               = mem->age * 40 + 0x27;
            prim->v3               = 0xFF;
            block->extent.corner.x = (((mem->angle * 23) / block->depth) * rsin(mem->scale)) >> 12;
            block->extent.corner.y = (((mem->angle * 23) / block->depth) * rcos(mem->scale)) >> 12;
            prim->x0               = block->screenX + (u16)block->extent.corner.x;
            prim->x3               = block->screenX - (u16)block->extent.corner.x;
            prim->y0               = block->screenY - (u16)block->extent.corner.y;
            prim->y3               = block->screenY + (u16)block->extent.corner.y;
            block->extent.corner.x = (((mem->angle * 23) / block->depth) * rsin(mem->scale + 0x400)) >> 12;
            block->extent.corner.y = (((mem->angle * 23) / block->depth) * rcos(mem->scale + 0x400)) >> 12;
            prim->x1               = block->screenX + (u16)block->extent.corner.x;
            prim->x2               = block->screenX - (u16)block->extent.corner.x;
            prim->y1               = block->screenY - (u16)block->extent.corner.y;
            prim->y2               = block->screenY + (u16)block->extent.corner.y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        mem->age++;
        if (mem->age < 2) {
            return;
        }
    } else if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    effectKillTask(mem, arg0);
}

void Gp_EffSprTask72(Task* arg0)
{
    EffectWork*         mem;
    GfxCoord*           coord;
    EffectShapeScratch* head;
    EffectShapeScratch* block;
    EffectShapeScratch* projectionScratch;
    POLY_FT4*           prim;
    s16                 flag;
    s32                 rng;
    u16                 vz;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag < ROOM_EFFECT_CONTROL_HIDDEN) {
        actorRenderComposeCoord(coord);
        if (arg0->state == 0) {
            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->scale      = (((u32)rng >> 16) & 0x800) - 0x200;
            gRandomLcgState = rng;
            mem->angle      = arg0->spawnArg1.halves.low;
            arg0->state     = 1;
        }
        head                                     = SCRATCH_STACK_CURSOR(EffectShapeScratch);
        (head - 1)->worldPoint.vx                = (u16)coord->workm.t[0];
        block                                    = head - 1;
        block->worldPoint.vy                     = (u16)coord->workm.t[1];
        vz                                       = (u16)coord->workm.t[2];
        SCRATCH_STACK_CURSOR(EffectShapeScratch) = block;
        block->worldPoint.vz                     = vz;
        projectionScratch                        = block;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&projectionScratch->worldPoint);
        gte_rtps();
        gte_stsxy(&(head - 1)->screenX);
        gte_stflg(&(head - 1)->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&(head - 1)->depth);
            block->depth   = block->depth + 1;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage            = 0x28;
            prim->clut             = 0x4288;
            prim->u0               = mem->age * 32;
            prim->v0               = 0x38;
            prim->u1               = mem->age * 32 + 0x1F;
            prim->v1               = 0x38;
            prim->u2               = mem->age * 32;
            prim->v2               = 0x57;
            prim->u3               = mem->age * 32 + 0x1F;
            prim->v3               = 0x57;
            block->extent.corner.x = (((mem->angle * 31) / block->depth) * rsin(mem->scale)) >> 12;
            block->extent.corner.y = (((mem->angle * 31) / block->depth) * rcos(mem->scale)) >> 12;
            prim->x0               = block->screenX + (u16)block->extent.corner.x;
            prim->x3               = block->screenX - (u16)block->extent.corner.x;
            prim->y0               = block->screenY - (u16)block->extent.corner.y;
            prim->y3               = block->screenY + (u16)block->extent.corner.y;
            block->extent.corner.x = (((mem->angle * 31) / block->depth) * rsin(mem->scale + 0x400)) >> 12;
            block->extent.corner.y = (((mem->angle * 31) / block->depth) * rcos(mem->scale + 0x400)) >> 12;
            prim->x1               = block->screenX + (u16)block->extent.corner.x;
            prim->x2               = block->screenX - (u16)block->extent.corner.x;
            prim->y1               = block->screenY - (u16)block->extent.corner.y;
            prim->y2               = block->screenY + (u16)block->extent.corner.y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        mem->age++;
        if (mem->age < 2) {
            return;
        }
    } else if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    effectKillTask(mem, arg0);
}

void Gp_EffLineTaskA3(Task* arg0)
{
    EffectWork*        mem;
    GfxCoord*          coord;
    EffectLineScratch* block;
    LINE_G2*           prim;
    s16                flag;
    s16                c;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag < ROOM_EFFECT_CONTROL_HIDDEN) {
        actorRenderComposeCoord(coord);
        if (arg0->state == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->scale      = (gRandomLcgState >> 16) % 3 + 1;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vx    = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vy    = ((gRandomLcgState >> 16) & 0x1FF) + 0x200;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vz    = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
            arg0->state     = 1;
        }
        block                  = SCRATCH_STACK_RESERVE_BLOCK(EffectLineScratch);
        block->endpoints[0].vx = coord->workm.t[0];
        block->endpoints[0].vy = coord->workm.t[1];
        block->endpoints[0].vz = coord->workm.t[2];
        gte_SetRotMatrix(&mem->parent->coord);
        gte_ldv0(&mem->move);
        gte_rtv0();
        gte_stsv(&block->endpoints[1]);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&block->endpoints[1]);
        gte_rtv0();
        gte_stsv(&block->endpoints[1]);
        gte_lddp((mem->age << 11) + 0x1000);
        gte_ldsv(&block->endpoints[1]);
        gte_gpf12();
        gte_stsv(&block->endpoints[1]);
        block->endpoints[1].vx += block->endpoints[0].vx;
        block->endpoints[1].vy += block->endpoints[0].vy;
        block->endpoints[1].vz += block->endpoints[0].vz;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->endpoints[0]);
        gte_rtps();
        gte_stsxy(&block->screenEndpoints[0]);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_ldv0(&block->endpoints[1]);
            gte_rtps();
            gte_stsxy(&block->screenEndpoints[1]);
            gte_stflg(&block->projectionFlags);
            if (block->projectionFlags >= 0) {
                gte_stszotz(&block->depth);
                block->depth   = block->depth + 1;
                prim           = gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setlen(prim, 4);
                setcode(prim, 0x50);
                c        = 0xFF - (mem->age << 6);
                prim->r0 = 0;
                prim->g0 = 0;
                prim->b0 = 0;
                prim->r1 = c;
                prim->g1 = c >> mem->scale;
                prim->b1 = c >> 3;
                prim->x0 = block->screenEndpoints[0].vx;
                prim->y0 = block->screenEndpoints[0].vy;
                prim->x1 = block->screenEndpoints[1].vx;
                prim->y1 = block->screenEndpoints[1].vy;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        prim);
                gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectLineScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        mem->age++;
        if (mem->age < 4) {
            return;
        }
    } else if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    effectKillTask(mem, arg0);
}

static void Gp_DrawEffSprite6C(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    EffectShapeScratch* head;
    EffectShapeScratch* block;
    EffectShapeScratch* projectionScratch;
    POLY_FT4*           prim;
    s32                 ang;
    u16                 vz;

    head                                     = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    (head - 1)->worldPoint.vx                = (u16)arg0->workm.t[0];
    block                                    = head - 1;
    block->worldPoint.vy                     = (u16)arg0->workm.t[1];
    vz                                       = (u16)arg0->workm.t[2];
    SCRATCH_STACK_CURSOR(EffectShapeScratch) = block;
    block->worldPoint.vz                     = vz;
    projectionScratch                        = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projectionScratch->worldPoint);
    gte_rtps();
    gte_stsxy(&(head - 1)->screenX);
    gte_stflg(&(head - 1)->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&(head - 1)->depth);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x29;
        prim->clut  = 0x428B;
        setUVWH(prim, 0x70, 0xC8, 0x37, 0x37);
        block->extent.corner.x = ((((s16)arg1 * 55) / block->depth) * rsin((s16)arg2)) >> 12;
        block->extent.corner.y = ((((s16)arg1 * 55) / block->depth) * rcos((s16)arg2)) >> 12;
        prim->x0               = block->screenX + (u16)block->extent.corner.x;
        prim->x3               = block->screenX - (u16)block->extent.corner.x;
        prim->y0               = block->screenY - (u16)block->extent.corner.y;
        prim->y3               = block->screenY + (u16)block->extent.corner.y;
        ang                    = (s16)arg2 + 0x400;
        block->extent.corner.x = ((((s16)arg1 * 55) / block->depth) * rsin(ang)) >> 12;
        block->extent.corner.y = ((((s16)arg1 * 55) / block->depth) * rcos(ang)) >> 12;
        prim->x1               = block->screenX + (u16)block->extent.corner.x;
        prim->x2               = block->screenX - (u16)block->extent.corner.x;
        prim->y1               = block->screenY - (u16)block->extent.corner.y;
        prim->y2               = block->screenY + (u16)block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

void Gp_EffSprTask35(Task* arg0)
{
    EffectWork*         mem;
    GfxCoord*           coord;
    EffectShapeScratch* head;
    EffectShapeScratch* block;
    EffectShapeScratch* projectionScratch;
    POLY_FT4*           prim;
    s16                 flag;
    s16                 val;
    s32                 rng;
    s32                 t2;
    s32                 quot;
    u16                 vz;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
    } else {
        if (arg0->state == 0) {
            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->scale      = ((u32)rng >> 16) & 0xFFF;
            gRandomLcgState = rng;
            mem->angle      = arg0->spawnArg1.halves.low & 0xFFF;
            if (arg0->spawnArg1.value & 0xF0000) {
                val = (arg0->spawnArg1.value >> 16) & 0xF;
            } else {
                val = 1;
            }
            mem->period = val;
            if (arg0->spawnArg1.value & 0x1000) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
            }
            arg0->state = 1;
        }
        actorRenderComposeCoord(coord);
        head                                     = SCRATCH_STACK_CURSOR(EffectShapeScratch);
        (head - 1)->worldPoint.vx                = (u16)coord->workm.t[0];
        block                                    = head - 1;
        block->worldPoint.vy                     = (u16)coord->workm.t[1];
        vz                                       = (u16)coord->workm.t[2];
        SCRATCH_STACK_CURSOR(EffectShapeScratch) = block;
        block->worldPoint.vz                     = vz;
        projectionScratch                        = block;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&projectionScratch->worldPoint);
        gte_rtps();
        gte_stsxy(&(head - 1)->screenX);
        gte_stflg(&(head - 1)->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&(head - 1)->depth);
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage            = 0x28;
            prim->clut             = 0x4242;
            quot                   = mem->age / mem->period;
            prim->v0               = 0;
            prim->u0               = quot * 24 + 0x30;
            quot                   = mem->age / mem->period;
            prim->v1               = 0;
            prim->u1               = quot * 24 + 0x47;
            quot                   = mem->age / mem->period;
            prim->v2               = 0x17;
            prim->u2               = quot * 24 + 0x30;
            quot                   = mem->age / mem->period;
            prim->v3               = 0x17;
            prim->u3               = quot * 24 + 0x47;
            block->extent.corner.x = (((mem->angle * 23) / block->depth) * rsin(mem->scale)) >> 12;
            block->extent.corner.y = (((mem->angle * 23) / block->depth) * rcos(mem->scale)) >> 12;
            prim->x0               = block->screenX + (u16)block->extent.corner.x;
            prim->x3               = block->screenX - (u16)block->extent.corner.x;
            prim->y0               = block->screenY - (u16)block->extent.corner.y;
            prim->y3               = block->screenY + (u16)block->extent.corner.y;
            block->extent.corner.x = (((mem->angle * 23) / block->depth) * rsin(mem->scale + 0x400)) >> 12;
            block->extent.corner.y = (((mem->angle * 23) / block->depth) * rcos(mem->scale + 0x400)) >> 12;
            prim->x1               = block->screenX + (u16)block->extent.corner.x;
            prim->x2               = block->screenX - (u16)block->extent.corner.x;
            prim->y1               = block->screenY - (u16)block->extent.corner.y;
            prim->y2               = block->screenY + (u16)block->extent.corner.y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        coord->coord.t[0]  += mem->move.vx;
        coord->coord.t[1]  += mem->move.vy;
        t2                  = coord->coord.t[2] + mem->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        coord->coord.t[2]   = t2;
        mem->age++;
        if (mem->age <= mem->period * 4 - 1) {
            return;
        }
    }
    effectKillTask(mem, arg0);
}

void Gp_EffSprTask6F(Task* arg0)
{
    EffectWork*         mem;
    GfxCoord*           coord;
    EffectShapeScratch* head;
    EffectShapeScratch* block;
    EffectShapeScratch* projectionScratch;
    POLY_FT4*           prim;
    s16                 flag;
    s32                 t2;
    s32                 quot;
    u16                 vz;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
    } else {
        actorRenderComposeCoord(coord);
        if (arg0->state == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->scale      = (gRandomLcgState >> 16) & 0xFFF;
            mem->angle      = arg0->spawnArg1.halves.low & 0xFFF;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->period     = ((gRandomLcgState >> 16) & 1) + 1;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vx    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vy    = (gRandomLcgState >> 14) & 0x7C;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vz    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
            gte_SetRotMatrix(&mem->parent->coord);
            gte_ldv0(&mem->move);
            gte_rtv0();
            gte_stsv(&mem->move);
            arg0->state = 1;
        }
        head                                     = SCRATCH_STACK_CURSOR(EffectShapeScratch);
        (head - 1)->worldPoint.vx                = (u16)coord->workm.t[0];
        block                                    = head - 1;
        block->worldPoint.vy                     = (u16)coord->workm.t[1];
        vz                                       = (u16)coord->workm.t[2];
        SCRATCH_STACK_CURSOR(EffectShapeScratch) = block;
        block->worldPoint.vz                     = vz;
        projectionScratch                        = block;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&projectionScratch->worldPoint);
        gte_rtps();
        gte_stsxy(&(head - 1)->screenX);
        gte_stflg(&(head - 1)->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&(head - 1)->depth);
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage            = 0x28;
            prim->clut             = 0x4253;
            quot                   = mem->age / mem->period;
            prim->v0               = 0x18;
            prim->u0               = quot * 32;
            quot                   = mem->age / mem->period;
            prim->v1               = 0x18;
            prim->u1               = quot * 32 + 0x1F;
            quot                   = mem->age / mem->period;
            prim->v2               = 0x37;
            prim->u2               = quot * 32;
            quot                   = mem->age / mem->period;
            prim->v3               = 0x37;
            prim->u3               = quot * 32 + 0x1F;
            block->extent.corner.x = (((mem->angle * 31) / block->depth) * rsin(mem->scale)) >> 12;
            block->extent.corner.y = (((mem->angle * 31) / block->depth) * rcos(mem->scale)) >> 12;
            prim->x0               = block->screenX + (u16)block->extent.corner.x;
            prim->x3               = block->screenX - (u16)block->extent.corner.x;
            prim->y0               = block->screenY - (u16)block->extent.corner.y;
            prim->y3               = block->screenY + (u16)block->extent.corner.y;
            block->extent.corner.x = (((mem->angle * 31) / block->depth) * rsin(mem->scale + 0x400)) >> 12;
            block->extent.corner.y = (((mem->angle * 31) / block->depth) * rcos(mem->scale + 0x400)) >> 12;
            prim->x1               = block->screenX + (u16)block->extent.corner.x;
            prim->x2               = block->screenX - (u16)block->extent.corner.x;
            prim->y1               = block->screenY - (u16)block->extent.corner.y;
            prim->y2               = block->screenY + (u16)block->extent.corner.y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        coord->coord.t[0]  += mem->move.vx;
        coord->coord.t[1]  += mem->move.vy;
        t2                  = coord->coord.t[2] + mem->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        coord->coord.t[2]   = t2;
        mem->age++;
        if (mem->age <= mem->period * 8 - 1) {
            return;
        }
    }
    effectKillTask(mem, arg0);
}

void Gp_EffModelTask(Task* arg0)
{
    SVECTOR     delta;
    SVECTOR     dir;
    SVECTOR     pos;
    VECTOR      vec;
    VECTOR      tmp;
    TmdObject*  extra;
    EffectWork* mem;
    GfxCoord*   coord;
    SVECTOR*    vel;
    s16         flag;

    extra = arg0->extra.tmd;
    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = extra->coords;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto release;
    }
    actorRenderComposeCoord(coord);
    if (arg0->state == 0) {
        extra->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        switch (arg0->spawnArg1.value) {
            case 1:
            default:
                mem->scale      = 0xD4;
                mem->angle      = 0x14;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = ((gRandomLcgState >> 16) & 0x3F) + 0x60;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy    = 0x10 - ((gRandomLcgState >> 16) & 0x7F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz    = (gRandomLcgState >> 16) & 0x7F;
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
                break;
            case 2:
                mem->scale      = 0x100;
                mem->angle      = 0x14;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = ((gRandomLcgState >> 16) & 0x3F) + 0x60;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy    = 0x10 - ((gRandomLcgState >> 16) & 0x7F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz    = (gRandomLcgState >> 16) & 0x7F;
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
                break;
            case 3:
                mem->scale      = 0x114;
                mem->angle      = 0xF;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = ((gRandomLcgState >> 16) & 0x3F) + 0x60;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy    = 0x10 - ((gRandomLcgState >> 16) & 0x7F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz    = (gRandomLcgState >> 16) & 0x7F;
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
                break;
            case 30:
            case 31:
            case 32:
                mem->scale      = 0x114;
                mem->angle      = 0xA;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = ((gRandomLcgState >> 16) & 0x3F) + 0x60;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy    = 0x10 - ((gRandomLcgState >> 16) & 0x7F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz    = (gRandomLcgState >> 16) & 0x7F;
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
                break;
            case 5:
            case 33:
                mem->scale      = 0xD4;
                mem->angle      = 0x14;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = ((gRandomLcgState >> 16) & 0x3F) + 0x60;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy    = 0x10 - ((gRandomLcgState >> 16) & 0x7F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz    = (gRandomLcgState >> 16) & 0x7F;
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
                break;
            case 9:
                mem->angle      = 0x14;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy    = ((gRandomLcgState >> 16) & 0x3F) + 0x40;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->scale      = ((gRandomLcgState >> 16) & 0x3F) + 0x40;
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
                break;
            case 16:
            case 20:
            case 21:
            case 25:
            case 26:
            case 27:
            case 28:
            case 29:
                mem->scale      = 0x114;
                mem->angle      = 0xF;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = ((gRandomLcgState >> 16) & 0x3F) + 0x60;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy    = 0x10 - ((gRandomLcgState >> 16) & 0x7F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz    = (gRandomLcgState >> 16) & 0x7F;
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
                break;
            case 17:
                mem->scale      = 0x114;
                mem->angle      = 5;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = ((gRandomLcgState >> 16) & 0x3F) + 0x40;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz    = 0xFF80 - ((gRandomLcgState >> 16) & 0x3F);
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
                break;
            case 13:
            case 14:
            case 15:
                mem->scale      = 0xBF;
                mem->angle      = 0x14;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = ((gRandomLcgState >> 16) & 0x3F) + 0x60;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy    = 0x10 - ((gRandomLcgState >> 16) & 0x7F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz    = (gRandomLcgState >> 16) & 0x7F;
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
                break;
            case 23:
                mem->scale      = 0xBF;
                mem->angle      = 0x14;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = 0xFFA0 - ((gRandomLcgState >> 16) & 0x3F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy    = 0x10 - ((gRandomLcgState >> 16) & 0x7F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz    = -((gRandomLcgState >> 16) & 0x7F);
                memset(&tmp, 0, 0x10);
                tmp.vx = mem->move.vx;
                tmp.vy = mem->move.vy;
                tmp.vz = mem->move.vz;
                vec    = tmp;
                ApplyTransposeMatrixLV(&coord->coord, &vec, &vec);
                mem->move.vx = vec.vx;
                mem->move.vy = vec.vy;
                mem->move.vz = vec.vz;
                break;
            case 11:
                mem->scale      = 0x60;
                mem->angle      = 0x14;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz    = (((gRandomLcgState >> 16) & 0x1F) + 0x10);
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
                break;
            case 12:
                mem->scale      = 0x80;
                mem->angle      = 0xF;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz    = 0xFFC0 - ((gRandomLcgState >> 16) & 0x3F);
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
                break;
            case 37:
                mem->scale      = 0x60;
                mem->angle      = 0x14;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz    = 0xFFF0 - ((gRandomLcgState >> 16) & 0x1F);
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
                break;
        }
        VectorNormalSS(&mem->move, &mem->move);
        gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->pos.vx         = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
        gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->pos.vy         = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
        gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->pos.vz         = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        arg0->state         = 1;
        gfxRotMatrixX(&coord->coord, 0x800, GRAPHICS_ROTATION_COMPOSE);
        return;
    }
    Gfx_RotMatrixXYZ(&coord->coord, &mem->pos, 0);
    MatrixNormal(&coord->coord, &coord->coord);
    gte_lddp(mem->scale);
    vel = &mem->move;
    gte_ldsv(vel);
    gte_gpf12();
    gte_stsv(&delta);
    coord->coord.t[0]  += delta.vx;
    coord->coord.t[1]  += delta.vy;
    coord->coord.t[2]  += delta.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(&delta);
    gte_rtv0();
    gte_stsv(&dir);
    pos.vx  = (u16)coord->workm.t[0];
    pos.vy  = (u16)coord->workm.t[1];
    pos.vz  = (u16)coord->workm.t[2];
    dir.vx += pos.vx;
    dir.vy += pos.vy;
    dir.vz += pos.vz;
    if (func_800DE7CC(&dir, &pos, &dir, &pos) == 1) {
        coord->coord.t[0] -= delta.vx;
        coord->coord.t[1] -= delta.vy;
        coord->coord.t[2] -= delta.vz;
        mem->move.vx       = (pos.vx >> 1) + (mem->move.vx >> 1);
        mem->move.vy       = pos.vy + (mem->move.vy >> 1);
        mem->move.vz       = (pos.vz >> 1) + (mem->move.vz >> 1);
        VectorNormalSS(vel, vel);
        mem->scale = (mem->scale * 2) / 3;
        gte_lddp(mem->scale);
        gte_ldsv(vel);
        gte_gpf12();
        gte_stsv(&delta);
        coord->coord.t[0] += delta.vx;
        coord->coord.t[1] += delta.vy;
        coord->coord.t[2] += delta.vz;
    } else {
        mem->move.vy += 0x180;
    }
    mem->age++;
    if (mem->angle < mem->age) {
        extra->flags = (gDisplayState.animFrame & 1) ? extra->flags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW : extra->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
        if (mem->angle * 2 < mem->age) {
            goto release;
        }
    }
    return;
release:
    effectKillTask(mem, arg0);
}

void Gp_EffCtlTask6E(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s32         rng;

    coord = arg0->extra.coordBody->coord;
    mem   = arg0->spawnArg2.pointer;
    if (arg0->state == 0) {
        if (arg0->spawnArg1.value & 0xF0000) {
            mem->scale = (u32)arg0->spawnArg1.value >> 16;
        } else {
            mem->scale = 0xC;
        }
        arg0->spawnArg1.value = (u16)arg0->spawnArg1.value;
        coord->parent         = mem->parent;
        coord->coord.t[0]     = D_801125EC[arg0->spawnArg1.value].vx;
        coord->coord.t[1]     = D_801125EC[arg0->spawnArg1.value].vy;
        coord->coord.t[2]     = D_801125EC[arg0->spawnArg1.value].vz;
        coord->coord.t[0]    += mem->pos.vx;
        coord->coord.t[1]    += mem->pos.vy;
        coord->coord.t[2]    += mem->pos.vz;
        coord->composeStamp   = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        arg0->state     = 1;
        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng;
        Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, (((u32)rng >> 16) & 0x1FF) | 0x11200, 0);
        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng;
        Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, (((u32)rng >> 16) & 0x1FF) | 0x21200, 0);
        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rng;
        Gp_SpawnEff(EFFECT_MUZZLE_SPARK, coord, (((u32)rng >> 16) & 0x1FF) | 0x31200, 0);
    }
    Gp_SpawnEff(EFFECT_091, coord, arg0->spawnArg1.value, 0);
    mem->age++;
    if (mem->age > mem->scale - 1) {
        effectKillTask(mem, arg0);
    }
}

void Gp_EffCtlTask6D(Task* arg0)
{
    GfxCoord* coord;
    MATRIX*   m;
    void*     mem;
    s32       i;
    s32       one;

    i                    = 0;
    one                  = ONE;
    coord                = arg0->extra.coordBody->coord;
    mem                  = arg0->spawnArg2.pointer;
    m                    = &coord->coord;
    *(s32*)&coord->coord = one;
    MATRIX_PAIR(m, 0, 2) = 0;
    MATRIX_PAIR(m, 1, 1) = one;
    MATRIX_PAIR(m, 2, 0) = 0;
    m->m[2][2]           = one;
    coord->composeStamp  = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);

    for (; i < 6; i++) {
        Gp_SpawnEff(EFFECT_BULLET_CASING, coord, 9, 0);
    }

    effectKillTask(mem, arg0);
}

void Gp_EffTileTaskA4(Task* arg0)
{
    EffectWork*               mem;
    GfxCoord*                 coord;
    _EffectPixelSparkScratch* scratch;
    TILE*                     prim;
    s16                       c;

    coord   = arg0->extra.coordBody->coord;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_EffectPixelSparkScratch);
    mem     = arg0->spawnArg2.pointer;
    actorRenderComposeCoord(coord);
    if (arg0->state == 0) {
        if (arg0->spawnArg1.value != 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vx    = 0x20 - ((gRandomLcgState >> 16) & 0x3F);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vy    = 0x20 - ((gRandomLcgState >> 16) & 0x3F);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vz    = 0x20 - ((gRandomLcgState >> 16) & 0x3F);
        } else {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vx    = 0x40 - ((gRandomLcgState >> 16) & 0x7F);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vy    = ((gRandomLcgState >> 16) & 0xFF) + 0x100;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vz    = 0x40 - ((gRandomLcgState >> 16) & 0x7F);

            gte_SetRotMatrix(&mem->parent->coord);
            gte_ldv0(&mem->move);
            gte_rtv0();
            gte_stsv(&mem->move);

            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->scale      = (gRandomLcgState >> 16) % 3 + 1;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->angle      = ((gRandomLcgState >> 16) & 1) + 1;
        }
        arg0->state = 1;
    }
    coord->coord.t[0]     += mem->move.vx;
    coord->coord.t[1]     += mem->move.vy;
    coord->coord.t[2]     += mem->move.vz;
    coord->composeStamp    = GRAPHICS_COORD_DIRTY;
    scratch->worldPoint.vx = (u16)coord->workm.t[0];
    scratch->worldPoint.vy = (u16)coord->workm.t[1];
    scratch->worldPoint.vz = (u16)coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint);
    gte_rtps();
    gte_stsxy(&scratch->screenPoint);
    gte_stflg(&scratch->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depth);
        scratch->depth = scratch->depth + 1;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 3);
        setcode(prim, 0x60);
        c        = 0xFF - (u16)mem->age * 0x10;
        prim->w  = mem->angle;
        prim->h  = mem->angle;
        prim->r0 = c;
        prim->g0 = c >> mem->scale;
        prim->b0 = c >> 3;
        prim->x0 = scratch->screenPoint.vx;
        prim->y0 = scratch->screenPoint.vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
        gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, scratch->depth);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_EffectPixelSparkScratch);
    mem->age++;
    if (mem->age >= 8) {
        effectKillTask(mem, arg0);
    }
}

void Gp_EffCtlTask3B(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s16         flag;
    s32         i;
    s32         rng;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag < ROOM_EFFECT_CONTROL_HIDDEN) {
        actorRenderComposeCoord(coord);
        if (arg0->state == 0) {
            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->scale      = ((u32)rng >> 16) & 0xFFF;
            gRandomLcgState = rng;
            if (arg0->spawnArg1.value & 0xFFF) {
                mem->angle = arg0->spawnArg1.halves.low & 0xFFF;
            } else {
                mem->angle = 0x200;
            }
            for (i = 0; i < 6; i++) {
                Gp_SpawnEff(EFFECT_PIXEL_SPARK, coord, 1, 0);
            }
            arg0->state = 1;
        }
        Gp_DrawEffSprite3B(coord, mem->age, mem->angle, mem->scale);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        mem->age++;
        if (mem->age < 4) {
            return;
        }
    } else if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        return;
    }
    effectKillTask(mem, arg0);
}

static void Gp_DrawEffSprite3B(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    EffectShapeScratch* head;
    EffectShapeScratch* block;
    EffectShapeScratch* projectionScratch;
    POLY_FT4*           prim;
    s32                 uv;
    s32                 ang;
    u16                 vz;

    head                                     = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    (head - 1)->worldPoint.vx                = (u16)arg0->workm.t[0];
    block                                    = head - 1;
    block->worldPoint.vy                     = (u16)arg0->workm.t[1];
    vz                                       = (u16)arg0->workm.t[2];
    SCRATCH_STACK_CURSOR(EffectShapeScratch) = block;
    block->worldPoint.vz                     = vz;
    projectionScratch                        = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projectionScratch->worldPoint);
    gte_rtps();
    gte_stsxy(&(head - 1)->screenX);
    gte_stflg(&(head - 1)->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&(head - 1)->depth);
        block->depth   = block->depth + 1;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage            = 0x28;
        prim->clut             = 0x4241;
        uv                     = arg1 * 0x18;
        prim->u0               = uv - 0x70;
        prim->u2               = uv - 0x70;
        prim->v0               = 0;
        prim->u1               = uv - 0x59;
        prim->v1               = 0;
        prim->v2               = 0x17;
        prim->u3               = uv - 0x59;
        prim->v3               = 0x17;
        block->extent.corner.x = ((((s16)arg2 * 23) / block->depth) * rsin((s16)arg3)) >> 12;
        block->extent.corner.y = ((((s16)arg2 * 23) / block->depth) * rcos((s16)arg3)) >> 12;
        prim->x0               = block->screenX + (u16)block->extent.corner.x;
        prim->x3               = block->screenX - (u16)block->extent.corner.x;
        prim->y0               = block->screenY - (u16)block->extent.corner.y;
        prim->y3               = block->screenY + (u16)block->extent.corner.y;
        ang                    = (s16)arg3 + 0x400;
        block->extent.corner.x = ((((s16)arg2 * 23) / block->depth) * rsin(ang)) >> 12;
        block->extent.corner.y = ((((s16)arg2 * 23) / block->depth) * rcos(ang)) >> 12;
        prim->x1               = block->screenX + (u16)block->extent.corner.x;
        prim->x2               = block->screenX - (u16)block->extent.corner.x;
        prim->y1               = block->screenY - (u16)block->extent.corner.y;
        prim->y2               = block->screenY + (u16)block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

void Gp_EffSprTask5C(Task* arg0)
{
    EffectShapeScratch*             head;
    EffectShapeScratch*             block;
    EffectShapeScratch*             projectionScratch;
    GfxCoord*                       coord;
    EffectWork*                     mem;
    POLY_FT4*                       prim;
    const EffectSpriteTextureFrame* textureFrame;
    s16                             flag;
    s16                             scale;
    s16                             step;
    s32                             rng;
    s32                             i;
    s32                             n;
    s32                             t2;
    s32                             tmp;
    u16                             vz;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
    } else {
        actorRenderComposeCoord(coord);
        if (arg0->state == 0) {
            scale = 0x200;
            if (arg0->spawnArg1.value & 0xFFF) {
                scale = arg0->spawnArg1.halves.low & 0xFFF;
            }
            mem->scale      = scale;
            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->angle      = ((u32)rng >> 16) & 0xFFF;
            gRandomLcgState = rng;
            if (arg0->spawnArg1.value & 0xF000) {
                step = (arg0->spawnArg1.value >> 12) & 0xF;
            } else {
                step = 2;
            }
            mem->period = step;
            mem->step   = (s32)((u16)mem->scale << 16) >> 23;
            tmp         = arg0->spawnArg1.signedBytes[3];
            mem->index  = tmp & 0xF;
            if (mem->index != 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                gte_lddp(mem->scale << 3);
                gte_ldsv(&mem->move);
                gte_gpf12();
                gte_stsv(&mem->move);
                gte_lddp(mem->index << 12);
                gte_ldsv(&mem->move);
                gte_gpf12();
                gte_stsv(&mem->move);
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
            } else if (!(arg0->spawnArg1.value & 0xF0000000)) {
                n = gDisplayState.animFrame & 3;
                i = 0;
                if (n != 0) {
                    do {
                        Gp_SpawnEff(EFFECT_EXPLOSION, coord, ((s32)((u16)mem->scale << 16) >> 17) | 0x02001000, 0);
                        i += 1;
                    } while (i < n);
                }
                n = gDisplayState.animFrame % 3;
                i = 0;
                if (n > 0) {
                    do {
                        Gp_SpawnEff(EFFECT_EXPLOSION, coord, ((s32)((u16)mem->scale << 16) >> 17) | 0x01002000, 0);
                        i += 1;
                    } while (i < n);
                }
            }
            arg0->state = 1;
        }
        head                                     = SCRATCH_STACK_CURSOR(EffectShapeScratch);
        (head - 1)->worldPoint.vx                = (u16)coord->workm.t[0];
        block                                    = head - 1;
        block->worldPoint.vy                     = (u16)coord->workm.t[1];
        vz                                       = (u16)coord->workm.t[2];
        SCRATCH_STACK_CURSOR(EffectShapeScratch) = block;
        block->worldPoint.vz                     = vz;
        projectionScratch                        = block;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&projectionScratch->worldPoint);
        gte_rtps();
        gte_stsxy(&(head - 1)->screenX);
        gte_stflg(&(head - 1)->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&(head - 1)->depth);
            block->depth   = block->depth + 1;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2C);
            textureFrame = &gEffectSpriteAtlasFrames[mem->age / mem->period];
            prim->code  |= 3;
            prim->tpage  = EFFECT_SPRITE_ATLAS_TEXTURE_PAGE;
            prim->clut   = getClut(textureFrame->clutX, textureFrame->clutY);
            prim->u0     = textureFrame->u;
            prim->v0     = textureFrame->v;
            prim->u1     = textureFrame->u + EFFECT_SPRITE_ATLAS_UV_SPAN;
            prim->v1     = textureFrame->v;
            prim->u2     = textureFrame->u;
            prim->v2     = textureFrame->v + EFFECT_SPRITE_ATLAS_UV_SPAN;
            prim->u3     = textureFrame->u + EFFECT_SPRITE_ATLAS_UV_SPAN;
            prim->v3     = textureFrame->v + EFFECT_SPRITE_ATLAS_UV_SPAN;
            // Reuse the inclusive texel span to size the billboard's half-diagonal.
            block->extent.corner.x = (((mem->scale * EFFECT_SPRITE_ATLAS_UV_SPAN) / block->depth) * rsin(mem->angle)) >> 12;
            block->extent.corner.y = (((mem->scale * EFFECT_SPRITE_ATLAS_UV_SPAN) / block->depth) * rcos(mem->angle)) >> 12;
            prim->x0               = block->screenX + (u16)block->extent.corner.x;
            prim->x3               = block->screenX - (u16)block->extent.corner.x;
            prim->y0               = block->screenY - (u16)block->extent.corner.y;
            prim->y3               = block->screenY + (u16)block->extent.corner.y;
            block->extent.corner.x = (((mem->scale * EFFECT_SPRITE_ATLAS_UV_SPAN) / block->depth) * rsin(mem->angle + 0x400)) >> 12;
            block->extent.corner.y = (((mem->scale * EFFECT_SPRITE_ATLAS_UV_SPAN) / block->depth) * rcos(mem->angle + 0x400)) >> 12;
            prim->x1               = block->screenX + (u16)block->extent.corner.x;
            prim->x2               = block->screenX - (u16)block->extent.corner.x;
            prim->y1               = block->screenY - (u16)block->extent.corner.y;
            prim->y2               = block->screenY + (u16)block->extent.corner.y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        coord->coord.t[0]  += mem->move.vx;
        coord->coord.t[1]  += mem->move.vy;
        t2                  = coord->coord.t[2] + mem->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        coord->coord.t[2]   = t2;
        mem->scale         += mem->step;
        mem->age++;
        if (mem->age <= mem->period * ARRAY_SIZE(gEffectSpriteAtlasFrames) - 1) {
            return;
        }
    }
    effectKillTask(mem, arg0);
}

void func_800F289C(Task* arg0)
{
    EffectShapeScratch* block;
    EffectWork*         mem;
    GfxCoord*           coord;
    POLY_FT4*           prim;
    s16                 flag;
    s16                 scale;
    s16                 mode;
    s32                 tmp;
    s32                 i;
    s32                 n;
    s32                 mask;
    s32                 step;
    s32                 step2;
    s32                 mask2;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
    } else {
        actorRenderComposeCoord(coord);
        if (arg0->state == 0) {
            scale = 0x200;
            if (arg0->spawnArg1.value & 0xFFF) {
                scale = arg0->spawnArg1.halves.low & 0xFFF;
            }
            mem->scale      = scale;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->angle      = (gRandomLcgState >> 16) & 0xFFF;
            if (arg0->spawnArg1.value & 0xF000) {
                mem->period = (arg0->spawnArg1.value >> 12) & 0xF;
            } else {
                mem->period = 1;
            }
            if (arg0->spawnArg1.value & 0xFF0000) {
                mem->step = (arg0->spawnArg1.value >> 16) & 0xFF;
            } else {
                mem->step = mem->scale >> 8;
            }
            tmp        = arg0->spawnArg1.signedBytes[3];
            mode       = tmp & 0xF;
            mem->index = mode;
            if (mode != 0) {
                if (mode == 1) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vx    = 8 - ((gRandomLcgState >> 16) & 0xF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vy    = ((gRandomLcgState >> 16) & 0xF) * 3;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vz    = 8 - ((gRandomLcgState >> 16) & 0xF);
                } else {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vx    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vy    = 0x10 - ((gRandomLcgState >> 0x10) & 0x1F);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vz    = 0x10 - ((gRandomLcgState >> 0x10) & 0x1F);
                    gte_lddp((mem->scale << 3));
                    gte_ldsv(&mem->move);
                    gte_gpf12();
                    gte_stsv(&mem->move);
                    gte_lddp((mem->index << 11));
                    gte_ldsv(&mem->move);
                    gte_gpf12();
                    gte_stsv(&mem->move);
                }
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0();
                gte_stsv(&mem->move);
            } else {
                switch (mem->step) {
                    case 1:
                        mem->move.vx    = 0;
                        mem->move.vz    = 0;
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vy    = -((gRandomLcgState >> 16) & 0xF) - 0x20;
                        break;
                    case 2:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vx    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vy    = -((gRandomLcgState >> 0x10) & 0x1F) - 0x10;
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vz    = 0x10 - ((gRandomLcgState >> 0x10) & 0x1F);
                        break;
                    case 3:
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vx    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vy    = 0x10 - ((gRandomLcgState >> 0x10) & 0x1F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vz    = 0x10 - ((gRandomLcgState >> 0x10) & 0x1F);
                        gte_lddp((mem->scale << 2));
                        gte_ldsv(&mem->move);
                        gte_gpf12();
                        gte_stsv(&mem->move);
                        break;
                }
            }
            if (arg0->spawnArg1.value & 0x30000000) {
                n = gDisplayState.animFrame & 3;
                for (i = 0; i < n; i++) {
                    step = mem->scale - (mem->scale >> 2);
                    mask = (arg0->spawnArg1.value & 0xC0000000) | 0x6002000;
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, step | mask, 0);
                }
                n = gDisplayState.animFrame % 3;
                for (i = 0; i < n; i++) {
                    step2 = mem->scale - (mem->scale >> 2);
                    mask2 = (arg0->spawnArg1.value & 0xC0000000) | 0x4003000;
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, step2 | mask2, 0);
                }
            }
            arg0->state = 1;
        }
        block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
        block->worldPoint.vx = coord->workm.t[0];
        block->worldPoint.vy = coord->workm.t[1];
        block->worldPoint.vz = coord->workm.t[2];
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
            setlen(prim, 9);
            setcode(prim, 0x2F);
            if (arg0->spawnArg1.value & 0xC0000000) {
                prim->tpage = ((((u32)arg0->spawnArg1.value >> 30) - 1) & 3) << 5 | 8;
            } else {
                prim->tpage = 0x28;
            }
            prim->clut             = 0x4253;
            prim->u0               = (mem->age / mem->period) << 5;
            prim->v0               = 0x18;
            prim->u1               = ((mem->age / mem->period) << 5) + 0x1F;
            prim->v1               = 0x18;
            prim->u2               = (mem->age / mem->period) << 5;
            prim->v2               = 0x37;
            prim->u3               = ((mem->age / mem->period) << 5) + 0x1F;
            prim->v3               = 0x37;
            block->extent.corner.x = (((mem->scale * 0x1F) / block->depth) * rsin(mem->angle)) >> 12;
            block->extent.corner.y = (((mem->scale * 0x1F) / block->depth) * rcos(mem->angle)) >> 12;
            prim->x0               = block->screenX + block->extent.corner.x;
            prim->x3               = block->screenX - block->extent.corner.x;
            prim->y0               = block->screenY - block->extent.corner.y;
            prim->y3               = block->screenY + block->extent.corner.y;
            block->extent.corner.x = (((mem->scale * 0x1F) / block->depth) * rsin(mem->angle + 0x400)) >> 12;
            block->extent.corner.y = (((mem->scale * 0x1F) / block->depth) * rcos(mem->angle + 0x400)) >> 12;
            prim->x1               = block->screenX + block->extent.corner.x;
            prim->x2               = block->screenX - block->extent.corner.x;
            prim->y1               = block->screenY - block->extent.corner.y;
            prim->y2               = block->screenY + block->extent.corner.y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
        if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
            return;
        }
        coord->coord.t[0]  += mem->move.vx;
        coord->coord.t[1]  += mem->move.vy;
        coord->coord.t[2]  += mem->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        mem->age++;
        if (mem->age <= mem->period * 8 - 1) {
            return;
        }
    }
    effectKillTask(mem, arg0);
}

void Gp_EffSprTask76(Task* arg0)
{
    EffectShapeScratch* block;
    GfxCoord*           coord;
    EffectWork*         mem;
    POLY_FT4*           prim;
    u16                 uvSpan;
    s16                 scale;
    s32                 rng;

    coord = arg0->extra.coordBody->coord;
    block = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    mem   = arg0->spawnArg2.pointer;
    actorRenderComposeCoord(coord);
    block->worldPoint.vx = coord->workm.t[0];
    block->worldPoint.vy = coord->workm.t[1];
    block->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth   = block->depth + 1;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        uvSpan = Gp_EffSprRecs[mem->age].uvSpan;
        if (arg0->state == 0) {
            if (arg0->spawnArg1.value & 0xFFF) {
                scale = arg0->spawnArg1.halves.low & 0xFFF;
            } else {
                scale = 0x200;
            }
            mem->scale      = scale;
            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rng;
            mem->angle      = ((u32)rng >> 16) & 0xFFF;
            arg0->state     = 1;
        }
        prim->code            |= 3;
        prim->tpage            = getTPage(0, 1, Gp_EffSprRecs[mem->age].tpageX, 0);
        prim->clut             = getClut(Gp_EffSprRecs[mem->age].clutX, Gp_EffSprRecs[mem->age].clutY);
        prim->u0               = Gp_EffSprRecs[mem->age].u;
        prim->v0               = Gp_EffSprRecs[mem->age].v;
        prim->u1               = Gp_EffSprRecs[mem->age].u + uvSpan;
        prim->v1               = Gp_EffSprRecs[mem->age].v;
        prim->u2               = Gp_EffSprRecs[mem->age].u;
        prim->v2               = Gp_EffSprRecs[mem->age].v + uvSpan;
        prim->u3               = Gp_EffSprRecs[mem->age].u + uvSpan;
        prim->v3               = Gp_EffSprRecs[mem->age].v + uvSpan;
        block->extent.corner.x = ((((s16)uvSpan * mem->scale) / block->depth) * rsin(mem->angle)) >> 12;
        block->extent.corner.y = ((((s16)uvSpan * mem->scale) / block->depth) * rcos(mem->angle)) >> 12;
        prim->x0               = block->screenX + (u16)block->extent.corner.x;
        prim->x3               = block->screenX - (u16)block->extent.corner.x;
        prim->y0               = block->screenY - (u16)block->extent.corner.y;
        prim->y3               = block->screenY + (u16)block->extent.corner.y;
        block->extent.corner.x = ((((s16)uvSpan * mem->scale) / block->depth) * rsin(mem->angle + 0x400)) >> 12;
        block->extent.corner.y = ((((s16)uvSpan * mem->scale) / block->depth) * rcos(mem->angle + 0x400)) >> 12;
        prim->x1               = block->screenX + (u16)block->extent.corner.x;
        prim->x2               = block->screenX - (u16)block->extent.corner.x;
        prim->y1               = block->screenY - (u16)block->extent.corner.y;
        prim->y2               = block->screenY + (u16)block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
    mem->age++;
    if (mem->age >= 4) {
        effectKillTask(mem, arg0);
    }
}

void Gp_EffSprTask7C(Task* arg0)
{
    GfxCoord            hit;
    EffectWork*         mem;
    GfxCoord*           coord;
    EffectShapeScratch* head;
    EffectShapeScratch* projectionScratch;
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    s16                 flag;
    s32                 rng;
    s16                 scale;
    s16                 step;
    s32                 col;
    s32                 tmp;
    u32                 param;
    u16                 vz;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    param = 0x80;
    if (flag >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto release;
    }
    if (mem->index == 0) {
        scale = 0x200;
        if (arg0->spawnArg1.value & 0xFFF) {
            scale = arg0->spawnArg1.halves.low & 0xFFF;
        }
        mem->scale      = scale;
        rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->angle      = ((u32)rng >> 16) & 0xFFF;
        gRandomLcgState = rng;
        if (arg0->spawnArg1.value & 0xF000) {
            step = (arg0->spawnArg1.value >> 12) & 0xF;
        } else {
            step = 1;
        }
        mem->period     = step;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->step       = 0x100 - ((gRandomLcgState >> 16) & 0x1F0);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->move.vx    = 0x40 - ((gRandomLcgState >> 16) & 0x7F);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->move.vy    = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->move.vz    = 0x40 - ((gRandomLcgState >> 16) & 0x7F);
        mem->index++;
    }
    actorRenderComposeCoord(coord);
    head                                     = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    projectionScratch                        = head - 1;
    projectionScratch->worldPoint.vx         = (u16)coord->workm.t[0];
    block                                    = projectionScratch;
    block->worldPoint.vy                     = (u16)coord->workm.t[1];
    vz                                       = (u16)coord->workm.t[2];
    SCRATCH_STACK_CURSOR(EffectShapeScratch) = block;
    block->worldPoint.vz                     = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projectionScratch->worldPoint);
    gte_rtps();
    gte_stsxy(&(head - 1)->screenX);
    gte_stflg(&(head - 1)->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&(head - 1)->depth);
        block->depth   = block->depth + 1;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        if (mem->age >= 0x18) {
            col = (0x1F - mem->age) * 16;
            __asm__ volatile("" : "=r"(tmp) : "0"(col));
            param    = (u8)tmp;
            prim->r0 = col;
            prim->g0 = col;
            prim->b0 = col;
        } else {
            setcode(prim, 0x2D);
        }
        prim->tpage            = 0x28;
        prim->code            |= 2;
        prim->clut             = 0x428A;
        prim->u0               = ((mem->age / mem->period) % 6) * 16;
        prim->v0               = 0x58;
        prim->u1               = ((mem->age / mem->period) % 6) * 16 + 0xF;
        prim->v1               = 0x58;
        prim->u2               = ((mem->age / mem->period) % 6) * 16;
        prim->v2               = 0x67;
        prim->u3               = ((mem->age / mem->period) % 6) * 16 + 0xF;
        prim->v3               = 0x67;
        block->extent.corner.x = (((mem->scale * 15) / block->depth) * rsin(mem->angle)) >> 12;
        block->extent.corner.y = (((mem->scale * 15) / block->depth) * rcos(mem->angle)) >> 12;
        prim->x0               = block->screenX + (u16)block->extent.corner.x;
        prim->x3               = block->screenX - (u16)block->extent.corner.x;
        prim->y0               = block->screenY - (u16)block->extent.corner.y;
        prim->y3               = block->screenY + (u16)block->extent.corner.y;
        block->extent.corner.x = (((mem->scale * 15) / block->depth) * rsin(mem->angle + 0x400)) >> 12;
        block->extent.corner.y = (((mem->scale * 15) / block->depth) * rcos(mem->angle + 0x400)) >> 12;
        prim->x1               = block->screenX + (u16)block->extent.corner.x;
        prim->x2               = block->screenX - (u16)block->extent.corner.x;
        prim->y1               = block->screenY - (u16)block->extent.corner.y;
        prim->y2               = block->screenY + (u16)block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        return;
    }
    coord->coord.t[0]  += mem->move.vx;
    coord->coord.t[1]  += mem->move.vy;
    coord->coord.t[2]  += mem->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    mem->move.vy += 5;
    mem->angle   += mem->step;
    mem->age++;
    if (mem->age >= 0x1F) {
    release:
        effectKillTask(mem, arg0);
        return;
    }
    if (Gp_TraceGroundCoord(coord, &hit) == 1) {
        Gp_DrawEffSprite7C(&hit, mem->scale >> 1, param);
    }
    if (coord->coord.t[1] > hit.coord.t[1]) {
        coord->coord.t[1] -= mem->move.vy * 2;
        mem->move.vy       = -(mem->move.vy >> 1);
        mem->move.vx       = mem->move.vx >> 1;
        mem->move.vz       = mem->move.vz >> 1;
    }
}

void func_800F4308(Task* arg0)
{
    u8                             rgb[3];
    EffectWork*                    mem;
    GfxCoord*                      coord;
    GfxCoord*                      lightCoord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          slot;
    ModelObjectCoordBody*          body;
    SVECTOR*                       vec;
    s16                            flag;
    s32                            scale11;
    s32                            scale12;
    s32                            count;
    s32                            cond;
    s32                            condInc;
    s32                            i;
    s32                            t2_10;
    s32                            t2_11;
    s32                            t2_12;
    s32                            rng;
    s32                            tmp;

    lightSlot  = &gWorldCoordTransientPointLights[1];
    slot       = &lightSlot->light;
    lightCoord = &slot->head.transform.coord;
    body       = arg0->extra.coordBody;
    mem        = arg0->spawnArg2.pointer;
    flag       = gRoomEffectState->effectControl;
    coord      = body->coord;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        cond = flag < ROOM_EFFECT_CONTROL_CANCEL_MIN;
        goto release;
    }
    actorRenderComposeCoord(coord);
    mem->age = mem->age + 1;
    switch (arg0->spawnArg1.value) {
        case 10:
            switch (arg0->state) {
                case 0:
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vx    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vy    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vz    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    vec             = &mem->move;
                    Gp_SpawnEff(EFFECT_EXPLOSION, coord, 0x600, vec);
                    rng = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_BOUNCING_SPARK, coord, (((u32)rng >> 16) & 0x3F) | 0x100, vec);
                    rng = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_BOUNCING_SPARK, coord, (((u32)rng >> 16) & 0x3F) | 0x100, vec);
                    arg0->state++;
                    break;
                case 1:
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vx    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vy    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vz    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    rng = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, (((u32)rng >> 16) & 0x1FF) | 0xD0000400,
                                &mem->move);
                    if (mem->age >= 7) {
                        arg0->state++;
                    }
                    break;
                case 2:
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vx    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vy    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vz    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    rng = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, (((u32)rng >> 16) & 0xFF) | 0x82003400,
                                &mem->move);
                    if (mem->age >= 0xB) {
                        arg0->state++;
                    }
                    break;
            }
            lightSlot->framesLeft    = 0x10;
            count                    = mem->age;
            slot->outer              = 0x2580;
            slot->head.color.r       = 0x1000;
            slot->head.color.g       = 0xC00;
            slot->head.color.b       = 0x800;
            slot->inner              = (0x898 - (count * 0x64)) * 4;
            lightCoord->coord.t[0]   = coord->coord.t[0];
            lightCoord->coord.t[1]   = coord->coord.t[1];
            t2_10                    = coord->coord.t[2];
            lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            lightCoord->coord.t[2]   = t2_10;
            cond                     = mem->age < 0x15;
            goto release;
        case 11:
            switch (arg0->state) {
                case 0:
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vx    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vy    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vz    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    Gp_SpawnEff(EFFECT_IMPACT_FLASH, coord, 0x500, &mem->move);
                    condInc = mem->age < 2;
                    goto maybe11;
                case 1:
                    i = 0;
                    do {
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vx    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vy    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vz    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                        rng = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, (((u32)rng >> 16) & 0x1FF) | 0x82004400,
                                    &mem->move);
                        i += 1;
                    } while (i < 2);
                    condInc = mem->age < 9;
                    goto maybe11;
                case 2:
                    i = 0;
                    do {
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vx    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vy    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        mem->move.vz    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                        rng = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, (((u32)rng >> 16) & 0xFF) | 0xD0000400,
                                    &mem->move);
                        i += 1;
                    } while (i < 2);
                    condInc = mem->age < 0xD;
                maybe11:
                    if (condInc != 0) {
                        break;
                    }
                    arg0->state += 1;
                    break;
            }
            if (mem->scale++ < 8) {
                tmp     = -0x80 - (mem->scale << 4);
                rgb[0]  = tmp;
                rgb[1]  = (rgb[0] * 3) >> 2;
                rgb[2]  = (rgb[0] * 2) / 3;
                scale11 = (mem->scale << 6) + 0x40;
                Gp_DrawArc(coord, (s16)scale11, (s16)scale11, rgb);
            }
            if (mem->age < 4) {
                i = 0;
                do {
                    Gp_SpawnEff(EFFECT_SPARK_STREAK, coord, 0, 0);
                    i += 1;
                } while (i < 3);
            }
            lightSlot->framesLeft    = 0x10;
            count                    = mem->age;
            slot->outer              = 0x2580;
            slot->head.color.r       = 0xC00;
            slot->head.color.g       = 0xC00;
            slot->head.color.b       = 0x800;
            slot->inner              = (0x898 - (count * 0x64)) * 4;
            lightCoord->coord.t[0]   = coord->coord.t[0];
            lightCoord->coord.t[1]   = coord->coord.t[1];
            t2_11                    = coord->coord.t[2];
            lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            lightCoord->coord.t[2]   = t2_11;
            cond                     = mem->age < 0x15;
            goto release;
        case 12:
            if (arg0->state != 0) {
                if (arg0->state == 1) {
                    goto case12_1;
                }
                goto skip12;
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vx    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vy    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vz    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
            Gp_SpawnEff(EFFECT_IMPACT_FLASH, coord, 0x500, &mem->move);
            condInc = mem->age < 2;
            goto maybe12;
        case12_1:
            i = 0;
            do {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vx    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vy    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->move.vz    = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                rng = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, (((u32)rng >> 16) & 0x1FF) | 0x82004400,
                            &mem->move);
                i += 1;
            } while (i < 2);
            condInc = mem->age < 9;
        maybe12:
            if (condInc == 0) {
                arg0->state += 1;
            }
        skip12:
            if (mem->scale++ < 8) {
                rgb[2]  = (-0x80 - (mem->scale << 4)) * 2;
                rgb[1]  = (rgb[2] & 0xE0) >> 2;
                rgb[0]  = rgb[1];
                scale12 = (mem->scale << 6) + 0x40;
                Gp_DrawArc(coord, (s16)scale12, (s16)scale12, rgb);
                if (gDisplayState.animFrame & 1) {
                    rgb[2] = ~(mem->scale * 0x1F);
                    rgb[1] = rgb[2] >> 2;
                    rgb[0] = rgb[1];
                    Gp_DrawFadeQuad(rgb, 1);
                }
            }
            lightSlot->framesLeft    = 0x10;
            count                    = mem->age;
            slot->outer              = 0x2580;
            slot->head.color.r       = 0x800;
            slot->head.color.g       = 0xC00;
            slot->head.color.b       = 0x1000;
            slot->inner              = (0x898 - (count * 0x64)) * 4;
            lightCoord->coord.t[0]   = coord->coord.t[0];
            lightCoord->coord.t[1]   = coord->coord.t[1];
            t2_12                    = coord->coord.t[2];
            lightCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            lightCoord->coord.t[2]   = t2_12;
            cond                     = mem->age < 0x15;
            goto release;
        default:
            return;
    }
release:
    if (cond == 0) {
        effectKillTask(mem, arg0);
    }
}

void Gp_EffLineTask92(Task* arg0)
{
    EffectLineScratch* block;
    EffectWork*        mem;
    GfxCoord*          coord;
    MATRIX*            m;
    LINE_F2*           prim;
    s16                step;
    s32                rng;
    s32                one;
    s16                val;

    block = SCRATCH_STACK_RESERVE_BLOCK(EffectLineScratch);
    coord = arg0->extra.coordBody->coord;
    mem   = arg0->spawnArg2.pointer;
    actorRenderComposeCoord(coord);
    if (mem->age == 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->move.vx    = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->move.vy    = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->move.vz    = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        step            = 2;
        if (((gRandomLcgState >> 16) & 3) != 0) {
            step = 1;
        }
        rng                  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        one                  = ONE;
        m                    = &coord->coord;
        mem->angle           = (((u32)rng >> 16) & 1) + 1;
        mem->scale           = step;
        *(s32*)&coord->coord = one;
        MATRIX_PAIR(m, 0, 2) = 0;
        MATRIX_PAIR(m, 1, 1) = one;
        MATRIX_PAIR(m, 2, 0) = 0;
        m->m[2][2]           = one;
        mem->pos.vx          = (u16)coord->workm.t[0];
        mem->pos.vy          = (u16)coord->workm.t[1];
        mem->pos.vz          = (u16)coord->workm.t[2];
        gRandomLcgState      = rng;
    }
    coord->coord.t[0]     += mem->move.vx;
    coord->coord.t[1]     += mem->move.vy;
    coord->coord.t[2]     += mem->move.vz;
    coord->composeStamp    = GRAPHICS_COORD_DIRTY;
    block->endpoints[0].vx = coord->workm.t[0];
    block->endpoints[0].vy = coord->workm.t[1];
    block->endpoints[0].vz = coord->workm.t[2];
    block->endpoints[1].vx = mem->pos.vx;
    block->endpoints[1].vy = mem->pos.vy;
    block->endpoints[1].vz = mem->pos.vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->endpoints[0]);
    gte_rtps();
    gte_stsxy(&block->screenEndpoints[0]);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_ldv0(&block->endpoints[1]);
        gte_rtps();
        gte_stsxy(&block->screenEndpoints[1]);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->depth);
            prim           = gGpuPrimCursor;
            block->depth   = block->depth + 1;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 3);
            setcode(prim, 0x40);
            val = 0xFF - (mem->age << (6 - mem->scale));
            if (arg0->spawnArg1.value != 0) {
                prim->r0 = val >> 3;
                prim->g0 = val >> mem->angle;
                prim->b0 = val;
            } else {
                prim->r0 = val;
                prim->g0 = val >> mem->angle;
                prim->b0 = val >> 3;
            }
            prim->x0 = block->screenEndpoints[0].vx;
            prim->y0 = block->screenEndpoints[0].vy;
            prim->x1 = block->screenEndpoints[1].vx;
            prim->y1 = block->screenEndpoints[1].vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
            mem->pos.vx = (u16)coord->workm.t[0];
            mem->pos.vy = (u16)coord->workm.t[1];
            mem->pos.vz = (u16)coord->workm.t[2];
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectLineScratch);
    mem->age++;
    if (mem->age > mem->scale * 8 - 1) {
        effectKillTask(mem, arg0);
    }
}

void Gp_EffPolyTask9C(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s16         flag;

    mem   = arg0->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        if (flag < ROOM_EFFECT_CONTROL_HIDDEN) {
            if (arg0->state == 0) {
                mem->scale  = 0x10;
                mem->angle  = 0x20;
                mem->period = D_8011291C[arg0->spawnArg1.value].color;
                mem->step   = D_8011291C[arg0->spawnArg1.value].radiusStep;
                arg0->state++;
            }
            actorRenderComposeCoord(coord);
            mem->scale -= 2;
            mem->angle += mem->step;
            Gp_DrawEffShard(coord, mem->angle, mem->scale, mem->period);
            mem->age++;
            if (mem->age < 8) {
                return;
            }
        } else {
            return;
        }
    }
    effectKillTask(mem, arg0);
}

static void Gp_DrawEffShard(GfxCoord* arg0, s16 arg1, s16 arg2, u16 arg3)
{
    EffectShapeScratch* block;
    POLY_G4*            quad;
    POLY_G3*            tri;
    s32                 ang;
    s32                 ang2;
    s32                 rng;
    s32                 base;
    u8                  r;
    u8                  g;
    u8                  b;

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
        // Sweep the outer and inner radii into a shaded ring.
        block->extent.burst.outer = (arg1 << 8) / block->depth;
        block->extent.burst.inner = (arg1 << 7) / block->depth;
        r                         = arg2 * ((arg3 >> 8) & 0xF);
        g                         = arg2 * ((arg3 >> 4) & 0xF);
        b                         = arg2 * (arg3 & 0xF);
        ang                       = 0;
        do {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setlen(quad, 8);
            setcode(quad, 0x38);
            quad->r0 = r;
            quad->r1 = r;
            quad->g0 = g;
            quad->b0 = b;
            quad->g1 = g;
            quad->b1 = b;
            quad->r2 = 0;
            quad->g2 = 0;
            quad->b2 = 0;
            quad->r3 = 0;
            quad->g3 = 0;
            quad->b3 = 0;
            quad->x0 = block->screenX + ((block->extent.burst.outer * rsin(ang)) >> 12);
            ang2     = ang + 0x100;
            quad->y0 = block->screenY + ((block->extent.burst.outer * rcos(ang)) >> 12);
            quad->x1 = block->screenX + ((block->extent.burst.outer * rsin(ang2)) >> 12);
            quad->y1 = block->screenY + ((block->extent.burst.outer * rcos(ang2)) >> 12);
            quad->x2 = block->screenX + ((block->extent.burst.inner * rsin(ang)) >> 12);
            quad->y2 = block->screenY + ((block->extent.burst.inner * rcos(ang)) >> 12);
            quad->x3 = block->screenX + ((block->extent.burst.inner * rsin(ang2)) >> 12);
            quad->y3 = block->screenY + ((block->extent.burst.inner * rcos(ang2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            ang = ang2;
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, block->depth);
        } while (ang < 0x1000);
        if (arg3 & 0x1000) {
            // Reuse the sizing words for the optional radial spikes.
            block->extent.spike.radius    = 0x12000 / block->depth;
            block->extent.spike.halfWidth = 0x2400 / block->depth;
            ang                           = 0;
            do {
                rng = gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                tri                   = gGpuPrimCursor;
                gGpuPrimCursor        = tri + 1;
                setlen(tri, 6);
                setcode(tri, 0x30);
                tri->r0 = r;
                tri->g0 = g;
                tri->b0 = b;
                tri->r1 = 0;
                tri->g1 = 0;
                tri->b1 = 0;
                tri->r2 = 0;
                tri->g2 = 0;
                tri->b2 = 0;
                base    = ang + (((u32)rng >> 16) & 0x300);
                tri->x0 = block->screenX;
                tri->y0 = block->screenY;
                tri->x1 = block->screenX + ((block->extent.spike.radius * rsin(base)) >> 12) +
                          ((block->extent.spike.halfWidth * rsin(base + 0xC00)) >> 12);
                tri->y1 = block->screenY + ((block->extent.spike.radius * rcos(base)) >> 12) +
                          ((block->extent.spike.halfWidth * rcos(base + 0xC00)) >> 12);
                tri->x2 = block->screenX + ((block->extent.spike.radius * rsin(base)) >> 12) +
                          ((block->extent.spike.halfWidth * rsin(base + 0x400)) >> 12);
                tri->y2 = block->screenY + ((block->extent.spike.radius * rcos(base)) >> 12) +
                          ((block->extent.spike.halfWidth * rcos(base + 0x400)) >> 12);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        tri);
                ang += 0x400;
                gpuSetPrimitiveBlendMode(tri, GPU_BLEND_ADD, block->depth);
            } while (ang < 0x1000);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

void Gp_EffSprTask9E(Task* arg0)
{
    EffectQuadScratch* quadScratch;
    s32                i;
    EffectWork*        mem;
    GfxCoord*          coord;
    POLY_FT4*          prim;
    s32                scale;
    s32                shade;
    u8                 col;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (arg0->state == 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gfxRotMatrixY(&coord->coord, (gRandomLcgState >> 16) & 0xFFF, 1);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        if (arg0->spawnArg1.value & 0xFFF) {
            scale = arg0->spawnArg1.halves.low & 0xFFF;
        } else {
            scale = 0x400;
        }
        mem->scale  = scale;
        mem->angle  = 0x3FF;
        arg0->state = 1;
    }
    actorRenderComposeCoord(coord);

    quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadScratch);
    for (i = 0; i < ARRAY_SIZE(D_80111E38); i++) {
        quadScratch->vertices[i].vx = (u16)D_80111E38[i].axis0Sign * (u16)mem->scale;
        quadScratch->vertices[i].vy = 0;
        quadScratch->vertices[i].vz = (u16)D_80111E38[i].axis1Sign * (u16)mem->scale;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&quadScratch->vertices[i]);
        gte_rtv0();
        gte_stsv(&quadScratch->vertices[i]);
        quadScratch->vertices[i].vx += coord->workm.t[0];
        quadScratch->vertices[i].vy += coord->workm.t[1];
        quadScratch->vertices[i].vz += coord->workm.t[2];
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    shade = -0x80 - (mem->age >> 3);
    col   = shade;
    gte_stsxy(&quadScratch->screenCorners[0]);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&quadScratch->screenCorners[1], &quadScratch->screenCorners[2], &quadScratch->screenCorners[3]);
    gte_stflg(&quadScratch->projectionFlags);
    if (quadScratch->projectionFlags >= 0) {
        gte_stszotz(&quadScratch->depth);
        quadScratch->depth += 0x80;
        prim                = gGpuPrimCursor;
        gGpuPrimCursor      = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->g0    = col >> 2;
        prim->b0    = col >> 2;
        prim->tpage = 0x29;
        prim->r0    = shade;
        prim->clut  = 0x428C;
        prim->u0    = 0xA8;
        prim->v0    = 0xC8;
        prim->u1    = 0xDF;
        prim->v1    = 0xC8;
        prim->u2    = 0xA8;
        prim->v2    = 0xFF;
        prim->u3    = 0xDF;
        prim->v3    = 0xFF;
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
    mem->age++;
    if (mem->angle < mem->age) {
        effectKillTask(mem, arg0);
    }
}

void Gp_EffSprTask54(Task* arg0)
{
    EffectShapeScratch* head;
    EffectShapeScratch* block;
    EffectShapeScratch* projectionScratch;
    s16                 count;
    s16                 step;
    u16                 vz;
    EffectWork*         mem;
    GfxCoord*           coord;
    POLY_FT4*           prim;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(mem, arg0);
        }
        return;
    }

    actorRenderComposeCoord(coord);
    if (arg0->state == 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->scale      = arg0->spawnArg1.halves.low & 0xFFF;
        mem->angle      = (gRandomLcgState >> 16) & 0xFFF;
        if (arg0->spawnArg1.value & 0xF000) {
            count = (arg0->spawnArg1.value >> 12) & 0xF;
        } else {
            count = 1;
        }
        mem->period     = count;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->move.vx    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->move.vy    = -((gRandomLcgState >> 16) & 0xF);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->move.vz    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
        if (arg0->spawnArg1.value < 0) {
            if (mem->angle & 1) {
                Gp_SpawnEff(EFFECT_DUST_PUFF, coord, ((mem->scale * 3) >> 2) + 0x3000, NULL);
            }
            if (!(mem->angle & 3)) {
                Gp_SpawnEff(EFFECT_DUST_PUFF, coord, ((mem->scale * 3) >> 2) + 0x3000, NULL);
            }
        }
        arg0->state = 1;
    }

    head                                     = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    (head - 1)->worldPoint.vx                = (u16)coord->workm.t[0];
    block                                    = head - 1;
    block->worldPoint.vy                     = (u16)coord->workm.t[1];
    vz                                       = (u16)coord->workm.t[2];
    SCRATCH_STACK_CURSOR(EffectShapeScratch) = block;
    block->worldPoint.vz                     = vz;
    projectionScratch                        = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projectionScratch->worldPoint);
    gte_rtps();
    gte_stsxy(&(head - 1)->screenX);
    gte_stflg(&(head - 1)->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&(head - 1)->depth);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        prim->r0    = 0x68;
        prim->g0    = 0x70;
        prim->b0    = 0x38;
        prim->tpage = 0x28;
        setSemiTrans(prim, 1);
        prim->clut = 0x4253;
        prim->u0   = (mem->age / mem->period) << 5;
        prim->v0   = 0x18;
        prim->u1   = ((mem->age / mem->period) << 5) + 0x1F;
        prim->v1   = 0x18;
        prim->u2   = (mem->age / mem->period) << 5;
        prim->v2   = 0x37;
        prim->u3   = ((mem->age / mem->period) << 5) + 0x1F;
        prim->v3   = 0x37;

        block->extent.corner.x = ((((s32)mem->scale * 31) / block->depth) * rsin(mem->angle)) >> 12;
        block->extent.corner.y = ((((s32)mem->scale * 31) / block->depth) * rcos(mem->angle)) >> 12;
        prim->x0               = block->screenX + (u16)block->extent.corner.x;
        prim->x3               = block->screenX - (u16)block->extent.corner.x;
        prim->y0               = block->screenY - (u16)block->extent.corner.y;
        prim->y3               = block->screenY + (u16)block->extent.corner.y;
        block->extent.corner.x = ((((s32)mem->scale * 31) / block->depth) * rsin(mem->angle + 0x400)) >> 12;
        block->extent.corner.y = ((((s32)mem->scale * 31) / block->depth) * rcos(mem->angle + 0x400)) >> 12;
        prim->x1               = block->screenX + (u16)block->extent.corner.x;
        prim->x2               = block->screenX - (u16)block->extent.corner.x;
        prim->y1               = block->screenY - (u16)block->extent.corner.y;
        prim->y2               = block->screenY + (u16)block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        coord->coord.t[0]  += mem->move.vx;
        coord->coord.t[1]  += mem->move.vy;
        coord->coord.t[2]  += mem->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        step                = mem->age + 1;
        mem->age            = step;
        if (step > (mem->period * 8) - 1) {
            effectKillTask(mem, arg0);
        }
    }
}

void Gp_DrawEffSprite7C(GfxCoord* arg0, s32 arg1, u32 arg2)
{
    EffectQuadScratch* quadScratch;
    s32                i;
    POLY_FT4*          prim;

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
        setcode(prim, 0x2C);
        prim->r0    = arg2 >> 1;
        prim->tpage = 0x29;
        prim->clut  = 0x430F;
        prim->u0    = 0xE0;
        prim->v0    = 0xC8;
        prim->v1    = 0xC8;
        prim->u2    = 0xE0;
        prim->g0    = arg2;
        prim->b0    = arg2;
        prim->u1    = 0xFF;
        prim->v2    = 0xE7;
        prim->u3    = 0xFF;
        prim->v3    = 0xE7;
        setSemiTrans(prim, 1);
        prim->x0 = quadScratch->screenCorners[0].vx;
        prim->y0 = quadScratch->screenCorners[0].vy;
        prim->x1 = quadScratch->screenCorners[1].vx;
        prim->y1 = quadScratch->screenCorners[1].vy;
        prim->x2 = quadScratch->screenCorners[2].vx;
        prim->y2 = quadScratch->screenCorners[2].vy;
        prim->x3 = quadScratch->screenCorners[3].vx;
        prim->y3 = quadScratch->screenCorners[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadScratch);
}

void Gp_DrawEffGroundQuad(VECTOR3* pos, s32 size, s16 shade)
{
    EffectQuadScratch* quadScratch;
    s32                i;
    POLY_FT4*          prim;

    if (shade >= 0 && gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
        quadScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectQuadScratch);
        gte_SetTransMatrix(&GsWSMATRIX);
        for (i = 0; i < ARRAY_SIZE(D_80111E38); i++) {
            quadScratch->vertices[i].vx = (u16)D_80111E38[i].axis0Sign * size;
            quadScratch->vertices[i].vy = 0;
            quadScratch->vertices[i].vz = (u16)D_80111E38[i].axis1Sign * size;
            gte_SetRotMatrix(&gGfxViewCoord.workm);
            gte_ldv0(&quadScratch->vertices[i]);
            gte_rtv0();
            gte_stsv(&quadScratch->vertices[i]);
            quadScratch->vertices[i].vx += pos->vx;
            quadScratch->vertices[i].vy += pos->vy;
            quadScratch->vertices[i].vz += pos->vz;
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
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2C);
            if (shade == 0) {
                setcode(prim, 0x2D);
            } else {
                prim->r0 = shade;
                prim->g0 = shade;
                prim->b0 = shade;
            }
            prim->tpage = 0x48;
            prim->clut  = 0x4283;
            prim->u0    = 0xC0;
            prim->v0    = 0x98;
            prim->v1    = 0x98;
            prim->u2    = 0xC0;
            prim->u1    = 0xF7;
            prim->v2    = 0xCF;
            prim->u3    = 0xF7;
            prim->v3    = 0xCF;
            setSemiTrans(prim, 1);
            prim->x0 = quadScratch->screenCorners[0].vx;
            prim->y0 = quadScratch->screenCorners[0].vy;
            prim->x1 = quadScratch->screenCorners[1].vx;
            prim->y1 = quadScratch->screenCorners[1].vy;
            prim->x2 = quadScratch->screenCorners[2].vx;
            prim->y2 = quadScratch->screenCorners[2].vy;
            prim->x3 = quadScratch->screenCorners[3].vx;
            prim->y3 = quadScratch->screenCorners[3].vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectQuadScratch);
    }
}

void Gp_EffSprTask53(Task* arg0)
{
    VECTOR3   vec;
    Task*     slot;
    GfxCoord* coord;
    GfxCoord* parent;

    slot  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    coord = arg0->extra.coordBody->coord;
    if (slot != NULL) {
        if (arg0->state == 0) {
            parent              = slot->extra.tmd->coords;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->parent       = parent + 1;
            actorRenderComposeCoord(coord);
            arg0->state = 1;
        } else if (gRoomEffectState->groundShadowShade >= 0) {
            if (!(slot->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
                actorRenderComposeCoord(coord);
                if ((s16)func_800EA1A8(MATRIX_TRANS(&coord->workm), &vec) != 0) {
                    Gp_DrawEffGroundQuad(&vec, 0x1C0, gRoomEffectState->groundShadowShade);
                }
            }
        }
    }
}

TmdBone D_80111ED0[1] = {
#include "assets/gameplay_effect_80111fc8_rest_pose.inc"
};
u32 D_80111EF4[1] = {
#include "assets/gameplay_effect_80111fc8_part_vertices.inc"
};
SVECTOR D_80111EF8[6] = {
#include "assets/gameplay_effect_80111fc8_vertices.inc"
};
SVECTOR D_80111F28[4] = {
#include "assets/gameplay_effect_80111fc8_normals.inc"
};
u32 D_80111F48[32] = {
#include "assets/gameplay_effect_80111fc8_packets.inc"
};
TmdSource D_80111FC8    = { 0, 196, 0, 1, D_80111EF4, D_80111EF8, D_80111F28, D_80111ED0, D_80111F48 };
TmdBone   D_80111FEC[1] = {
#include "assets/gameplay_effect_80111fc8_rest_pose.inc"
};
u32 D_80112010[1] = {
#include "assets/gameplay_effect_80111fc8_part_vertices.inc"
};
SVECTOR D_80112014[6] = {
#include "assets/gameplay_effect_801120e4_vertices.inc"
};
SVECTOR D_80112044[4] = {
#include "assets/gameplay_effect_801120e4_normals.inc"
};
u32 D_80112064[32] = {
#include "assets/gameplay_effect_801120e4_packets.inc"
};
TmdSource D_801120E4    = { 0, 196, 0, 1, D_80112010, D_80112014, D_80112044, D_80111FEC, D_80112064 };
TmdBone   D_80112108[1] = {
#include "assets/gameplay_effect_80111fc8_rest_pose.inc"
};
u32 D_8011212C[1] = {
#include "assets/gameplay_effect_80111fc8_part_vertices.inc"
};
SVECTOR D_80112130[6] = {
#include "assets/gameplay_effect_80112200_vertices.inc"
};
SVECTOR D_80112160[4] = {
#include "assets/gameplay_effect_80112200_normals.inc"
};
u32 D_80112180[32] = {
#include "assets/gameplay_effect_80112200_packets.inc"
};
TmdSource D_80112200    = { 0, 196, 0, 1, D_8011212C, D_80112130, D_80112160, D_80112108, D_80112180 };
TmdBone   D_80112224[1] = {
#include "assets/gameplay_effect_80111fc8_rest_pose.inc"
};
u32 D_80112248[1] = {
#include "assets/gameplay_effect_80111fc8_part_vertices.inc"
};
SVECTOR D_8011224C[6] = {
#include "assets/gameplay_effect_8011231c_vertices.inc"
};
SVECTOR D_8011227C[4] = {
#include "assets/gameplay_effect_8011231c_normals.inc"
};
u32 D_8011229C[32] = {
#include "assets/gameplay_effect_8011231c_packets.inc"
};
TmdSource D_8011231C    = { 0, 196, 0, 1, D_80112248, D_8011224C, D_8011227C, D_80112224, D_8011229C };
TmdBone   D_80112340[1] = {
#include "assets/gameplay_effect_80111fc8_rest_pose.inc"
};
u32 D_80112364[1] = {
#include "assets/gameplay_effect_801124b8_part_vertices.inc"
};
SVECTOR D_80112368[8] = {
#include "assets/gameplay_effect_801124b8_vertices.inc"
};
SVECTOR D_801123A8[10] = {
#include "assets/gameplay_effect_801124b8_normals.inc"
};
u32 D_801123F8[48] = {
#include "assets/gameplay_effect_801124b8_packets.inc"
};
TmdSource D_801124B8     = { 0, 312, 0, 1, D_80112364, D_80112368, D_801123A8, D_80112340, D_801123F8 };
SVECTOR   D_801124DC[34] = {
    { 0, 0, 0, 0 },
    { 0, 384, 96, 0 },
    { 0, 448, 96, 0 },
    { 0, 512, 96, 0 },
    { 0, 384, 96, 0 },
    { 0, 448, 128, 0 },
    { 0, 288, 128, 0 },
    { 0, 288, 96, 0 },
    { 0, 0, 0, 0 },
    { 0, 480, 128, 0 },
    { 0, 0, 0, 0 },
    { 0, 448, 128, 0 },
    { 0, 640, 128, 0 },
    { 0, 560, 96, 0 },
    { 0, 672, 96, 0 },
    { 0, 752, 96, 0 },
    { 0, 640, 96, 0 },
    { 0, 864, 144, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 640, 96, 0 },
    { 0, 640, 96, 0 },
    { 0, 0, 0, 0 },
    { 0, 32, 768, 0 },
    { 0, 0, 0, 0 },
    { 0, 640, 96, 0 },
    { 0, 640, 96, 0 },
    { 0, 640, 96, 0 },
    { 0, 640, 96, 0 },
    { 0, 640, 96, 0 },
    { 0, 576, 128, 0 },
    { 0, 576, 128, 0 },
    { 0, 576, 128, 0 },
    { 0, 448, 128, 0 },
};
SVECTOR D_801125EC[34] = {
    { 0, 0, 0, 0 },
    { 0, 0, 256, 0 },
    { 0, 0, 320, 0 },
    { 0, 0, 384, 0 },
    { 0, 0, 256, 0 },
    { 0, 0, 256, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 256, 0 },
    { 0, 384, 0, 0 },
    { 0, 0, 384, 0 },
    { 0, 0, 448, 0 },
    { 0, 0, 576, 0 },
    { 0, 0, 480, 0 },
    { 0, 64, 576, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 480, 0 },
    { 0, 0, 480, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 512, 0 },
    { 0, 0, 512, 0 },
    { 0, 0, 448, 0 },
    { 0, 0, 512, 0 },
    { 0, 0, 512, 0 },
    { 0, 0, 256, 0 },
    { 0, 0, 256, 0 },
    { 0, 0, 256, 0 },
    { 0, 0, 480, 0 },
};
SVECTOR D_801126FC[34] = {
    { 0, 0, 0, 0 },
    { 0, 384, 96, 0 },
    { 0, 448, 96, 0 },
    { 0, 512, 96, 0 },
    { 0, 384, 96, 0 },
    { 0, 448, 128, 0 },
    { 0, 288, 128, 0 },
    { 0, 288, 96, 0 },
    { 0, 0, 0, 0 },
    { 0, 480, 128, 0 },
    { 0, 0, 0, 0 },
    { 0, 448, 128, 0 },
    { 0, 640, 128, 0 },
    { 0, 560, 96, 0 },
    { 0, 672, 96, 0 },
    { 0, 752, 96, 0 },
    { 0, 640, 96, 0 },
    { 0, 864, 144, 0 },
    { 0, 0, 0, 0 },
    { 0, 768, 0, 0 },
    { 0, 640, 96, 0 },
    { 0, 640, 96, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 640, 96, 0 },
    { 0, 640, 96, 0 },
    { 0, 640, 96, 0 },
    { 0, 640, 96, 0 },
    { 0, 640, 96, 0 },
    { 0, 576, 64, 0 },
    { 0, 576, 64, 0 },
    { 0, 576, 64, 0 },
    { 0, 640, 96, 0 },
};
SVECTOR D_8011280C[34] = {
    { 0, 0, 0, 0 },
    { 0, 0, 256, 0 },
    { 0, 0, 320, 0 },
    { 0, 0, 384, 0 },
    { 0, 0, 256, 0 },
    { 0, 0, 256, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 256, 0 },
    { 0, 384, 0, 0 },
    { 0, 0, 384, 0 },
    { 0, 0, 448, 0 },
    { 0, 0, 576, 0 },
    { 0, 0, 512, 0 },
    { 0, 64, 576, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 512, 0 },
    { 0, 0, 256, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 512, 0 },
    { 0, 0, 512, 0 },
    { 0, 0, 256, 0 },
    { 0, 0, 512, 0 },
    { 0, 0, 512, 0 },
    { 0, 0, 384, 0 },
    { 0, 0, 384, 0 },
    { 0, 0, 384, 0 },
    { 0, 448, 128, 0 },
};
_EffectCriticalHitStyle D_8011291C[6] = {
    { EFFECT_CRITICAL_HIT_STYLE_SPIKES | EFFECT_CRITICAL_HIT_STYLE_COLOR(15, 15, 7), 24 },
    { EFFECT_CRITICAL_HIT_STYLE_COLOR(15, 7, 7), 32 },
    { EFFECT_CRITICAL_HIT_STYLE_SPIKES | EFFECT_CRITICAL_HIT_STYLE_COLOR(7, 15, 15), 24 },
    { EFFECT_CRITICAL_HIT_STYLE_SPIKES | EFFECT_CRITICAL_HIT_STYLE_COLOR(7, 15, 7), 24 },
    { EFFECT_CRITICAL_HIT_STYLE_SPIKES | EFFECT_CRITICAL_HIT_STYLE_COLOR(15, 7, 15), 24 },
    { EFFECT_CRITICAL_HIT_STYLE_COLOR(7, 7, 15), 24 },
};
_EffectImpactFlashFrame Gp_EffSprRecs[4] = {
    { 55, 112, 200, 176, 266, 576 },
    { 31, 192, 56, 192, 266, 512 },
    { 55, 168, 200, 192, 266, 576 },
    { 31, 224, 56, 192, 266, 512 },
};
