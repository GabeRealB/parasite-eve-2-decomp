#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "rooms/room.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b3_dumping_hole.h"

#define DUMPING_HOLE_RAND() ((s32)((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16))

/// Spawns one debris task and gives it a work block seeded with `seed`.
#define DUMPING_HOLE_SPAWN_DEBRIS(seed)                                                               \
    {                                                                                                 \
        Task*                  t = Task_SpawnFromTable(&D_shelter_b3_dumping_hole_80188C04, 0, 0, 0); \
        DumpingHoleDebrisSeed* w = Mem_Malloc(0x24, 0);                                               \
        t->work                  = w;                                                                 \
        if (w == NULL) {                                                                              \
            taskKill(t);                                                                              \
        } else {                                                                                      \
            Mem_Set(w, 0, 0x24);                                                                      \
            *w = seed;                                                                                \
        }                                                                                             \
    }

typedef struct {
    s32   field_0;
    s32   field_4;
    s32   field_8;
    u8    pad_0C[0x4];
    s16   field_10;
    s16   field_12;
    s16   field_14;
    u8    pad_16[0xE];
    Task* field_24;
    Task* field_28;
    Task* field_2C;
    u16   field_30;
    u16   field_32;
    u16   field_34;
    u8    pad_36[0x2];
    u16   field_38;
    u16   field_3A;
    u16   field_3C;
    u8    pad_3E[0x2];
    s16   field_40;
    u16   field_42;
    s16   field_44;
    s16   field_46;
    s16   field_48;
    s16   field_4A;
    u16   field_4C;
} DumpingHoleEntity;

typedef struct {
    s16 field_0;
    s16 field_2;
    s16 field_4;
    u8  pad_6[0x2];
    s16 field_8;
    u8  pad_A[0x2];
    s16 field_C;
    s16 field_E;
    s16 field_10;
    u8  pad_12[0x2];
    s16 field_14;
    s16 field_16;
    s16 field_18;
    u8  pad_1A[0x2];
    s16 field_1C;
    u16 field_1E;
    u16 field_20;
} DumpingHoleAnimWork;

typedef struct {
    s16 field_0;
    s16 field_2;
    s16 field_4;
    s16 field_6;
    s16 field_8;
    s16 field_A;
} DumpingHoleAnimFrame;

typedef struct {
    s32 field_0;
    s32 field_4;
    s32 field_8;
    u8  pad_C[0x4];
    s16 field_10;
    s16 field_12;
    s16 field_14;
} DumpingHoleCoordCfg;

typedef struct {
    MATRIX field_0;
    MATRIX field_20;
    u16    rotX; // Accumulated rotation about X, advanced by `spinX` each frame
    u16    rotY; // Accumulated rotation about Y, advanced by `spinY` each frame
    u16    rotZ; // Accumulated rotation about Z; advanced but never applied
    u8     pad_46[0x2];
    s16    velX; // Per-frame translation added to the coordinate
    s16    velY;
    s16    velZ;
    u8     pad_4E[0x2];
    s16    spinX; // Per-frame rotation step, chosen at random at launch
    s16    spinY;
    s16    spinZ;
    u8     pad_56[0x2];
    u16    fall; // Downward speed added to `velY`, growing by 5 each frame
    u8     pad_5A[0x2];
} DumpingHoleCoordWork;

/// Stack block for projecting a point through `GsWSMATRIX`: the point, then
/// the screen position and depth the projection writes back.
typedef struct {
    SVECTOR pos;
    s16     sx;
    s16     sy;
    s32     otz;
} DumpingHoleProjection;

/// An entry of the spawn tables walked by message state 6; a 0xFFFF first
/// word ends the table.
typedef struct {
    s32 field_0;
    u8  pad_4[0x14];
} DumpingHoleSpawnEntry;

/// An entry of the third spawn table: a position, and a flag asking for a
/// ring of debris tasks around it. A 0xFFFF `x` ends the table.
typedef struct {
    s32 x;
    s32 y;
    s32 z;
    u8  pad_C[0xC];
    u16 field_18;
} DumpingHoleDebrisEntry;

/// The leading bytes of a debris task's work block, copied in whole.
typedef struct {
    s16 x;
    s16 y;
    s16 z;
    s16 pad_6;
    s16 field_8;
    s16 field_A;
} DumpingHoleDebrisSeed;

typedef struct {
    u8 pad_00[0x4A];
    u8 field_4A;
} DumpingHoleFlags;

typedef struct {
    u8  pad_00[0xC];
    s16 field_C;
    s16 field_E;
    s16 field_10;
} DumpingHoleSpawnWork;

typedef struct {
    u16 field_0;
    u16 field_2;
    u16 field_4;
} DumpingHoleSpawnArg;

/// Spawn record for a falling shard: where it starts relative to `parent`, its
/// base velocity, its size, and the downward speed it gains each frame.
typedef struct {
    SVECTOR        pos;
    SVECTOR        vel;
    GsCOORDINATE2* parent;
    u16            size;
    u16            fall;
} DumpingHoleShardCfg;

/// Work block of a falling shard: its current rotation and spin, its velocity,
/// the three corners of the triangle it draws, and the per-frame fall speed.
typedef struct {
    SVECTOR rot;
    SVECTOR rotSpeed;
    SVECTOR vel;
    SVECTOR verts[3];
    u16     fall;
} DumpingHoleShard;

typedef struct {
    MATRIX         lightMtx;
    MATRIX         colorMtx;
    GpXformArg     pose;     // Sent to the task itself with message 0x7D4
    SVECTOR        field_58; // Spawn parameters handed by address to the table spawns
    SVECTOR        field_60;
    GsCOORDINATE2* field_68;
    s16            field_6C;
    s16            field_6E;
    VECTOR         scale; // Per-axis scale applied to the rotation of model part 3
    Task*          field_80;
    Task*          field_84;
    Task*          field_88;
    u16            state; // One-shot command, cleared once handled
    u16            step;  // Progress through the sequence the command started
    u16            timer; // Frames spent in the current step
    u8             pad_92[0x2];
    s16            field_94;
    s16            field_96;
    s16            field_98; // X rotation of model part 1 once the sequence reaches step 2
    s16            field_9A;
    u16            field_9C; // Latch: set once the state-F0 release has been issued
    u8             pad_9E[0x2];
} DumpingHoleEntity4;

typedef struct {
    /* 0x00 */ char magic[0x8];
    /* 0x08 */ s32  field_8;
    /* 0x0C */ s32  field_C;
    /* 0x10 */ s32  field_10;
} Reloc80181D68Hdr;

typedef struct {
    /* 0x00 */ u8  pad[0x8];
    /* 0x08 */ s32 field_8;
} Reloc80181D68Entry1;

typedef struct {
    /* 0x00 */ s16                 count;
    /* 0x02 */ u8                  pad[0xE];
    /* 0x10 */ Reloc80181D68Entry1 entries[1];
} Reloc80181D68Table1;

typedef struct {
    /* 0x00 */ s32 count;
    /* 0x04 */ s32 entries[1];
} Reloc80181D68Table2;

typedef struct {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u8  r;
    /* 0x05 */ u8  g;
    /* 0x06 */ u8  b;
    /* 0x07 */ u8  code;
    /* 0x08 */ s16 field_8;
    /* 0x0A */ s16 field_A;
    /* 0x0C */ u8  field_C;
    /* 0x0D */ u8  field_D;
    /* 0x0E */ u8  field_E;
    /* 0x0F */ u8  pad_F;
    /* 0x10 */ s16 field_10;
    /* 0x12 */ s16 field_12;
    /* 0x14 */ u8  field_14;
    /* 0x15 */ u8  field_15;
    /* 0x16 */ u8  field_16;
    /* 0x17 */ u8  pad_17;
    /* 0x18 */ s16 field_18;
    /* 0x1A */ s16 field_1A;
} Prim82AA0;

typedef struct {
    /* 0x00 */ u8 pad[3];
    /* 0x03 */ u8 field_3;
} GlyphEntry;

/// Enemy table the pair slots spawn from; it lies outside the room's image.
extern TaskDesc D_80151E60;

/// The encounter's enemy slots, in the order the controller starts them.
extern OverlayEncounterSlot D_shelter_b3_dumping_hole_8018B7BC[];

extern TaskDesc               D_shelter_b3_dumping_hole_80188C04;
extern TaskDesc               D_shelter_b3_dumping_hole_80188BC8;
extern s16                    D_shelter_b3_dumping_hole_8018809C;
extern Task*                  D_shelter_b3_dumping_hole_8018F4A8;
extern s16                    D_shelter_b3_dumping_hole_80188154[];
extern DumpingHoleAnimFrame   D_shelter_b3_dumping_hole_801880B8[];
extern s16                    D_shelter_b3_dumping_hole_8018816C[];
extern s16                    D_shelter_b3_dumping_hole_80188184[];
extern u8                     D_80073BA9;
extern s8                     D_8007218A;
extern s32                    D_shelter_b3_dumping_hole_8018819C[];
extern u8                     D_shelter_b3_dumping_hole_801881CC;
extern s32                    D_shelter_b3_dumping_hole_801881E4;
extern DumpingHoleSpawnEntry  D_shelter_b3_dumping_hole_801881FC[];
extern DumpingHoleSpawnEntry  D_shelter_b3_dumping_hole_80188304[];
extern DumpingHoleDebrisEntry D_shelter_b3_dumping_hole_801884CC[];
extern u8                     D_80071075;
extern s8                     D_80114C12;
extern s8                     D_8007272D[];
extern s32                    D_shelter_b3_dumping_hole_801880A0[];
extern s32                    D_shelter_b3_dumping_hole_80188640;
extern s32                    D_shelter_b3_dumping_hole_80188A78;
extern DumpingHoleFlags       D_shelter_b3_dumping_hole_8018EF04;
extern u16                    D_shelter_b3_dumping_hole_8018F4B0;
extern Task*                  D_shelter_b3_dumping_hole_8018F4AC;
extern GpXformArg             D_shelter_b3_dumping_hole_8018966C;
extern TaskDesc               D_shelter_b3_dumping_hole_80189ADC;
extern s32                    D_shelter_b3_dumping_hole_8018F4D8;
extern s32                    D_shelter_b3_dumping_hole_8018965C;
extern s32                    D_shelter_b3_dumping_hole_8018968C;
extern s32                    D_shelter_b3_dumping_hole_801899A4;
extern TaskDesc               D_shelter_b3_dumping_hole_8018AFBC;
extern GpEvt12*               D_shelter_b3_dumping_hole_8018F4BC;
extern s16                    D_shelter_b3_dumping_hole_8018F4C6;
extern OverlayCapWindow       D_shelter_b3_dumping_hole_8018B5A0[];
extern GlyphUvwh*             D_shelter_b3_dumping_hole_8018F4B8;
extern s32                    D_shelter_b3_dumping_hole_8018F4B4;
extern s16                    D_shelter_b3_dumping_hole_8018F4C0;
extern s16                    D_shelter_b3_dumping_hole_8018F4C2;
extern s16                    D_shelter_b3_dumping_hole_8018F4C4;
extern s16                    D_shelter_b3_dumping_hole_8018F4C8;
extern s16                    D_shelter_b3_dumping_hole_8018F4CA;
extern u8                     D_shelter_b3_dumping_hole_8018F4D0;
extern GlyphUvwh              D_8010FB70[];
extern s16                    D_shelter_b3_dumping_hole_8018B578;
extern s16                    D_shelter_b3_dumping_hole_8018B57A;
extern u16                    D_shelter_b3_dumping_hole_8018F4CC;
extern u16                    D_shelter_b3_dumping_hole_8018F4CE;
extern s32                    D_shelter_b3_dumping_hole_8018B670;
extern s32                    D_shelter_b3_dumping_hole_8018B674;
extern TaskDesc               D_shelter_b3_dumping_hole_8018B588;
extern u16                    D_shelter_b3_dumping_hole_8018F4D4;
extern TaskDesc               D_80142604;
extern TaskDesc               D_801575F0;
extern TaskDesc               D_shelter_b3_dumping_hole_8018B594;
extern TaskDesc               D_shelter_b3_dumping_hole_8018B83C;
extern u8                     D_shelter_b3_dumping_hole_8018B7AC[];
extern SVECTOR                D_shelter_b3_dumping_hole_8018B86C[];
extern SVECTOR                D_shelter_b3_dumping_hole_8018B8BC[];
extern SVECTOR                D_shelter_b3_dumping_hole_8018B92C[];
extern SVECTOR                D_shelter_b3_dumping_hole_8018B93C[];
extern SVECTOR                D_shelter_b3_dumping_hole_8018B94C[];
extern SVECTOR                D_shelter_b3_dumping_hole_8018B95C[];
extern SVECTOR                D_shelter_b3_dumping_hole_8018B96C[];
extern SVECTOR                D_shelter_b3_dumping_hole_8018B974[];
extern SVECTOR                D_shelter_b3_dumping_hole_8018B98C[];

void func_shelter_b3_dumping_hole_8017EDB8(Task* arg0);
s32  func_shelter_b3_dumping_hole_80181E70(s16 arg0, s16 arg1, s32 arg2);
void func_shelter_b3_dumping_hole_80184638(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b3_dumping_hole_80184E7C(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b3_dumping_hole_801866CC(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
void func_shelter_b3_dumping_hole_80186AB8(GsCOORDINATE2* arg0, s32 arg1, s32 arg2);
void func_shelter_b3_dumping_hole_8017F1B0(Task* arg0);
void func_shelter_b3_dumping_hole_80183218(u8 arg0);
void func_shelter_b3_dumping_hole_8017FD9C(s32 arg0, s32 arg1);
void func_shelter_b3_dumping_hole_80181C8C(void);
s32  func_shelter_b3_dumping_hole_80181F80(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_b3_dumping_hole_80182AA0(void);
s32  func_shelter_b3_dumping_hole_80182FD0(s32 arg0);
s16  func_shelter_b3_dumping_hole_80182C24(u16* arg0);
s16  func_shelter_b3_dumping_hole_80182D34(u16* arg0, s32 arg1);
s32  func_shelter_b3_dumping_hole_80182F18(u16* arg0);
s16  func_shelter_b3_dumping_hole_801829B4(u16* arg0);
s32  func_shelter_b3_dumping_hole_80182E50(s32 arg0);
void func_shelter_b3_dumping_hole_801833EC(Task* arg0);
void func_shelter_b3_dumping_hole_80183E6C(s16 arg0, s16 arg1, s16 arg2);
s32  func_shelter_b3_dumping_hole_80181D68(s32 arg0);
void func_shelter_b3_dumping_hole_80183298(Task* arg0);
void func_shelter_b3_dumping_hole_801836E0(Task* arg0);
void func_shelter_b3_dumping_hole_8018378C(Task* arg0);
void func_shelter_b3_dumping_hole_80183824(Task* arg0);
void func_shelter_b3_dumping_hole_801838A0(Task* arg0);
void func_shelter_b3_dumping_hole_80183950(Task* arg0);
void func_shelter_b3_dumping_hole_80183A00(Task* arg0);
void func_shelter_b3_dumping_hole_80183A98(Task* arg0);
void func_shelter_b3_dumping_hole_80183AEC(Task* arg0);
void func_shelter_b3_dumping_hole_80183B9C(Task* arg0);
void func_shelter_b3_dumping_hole_80183C38(Task* arg0);
void func_shelter_b3_dumping_hole_80183C8C(Task* arg0);
void func_shelter_b3_dumping_hole_80183CA0(Task* arg0);
void func_shelter_b3_dumping_hole_80183D34(Task* arg0);
void func_shelter_b3_dumping_hole_80183E08(Task* arg0);
void func_shelter_b3_dumping_hole_80183F04(Task* arg0);
void func_shelter_b3_dumping_hole_8018596C(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
void func_shelter_b3_dumping_hole_80185DCC(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);

/// Returns 1 when the screen position (`x`, `y`) lies outside the 320x240
/// screen centred on the origin, 0 when it is on screen.
static inline u16 _shelterB3DumpingHoleIsOffscreen(s16 x, s16 y)
{
    if (x < -0xA0) {
        return 1;
    }
    if (x > 0xA0) {
        return 1;
    }
    if (y < -0x78) {
        return 1;
    }
    if (y > 0x78) {
        return 1;
    }
    return 0;
}

/// Draws a camera-facing textured quad centred on `coord`'s origin. The origin
/// is projected with the coordinate's world-screen matrix; if it lands outside
/// the 320x240 screen or behind the camera nothing is drawn and 1 is returned.
/// Otherwise a semi-transparent `POLY_FT4` of `w` x `h` texels at (`u`, `v`),
/// scaled by `scale` (4096 = 1.0), is linked into the ordering table at the
/// projected depth, or at `otzOverride` when that is non-zero, and 0 is returned.
u16 func_shelter_b3_dumping_hole_8017DA00(GsCOORDINATE2* coord, s16 w, s16 h, s16 u,
                                          s16 v, s16 tpageX, s16 tpageY, s16 scale,
                                          s16 clut, s32 otzOverride)
{
    SVECTOR   origin;
    s32       sxy;
    s32       z;
    s32       otz;
    u16       off;
    POLY_FT4* prim;
    u16       hw;
    u16       hh;
    s16       sx;
    s16       sy;

    Gp_UpdateCoord(coord);
    gte_SetTransMatrix(&coord->workm);
    gte_SetRotMatrix(&coord->workm);
    origin.vx = origin.vy = origin.vz = 0;
    gte_ldv0(&origin);
    gte_rtps();
    gte_stsxy(&sxy);
    gte_stszotz(&z);
    sy = sxy >> 16;
    sx = sxy;
    if (otzOverride != 0) {
        z = otzOverride;
    }
    otz = z;
    if (sx < -0xA0) {
        off = 1;
    } else if (sx > 0xA0 || sy < -0x78 || sy > 0x78 || otz < 0) {
        off = 1;
    } else {
        off = 0;
    }
    if (off) {
        return 1;
    }
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2F);
    hw = w * scale / 4096;
    hh = h * scale / 4096;
    setXY4(prim, sx - hw / 2, sy - hh / 2, sx + hw / 2, sy - hh / 2, sx - hw / 2, sy + hh / 2,
           sx + hw / 2, sy + hh / 2);
    setUV4(prim, u, v, u + w - 1, v, u, v + h - 1, u + w - 1, v + h - 1);
    prim->clut  = clut;
    prim->tpage = getTPage(0, 1, tpageX / 64 * 64, tpageY / 256 * 256);
    addPrim(gGpuCurrentOt + (z >> 4), prim);
    return 0;
}

void func_shelter_b3_dumping_hole_8017DCFC(Task* arg0)
{
    DumpingHoleAnimWork* W      = (DumpingHoleAnimWork*)arg0->work;
    GsCOORDINATE2*       coord  = ((TmdObject*)arg0->extra)->coords;
    DumpingHoleEntity*   entity = D_shelter_b3_dumping_hole_8018F4A8->work;

    if (entity->field_42 == 1) {
        taskKill((Task*)arg0);
        return;
    }

    switch (arg0->state) {
        case 0:
            coord->sub        = &gGfxViewCoord;
            coord->coord.t[0] = W->field_0;
            coord->coord.t[1] = W->field_2;
            coord->coord.t[2] = W->field_4;
            arg0->state++;
            return;
        case 1:
            if (entity->field_42 != 2) {
                return;
            }
            W->field_1C = 5;
            W->field_14 = 0;
            W->field_18 = 0;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            W->field_16 = 0xFFF6 - ((Gp_LcgState >> 16) & 7);
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            W->field_20 = (Gp_LcgState >> 16) & 7;
            arg0->state++;
            return;
        case 2:
            if (W->field_20 == 0) {
                arg0->state = 3;
            } else {
                W->field_20--;
            }
            return;
        case 3: {
            s32 t1e     = W->field_1E + 1;
            W->field_1E = t1e;
            if (D_shelter_b3_dumping_hole_80188154[W->field_1C] < (s16)t1e) {
                *(u16*)&W->field_1C = *(u16*)&W->field_1C + 1;
                W->field_1E         = 0;
                if (*(u16*)&D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_0 == 0xFFFF) {
                    taskKill((Task*)arg0);
                    return;
                }
            }
            break;
        }
        default:
            return;
    }

    coord->coord.t[1] += W->field_16;
    if (func_shelter_b3_dumping_hole_8017DA00(
            coord,
            D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_8,
            D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_A,
            D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_2,
            D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_6,
            D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_0,
            D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_4,
            W->field_8, 0x43C0, 0) != 0) {
        taskKill((Task*)arg0);
        return;
    }
    coord->flg = 0;
}

void func_shelter_b3_dumping_hole_8017DF90(Task* arg0)
{
    DumpingHoleAnimWork* W     = (DumpingHoleAnimWork*)arg0->work;
    GsCOORDINATE2*       coord = ((TmdObject*)arg0->extra)->coords;
    SVECTOR              vec;
    SVECTOR              pos;
    DVECTOR              sxy;
    POLY_FT4*            prim;
    u16                  offscreen;
    u16                  hw;
    u16                  hh;
    s16                  sx;
    s16                  sy;
    s32                  y;
    s16                  w;
    s16                  h;
    s16                  u;
    s16                  v;
    s16                  tx;
    s16                  ty;
    s16                  scale = 0x1000;
    s32                  otz   = 0x3E8;

    if (*(u16*)&((DumpingHoleEntity*)D_shelter_b3_dumping_hole_8018F4A8->work)->field_44 == 1) {
        taskKill(arg0);
        return;
    }

    switch (arg0->state) {
        case 0:
            coord->sub = &gGfxViewCoord;
            Gp_ComposeParentWorld((GsCOORDINATE2*)arg0->spawnArg2, &coord->coord, &vec);
            coord->coord.t[0] = vec.vx + W->field_C;
            coord->coord.t[1] = vec.vy + W->field_E;
            coord->coord.t[2] = vec.vz + W->field_10;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            W->field_16       = 0xFFF6 - ((Gp_LcgState >> 16) & 7);
            W->field_14       = 0;
            W->field_18       = 0;
            W->field_1C       = 0;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            W->field_20       = ((Gp_LcgState >> 16) & 7) + 0x14;
            arg0->state++;
            return;
        case 1:
            if (W->field_20 == 0) {
                arg0->state = 2;
            } else {
                W->field_20--;
            }
            return;
        case 2: {
            s32 t1e     = W->field_1E + 1;
            W->field_1E = t1e;
            if (D_shelter_b3_dumping_hole_8018816C[W->field_1C] < (s16)t1e) {
                *(u16*)&W->field_1C = *(u16*)&W->field_1C + 1;
                W->field_1E         = 0;
                if (*(u16*)&D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_0 == 0xFFFF) {
                    taskKill(arg0);
                    return;
                }
            }
            break;
        }
        default:
            return;
    }

    coord->coord.t[1] += W->field_16;
    w                  = D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_8;
    h                  = D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_A;
    u                  = D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_2;
    v                  = D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_6;
    tx                 = D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_0;
    ty                 = D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_4;
    Gp_UpdateCoord(coord);
    gte_SetTransMatrix(&coord->workm);
    gte_SetRotMatrix(&coord->workm);
    pos.vx = pos.vy = pos.vz = 0;
    gte_ldv0(&pos);
    gte_rtps();
    gte_stsxy(&sxy);
    sx        = sxy.vx;
    y         = sxy.vy;
    sy        = y;
    offscreen = _shelterB3DumpingHoleIsOffscreen(sx, y);
    if (offscreen) {
        taskKill(arg0);
        return;
    }
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(prim + 1);
    setPolyFT4(prim);
    setSemiTrans(prim, 1);
    prim->r0 = prim->g0 = prim->b0 = 0x20;
    hw                             = w * scale / 4096;
    hh                             = h * scale / 4096;
    setXY4(prim, sx - hw / 2, sy - hh / 2, sx + hw / 2, sy - hh / 2, sx - hw / 2, sy + hh / 2, sx + hw / 2, sy + hh / 2);
    setUV4(prim, u, v, u + w - 1, v, u, v + h - 1, u + w - 1, v + h - 1);
    prim->clut  = 0x43C0;
    prim->tpage = getTPage(0, 1, (tx / 64) * 64, (ty / 256) * 256);
    addPrim(&gGpuCurrentOt[otz >> 4], prim);
    coord->flg = 0;
}

void func_shelter_b3_dumping_hole_8017E440(Task* arg0)
{
    DumpingHoleAnimWork* work  = (DumpingHoleAnimWork*)arg0->work;
    GsCOORDINATE2*       coord = ((TmdObject*)arg0->extra)->coords;
    SVECTOR              vec;
    s32                  sa1;
    u32                  roll1;
    u32                  roll2;
    s32                  base18;
    s16                  var0;
    s16                  delta;

    if (*(u16*)&((DumpingHoleEntity*)D_shelter_b3_dumping_hole_8018F4A8->work)->field_48 == 1) {
        taskKill(arg0);
        return;
    }

    switch (arg0->state) {
        case 0:
            coord->sub = &gGfxViewCoord;
            Gp_ComposeParentWorld((GsCOORDINATE2*)arg0->spawnArg2, &coord->coord, &vec);
            coord->coord.t[0] = vec.vx;
            coord->coord.t[1] = vec.vy;
            coord->coord.t[2] = vec.vz;
            arg0->work        = (TaskIdMap*)Mem_Malloc(0x24, 0);
            if (arg0->work == NULL) {
                taskKill(arg0);
                return;
            }
            work = (DumpingHoleAnimWork*)arg0->work;
            Mem_Set(work, 0, 0x24);
            work->field_16 = -0xA;
            work->field_14 = 0;
            work->field_18 = 0;
            work->field_8  = 0x1000;
            work->field_14 = 0;
            roll1          = Gp_LcgState * 5 + 0x71357911;
            work->field_16 = 0xFFF1 - ((roll1 >> 16) & 7);
            sa1            = arg0->spawnArg1;
            Gp_LcgState    = roll1;
            if (sa1 == 0) {
                roll2       = roll1 * 5 + 0x71357911;
                base18      = work->field_18;
                Gp_LcgState = roll2;
                if ((roll2 >> 16) & 1) {
                    Gp_LcgState = roll2 * 5 + 0x71357911;
                    var0        = base18 + ((Gp_LcgState >> 16) & 1);
                } else {
                    Gp_LcgState = roll2 * 5 + 0x71357911;
                    var0        = base18 - ((Gp_LcgState >> 16) & 1);
                }
                work->field_18 = var0;
            } else {
                if (sa1 < 0) {
                    Gp_LcgState = roll1 * 5 + 0x71357911;
                    delta       = *(u16*)&work->field_18 + ((u16)arg0->spawnArg1 - ((Gp_LcgState >> 16) & 1));
                } else {
                    Gp_LcgState = roll1 * 5 + 0x71357911;
                    delta       = *(u16*)&work->field_18 + ((u16)arg0->spawnArg1 + ((Gp_LcgState >> 16) & 1));
                }
                work->field_18 = delta;
            }
            work->field_1C = 0;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_20 = (Gp_LcgState >> 16) & 7;
            arg0->state++;
            return;
        case 1:
            if (work->field_20 == 0) {
                arg0->state = 2;
            } else {
                work->field_20--;
            }
            return;
        case 2: {
            s32 t1e        = work->field_1E + 1;
            work->field_1E = t1e;
            if (D_shelter_b3_dumping_hole_80188184[work->field_1C] < (s16)t1e) {
                *(u16*)&work->field_1C = *(u16*)&work->field_1C + 1;
                work->field_1E         = 0;
                if (*(u16*)&D_shelter_b3_dumping_hole_801880B8[work->field_1C].field_0 == 0xFFFF) {
                    taskKill(arg0);
                    return;
                }
            }
            coord->coord.t[1] += work->field_16;
            coord->coord.t[2] += work->field_18;
            func_shelter_b3_dumping_hole_8017DA00(
                coord,
                D_shelter_b3_dumping_hole_801880B8[work->field_1C].field_8,
                D_shelter_b3_dumping_hole_801880B8[work->field_1C].field_A,
                D_shelter_b3_dumping_hole_801880B8[work->field_1C].field_2,
                D_shelter_b3_dumping_hole_801880B8[work->field_1C].field_6,
                D_shelter_b3_dumping_hole_801880B8[work->field_1C].field_0,
                D_shelter_b3_dumping_hole_801880B8[work->field_1C].field_4,
                work->field_8, 0x43C0, 0);
            coord->flg = 0;
            return;
        }
    }
}

void func_shelter_b3_dumping_hole_8017E7DC(Task* arg0)
{
    DumpingHoleCoordWork*         work;
    register TmdObject*           extra asm("s2");
    GsCOORDINATE2*                coord;
    register DumpingHoleCoordCfg* cfg asm("s3");
    VECTOR                        v;
    TmdObject*                    e2;

    extra      = (TmdObject*)arg0->extra;
    cfg        = (DumpingHoleCoordCfg*)arg0->spawnArg2;
    coord      = extra->coords;
    work       = (DumpingHoleCoordWork*)Mem_Malloc(0x5C, 0);
    arg0->work = (TaskIdMap*)work;
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    Mem_Set(work, 0, 0x5C);
    coord->sub                       = &gGfxViewCoord;
    ((TmdObject*)arg0->extra)->flags = 0;
    Tmd_AllocBuffers(extra);
    extra->lightMtx   = &work->field_0;
    extra->colorMtx   = &work->field_20;
    coord->coord.t[0] = cfg->field_0;
    coord->coord.t[1] = cfg->field_4;
    coord->coord.t[2] = cfg->field_8;
    Gfx_RotMatrixY(&coord->coord, cfg->field_12, 1);
    Gfx_RotMatrixX(&coord->coord, cfg->field_10, 0);
    Gfx_RotMatrixZ(&coord->coord, cfg->field_14, 0);
    coord->flg = 0;
    Task_Reparent(D_shelter_b3_dumping_hole_8018F4A8, arg0);
    Gp_UpdateCoord(coord);
    e2   = (TmdObject*)arg0->extra;
    v.vx = e2->coords->workm.t[0];
    v.vy = ((TmdObject*)arg0->extra)->coords->workm.t[1];
    v.vz = ((TmdObject*)arg0->extra)->coords->workm.t[2];
    func_800D7A9C(e2, &v, 0, 3);
}

/// Debris thrown from the hole. Once the room signals, the piece is projected
/// to the screen: off-screen or behind the camera it is dropped, otherwise it
/// is launched away from the screen centre with a random speed and spin, and
/// then falls under a growing downward speed.
void func_shelter_b3_dumping_hole_8017E94C(Task* arg0)
{
    DumpingHoleCoordWork* work  = (DumpingHoleCoordWork*)arg0->work;
    u16                   flag  = ((DumpingHoleEntity*)D_shelter_b3_dumping_hole_8018F4A8->work)->field_40;
    GsCOORDINATE2*        coord = ((TmdObject*)arg0->extra)->coords;
    GsCOORDINATE2*        c2;
    DumpingHoleProjection p;
    s32                   sx;
    s32                   sy;
    s16                   angle;
    s32                   x;
    s32                   y;
    s32                   z;

    if (flag == 2) {
        taskKill(arg0);
        return;
    }
    switch (arg0->state) {
        case 0:
            func_shelter_b3_dumping_hole_8017E7DC(arg0);
            arg0->state++;
            return;
        case 1:
            if (flag == 1) {
                arg0->state = 2;
            }
            return;
        case 2:
            p.pos.vx = coord->workm.t[0];
            p.pos.vy = coord->workm.t[1];
            p.pos.vz = coord->workm.t[2];
            gte_SetTransMatrix(&GsWSMATRIX);
            gte_SetRotMatrix(&GsWSMATRIX);
            gte_ldv0(&p.pos);
            gte_rtps();
            gte_stsxy(&p.sx);
            gte_stszotz(&p.otz);
            sx = p.sx;
            sy = p.sy;
            if (sx < -0xA0) {
                taskKill(arg0);
                return;
            }
            if (sx > 0xA0) {
                taskKill(arg0);
                return;
            }
            if (sy < -0x78) {
                taskKill(arg0);
                return;
            }
            if (sy > 0x78) {
                taskKill(arg0);
                return;
            }
            if (p.otz < 0) {
                taskKill(arg0);
                return;
            }
            angle      = ratan2(sy, sx);
            work->velZ = rcos(angle) * ((DUMPING_HOLE_RAND() & 7) + 0x11) / 4096;
            work->velY = rsin(angle) * ((DUMPING_HOLE_RAND() & 7) + 5) / 4096;
            switch (arg0->spawnArg1) {
                case 0:
                    work->velX = (DUMPING_HOLE_RAND() & 0x1F) + 0x32;
                    break;
                case 1:
                    work->velX = (DUMPING_HOLE_RAND() & 0x1F) + 0x28;
                    break;
                case 2:
                    work->velX = (DUMPING_HOLE_RAND() & 0x1F) + 0x1E;
                    break;
            }
            x = DUMPING_HOLE_RAND() & 0x7F;
            if (DUMPING_HOLE_RAND() & 0x8000) {
                x = -x;
            }
            work->spinX = x;
            y           = DUMPING_HOLE_RAND() & 0x7F;
            if (DUMPING_HOLE_RAND() & 0x8000) {
                y = -y;
            }
            work->spinY = y;
            z           = DUMPING_HOLE_RAND() & 0x7F;
            if (DUMPING_HOLE_RAND() & 0x8000) {
                z = -z;
            }
            work->spinZ = z;
            work->fall  = 0;
            arg0->state++;
            return;
        case 3:
            work->rotX     += work->spinX;
            work->rotY     += work->spinY;
            work->rotZ     += work->spinZ;
            work->fall     += 5;
            c2              = ((TmdObject*)arg0->extra)->coords;
            c2->sub         = &gGfxViewCoord;
            c2->coord.t[0] += work->velX;
            c2->coord.t[1] += work->velY + work->fall;
            c2->coord.t[2] += work->velZ;
            Gfx_RotMatrixY(&c2->coord, (s16)work->rotY, 1);
            Gfx_RotMatrixX(&c2->coord, (s16)work->rotX, 0);
            c2->flg = 0;
            return;
    }
}

void func_shelter_b3_dumping_hole_8017EDB8(Task* arg0)
{
    DumpingHoleEntity* work = (DumpingHoleEntity*)arg0->work;
    union {
        GpAnimArg anim;
        GpCmdArg  loc;
    } msg;
    u8 area;

    if (work->field_24 != NULL) {
        Gp_DispatchMsg(work->field_24, 0x3ED, 0, 0);
    }
    switch (work->field_30) {
        case 0:
            break;
        case 1:
            switch (work->field_32) {
                case 0:
                    Gp_DispatchMsg(work->field_24, 0x3E9, (s32)D_shelter_b3_dumping_hole_8018819C, 0);
                    Gp_DispatchMsg(work->field_24, 0x3F2, (s32)&D_shelter_b3_dumping_hole_8018819C[6], 0);
                    work->field_32++;
                    return;
                case 1:
                    if (Gp_DispatchMsg(work->field_24, 0x3F0, 0, 0) == 0) {
                        work->field_34 = 0;
                        work->field_32++;
                    }
                    return;
                case 2:
                    if (++work->field_34 < 6) {
                        return;
                    }
                    {
                        DumpingHoleEntity* w2       = (DumpingHoleEntity*)arg0->work;
                        s32                weaponId = D_80073BA9;
                        msg.anim.animBlock.index    = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
                        msg.anim.field_4            = 0x2F;
                        msg.anim.field_8            = 1;
                        msg.anim.field_C            = 0xA;
                        msg.anim.field_10           = 0;
                        Gp_DispatchMsg(w2->field_24, 0x3E8, (s32)&msg, 0);
                    }
                    break;
                default:
                    return;
            }
            break;
        case 2: {
            DumpingHoleEntity* w2       = (DumpingHoleEntity*)arg0->work;
            s32                weaponId = D_80073BA9;
            msg.anim.animBlock.index    = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.anim.field_4            = 0x32;
            msg.anim.field_8            = 0;
            msg.anim.field_C            = 0;
            msg.anim.field_10           = 0;
            Gp_DispatchMsg(w2->field_24, 0x3E8, (s32)&msg, 0);
        } break;
        case 3:
            Gp_DispatchMsg(work->field_24, 0x3F3, 2, 0);
            break;
        case 4:
            Gp_DispatchMsg(work->field_24, 0x3F3, 1, 0);
            Gp_DispatchMsg(work->field_24, 0x3E9, (s32)&D_shelter_b3_dumping_hole_801881CC, 0);
            {
                DumpingHoleEntity* w2       = (DumpingHoleEntity*)arg0->work;
                s32                weaponId = D_80073BA9;
                msg.anim.animBlock.index    = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
                msg.anim.field_4            = 9;
                msg.anim.field_8            = 0;
                msg.anim.field_C            = 0;
                msg.anim.field_10           = 0;
                Gp_DispatchMsg(w2->field_24, 0x3E8, (s32)&msg, 0);
            }
            break;
        case 5:
            switch (work->field_32) {
                case 0:
                    msg.loc.from.loc.stage = gGameSession->at4.loc.stage;
                    area                   = gGameSession->at4.loc.area;
                    msg.loc.command        = 1;
                    msg.loc.from.loc.area  = area;
                    Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
                    work->field_34 = 0;
                    work->field_32++;
                    return;
                case 1:
                    if (++work->field_34 < 0x10) {
                        return;
                    }
                    {
                        DumpingHoleEntity* w2       = (DumpingHoleEntity*)arg0->work;
                        s32                weaponId = D_80073BA9;
                        msg.anim.animBlock.index    = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
                        msg.anim.field_4            = 0x30;
                        msg.anim.field_8            = 1;
                        msg.anim.field_C            = 0xA;
                        msg.anim.field_10           = 0;
                        Gp_DispatchMsg(w2->field_24, 0x3E8, (s32)&msg, 0);
                    }
                    break;
                default:
                    return;
            }
            break;
        case 6: {
            DumpingHoleEntity* w2       = (DumpingHoleEntity*)arg0->work;
            s32                weaponId = D_80073BA9;
            msg.anim.animBlock.index    = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.anim.field_4            = 0x33;
            msg.anim.field_8            = 1;
            msg.anim.field_C            = 0xA;
            msg.anim.field_10           = 0;
            Gp_DispatchMsg(w2->field_24, 0x3E8, (s32)&msg, 0);
        }
            Gp_DispatchMsg(work->field_24, 0x3FD, 0x20, 0);
            break;
        case 7: {
            DumpingHoleEntity* w2       = (DumpingHoleEntity*)arg0->work;
            s32                weaponId = D_80073BA9;
            msg.anim.animBlock.index    = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            msg.anim.field_4            = 0x31;
            msg.anim.field_8            = 1;
            msg.anim.field_C            = 0xA;
            msg.anim.field_10           = 0;
            Gp_DispatchMsg(w2->field_24, 0x3E8, (s32)&msg, 0);
        } break;
    }
    work->field_30 = 0;
}

void func_shelter_b3_dumping_hole_8017F1B0(Task* arg0)
{
    DumpingHoleEntity*      work = (DumpingHoleEntity*)arg0->work;
    GpCmdArg                msg;
    DumpingHoleDebrisSeed   seed;
    DumpingHoleDebrisEntry* e;
    DumpingHoleDebrisEntry* p;
    u16                     i;
    u8                      area;

    switch (work->field_38) {
        case 0:
            break;
        case 1:
            switch (work->field_3A) {
                case 0:
                    work->field_40 = 1;
                    work->field_42 = 2;
                    work->field_3C = 0;
                    work->field_3A++;
                    return;
                case 1:
                    if (++work->field_3C < 3) {
                        return;
                    }
                    Gp_DispatchMsg(((DumpingHoleEntity*)D_shelter_b3_dumping_hole_8018F4A8->work)->field_28, 0x7D5, 1, 0);
                    msg.from.loc.stage = gGameSession->at4.loc.stage;
                    area               = gGameSession->at4.loc.area;
                    msg.command        = 2;
                    msg.from.loc.area  = area;
                    Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
                    break;
                default:
                    return;
            }
            break;
        case 2:
            Gp_DispatchMsg(work->field_28, 0x7D4, (s32)work, 0);
            msg.from.loc.stage = gGameSession->at4.loc.stage;
            area               = gGameSession->at4.loc.area;
            msg.command        = 3;
            msg.from.loc.area  = area;
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
            break;
        case 4:
            msg.from.loc.stage = gGameSession->at4.loc.stage;
            area               = gGameSession->at4.loc.area;
            msg.command        = 5;
            msg.from.loc.area  = area;
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
            break;
        case 5:
            Gp_DispatchMsg(work->field_2C, 0x7D4, (s32)&D_shelter_b3_dumping_hole_801881E4, 0);
            break;
        case 6:
            switch (work->field_3A) {
                case 0:
                    work->field_44 = 1;
                    work->field_3A++;
                    return;
                case 1:
                    for (i = 0; D_shelter_b3_dumping_hole_801881FC[i].field_0 != 0xFFFF; i++) {
                        Task_SpawnFromTable(&D_shelter_b3_dumping_hole_80188BC8, 2, 0, (s32)&D_shelter_b3_dumping_hole_801881FC[i]);
                    }
                    for (i = 0; D_shelter_b3_dumping_hole_80188304[i].field_0 != 0xFFFF; i++) {
                        Task_SpawnFromTable(&D_shelter_b3_dumping_hole_80188BC8, 3, 1, (s32)&D_shelter_b3_dumping_hole_80188304[i]);
                    }
                    for (i = 0; D_shelter_b3_dumping_hole_801884CC[i].x != 0xFFFF; i++) {
                        e = &D_shelter_b3_dumping_hole_801884CC[i];
                        Task_SpawnFromTable(&D_shelter_b3_dumping_hole_80188BC8, 4, 2, (s32)e);
                        if (e->field_18 != 0) {
                            p            = e;
                            seed.x       = p->x;
                            seed.y       = p->y;
                            seed.z       = p->z;
                            seed.field_8 = 0x1000;
                            seed.field_A = 1;
                            DUMPING_HOLE_SPAWN_DEBRIS(seed);
                            seed.y = p->y + 200;
                            seed.z = p->z + 200;
                            DUMPING_HOLE_SPAWN_DEBRIS(seed);
                            seed.y = p->y + 200;
                            seed.z = p->z - 200;
                            DUMPING_HOLE_SPAWN_DEBRIS(seed);
                            seed.y = p->y - 200;
                            seed.z = p->z + 200;
                            DUMPING_HOLE_SPAWN_DEBRIS(seed);
                            seed.y = p->y - 200;
                            seed.z = p->z - 200;
                            DUMPING_HOLE_SPAWN_DEBRIS(seed);
                        }
                    }
                    break;
                default:
                    return;
            }
            break;
        case 7:
            work->field_42 = 1;
            work->field_40 = 2;
            break;
    }
    work->field_38 = 0;
}

void func_shelter_b3_dumping_hole_8017F820(Task* arg0)
{
    DumpingHoleEntity* work;
    DumpingHoleEntity* w;
    Task*              t;
    DumpingHoleEntity* w2;
    GpCopyArg          msg;
    GpAnimArg          anim;
    GpAnimArg*         p;
    s32                n;
    s32                weaponId;

    switch (arg0->state) {
        case 0:
            work       = (DumpingHoleEntity*)Mem_Malloc(0x50, 0);
            arg0->work = work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                Mem_Set(work, 0, 0x50);
                work->field_24                     = gameGetPtrSlot(3);
                D_shelter_b3_dumping_hole_8018F4A8 = arg0;
                work->field_28                     = (Task*)Gp_FindWorkById(gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8))->field_0;
                work->field_2C                     = (Task*)Gp_FindWorkById((gGameSession->at4.loc.stage << 8) | (u16)(gGameSession->at4.loc.area | 0x1000))->field_0;
                work->field_42                     = 0;
                work->field_40                     = 0;
                work->field_4A                     = 0;
                work->field_48                     = 0;
                work->field_46                     = 0;
            }
            w             = (DumpingHoleEntity*)arg0->work;
            w->field_0    = ((TmdObject*)w->field_28->extra)->coords->coord.t[0];
            t             = w->field_28;
            w->field_4    = ((TmdObject*)t->extra)->coords->coord.t[1];
            w->field_8    = ((TmdObject*)t->extra)->coords->coord.t[2];
            w->field_12   = 0x400;
            w->field_10   = 0;
            w->field_14   = 0;
            D_8007272D[0] = 0xC;
            D_80062735    = 3;
            arg0->state++;
            break;
        case 1:
            if (gGameSession->eventState != 0) {
                break;
            }
            if (Gp_CapBusy() != 0) {
                break;
            }
            if (D_80114C12 == 1 || D_80071075 != 0 || Player_Status.coordMtx->t[0] < 0x36B1) {
                break;
            }
            w2 = (DumpingHoleEntity*)arg0->work;
            n  = 0;
            while (D_shelter_b3_dumping_hole_801880A0[n & 0xFFFF] != 0) {
                n += 1;
            }
            msg.words = &D_shelter_b3_dumping_hole_801880A0[0];
            msg.count = n & 0xFFFF;
            Gp_DispatchMsg(w2->field_24, 0x3F7, (s32)&msg, 0);
            weaponId             = D_80073BA9;
            p                    = &anim;
            anim.animBlock.index = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            p->field_4           = 1;
            p->field_8           = 1;
            p->field_C           = 0xA;
            anim.field_10        = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&anim, 0);
            arg0->state++;
            break;
        case 2:
            func_800E8634((s32)&D_shelter_b3_dumping_hole_80188640, 0, (s32)&D_shelter_b3_dumping_hole_80188A78);
            arg0->state++;
            break;
        case 3:
            if (gGameSession->eventState == 0) {
                D_shelter_b3_dumping_hole_8018EF04.field_4A &= ~0x40;
                taskKill(arg0);
                return;
            }
            func_shelter_b3_dumping_hole_8017EDB8(arg0);
            func_shelter_b3_dumping_hole_8017F1B0(arg0);
            break;
    }
}

s16 func_shelter_b3_dumping_hole_8017FB70(void)
{
    if (gGameSession->at4.loc.room == 2) {
        return 0;
    }
    return D_shelter_b3_dumping_hole_8018809C;
}

void func_shelter_b3_dumping_hole_8017FBA0(Task* arg0)
{
    OverlayFadeWork*   fade;
    OverlayFadeWork*   alloc;
    DumpingHoleEntity* ent;

    ent  = D_shelter_b3_dumping_hole_8018F4A8->work;
    fade = (OverlayFadeWork*)arg0->work;
    if (ent->field_4C == 1) {
        taskKill(arg0);
        return;
    }
    switch (arg0->state) {
        case 0:
            alloc      = (OverlayFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            fade         = alloc;
            fade->b      = 0xFF;
            fade->g      = 0xFF;
            fade->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->r, 2);
            fade->r -= (u16)arg0->spawnArg1;
            fade->g -= (u16)arg0->spawnArg1;
            fade->b -= (u16)arg0->spawnArg1;
            if (fade->r < 0) {
                taskKill(arg0);
            }
            break;
    }
}

/// Sends message 0x7DA to the slot-4 task, tagged with the current session's
/// stage and area and the caller's selector. The actors' shared library carries
/// the same body.
void func_shelter_b3_dumping_hole_8017FCA0(s16 arg0)
{
    GpCmdArg msg;

    msg.from.loc.stage = gGameSession->at4.loc.stage;
    msg.from.loc.area  = gGameSession->at4.loc.area;
    msg.command        = arg0;
    Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
}

void func_shelter_b3_dumping_hole_8017FCF4(Task* arg0, DumpingHoleSpawnArg* arg1)
{
    Task*                 task;
    DumpingHoleSpawnWork* work;

    task       = Task_SpawnFromTable(&D_shelter_b3_dumping_hole_80188C04, 1, 0, (s32)arg0);
    work       = (DumpingHoleSpawnWork*)Mem_Malloc(0x24, 0);
    task->work = (TaskIdMap*)work;
    if (work == NULL) {
        taskKill(task);
        return;
    }
    Mem_Set(work, 0, 0x24);
    work->field_C  = arg1->field_0;
    work->field_E  = arg1->field_2;
    work->field_10 = arg1->field_4;
}

void func_shelter_b3_dumping_hole_8017FD9C(s32 arg0, s32 arg1)
{
    if ((arg1 << 0x10) == 0) {
        Task_SpawnFromTable(&D_shelter_b3_dumping_hole_80188C04, 3, 0, arg0);
        Task_SpawnFromTable(&D_shelter_b3_dumping_hole_80188C04, 3, -0xA, arg0);
        Task_SpawnFromTable(&D_shelter_b3_dumping_hole_80188C04, 3, 0xA, arg0);
    }
}

void func_shelter_b3_dumping_hole_8017FE10(s32 arg0)
{
    DumpingHoleEntity* p = D_shelter_b3_dumping_hole_8018F4A8->work;
    if (arg0 == 0) {
        p->field_48 = 1;
    }
}

void func_shelter_b3_dumping_hole_8017FE34(void)
{
    Task_SpawnFromTable(&D_shelter_b3_dumping_hole_80188BC8, 1, 9, 0);
}

void func_shelter_b3_dumping_hole_8017FE64(s32 arg0)
{
    DumpingHoleEntity* p = D_shelter_b3_dumping_hole_8018F4A8->work;
    Gp_DispatchMsg(p->field_28, 0x7D5, arg0, 0);
}

void func_shelter_b3_dumping_hole_8017FE9C(s32 arg0)
{
    DumpingHoleEntity* p = D_shelter_b3_dumping_hole_8018F4A8->work;
    Gp_DispatchMsg(p->field_2C, 0x7D5, arg0, 0);
}

void func_shelter_b3_dumping_hole_8017FED4(s16 arg0)
{
    DumpingHoleEntity* p = D_shelter_b3_dumping_hole_8018F4A8->work;
    p->field_30          = arg0;
    p->field_32          = 0;
}

void func_shelter_b3_dumping_hole_8017FEF4(s16 arg0)
{
    DumpingHoleEntity* p = D_shelter_b3_dumping_hole_8018F4A8->work;
    p->field_38          = arg0;
    p->field_3A          = 0;
}

void func_shelter_b3_dumping_hole_8017FF14(void)
{
    register Task*              st asm("s0")  = D_shelter_b3_dumping_hole_8018F4A8;
    register DumpingHoleEntity* ent asm("s1") = st->work;
    DumpingHoleEntity*          ent2;
    s32                         desc[5];

    Gp_DispatchMsg(ent->field_2C, 0x7D5, 2, 0);
    Gp_DispatchMsg(ent->field_24, 0x3F3, 1, 0);
    Gp_DispatchMsg(ent->field_24, 0x3E9, (s32)&D_shelter_b3_dumping_hole_801881CC, 0);
    ent2    = st->work;
    desc[0] = D_80073BA9 + (D_8007218A == 1 ? 1 : 0x22);
    desc[1] = 9;
    desc[2] = 0;
    desc[3] = 0;
    desc[4] = 0;
    Gp_DispatchMsg(ent2->field_24, 0x3E8, (s32)desc, 0);
    ent->field_40 = 2;
    ent->field_46 = 1;
    ent->field_42 = 1;
    ent->field_4C = 1;
    ent->field_44 = 1;
    CdCmd_CancelReplaceAndActivate();
}

void func_shelter_b3_dumping_hole_8017FFF4(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

void func_shelter_b3_dumping_hole_80180014(void)
{
    CdCmd_EnqueueOverlay81();
}

void func_shelter_b3_dumping_hole_80180034(void)
{
    Gp_RestoreStreamRng();
    CdCmd_CancelReplaceAndActivate();
}

/// A shard thrown out of the hole, alive only while the room flag is set. On
/// its first frame it places itself from the spawn record and randomises its
/// velocity, spin and triangle shape; afterwards it falls and spins, is dropped
/// once its origin leaves the screen or passes behind the camera, and otherwise
/// draws itself as a shaded triangle.
void func_shelter_b3_dumping_hole_8018005C(Task* arg0)
{
    DumpingHoleShard*    work;
    GsCOORDINATE2*       coord;
    DumpingHoleShardCfg* cfg;
    POLY_G3*             prim;
    SVECTOR              ofs;
    s16                  x[3];
    s16                  y[3];
    SVECTOR              origin;
    s32                  sxy;
    s32                  otz;
    s16                  i;
    s16                  sx;
    s16                  sy;
    s32                  v;
    u32                  r;
    s32                  s;
    s32                  q;
    s32                  w;
    s32                  wz;
    s32                  bx;
    s32                  by;
    s32                  bz;
    s32                  size;

    work  = (DumpingHoleShard*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    cfg   = (DumpingHoleShardCfg*)arg0->spawnArg2;
    if (D_shelter_b3_dumping_hole_8018F4B0 == 0) {
        taskKill(arg0);
        return;
    }
    switch (arg0->state) {
        case 0:
            arg0->work = memCalloc(0x34, 0);
            if (arg0->work == NULL) {
                goto kill;
            }
            work       = (DumpingHoleShard*)arg0->work;
            coord->sub = &gGfxViewCoord;
            Mem_Set(arg0->work, 0, 0x34);
            Gp_ComposeParentWorld(cfg->parent, &coord->coord, &ofs);
            coord->coord.t[0] = ofs.vx + cfg->pos.vx;
            coord->coord.t[1] = ofs.vy + cfg->pos.vy;
            coord->coord.t[2] = ofs.vz + cfg->pos.vz;
            bx                = cfg->vel.vx;
            if (DUMPING_HOLE_RAND() & 1) {
                w = bx + (DUMPING_HOLE_RAND() & 0x1F);
            } else {
                w = bx - (DUMPING_HOLE_RAND() & 0x1F);
            }
            work->vel.vx = w;
            by           = cfg->vel.vy;
            if (DUMPING_HOLE_RAND() & 1) {
                w = by + (DUMPING_HOLE_RAND() & 0x1F);
            } else {
                w = by - (DUMPING_HOLE_RAND() & 0x1F);
            }
            work->vel.vy = w;
            bz           = cfg->vel.vz;
            if (DUMPING_HOLE_RAND() & 1) {
                wz = bz + (DUMPING_HOLE_RAND() & 0x1F);
            } else {
                wz = bz - (DUMPING_HOLE_RAND() & 0x1F);
            }
            work->vel.vz      = wz;
            work->fall        = cfg->fall;
            work->rotSpeed.vx = (DUMPING_HOLE_RAND() & 1) ? (DUMPING_HOLE_RAND() & 0x7F) : -(DUMPING_HOLE_RAND() & 0x7F);
            work->rotSpeed.vy = (DUMPING_HOLE_RAND() & 1) ? (DUMPING_HOLE_RAND() & 0x7F) : -(DUMPING_HOLE_RAND() & 0x7F);
            work->rotSpeed.vz = (DUMPING_HOLE_RAND() & 1) ? (DUMPING_HOLE_RAND() & 0x7F) : -(DUMPING_HOLE_RAND() & 0x7F);
            if (work->rotSpeed.vx > 0) {
                work->rotSpeed.vx += 100;
            } else {
                work->rotSpeed.vx -= 100;
            }
            if (work->rotSpeed.vy > 0) {
                work->rotSpeed.vy += 100;
            } else {
                work->rotSpeed.vy -= 100;
            }
            if (work->rotSpeed.vz > 0) {
                work->rotSpeed.vz += 100;
            } else {
                work->rotSpeed.vz -= 100;
            }
            work->verts[0].vx = 0;
            size              = cfg->size;
            wz                = (DUMPING_HOLE_RAND() & 1) ? size + (u16)(cfg->size / 10) : size;
            work->verts[0].vy = wz;
            work->verts[0].vz = 0;
            v                 = cfg->size * rsin(0x2AA);
            if (v < 0) {
                v += 0xFFF;
            }
            r           = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = r;
            r         >>= 16;
            // Matching carrier, not reconstructed source: the empty asm ties
            // the shift below to the draw, so it is scheduled after the store.
            SOFT_TOUCH_REG_USE(v, r);
            q = v >> 12;
            if (r & 1) {
                w = q + (u16)(cfg->size / 10);
            } else {
                w = q;
            }
            work->verts[1].vx = w;
            v                 = cfg->size * rsin(0x155);
            if (v < 0) {
                v += 0xFFF;
            }
            r = Gp_LcgState * 5 + 0x71357911;
            SOFT_TOUCH_REG_USE(v, r);
            s           = v >> 12;
            Gp_LcgState = r;
            r         >>= 16;
            q           = -s;
            if (r & 1) {
                w = q - (u16)(cfg->size / 10);
            } else {
                w = q;
            }
            work->verts[1].vy = w;
            work->verts[1].vz = 0;
            v                 = cfg->size * rsin(0x2AA);
            if (v < 0) {
                v += 0xFFF;
            }
            r = Gp_LcgState * 5 + 0x71357911;
            SOFT_TOUCH_REG_USE(v, r);
            s           = v >> 12;
            Gp_LcgState = r;
            r         >>= 16;
            q           = -s;
            if (r & 1) {
                w = q - (u16)(cfg->size / 10);
            } else {
                w = q;
            }
            work->verts[2].vx = w;
            v                 = cfg->size * rsin(0x155);
            if (v < 0) {
                v += 0xFFF;
            }
            r = Gp_LcgState * 5 + 0x71357911;
            SOFT_TOUCH_REG_USE(v, r);
            s           = v >> 12;
            Gp_LcgState = r;
            r         >>= 16;
            q           = -s;
            if (r & 1) {
                w = q - (u16)(cfg->size / 10);
            } else {
                w = q;
            }
            work->verts[2].vy = w;
            work->verts[2].vz = 0;
            arg0->state++;
            break;
        case 1:
            work->vel.vy      += work->fall;
            coord->coord.t[0] += work->vel.vx;
            coord->coord.t[1] += work->vel.vy;
            coord->coord.t[2] += work->vel.vz;
            Gp_UpdateCoord(coord);
            gte_SetTransMatrix(&coord->workm);
            gte_SetRotMatrix(&coord->workm);
            origin.vz = 0;
            origin.vy = 0;
            origin.vx = 0;
            gte_ldv0(&origin);
            gte_rtps();
            gte_stsxy(&sxy);
            gte_stszotz(&otz);
            sy = sxy >> 16;
            sx = sxy;
            if (sx < -0xA0) {
                goto kill;
            }
            if (sx > 0xA0 || sy < -0x78 || sy > 0x78 || otz < 0) {
            kill:
                taskKill(arg0);
                break;
            }
            for (i = 0; i < 3; i++) {
                gte_ldv0(&work->verts[i]);
                gte_rtps();
                gte_stsxy(&sxy);
                gte_stszotz(&otz);
                x[i] = sxy;
                y[i] = sxy >> 16;
            }
            prim           = (POLY_G3*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG3(prim);
            setRGB0(prim, 0x10, 0x10, 0x10);
            setRGB1(prim, 0x40, 0x40, 0x40);
            setRGB2(prim, 0x80, 0x80, 0x80);
            prim->x0 = x[0];
            prim->y0 = y[0];
            prim->x1 = x[1];
            prim->y1 = y[1];
            prim->x2 = x[2];
            prim->y2 = y[2];
            addPrim(&gGpuCurrentOt[otz >> 4], prim);
            work->rot.vx += work->rotSpeed.vx;
            work->rot.vy += work->rotSpeed.vy;
            work->rot.vz += work->rotSpeed.vz;
            Gfx_RotMatrixY(&coord->coord, work->rot.vy, 1);
            Gfx_RotMatrixX(&coord->coord, work->rot.vx, 0);
            Gfx_RotMatrixZ(&coord->coord, work->rot.vz, 0);
            coord->flg = 0;
            break;
    }
}

/// Per-frame handler of the event task: runs the command in `state`, which is
/// cleared once handled unless it is a multi-frame sequence.
///
/// - 1 sends the slot-3 task its animation and the slot-4 task message 0x7DA
///   for the current stage and area.
/// - 2 is a sequence stepped by `step`. Step 0 sends the opening messages,
///   overrides the view vector and places the task at its starting pose.
///   Step 1 spawns a burst of table tasks every 16 frames and moves model
///   part 2; from frame 0x3D it also shrinks part 3 along X and Z, rebuilding
///   its rotation from identity each frame, and at frame 0x5B it spawns the
///   closing effects and advances. Until then the pose rotation alternates
///   either side of its starting value each frame. Step 2 keeps moving part 2
///   and, after six frames, tips part 1 about X. Every frame of the sequence
///   also shakes the screen by one unit.
/// - 3 and 5 undo the sequence's overrides and send the slot-4 task message
///   0x7DA; 5 also restores the saved view, resets the slot-3 animation and
///   ends the task in `field_88`.
/// - 4 sends the task in `field_84` message 0x7D5.
/// - 6 is a second sequence that alternates `func_shelter_b3_dumping_hole_80183218`
///   calls, then spawns the task kept in `field_88`.
///
/// The message buffers are unions because the cases share their stack slots.
void func_shelter_b3_dumping_hole_8018098C(Task* task)
{
    register short      t4 asm("t4");
    register short      t5 asm("t5");
    register short      t6 asm("t6");
    DumpingHoleEntity4* work;
    OverlayMat*         mtx;
    OverlayMat*         ident;
    OverlayMat*         ident2;
    VECTOR*             sc;
    GsCOORDINATE2*      coords;
    TmdObject*          obj;
    u8*                 head;
    u32                 scratch;
    SVECTOR*            sv;
    u8*                 head2;
    u32                 scratch2;
    u16                 i;
    GpCmdArg*           loc3;
    GpCmdArg*           loc5;
    s32*                p;
    union {
        s32      words[5];
        GpCmdArg loc;
        SVECTOR  vec[4];
    } buf;
    union {
        SVECTOR  vec;
        GpCmdArg loc;
    } buf2;
    s32 words[5];

    work = task->work;
    switch (work->state) {
        case 1:
            Gp_PulseState1C();
            Gp_StateC08.field_6 |= 1;
            buf.words[0]         = D_80073BA9 + (D_8007218A == 1 ? 1 : 0x22);
            buf.words[1]         = 9;
            buf.words[2]         = 1;
            buf.words[3]         = 0xA;
            buf.words[4]         = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)buf.words, 0);
            buf.loc.from.loc.stage = gGameSession->at4.loc.stage;
            buf.loc.from.loc.area  = gGameSession->at4.loc.area;
            buf.loc.command        = 0xA;
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&buf.loc, 0x7DB);
            work->state = 0;
            return;
        case 2:
            switch (work->step) {
                case 0:
                    Gp_DispatchMsg(work->field_80, 0x3F3, 2, 0);
                    buf.vec[0].vx = 0x800;
                    buf.vec[0].vy = 0x800;
                    buf.vec[0].vz = 0x800;
                    Gp_SetOverrideVec(&buf.vec[0]);
                    Gp_DispatchMsg(work->field_84, 0x7D5, 2, 0);
                    Gp_DispatchMsg(task, 0x7D5, 1, 0);
                    work->pose.pos.vx = D_shelter_b3_dumping_hole_8018966C.pos.vx;
                    work->pose.pos.vy = D_shelter_b3_dumping_hole_8018966C.pos.vy;
                    work->pose.pos.vz = D_shelter_b3_dumping_hole_8018966C.pos.vz;
                    work->pose.rot.vx = D_shelter_b3_dumping_hole_8018966C.rot.vx;
                    work->pose.rot.vy = D_shelter_b3_dumping_hole_8018966C.rot.vy;
                    work->pose.rot.vz = D_shelter_b3_dumping_hole_8018966C.rot.vz;
                    Gp_DispatchMsg(task, 0x7D4, (s32)&work->pose, 0);
                    work->field_58.vx                  = 0;
                    work->field_58.vy                  = 0;
                    work->field_58.vz                  = 0;
                    D_shelter_b3_dumping_hole_8018F4B0 = 1;
                    work->field_68                     = &((TmdObject*)task->extra)->coords[2];
                    work->field_6C                     = 0x14;
                    work->scale.vx                     = 0x1000;
                    work->scale.vy                     = 0x1000;
                    work->scale.vz                     = 0x1000;
                    work->timer                        = 0;
                    work->step++;
                    break;
                case 1:
                    work->timer++;
                    if (!(gDisplayState.animFrame & 0xF)) {
                        work->field_60.vx = 0;
                        work->field_60.vy = 0;
                        work->field_60.vz = 0;
                        work->field_6E    = 4;
                        for (i = 0; i < 10; i++) {
                            Task_SpawnFromTable(&D_shelter_b3_dumping_hole_80189ADC, 1, 0, (s32)&work->field_58);
                        }
                    }
                    if (work->timer >= 0x5B) {
                        buf.vec[2].vx = 0xC8;
                        buf.vec[2].vy = 0x190;
                        buf.vec[2].vz = -0xC8;
                        ApplyMatrixSV(&((TmdObject*)task->extra)->coords->coord, &buf.vec[2], &buf.vec[3]);
                        Gp_SpawnEff(0x60070, ((TmdObject*)task->extra)->coords, 0x608, &buf.vec[3]);
                        buf.vec[2].vx = 0xC8;
                        buf.vec[2].vy = 0x190;
                        buf.vec[2].vz = -0x320;
                        ApplyMatrixSV(&((TmdObject*)task->extra)->coords->coord, &buf.vec[2], &buf.vec[3]);
                        Gp_SpawnEff(0x60070, ((TmdObject*)task->extra)->coords, 0x608, &buf.vec[3]);
                        work->timer    = 0;
                        work->field_98 = 0;
                        work->field_9A = 0x80;
                        work->step++;
                        break;
                    }
                    if (work->timer >= 0x1F) {
                        if (work->timer >= 0x3D) {
                            ((TmdObject*)task->extra)->coords[2].coord.t[1] += 8;
                            work->scale.vx                                  -= 10;
                            work->scale.vz                                  -= 10;
                            ident                                            = (OverlayMat*)&((TmdObject*)task->extra)->coords[3].coord;
                            sc                                               = &work->scale;

                            ident->ident.m00_m01 = 0x1000;
                            ident->ident.m02_m10 = 0;
                            ident->ident.m11_m12 = 0x1000;
                            ident->ident.m20_m21 = 0;
                            ident->ident.m22     = 0x1000;

                            // Scale each column of the rotation through a
                            // scratchpad SVECTOR and the GTE.
                            obj = task->extra;
                            __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
                            scratch                = *(u32*)(head + 0x3FC);
                            coords                 = obj->coords;
                            sv                     = (SVECTOR*)(scratch - 8);
                            *(SVECTOR**)0x1F8003FC = sv;
                            TOUCH_REG(sv);
                            mtx = (OverlayMat*)&coords[3].coord;
                            TOUCH_REG(mtx);

                            COMPILER_BARRIER();
                            t4     = mtx->mat.m[0][0];
                            t5     = mtx->mat.m[1][0];
                            t6     = mtx->mat.m[2][0];
                            sv->vx = t4;
                            sv->vy = t5;
                            sv->vz = t6;
                            gte_lddp(sc->vx);
                            gte_ldsv(sv);
                            gte_gpf12();
                            gte_stsv(sv);
                            t4               = sv->vx;
                            t5               = sv->vy;
                            t6               = sv->vz;
                            mtx->mat.m[0][0] = t4;
                            mtx->mat.m[1][0] = t5;
                            mtx->mat.m[2][0] = t6;

                            COMPILER_BARRIER();
                            t4     = mtx->mat.m[0][1];
                            t5     = mtx->mat.m[1][1];
                            t6     = mtx->mat.m[2][1];
                            sv->vx = t4;
                            sv->vy = t5;
                            sv->vz = t6;
                            gte_lddp(sc->vy);
                            gte_ldsv(sv);
                            gte_gpf12();
                            gte_stsv(sv);
                            t4               = sv->vx;
                            t5               = sv->vy;
                            t6               = sv->vz;
                            mtx->mat.m[0][1] = t4;
                            mtx->mat.m[1][1] = t5;
                            mtx->mat.m[2][1] = t6;

                            COMPILER_BARRIER();
                            t4     = mtx->mat.m[0][2];
                            t5     = mtx->mat.m[1][2];
                            t6     = mtx->mat.m[2][2];
                            sv->vx = t4;
                            sv->vy = t5;
                            sv->vz = t6;
                            gte_lddp(sc->vz);
                            gte_ldsv(sv);
                            gte_gpf12();
                            gte_stsv(sv);
                            t4               = sv->vx;
                            t5               = sv->vy;
                            t6               = sv->vz;
                            mtx->mat.m[0][2] = t4;
                            mtx->mat.m[1][2] = t5;
                            mtx->mat.m[2][2] = t6;

                            __asm__ volatile("lui %0, 0x1F80" : "=r"(head2));
                            scratch2  = *(u32*)(head2 + 0x3FC);
                            scratch2 += 8;
                            __asm__ volatile("sw %0, 0x1F8003FC" ::"r"(scratch2) : "memory");
                        } else if (work->timer < 0x20) {
                            ((TmdObject*)task->extra)->coords[2].coord.t[1] += 0x20;
                            ident2                                           = (OverlayMat*)&((TmdObject*)task->extra)->coords[3].coord;

                            ident2->ident.m00_m01 = 0x1000;
                            ident2->ident.m02_m10 = 0;
                            ident2->ident.m11_m12 = 0x1000;
                            ident2->ident.m20_m21 = 0;
                            ident2->ident.m22     = 0x1000;
                        }
                        if (!(gDisplayState.animFrame & 0xF)) {
                            buf.vec[1].vx = 0xC8;
                            buf.vec[1].vy = 0x190;
                            buf.vec[1].vz = -0xC8;
                            ApplyMatrixSV(&((TmdObject*)task->extra)->coords->coord, &buf.vec[1], &buf2.vec);
                            Gp_SpawnEff(0x600E0, ((TmdObject*)task->extra)->coords, 0x200, &buf2.vec);
                            buf.vec[1].vx = 0xC8;
                            buf.vec[1].vy = 0x190;
                            buf.vec[1].vz = -0x320;
                            ApplyMatrixSV(&((TmdObject*)task->extra)->coords->coord, &buf.vec[1], &buf2.vec);
                            Gp_SpawnEff(0x600E0, ((TmdObject*)task->extra)->coords, 0x200, &buf2.vec);
                        }
                    }
                    if (gDisplayState.animFrame & 1) {
                        work->field_98 = work->pose.rot.vx = D_shelter_b3_dumping_hole_8018966C.rot.vx + 0x10;
                    } else {
                        work->field_98 = work->pose.rot.vx = D_shelter_b3_dumping_hole_8018966C.rot.vx - 0x10;
                    }
                    Gp_DispatchMsg(task, 0x7D4, (s32)&work->pose, 0);
                    break;
                case 2:
                    if (!(gDisplayState.animFrame & 0xF)) {
                        buf2.vec.vx = -0x190;
                        buf2.vec.vy = 0x190;
                        buf2.vec.vz = 0x190;
                        Gp_SpawnEff(0x600E0, &((TmdObject*)task->extra)->coords[1], 0x200, &buf2.vec);
                    }
                    if (++work->timer >= 6) {
                        if (work->field_98 < -0x154) {
                            work->field_98 -= 1;
                        } else {
                            work->field_98 -= 8;
                        }
                        Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords[1].coord, work->field_98, 1);
                    }
                    ((TmdObject*)task->extra)->coords[2].coord.t[1] += 0x190;
                    Gp_DispatchMsg(task, 0x7D4, (s32)&work->pose, 0);
                    break;
            }
            if (gDisplayState.animFrame & 1) {
                Display_ClampField126(1);
            } else {
                Display_ClampField126(-1);
            }
            return;
        case 3:
            Gp_SetOverrideVec(NULL);
            Gp_DispatchMsg(work->field_80, 0x3F3, 1, 0);
            Gp_DispatchMsg(work->field_84, 0x7D5, 1, 0);
            Gp_DispatchMsg(task, 0x7D5, 2, 0);
            D_shelter_b3_dumping_hole_8018F4B0 = 0;
            work->field_96                     = 1;
            Gp_PulseState1C();
            buf2.loc.from.loc.stage = gGameSession->at4.loc.stage;
            buf2.loc.from.loc.area  = gGameSession->at4.loc.area;
            loc3                    = &buf2.loc;
            loc3->command           = 0xB;
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)loc3, 0x7DB);
            Display_ClampField126(0);
            work->state = 0;
            return;
        case 4:
            Gp_DispatchMsg(work->field_84, 0x7D5, 2, 0);
            break;
        case 5:
            Mc_SaveData.at4.loc.view = work->field_94;
            Gp_DispatchMsg(work->field_80, 0x3F3, 1, 0);
            Gp_DispatchMsg(work->field_84, 0x7D5, 1, 0);
            work->field_96 = 1;
            Gp_PulseState1C();
            buf2.loc.from.loc.stage = gGameSession->at4.loc.stage;
            buf2.loc.from.loc.area  = gGameSession->at4.loc.area;
            loc5                    = &buf2.loc;
            loc5->command           = 0xC;
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)loc5, 0x7DB);
            p        = words;
            words[0] = D_80073BA9 + (Mc_SaveData.characterId == 1 ? 1 : 0x22);
            p[1]     = 1;
            p[2]     = 1;
            p[3]     = 0xA;
            words[4] = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)words, 0);
            if (work->field_88 != NULL) {
                Task_CallExit(work->field_88);
                work->field_88 = NULL;
            }
            break;
        case 0:
            break;
        case 6:
            switch (work->step) {
                case 1:
                case 3:
                case 5:
                    func_shelter_b3_dumping_hole_80183218(0);
                    work->timer = 0;
                    work->step++;
                    return;
                case 0:
                case 2:
                case 4:
                    func_shelter_b3_dumping_hole_80183218(1);
                    work->timer = 0;
                    work->step++;
                    return;
                case 6:
                    if (++work->timer >= 0xB) {
                        work->field_88 = Task_Spawn(1, 0x2D, 0x10, 0);
                        work->timer    = 0;
                        work->step++;
                    }
                    return;
                case 7:
                    if (++work->timer >= 6) {
                        func_shelter_b3_dumping_hole_80183218(1);
                        func_shelter_b3_dumping_hole_80183218(2);
                        break;
                    }
                    return;
                default:
                    return;
            }
            break;
    }
    work->state = 0;
}

void func_shelter_b3_dumping_hole_80181430(void)
{
    DumpingHoleEntity4* ent;
    GpCmdArg            desc;
    s32                 desc3[5];
    s32*                p3;

    ent = D_shelter_b3_dumping_hole_8018F4AC->work;
    Gp_SetOverrideVec(NULL);
    if (ent->field_88 != NULL) {
        Task_CallExit(ent->field_88);
        ent->field_88 = NULL;
    }
    ent->field_96 = 1;
    Gp_PulseState1C();

    D_shelter_b3_dumping_hole_8018F4B0 = 0;
    desc.from.loc.stage                = gGameSession->at4.loc.stage;
    desc.from.loc.area                 = gGameSession->at4.loc.area;
    desc.command                       = 0x13;
    Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&desc, 0x7DB);

    Display_ClampField126(0);
    Gp_DispatchMsg(ent->field_84, 0x7D5, 1, 0);
    Gp_DispatchMsg(ent->field_80, 0x3F3, 1, 0);

    p3       = desc3;
    desc3[0] = D_80073BA9 + (D_8007218A == 1 ? 1 : 0x22);
    p3[1]    = 1;
    desc3[2] = 0;
    desc3[3] = 0;
    desc3[4] = 0;
    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)desc3, 0);
    CdCmd_CancelReplaceAndActivate();
}

void func_shelter_b3_dumping_hole_80181560(Task* task)
{
    s32                 desc[5];
    TmdObject*          obj;
    TmdObject*          tail;
    DumpingHoleEntity4* work;

    switch (task->state) {
        case 0:
            if (D_80114C12 == 1 || D_80071075 != 0) {
                return;
            }
            obj        = task->extra;
            task->work = memCalloc(0xA0, false);
            if (task->work == NULL) {
                taskKill(task);
            } else {
                ((TmdObject*)task->extra)->coords->sub = &gGfxViewCoord;
                work                                   = task->work;
                Mem_Set(work, 0, 0xA0);
                work->field_80                     = gameGetPtrSlot(3);
                D_shelter_b3_dumping_hole_8018F4AC = task;
                work->field_84                     = (Task*)Gp_FindWorkById(gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8))->field_0;
                obj->lightMtx                      = &work->lightMtx;
                obj->colorMtx                      = &work->colorMtx;
                task->msgTable                     = &D_shelter_b3_dumping_hole_8018965C;
                func_shelter_b3_dumping_hole_80183218(0);
            }
            D_shelter_b3_dumping_hole_8018F4D8          = 0;
            ((DumpingHoleEntity4*)task->work)->field_94 = gGameSession->at4.loc.view;
            Gp_MsgPlayerWeapon(0);
            desc[0] = D_80073BA9 + (D_8007218A == 1 ? 1 : 0x22);
            desc[1] = 9;
            desc[2] = 1;
            desc[3] = 0xA;
            desc[4] = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)desc, 0);
            task->state++;
            break;
        case 1:
            D_shelter_b3_dumping_hole_8018809C = 0;
            func_800E8634((s32)&D_shelter_b3_dumping_hole_8018968C, 0, (s32)&D_shelter_b3_dumping_hole_801899A4);
            task->state++;
            break;
        case 2:
            if (gGameSession->eventState == 0) {
                GameFlag_SetNibble(0x11D, 1);
                taskKill(task);
            } else {
                func_shelter_b3_dumping_hole_8018098C(task);
            }
            break;
    }
    tail    = task->extra;
    desc[0] = tail->coords->workm.t[0];
    desc[1] = ((TmdObject*)task->extra)->coords->workm.t[1];
    desc[2] = ((TmdObject*)task->extra)->coords->workm.t[2];
    func_800D7A9C(tail, (VECTOR*)desc, 0, 3);
}

/// Sets how the task's model is treated from `arg2`: 0 hides it and leaves its
/// primitive buffer to be allocated on demand, 1 shows it with the same
/// allocation, 2 hides it and exempts it from that allocation.
void func_shelter_b3_dumping_hole_801817D8(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* extra;

    extra = (TmdObject*)task->extra;
    switch (arg2) {
        case 0:
            extra->flags = (extra->flags | 0x80) & 0xFFFB;
            return;
        case 1:
            extra->flags = extra->flags & 0xFF7B;
            return;
        case 2:
            extra->flags = extra->flags | 0x84;
            return;
    }
}

/// Places the task's model at `placement`: the position becomes the
/// coordinate's translation, the rotation is applied in Y, X, Z order, and the
/// coordinate is marked for recomputation.
void func_shelter_b3_dumping_hole_80181854(Task* task, s32 arg1, GpXformArg* placement)
{
    GsCOORDINATE2* coord;
    MATRIX*        mtx;

    coord             = ((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    mtx               = &coord->coord;
    coord->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(mtx, placement->rot.vy, 1);
    Gfx_RotMatrixX(mtx, placement->rot.vx, 0);
    Gfx_RotMatrixZ(mtx, placement->rot.vz, 0);
    coord->flg = 0;
}

void func_shelter_b3_dumping_hole_801818E0(void)
{
    DumpingHoleEntity4* p = D_shelter_b3_dumping_hole_8018F4AC->work;
    if (p->field_9C == 0) {
        Gp_ReleaseStateF0Add((Task*)Gp_LookupSlot4(0), 0x20);
        Gp_StateF0.field_6       = 0;
        gGameSession->flowFlags |= 0x80;
        p->field_9C              = 1;
    }
}

void func_shelter_b3_dumping_hole_80181958(s32 arg0)
{
    DumpingHoleEntity4* p = D_shelter_b3_dumping_hole_8018F4AC->work;
    Gp_DispatchMsg(p->field_80, 0x3F3, arg0, 0);
}

void func_shelter_b3_dumping_hole_80181990(s16 arg0)
{
    DumpingHoleEntity4* p = D_shelter_b3_dumping_hole_8018F4AC->work;
    p->state              = arg0;
    p->step               = 0;
}

void func_shelter_b3_dumping_hole_801819B0(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

void func_shelter_b3_dumping_hole_801819D0(void)
{
    CdCmd_EnqueueOverlay81();
}

void func_shelter_b3_dumping_hole_801819F0(void)
{
    Gp_RestoreStreamRng();
    CdCmd_CancelReplaceAndActivate();
}

/// Spawns the task described by `D_shelter_b3_dumping_hole_8018AFBC`.
void func_shelter_b3_dumping_hole_80181A18(void)
{
    Task_SpawnFromTable(&D_shelter_b3_dumping_hole_8018AFBC, 0, 0, 0);
}

void func_shelter_b3_dumping_hole_80181A48(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            arg0->spawnArg1     = 3;
            arg0->killCountdown = 8;
            arg0->state        += 1;
            break;
        case 1:
            if (--arg0->killCountdown < 0) {
                arg0->state += 1;
            }
            Display_ClampField126(arg0->spawnArg1);
            arg0->spawnArg1 = -arg0->spawnArg1;
            break;
        default:
            Display_ClampField126(0);
            taskKill(arg0);
            break;
    }
}

void func_shelter_b3_dumping_hole_80181B04(s16 arg0)
{
    func_shelter_b3_dumping_hole_8017FD9C(
        (s32) & ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords[1], arg0);
}

void func_shelter_b3_dumping_hole_80181B44(s32 arg0)
{
    func_shelter_b3_dumping_hole_8017FE10(arg0);
}

/// Drives the room's timed captions: state 0 arms the task, and each later
/// tick scans `D_shelter_b3_dumping_hole_8018B5A0` for the first window
/// holding `gGameSession->sceneClock` and, on a hit, starts that window's
/// caption at its line key, with the low half of the task's `spawnArg1` as
/// the line delay, and shows its current line. The clock then counts down one, unless a
/// caption is running or `Gp_StateF0.field_4` is set.
void func_shelter_b3_dumping_hole_80181B64(Task* task, s32 arg1)
{
    s32 i;
    s32 script;
    s32 key;
    s32 time;

    switch (task->state) {
        case 0:
            task->state = 1;
            break;
        case 1:
            script = 0;
            key    = arg1;
            for (i = 0; D_shelter_b3_dumping_hole_8018B5A0[i].upper != -1; i++) {
                time = gGameSession->sceneClock;
                if ((D_shelter_b3_dumping_hole_8018B5A0[i].upper * 30 >= time) &&
                    (D_shelter_b3_dumping_hole_8018B5A0[i].lower * 30 < time)) {
                    script = D_shelter_b3_dumping_hole_8018B5A0[i].script;
                    key    = D_shelter_b3_dumping_hole_8018B5A0[i].key;
                    break;
                }
            }
            if (script != 0) {
                func_shelter_b3_dumping_hole_80181E70(script, key, (s16)task->spawnArg1);
                func_shelter_b3_dumping_hole_80181C8C();
            }
            if ((Gp_CapBusy() == 0) && (Gp_StateF0.field_4 == 0)) {
                gGameSession->sceneClock = (u16)gGameSession->sceneClock - 1;
            }
            break;
    }
}

void func_shelter_b3_dumping_hole_80181C8C(void)
{
    if (D_shelter_b3_dumping_hole_8018F4BC == NULL) {
        return;
    }
    if (D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_8 == -1) {
        return;
    }
    if (Gp_CapBusy() != 0) {
        return;
    }
    func_shelter_b3_dumping_hole_80181F80(
        D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_8, 0x80, 1,
        D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_0 |
            ((D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_1 & 0x10)
             << 4));
    if (D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_4 & 1) {
        return;
    }
    func_shelter_b3_dumping_hole_80182AA0();
}

s32 func_shelter_b3_dumping_hole_80181D68(s32 arg0)
{
    Reloc80181D68Hdr*    hdr = (Reloc80181D68Hdr*)arg0;
    Reloc80181D68Entry1* r;
    s32*                 q;
    s32                  n1;
    s32                  n2;
    s32                  i;

    if (strncmp((char*)hdr, "CAP", 3) != 0) {
        return 0;
    }
    if (hdr->field_8 > 0) {
        hdr->field_8  += (s32)hdr;
        hdr->field_C  += (s32)hdr;
        hdr->field_10 += (s32)hdr;
        n1             = ((Reloc80181D68Table1*)hdr->field_C)->count;
        r              = &((Reloc80181D68Table1*)hdr->field_C)->entries[0];
        for (i = 0; i < n1; i++) {
            if (r->field_8 != -1) {
                r->field_8 += (s32)hdr;
            } else {
                r++;
            }
            r++;
        }
        n2 = ((Reloc80181D68Table2*)hdr->field_10)->count;
        q  = &((Reloc80181D68Table2*)hdr->field_10)->entries[0];
        for (i = 0; i < n2; i++) {
            if (*q != 0) {
                *q += (s32)hdr;
            }
            q++;
        }
    }
    D_shelter_b3_dumping_hole_8018F4B8 = (GlyphUvwh*)hdr->field_8;
    D_shelter_b3_dumping_hole_8018F4B4 = hdr->field_10 + 4;
    return 1;
}

s32 func_shelter_b3_dumping_hole_80181E70(s16 arg0, s16 arg1, s32 arg2)
{
    GpEvt12* entry;

    entry                              = ((GpEvt12**)D_shelter_b3_dumping_hole_8018F4B4)[arg0];
    D_shelter_b3_dumping_hole_8018F4BC = entry;
    if (entry == NULL) {
        return 1;
    }
    D_shelter_b3_dumping_hole_8018F4CA = arg1;
    D_shelter_b3_dumping_hole_8018F4C6 = func_shelter_b3_dumping_hole_80182FD0(1);
    D_shelter_b3_dumping_hole_8018F4C4 = arg2;
    D_shelter_b3_dumping_hole_8018F4C0 = func_shelter_b3_dumping_hole_80182C24(
        (u16*)D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_8);
    D_shelter_b3_dumping_hole_8018F4C2 = func_shelter_b3_dumping_hole_801829B4(
        (u16*)D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_8);
    D_shelter_b3_dumping_hole_8018F4C8 = func_shelter_b3_dumping_hole_80182E50(
        D_shelter_b3_dumping_hole_8018F4BC[D_shelter_b3_dumping_hole_8018F4C6].field_8);
    D_shelter_b3_dumping_hole_8018F4D0 = 0x1E;
    return 0;
}

s32 func_shelter_b3_dumping_hole_80181F80(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    u16*       text;
    u16*       body;
    s32        title;
    s16        sc;
    u32        shifted;
    s32        titleWidth;
    s16        lineIdx;
    s16        x;
    s32        y;
    s16        i;
    u16        code;
    s16        centered;
    s32        palette;
    s16        t;
    s16        t2;
    s16        glyphY;
    s32        top;
    POLY_G4*   bg;
    POLY_G4*   bg2;
    DR_MODE*   dm;
    POLY_FT4*  ft;
    POLY_GT4*  gt;
    POLY_GT4*  gt2;
    GlyphUvwh* icon;

    lineIdx = 0;
    title   = arg3;
    text    = (u16*)arg0;
    x       = func_shelter_b3_dumping_hole_80182D34((u16*)arg0, 0) - 0xA0;
    y       = (u16)D_shelter_b3_dumping_hole_8018F4C2 - 0x78;

    bg             = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(bg + 1);
    setlen(bg, 8);
    setcode(bg, 0x3A);
    setRGB0(bg, 0, 0, 0);
    setRGB1(bg, 0, 0, 0);
    setRGB2(bg, 0, 0x40, 0x20);
    setRGB3(bg, 0, 0x40, 0x20);
    bg->x0 = (u16)D_shelter_b3_dumping_hole_8018F4C0 - 0xA7;
    bg->y0 = ((u16)D_shelter_b3_dumping_hole_8018F4C4 - 0x77) - gDisplayState.vramYOffset - (u16)D_shelter_b3_dumping_hole_8018F4C8;
    bg->x1 = (u16)D_shelter_b3_dumping_hole_8018F4C0 - D_shelter_b3_dumping_hole_8018F4C0 * 2 + 0xAB;
    bg->y1 = ((u16)D_shelter_b3_dumping_hole_8018F4C4 - 0x77) - gDisplayState.vramYOffset - (u16)D_shelter_b3_dumping_hole_8018F4C8;
    bg->x2 = (u16)D_shelter_b3_dumping_hole_8018F4C0 - 0xA7;
    bg->y2 = ((u16)D_shelter_b3_dumping_hole_8018F4C4 - 0x77) - gDisplayState.vramYOffset - (u16)D_shelter_b3_dumping_hole_8018F4C8 + (u16)D_shelter_b3_dumping_hole_8018F4C8;
    bg->x3 = (u16)D_shelter_b3_dumping_hole_8018F4C0 - D_shelter_b3_dumping_hole_8018F4C0 * 2 + 0xAB;
    bg->y3 = ((u16)D_shelter_b3_dumping_hole_8018F4C4 - 0x77) - gDisplayState.vramYOffset - (u16)D_shelter_b3_dumping_hole_8018F4C8 + (u16)D_shelter_b3_dumping_hole_8018F4C8;
    addPrim(&gGpuCurrentOt[3], bg);
    bg2            = (POLY_G4*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(bg2 + 1);
    *bg2           = *bg;
    addPrim(&gGpuCurrentOt[3], bg2);
    dm             = (DR_MODE*)gGpuPrimCursor;
    gGpuPrimCursor = (u8*)(dm + 1);
    setlen(dm, 1);
    dm->code[0] = 0xE100020A;
    addPrim(&gGpuCurrentOt[3], dm);

    body = text;
    if (title & 0xFF) {
        ft             = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = (u8*)(ft + 1);
        setlen(ft, 9);
        setcode(ft, 0x2D);
        title      = title - 1;
        top        = ((u16)D_shelter_b3_dumping_hole_8018F4C4 - 0x77) - (u16)D_shelter_b3_dumping_hole_8018F4C8;
        ft->x0     = (u16)D_shelter_b3_dumping_hole_8018F4C0 - 0xA7;
        ft->y0     = (top - gDisplayState.vramYOffset) - D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].h;
        titleWidth = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].w - 0xA7;
        ft->x1     = (u16)D_shelter_b3_dumping_hole_8018F4C0 + titleWidth;
        ft->y1     = (top - gDisplayState.vramYOffset) - D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].h;
        ft->x2     = (u16)D_shelter_b3_dumping_hole_8018F4C0 - 0xA7;
        ft->y2     = top - gDisplayState.vramYOffset;
        titleWidth = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].w - 0xA7;
        ft->x3     = (u16)D_shelter_b3_dumping_hole_8018F4C0 + titleWidth;
        ft->y3     = top - gDisplayState.vramYOffset;
        ft->u0     = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].u;
        ft->v0     = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].v;
        ft->u1     = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].u + D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].w;
        ft->v1     = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].v;
        ft->u2     = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].u;
        ft->v2     = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].v + D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].h;
        ft->u3     = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].u + D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].w;
        ft->v3     = D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].v + D_shelter_b3_dumping_hole_8018F4B8[title & 0xFF].h;
        ft->clut   = 0x3D93;
        ft->tpage  = getTPage(0, 1, D_shelter_b3_dumping_hole_8018B578, D_shelter_b3_dumping_hole_8018B57A);
        addPrim(&gGpuCurrentOt[2], ft);
    }

    centered = 1;
    i        = 0;
    while (1) {
        code    = body[i];
        shifted = (u32)code << 16;
        sc      = (s32)shifted >> 16;
        if (sc == -1) {
            break;
        }
        if (sc == -2) {
            t2                                 = lineIdx + 1;
            lineIdx                            = t2;
            D_shelter_b3_dumping_hole_8018F4CE = y - 2;
            D_shelter_b3_dumping_hole_8018F4CC = x + 4;
            y                                 += func_shelter_b3_dumping_hole_80182F18(&body[i + 1]);
            if (centered != 0) {
                x = func_shelter_b3_dumping_hole_80182D34((u16*)arg0, t2) - 0xA0;
            } else {
                x = (u16)D_shelter_b3_dumping_hole_8018F4C0 - 0xA0;
            }
            i++;
            continue;
        } else if (sc == -3) {
            x += 3;
            i++;
            continue;
        } else if ((code & 0xFF00) == 0x8400) {
            icon           = &D_8010FB70[code & 0xFF];
            ft             = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(ft + 1);
            setlen(ft, 9);
            setcode(ft, 0x2D);
            ft->clut  = 0x3C00;
            ft->tpage = 0x1E;
            t         = (y - gDisplayState.vramYOffset) + 1;
            ft->x0    = x;
            ft->y0    = t - icon->h;
            ft->x1    = x + icon->w;
            ft->y1    = t - icon->h;
            ft->x2    = x;
            ft->y2    = t;
            ft->x3    = x + icon->w;
            ft->y3    = t;
            ft->u0    = icon->u;
            ft->v0    = icon->v;
            ft->u1    = icon->u + icon->w;
            ft->v1    = icon->v;
            ft->u2    = icon->u;
            ft->v2    = icon->v + icon->h;
            ft->u3    = icon->u + icon->w;
            ft->v3    = icon->v + icon->h;
            addPrim(&gGpuCurrentOt[2], ft);
            x += icon->w;
            i++;
            continue;
        } else {
            palette        = (shifted >> 26) & 3;
            code           = code & 0x3FF;
            glyphY         = y - gDisplayState.vramYOffset;
            gt             = (POLY_GT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(gt + 1);
            setcode(gt, 0x3C);
            setlen(gt, 12);
            setShadeTex(gt, 1);
            setRGB0(gt, 0x70, 0x70, 0x70);
            setRGB1(gt, 0x70, 0x70, 0x70);
            setRGB2(gt, 0x70, 0x70, 0x70);
            setRGB3(gt, 0x70, 0x70, 0x70);
            setSemiTrans(gt, 1);
            gt->clut  = palette | 0x3D50;
            gt->x0    = x;
            gt->tpage = getTPage(0, 1, D_shelter_b3_dumping_hole_8018B578, D_shelter_b3_dumping_hole_8018B57A);
            gt->y0    = glyphY - D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].h;
            gt->x1    = x + D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].w;
            gt->y1    = glyphY - D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].h;
            gt->x2    = x;
            gt->y2    = glyphY;
            gt->x3    = x + D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].w;
            gt->y3    = glyphY;
            gt->u0    = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].u;
            gt->v0    = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].v;
            gt->u1    = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].u + D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].w;
            gt->v1    = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].v;
            gt->u2    = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].u;
            gt->v2    = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].v + D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].h;
            gt->u3    = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].u + D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].w;
            gt->v3    = D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].v + D_shelter_b3_dumping_hole_8018F4B8[code & 0x3FF].h;
            addPrim(&gGpuCurrentOt[2], gt);
            gt2            = (POLY_GT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(gt2 + 1);
            *gt2           = *gt;
            gt2->tpage     = getTPage(0, 2, D_shelter_b3_dumping_hole_8018B578, D_shelter_b3_dumping_hole_8018B57A);
            addPrim(&gGpuCurrentOt[2], gt2);
            x = D_shelter_b3_dumping_hole_8018F4B8[(s16)code].w + x - 1;
        }
        i++;
    }
    return 0;
}

/// Top Y of the caption block the text stream `arg0` holds: every line after
/// the first `-2` adds its height (the tallest glyph's `h + 2`, or 2 when empty)
/// and the total is subtracted from `D_shelter_b3_dumping_hole_8018F4C4`. Gameplay's
/// `Gp_CapTextTopY` is the same walk against a fixed 0xD0, and the two pins are
/// what that twin carries; unpinned the body lands at 92%.
s16 func_shelter_b3_dumping_hole_801829B4(u16* arg0)
{
    s32                 lineH;
    s32                 total;
    s32                 i;
    s32                 seenBreak;
    u16                 code;
    s32                 shifted;
    register s32        next asm("v1");
    volatile GlyphUvwh* glyph;
    register s32        v0tmp asm("v0");

    lineH     = 0;
    total     = lineH;
    i         = lineH;
    code      = arg0[0];
    shifted   = code << 16;
    seenBreak = lineH;
    v0tmp     = -1;
    if (shifted >> 16 != v0tmp) {
        do {
            v0tmp = seenBreak;
            if (shifted >> 16 == -2) {
                if (v0tmp != 0) {
                    if (lineH == 0) {
                        lineH = 2;
                    }
                    total += lineH;
                } else {
                    seenBreak = 1;
                }
                lineH = 0;
            } else if (shifted >> 16 != -3) {
                if (shifted >> 16 >= 0) {
                    glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)D_shelter_b3_dumping_hole_8018F4B8);
                    if (lineH < glyph->h + 2) {
                        v0tmp = glyph->h;
                        TOUCH_REG(v0tmp);
                        lineH = v0tmp + 2;
                    }
                }
            }
            next    = i + 1;
            code    = arg0[(s16)next];
            i       = next;
            shifted = code << 16;
            v0tmp   = -1;
        } while (shifted >> 16 != v0tmp);
    }
    return (s16)(D_shelter_b3_dumping_hole_8018F4C4 - total);
}

void func_shelter_b3_dumping_hole_80182AA0(void)
{
    Prim82AA0* prim;
    s32        c1;
    s32        c2;

    if (D_shelter_b3_dumping_hole_8018F4D0 != 0) {
        D_shelter_b3_dumping_hole_8018F4D0 -= 1;
        return;
    }
    prim           = (Prim82AA0*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 6);
    prim->code     = 0x30;
    c1             = (D_shelter_b3_dumping_hole_8018B670 << 7) / 15;
    prim->r        = c1;
    prim->g        = c1;
    prim->b        = c1;
    c1             = (D_shelter_b3_dumping_hole_8018B670 * 192) / 15;
    c2             = c1;
    prim->field_C  = c2;
    prim->field_D  = c2;
    prim->field_E  = c2;
    prim->field_14 = c2;
    prim->field_15 = c2;
    prim->field_16 = c2;
    prim->field_8  = D_shelter_b3_dumping_hole_8018F4CC + 3;
    prim->field_A  = D_shelter_b3_dumping_hole_8018F4CE;
    prim->field_10 = D_shelter_b3_dumping_hole_8018F4CC;
    prim->field_18 = D_shelter_b3_dumping_hole_8018F4CC + 7;
    prim->field_12 = D_shelter_b3_dumping_hole_8018F4CE - 7;
    prim->field_1A = D_shelter_b3_dumping_hole_8018F4CE - 7;
    addPrim(&gGpuCurrentOt[2], prim);
    if (D_shelter_b3_dumping_hole_8018B674 == 0) {
        D_shelter_b3_dumping_hole_8018B670 += 1;
        if (D_shelter_b3_dumping_hole_8018B670 >= 0xF) {
            D_shelter_b3_dumping_hole_8018B674 = 1;
        }
    } else {
        D_shelter_b3_dumping_hole_8018B670 -= 1;
        if (D_shelter_b3_dumping_hole_8018B670 < 9) {
            D_shelter_b3_dumping_hole_8018B674 = 0;
        }
    }
}

/// Horizontal centring offset of the caption line the text stream `arg0`
/// starts with: the widest line's pixel width subtracted from the 0x140 screen
/// width, halved, minus 5. The walk is the one `func_actor_215100_8014C360`
/// makes, and gameplay's `Gp_CapCenterX` compiles to the same 0x110 bytes with
/// only the glyph table symbol differing — `-2` closes a line and keeps the
/// running maximum, `-3` and `0x8400`-masked codes indent it by 3 and 0x10, and
/// each glyph code (non-negative, `& 0x3FF` indexing `D_shelter_b3_dumping_hole_8018F4B8`)
/// advances it by that glyph's `w - 1`.
///
/// The three pins are what gameplay's twin carries; leaving them out keeps the
/// block structure and instruction count but moves 71 register choices.
s16 func_shelter_b3_dumping_hole_80182C24(u16* arg0)
{
    register s32        lineW asm("t0");
    s32                 maxW;
    s32                 i;
    register s32        width asm("v1");
    u16                 code;
    s32                 shifted;
    s32                 masked;
    volatile GlyphUvwh* glyph;
    register s32        v0tmp asm("v0");
    GlyphUvwh*          table;

    lineW   = 0;
    maxW    = lineW;
    i       = lineW;
    code    = arg0[0];
    shifted = code << 16;
    v0tmp   = -1;
    if (shifted >> 16 != v0tmp) {
        table = D_shelter_b3_dumping_hole_8018F4B8;
        do {
            shifted = shifted >> 16;
            v0tmp   = -2;
            if (shifted == v0tmp) {
                if ((lineW << 16) > (maxW << 16)) {
                    maxW = lineW;
                }
                lineW = 0;
                goto do_inc;
            }
            v0tmp = -3;
            if (shifted == v0tmp) {
                lineW += 3;
                goto do_inc;
            }
            masked = shifted & 0xFF00;
            TOUCH_REG(masked);
            v0tmp = 0x8400;
            if (masked == v0tmp) {
                lineW += 0x10;
                goto do_inc;
            }
            if (shifted >= 0) {
                v0tmp = i + 1;
                i     = v0tmp;
                TOUCH_REG(v0tmp);
                glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)table);
                code  = arg0[(s16)v0tmp];
                lineW = glyph->w + lineW - 1;
                goto after_load;
            }
            if (shifted < 0) {
            do_inc:
                v0tmp = i + 1;
                i     = v0tmp;
                TOUCH_REG(v0tmp);
                code = arg0[(s16)v0tmp];
            }
        after_load:
            shifted = code << 16;
            width   = shifted >> 16;
            v0tmp   = -1;
        } while (width != v0tmp);
    }
    width = (s16)maxW;
    return (0x140 - width) / 2 - 5;
}

/// Horizontal centring offset of line `arg1` of the caption text stream
/// `arg0`: that line's pixel width subtracted from 0x140, halved, minus 5.
/// Same walk as `func_actor_215100_8014C06C`, but keeps the width of the
/// selected line instead of the widest; gameplay's `Gp_CapCenterXLine`
/// compiles to the same bytes, pins included.
s16 func_shelter_b3_dumping_hole_80182D34(u16* arg0, s32 arg1)
{
    register s32        lineW asm("t1");
    s32                 selectedW;
    s32                 i;
    s32                 lineIndex;
    register s32        width asm("v1");
    u16                 code;
    s32                 shifted;
    s32                 masked;
    volatile GlyphUvwh* glyph;
    register s32        v0tmp asm("v0");
    GlyphUvwh*          table;

    lineW     = 0;
    selectedW = lineW;
    i         = lineW;
    lineIndex = lineW;
    code      = arg0[0];
    shifted   = code << 16;
    v0tmp     = -1;
    if (shifted >> 16 != v0tmp) {
        table = D_shelter_b3_dumping_hole_8018F4B8;
        do {
            shifted = shifted >> 16;
            v0tmp   = -2;
            if (shifted == v0tmp) {
                if ((s16)lineIndex == arg1) {
                    selectedW = lineW;
                }
                lineW = 0;
                v0tmp = i + 1;
                i     = v0tmp;
                lineIndex++;
                goto after_inc;
            }
            v0tmp = -3;
            if (shifted == v0tmp) {
                lineW += 3;
                goto do_inc;
            }
            masked = shifted & 0xFF00;
            TOUCH_REG(masked);
            v0tmp = 0x8400;
            if (masked == v0tmp) {
                lineW += 0x10;
                goto do_inc;
            }
            if (shifted >= 0) {
                v0tmp = i + 1;
                i     = v0tmp;
                TOUCH_REG(v0tmp);
                glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)table);
                code  = arg0[(s16)v0tmp];
                lineW = glyph->w + lineW - 1;
                goto after_load;
            }
            if (shifted < 0) {
            do_inc:
                v0tmp = i + 1;
                i     = v0tmp;
            after_inc:
                TOUCH_REG(v0tmp);
                code = arg0[(s16)v0tmp];
            }
        after_load:
            shifted = code << 16;
            width   = shifted >> 16;
            v0tmp   = -1;
        } while (width != v0tmp);
    }
    width = (s16)selectedW;
    return (0x140 - width) / 2 - 5;
}

s32 func_shelter_b3_dumping_hole_80182E50(s32 arg0)
{
    u16*        p = (u16*)arg0;
    short       acc;
    short       total;
    u16         i;
    u16         tok;
    s32         sh;
    s32         t;
    s32         ni;
    GlyphEntry* e;

    acc   = 0;
    total = acc;
    i     = total;
    tok   = *p;
    sh    = tok << 16;
    if ((sh >> 16) != -1) {
        do {
            t = sh >> 16;
            if (t == -2) {
                if (acc == 0) {
                    acc = 2;
                }
                total += acc;
                acc    = 0;
            } else if (t == -3) {
            } else if (t >= 0) {
                e = (GlyphEntry*)((tok & 0x3FF) * sizeof(GlyphEntry) + (s32)D_shelter_b3_dumping_hole_8018F4B8);
                if (acc < e->field_3 + 2) {
                    acc = e->field_3 + 2;
                }
            }
            ni  = (i = i + 1);
            tok = p[(s16)ni];
            sh  = tok << 16;
        } while ((sh >> 16) != -1);
    }
    return (s16)total;
}

/// Height of the caption line the text stream `arg0` starts with, walking it
/// the way gameplay's `func_800E6BB8` does — this overlay's caption system is
/// a copy of that one, and the two functions compile to the same 0xB8 bytes
/// with only the glyph table symbol differing.
///
/// The running maximum starts at 0 and each glyph code (non-negative, `& 0x3FF`
/// indexing `D_shelter_b3_dumping_hole_8018F4B8`) raises it to that glyph's `h + 2`. Either
/// terminator ends the scan: `-2` leaves the maximum as it stands, `-1` forces
/// 0xD, and any other negative code is stepped over like a glyph without
/// touching the maximum. A maximum still at 0 — the stream opened with `-2` —
/// comes back as 2.
s32 func_shelter_b3_dumping_hole_80182F18(u16* arg0)
{
    s32                 height;
    s32                 i;
    s32                 cont;
    u16                 code;
    s32                 shifted;
    volatile GlyphUvwh* glyph;
    GlyphUvwh*          table;
    s32                 next;
    s32                 htmp;
    s32                 v0tmp;

    height  = 0;
    i       = height;
    cont    = 1;
    code    = arg0[0];
    table   = D_shelter_b3_dumping_hole_8018F4B8;
    shifted = code << 16;
    for (;;) {
        shifted = shifted >> 16;
        if (shifted == -2) {
            cont = 0;
        } else if (shifted == -1) {
            cont   = 0;
            height = 0xD;
        } else {
            if (shifted >= 0) {
                glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)table);
                if (height < glyph->h + 2) {
                    htmp   = glyph->h;
                    height = htmp + 2;
                    goto do_inc;
                }
                next = i + 1;
            } else {
            do_inc:
                next = i + 1;
            }
            i = next;
            TOUCH_REG(next);
            code = arg0[(s16)next];
        }
        v0tmp = cont;
        TOUCH_REG(v0tmp);
        if (v0tmp == 0) {
            break;
        }
        shifted = code << 16;
    }
    if (height == 0) {
        height = 2;
    }
    return height;
}

s32 func_shelter_b3_dumping_hole_80182FD0(s32 arg0)
{
    s32      sentinel = -1;
    s32      base     = (s32)D_shelter_b3_dumping_hole_8018F4BC;
    s32      target   = D_shelter_b3_dumping_hole_8018F4CA;
    GpEvt12* e        = (GpEvt12*)(arg0 * sizeof(GpEvt12) + base);

loop:
    if (e->field_8 != sentinel) {
        if (e->field_5 != target) {
            e++;
            arg0++;
            goto loop;
        }
    }
    return arg0;
}

void func_shelter_b3_dumping_hole_80183024(Task* arg0)
{
    if ((arg0->spawnArg1 -= 1) <= 0) {
        taskKill(arg0);
    }
    func_shelter_b3_dumping_hole_80181C8C();
}

void func_shelter_b3_dumping_hole_80183060(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            arg0->state = 1;
            break;
        case 1:
            arg0->spawnArg1 -= 1;
            if (arg0->spawnArg1 <= 0 || Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0) {
                taskKill(arg0);
                Stage_SetEndingFlag();
            }
            break;
    }
    func_shelter_b3_dumping_hole_80181C8C();
}

/// Selects entry `arg0` through `func_shelter_b3_dumping_hole_80181E70` with a
/// fixed third argument of 0xD0, then spawns the task described by
/// `D_shelter_b3_dumping_hole_8018B588`, passing `arg2` to the spawn.
void func_shelter_b3_dumping_hole_801830F0(s16 arg0, s16 arg1, s16 arg2)
{
    func_shelter_b3_dumping_hole_80181E70(arg0, arg1, 0xD0);
    Task_SpawnFromTable(&D_shelter_b3_dumping_hole_8018B588, 0, arg2, 0);
}

void func_shelter_b3_dumping_hole_80183144(s16 arg0, s16 arg1, s16 arg2)
{
    func_shelter_b3_dumping_hole_80181E70(arg0, arg1, 0xD0);
    Display_InitModeObj(&D_shelter_b3_dumping_hole_8018B594, arg2, 0, 0);
}

void func_shelter_b3_dumping_hole_80183198(s16 arg0, s16 arg1, s16 arg2)
{
    s32 count;
    s32 i;

    count                              = 0;
    D_shelter_b3_dumping_hole_8018B578 = arg0;
    D_shelter_b3_dumping_hole_8018B57A = arg1;
    for (i = 0; i < 0x32; i++) {
        if (D_8006C338[i].field_0 == 3) {
            if (count == arg2) {
                func_shelter_b3_dumping_hole_80181D68(D_8006C338[i].field_4);
                break;
            }
            count++;
        }
    }
}

/// Hides or shows sprite commands 1 and 2 of the area's view 13 through their
/// `GpSprtCmd::field_4`: 0 hides both, 1 shows command 2 and 2 shows command 1.
void func_shelter_b3_dumping_hole_80183218(u8 arg0)
{
    GpAreaKey* g4 = &gGameSession->at4.loc;
    GpSprtCmd* vs = Gp_SprtTables[g4->stage - 1]->field_0[g4->area - 1][13].field_4;

    if (arg0 == 0) {
        vs[1].field_4 = 1;
        vs[2].field_4 = 1;
    } else if (arg0 == 1) {
        vs[2].field_4 = 0;
    } else if (arg0 == 2) {
        vs[1].field_4 = 0;
    }
}

/// Spawns the two enemies of one slot from the `D_80151E60` table, numbering
/// them from the spawn counter, and marks the slot live. Actor 342400 carries
/// the same body.
void func_shelter_b3_dumping_hole_80183298(Task* arg0)
{
    OverlayEncounterPairWork* work;
    GpEnemy*                  enemy;
    Task*                     task;
    TmdObject*                obj;

    work = memCalloc(0xC, 0);
    if (work == NULL) {
        goto kill;
    }
    arg0->work   = (TaskIdMap*)work;
    work->enemy0 = Gp_SpawnEnemyFromTable(&D_80151E60, 1, 1, 0);
    work->enemy1 = Gp_SpawnEnemyFromTable(&D_80151E60, 1, 1, 0);
    if (work->enemy0 == NULL && work->enemy1 == NULL) {
    kill:
        taskKill(arg0);
        return;
    }
    if (work->enemy0 != NULL) {
        enemy           = work->enemy0;
        enemy->placeKey = D_shelter_b3_dumping_hole_8018F4D4 << 12;
        D_shelter_b3_dumping_hole_8018F4D4++;
        task       = enemy->task;
        obj        = task->extra;
        obj->tpage = 3;
        obj->clut  = 5;
        enemy->hp  = 1;
    }
    if (work->enemy1 != NULL) {
        enemy           = work->enemy1;
        enemy->placeKey = D_shelter_b3_dumping_hole_8018F4D4 << 12;
        D_shelter_b3_dumping_hole_8018F4D4++;
        task       = enemy->task;
        obj        = task->extra;
        obj->tpage = 3;
        obj->clut  = 5;
        enemy->hp  = 1;
    }
    D_shelter_b3_dumping_hole_8018B7BC[(s16)(arg0->spawnArg1 >> 16)].status = 1;
    arg0->state++;
}

void func_shelter_b3_dumping_hole_801833EC(Task* arg0)
{
    OverlayEncounterCtrlWork* ent = (OverlayEncounterCtrlWork*)arg0->work;
    s16                       count;
    s16                       i;
    s16                       idx;
    s16                       type;
    s16                       arg;

    count = 0;
    for (i = 0; i < 16; i++) {
        if (D_shelter_b3_dumping_hole_8018B7BC[i].status == 1) {
            count++;
        }
    }
    if (count < 3) {
        idx = ent->nextSlot;
        if (idx < 16 && gGameSession->sceneClock >= 0x3D) {
            type = D_shelter_b3_dumping_hole_8018B7BC[idx].kind;
            arg  = D_shelter_b3_dumping_hole_8018B7BC[idx].command;
            switch (type) {
                case 0:
                    Task_SpawnFromTable(&D_shelter_b3_dumping_hole_8018B83C, 1, (idx << 16) + arg, 0);
                    break;
                case 1:
                    Task_SpawnFromTable(&D_shelter_b3_dumping_hole_8018B83C, 2, (idx << 16) + arg, 0);
                    break;
                case 2:
                    Task_SpawnFromTable(&D_shelter_b3_dumping_hole_8018B83C, 3, (idx << 16) + arg, 0);
                    break;
            }
            ent->nextSlot++;
        }
    }
}

void func_shelter_b3_dumping_hole_80183530(Task* arg0, s32 arg1, GpCmdArg* arg2)
{
    OverlayEncounterCtrlWork* ent = (OverlayEncounterCtrlWork*)arg0->work;
    if (arg2->command == 4) {
        ent->stop = arg2->command;
    }
}

/// States of the task that works through the room's 16 enemy slots: set-up,
/// the first three slot spawns, a 15-frame wait before `Gp_ArmStateF0`, and a
/// loop that starts the next slot while fewer than three are live and ends
/// once all 16 have been cleared.
const TaskFuncTable4 D_shelter_b3_dumping_hole_8017D654 = { {
    func_shelter_b3_dumping_hole_801836E0,
    func_shelter_b3_dumping_hole_8018378C,
    func_shelter_b3_dumping_hole_80183824,
    func_shelter_b3_dumping_hole_801838A0,
} };

/// States of a slot task holding one enemy from `D_80142604`: spawn it, after
/// a delay switch its palette and send it message 0x7DB, then wait for its hit
/// points to run out.
const TaskFuncTable3 D_shelter_b3_dumping_hole_8017D664 = { {
    func_shelter_b3_dumping_hole_80183950,
    func_shelter_b3_dumping_hole_80183A00,
    func_shelter_b3_dumping_hole_80183A98,
} };

/// States of a slot task holding one enemy from `D_801575F0`: spawn it, after
/// a delay switch its palette and send it message 0x7DB, then wait for its hit
/// points to run out.
const TaskFuncTable3 D_shelter_b3_dumping_hole_8017D670 = { {
    func_shelter_b3_dumping_hole_80183AEC,
    func_shelter_b3_dumping_hole_80183B9C,
    func_shelter_b3_dumping_hole_80183C38,
} };

/// States of a slot task holding a pair of enemies from `D_80151E60`: spawn
/// them, one idle frame, send the first message 0x7DB, send the second the
/// same after a delay, then wait until both are gone.
const TaskFuncTable5 D_shelter_b3_dumping_hole_8017D67C = { {
    func_shelter_b3_dumping_hole_80183298,
    func_shelter_b3_dumping_hole_80183C8C,
    func_shelter_b3_dumping_hole_80183CA0,
    func_shelter_b3_dumping_hole_80183D34,
    func_shelter_b3_dumping_hole_80183E08,
} };

void func_shelter_b3_dumping_hole_80183550(Task* task)
{
    TaskFuncTable4 sp;

    sp = D_shelter_b3_dumping_hole_8017D654;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}

void func_shelter_b3_dumping_hole_801835C8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b3_dumping_hole_8017D664;
    sp.funcs[task->state](task);
}

void func_shelter_b3_dumping_hole_80183620(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b3_dumping_hole_8017D670;
    sp.funcs[task->state](task);
}

void func_shelter_b3_dumping_hole_80183678(Task* task)
{
    TaskFuncTable5 sp;

    sp = D_shelter_b3_dumping_hole_8017D67C;
    sp.funcs[task->state](task);
}

void func_shelter_b3_dumping_hole_801836E0(Task* arg0)
{
    OverlayEncounterCtrlWork* work;
    s32                       i;

    if ((u8)gGameSession->spawnPhase[0] == 2) {
        taskKill(arg0);
        return;
    }
    work = memCalloc(6, 0);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    for (i = 15; i >= 0; i--) {
        D_shelter_b3_dumping_hole_8018B7BC[i].status = 0;
    }
    D_shelter_b3_dumping_hole_8018F4D4 = 0;
    arg0->work                         = work;
    arg0->msgTable                     = D_shelter_b3_dumping_hole_8018B7AC;
    arg0->state                       += 1;
}

void func_shelter_b3_dumping_hole_8018378C(Task* arg0)
{
    OverlayEncounterCtrlWork* ent = (OverlayEncounterCtrlWork*)arg0->work;
    s32                       i;

    for (i = 0; i < 3; i++) {
        s16 idx = ent->nextSlot;
        func_shelter_b3_dumping_hole_80183E6C(idx, D_shelter_b3_dumping_hole_8018B7BC[idx].kind,
                                              D_shelter_b3_dumping_hole_8018B7BC[idx].command);
        ent->nextSlot += 1;
    }
    arg0->state += 1;
}

void func_shelter_b3_dumping_hole_80183824(Task* arg0)
{
    OverlayEncounterCtrlWork* ent = (OverlayEncounterCtrlWork*)arg0->work;
    if ((s16)(ent->frames += 1) == 0xF) {
        ((void (*)(s32))Gp_IncStateF0Ref)(0);
        gGameSession->spawnPhase[0] = 1;
        Gp_ArmStateF0(1);
        arg0->state += 1;
    }
}

void func_shelter_b3_dumping_hole_801838A0(Task* arg0)
{
    s16 count;
    s32 i;

    count = 0;
    if (((OverlayEncounterCtrlWork*)arg0->work)->stop != 4) {
        func_shelter_b3_dumping_hole_801833EC(arg0);
        for (i = 0; i < 0x10; i++) {
            if (D_shelter_b3_dumping_hole_8018B7BC[i].status == 2) {
                count++;
            }
        }
        if (count == 0x10) {
            ((void (*)(Task*, s32))Gp_ReleaseStateF0Clear)(arg0, 0);
            gGameSession->spawnPhase[0] = 2;
            taskKill(arg0);
        }
    }
}

void func_shelter_b3_dumping_hole_80183950(Task* arg0)
{
    OverlayEncounterSingleWork* work = memCalloc(8, 0);
    if (work != NULL) {
        GpEnemy* enemy;
        arg0->work = work;
        enemy      = Gp_SpawnEnemyFromTable(&D_80142604, 1, 0, NULL);
        if (enemy != NULL) {
            u16 idx;
            D_shelter_b3_dumping_hole_8018B7BC[(s16)(arg0->spawnArg1 >> 16)].status = 1;
            idx                                                                     = D_shelter_b3_dumping_hole_8018F4D4;
            work->enemy                                                             = enemy;
            enemy->placeKey                                                         = idx << 12;
            D_shelter_b3_dumping_hole_8018F4D4                                      = idx + 1;
            arg0->state                                                            += 1;
            return;
        }
    }
    taskKill(arg0);
}

void func_shelter_b3_dumping_hole_80183A00(Task* arg0)
{
    GpCmdArg                    desc;
    OverlayEncounterSingleWork* ent = (OverlayEncounterSingleWork*)arg0->work;
    GpEnemy*                    t0  = ent->enemy;
    Task*                       t00 = t0->task;

    if ((s16)(ent->frames += 1) >= 0x2E) {
        TmdObject* p        = t00->extra;
        p->clut             = 2;
        p->tpage            = 0;
        t0->workType        = 0x900;
        desc.from.loc.stage = 0;
        desc.from.loc.area  = 0x2C;
        desc.command        = arg0->spawnArg1;
        Gp_DispatchMsg(t00, 0x7DB, (s32)&desc, 0);
        arg0->state += 1;
    }
}

void func_shelter_b3_dumping_hole_80183A98(Task* arg0)
{
    if (((OverlayEncounterSingleWork*)arg0->work)->enemy->hp <= 0) {
        D_shelter_b3_dumping_hole_8018B7BC[(s16)(arg0->spawnArg1 >> 16)].status = 2;
        taskKill(arg0);
    }
}

void func_shelter_b3_dumping_hole_80183AEC(Task* arg0)
{
    OverlayEncounterSingleWork* work = memCalloc(8, 0);
    if (work != NULL) {
        GpEnemy* enemy;
        arg0->work = work;
        enemy      = Gp_SpawnEnemyFromTable(&D_801575F0, 2, 0, NULL);
        if (enemy != NULL) {
            u16 idx;
            D_shelter_b3_dumping_hole_8018B7BC[(s16)(arg0->spawnArg1 >> 16)].status = 1;
            idx                                                                     = D_shelter_b3_dumping_hole_8018F4D4;
            work->enemy                                                             = enemy;
            enemy->placeKey                                                         = idx << 12;
            D_shelter_b3_dumping_hole_8018F4D4                                      = idx + 1;
            arg0->state                                                            += 1;
            return;
        }
    }
    taskKill(arg0);
}

void func_shelter_b3_dumping_hole_80183B9C(Task* arg0)
{
    GpCmdArg                    desc;
    OverlayEncounterSingleWork* ent = (OverlayEncounterSingleWork*)arg0->work;
    GpEnemy*                    t0  = ent->enemy;
    Task*                       t00 = t0->task;

    if ((s16)(ent->frames += 1) >= 0x3D) {
        TmdObject* p        = t00->extra;
        p->tpage            = 2;
        p->clut             = 4;
        t0->workType        = 0x900;
        desc.from.loc.stage = 0;
        desc.from.loc.area  = 0x2A;
        desc.command        = arg0->spawnArg1;
        Gp_DispatchMsg(t00, 0x7DB, (s32)&desc, 0);
        arg0->state += 1;
    }
}

void func_shelter_b3_dumping_hole_80183C38(Task* arg0)
{
    if (((OverlayEncounterSingleWork*)arg0->work)->enemy->hp <= 0) {
        D_shelter_b3_dumping_hole_8018B7BC[(s16)(arg0->spawnArg1 >> 16)].status = 2;
        taskKill(arg0);
    }
}

/// Advances the task to its next state.
void func_shelter_b3_dumping_hole_80183C8C(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

void func_shelter_b3_dumping_hole_80183CA0(Task* arg0)
{
    GpCmdArg                  desc;
    OverlayEncounterPairWork* ent = (OverlayEncounterPairWork*)arg0->work;
    GpEnemy*                  t0  = ent->enemy0;

    if (t0 != NULL) {
        Task*      t00      = t0->task;
        TmdObject* p        = t00->extra;
        p->tpage            = 3;
        p->clut             = 5;
        t0->workType        = 0x900;
        desc.from.loc.stage = 0;
        desc.from.loc.area  = 0x2E;
        desc.command        = arg0->spawnArg1;
        Gp_DispatchMsg(t00, 0x7DB, (s32)&desc, 0);
    }
    ent->frames  = 0;
    arg0->state += 1;
}

void func_shelter_b3_dumping_hole_80183D34(Task* arg0)
{
    OverlayEncounterPairWork* ent = (OverlayEncounterPairWork*)arg0->work;
    GpEnemy*                  t   = ent->enemy1;

    func_shelter_b3_dumping_hole_80183F04(arg0);
    if (ent->enemy1 != NULL) {
        if ((s16)(ent->frames += 1) < 0x3D) {
            return;
        }
        {
            Task*      t00 = ent->enemy1->task;
            TmdObject* p   = t00->extra;
            GpCmdArg   desc;
            p->tpage            = 3;
            p->clut             = 5;
            t->workType         = 0x900;
            desc.from.loc.stage = 0;
            desc.from.loc.area  = 0x2E;
            desc.command        = arg0->spawnArg1;
            Gp_DispatchMsg(t00, 0x7DB, (s32)&desc, 0);
        }
    }
    ent->frames  = 0;
    arg0->state += 1;
}

void func_shelter_b3_dumping_hole_80183E08(Task* arg0)
{
    OverlayEncounterPairWork* ent = (OverlayEncounterPairWork*)arg0->work;
    func_shelter_b3_dumping_hole_80183F04(arg0);
    if (ent->goneMask == 3) {
        D_shelter_b3_dumping_hole_8018B7BC[(s16)(arg0->spawnArg1 >> 16)].status = 2;
        taskKill(arg0);
    }
}

void func_shelter_b3_dumping_hole_80183E6C(s16 arg0, s16 arg1, s16 arg2)
{
    switch (arg1) {
        case 0:
            Task_SpawnFromTable(&D_shelter_b3_dumping_hole_8018B83C, 1, (arg0 << 16) + arg2, 0);
            break;
        case 1:
            Task_SpawnFromTable(&D_shelter_b3_dumping_hole_8018B83C, 2, (arg0 << 16) + arg2, 0);
            break;
        case 2:
            Task_SpawnFromTable(&D_shelter_b3_dumping_hole_8018B83C, 3, (arg0 << 16) + arg2, 0);
            break;
    }
}

void func_shelter_b3_dumping_hole_80183F04(Task* arg0)
{
    OverlayEncounterPairWork* p = (OverlayEncounterPairWork*)arg0->work;

    if (p->enemy0 != NULL) {
        if (p->enemy0->hp <= 0) {
            p->enemy0 = NULL;
        }
    } else {
        p->goneMask |= 1;
    }
    if (p->enemy1 != NULL) {
        if (p->enemy1->hp <= 0) {
            p->enemy1 = NULL;
        }
    } else {
        p->goneMask |= 2;
    }
}

void func_shelter_b3_dumping_hole_80183F84(Task* task)
{
    u8 view;

    if (task->state == 0) {
        D_shelter_b3_dumping_hole_8018F4D8 = 0;
        task->state                        = 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B96C[0], 0x300, 0x100);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B96C[1], 0x300, 0x200);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B96C[2], 0x300, 0x300);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B96C[3], 0x300, 0x400);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B96C[4], 0x300, 0x400);
            break;
        case 3:
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B96C[0], 0x300, 0x200);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B96C[1], 0x300, 0x300);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B96C[2], 0x300, 0x400);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B96C[3], 0x300, 0x400);
            break;
        case 4:
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B94C[0], 0x280, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B94C[1], 0x280, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B94C[9], 0x300, 0x400);
            break;
        case 7:
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B974[0], 0x300, 0x400);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B974[1], 0x300, 0x400);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B974[2], 0x300, 0x400);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B974[3], 0x300, 0x400);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B974[4], 0x300, 0x400);
            break;
        case 14:
            if (D_shelter_b3_dumping_hole_8018F4D8 != 0) {
                func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B95C[0], 0x280, 0x44);
                func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B95C[1], 0x280, 0x40);
            }
            break;
        case 15:
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[0], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[2], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[0x20], 0x300, 0x100);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[0x21], 0x300, 0x200);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[0x22], 0x300, 0x300);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[0x23], 0x300, 0x400);
            break;
        case 17:
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B98C[0], 0x300, 0x400);
            break;
        case 18:
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B8BC[0], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B8BC[0x12], 0x280, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B8BC[0x13], 0x280, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B8BC[0x19], 0x300, 0x400);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B8BC[0x1A], 0x300, 0x300);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B8BC[0x1B], 0x300, 0x200);
            break;
        case 19:
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B96C[0], 0x300, 0x400);
            break;
        case 21:
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B92C[0], 0x400, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B92C[1], 0x400, 0x444);
            break;
        case 22:
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B95C[0], 0x280, 0x44);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B95C[1], 0x280, 0x40);
            break;
        case 23:
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[0], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[0x20], 0x300, 0x400);
            break;
        case 26:
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B93C[0], 0x300, 0x400);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B93C[1], 0x300, 0x400);
            break;
        case 29:
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B94C[0], 0x280, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B94C[1], 0x280, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B94C[2], 0x280, 0x44);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B94C[3], 0x280, 0x40);
            break;
        case 30:
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B94C[0], 0x280, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B94C[1], 0x280, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B94C[2], 0x280, 0x44);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B94C[3], 0x280, 0x40);
            break;
        case 31:
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[0], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[2], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[4], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[12], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[14], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[24], 0x400, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[26], 0x400, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[32], 0x300, 0x200);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[33], 0x300, 0x300);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[34], 0x300, 0x400);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[38], 0x300, 0x200);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[39], 0x300, 0x300);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[40], 0x300, 0x400);
            break;
        case 34:
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B96C[0], 0x300, 0x200);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B96C[1], 0x300, 0x200);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B96C[2], 0x300, 0x300);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B96C[3], 0x300, 0x300);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B96C[4], 0x300, 0x400);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B96C[5], 0x300, 0x400);
            break;
        case 35:
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B96C[0], 0x300, 0x200);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B96C[1], 0x300, 0x400);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B96C[6], 0x300, 0x200);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B96C[7], 0x300, 0x400);
            break;
        case 13:
        case 37:
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[0], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[2], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[4], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[6], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[12], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[14], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184638(&D_shelter_b3_dumping_hole_8018B86C[16], 0x200, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[24], 0x400, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[25], 0x400, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[26], 0x400, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[27], 0x400, 0x444);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[32], 0x300, 0x100);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[33], 0x300, 0x200);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[34], 0x300, 0x300);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[35], 0x300, 0x400);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[38], 0x300, 0x100);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[39], 0x300, 0x200);
            func_shelter_b3_dumping_hole_80184E7C(&D_shelter_b3_dumping_hole_8018B86C[40], 0x300, 0x300);
            break;
    }
}

/// Draws a glowing capsule between the world points `arg0` and `arg0 + 1`,
/// projected through `Gfx_ViewWorldMtx`. Each end is a disc of radius
/// `(s16)arg1 * 64` over its depth; for each 0x400 step across half a turn
/// from the screen-space angle between the ends, one gouraud `POLY_G4` wedge
/// is queued at each end and one band joins them. The lit vertices, at the
/// centres, take the colour packed in `arg2` (one nibble per channel),
/// flickering with the animation frame. Nothing is drawn when either
/// projection flags an error.
void func_shelter_b3_dumping_hole_80184638(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    RoomDraw08Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    SVECTOR*           p1;
    s32                ang;
    s32                t;
    s32                t3;
    s32                t2;
    s32                limit;
    s32                angStart;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    s32                scaled;
    s32                sum;
    u8                 r;
    u8                 g;
    u8                 b;

    p1      = arg0 + 1;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u8* tmp asm("v0");
        tmp      = head - 0x1C;
        block    = (RoomDraw08Scratch*)tmp;
        *scratch = tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw08Scratch*)(head - 0x1C))->sx0);
    gte_stflg(&((RoomDraw08Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&((RoomDraw08Scratch*)(head - 0x1C))->sx1);
        gte_stflg(&((RoomDraw08Scratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((RoomDraw08Scratch*)(head - 0x1C))->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / ((RoomDraw08Scratch*)(head - 0x1C))->otz0;
            block->r1 = scaled / block->otz1;
            ang       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            SCHED_BARRIER();
            ang    = (s16)ang;
            blend  = (*(u8*)&ds->animFrame & 1) * 8;
            packed = arg2 << 16;
            tr     = (packed >> 20) & 0xF0;
            tg     = (packed >> 16) & 0xF0;
            r      = blend | tr;
            g      = blend | tg;
            b      = blend | ((arg2 & 0xF) << 4);
            if (ang < ang + 0x800) {
                angStart = ang;
                limit    = ang + 0x800;
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    prim = (POLY_G4*)gGpuPrimCursor;
                    USE_REG(prim);
                    t   = ang - angStart;
                    t <<= 1;
                    TOUCH_REG(t);
                    sum            = angStart + t;
                    t              = sum;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, r, g, b);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim((u_long*)(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                                      (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    SCHED_BARRIER();
                    t3   = ang + 0x800;
                    prim = (POLY_G4*)gGpuPrimCursor;
                    SOFT_BARRIER();
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, r, g, b);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    addPrim((u_long*)(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

/// Draws a glowing disc at the world point `arg0`, projected through
/// `Gfx_ViewWorldMtx`: four gouraud `POLY_G4` wedges of radius
/// `(s16)arg1 * 64` over the depth, dark at the rim. The centre takes the
/// colour packed in `arg2` (one nibble per channel), flickering with the
/// animation frame. Nothing is drawn when the projection flags an error.
void func_shelter_b3_dumping_hole_80184E7C(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    register u8*       tmp asm("v0");
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    DisplayState*      ds;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    u8                 r;
    u8                 g;
    u8                 b;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    tmp      = head - 0x10;
    block    = (RoomDraw13Scratch*)tmp;
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1 = ((s16)arg1 * 64) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        ang  = 0;
        tmp  = (u8*)&gDisplayState;
        SOFT_TOUCH_REG(tmp);
        ds            = (DisplayState*)tmp;
        blend         = (*(u8*)&ds->animFrame & 1) * 8;
        packed        = arg2 << 16;
        tr            = (packed >> 20) & 0xF0;
        tg            = (packed >> 16) & 0xF0;
        r             = blend | tr;
        g             = blend | tg;
        b             = blend | ((arg2 & 0xF) << 4);
        block->radius = arg1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

/// Per-frame update of a spark or debris effect task. State 0 seeds the work
/// from `spawnArg1`: the spin angle from its low 12 bits, the frame step from
/// bits 12-14, the palette bank from bits 28-30, and, unless the work already
/// carries a velocity, a random one of the kind bits 24-27 select, scaled to
/// the strength in bits 16-23 through the GTE. A negative `spawnArg1` picks
/// the second sprite set (state 2). Later ticks draw the frame, drift the
/// coordinate by the velocity under a small pull, and advance the frame every
/// `period` ticks, releasing the task after the set's last frame. While an
/// event is running the task only draws, and is released once the event state
/// reaches 4.
void func_shelter_b3_dumping_hole_8018521C(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;
    s32            step;
    s32            level;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (task->spawnArg1 < 0) {
            func_shelter_b3_dumping_hole_80185DCC(coord, work->index | work->pos.vx, work->scale, work->angle);
        } else {
            func_shelter_b3_dumping_hole_8018596C(coord, work->index | work->pos.vx, work->scale, work->angle);
        }
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }
    work->age++;
    switch (task->state) {
        case 0:
            work->scale = task->spawnArg1 & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1 & 0xF000) {
                step = (task->spawnArg1 >> 12) & 7;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
            task->state  = 1;
            task->state  = task->spawnArg1 < 0 ? 2 : 1;
            work->pos.vx = (task->spawnArg1 >> 16) & 0x7000;
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1 & 0xFF0000) {
                    level = (task->spawnArg1 >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                switch ((task->spawnArg1 >> 24) & 0xF) {
                    case 0:
                        work->step = 0;
                        break;
                    case 1:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                    case 6:
                        work->move.vy = 0;
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 7:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = ((u32)Gp_LcgState >> 16) & 0xFF;
                        gte_SetRotMatrix(&work->parent->coord);
                        gte_ldv0(&work->move);
                        gte_rtv0();
                        gte_stsv(&work->move);
                        break;
                }
                vec = &work->move;
                VectorNormalSS(vec, vec);
                gte_lddp(work->step);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->step = 0x40;
            }
            break;
        case 1:
            func_shelter_b3_dumping_hole_8018596C(coord, work->index | work->pos.vx, work->scale, work->angle);
            if (work->step != 0) {
                coord->coord.t[0] += work->move.vx;
                coord->coord.t[1] += work->move.vy;
                coord->coord.t[2] += work->move.vz;
                coord->flg         = 0;
                if (((task->spawnArg1 >> 24) & 0xF) == 7) {
                    work->move.vy += work->age / 10;
                } else {
                    work->move.vy -= 2;
                }
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= 12) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
        case 2:
            func_shelter_b3_dumping_hole_80185DCC(coord, work->index | work->pos.vx, work->scale, work->angle);
            if (work->step != 0) {
                coord->coord.t[0] += work->move.vx;
                coord->coord.t[1] += work->move.vy;
                coord->coord.t[2] += work->move.vz;
                coord->flg         = 0;
                if (((task->spawnArg1 >> 24) & 0xF) == 7) {
                    work->move.vy += work->age / 10;
                } else {
                    work->move.vy -= 1;
                }
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= 10) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
    }
}

void func_shelter_b3_dumping_hole_8018596C(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    u16              col;
    u16              row;
    s32              u0;
    s32              v0;
    s32              ang;
    s32              ang2;
    u16              bank;
    u32              idx;

    head                                      = *(u8**)G_SCRATCH_HEAD;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = arg0->workm.t[0];
    *(void**)G_SCRATCH_HEAD                   = head - 0x1C;
    block                                     = *(GpFxQuadScratch**)G_SCRATCH_HEAD;
    block->vec.vy                             = arg0->workm.t[1];
    block->vec.vz                             = arg0->workm.t[2];
    idx                                       = arg1;
    idx                                      &= 0xFFF;
    bank                                      = arg1 >> 12;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(block);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        if (bank >= 2) {
            prim->clut = 0x428F;
        } else {
            prim->clut = ((bank + 0x10E) << 6) | (idx & 0x3F);
        }
        col = (u16)idx % 5;
        row = (u16)idx / 5;
        ang = arg3;
        u0  = col * 0x30;
        v0  = row * 0x30;
        setUV4(prim, u0, v0 + 0x68, u0 + 0x2F, v0 + 0x68, u0, v0 - 0x69, u0 + 0x2F, v0 - 0x69);
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = ang + 0x400;
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(u8**)G_SCRATCH_HEAD += 0x1C;
}

/// Draws a spinning textured sprite at the world position of `arg0`,
/// projected through `GsWSMATRIX`: one semi-transparent `POLY_FT4` whose
/// corners lie `(s16)arg2 * 47` over the depth from the centre, at the angle
/// `arg3` and a quarter turn past it. The low 12 bits of `arg1` pick the
/// frame, a 48x48 cell in a five-wide grid of the texture page; the top bits
/// pick one of two palettes. Nothing is drawn when the projection flags an
/// error.
void func_shelter_b3_dumping_hole_80185DCC(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    u16              col;
    u16              row;
    s32              u0;
    s32              v0;
    s32              ang;
    s32              ang2;
    u16              bank;
    u16              vz;

    bank                                      = arg1 >> 12;
    arg1                                     &= 0xFFF;
    head                                      = *(u8**)G_SCRATCH_HEAD;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = arg0->workm.t[0];
    *(void**)G_SCRATCH_HEAD                   = head - 0x1C;
    block                                     = *(GpFxQuadScratch**)G_SCRATCH_HEAD;
    block->vec.vy                             = arg0->workm.t[1];
    block->vec.vz                             = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(block);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2C;
        prim->clut  = bank ? 0x428F : 0x43D0;
        col         = arg1 % 5;
        row         = arg1 / 5;
        ang         = arg3;
        u0          = col * 0x30;
        v0          = row * 0x30;
        setUV4(prim, u0, v0 - 0x80, u0 + 0x2F, v0 - 0x80, u0, v0 - 0x51, u0 + 0x2F, v0 - 0x51);
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        ang2      = ang + 0x400;
        block->dx = (((arg2 * 47) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 47) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *(u8**)G_SCRATCH_HEAD += 0x1C;
}

/// Per-frame update of an effect task drawn with
/// `func_shelter_b3_dumping_hole_801866CC` (state 1) or
/// `func_shelter_b3_dumping_hole_80186AB8` (state 2). State 0 seeds the work from
/// `spawnArg1` and, when `move` is zero, picks a random velocity scaled
/// through the GTE. Later ticks draw, drift the coordinate by that velocity
/// with `vy` growing by 6, and advance the frame every `period` ticks,
/// releasing the task after frame 7. While an event is running the task only
/// draws, and is released once the event state reaches 4.
void func_shelter_b3_dumping_hole_80186218(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    SVECTOR*       vec;
    s32            kind;
    s32            step;
    s32            state;
    s32            level;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState < 4) {
            if (task->state < 2) {
                func_shelter_b3_dumping_hole_801866CC(coord, work->index, work->scale, work->angle);
            } else {
                func_shelter_b3_dumping_hole_80186AB8(coord, (u16)work->index, work->scale);
            }
            return;
        }
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    work->age++;
    switch (task->state) {
        case 0:
            work->scale = ((GpEffSpawnArg*)&task->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1 & 0xF000) {
                step = (task->spawnArg1 >> 12) & 0xF;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
            state        = 1;
            if (task->spawnArg1 & 0xF0000000) {
                state = 2;
            }
            task->state = state;
            if (((u16)work->move.vx | (u16)work->move.vy | (u16)work->move.vz) == 0) {
                if (task->spawnArg1 & 0xFF0000) {
                    level = (task->spawnArg1 >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                kind       = ((GpEffSpawnArgHi*)&task->spawnArg1)->field_3;
                switch (kind & 0xF) {
                    case 0:
                        work->step = 0;
                        break;
                    case 1:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                }
                vec = &work->move;
                VectorNormalSS(vec, vec);
                gte_lddp(work->step);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->step = 0x40;
            }
            return;
        case 1:
            func_shelter_b3_dumping_hole_801866CC(coord, work->index, work->scale, work->angle);
            break;
        case 2:
            func_shelter_b3_dumping_hole_80186AB8(coord, (u16)work->index, work->scale);
            break;
        default:
            return;
    }
    if (work->step != 0) {
        coord->coord.t[0] += work->move.vx;
        coord->coord.t[1] += work->move.vy;
        coord->coord.t[2] += work->move.vz;
        coord->flg         = 0;
        work->move.vy     += 6;
    }
    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= 8) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws a spinning textured sprite at the world position of `arg0`,
/// projected through `GsWSMATRIX`: one semi-transparent `POLY_FT4` whose
/// corners lie `(s16)arg2 * 31` over the depth from the centre, at the angle
/// `arg3` and a quarter turn past it. `arg1` picks the frame, a 32x32 cell
/// in a row of the texture page. Nothing is drawn when the projection flags
/// an error.
void func_shelter_b3_dumping_hole_801866CC(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              u1;
    s32              v;
    s32              ang;
    s32              ang2;
    u16              vz;

    scratch                                   = (void**)G_SCRATCH_HEAD;
    head                                      = *scratch;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = *(u16*)&arg0->workm.t[1];
    vz                                        = *(u16*)&arg0->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        ang            = arg3;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2C;
        prim->clut  = 0x43D3;
        u0          = arg1 << 5;
        v           = 0xE0;
        u1          = u0 + 0x1F;
        setUV4(prim, u0, v, u1, v, u0, 0xFF, u1, 0xFF);
        block->dx = (((arg2 * 31) / block->otz) * rsin(ang)) >> 12;
        block->dy = (((arg2 * 31) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sy - *(u16*)&block->dy;
        ang2      = ang + 0x400;
        prim->y3  = *(u16*)&block->sy + *(u16*)&block->dy;
        block->dx = (((arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = (((arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = *(u16*)&block->sx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
}

/// Draws a textured billboard at the world position of `arg0`, projected
/// through `GsWSMATRIX`: one semi-transparent axis-aligned `POLY_FT4`, a
/// square of half-side `(s16)arg2 * 55` over the depth, raised so the
/// projected point sits three quarters of the way down it. `arg1` picks the
/// frame, a 56x56 cell in a four-by-two grid of the texture page. Nothing is
/// drawn when the projection flags an error.
void func_shelter_b3_dumping_hole_80186AB8(GsCOORDINATE2* arg0, s32 arg1, s32 arg2)
{
    void**         scratch;
    u8*            head;
    GpRingScratch* block;
    POLY_FT4*      prim;
    SVECTOR*       vec;
    DisplayState*  ds;
    s32            tex;
    u32            cell;
    s32            u1;
    s32            v0;
    s32            v1;
    s32            sarg;
    s32            t;
    s16            xy;
    u16            vz;

    scratch                                 = (void**)G_SCRATCH_HEAD;
    head                                    = *scratch;
    ((GpRingScratch*)(head - 0x18))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                   = (GpRingScratch*)(head - 0x18);
    block->vec.vy                           = *(u16*)&arg0->workm.t[1];
    vz                                      = *(u16*)&arg0->workm.t[2];
    tex                                     = arg1;
    SOFT_TOUCH_REG(tex);
    *scratch      = block;
    block->vec.vz = vz;
    vec           = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpRingScratch*)(head - 0x18))->sx);
    gte_stflg(&((GpRingScratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpRingScratch*)(head - 0x18))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        cell        = (u16)tex;
        prim->clut  = 0x4393;
        tex         = (cell & 3) * 0x38;
        SOFT_BARRIER();
        v0          = ((cell & 7) >> 2) * 0x38;
        u1          = tex + 0x37;
        prim->v0    = v0;
        prim->v1    = v0;
        v1          = v0 + 0x37;
        prim->u1    = u1;
        prim->u3    = u1;
        sarg        = (s16)arg2;
        prim->v2    = v1;
        prim->v3    = v1;
        t           = sarg * 0x38;
        prim->u0    = tex;
        prim->u2    = tex;
        block->step = (t - sarg) / block->otz;
        xy          = *(u16*)&block->sx - *(u16*)&block->step;
        prim->x0 = prim->x2 = xy;
        xy                  = *(u16*)&block->sx + *(u16*)&block->step;
        prim->x1 = prim->x3 = xy;
        xy                  = (*(u16*)&block->sy - *(u16*)&block->step) - (block->step >> 1);
        ds                  = &gDisplayState;
        prim->y0 = prim->y1 = xy;
        xy                  = *(u16*)&block->sy + (block->step >> 1);
        prim->y2 = prim->y3 = xy;
        addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x18;
}

void func_shelter_b3_dumping_hole_80186D4C(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    MATRIX*        m;
    s32            i;

    mem   = (GpEffWork*)arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        func_shelter_b3_dumping_hole_80186AB8(coord, (mem->age / 2) & 0xFFFF, 0x380);
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }
    if (arg0->state == 0) {
        m                            = &coord->coord;
        coord->sub                   = mem->parent;
        *(s32*)&coord->coord.m[0][0] = 0x1000;
        *(s32*)&m->m[0][2]           = 0;
        *(s32*)&m->m[1][1]           = 0x1000;
        *(s32*)&m->m[2][0]           = 0;
        m->m[2][2]                   = 0x1000;
        coord->coord.t[2]            = 0;
        coord->coord.t[1]            = 0;
        coord->coord.t[0]            = 0;
        coord->flg                   = 0;
        Gp_UpdateCoord(coord);
        arg0->state = 1;
    }
    mem->age += 1;
    switch (arg0->spawnArg1) {
        case 0:
            Gp_SpawnEff(0x6019A, coord, 0x14002400, NULL);
            arg0->spawnArg1 = 1;
            return;
        case 1:
            func_shelter_b3_dumping_hole_80186AB8(coord, (mem->age / 2) & 0xFFFF, 0x380);
            if (!(mem->age & 1)) {
                Gp_SpawnEff(0x6019A, coord, 0x1001400, NULL);
            }
            mem->age += 1;
            return;
        case 2:
            Gp_SpawnEff(0x6019A, coord, 0x10002380, NULL);
            for (i = 0; i < 4; i++) {
                Gp_SpawnEff(0x6019A, coord, 0x2002400, NULL);
                Gp_SpawnEff(0x60199, coord, 0x2202300, NULL);
            }
            arg0->spawnArg1 = 3;
            return;
        case 3:
            Gp_ReleaseState1CMem(mem, arg0);
            return;
    }
}
