#include "pe/inferno.h"

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
#include "gameplay/pad_script.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/sound.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// Vertices on each rim of one inferno fan band.
///
/// The texture row has one cell per vertex, so a segment index and its frame
/// wrap with this count. `INFERNO_FAN_SEGMENT_YAW` is the angle between
/// adjacent vertices, in the frame's 4096-per-turn units. Six steps close the
/// band four units short of a full turn.
#define INFERNO_FAN_SEGMENT_COUNT 6
#define INFERNO_FAN_SEGMENT_YAW   0x2AA

/// The two bands of one inferno fan, in shape-table and texture-phase order.
///
/// `INFERNO_FAN_RISING_BAND` is lifted by `EffectWork::period` plus the
/// shape's lift and has base radius 0x100. `INFERNO_FAN_CONSTANT_LIFT_BAND`
/// uses the shape's lift alone and has base radius 0x200.
/// `INFERNO_FAN_BAND_COUNT` is how many of those columns the fan has.
#define INFERNO_FAN_RISING_BAND        0
#define INFERNO_FAN_CONSTANT_LIFT_BAND 1
#define INFERNO_FAN_BAND_COUNT         2

/// Per-segment texture-cell phase for both bands of one inferno fan.
///
/// The fan task allocates one as its `Task::work`. Each byte is the high
/// half of an LCG draw. A drawer adds `EffectWork::age` and reduces modulo
/// `INFERNO_FAN_SEGMENT_COUNT`; the residue selects that segment's cell on
/// the six-cell texture row.
///
/// `band` names the two columns. `byBand` is those same bytes in band-major
/// order, which is how a drawer addresses the column its `kind` selects.
/// `byte` is that order flattened, so the fill can write segment `i` of both
/// columns from one pointer and stay inside one array. `byte[i]` is segment
/// `i` of `risingBand`; `byte[i + INFERNO_FAN_SEGMENT_COUNT]` is that segment
/// of `constantLiftBand`.
typedef union {
    struct {
        u8 risingBand[INFERNO_FAN_SEGMENT_COUNT];                 // Cell phase of the band whose lift is period + lift
        u8 constantLiftBand[INFERNO_FAN_SEGMENT_COUNT];           // Cell phase of the band whose lift is the shape's lift alone
    } band;
    u8 byBand[INFERNO_FAN_BAND_COUNT][INFERNO_FAN_SEGMENT_COUNT]; // [0] risingBand, [1] constantLiftBand
    u8 byte[INFERNO_FAN_BAND_COUNT * INFERNO_FAN_SEGMENT_COUNT];  // risingBand, then constantLiftBand
} _InfernoFanTexturePhase;
STATIC_ASSERT_SIZEOF(_InfernoFanTexturePhase, 0xC);

/// Scratch-stack workspace for one band of the inferno ground fan.
///
/// A drawer places one vertex of each rim at every `INFERNO_FAN_SEGMENT_YAW`
/// in the effect coordinate's local frame, rotates it by that coordinate's
/// `workm` and adds its translation. `topRing` is the wider rim, displaced by
/// the band's lift along local -Y. `bottomRing` is the narrower rim and stays
/// in the local XZ plane. Quad `i` takes vertices 0 and 1 from `topRing[i]`
/// and `topRing[i + 1]`, and vertices 2 and 3 from `bottomRing[i]` and
/// `bottomRing[i + 1]`.
///
/// `sxy0`..`sxy3` are those vertices' screen positions as packed words. X is
/// the low half and Y is the arithmetic shift of the high half, so the words
/// stay signed. Ordering depth and the GTE flag are the drawer's locals, and
/// pointers into the block end at its release.
typedef struct {
    SVECTOR topRing[INFERNO_FAN_SEGMENT_COUNT];    // Wider rim, lifted along local -Y; quad vertices 0 and 1
    SVECTOR bottomRing[INFERNO_FAN_SEGMENT_COUNT]; // Narrower rim in the local XZ plane; quad vertices 2 and 3
    s32     sxy0;                                  // Packed screen position of the current quad's vertex 0
    s32     sxy1;                                  // Packed screen position of vertex 1
    s32     sxy2;                                  // Packed screen position of vertex 2
    s32     sxy3;                                  // Packed screen position of vertex 3
} _InfernoFanScratch;
STATIC_ASSERT_SIZEOF(_InfernoFanScratch, 0x70);

/// The two fan shapes the inferno wall sweeps through.
static EffectBandShape D_inferno_801304E4[] = {
    { 0x0100, 0x0800, 0x0200 },
    { 0x0200, 0x0600, 0x0300 },
};

/// The `sndEvtRequestScriptStart` id the inferno cast plays, indexed by
/// the cast's level, `Gp_StateC08.attachId % 10 - 1`.
/// The same index also picks the state `func_inferno_8012EF88` advances to,
/// which is why the three ids and the three state chains run in step.
static s32 D_inferno_801304F0[] = { 0xE0100001, 0xE0130001, 0xE00D0001 };

static void func_inferno_8012F3EC(s16 arg0);
static void func_inferno_8012F978(EffectWork* mem, GfxCoord* coord, s32 kind, _InfernoFanTexturePhase* phase);
static void func_inferno_8012FF34(EffectWork* mem, GfxCoord* coord, s32 kind, _InfernoFanTexturePhase* phase);

/// Runs one frame of the inferno cast: a state machine driven by
/// `Task::state`, with the chain it takes chosen in state 0 from
/// `Gp_StateC08.attachId % 10 - 1` (the combo counter), which also picks the
/// roar from `D_inferno_801304F0` and lands the task on state 1, 5 or 9.
/// State 1 spawns the two ignition effects, state 5 fans six flames around a
/// 0x400 step, state 9 the ground burst; states 10 and 11 fade the effect
/// brightness scalar (`EffectWork::angle`) down and back up and each fire one ring of
/// flames on their own tick, and state 12 fades out and releases. Every state
/// updates the effect coordinate first, and any state releases immediately if
/// the player is dying (`Gp_StateC08.effectPhase`) or parasite-energy effects are
/// cancelled (`gRoomEffectState->peEffectControl`).
void func_inferno_8012EF88(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s32         i;
    s32         pan;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        goto release;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    mem->age = mem->age + 1;
    switch (arg0->state) {
        case 0:
            mem->scale = 0x200;
            mem->angle = 0xFF;
            pan        = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(D_inferno_801304F0[(u16)(Gp_StateC08.attachId % 10) - 1], pan,
                                     (s8)worldCoordGetOriginAudioDepth(coord));
            arg0->state = ((u16)(Gp_StateC08.attachId % 10) - 1) * 4 + 1;
            return;
        case 1:
            Gp_SpawnEff((EFFECT_INFERNO_FLAME | EFFECT_SPAWN_UNLIMITED), coord, 3, NULL);
            Gp_SpawnEff((EFFECT_INFERNO_FLAME | EFFECT_SPAWN_UNLIMITED), coord, 5, NULL);
            Gp_StateC08.flags |= ATTACHMENT_FLAG_APPLY_STATS;
            Gp_SpawnPadLerp(0x10, 0xFF, 8);
            arg0->state = 0xC;
            return;
        case 5:
            i = 0x200;
            func_inferno_8012F3EC(mem->angle);
            Gp_SpawnEff((EFFECT_INFERNO_FLAME | EFFECT_SPAWN_UNLIMITED), coord, 3, NULL);
            mem->scale = 0x600;
            do {
                mem->move.vx = (rsin(i) * mem->scale) >> 12;
                mem->move.vz = (rcos(i) * mem->scale) >> 12;
                i           += 0x400;
                Gp_SpawnEff((EFFECT_INFERNO_FLAME | EFFECT_SPAWN_UNLIMITED), coord, 4, &mem->move);
            } while (i < 0x1200);
            Gp_StateC08.flags |= ATTACHMENT_FLAG_APPLY_STATS;
            Gp_SpawnPadLerp(0x14, 0xFF, 8);
            arg0->state = 0xC;
            return;
        case 9:
            Gp_SpawnEff((EFFECT_INFERNO_FLAME | EFFECT_SPAWN_UNLIMITED), coord, 0, NULL);
            Gp_SpawnPadLerp(0xC, 0xFF, 8);
            arg0->state = 0xA;
            return;
        case 10:
            func_inferno_8012F3EC(mem->angle);
            mem->angle = mem->angle - 0x10;
            if (mem->age != 0xC) {
                return;
            }
            mem->scale = 0x600;
            i          = 0x155;
            do {
                mem->move.vx = (rsin(i) * mem->scale) >> 12;
                mem->move.vz = (rcos(i) * mem->scale) >> 12;
                i           += 0x2AA;
                Gp_SpawnEff((EFFECT_INFERNO_FLAME | EFFECT_SPAWN_UNLIMITED), coord, 1, &mem->move);
            } while (i < 0x1151);
            Gp_SpawnPadLerp(0xC, 0xFF, 8);
            arg0->state = 0xB;
            return;
        case 11:
            func_inferno_8012F3EC(mem->angle);
            if (mem->angle < 0xF0) {
                mem->angle = mem->angle + 0x10;
            }
            if (mem->age != 0x18) {
                return;
            }
            mem->scale = 0x900;
            i          = 0;
            do {
                mem->move.vx = (rsin(i) * mem->scale) >> 12;
                mem->move.vz = (rcos(i) * mem->scale) >> 12;
                i           += 0x2AA;
                Gp_SpawnEff((EFFECT_INFERNO_FLAME | EFFECT_SPAWN_UNLIMITED), coord, 2, &mem->move);
            } while (i < 0xFFC);
            Gp_StateC08.flags |= ATTACHMENT_FLAG_APPLY_STATS;
            Gp_SpawnPadLerp(0x18, 0xFF, 8);
            arg0->state = 0xC;
            mem->angle  = 0xFF;
            return;
        case 12:
            func_inferno_8012F3EC(mem->angle);
            if (mem->angle >= 9) {
                mem->angle = mem->angle - 8;
                return;
            }
            break;
        default:
            return;
    }
release:
    effectKillTask(mem, arg0);
}

/// Full-screen wash quad drawn by the inferno cast: an unshaded `POLY_F4`
/// covering the 320x240 view in `arg0` / `arg0 >> 1` / `arg0 >> 2` red-amber,
/// added to OT slot 0x30 and followed by a shifted-tpage semi-trans packet.
static void func_inferno_8012F3EC(s16 arg0)
{
    POLY_F4*      p;
    DisplayState* ds;
    s32           x0;
    s32           x1;
    s32           yTop;
    s32           yBot;
    s32           z;

    ds   = &gDisplayState;
    x0   = -0xA0;
    x1   = 0xA0;
    yTop = -0x78;
    yBot = 0x78;
    z    = 0x30;

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setPolyF4(p);
    setRGB0(p, arg0, arg0 >> 1, arg0 >> 2);
    p->x0 = x0;
    p->y0 = yTop - ds->vramYOffset;
    p->x1 = x1;
    p->y1 = yTop - ds->vramYOffset;
    p->x2 = x0;
    p->y2 = yBot - ds->vramYOffset;
    p->x3 = x1;
    p->y3 = yBot - ds->vramYOffset;
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)z << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), p);
    gpuSetPrimitiveBlendMode(p, GPU_BLEND_ADD, z);
}

/// Companion inferno-cast task: state 0 allocates an `_InfernoFanTexturePhase` and
/// fills both bands from the LCG, scales `EffectWork::pos` by 0x80
/// (`gte_gpf12`) and rotates it into `move`. States 1–6 fade `scale` while
/// spinning `angle` / `period` / `step` and drawing the rising band through
/// `func_inferno_8012F978` and the constant-lift band through
/// `func_inferno_8012FF34`. State 3 also walks the effect coordinate by
/// `move`. Releases if the player is dying, the room is fading, or the
/// state's brightness floor is hit. `Task::spawnArg1 + 1` selects the chain
/// from state 0.
void func_inferno_8012F530(Task* arg0)
{
    EffectWork*              mem;
    GfxCoord*                coord;
    _InfernoFanTexturePhase* phase;
    u8*                      segment;
    s32                      i;
    s32                      rng;
    s32                      tz;

    phase = (_InfernoFanTexturePhase*)arg0->work;
    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        goto release;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    mem->age = mem->age + 1;
    switch (arg0->state) {
        case 0:
            phase = memCalloc(sizeof(_InfernoFanTexturePhase), 0);
            if (phase == NULL) {
                mem->age = 0;
                return;
            }
            arg0->work = phase;
            mem->scale = 0x80;
            // Segment i of the rising band, then the same segment of the constant-lift band.
            i = 0;
            do {
                segment         = &phase->byte[i];
                rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = rng;
                segment[0]      = (u32)rng >> 16;
                i++;
                rng                                = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState                    = rng;
                segment[INFERNO_FAN_SEGMENT_COUNT] = (u32)rng >> 16;
            } while (i < INFERNO_FAN_SEGMENT_COUNT);
            arg0->state = arg0->spawnArg1.value + 1;
            gte_lddp(0x80);
            gte_ldsv(&mem->pos);
            gte_gpf12();
            gte_stsv(&mem->move);
            gte_SetRotMatrix(&coord->coord);
            gte_ldv0(&mem->move);
            gte_rtv0();
            gte_stsv(&mem->move);
            return;
        case 1:
            if (mem->scale >= 5) {
                if (mem->period < 0xC00) {
                    mem->period = mem->period + 0xC0;
                } else {
                    mem->scale = mem->scale - 4;
                }
                mem->angle = mem->angle + 0x20;
                mem->step  = mem->step + 0x18;
                func_inferno_8012F978(mem, coord, INFERNO_FAN_RISING_BAND, phase);
                func_inferno_8012FF34(mem, coord, INFERNO_FAN_CONSTANT_LIFT_BAND, phase);
                return;
            }
            break;
        case 2:
            if (mem->scale >= 9) {
                mem->scale  = mem->scale - 8;
                mem->angle  = mem->angle + 0x20;
                mem->period = mem->period + 0xC0;
                mem->step   = mem->step + 0x18;
                func_inferno_8012F978(mem, coord, INFERNO_FAN_RISING_BAND, phase);
                func_inferno_8012FF34(mem, coord, INFERNO_FAN_CONSTANT_LIFT_BAND, phase);
                return;
            }
            break;
        case 3:
            coord->coord.t[0]  += mem->move.vx;
            coord->coord.t[1]  += mem->move.vy;
            tz                  = coord->coord.t[2] + mem->move.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[2]   = tz;
            if (mem->scale >= 9) {
                mem->scale  = mem->scale - 8;
                mem->angle  = mem->angle + 0x20;
                mem->period = mem->period + 0xC0;
                mem->step   = mem->step + 0x18;
                func_inferno_8012F978(mem, coord, INFERNO_FAN_RISING_BAND, phase);
                func_inferno_8012FF34(mem, coord, INFERNO_FAN_CONSTANT_LIFT_BAND, phase);
                return;
            }
            break;
        case 4:
            if (mem->scale >= 7) {
                mem->scale  = mem->scale - 6;
                mem->angle  = mem->angle + 0x40;
                mem->period = mem->period + 0xC0;
                mem->step   = mem->step + 0x10;
                func_inferno_8012F978(mem, coord, INFERNO_FAN_RISING_BAND, phase);
                func_inferno_8012FF34(mem, coord, INFERNO_FAN_CONSTANT_LIFT_BAND, phase);
                return;
            }
            break;
        case 5:
            if (mem->scale >= 7) {
                mem->scale  = mem->scale - 6;
                mem->angle  = mem->angle + 0x40;
                mem->period = mem->period + 0x40;
                mem->step   = mem->step + 0x18;
                func_inferno_8012F978(mem, coord, INFERNO_FAN_RISING_BAND, phase);
                func_inferno_8012FF34(mem, coord, INFERNO_FAN_CONSTANT_LIFT_BAND, phase);
                return;
            }
            break;
        case 6:
            if (mem->scale >= 7) {
                mem->scale  = mem->scale - 6;
                mem->angle  = mem->angle + 0x80;
                mem->period = mem->period + 0x20;
                mem->step   = mem->step + 0x20;
                func_inferno_8012F978(mem, coord, INFERNO_FAN_RISING_BAND, phase);
                func_inferno_8012FF34(mem, coord, INFERNO_FAN_CONSTANT_LIFT_BAND, phase);
                return;
            }
            break;
        default:
            return;
    }
release:
    effectKillTask(mem, arg0);
}

/// Draws the raised band of the inferno's ground fan, the twin of
/// `func_inferno_8012FF34`. The rims and the primitives match. This band's
/// top rim is lifted by `EffectWork::period + lift` along local -Y instead
/// of `lift` alone, so it rises as `period` winds up. `kind` picks the row
/// of `D_inferno_801304E4` that sizes it.
static void func_inferno_8012F978(EffectWork* mem, GfxCoord* coord, s32 kind, _InfernoFanTexturePhase* phase)
{
    u8*                 head;
    _InfernoFanScratch* block;
    EffectBandShape*    row;
    EffectBandShape*    tbl;
    SVECTOR*            op;
    POLY_FT4*           prim;
    s32                 flag;
    s32                 otz;
    s32                 i;
    s32                 next;
    s32                 ang;
    s32                 u;
    s16                 inner;
    s16                 outer;
    u16                 height;
    u16                 frame;

    tbl                        = D_inferno_801304E4;
    row                        = &tbl[kind];
    height                     = mem->period + row->lift;
    inner                      = mem->angle + row->baseRadius;
    outer                      = row->spread + (inner + mem->step);
    head                       = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(void) = head - sizeof(_InfernoFanScratch);
    block                      = (_InfernoFanScratch*)(head - sizeof(_InfernoFanScratch));
    gte_SetTransMatrix(&GsWSMATRIX);
    // Wider lifted rim, then the narrower rim in the local XZ plane.
    for (i = 0; i < INFERNO_FAN_SEGMENT_COUNT; i++) {
        ang                  = i * INFERNO_FAN_SEGMENT_YAW;
        block->topRing[i].vx = (rsin(ang) * outer) >> 12;
        block->topRing[i].vy = -height;
        block->topRing[i].vz = (rcos(ang) * outer) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->topRing[i]);
        gte_rtv0();
        gte_stsv(&block->topRing[i]);
        block->topRing[i].vx    = (u16)block->topRing[i].vx + (u16)coord->workm.t[0];
        block->topRing[i].vy    = (u16)block->topRing[i].vy + (u16)coord->workm.t[1];
        block->topRing[i].vz    = (u16)block->topRing[i].vz + (u16)coord->workm.t[2];
        block->bottomRing[i].vx = (rsin(ang) * inner) >> 12;
        op                      = &block->topRing[i] + INFERNO_FAN_SEGMENT_COUNT;
        op->vy                  = 0;
        op->vz                  = (rcos(ang) * inner) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->bottomRing[i]);
        gte_rtv0();
        gte_stsv(&block->bottomRing[i]);
        block->bottomRing[i].vx = (u16)block->bottomRing[i].vx + (u16)coord->workm.t[0];
        op->vy                  = (u16)op->vy + (u16)coord->workm.t[1];
        op->vz                  = (u16)op->vz + (u16)coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < INFERNO_FAN_SEGMENT_COUNT; i++) {
        gte_ldv0(&block->topRing[i]);
        gte_rtps();
        frame = (phase->byBand[kind][i] + mem->age) % INFERNO_FAN_SEGMENT_COUNT;
        gte_stsxy(&block->sxy0);
        next = i + 1;
        gte_ldv3(&block->topRing[next % INFERNO_FAN_SEGMENT_COUNT], &block->bottomRing[i], &block->bottomRing[next % INFERNO_FAN_SEGMENT_COUNT]);
        gte_rtpt();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&flag);
        if (flag >= 0) {
            gte_stszotz(&otz);
            otz++;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2E);
            setRGB0(prim, mem->scale, mem->scale, mem->scale);
            prim->tpage = 0x2A;
            prim->clut  = 0x4282;
            u           = frame * 0x28;
            setUV4(prim, u, 0x60, u + 0x27, 0x60, u, 0x87, u + 0x27, 0x87);
            prim->x0 = block->sxy0;
            prim->y0 = block->sxy0 >> 16;
            prim->x1 = block->sxy1;
            prim->y1 = block->sxy1 >> 16;
            prim->x2 = block->sxy2;
            prim->y2 = block->sxy2 >> 16;
            prim->x3 = block->sxy3;
            prim->y3 = block->sxy3 >> 16;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(_InfernoFanScratch));
}

/// Draws one band of the inferno's ground fan. `kind` picks the row of
/// `D_inferno_801304E4` that sizes it. The top rim has radius
/// `angle + baseRadius + step + spread` and is lifted `lift` along local -Y;
/// the bottom rim has radius `angle + baseRadius` and stays in the local XZ
/// plane. Both are built by `rsin` / `rcos` a sixth of a turn apart, rotated
/// by `coord`'s `workm` and offset by its translation. Each segment is then
/// projected through `GsWSMATRIX` and linked as one semi-transparent
/// `POLY_FT4`. `phase` and `EffectWork::age` pick which of the
/// `INFERNO_FAN_SEGMENT_COUNT` texture cells it uses, and a negative
/// `gte_stflg` drops the segment.
static void func_inferno_8012FF34(EffectWork* mem, GfxCoord* coord, s32 kind, _InfernoFanTexturePhase* phase)
{
    u8*                 head;
    _InfernoFanScratch* block;
    EffectBandShape*    row;
    EffectBandShape*    tbl;
    SVECTOR*            op;
    POLY_FT4*           prim;
    s32                 flag;
    s32                 otz;
    s32                 i;
    s32                 next;
    s32                 ang;
    s32                 u;
    s16                 inner;
    s16                 outer;
    u16                 height;
    u16                 frame;

    tbl                        = D_inferno_801304E4;
    row                        = &tbl[kind];
    inner                      = mem->angle + row->baseRadius;
    outer                      = row->spread + (inner + mem->step);
    height                     = row->lift;
    head                       = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(void) = head - sizeof(_InfernoFanScratch);
    block                      = (_InfernoFanScratch*)(head - sizeof(_InfernoFanScratch));
    gte_SetTransMatrix(&GsWSMATRIX);
    // Wider lifted rim, then the narrower rim in the local XZ plane.
    for (i = 0; i < INFERNO_FAN_SEGMENT_COUNT; i++) {
        ang                  = i * INFERNO_FAN_SEGMENT_YAW;
        block->topRing[i].vx = (rsin(ang) * outer) >> 12;
        block->topRing[i].vy = -height;
        block->topRing[i].vz = (rcos(ang) * outer) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->topRing[i]);
        gte_rtv0();
        gte_stsv(&block->topRing[i]);
        block->topRing[i].vx    = (u16)block->topRing[i].vx + (u16)coord->workm.t[0];
        block->topRing[i].vy    = (u16)block->topRing[i].vy + (u16)coord->workm.t[1];
        block->topRing[i].vz    = (u16)block->topRing[i].vz + (u16)coord->workm.t[2];
        block->bottomRing[i].vx = (rsin(ang) * inner) >> 12;
        op                      = &block->topRing[i] + INFERNO_FAN_SEGMENT_COUNT;
        op->vy                  = 0;
        op->vz                  = (rcos(ang) * inner) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->bottomRing[i]);
        gte_rtv0();
        gte_stsv(&block->bottomRing[i]);
        block->bottomRing[i].vx = (u16)block->bottomRing[i].vx + (u16)coord->workm.t[0];
        op->vy                  = (u16)op->vy + (u16)coord->workm.t[1];
        op->vz                  = (u16)op->vz + (u16)coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < INFERNO_FAN_SEGMENT_COUNT; i++) {
        gte_ldv0(&block->topRing[i]);
        gte_rtps();
        frame = (phase->byBand[kind][i] + mem->age) % INFERNO_FAN_SEGMENT_COUNT;
        gte_stsxy(&block->sxy0);
        next = i + 1;
        gte_ldv3(&block->topRing[next % INFERNO_FAN_SEGMENT_COUNT], &block->bottomRing[i], &block->bottomRing[next % INFERNO_FAN_SEGMENT_COUNT]);
        gte_rtpt();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&flag);
        if (flag >= 0) {
            gte_stszotz(&otz);
            otz++;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2E);
            setRGB0(prim, mem->scale, mem->scale, mem->scale);
            prim->tpage = 0x2A;
            prim->clut  = 0x4282;
            u           = frame * 0x28;
            setUV4(prim, u, 0x60, u + 0x27, 0x60, u, 0x87, u + 0x27, 0x87);
            prim->x0 = block->sxy0;
            prim->y0 = block->sxy0 >> 16;
            prim->x1 = block->sxy1;
            prim->y1 = block->sxy1 >> 16;
            prim->x2 = block->sxy2;
            prim->y2 = block->sxy2 >> 16;
            prim->x3 = block->sxy3;
            prim->y3 = block->sxy3 >> 16;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(_InfernoFanScratch));
}
