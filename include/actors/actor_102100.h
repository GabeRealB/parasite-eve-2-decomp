#ifndef ACTOR_102100_H
#define ACTOR_102100_H

#include "common.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// `field_8` is the actor's `GsCOORDINATE2` array: entry 0 is the actor's own
/// coordinate, entry 3 the alternate sight origin used by `Actor02100_Fn00DCC`.
typedef struct Actor02100Obj2C {
    /* 0x0 */ byte           pad_0[8];
    /* 0x8 */ GsCOORDINATE2* field_8;
    /* 0xC */ s16            field_C;
} Actor02100Obj2C;

/// Overlay-local view of the spawn parameter block (`Task::spawnArg2`).
/// `field_3C` points at the per-enemy parameter record whose byte 0xF holds
/// the sight-range index (low 3 bits) into `Actor02100_D03E00`.
typedef struct Actor02100Params {
    /* 0x0 */ byte pad_0[0xF];
    /* 0xF */ u8   field_F;
} Actor02100Params;

typedef struct Actor02100Spawn {
    /* 0x00 */ byte              pad_0[8];
    /* 0x08 */ u16               field_8;
    /* 0x0A */ byte              pad_A[6];
    /* 0x10 */ byte              field_10[0x2C];
    /* 0x3C */ Actor02100Params* field_3C;
    /* 0x40 */ s16               field_40;
} Actor02100Spawn;

typedef struct Actor02100Fn014E4Scratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR shortVec;
} Actor02100Fn014E4Scratch;
STATIC_ASSERT_SIZEOF(Actor02100Fn014E4Scratch, 0x18);

typedef struct Actor02100Fn011C4Scratch {
    /* 0x00 */ VECTOR  transformed;
    /* 0x10 */ VECTOR  delta;
    /* 0x20 */ VECTOR  lock;
    /* 0x30 */ SVECTOR from;
    /* 0x38 */ SVECTOR to;
} Actor02100Fn011C4Scratch;
STATIC_ASSERT_SIZEOF(Actor02100Fn011C4Scratch, 0x40);

/// 0x20-byte scratch block taken from `G_SCRATCH_HEAD` by
/// `Actor02100_Fn00DCC`: the world-space delta between the two coordinates,
/// then the two endpoints as `SVECTOR`s for the line-of-sight test.
typedef struct Actor02100Sight {
    /* 0x00 */ VECTOR  delta;
    /* 0x10 */ SVECTOR from;
    /* 0x18 */ SVECTOR to;
} Actor02100Sight;
STATIC_ASSERT_SIZEOF(Actor02100Sight, 0x20);

/// 8-byte block taken from `G_SCRATCH_HEAD` by `Actor02100_Fn034E0`: the
/// projected screen position (`stsxy`) and the quartered depth (`stszotz`).
typedef struct Actor02100Screen {
    /* 0x0 */ DVECTOR sxy;
    /* 0x4 */ s32     sz;
} Actor02100Screen;
STATIC_ASSERT_SIZEOF(Actor02100Screen, 8);

/// 0x28-byte scratch `Actor02100_Fn01FF0` takes from `G_SCRATCH_HEAD` while it
/// re-aims the actor. `shortVec` is the local offset fed to the GTE,
/// `transformed` that offset rotated by the coordinate's matrix and translated
/// by its position, and `delta` the same point mapped back into the
/// coordinate's own frame; the difference between `delta` and the stored target
/// position is what `Gp_OrientAlong` turns into the actor's facing matrix.
typedef struct Actor02100Fn01FF0Scratch {
    /* 0x00 */ VECTOR  transformed;
    /* 0x10 */ VECTOR  delta;
    /* 0x20 */ SVECTOR shortVec;
} Actor02100Fn01FF0Scratch;
STATIC_ASSERT_SIZEOF(Actor02100Fn01FF0Scratch, 0x28);

/// 0x48-byte block `Actor02100_Fn01FF0` reserves on entry and releases on
/// return. Only `shortVec` is read - it is the spawn offset passed to the two
/// effects state 2 emits; the leading bytes are never touched, and the helpers
/// the function inlines allocate their own scratch below this block.
typedef struct Actor02100Fn01FF0Block {
    /* 0x00 */ byte    pad_0[0x40];
    /* 0x40 */ SVECTOR shortVec;
} Actor02100Fn01FF0Block;
STATIC_ASSERT_SIZEOF(Actor02100Fn01FF0Block, 0x48);

/// Entry of `Actor02100_D03D88`, selected by `Actor02100Work::field_178`, read
/// through two views of the same sixteen bytes. `bounds` holds the frame
/// counts that drive the state machine: `field_17A` advances state 0 to state 1
/// once it reaches `field_0`, and returns state 6 to state 0 once it reaches
/// `field_2`. The remaining twelve bytes are two RGB triplets, one component
/// per short; `Actor02100_Fn02924` selects a triplet with its style argument
/// and reads each component's low byte, so it indexes the whole entry as
/// `shorts` and reaches the triplets at indices 2..7.
typedef union Actor02100Fn01FF0Timing {
    struct Actor02100Fn01FF0TimingBounds {
        /* 0x0 */ s16  field_0;
        /* 0x2 */ s16  field_2;
        /* 0x4 */ byte pad_4[0xC];
    } bounds;
    s16 shorts[8];
} Actor02100Fn01FF0Timing;
STATIC_ASSERT_SIZEOF(Actor02100Fn01FF0Timing, 0x10);

/// 0x3C-byte block `Actor02100_Fn02924` takes from `G_SCRATCH_HEAD` while it
/// draws one beam between the two screen points in `Actor02100Work`. `delta` is
/// the span between those points, which `VectorNormalS` turns into `normal`;
/// the y component of `normal` is then negated, so that scaling the pair by a
/// width gives the sideways offset of a beam edge. The beam is drawn as eight
/// segments: `depth` is the current segment's depth, `depthStep` the depth
/// added per segment, and `stepX`/`stepY` the per-segment step of the centre
/// line. `x` and `y` hold one segment's six corners - the two centre-line
/// points, then the two points of each edge.
typedef struct Actor02100Fn02924Scratch {
    /* 0x00 */ VECTOR  delta;
    /* 0x10 */ SVECTOR normal;
    /* 0x18 */ s32     depth;
    /* 0x1C */ s32     depthStep;
    /* 0x20 */ s16     x[6];
    /* 0x2C */ s16     y[6];
    /* 0x38 */ s16     stepX;
    /* 0x3A */ s16     stepY;
} Actor02100Fn02924Scratch;
STATIC_ASSERT_SIZEOF(Actor02100Fn02924Scratch, 0x3C);

/// One beam style in an `Actor02100_D03DD8` row. Each value is scaled by the
/// beam normal and added to the centre line, so the pair places the beam's two
/// edges; every stored row holds a negative and a positive offset of the same
/// size, which makes the beam symmetric about its centre.
typedef struct Actor02100Fn02924Edges {
    /* 0x0 */ s16 first;
    /* 0x2 */ s16 second;
} Actor02100Fn02924Edges;

/// Row of `Actor02100_D03DD8`, selected by `Actor02100Work::field_178`. The
/// style argument of `Actor02100_Fn02924` picks one of the two entries, and
/// picks the matching colour triplet out of `Actor02100_D03D88`.
typedef struct Actor02100Fn02924Widths {
    /* 0x0 */ Actor02100Fn02924Edges styles[2];
} Actor02100Fn02924Widths;

/// Entry of `Actor02100_D03E1C`: the corners of one of the two quads a segment
/// is built from, as indices into `Actor02100Fn02924Scratch::x` and `y`. Both
/// entries name the centre-line pair first and one edge second, and the quad is
/// shaded from the beam colour on those first two corners to black on the
/// other two, so each quad fades outwards from the centre line.
typedef struct Actor02100Fn02924Corners {
    /* 0x0 */ s16 corners[4];
} Actor02100Fn02924Corners;

struct Actor02100;

typedef struct Actor02100Work {
    /* 0x000 */ byte               pad_0[0x40];
    /* 0x040 */ GpObj              field_40;
    /* 0x060 */ GpRec18            field_60;
    /* 0x078 */ GpObj              field_78;
    /* 0x098 */ GpRec18            field_98;
    /* 0x0B0 */ SVECTOR            field_B0;
    /* 0x0B8 */ byte               pad_B8[0x10];
    /* 0x0C8 */ GpObj              field_C8;
    /* 0x0E8 */ SVECTOR            field_E8;
    /* 0x0F0 */ byte               pad_F0[0x10];
    /* 0x100 */ GpEffArg           field_100;
    /* 0x108 */ VECTOR             field_108;
    /* 0x118 */ s16                field_118;
    /* 0x11A */ s16                field_11A;
    /* 0x11C */ s16                field_11C;
    /* 0x11E */ byte               pad_11E[2];
    /* 0x120 */ s16                field_120;
    /* 0x122 */ s16                field_122;
    /* 0x124 */ s16                field_124;
    /* 0x126 */ byte               pad_126[2];
    /* 0x128 */ SVECTOR            field_128[2];
    /* 0x138 */ s16                field_138;
    /* 0x13A */ s16                field_13A;
    /* 0x13C */ s16                field_13C;
    /* 0x13E */ byte               pad_13E[2];
    /* 0x140 */ struct Actor02100* field_140;
    /* 0x144 */ MATRIX             field_144;
    /* 0x164 */ s32                field_164;
    /* 0x168 */ s32                field_168;
    /* 0x16C */ s16                field_16C;
    /* 0x16E */ s16                field_16E;
    /* 0x170 */ s16                field_170;
    /* 0x172 */ s16                field_172;
    /* 0x174 */ s16                field_174;
    /* 0x176 */ s16                field_176;
    /* 0x178 */ s16                field_178;
    /* 0x17A */ s16                field_17A;
    /* 0x17C */ s16                field_17C;
    /* 0x17E */ s16                field_17E;
    /* 0x180 */ s16                field_180;
    /* 0x182 */ u16                field_182;
    /* 0x184 */ s16                field_184;
    /* 0x186 */ s16                field_186;
    /* 0x188 */ s16                field_188;
    /* 0x18A */ s16                field_18A;
    /* 0x18C */ s16                field_18C[2];
    /* 0x190 */ s16                field_190[2];
    /* 0x194 */ s32                field_194[2];
} Actor02100Work;
STATIC_ASSERT_SIZEOF(Actor02100Work, 0x19C);

typedef struct Actor02100 {
    /* 0x00 */ byte             pad_0[0x1C];
    /* 0x1C */ Actor02100Work*  field_1C;
    /* 0x20 */ Actor02100Spawn* field_20;
    /* 0x24 */ byte             pad_24[8];
    /* 0x2C */ Actor02100Obj2C* field_2C;
    /* 0x30 */ s32              field_30;
} Actor02100;
STATIC_ASSERT_SIZEOF(Actor02100, 0x34);

/// `Gp_UnlinkNode` list entry at +0x10 of `Actor02100Ctx`. `field_4` is the
/// flag byte the per-state handlers write (`Actor02100_Fn031C4`).
typedef struct GpLinkNode {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ u8   field_4;
    /* 0x5 */ byte pad_5[3];
} GpLinkNode;
STATIC_ASSERT_SIZEOF(GpLinkNode, 0x8);

typedef struct Actor02100Ctx {
    /* 0x00 */ byte           pad_0[0x10];
    /* 0x10 */ GpLinkNode node;
    /* 0x18 */ byte           pad_18[0x3C];
    /* 0x54 */ s32            field_54;
} Actor02100Ctx;
STATIC_ASSERT_SIZEOF(Actor02100Ctx, 0x58);

typedef void (*Actor02100StateFunc)(Actor02100Ctx* arg0, Actor02100* arg1);

typedef struct {
    Actor02100StateFunc funcs[3];
} Actor02100StateFuncTable3;

extern u32 Gp_LcgState;

extern GpU16Pair                 Actor02100_D03D64;
extern Actor02100Fn01FF0Timing   Actor02100_D03D88[];
extern Actor02100Fn02924Widths   Actor02100_D03DD8[];
extern s16                       Actor02100_D03E00[];
extern Actor02100Fn02924Corners  Actor02100_D03E1C[];
extern s16                       Actor02100_D03E2C[];
extern Actor02100StateFuncTable3 Actor02100_D00004;

void Actor02100_Fn03168(Actor02100* arg0);
void Actor02100_Fn031C4(Actor02100Ctx* arg0, Actor02100* arg1);
void Actor02100_Fn032E4(Actor02100* arg0);
void Actor02100_Fn035D4(Actor02100Ctx* arg0, Actor02100* arg1);
s32  Actor02100_Fn014E4(Actor02100* arg0);

#endif
