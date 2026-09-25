#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>

#include "actors/actor_503500.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// Matrix table of the 0x3D8 block, which runs from 0x40 up to the block's
/// `obj160` display node: `func_actor_503500_80141FC8`'s sub-state 0 resets
/// entries 1..8 to an identity rotation with zero translation. The shared
/// `Actor503500Work` view names other blocks' fields over the same bytes.
typedef struct Actor503500Work3D8Mtx {
    /* 0x000 */ byte   pad_0[0x40];
    /* 0x040 */ MATRIX mats[9];
} Actor503500Work3D8Mtx;
STATIC_ASSERT_SIZEOF(Actor503500Work3D8Mtx, 0x160);

/// Chain state of the 0x3D8 block, which the shared `Actor503500Work` view
/// names with the 0x2EC block's fields: `func_actor_503500_80141448` samples a
/// cubic Bezier into `pts` (root first), re-aims the chain along it and hands
/// `angles` to `func_actor_503500_80142220`, which only reads the `vx` pitch of
/// entries 2..7. `field_3B6` / `field_3CC` are the same bytes as the shared
/// view's, loaded signed here.
typedef struct Actor503500Work3D8Chain {
    /* 0x000 */ byte    pad_0[0x2C8];
    /* 0x2C8 */ SVECTOR pts[9];
    /* 0x310 */ SVECTOR angles[9];
    /* 0x358 */ SVECTOR field_358; // far control point, local to the root's parent
    /* 0x360 */ byte    pad_360[0x54];
    /* 0x3B4 */ s16     field_3B4; // sway amplitude
    /* 0x3B6 */ s16     field_3B6; // sway fade-in, 0..0x1000
    /* 0x3B8 */ s16     phase[9];  // sway phase per link, stepped by 0x80
    /* 0x3CA */ byte    pad_3CA[0x2];
    /* 0x3CC */ s16     field_3CC; // weight of the rest pitch table
} Actor503500Work3D8Chain;
STATIC_ASSERT_SIZEOF(Actor503500Work3D8Chain, 0x3CE);

/// Element of `D_actor_503500_80177B60`, the 0x3D8 blocks
/// `func_actor_503500_8013FA74` clears for spawn slots 0xD..0x10. Like
/// `Actor503500Work2EC`, the shared `Actor503500Work` cannot be indexed at this
/// stride; the task's `field_1C` still points here through the shared view,
/// and `Actor503500Work3D8Mtx` / `Actor503500Work3D8Chain` are narrower views
/// of the same bytes. It opens with the light / colour matrices the init
/// republishes on `TmdObject::lightMtx` / `field_20`, then a private copy of
/// model parts 1..8's `coord` matrices.
typedef struct Actor503500Work3D8 {
    /* 0x000 */ MATRIX    light;
    /* 0x020 */ MATRIX    color;
    /* 0x040 */ MATRIX    mats[9];
    /* 0x160 */ GpObj     obj160;
    /* 0x180 */ GpRec18   rec180[8]; // obj160's table, count 8
    /* 0x240 */ GpObj     obj240;
    /* 0x260 */ GpRec18   rec260[4]; // obj240's table, count 4
    /* 0x2C0 */ GpEffArg  field_2C0; // record this block's effects are spawned with
    /* 0x2C8 */ SVECTOR   pts[9];
    /* 0x310 */ SVECTOR   angles[9];
    /* 0x358 */ SVECTOR   field_358;
    /* 0x360 */ SVECTOR   field_360; // field_358 before this frame's step
    /* 0x368 */ SVECTOR   field_368;
    /* 0x370 */ byte      pad_370[0x28];
    /* 0x398 */ s32       field_398; // step speed toward field_368
    /* 0x39C */ GpFixed16 field_39C; // speed limit; integer half is the arrival radius
    /* 0x3A0 */ byte      pad_3A0[0x4];
    /* 0x3A4 */ s16       field_3A4; // sub-state, see func_actor_503500_80142310
    /* 0x3A6 */ byte      pad_3A6[0x2];
    /* 0x3A8 */ s16       field_3A8; // hit countdown, raised by each landed id's stun
    /* 0x3AA */ s16       field_3AA;
    /* 0x3AC */ byte      pad_3AC[0x6];
    /* 0x3B2 */ u16       field_3B2; // fade level, stepped by 0x10 up to 0x1000
    /* 0x3B4 */ s16       field_3B4; // sway amplitude
    /* 0x3B6 */ s16       field_3B6; // sway fade-in, 0..0x1000
    /* 0x3B8 */ s16       phase[9];  // sway phase per link, seeded to i * 0x200
    /* 0x3CA */ byte      pad_3CA[0xA];
    /* 0x3D4 */ s8        field_3D4; // set while field_358 sits on field_368
    /* 0x3D5 */ s8        field_3D5;
    /* 0x3D6 */ s8        field_3D6;
    /* 0x3D7 */ s8        field_3D7; // TMD buffer countdown
} Actor503500Work3D8;
STATIC_ASSERT_SIZEOF(Actor503500Work3D8, 0x3D8);

/// The 0xC0 block `func_actor_503500_80144300` allocates: the display node,
/// the four-entry `GpRec18` table its `field_C` points at, the effect task it
/// reparents itself under, and the payload `func_actor_503500_80144778` steps
/// every frame. `field_84` is the world position (the coordinate's translation
/// in 16.16) and `field_94` the copy it restores from; `field_A4` is the
/// forward offset `ApplyMatrixLV` rotates out of `Task::spawnArg2`.
typedef struct Actor503500WorkC0 {
    /* 0x00 */ GpObj             obj;
    /* 0x20 */ GpRec18           rec[4];
    /* 0x80 */ Task*             field_80;
    /* 0x84 */ Actor503500FixVec field_84;
    /* 0x94 */ VECTOR            field_94;
    /* 0xA4 */ VECTOR            field_A4; // per-frame velocity added onto field_84
    /* 0xB4 */ s32               field_B4;
    /* 0xB8 */ s16               field_B8;
    /* 0xBA */ u16               field_BA; // sub-state frame counter
    /* 0xBC */ s8                field_BC; // sub-state index, -1 finishes the task
    /* 0xBD */ byte              pad_BD[0x1];
    /* 0xBE */ s8                field_BE; // set when a record's kind (key high half) is 1
    /* 0xBF */ s8                field_BF; // nonzero skips the push-back step
} Actor503500WorkC0;
STATIC_ASSERT_SIZEOF(Actor503500WorkC0, 0xC0);

/// The 0xB4 block `func_actor_503500_801448E8` allocates: the same head and
/// world-position pair as `Actor503500WorkC0`, then the payload
/// `func_actor_503500_80144B40` steps every frame. `field_A8` is the speed,
/// `Task::spawnArg2` or 0x100000 when the spawner passes none.
typedef struct Actor503500WorkB4 {
    /* 0x00 */ GpObj   obj;
    /* 0x20 */ GpRec18 rec[4];
    /* 0x80 */ Task*   field_80;
    /* 0x84 */ VECTOR  field_84;
    /* 0x94 */ VECTOR  field_94;
    /* 0xA4 */ byte    pad_A4[0x4];
    /* 0xA8 */ s32     field_A8;
    /* 0xAC */ s16     field_AC;
    /* 0xAE */ s16     field_AE;
    /* 0xB0 */ s8      field_B0;
    /* 0xB1 */ byte    pad_B1[0x3];
} Actor503500WorkB4;
STATIC_ASSERT_SIZEOF(Actor503500WorkB4, 0xB4);

/// Element of `D_actor_503500_801714E0`, the payload
/// `func_actor_503500_801437D0` sends the player as message 0x3FF. It picks one
/// of the two rows by which side of the player the hit coordinate lies on.
/// Every row seen so far points `field_0` at `D_actor_503500_801714C8`.
typedef struct Actor503500Msg3FF {
    /* 0x00 */ void* field_0;
    /* 0x04 */ s32   field_4;
    /* 0x08 */ s32   field_8;
    /* 0x0C */ s32   field_C;
    /* 0x10 */ s32   field_10;
} Actor503500Msg3FF;
STATIC_ASSERT_SIZEOF(Actor503500Msg3FF, 0x14);

/// Work block of the knock-back task `func_actor_503500_801437D0` spawns
/// (`Mem_Set(_, 0x38)` in `func_actor_503500_80143AC0`). `rot` is a copy of the
/// rotation handed over in `Task::spawnArg2`; every frame `speed` is pushed
/// through it by `ApplyMatrixLV` and added onto the 16.16 `pos`, whose integer
/// halves go to the player as message 0x3FE. `field_34` counts frames spent at
/// zero speed and `field_36` the remaining camera-shake frames.
typedef struct Actor503500Work38 {
    /* 0x00 */ Actor503500FixVec pos;
    /* 0x10 */ MATRIX            rot;
    /* 0x30 */ s32               speed;
    /* 0x34 */ s16               field_34;
    /* 0x36 */ s16               field_36;
} Actor503500Work38;
STATIC_ASSERT_SIZEOF(Actor503500Work38, 0x38);

/// Payload of message 0x3FE: the displacement `func_actor_503500_80143AC0` asks
/// the player to move by. A nonzero reply stops the push.
typedef struct Actor503500Msg3FE {
    /* 0x00 */ s32  x;
    /* 0x04 */ s32  y;
    /* 0x08 */ s32  z;
    /* 0x0C */ byte pad_C[0x4];
    /* 0x10 */ s16  field_10;
    /* 0x12 */ s8   field_12;
    /* 0x13 */ byte pad_13[0x1];
} Actor503500Msg3FE;
STATIC_ASSERT_SIZEOF(Actor503500Msg3FE, 0x14);

/// Work block of the 0xF4 enemy whose state-0 init is
/// `func_actor_503500_8013ECBC` (`D_actor_503500_80177A6C`), viewed through its
/// own type rather than the shared `Actor503500Work`: it keeps a halfword at
/// 0xEC -- `func_actor_503500_8013F4A4` stores one there and
/// `func_actor_503500_8013F9D4` clears it -- where the shared view already
/// names the 0xF0 block's `field_EC` / `field_ED` byte pair. That collision is
/// why this enemy gets a separate view. Its sub-state index is `field_F0`, the
/// one `func_actor_503500_8013F8AC` dispatches on.
typedef struct Actor503500WorkF4 {
    /* 0x00 */ GpObj obj;      // the display node, as in `Actor503500Work`
    /* 0x20 */ byte  pad_20[0xC8];
    /* 0xE8 */ s16   field_E8; // per-frame countdown, as in `Actor503500Work`
    /* 0xEA */ u16   field_EA; // sub-state frame counter
    /* 0xEC */ s16   field_EC;
    /* 0xEE */ byte  pad_EE[0x2];
    /* 0xF0 */ s8    field_F0; // sub-state index
    /* 0xF1 */ s8    field_F1; // sub-state phase, cleared with field_F0
    /* 0xF2 */ byte  pad_F2[0x2];
} Actor503500WorkF4;
STATIC_ASSERT_SIZEOF(Actor503500WorkF4, 0xF4);

/// The second 0xF4 block: the enemy whose state-0 init is
/// `func_actor_503500_8013CAE4` (`Mem_Set` over slot `spawnArg1` of
/// `D_actor_503500_801770E8`), viewed through its own type rather than the
/// shared `Actor503500Work`. It is named after its array because size no
/// longer tells the two 0xF4 shapes apart: this one keeps its two sub-state
/// counters as halfwords at 0xEC and 0xEE -- `func_actor_503500_8013D1CC` and
/// `func_actor_503500_8013D558` step 0xEC and `func_actor_503500_8013DBA8`
/// clears both -- where the shared view names the 0xEC / 0xED / 0xEE byte
/// triple the other 0xF0 blocks dispatch on, and where `Actor503500WorkF4`
/// puts its counter pair at 0xEA / 0xEC. Its sub-state index is `field_F0`,
/// the one `func_actor_503500_8013D990` dispatches on.
typedef struct Actor503500Work770E8 {
    /* 0x00 */ GpObj    obj;
    /* 0x20 */ GpRec18  rec[8];   // Gp_InitRec18Table(rec, 8, 0)
    /* 0xE0 */ GpEffArg field_E0; // record the block's effects are spawned with
    /* 0xE8 */ s16      field_E8; // per-frame countdown, as in `Actor503500Work`
    /* 0xEA */ byte     pad_EA[0x2];
    /* 0xEC */ u16      field_EC; // sub-state frame counter
    /* 0xEE */ s16      field_EE;
    /* 0xF0 */ s8       field_F0; // sub-state index
    /* 0xF1 */ s8       field_F1; // sub-state phase, cleared with field_F0
    /* 0xF2 */ byte     pad_F2[0x2];
} Actor503500Work770E8;
STATIC_ASSERT_SIZEOF(Actor503500Work770E8, 0xF4);

/// The first 0xF4 block: the enemy whose state-0 init is
/// `func_actor_503500_8013BEE4` (`Mem_Set` over `D_actor_503500_801776A0`),
/// viewed through its own type in the one function that needs it. Its death
/// sub-state `func_actor_503500_8013C558` keeps three halfwords at 0xEA /
/// 0xEC / 0xEE -- a frame counter, a scale that shrinks from 0x1000 and the
/// step it shrinks by -- where the shared view names the 0xEC / 0xED / 0xEE
/// byte triple. Its sub-state index is `field_F0`, the one
/// `func_actor_503500_8013CA34` dispatches on.
typedef struct Actor503500Work776A0 {
    /* 0x00 */ GpObj    obj;
    /* 0x20 */ GpRec18  rec[8];   // Gp_InitRec18Table(rec, 8, 0)
    /* 0xE0 */ GpEffArg field_E0; // record the block's effects are spawned with
    /* 0xE8 */ s16      field_E8; // per-frame countdown, as in `Actor503500Work`
    /* 0xEA */ s16      field_EA; // sub-state frame counter
    /* 0xEC */ u16      field_EC; // scale handed to func_actor_503500_80135E20
    /* 0xEE */ s16      field_EE; // per-frame step of field_EC, stepped down by 4
    /* 0xF0 */ s8       field_F0; // sub-state index
    /* 0xF1 */ s8       field_F1; // sub-state phase, cleared with field_F0
    /* 0xF2 */ byte     pad_F2[0x2];
} Actor503500Work776A0;
STATIC_ASSERT_SIZEOF(Actor503500Work776A0, 0xF4);

/// Element of `D_actor_503500_801774C0`, the two 0xF0 blocks
/// `func_actor_503500_8013AD64` clears for spawn slots 4 and 5. The shared
/// `Actor503500Work` cannot be indexed at this stride, so the array gets its
/// own type; the fields agree with the shared view, and the task's
/// `field_1C` still points at the block through that view. `field_EC` holds
/// the slot (`spawnArg1 - 4`) that selects this enemy's parent part and
/// local offset.
typedef struct Actor503500Work774C0 {
    /* 0x00 */ GpObj    obj;
    /* 0x20 */ GpRec18  rec;
    /* 0x38 */ byte     pad_38[0xA8];
    /* 0xE0 */ GpEffArg field_E0; // record the block's effects are spawned with
    /* 0xE8 */ byte     pad_E8[0x4];
    /* 0xEC */ s8       field_EC;
    /* 0xED */ byte     pad_ED[0x3];
} Actor503500Work774C0;
STATIC_ASSERT_SIZEOF(Actor503500Work774C0, 0xF0);

extern TaskDesc D_actor_503500_8016E9F0;
/// Per-spawn enemy parameter table indexed by `Task::spawnArg1`;
/// `func_actor_503500_8013BEE4` and `func_actor_503500_8013ECBC` park the row
/// in `GpEnemy::param` and seed the enemy's HP from its `hpMax`.
extern GpPairSrcE D_actor_503500_8016E7EC[];
/// Local offset the 0xF4 enemy applies to both its `GpEnemy::bodyPos` and
/// its display node's 0x10 vector.
extern SVECTOR         D_actor_503500_8016F36C;
extern Actor503500Work D_actor_503500_80177A6C;
/// The same pair for the other 0xF4 enemy, the one at
/// `D_actor_503500_801776A0`.
extern SVECTOR         D_actor_503500_8016F1B0;
extern Actor503500Work D_actor_503500_801776A0;
/// Eighteen effect offsets `func_actor_503500_8013C558` picks from at random.
extern SVECTOR D_actor_503500_8016F1B8[];
/// Rest pitch of each link of the 0x3D8 enemy's chain, scaled by
/// `Actor503500Work3D8Chain::field_3CC` in `func_actor_503500_80141448`.
extern s16 D_actor_503500_8016F434[];
void       func_actor_503500_80141A44(SVECTOR* pts, SVECTOR* p3, s32 len, s32 pos, s32* out);
void       func_actor_503500_80142220(SVECTOR* angles, GsCOORDINATE2* nodes);
/// The same pair for the 0xF0 enemy at `D_actor_503500_8017797C`.
extern SVECTOR         D_actor_503500_8016F2D8;
extern Actor503500Work D_actor_503500_8017797C;
/// Per-slot parent part index and local offset of the two 0xF0 enemies in
/// `D_actor_503500_801774C0`, indexed by `spawnArg1 - 4`.
extern s32                  D_actor_503500_8016F0E8[];
extern SVECTOR              D_actor_503500_8016F0F0[];
extern Actor503500Work774C0 D_actor_503500_801774C0[];
extern RECT                 D_actor_503500_8016F100;
/// `func_actor_503500_8013B8D0`'s nine effect offsets, one per frame, and the
/// per-side pair of rects it moves on frame 0x14, indexed by `field_EC`.
extern SVECTOR D_actor_503500_8016F168[];
/// Declared in `gameplay/gameplay.h`, which conflicts with this TU's headers.
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
/// The same three-entry cycle for `func_actor_503500_80140654`.
extern SVECTOR D_actor_503500_8016F448[];
/// Main-executable counter the actor paces periodic effects by (its value
/// modulo 6 or 12, its low bits).
extern s32  D_80070F70;
extern RECT D_actor_503500_8016F148[][2];
/// Offsets `func_actor_503500_8013F4A4` spawns its two effects at: rows 0-5
/// for the per-frame effect, rows 3-5 for the odd-frame one.
extern SVECTOR D_actor_503500_8016F374[];
extern RECT    D_actor_503500_8016F3A4;
/// `func_actor_503500_8013E384`'s per-shot tables, indexed by shot 0..5: a
/// spawn-arg term (`<< 12`) and the rotation each spawned child is turned by.
extern s16     D_actor_503500_8016F2E0[];
extern SVECTOR D_actor_503500_8016F2EC[];
/// `func_actor_503500_8013D1CC`'s spawn offset and rotation for its one child.
extern SVECTOR D_actor_503500_8016F258;
extern SVECTOR D_actor_503500_8016F260;
extern MATRIX* D_80073B8C;
/// The same pair for `func_actor_503500_8013E740`: nine effect offsets, one
/// picked by frame and one by `Gp_LcgState`, and the rect it moves on frame 8.
extern SVECTOR D_actor_503500_8016F31C[];
extern RECT    D_actor_503500_8016F364;
extern u32     Gp_LcgState;
void           func_actor_503500_8013BC54(Task* arg0);
/// Per-slot local offset of the 0xF4 enemies in `D_actor_503500_801770E8`,
/// indexed by `spawnArg1`.
extern SVECTOR              D_actor_503500_8016F210[];
extern Actor503500Work770E8 D_actor_503500_801770E8[];
void                        func_actor_503500_8013D85C(Task* arg0);
/// The two three-row effect offset tables `func_actor_503500_8013D558`
/// spawns from: the first for spawn slot 7, the second for every other slot.
extern SVECTOR D_actor_503500_8016F278[];
extern SVECTOR D_actor_503500_8016F290[];
extern s16     D_80073BA0;
void           func_actor_503500_8013AF60(Task* arg0, Actor503500Work* work, GpRec18* rec, s32 count);
void           func_actor_503500_8013CCBC(Task* arg0, Actor503500Work* work, GpRec18* rec, s32 count);
void           func_actor_503500_8013C088(Task* arg0, Actor503500Work* work, GpRec18* rec, s32 count);
void           func_actor_503500_8013DEB4(Task* arg0, Actor503500Work* work, GpRec18* rec, s32 count);
void           func_actor_503500_8013EA2C(Task* arg0);
void           func_actor_503500_8013EC20(Task* arg0, s32 arg1);
void           func_actor_503500_801431EC(Task* arg0, GpObj* arg1, GpRec18* arg2, s32 arg3);
void           func_actor_503500_80140D38(Task* arg0, GpObj* arg1, GpRec18* arg2, s32 arg3);
void           func_actor_503500_8014215C(Task* arg0, GpObj* arg1, GpRec18* arg2, s32 arg3);
void           func_actor_503500_801437D0(Task* arg0, GpRec18* arg1, s32 arg2);
void           func_actor_503500_8013B460(Task* arg0);
void           func_actor_503500_8013B8D0(Task* arg0);
void           func_actor_503500_8013BE0C(Task* arg0);
void           func_actor_503500_8013BCB4(Task* arg0);
void           func_actor_503500_8013C900(Task* arg0);
void           func_actor_503500_8013C9DC(Task* arg0);
void           func_actor_503500_8013C960(Task* arg0);
void           func_actor_503500_8013CA34(Task* arg0);
void           func_actor_503500_8013CA74(Task* arg0, s8 arg1);
void           func_actor_503500_8013D8BC(Task* arg0);
void           func_actor_503500_8013D914(Task* arg0);
void           func_actor_503500_8013D990(Task* arg0);
void           func_actor_503500_8013DA2C(Task* arg0, s32 arg1);
void           func_actor_503500_8013DC4C(Task* arg0);
void           func_actor_503500_8013F8AC(Task* arg0);
void           func_actor_503500_801422B8(s32 p0, s32 p1, s32 p2, s32 p3, SVECTOR* coeff);
void           func_actor_503500_801440F0(Task* arg0);
void           func_actor_503500_8013BD88(Task* arg0);
void           func_actor_503500_8013C558(Task* arg0);
void           func_actor_503500_8013E384(Task* arg0);
void           func_actor_503500_8013E740(Task* arg0);
void           func_actor_503500_8013EBE4(Task* arg0);
void           func_actor_503500_8013EA8C(Task* arg0);
void           func_actor_503500_8013EAE4(Task* arg0);
void           func_actor_503500_8013EB60(Task* arg0);
void           func_actor_503500_8013F778(Task* arg0);
void           func_actor_503500_8013F7D8(Task* arg0);
void           func_actor_503500_8013F830(Task* arg0);
/// Global "everything is frozen" mode byte in the main executable: 1 pauses the
/// actor, 2 hides it, anything else runs the normal per-frame chain.
extern u8 D_801153F4;
void      func_actor_503500_8013DBA8(Task* arg0, s32 arg1);
void      func_actor_503500_8013F328(Task* arg0);
void      func_actor_503500_8013F4A4(Task* arg0);
void      func_actor_503500_8013F948(Task* arg0);
void      func_actor_503500_8013F984(Task* arg0);
void      func_actor_503500_8013F9D4(Task* arg0, s32 arg1);
void      func_actor_503500_801400A4(Task* arg0);
void      func_actor_503500_80140654(Task* arg0);
void      func_actor_503500_80140BE8(Task* arg0);
void      func_actor_503500_80141248(Task* arg0);
void      func_actor_503500_80141448(Task* arg0);
void      func_actor_503500_80141B94(Task* arg0);
void      func_actor_503500_80141D7C(Task* arg0);
void      func_actor_503500_80141E64(Task* arg0);
void      func_actor_503500_80141F48(Task* arg0);
void      func_actor_503500_80141FC8(Task* arg0);
void      func_actor_503500_801420C4(Task* arg0);
void      func_actor_503500_801421A8(Task* arg0);
void      func_actor_503500_80142310(Task* arg0, s32 arg1);
void      func_actor_503500_8014271C(Task* arg0);
void      func_actor_503500_80142980(Task* arg0);
void      func_actor_503500_80143FFC(Task* arg0);
void      func_actor_503500_80144004(Task* arg0);
void      func_actor_503500_80144098(Task* arg0, s32 arg1, GpEnemy* arg2);
void      func_actor_503500_8014418C(Task* arg0);
void      func_actor_503500_801441E8(Task* arg0);
void      func_actor_503500_80144238(Task* arg0, s32 arg1);
void      func_actor_503500_80144520(Task* arg0);
void      func_actor_503500_80144778(Task* arg0);
void      func_actor_503500_80144B40(Task* arg0);
void      func_actor_503500_80144E10(Task* arg0);
void      func_actor_503500_8013BE48(Task* arg0, s32 arg1);
void      func_actor_503500_8013B60C(Task* arg0, s32 side, s32 arg2);
/// Per-slot local offset and Z-Y-X angles of the effects
/// `func_actor_503500_8013B60C` spawns; `side` mirrors the offset's X and the
/// angle's Y.
extern Actor503500UVec D_actor_503500_8016F108[];
extern Actor503500UVec D_actor_503500_8016F128[][3];
/// Per-slot parent part index and local offset of the 0x224 enemy in
/// `D_actor_503500_80178AC0`, indexed by `spawnArg1 - 0xA`, and the
/// translation it gives the task's own coordinate.
extern s32     D_actor_503500_80171464[];
extern SVECTOR D_actor_503500_80171480[];
/// Per-id flag table of `func_actor_503500_801431EC`: a hit whose id has bit
/// 0x8000 set and a non-zero entry at `id & 0x7F` switches the enemy to state 2
/// of `func_actor_503500_80144238`.
extern s8                 D_actor_503500_80171490[];
extern SVECTOR            D_actor_503500_80171478;
extern Actor503500Work224 D_actor_503500_80178AC0[];
/// `func_actor_503500_80144300`'s `Gp_PackPair` slot, two before
/// `D_actor_503500_8016E7D4`, and the local offset of its display node.
extern GpU16Pair*      D_actor_503500_8016E7CC[];
extern Actor503500UVec D_actor_503500_801715AC;
/// `func_actor_503500_801448E8`'s `Gp_PackPair` slot, the one before
/// `D_actor_503500_8016E7D4`, and the local offset of its display node.
extern GpU16Pair*      D_actor_503500_8016E7D0[];
extern Actor503500UVec D_actor_503500_801715B4;
/// Frame count `func_actor_503500_80144B40` holds its sub-state 1 for, indexed
/// by `Task::spawnArg1`.
extern s32 D_actor_503500_801715BC[];
/// libgte routine right after `RotMatrixX` in the main executable (likely
/// `RotMatrixY`); rotates `m` in place by `angle`.
void func_8004BFF8(s32 angle, MATRIX* m);
/// Task table `func_actor_503500_801437D0` spawns entry 0 from.
extern TaskDesc D_actor_503500_8017146C;
/// Message 0x3FF payloads, indexed by side (see `Actor503500Msg3FF`).
extern Actor503500Msg3FF D_actor_503500_801714E0[];
/// Payload of the 0x3F8 query `func_actor_503500_801437D0` sends the player.
extern s32 D_actor_503500_80171544;
void       func_actor_503500_80143F78(Task* arg0);
void       func_actor_503500_8014473C(Task* arg0);
void       func_actor_503500_80144DA8(Task* arg0);
/// Per-slot tables of the 0x3D8 enemies in `D_actor_503500_80177B60`: world
/// translation and rotation of the task's coordinate (indexed by
/// `spawnArg1 - 0xD`), the local offset its `GpEnemy::bodyPos` and first
/// display node share, and the second display node's local offset.
extern SVECTOR         D_actor_503500_8016F3AC[];
extern SVECTOR         D_actor_503500_8016F3CC[];
extern SVECTOR         D_actor_503500_8016F3EC;
extern Actor503500UVec D_actor_503500_8016F3F4[];
/// Rest control point seeded into `field_368` / `field_358`, also indexed by
/// `spawnArg1 - 0xD`. The code reaches it as `&table[spawnArg1 - 0xD]`, which
/// folds the bias into the address constant (`D_actor_503500_8016F414 - 0x68`
/// == `D_actor_503500_8016F3AC`); a distinct symbol is what keeps CSE from
/// sharing that address with the `D_actor_503500_8016F3AC` access above.
extern SVECTOR            D_actor_503500_8016F414[];
extern Actor503500Work3D8 D_actor_503500_80177B60[];
void                      func_actor_503500_80141D04(Task* arg0);
/// The rect `func_actor_503500_80142980` moves when `field_220` is set, and
/// the effect offsets of that death fall: five for the two-part spray, cycled,
/// and the per-side offset of the phase-3 spark.
extern RECT    D_actor_503500_8017155C;
extern SVECTOR D_actor_503500_80171564[5];
extern SVECTOR D_actor_503500_8017158C;
extern SVECTOR D_actor_503500_80171594;
/// Knock-back work block of `func_actor_503500_80143AC0`.
extern Actor503500Work38 D_actor_503500_80178F10;
/// Rows 2 and 3 of `D_actor_503500_801714E0` (the 0x3FF payload after the push,
/// indexed by side) and row 4 (the one sent once the player's weapon animation
/// word has been parked in `D_actor_503500_801714DC`).
extern Actor503500Msg3FF D_actor_503500_80171508[];
extern Actor503500Msg3FF D_actor_503500_80171530;
extern s32               D_actor_503500_801714DC;
/// Script pair handed to `Gp_SpawnScript18` when the push starts.
extern u8         D_actor_503500_8017159C[];
extern u8         D_actor_503500_801715A4[];
extern GpAnimBlk* Gp_PlayerAnimBlkTbl[];
extern u16        Gp_WeaponIdBase[];
void              func_actor_503500_8013AD64(Task* arg0);
void              func_actor_503500_8013BBCC(Task* arg0);
void              func_actor_503500_8013BEE4(Task* arg0);
void              func_actor_503500_8013C878(Task* arg0);
void              func_actor_503500_8013CAE4(Task* arg0);
void              func_actor_503500_8013D7D4(Task* arg0);
void              func_actor_503500_8013DD10(Task* arg0);
void              func_actor_503500_8013E9A4(Task* arg0);
void              func_actor_503500_8013ECBC(Task* arg0);
void              func_actor_503500_8013F6F0(Task* arg0);
void              func_actor_503500_8013FA74(Task* arg0);
void              func_actor_503500_8013FF0C(Task* arg0);
void              func_actor_503500_801423C8(Task* arg0);
void              func_actor_503500_80143EB4(Task* arg0);
void              func_actor_503500_80144300(Task* arg0);
void              func_actor_503500_801446E4(Task* arg0);
void              func_actor_503500_801448E8(Task* arg0);
void              func_actor_503500_80144D50(Task* arg0);

/// `Task::state` handlers `func_actor_503500_8013BE8C` dispatches through.
const TaskFuncTable3 D_actor_503500_80131FF0 = {
    {
        func_actor_503500_8013AD64,
        func_actor_503500_8013BBCC,
        func_actor_503500_8013BC54,
    },
};

/// State-0 init of the 0xF0 enemies in spawn slots 4 and 5: clears the slot's
/// block in `D_actor_503500_801774C0`, hangs the task's coordinate off the
/// parent part the slot names, links the enemy and its display node, and
/// hands the task to its exit callback.
void func_actor_503500_8013AD64(Task* arg0)
{
    GpEnemy*              enemy;
    Task*                 parent;
    GsCOORDINATE2*        coord;
    MATRIX*               mtx;
    GpRec18*              rec;
    Actor503500Work774C0* work;
    s32                   idx;

    idx    = arg0->spawnArg1 - 4;
    enemy  = arg0->spawnArg2;
    parent = arg0->parent;
    work   = &D_actor_503500_801774C0[idx];
    coord  = ((TmdObject*)arg0->extra)->coords;
    Mem_Set(work, 0, 0xF0);
    arg0->work     = (Actor503500Work*)work;
    work->field_EC = idx;

    coord->sub                   = &((TmdObject*)parent->extra)->coords[D_actor_503500_8016F0E8[idx]];
    *(s32*)&coord->coord.m[0][0] = 0x1000;
    mtx                          = &coord->coord;
    *(s32*)&mtx->m[0][2]         = 0;
    *(s32*)&mtx->m[1][1]         = 0x1000;
    *(s32*)&mtx->m[2][0]         = 0;
    mtx->m[2][2]                 = 0x1000;
    enemy->field_4               = mtx;
    enemy->field_48              = 0;
    Gp_LinkNode(&enemy->node);
    enemy->coord       = coord;
    enemy->node.flags |= 9;
    enemy->bodyPos.vx  = D_actor_503500_8016F0F0[idx].vx;
    enemy->bodyPos.vy  = D_actor_503500_8016F0F0[idx].vy;
    enemy->bodyPos.vz  = D_actor_503500_8016F0F0[idx].vz;
    rec                = &work->rec;
    enemy->param       = &D_actor_503500_8016E7EC[arg0->spawnArg1];
    enemy->recs        = rec;
    enemy->hp          = enemy->param->hpMax;

    work->obj.coord    = coord;
    work->obj.ctx.recs = rec;
    work->obj.pos.vx   = D_actor_503500_8016F0F0[idx].vx;
    work->obj.pos.vy   = D_actor_503500_8016F0F0[idx].vy;
    work->obj.pos.vz   = D_actor_503500_8016F0F0[idx].vz;
    work->obj.key      = 0x30023;
    work->obj.radius   = 0x5DC;
    work->obj.flags    = 1;
    Gp_LinkObj(2, &work->obj);
    Gp_InitRec18Table(rec, 8, 0);
    work->field_E0.spawnArgLo = 0x600;
    work->field_E0.coord      = coord;
    work->field_E0.spawnArgHi = 3;
    work->obj.flags          |= 0x8000;
    MoveImage(&D_actor_503500_8016F100, 0, 0x105);
    arg0->exitCallback = func_actor_503500_8013BC54;
    arg0->state       += 1;
}

/// Applies this frame's hits from the collision records `rec[0..count)`,
/// like `func_actor_503500_80137C90`: each attack id is taken once, only
/// type-2 ids land while the `field_E8` countdown is clear, and a hit that
/// empties `field_40` starts state 2. The hit effect is pulled to 1600 units
/// along the contact offset and placed at the `field_EC` sub-state's offset.
/// `arg1` is passed by the caller but unused.
void func_actor_503500_8013AF60(Task* arg0, Actor503500Work* arg1, GpRec18* rec, s32 count)
{
    MATRIX           mtx;
    MATRIX           rot;
    VECTOR           d;
    SVECTOR          pos;
    Actor503500Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   src;
    s16              stun;
    u32              id;
    s32              dmg;
    s32              crit;
    s32              scale;
    s32              i;
    s32              j;

    enemy = arg0->spawnArg2;
    work  = arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    for (i = 0; i < count; i++) {
        id = rec[i].key;
        for (j = 0; j < i; j++) {
            if (rec[j].key == id) {
                goto next;
            }
        }
        if ((id & 0xFFFF0000) == 0x10000) {
            continue;
        }
        if ((id & 0xFFFF0000) != 0x20000) {
            continue;
        }
        if (work->field_E8 != 0) {
            continue;
        }
        src = ((TmdObject*)Gp_ActorSlots[(id >> 7) & 1]->extra)->coords;
        Gp_ComposeParentWorld(coord, &mtx, &pos);
        d.vx = src->coord.t[0] - pos.vx;
        d.vy = src->coord.t[1] - pos.vy;
        d.vz = src->coord.t[2] - pos.vz;
        crit = 0;
        dmg  = Gp_ComputeDamage(id, SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz), crit, crit);
        if (Gp_RollEnemyChance(enemy, id, crit) != 0) {
            dmg *= 4;
            crit = 1;
        }
        func_800E2C78((GpObj40*)enemy, id, dmg, 0);
        func_800DA6E8(&enemy->node, dmg, 0);
        enemy->hp -= dmg;
        if (enemy->hp <= 0) {
            func_actor_503500_8013BE48(arg0, 2);
        }
        switch (Gp_GetIdParam0(id) & 0xFFFF) {
            case 0:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
                break;
            case 1:
                Gp_SetObjFlag1((GpObj4C*)enemy);
                break;
            case 2:
                Gp_SetObjFlag2((GpObj5D*)enemy, id, 0);
                break;
            case 3:
                Gp_SetObjFlag4((GpObj5C*)enemy, id, 0);
                break;
        }
        TRANSPOSE_ROT(&coord->workm, &rot);
        pos.vx = rec[i].point.vx - coord->workm.t[0];
        pos.vy = rec[i].point.vy - coord->workm.t[1];
        pos.vz = rec[i].point.vz - coord->workm.t[2];
        scale  = 0x640000 / SquareRoot0(pos.vx * pos.vx + pos.vy * pos.vy + pos.vz * pos.vz);
        pos.vx = pos.vx * scale / 4096;
        pos.vy = pos.vy * scale / 4096;
        pos.vz = pos.vz * scale / 4096;
        gte_SetRotMatrix(&rot);
        gte_ldv0(&pos);
        gte_rtv0();
        gte_stsv(&pos);
        pos.vx += D_actor_503500_8016F0F0[work->field_EC].vx;
        pos.vy += D_actor_503500_8016F0F0[work->field_EC].vy;
        pos.vz += D_actor_503500_8016F0F0[work->field_EC].vz;
        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &pos, &work->field_E0);
        if (crit != 0) {
            Gp_SpawnEff(0x6009C, coord, 0, &pos);
        }
        stun = Gp_GetIdParam2(id);
        if (work->field_E8 < stun) {
            work->field_E8 = stun;
        }
    next:;
    }
}

void func_actor_503500_8013B460(Task* arg0)
{
    Actor503500Work* work;
    s16              angle;
    s32              offset;
    s32              side;

    work = arg0->work;
    if (func_actor_503500_8013608C(arg0->parent) != 0) {
        func_actor_503500_8013BE48(arg0, 0);
        func_actor_503500_8013611C(arg0->spawnArg1);
        return;
    }
    switch (work->field_EE) {
        case 0:
            func_actor_503500_80135FB4(arg0->parent, 9, 0x10);
            work->field_EE++;
            break;
        case 1:
            work->field_EA++;
            if (work->field_EA >= 0x80) {
                offset = -0x12C;
                angle  = func_actor_503500_80136134(arg0->parent);
                side   = work->field_EC;
                if (side != 0) {
                    offset = 0x12C;
                }
                if (angle > -0x400 - offset && angle < 0x400 - offset) {
                    func_actor_503500_8013B60C(arg0, side, 0);
                }
                if (angle < offset - 0x400 || offset + 0x400 < angle) {
                    func_actor_503500_8013B60C(arg0, side, 1);
                }
                work->field_EE++;
            }
        case 2:
            if (func_actor_503500_80136014(arg0->parent, 9) != 0) {
                func_actor_503500_8013BE48(arg0, 0);
            }
            break;
    }
}

/// Spawns effect slot `arg2` of `D_actor_503500_8016E9F0` on the task's own
/// coordinate: the child's translation is the parent world position plus the
/// slot offset rotated into that frame, and its rotation is the parent's world
/// rotation times `RotMatrixZYX` of the slot angles. The negations go through
/// an `s32` so the sign extension of the `u16` component survives.
void func_actor_503500_8013B60C(Task* arg0, s32 side, s32 arg2)
{
    SVECTOR        pos;
    SVECTOR        ofs;
    MATRIX         m;
    GsCOORDINATE2* src;
    GsCOORDINATE2* coord;
    Task*          task;
    s32*           dst;
    s32*           p;
    s32            i;
    s32            t;
    u16            vx;
    u16            vy;

    src  = ((TmdObject*)arg0->extra)->coords;
    task = Task_SpawnFromTable(&D_actor_503500_8016E9F0, 1, 0, 0xA00000);
    if (task != NULL) {
        Gp_ComposeParentWorld(src, &m, &pos);
        coord  = ((TmdObject*)task->extra)->coords;
        ofs.vx = vx = D_actor_503500_8016F108[arg2].vx;
        ofs.vy      = D_actor_503500_8016F108[arg2].vy;
        ofs.vz      = D_actor_503500_8016F108[arg2].vz;
        if (side != 0) {
            t      = -(s16)vx;
            ofs.vx = t;
        }
        gte_SetRotMatrix(&m);
        gte_ldv0(&ofs);
        gte_rtv0();
        gte_stsv(&ofs);
        coord->coord.t[0] = pos.vx + ofs.vx;
        coord->coord.t[1] = pos.vy + ofs.vy;
        coord->coord.t[2] = pos.vz + ofs.vz;
        dst               = (s32*)coord->coord.m;
        p                 = (s32*)m.m;
        for (i = 0; i < 4; i++) {
            *dst++ = *p++;
        }
        coord->coord.m[2][2] = m.m[2][2];
        pos.vx               = D_actor_503500_8016F128[arg2][0].vx;
        pos.vy = vy = D_actor_503500_8016F128[arg2][0].vy;
        pos.vz      = D_actor_503500_8016F128[arg2][0].vz;
        if (side != 0) {
            t      = -(s16)vy;
            pos.vy = t;
        }
        RotMatrixZYX(&pos, &m);
        gte_SetRotMatrix(&coord->coord);
        gte_ldclmv(&m);
        gte_rtir();
        gte_stclmv(&coord->coord);
        gte_ldclmv((char*)&m + 2);
        gte_rtir();
        gte_stclmv((char*)&coord->coord + 2);
        gte_ldclmv((char*)&m + 4);
        gte_rtir();
        gte_stclmv((char*)&coord->coord + 4);
    }
}

/// Death state of the 0xF0 block, stepped by `field_EE`: phase 0 is the death
/// setup shared with `func_actor_503500_8013F4A4`; phase 1 spawns a mirrored
/// pair of effects per frame from `D_actor_503500_8016F168`, drifting with the
/// frame count, moves the side's VRAM rects on frame 0x14 and leaves at 0x1F.
void func_actor_503500_8013B8D0(Task* arg0)
{
    GpEnemy*         enemy;
    Actor503500Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          vec;
    s32              pan;
    s32              side;
    s32              n;
    s32              i;
    s32              t;

    enemy = arg0->spawnArg2;
    work  = arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    switch (work->field_EE) {
        case 0:
            work->obj.flags &= 0x7FFF;
            enemy->recs      = 0;
            Gp_UnlinkNode(&enemy->node);
            func_actor_503500_80135CE8(arg0->parent, arg0->spawnArg1);
            work->field_E8 = 0;
            ((void (*)(s32))Gp_IncStateF0Ref)(0);
            Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
            func_actor_503500_80136048(arg0->parent);
            enemy->reactionFlags &= 0xF0;
            pan                   = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(0x40230010, pan, (s8)(gpGetObjDepth(coord) / 2));
            work->field_EE++;
            break;
        case 1:
            if (func_actor_503500_801360BC(arg0->spawnArg1, 3) != 0) {
                i       = (s16)(work->field_EA % 9);
                vec.vx  = D_actor_503500_8016F168[i].vx;
                vec.vy  = D_actor_503500_8016F168[i].vy;
                vec.vz  = D_actor_503500_8016F168[i].vz;
                vec.vx -= work->field_EA * 10;
                vec.vy -= work->field_EA * 20;
                if (work->field_EC != 0) {
                    t      = vec.vx;
                    vec.vx = -t;
                }
                Gp_SpawnEff(0x60055, coord, 0x1800, &vec);
                Gp_SpawnEff(0x60070, coord, 0x80008600, &vec);
                t      = vec.vz;
                vec.vz = -t;
                Gp_SpawnEff(0x60055, coord, 0x1800, &vec);
                Gp_SpawnEff(0x60070, coord, 0x80008600, &vec);
            }
            work->field_EA++;
            if (work->field_EA >= 0x1F) {
                SndEvt_EnqueueType7(0x40230010, 0x2D);
                work->field_EE++;
            } else if (work->field_EA == 0x14) {
                side = work->field_EC;
                n    = 2;
                if (side != 0) {
                    n = 4;
                }
                MoveImage(&D_actor_503500_8016F148[side][0], (n << 6) + 0x140, 0x100);
                MoveImage(&D_actor_503500_8016F148[side][1], 0, n + 0xF7);
            }
            break;
        default:
            func_actor_503500_8013611C(arg0->spawnArg1);
            arg0->state++;
            break;
    }
}

void func_actor_503500_8013BBCC(Task* arg0)
{
    GpEnemy*       enemy;
    GsCOORDINATE2* coord;

    enemy = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (D_801153F4 == 1) {
        return;
    }
    if (D_801153F4 == 2) {
        enemy->node.flags |= 1;
        return;
    }
    coord->flg = 0;
    if (enemy->reactionFlags != 0) {
        func_actor_503500_8013BCB4(arg0);
    }
    func_actor_503500_8013BD0C(arg0);
    func_actor_503500_8013BD88(arg0);
}

void func_actor_503500_8013BC54(Task* arg0)
{
    GpEnemy* enemy;

    enemy                                                    = arg0->spawnArg2;
    ((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)->sub = &gGfxViewCoord;
    Gp_UnlinkObj(&((Actor503500Work*)arg0->work)->obj);
    enemy->recs = 0;
    arg0->work  = NULL;
    Gp_DestroyEnemy(enemy, arg0);
}

void func_actor_503500_8013BCB4(Task* arg0)
{
    GpEnemy* enemy;

    enemy = arg0->spawnArg2;
    if (enemy->reactionFlags & 1) {
        enemy->reactionFlags &= ~1;
    }
    if (enemy->reactionFlags & 2) {
        enemy->reactionFlags &= ~2;
    }
    if (enemy->reactionFlags & 0xC) {
        enemy->reactionFlags &= ~0xC;
    }
}

void func_actor_503500_8013BD0C(Task* arg0)
{
    Actor503500Work* work;
    s16              timer;

    work = arg0->work;
    if (work->field_E8 != 0) {
        timer          = (u16)work->field_E8 - 1;
        work->field_E8 = timer;
        if (timer < 0) {
            work->field_E8 = 0;
        }
    }
    if (func_actor_503500_80136208() == 0) {
        func_actor_503500_8013AF60(arg0, work, &work->rec, 8);
    }
    Gp_ClearRec18Occupied(&work->rec);
}

void func_actor_503500_8013BD88(Task* arg0)
{
    switch (((Actor503500Work*)arg0->work)->field_ED) {
        case 0:
            func_actor_503500_8013BE0C(arg0);
            break;
        case 1:
            func_actor_503500_8013B460(arg0);
            break;
        case 2:
            func_actor_503500_8013B8D0(arg0);
            break;
    }
}

/// The 0xF0 block's counterpart of `func_actor_503500_80138454`: when a kill is
/// pending, hands the block to sub-state 1 and cancels the countdown.
void func_actor_503500_8013BE0C(Task* arg0)
{
    if (arg0->killCountdown == 2) {
        func_actor_503500_8013BE48(arg0, 1);
        arg0->killCountdown = 0;
    }
}

/// The 0xF0 block's counterpart of `func_actor_503500_80138490`: puts the block
/// into sub-state `arg1`, clears the phase and frame counter that go with it,
/// cancels a pending kill, and records the slot's halfword as "asked to die"
/// when the sub-state is non-zero.
void func_actor_503500_8013BE48(Task* arg0, s32 arg1)
{
    Actor503500Work* work = arg0->work;

    work->field_ED      = arg1;
    work->field_EE      = 0;
    work->field_EA      = 0;
    arg0->killCountdown = 0;
    func_actor_503500_80135F9C(arg0->parent, arg0->spawnArg1, arg1 != 0);
}

void func_actor_503500_8013BE8C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80131FF0;
    sp.funcs[task->state](task);
}

/// `Task::state` handlers `func_actor_503500_8013CA8C` dispatches through.
const TaskFuncTable3 D_actor_503500_80132028 = {
    {
        func_actor_503500_8013BEE4,
        func_actor_503500_8013C878,
        func_actor_503500_8013C900,
    },
};

/// State-0 init of the 0xF4 enemy at `D_actor_503500_801776A0`, the twin of
/// `func_actor_503500_8013ECBC`: clears the block, resets the task's own
/// coordinate to a plain 4096 identity, parents it to part 16 of the parent
/// task's model, links the enemy node and its display node, and starts the
/// block in sub-state 0.
void func_actor_503500_8013BEE4(Task* arg0)
{
    GpEnemy*       enemy;
    Task*          parent;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parts;
    MATRIX*        mtx;
    GpRec18*       rec;

    coord  = ((TmdObject*)arg0->extra)->coords;
    enemy  = arg0->spawnArg2;
    parent = arg0->parent;
    Mem_Set(&D_actor_503500_801776A0, 0, 0xF4);
    arg0->work = &D_actor_503500_801776A0;

    parts                        = ((TmdObject*)parent->extra)->coords;
    *(s32*)&coord->coord.m[0][0] = 0x1000;
    coord->sub                   = &parts[16];
    mtx                          = &coord->coord;
    *(s32*)&mtx->m[0][2]         = 0;
    *(s32*)&mtx->m[1][1]         = 0x1000;
    *(s32*)&mtx->m[2][0]         = 0;
    mtx->m[2][2]                 = 0x1000;
    enemy->field_4               = mtx;
    enemy->field_48              = 0;
    Gp_LinkNode(&enemy->node);
    enemy->coord      = coord;
    enemy->node.flags = (enemy->node.flags | 8) & 0xFE;
    enemy->bodyPos.vx = D_actor_503500_8016F1B0.vx;
    enemy->bodyPos.vy = D_actor_503500_8016F1B0.vy;
    enemy->bodyPos.vz = D_actor_503500_8016F1B0.vz;
    rec               = &D_actor_503500_801776A0.rec;
    enemy->param      = &D_actor_503500_8016E7EC[arg0->spawnArg1];
    enemy->recs       = rec;
    enemy->hp         = enemy->param->hpMax;

    D_actor_503500_801776A0.obj.coord    = coord;
    D_actor_503500_801776A0.obj.ctx.recs = rec;
    D_actor_503500_801776A0.obj.key      = 0x30023;
    D_actor_503500_801776A0.obj.radius   = 0x3E8;
    D_actor_503500_801776A0.obj.flags    = 1;
    D_actor_503500_801776A0.obj.pos.vx   = D_actor_503500_8016F1B0.vx;
    D_actor_503500_801776A0.obj.pos.vy   = D_actor_503500_8016F1B0.vy;
    D_actor_503500_801776A0.obj.pos.vz   = D_actor_503500_8016F1B0.vz;
    Gp_LinkObj(2, &D_actor_503500_801776A0.obj);
    Gp_InitRec18Table(rec, 8, 0);
    D_actor_503500_801776A0.field_E0.spawnArgLo = 0x600;
    D_actor_503500_801776A0.field_E0.coord      = coord;
    D_actor_503500_801776A0.field_E0.spawnArgHi = 3;
    D_actor_503500_801776A0.obj.flags          |= 0x8000;
    func_actor_503500_8013CA74(arg0, 0);
    arg0->exitCallback = func_actor_503500_8013C900;
    arg0->state       += 1;
}

/// Applies this frame's hits from the collision records `arg2[0..arg3)` to
/// the enemy - the same body as `func_actor_503500_80137C90` on this block's
/// fields: each attack id is taken once, only type-2 ids land while the
/// `field_E8` countdown is clear, and a hit that empties `field_40` starts the
/// death sub-state. The hit effect is pulled to 1400 units along the contact
/// offset. `arg1` is passed by the caller but unused.
void func_actor_503500_8013C088(Task* arg0, Actor503500Work* arg1, GpRec18* arg2, s32 arg3)
{
    VECTOR           d;
    SVECTOR          pos;
    MATRIX           mtx;
    MATRIX           rot;
    Actor503500Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   src;
    s16              stun;
    u32              id;
    s32              dmg;
    s32              crit;
    s32              scale;
    s32              i;
    s32              j;

    enemy = arg0->spawnArg2;
    work  = arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    for (i = 0; i < arg3; i++) {
        id = arg2[i].key;
        for (j = 0; j < i; j++) {
            if (arg2[j].key == id) {
                goto next;
            }
        }
        if ((id & 0xFFFF0000) == 0x10000) {
            continue;
        }
        if ((id & 0xFFFF0000) != 0x20000) {
            continue;
        }
        if (work->field_E8 != 0) {
            continue;
        }
        src = ((TmdObject*)Gp_ActorSlots[(id >> 7) & 1]->extra)->coords;
        Gp_ComposeParentWorld(coord, &mtx, &pos);
        d.vx = src->coord.t[0] - pos.vx;
        d.vy = src->coord.t[1] - pos.vy;
        d.vz = src->coord.t[2] - pos.vz;
        crit = 0;
        dmg  = Gp_ComputeDamage(id, SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz), crit, crit);
        if (Gp_RollEnemyChance(enemy, id, crit) != 0) {
            dmg *= 4;
            crit = 1;
        }
        func_800E2C78((GpObj40*)enemy, id, dmg, 0);
        func_800DA6E8(&enemy->node, dmg, 0);
        enemy->hp -= dmg;
        if (enemy->hp <= 0) {
            func_actor_503500_8013CA74(arg0, 1);
        }
        switch (Gp_GetIdParam0(id) & 0xFFFF) {
            case 0:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
                break;
            case 1:
                Gp_SetObjFlag1((GpObj4C*)enemy);
                break;
            case 2:
                Gp_SetObjFlag2((GpObj5D*)enemy, id, 0);
                break;
            case 3:
                Gp_SetObjFlag4((GpObj5C*)enemy, id, 0);
                break;
        }
        TRANSPOSE_ROT(&coord->workm, &rot);
        pos.vx = arg2[i].point.vx - coord->workm.t[0];
        pos.vy = arg2[i].point.vy - coord->workm.t[1];
        pos.vz = arg2[i].point.vz - coord->workm.t[2];
        scale  = 0x578000 / SquareRoot0(pos.vx * pos.vx + pos.vy * pos.vy + pos.vz * pos.vz);
        pos.vx = pos.vx * scale / 4096;
        pos.vy = pos.vy * scale / 4096;
        pos.vz = pos.vz * scale / 4096;
        gte_SetRotMatrix(&rot);
        gte_ldv0(&pos);
        gte_rtv0();
        gte_stsv(&pos);
        pos.vx += D_actor_503500_8016F1B0.vx;
        pos.vy += D_actor_503500_8016F1B0.vy;
        pos.vz += D_actor_503500_8016F1B0.vz;
        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &pos, &work->field_E0);
        if (crit != 0) {
            Gp_SpawnEff(0x6009C, coord, 0, &pos);
        }
        stun = Gp_GetIdParam2(id);
        if (work->field_E8 < stun) {
            work->field_E8 = stun;
        }
    next:;
    }
}

/// Death sub-state of the first 0xF4 block, stepped by `field_F1`: phase 0
/// is the death setup shared with `func_actor_503500_8013D558` and seeds the
/// shrinking scale; phase 1 spawns three effects at random offsets for up to
/// 2000 frames while the scale shrinks by an ever smaller step, and leaves
/// once that step runs out.
void func_actor_503500_8013C558(Task* arg0)
{
    Actor503500Work776A0* work;
    GsCOORDINATE2*        coord;
    SVECTOR               vec;
    s32                   pan;

    work  = (Actor503500Work776A0*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    switch (work->field_F1) {
        case 0:
            work->obj.flags                  &= 0x7FFF;
            ((GpEnemy*)arg0->spawnArg2)->recs = 0;
            Gp_UnlinkNode(&((GpEnemy*)arg0->spawnArg2)->node);
            func_actor_503500_80135CE8(arg0->parent, arg0->spawnArg1);
            work->field_E8 = 0;
            ((void (*)(s32))Gp_IncStateF0Ref)(0);
            Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
            func_actor_503500_80136048(arg0->parent);
            ((GpEnemy*)arg0->spawnArg2)->reactionFlags &= 0xF0;
            pan                                         = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(0x40230010, pan, (s8)(gpGetObjDepth(coord) / 2));
            work->field_EC = 0x1000;
            work->field_EE = 0x80;
            work->field_F1++;
            break;
        case 1:
            if (++work->field_EA <= 2000) {
                if (func_actor_503500_801360BC(arg0->spawnArg1, 5) != 0) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60055, coord, 0x01001900,
                                &D_actor_503500_8016F1B8[(u16)((Gp_LcgState >> 16) % 18)]);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60055, coord, 0x01001700,
                                &D_actor_503500_8016F1B8[(u16)((Gp_LcgState >> 16) % 18)]);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x6018C, coord, 0x01404600,
                                &D_actor_503500_8016F1B8[(u16)((Gp_LcgState >> 16) % 18)]);
                }
            }
            vec.vx = 0x1000;
            vec.vy = 0x1000;
            vec.vz = work->field_EC;
            func_actor_503500_80135E20(arg0->parent, 0x10, &vec);
            work->field_EC -= work->field_EE;
            work->field_EE -= 4;
            if (work->field_EE <= 0) {
                SndEvt_EnqueueType7(0x40230010, 0x2D);
                func_actor_503500_8013611C(arg0->spawnArg1);
                work->field_F1++;
            }
            break;
        default:
            arg0->state++;
            break;
    }
}

/// Per-frame tick of the first 0xF4 block, the same shape as
/// `func_actor_503500_8013D7D4`: frozen mode 1 skips the frame entirely,
/// mode 2 only marks the enemy's link node, and anything else clears the
/// coordinate flag and runs the normal chain.
void func_actor_503500_8013C878(Task* arg0)
{
    GpEnemy*       enemy;
    GsCOORDINATE2* coord;

    enemy = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (D_801153F4 == 1) {
        return;
    }
    if (D_801153F4 == 2) {
        enemy->node.flags |= 1;
        return;
    }
    coord->flg = 0;
    if (enemy->reactionFlags != 0) {
        func_actor_503500_8013C9DC(arg0);
    }
    func_actor_503500_8013C960(arg0);
    func_actor_503500_8013CA34(arg0);
}

void func_actor_503500_8013C900(Task* arg0)
{
    GpEnemy* enemy;

    enemy                                                    = arg0->spawnArg2;
    ((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)->sub = &gGfxViewCoord;
    Gp_UnlinkObj(&((Actor503500Work*)arg0->work)->obj);
    enemy->recs = 0;
    arg0->work  = NULL;
    Gp_DestroyEnemy(enemy, arg0);
}

void func_actor_503500_8013C960(Task* arg0)
{
    Actor503500Work* work;
    s16              timer;

    work = arg0->work;
    if (work->field_E8 != 0) {
        timer          = (u16)work->field_E8 - 1;
        work->field_E8 = timer;
        if (timer < 0) {
            work->field_E8 = 0;
        }
    }
    if (func_actor_503500_80136208() == 0) {
        func_actor_503500_8013C088(arg0, work, &work->rec, 8);
    }
    Gp_ClearRec18Occupied(&work->rec);
}

void func_actor_503500_8013C9DC(Task* arg0)
{
    GpEnemy* enemy = arg0->spawnArg2;

    if (enemy->reactionFlags & 1) {
        enemy->reactionFlags &= ~1;
    }
    if (enemy->reactionFlags & 2) {
        enemy->reactionFlags &= ~2;
    }
    if (enemy->reactionFlags & 0xC) {
        enemy->reactionFlags &= ~0xC;
    }
}

void func_actor_503500_8013CA34(Task* arg0)
{
    switch (((Actor503500Work*)arg0->work)->field_F0) {
        case 0:
            break;
        case 1:
            func_actor_503500_8013C558(arg0);
            break;
    }
}

void func_actor_503500_8013CA74(Task* arg0, s8 arg1)
{
    Actor503500Work* work;

    work           = arg0->work;
    work->field_F0 = arg1;
    work->field_F1 = 0;
    work->field_EA = 0;
}

void func_actor_503500_8013CA8C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132028;
    sp.funcs[task->state](task);
}

/// `Task::state` handlers `func_actor_503500_8013DBF4` dispatches through.
const TaskFuncTable3 D_actor_503500_80132060 = {
    {
        func_actor_503500_8013CAE4,
        func_actor_503500_8013D7D4,
        func_actor_503500_8013D85C,
    },
};

/// State-0 init of the 0xF4 enemy in slot `spawnArg1` of
/// `D_actor_503500_801770E8`, the same shape as `func_actor_503500_8013BEE4`
/// but without linking the enemy node.
void func_actor_503500_8013CAE4(Task* arg0)
{
    GpEnemy*              enemy;
    Task*                 parent;
    GsCOORDINATE2*        coord;
    MATRIX*               mtx;
    GsCOORDINATE2*        parts;
    GpRec18*              rec;
    Actor503500Work770E8* work;
    SVECTOR*              pos;
    s32                   idx;

    idx    = arg0->spawnArg1;
    enemy  = arg0->spawnArg2;
    work   = &D_actor_503500_801770E8[idx];
    pos    = &D_actor_503500_8016F210[idx];
    coord  = ((TmdObject*)arg0->extra)->coords;
    parent = arg0->parent;
    Mem_Set(work, 0, 0xF4);
    arg0->work = (Actor503500Work*)work;

    parts                        = ((TmdObject*)parent->extra)->coords;
    *(s32*)&coord->coord.m[0][0] = 0x1000;
    coord->sub                   = &parts[1];
    mtx                          = &coord->coord;
    *(s32*)&mtx->m[0][2]         = 0;
    *(s32*)&mtx->m[1][1]         = 0x1000;
    *(s32*)&mtx->m[2][0]         = 0;
    mtx->m[2][2]                 = 0x1000;
    enemy->field_4               = mtx;
    enemy->field_48              = 0;
    enemy->coord                 = coord;
    enemy->node.flags            = (enemy->node.flags | 8) & 0xFE;
    enemy->bodyPos.vx            = pos->vx;
    enemy->bodyPos.vy            = pos->vy;
    enemy->bodyPos.vz            = pos->vz;
    rec                          = work->rec;
    enemy->param                 = &D_actor_503500_8016E7EC[arg0->spawnArg1];
    enemy->recs                  = rec;
    enemy->hp                    = enemy->param->hpMax;

    work->obj.coord    = coord;
    work->obj.ctx.recs = rec;
    work->obj.pos.vx   = pos->vx;
    work->obj.pos.vy   = pos->vy;
    work->obj.pos.vz   = pos->vz;
    work->obj.key      = 0x30023;
    work->obj.radius   = 0x190;
    work->obj.flags    = 1;
    Gp_LinkObj(2, &work->obj);
    Gp_InitRec18Table(rec, 8, 0);
    work->field_E0.spawnArgLo = 0x600;
    work->field_E0.coord      = coord;
    work->field_E0.spawnArgHi = 3;
    work->obj.flags          &= 0x7FFF;
    func_actor_503500_8013DBA8(arg0, 0);
    arg0->exitCallback = func_actor_503500_8013D85C;
    arg0->state       += 1;
}

/// Applies this frame's hits from the collision records `arg2[0..arg3)` to
/// this form - the same pass as `func_actor_503500_8013C088`: each attack id
/// is taken once, only type-2 ids land while the `field_E8` countdown is
/// clear, and a hit that empties `field_40` starts death sub-state 3. The hit
/// effect is pulled to 200 units along the contact offset and shifted by this
/// slot's `D_actor_503500_8016F210` entry. `arg1` is passed but unused.
void func_actor_503500_8013CCBC(Task* arg0, Actor503500Work* arg1, GpRec18* arg2, s32 arg3)
{
    VECTOR           d;
    SVECTOR          pos;
    MATRIX           mtx;
    MATRIX           rot;
    Actor503500Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   src;
    s16              stun;
    u32              id;
    s32              dmg;
    s32              crit;
    s32              scale;
    s32              i;
    s32              j;

    enemy = arg0->spawnArg2;
    work  = arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    for (i = 0; i < arg3; i++) {
        id = arg2[i].key;
        for (j = 0; j < i; j++) {
            if (arg2[j].key == id) {
                goto next;
            }
        }
        if ((id & 0xFFFF0000) == 0x10000) {
            continue;
        }
        if ((id & 0xFFFF0000) != 0x20000) {
            continue;
        }
        if (work->field_E8 != 0) {
            continue;
        }
        src = ((TmdObject*)Gp_ActorSlots[(id >> 7) & 1]->extra)->coords;
        Gp_ComposeParentWorld(coord, &mtx, &pos);
        d.vx = src->coord.t[0] - pos.vx;
        d.vy = src->coord.t[1] - pos.vy;
        d.vz = src->coord.t[2] - pos.vz;
        crit = 0;
        dmg  = Gp_ComputeDamage(id, SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz), crit, crit);
        if (Gp_RollEnemyChance(enemy, id, crit) != 0) {
            dmg *= 4;
            crit = 1;
        }
        func_800E2C78((GpObj40*)enemy, id, dmg, 0);
        func_800DA6E8(&enemy->node, dmg, 0);
        enemy->hp -= dmg;
        if (enemy->hp <= 0) {
            func_actor_503500_8013DBA8(arg0, 3);
        }
        switch (Gp_GetIdParam0(id) & 0xFFFF) {
            case 0:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
                break;
            case 1:
                Gp_SetObjFlag1((GpObj4C*)enemy);
                break;
            case 2:
                Gp_SetObjFlag2((GpObj5D*)enemy, id, 0);
                break;
            case 3:
                Gp_SetObjFlag4((GpObj5C*)enemy, id, 0);
                break;
        }
        TRANSPOSE_ROT(&coord->workm, &rot);
        pos.vx = arg2[i].point.vx - coord->workm.t[0];
        pos.vy = arg2[i].point.vy - coord->workm.t[1];
        pos.vz = arg2[i].point.vz - coord->workm.t[2];
        scale  = 0xC8000 / SquareRoot0(pos.vx * pos.vx + pos.vy * pos.vy + pos.vz * pos.vz);
        pos.vx = pos.vx * scale / 4096;
        pos.vy = pos.vy * scale / 4096;
        pos.vz = pos.vz * scale / 4096;
        gte_SetRotMatrix(&rot);
        gte_ldv0(&pos);
        gte_rtv0();
        gte_stsv(&pos);
        pos.vx += D_actor_503500_8016F210[arg0->spawnArg1].vx;
        pos.vy += D_actor_503500_8016F210[arg0->spawnArg1].vy;
        pos.vz += D_actor_503500_8016F210[arg0->spawnArg1].vz;
        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &pos, &work->field_E0);
        if (crit != 0) {
            Gp_SpawnEff(0x6009C, coord, 0, &pos);
        }
        stun = Gp_GetIdParam2(id);
        if (work->field_E8 < stun) {
            work->field_E8 = stun;
        }
    next:;
    }
}

/// Sub-state 2 of the second 0xF4 block, stepped by `field_F1`: phase 0
/// applies the boss's preset 0xA; from frame 0x14 phase 1 spawns one child
/// from `D_actor_503500_8016E9F0`, placed at the offset
/// `D_actor_503500_8016F258` in the actor's frame and turned by
/// `D_actor_503500_8016F260` (both mirrored for slot 8); phase 2 hands off
/// once `func_actor_503500_80136014` reports preset 0xA flagged.
void func_actor_503500_8013D1CC(Task* arg0)
{
    SVECTOR               pos;
    SVECTOR               ofs;
    MATRIX                m;
    Actor503500Work770E8* work;
    GsCOORDINATE2*        coord;
    GsCOORDINATE2*        dst;
    Task*                 task;
    s32*                  src;
    s32*                  out;
    s32                   i;
    s32                   t;

    work  = (Actor503500Work770E8*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (func_actor_503500_8013608C(arg0->parent) != 0) {
        func_actor_503500_8013DBA8(arg0, 1);
        func_actor_503500_8013611C(arg0->spawnArg1);
        return;
    }
    switch (work->field_F1) {
        case 0:
            func_actor_503500_80135FB4(arg0->parent, 0xA, 0x10);
            work->field_F1++;
            break;
        case 1:
            if ((s16)++work->field_EC < 0x14) {
                break;
            }
            task = Task_SpawnFromTable(&D_actor_503500_8016E9F0, 1, 1, 0xC00000);
            if (task != NULL) {
                Gp_ComposeParentWorld(coord, &m, &pos);
                dst    = ((TmdObject*)task->extra)->coords;
                ofs.vx = D_actor_503500_8016F258.vx;
                ofs.vy = D_actor_503500_8016F258.vy;
                ofs.vz = D_actor_503500_8016F258.vz;
                if (arg0->spawnArg1 == 8) {
                    t      = ofs.vx;
                    ofs.vx = -t;
                }
                gte_SetRotMatrix(&m);
                gte_ldv0(&ofs);
                gte_rtv0();
                gte_stsv(&ofs);
                dst->coord.t[0] = pos.vx + ofs.vx;
                dst->coord.t[1] = pos.vy + ofs.vy;
                dst->coord.t[2] = pos.vz + ofs.vz;
                out             = (s32*)&dst->coord;
                src             = (s32*)&m;
                for (i = 0; i < 4; i++) {
                    *out++ = *src++;
                }
                dst->coord.m[2][2] = m.m[2][2];
                pos.vx             = D_actor_503500_8016F260.vx;
                pos.vy             = D_actor_503500_8016F260.vy;
                pos.vz             = D_actor_503500_8016F260.vz;
                if (arg0->spawnArg1 == 8) {
                    t      = pos.vy;
                    pos.vy = -t;
                }
                RotMatrixZYX(&pos, &m);
                gte_SetRotMatrix(&dst->coord);
                gte_ldclmv(&m);
                gte_rtir();
                gte_stclmv(&dst->coord);
                gte_ldclmv((char*)&m + 2);
                gte_rtir();
                gte_stclmv((char*)&dst->coord + 2);
                gte_ldclmv((char*)&m + 4);
                gte_rtir();
                gte_stclmv((char*)&dst->coord + 4);
            }
            work->field_F1++;
            break;
        case 2:
            if (func_actor_503500_80136014(arg0->parent, 0xA) != 0) {
                func_actor_503500_8013DBA8(arg0, 1);
            }
            break;
    }
}

/// Sub-state 3 of the second 0xF4 block, stepped by `field_F1`: phase 0 is
/// the death setup shared with `func_actor_503500_8013F4A4`; phase 1 spawns an
/// effect per frame from a slot-dependent offset table for 6 frames, then a
/// final burst of three; the last phase keeps counting to 0x1F and leaves.
void func_actor_503500_8013D558(Task* arg0)
{
    Actor503500Work770E8* work;
    GsCOORDINATE2*        coord;
    SVECTOR*              vec;
    s32                   pan;

    work  = (Actor503500Work770E8*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    switch (work->field_F1) {
        case 0:
            work->obj.flags                  &= 0x7FFF;
            ((GpEnemy*)arg0->spawnArg2)->recs = 0;
            Gp_UnlinkNode(&((GpEnemy*)arg0->spawnArg2)->node);
            func_actor_503500_80135CE8(arg0->parent, arg0->spawnArg1);
            work->field_E8 = 0;
            ((void (*)(s32))Gp_IncStateF0Ref)(0);
            Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
            func_actor_503500_80136048(arg0->parent);
            ((GpEnemy*)arg0->spawnArg2)->reactionFlags &= 0xF0;
            pan                                         = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(0x40230010, pan, (s8)(gpGetObjDepth(coord) / 2));
            work->field_F1++;
            break;
        case 1:
            if (arg0->spawnArg1 == 7) {
                vec = D_actor_503500_8016F278;
            } else {
                vec = D_actor_503500_8016F290;
            }
            if (func_actor_503500_801360BC(arg0->spawnArg1, 2) != 0) {
                Gp_SpawnEff(0x60055, coord, 0x01001C00, &vec[(s16)((s16)work->field_EC % 3)]);
            }
            if ((s16)work->field_EC++ >= 5) {
                if (func_actor_503500_801360BC(arg0->spawnArg1, 6) != 0) {
                    Gp_SpawnEff(0x6018C, coord, 0x04404600, &vec[0]);
                    Gp_SpawnEff(0x6018C, coord, 0x05404600, &vec[1]);
                    Gp_SpawnEff(0x6018C, coord, 0x06404600, &vec[2]);
                }
                SndEvt_EnqueueType7(0x40230010, 0x2D);
                work->field_F1++;
            }
            break;
        default:
            work->field_EC++;
            if ((s16)work->field_EC >= 0x1F) {
                func_actor_503500_8013611C(arg0->spawnArg1);
                arg0->state++;
            }
            break;
    }
}

/// Per-frame tick of the second 0xF4 block, the same shape as
/// `func_actor_503500_8013BBCC`: frozen mode 1 skips the frame entirely,
/// mode 2 only marks the enemy's link node, and anything else clears the
/// coordinate flag and runs the normal chain.
void func_actor_503500_8013D7D4(Task* arg0)
{
    GpEnemy*       enemy;
    GsCOORDINATE2* coord;

    enemy = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (D_801153F4 == 1) {
        return;
    }
    if (D_801153F4 == 2) {
        enemy->node.flags |= 1;
        return;
    }
    coord->flg = 0;
    if (enemy->reactionFlags != 0) {
        func_actor_503500_8013D8BC(arg0);
    }
    func_actor_503500_8013D914(arg0);
    func_actor_503500_8013D990(arg0);
}

void func_actor_503500_8013D85C(Task* arg0)
{
    GpEnemy* enemy;

    enemy                                                    = arg0->spawnArg2;
    ((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)->sub = &gGfxViewCoord;
    Gp_UnlinkObj(&((Actor503500Work*)arg0->work)->obj);
    enemy->recs = 0;
    arg0->work  = NULL;
    Gp_DestroyEnemy(enemy, arg0);
}

void func_actor_503500_8013D8BC(Task* arg0)
{
    GpEnemy* enemy = arg0->spawnArg2;

    if (enemy->reactionFlags & 1) {
        enemy->reactionFlags &= ~1;
    }
    if (enemy->reactionFlags & 2) {
        enemy->reactionFlags &= ~2;
    }
    if (enemy->reactionFlags & 0xC) {
        enemy->reactionFlags &= ~0xC;
    }
}

void func_actor_503500_8013D914(Task* arg0)
{
    Actor503500Work* work;
    s16              timer;

    work = arg0->work;
    if (work->field_E8 != 0) {
        timer          = (u16)work->field_E8 - 1;
        work->field_E8 = timer;
        if (timer < 0) {
            work->field_E8 = 0;
        }
    }
    if (func_actor_503500_80136208() == 0) {
        func_actor_503500_8013CCBC(arg0, work, &work->rec, 8);
    }
    Gp_ClearRec18Occupied(&work->rec);
}

void func_actor_503500_8013D990(Task* arg0)
{
    s8 temp_v1;

    temp_v1 = ((Actor503500Work*)arg0->work)->field_F0;
    switch (temp_v1) {
        case 0:
            func_actor_503500_8013DC4C(arg0);
            break;
        case 1:
            func_actor_503500_8013DA2C(arg0, 0x258);
            break;
        case 2:
            func_actor_503500_8013D1CC(arg0);
            break;
        case 3:
            func_actor_503500_8013D558(arg0);
            break;
    }
}

/// Sub-state 1 of the second 0xF4 block: counts `field_EA` up to `arg1`
/// frames, then - if slot `kind` is free - respawns the two slot enemies,
/// each primed to die in 9 frames with a fifth of this enemy's HP (at least
/// 1), and drops back to sub-state 0 with the node unlinked. A pending
/// kill (`killCountdown == 2`) moves it to sub-state 2 instead.
void func_actor_503500_8013DA2C(Task* arg0, s32 arg1)
{
    Actor503500Work* work;
    GpEnemy*         enemy;
    GpEnemy*         child;
    s32              kind;
    s32              slotA;
    s32              slotB;
    s32              hp;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    if (arg1 < ++work->field_EA) {
        kind = 3;
        if (arg0->spawnArg1 == 7) {
            kind  = 2;
            slotA = 0xD;
            slotB = 0xE;
        } else {
            slotA = 0xF;
            slotB = 0x10;
        }
        if (func_actor_503500_80135E04(arg0->parent, kind) != 0) {
            child = func_actor_503500_80135D00(arg0->parent, slotA);
            hp    = (s16)(enemy->hp / 5);
            if (hp <= 0) {
                hp = 1;
            }
            if (child != NULL) {
                child->task->killCountdown = 9;
                child->hp                  = hp;
            }
            child = func_actor_503500_80135D00(arg0->parent, slotB);
            if (child != NULL) {
                child->task->killCountdown = 9;
                child->hp                  = hp;
            }
            work->field_EA = 0;
        }
        func_actor_503500_8013DBA8(arg0, 0);
        work->obj.flags &= 0x7FFF;
        Gp_UnlinkNode(&enemy->node);
        work->field_E8 = 0;
        return;
    }
    if (arg0->killCountdown == 2) {
        func_actor_503500_8013DBA8(arg0, 2);
        arg0->killCountdown = 0;
    }
}

/// The second 0xF4 block's counterpart of `func_actor_503500_8013BE48`: puts
/// the block into sub-state `arg1` (the one `func_actor_503500_8013D990`
/// dispatches on), clears the phase and the two counters that go with it,
/// cancels a pending kill, and records the slot's halfword as "asked to die"
/// for every sub-state but 1.
void func_actor_503500_8013DBA8(Task* arg0, s32 arg1)
{
    Actor503500Work770E8* work = (Actor503500Work770E8*)arg0->work;

    work->field_F0      = arg1;
    work->field_F1      = 0;
    work->field_EC      = 0;
    work->field_EE      = 0;
    arg0->killCountdown = 0;
    func_actor_503500_80135F9C(arg0->parent, arg0->spawnArg1, arg1 != 1);
}

void func_actor_503500_8013DBF4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132060;
    sp.funcs[task->state](task);
}

void func_actor_503500_8013DC4C(Task* arg0)
{
    Actor503500Work* work;
    s32              kind;
    s32              slotA;
    s32              slotB;

    kind = 3;
    if (arg0->spawnArg1 == 7) {
        kind  = 2;
        slotA = 0xD;
        slotB = 0xE;
    } else {
        slotA = 0xF;
        slotB = 0x10;
    }
    if ((func_actor_503500_80135E04(arg0->parent, kind) != 0) &&
        (func_actor_503500_80135E04(arg0->parent, slotA) != 0) &&
        (func_actor_503500_80135E04(arg0->parent, slotB) != 0)) {
        func_actor_503500_8013DBA8(arg0, 1);
        work             = arg0->work;
        work->obj.flags |= 0x8000;
        Gp_LinkNode(&((GpEnemy*)arg0->spawnArg2)->node);
    }
}

/// `Task::state` handlers `func_actor_503500_8013EC64` dispatches through.
const TaskFuncTable3 D_actor_503500_80132098 = {
    {
        func_actor_503500_8013DD10,
        func_actor_503500_8013E9A4,
        func_actor_503500_8013EA2C,
    },
};

/// State-0 init of the 0xF0 enemy at `D_actor_503500_8017797C`, the same shape
/// as `func_actor_503500_8013BEE4`: clears the block, resets the task's own
/// coordinate to a plain 4096 identity, parents it to part 1 of the parent
/// task's model, links the enemy node and its display node, and starts the
/// block in sub-state 0.
void func_actor_503500_8013DD10(Task* arg0)
{
    GpEnemy*       enemy;
    Task*          parent;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parts;
    MATRIX*        mtx;
    GpRec18*       rec;

    coord  = ((TmdObject*)arg0->extra)->coords;
    enemy  = arg0->spawnArg2;
    parent = arg0->parent;
    Mem_Set(&D_actor_503500_8017797C, 0, 0xF0);
    arg0->work = &D_actor_503500_8017797C;

    parts                        = ((TmdObject*)parent->extra)->coords;
    *(s32*)&coord->coord.m[0][0] = 0x1000;
    coord->sub                   = &parts[1];
    mtx                          = &coord->coord;
    *(s32*)&mtx->m[0][2]         = 0;
    *(s32*)&mtx->m[1][1]         = 0x1000;
    *(s32*)&mtx->m[2][0]         = 0;
    mtx->m[2][2]                 = 0x1000;
    enemy->field_4               = mtx;
    enemy->field_48              = 0;
    Gp_LinkNode(&enemy->node);
    enemy->coord      = coord;
    enemy->node.flags = (enemy->node.flags | 8) & 0xFE;
    enemy->bodyPos.vx = D_actor_503500_8016F2D8.vx;
    enemy->bodyPos.vy = D_actor_503500_8016F2D8.vy;
    enemy->bodyPos.vz = D_actor_503500_8016F2D8.vz;
    rec               = &D_actor_503500_8017797C.rec;
    enemy->param      = &D_actor_503500_8016E7EC[arg0->spawnArg1];
    enemy->recs       = rec;
    enemy->hp         = enemy->param->hpMax;

    D_actor_503500_8017797C.obj.coord    = coord;
    D_actor_503500_8017797C.obj.ctx.recs = rec;
    D_actor_503500_8017797C.obj.key      = 0x30023;
    D_actor_503500_8017797C.obj.radius   = 0x3E8;
    D_actor_503500_8017797C.obj.flags    = 1;
    D_actor_503500_8017797C.obj.pos.vx   = D_actor_503500_8016F2D8.vx;
    D_actor_503500_8017797C.obj.pos.vy   = D_actor_503500_8016F2D8.vy;
    D_actor_503500_8017797C.obj.pos.vz   = D_actor_503500_8016F2D8.vz;
    Gp_LinkObj(2, &D_actor_503500_8017797C.obj);
    Gp_InitRec18Table(rec, 8, 0);
    D_actor_503500_8017797C.field_E0.spawnArgLo = 0x600;
    D_actor_503500_8017797C.field_E0.coord      = coord;
    D_actor_503500_8017797C.field_E0.spawnArgHi = 3;
    D_actor_503500_8017797C.obj.flags          |= 0x8000;
    func_actor_503500_8013EC20(arg0, 0);
    arg0->exitCallback = func_actor_503500_8013EA2C;
    arg0->state       += 1;
}

/// Applies this frame's hits from the collision records `arg2[0..arg3)`, the
/// same pass as `func_actor_503500_80137C90` for this form: each attack id is
/// taken once, only type-2 ids land while the `field_E8` countdown is clear,
/// and a hit that empties `field_40` starts state 2. The hit effect is pulled
/// to 600 units along the contact offset. `arg1` is passed but unused.
void func_actor_503500_8013DEB4(Task* arg0, Actor503500Work* arg1, GpRec18* arg2, s32 arg3)
{
    VECTOR           d;
    SVECTOR          pos;
    MATRIX           mtx;
    MATRIX           rot;
    Actor503500Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   src;
    s16              stun;
    u32              id;
    s32              dmg;
    s32              crit;
    s32              scale;
    s32              i;
    s32              j;

    enemy = arg0->spawnArg2;
    work  = arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    for (i = 0; i < arg3; i++) {
        id = arg2[i].key;
        for (j = 0; j < i; j++) {
            if (arg2[j].key == id) {
                goto next;
            }
        }
        if ((id & 0xFFFF0000) == 0x10000) {
            continue;
        }
        if ((id & 0xFFFF0000) != 0x20000) {
            continue;
        }
        if (work->field_E8 != 0) {
            continue;
        }
        src = ((TmdObject*)Gp_ActorSlots[(id >> 7) & 1]->extra)->coords;
        Gp_ComposeParentWorld(coord, &mtx, &pos);
        d.vx = src->coord.t[0] - pos.vx;
        d.vy = src->coord.t[1] - pos.vy;
        d.vz = src->coord.t[2] - pos.vz;
        crit = 0;
        dmg  = Gp_ComputeDamage(id, SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz), crit, crit);
        if (Gp_RollEnemyChance(enemy, id, crit) != 0) {
            dmg *= 4;
            crit = 1;
        }
        func_800E2C78((GpObj40*)enemy, id, dmg, 0);
        func_800DA6E8(&enemy->node, dmg, 0);
        enemy->hp -= dmg;
        if (enemy->hp <= 0) {
            func_actor_503500_8013EC20(arg0, 2);
        }
        switch (Gp_GetIdParam0(id) & 0xFFFF) {
            case 0:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
                break;
            case 1:
                Gp_SetObjFlag1((GpObj4C*)enemy);
                break;
            case 2:
                Gp_SetObjFlag2((GpObj5D*)enemy, id, 0);
                break;
            case 3:
                Gp_SetObjFlag4((GpObj5C*)enemy, id, 0);
                break;
        }
        TRANSPOSE_ROT(&coord->workm, &rot);
        pos.vx = arg2[i].point.vx - coord->workm.t[0];
        pos.vy = arg2[i].point.vy - coord->workm.t[1];
        pos.vz = arg2[i].point.vz - coord->workm.t[2];
        scale  = 0x258000 / SquareRoot0(pos.vx * pos.vx + pos.vy * pos.vy + pos.vz * pos.vz);
        pos.vx = pos.vx * scale / 4096;
        pos.vy = pos.vy * scale / 4096;
        pos.vz = pos.vz * scale / 4096;
        gte_SetRotMatrix(&rot);
        gte_ldv0(&pos);
        gte_rtv0();
        gte_stsv(&pos);
        pos.vx += D_actor_503500_8016F2D8.vx;
        pos.vy += D_actor_503500_8016F2D8.vy;
        pos.vz += D_actor_503500_8016F2D8.vz;
        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &pos, &work->field_E0);
        if (crit != 0) {
            Gp_SpawnEff(0x6009C, coord, 0, &pos);
        }
        stun = Gp_GetIdParam2(id);
        if (work->field_E8 < stun) {
            work->field_E8 = stun;
        }
    next:;
    }
}

/// Sub-state 1 of the third 0xF0 block, stepped by `field_ED`: step 0 applies
/// the boss's preset 0xB; from frame 0x42 step 1 spawns six children from
/// `D_actor_503500_8016E9F0`, one a frame, each placed at a fixed offset in the
/// actor's frame and turned by its row of `D_actor_503500_8016F2EC`; step 3
/// hands off once `func_actor_503500_80136014` reports preset 0xB flagged.
void func_actor_503500_8013E384(Task* arg0)
{
    SVECTOR          pos;
    SVECTOR          ofs;
    MATRIX           m;
    Actor503500Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   dst;
    Task*            task;
    s32*             src;
    s32*             out;
    s32              i;
    s32              arg;
    s16              idx;

    work  = arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (func_actor_503500_8013608C(arg0->parent) != 0) {
        func_actor_503500_8013EC20(arg0, 0);
        func_actor_503500_8013611C(arg0->spawnArg1);
        return;
    }
    switch (work->field_ED) {
        case 0:
            func_actor_503500_80135FB4(arg0->parent, 0xB, 0x10);
            work->field_ED++;
            break;
        case 1:
            if (++work->field_EA < 0x42) {
                break;
            }
        case 2:
            idx  = work->field_EA - 0x42;
            arg  = (D_actor_503500_8016F2E0[idx] << 12) + (-D_80073B8C->t[1] << 24) / 1000;
            task = Task_SpawnFromTable(&D_actor_503500_8016E9F0, 0, 1, arg);
            if (task != NULL) {
                Gp_ComposeParentWorld(coord, &m, &pos);
                dst    = ((TmdObject*)task->extra)->coords;
                ofs.vy = 0x190;
                ofs.vx = 0;
                ofs.vz = 0x960;
                gte_SetRotMatrix(&m);
                gte_ldv0(&ofs);
                gte_rtv0();
                gte_stsv(&ofs);
                dst->coord.t[0] = pos.vx + ofs.vx;
                dst->coord.t[1] = pos.vy + ofs.vy;
                dst->coord.t[2] = pos.vz + ofs.vz;
                src             = (s32*)&m;
                out             = (s32*)&dst->coord;
                for (i = 0; i < 4; i++) {
                    *out++ = *src++;
                }
                dst->coord.m[2][2] = m.m[2][2];
                RotMatrix(&D_actor_503500_8016F2EC[idx], &m);
                gte_SetRotMatrix(&dst->coord);
                gte_ldclmv(&m);
                gte_rtir();
                gte_stclmv(&dst->coord);
                gte_ldclmv((char*)&m + 2);
                gte_rtir();
                gte_stclmv((char*)&dst->coord + 2);
                gte_ldclmv((char*)&m + 4);
                gte_rtir();
                gte_stclmv((char*)&dst->coord + 4);
            }
            if (idx >= 5) {
                work->field_ED += 2;
            }
            break;
        case 3:
            if (func_actor_503500_80136014(arg0->parent, 0xB) != 0) {
                func_actor_503500_8013EC20(arg0, 0);
            }
            break;
    }
}

/// Sub-state 2 of the third 0xF0 block, stepped by `field_ED`: the same
/// death sequence as `func_actor_503500_8013F4A4`, except the effects come
/// from `D_actor_503500_8016F31C` - the odd-frame one at a random row - and
/// the VRAM rect is restored on frame 8 rather than at the end.
void func_actor_503500_8013E740(Task* arg0)
{
    GpEnemy*         enemy;
    Actor503500Work* work;
    GsCOORDINATE2*   coord;
    s32              pan;

    enemy = arg0->spawnArg2;
    work  = arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    switch (work->field_ED) {
        case 0:
            work->obj.flags &= 0x7FFF;
            enemy->recs      = 0;
            Gp_UnlinkNode(&enemy->node);
            func_actor_503500_80135CE8(arg0->parent, arg0->spawnArg1);
            work->field_E8 = 0;
            ((void (*)(s32))Gp_IncStateF0Ref)(0);
            Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
            func_actor_503500_80136048(arg0->parent);
            enemy->reactionFlags &= 0xF0;
            pan                   = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(0x40230010, pan, (s8)(gpGetObjDepth(coord) / 2));
            work->field_ED++;
            break;
        case 1:
            if (func_actor_503500_801360BC(arg0->spawnArg1, 3) != 0) {
                Gp_SpawnEff(0x60055, coord, 0x01001C00,
                            &D_actor_503500_8016F31C[(s16)(work->field_EA % 9)]);
                if (work->field_EA & 1) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x6018C, coord, 0x04404600,
                                &D_actor_503500_8016F31C[(u16)((Gp_LcgState >> 16) % 9)]);
                }
            }
            if (work->field_EA == 8) {
                MoveImage(&D_actor_503500_8016F364, 0x141, 0x12A);
            }
            if (work->field_EA++ >= 0x1F) {
                func_actor_503500_8013611C(arg0->spawnArg1);
                SndEvt_EnqueueType7(0x40230010, 0x2D);
                work->field_ED++;
            }
            break;
        default:
            arg0->state++;
            break;
    }
}

/// The third 0xF0 block's per-frame tick, the same shape as
/// `func_actor_503500_8013D7D4`: frozen mode 1 skips the frame entirely,
/// mode 2 only marks the enemy's link node, and anything else clears the
/// coordinate flag and runs the normal chain.
void func_actor_503500_8013E9A4(Task* arg0)
{
    GpEnemy*       enemy;
    GsCOORDINATE2* coord;

    enemy = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (D_801153F4 == 1) {
        return;
    }
    if (D_801153F4 == 2) {
        enemy->node.flags |= 1;
        return;
    }
    coord->flg = 0;
    if (enemy->reactionFlags != 0) {
        func_actor_503500_8013EA8C(arg0);
    }
    func_actor_503500_8013EAE4(arg0);
    func_actor_503500_8013EB60(arg0);
}

void func_actor_503500_8013EA2C(Task* arg0)
{
    GpEnemy* enemy;

    enemy                                                    = arg0->spawnArg2;
    ((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)->sub = &gGfxViewCoord;
    Gp_UnlinkObj(&((Actor503500Work*)arg0->work)->obj);
    enemy->recs = 0;
    arg0->work  = NULL;
    Gp_DestroyEnemy(enemy, arg0);
}

void func_actor_503500_8013EA8C(Task* arg0)
{
    GpEnemy* enemy;

    enemy = arg0->spawnArg2;
    if (enemy->reactionFlags & 1) {
        enemy->reactionFlags &= ~1;
    }
    if (enemy->reactionFlags & 2) {
        enemy->reactionFlags &= ~2;
    }
    if (enemy->reactionFlags & 0xC) {
        enemy->reactionFlags &= ~0xC;
    }
}

void func_actor_503500_8013EAE4(Task* arg0)
{
    Actor503500Work* work;
    s16              timer;

    work = arg0->work;
    if (work->field_E8 != 0) {
        timer          = (u16)work->field_E8 - 1;
        work->field_E8 = timer;
        if (timer < 0) {
            work->field_E8 = 0;
        }
    }
    if (func_actor_503500_80136208() == 0) {
        func_actor_503500_8013DEB4(arg0, work, &work->rec, 8);
    }
    Gp_ClearRec18Occupied(&work->rec);
}

void func_actor_503500_8013EB60(Task* arg0)
{
    switch (((Actor503500Work*)arg0->work)->field_EC) {
        case 0:
            func_actor_503500_8013EBE4(arg0);
            break;
        case 1:
            func_actor_503500_8013E384(arg0);
            break;
        case 2:
            func_actor_503500_8013E740(arg0);
            break;
    }
}

/// The third 0xF0 block's counterpart of `func_actor_503500_80138454`: when a
/// kill is pending, hands the block to sub-state 1 and cancels the countdown.
void func_actor_503500_8013EBE4(Task* arg0)
{
    if (arg0->killCountdown == 2) {
        func_actor_503500_8013EC20(arg0, 1);
        arg0->killCountdown = 0;
    }
}

/// The third 0xF0 block's counterpart of `func_actor_503500_8013BE48`: puts the
/// block into sub-state `arg1` (the one `func_actor_503500_8013EB60`
/// dispatches on), clears the phase and frame counter that go with it, cancels
/// a pending kill, and records the slot's halfword as "asked to die" when the
/// sub-state is non-zero.
void func_actor_503500_8013EC20(Task* arg0, s32 arg1)
{
    Actor503500Work* work = arg0->work;

    work->field_EC      = arg1;
    work->field_ED      = 0;
    work->field_EA      = 0;
    arg0->killCountdown = 0;
    func_actor_503500_80135F9C(arg0->parent, arg0->spawnArg1, arg1 != 0);
}

void func_actor_503500_8013EC64(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132098;
    sp.funcs[task->state](task);
}

/// `Task::state` handlers `func_actor_503500_8013FA1C` dispatches through.
const TaskFuncTable3 D_actor_503500_801320D0 = {
    {
        func_actor_503500_8013ECBC,
        func_actor_503500_8013F6F0,
        func_actor_503500_8013F778,
    },
};

/// State-0 init of the 0xF4 enemy at `D_actor_503500_80177A6C`: clears the
/// block, resets the task's own coordinate to a plain 4096 identity, parents
/// it to part 8 of the parent task's model, links the enemy node and its
/// display node, and hands the block to sub-state 3.
void func_actor_503500_8013ECBC(Task* arg0)
{
    GpEnemy*       enemy;
    Task*          parent;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parts;
    MATRIX*        mtx;
    GpRec18*       rec;

    coord  = ((TmdObject*)arg0->extra)->coords;
    enemy  = arg0->spawnArg2;
    parent = arg0->parent;
    Mem_Set(&D_actor_503500_80177A6C, 0, 0xF4);
    arg0->work = &D_actor_503500_80177A6C;

    parts                        = ((TmdObject*)parent->extra)->coords;
    *(s32*)&coord->coord.m[0][0] = 0x1000;
    coord->sub                   = &parts[8];
    mtx                          = &coord->coord;
    *(s32*)&mtx->m[0][2]         = 0;
    *(s32*)&mtx->m[1][1]         = 0x1000;
    *(s32*)&mtx->m[2][0]         = 0;
    mtx->m[2][2]                 = 0x1000;
    enemy->field_4               = mtx;
    enemy->field_48              = 0;
    Gp_LinkNode(&enemy->node);
    enemy->coord       = coord;
    enemy->node.flags |= 9;
    enemy->bodyPos.vx  = D_actor_503500_8016F36C.vx;
    enemy->bodyPos.vy  = D_actor_503500_8016F36C.vy;
    enemy->bodyPos.vz  = D_actor_503500_8016F36C.vz;
    rec                = &D_actor_503500_80177A6C.rec;
    enemy->param       = &D_actor_503500_8016E7EC[arg0->spawnArg1];
    enemy->recs        = rec;
    enemy->hp          = enemy->param->hpMax;

    D_actor_503500_80177A6C.obj.coord    = coord;
    D_actor_503500_80177A6C.obj.ctx.recs = rec;
    D_actor_503500_80177A6C.obj.pos.vx   = D_actor_503500_8016F36C.vx;
    D_actor_503500_80177A6C.obj.pos.vy   = D_actor_503500_8016F36C.vy;
    D_actor_503500_80177A6C.obj.pos.vz   = D_actor_503500_8016F36C.vz;
    D_actor_503500_80177A6C.obj.key      = 0x30023;
    D_actor_503500_80177A6C.obj.radius   = 0x320;
    D_actor_503500_80177A6C.obj.flags    = 1;
    Gp_LinkObj(2, &D_actor_503500_80177A6C.obj);
    Gp_InitRec18Table(rec, 8, 0);
    D_actor_503500_80177A6C.field_E0.spawnArgLo = 0x600;
    D_actor_503500_80177A6C.field_E0.coord      = coord;
    D_actor_503500_80177A6C.field_E0.spawnArgHi = 3;
    D_actor_503500_80177A6C.obj.flags          &= 0x7FFF;
    func_actor_503500_8013F9D4(arg0, 3);
    arg0->exitCallback = func_actor_503500_8013F778;
    arg0->state       += 1;
}

/// Applies this frame's hits from the collision records `arg2[0..arg3)` to
/// the 0xF4 block's enemy, like `func_actor_503500_80139A20`: each attack id is
/// taken once, only type-2 ids land while the `field_E8` countdown is clear,
/// and a hit that empties `field_40` starts sub-state 2 but still applies the
/// id's status effect. The hit effect is pulled to 400 units along the contact
/// offset. `arg1` is passed by the caller but unused.
void func_actor_503500_8013EE5C(Task* arg0, Actor503500Work* arg1, GpRec18* arg2, s32 arg3)
{
    VECTOR           d;
    SVECTOR          pos;
    MATRIX           mtx;
    MATRIX           rot;
    Actor503500Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   src;
    s16              stun;
    u32              id;
    s32              dmg;
    s32              crit;
    s32              scale;
    s32              i;
    s32              j;

    enemy = arg0->spawnArg2;
    work  = arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    for (i = 0; i < arg3; i++) {
        id = arg2[i].key;
        for (j = 0; j < i; j++) {
            if (arg2[j].key == id) {
                goto next;
            }
        }
        if ((id & 0xFFFF0000) == 0x10000) {
            continue;
        }
        if ((id & 0xFFFF0000) != 0x20000) {
            continue;
        }
        if (work->field_E8 != 0) {
            continue;
        }
        src = ((TmdObject*)Gp_ActorSlots[(id >> 7) & 1]->extra)->coords;
        Gp_ComposeParentWorld(coord, &mtx, &pos);
        d.vx = src->coord.t[0] - pos.vx;
        d.vy = src->coord.t[1] - pos.vy;
        d.vz = src->coord.t[2] - pos.vz;
        crit = 0;
        dmg  = Gp_ComputeDamage(id, SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz), crit, crit);
        if (Gp_RollEnemyChance(enemy, id, crit) != 0) {
            dmg *= 4;
            crit = 1;
        }
        func_800E2C78((GpObj40*)enemy, id, dmg, 0);
        func_800DA6E8(&enemy->node, dmg, 0);
        enemy->hp -= dmg;
        if (enemy->hp <= 0) {
            func_actor_503500_8013F9D4(arg0, 2);
        }
        switch (Gp_GetIdParam0(id) & 0xFFFF) {
            case 0:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
                break;
            case 1:
                Gp_SetObjFlag1((GpObj4C*)enemy);
                break;
            case 2:
                Gp_SetObjFlag2((GpObj5D*)enemy, id, 0);
                break;
            case 3:
                Gp_SetObjFlag4((GpObj5C*)enemy, id, 0);
                break;
        }
        TRANSPOSE_ROT(&coord->workm, &rot);
        pos.vx = arg2[i].point.vx - coord->workm.t[0];
        pos.vy = arg2[i].point.vy - coord->workm.t[1];
        pos.vz = arg2[i].point.vz - coord->workm.t[2];
        scale  = 0x190000 / SquareRoot0(pos.vx * pos.vx + pos.vy * pos.vy + pos.vz * pos.vz);
        pos.vx = pos.vx * scale / 4096;
        pos.vy = pos.vy * scale / 4096;
        pos.vz = pos.vz * scale / 4096;
        gte_SetRotMatrix(&rot);
        gte_ldv0(&pos);
        gte_rtv0();
        gte_stsv(&pos);
        pos.vx += D_actor_503500_8016F36C.vx;
        pos.vy += D_actor_503500_8016F36C.vy;
        pos.vz += D_actor_503500_8016F36C.vz;
        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &pos, &work->field_E0);
        if (crit != 0) {
            Gp_SpawnEff(0x6009C, coord, 0, &pos);
        }
        stun = Gp_GetIdParam2(id);
        if (work->field_E8 < stun) {
            work->field_E8 = stun;
        }
    next:;
    }
}

/// A sub-state of the 0xF4 block, stepped by `field_F1`: applies the boss's
/// animation preset 0xC, counts 0x6F frames (spawning table entry 3 at frame
/// 0x1E with its coordinate parented to this task's), applies preset 0xD once
/// the boss reports sub-state 0xC done, and leaves when 0xD is done too.
void func_actor_503500_8013F328(Task* arg0)
{
    Actor503500WorkF4* work = (Actor503500WorkF4*)arg0->work;
    Task*              task;
    GsCOORDINATE2*     coord;

    if (func_actor_503500_8013608C(arg0->parent) != 0) {
        func_actor_503500_8013F9D4(arg0, 0);
        func_actor_503500_8013611C(arg0->spawnArg1);
        return;
    }
    switch (work->field_F1) {
        case 0:
            func_actor_503500_80135FB4(arg0->parent, 0xC, 8);
            work->field_F1++;
            break;
        case 1:
            work->field_EA++;
            if ((s16)work->field_EA >= 0x6F) {
                work->field_EA = 0;
                work->field_F1++;
            } else if ((s16)work->field_EA == 0x1E) {
                task = Task_SpawnFromTable(&D_actor_503500_8016E9F0, 3, 0, 0);
                if (task != NULL) {
                    coord             = ((TmdObject*)task->extra)->coords;
                    coord->sub        = ((TmdObject*)arg0->extra)->coords;
                    coord->coord.t[0] = 0;
                    coord->coord.t[1] = 0;
                    coord->coord.t[2] = 0;
                }
            }
            break;
        case 2:
            if (func_actor_503500_80136014(arg0->parent, 0xC) != 0) {
                func_actor_503500_80135FB4(arg0->parent, 0xD, 0x10);
                work->field_F1++;
            }
            break;
        case 3:
            if (func_actor_503500_80136014(arg0->parent, 0xD) != 0) {
                func_actor_503500_8013F9D4(arg0, 0);
            }
            break;
    }
}

/// Sub-state 2 of the 0xF4 block, stepped by `field_F1`: phase 0 hides the
/// display node, unlinks the enemy node, releases its state-F0 reference and
/// plays the death sound; phase 1 spawns effects from `D_actor_503500_8016F374`
/// for 0x1F frames, then restores the VRAM rect and moves on.
void func_actor_503500_8013F4A4(Task* arg0)
{
    GpEnemy*           enemy;
    Actor503500WorkF4* work;
    GsCOORDINATE2*     coord;
    s32                pan;

    enemy = arg0->spawnArg2;
    work  = (Actor503500WorkF4*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    switch (work->field_F1) {
        case 0:
            work->obj.flags &= 0x7FFF;
            enemy->recs      = 0;
            Gp_UnlinkNode(&enemy->node);
            func_actor_503500_80135CE8(arg0->parent, arg0->spawnArg1);
            work->field_E8 = 0;
            ((void (*)(s32))Gp_IncStateF0Ref)(0);
            Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
            func_actor_503500_80136048(arg0->parent);
            enemy->reactionFlags &= 0xF0;
            pan                   = (s8)Gp_GetObjPan(coord);
            SndEvt_EnqueueType6(0x40230010, pan, (s8)(gpGetObjDepth(coord) / 2));
            work->field_EC = 2;
            work->field_F1++;
            break;
        case 1:
            if (func_actor_503500_801360BC(arg0->spawnArg1, 3) != 0) {
                Gp_SpawnEff(0x60055, coord, 0x01001A00,
                            &D_actor_503500_8016F374[(s16)((s16)work->field_EA % 6)]);
                if (work->field_EA & 1) {
                    Gp_SpawnEff(0x6018C, coord, 0x04404600,
                                &D_actor_503500_8016F374[(s16)((s16)work->field_EA % 3) + 3]);
                }
            }
            work->field_EA++;
            if ((s16)work->field_EA >= 0x1F) {
                MoveImage(&D_actor_503500_8016F3A4, 0x140, 0x100);
                SndEvt_EnqueueType7(0x40230010, 0x2D);
                func_actor_503500_8013611C(arg0->spawnArg1);
                work->field_F1++;
            }
            break;
        default:
            arg0->state++;
            break;
    }
}

/// The 0xF4 block's per-frame tick, the same shape as
/// `func_actor_503500_8013E9A4`: frozen mode 1 skips the frame entirely,
/// mode 2 only marks the enemy's link node, and anything else clears the
/// coordinate flag and runs the normal chain.
void func_actor_503500_8013F6F0(Task* arg0)
{
    GpEnemy*       enemy;
    GsCOORDINATE2* coord;

    enemy = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (D_801153F4 == 1) {
        return;
    }
    if (D_801153F4 == 2) {
        enemy->node.flags |= 1;
        return;
    }
    coord->flg = 0;
    if (enemy->reactionFlags != 0) {
        func_actor_503500_8013F7D8(arg0);
    }
    func_actor_503500_8013F830(arg0);
    func_actor_503500_8013F8AC(arg0);
}

void func_actor_503500_8013F778(Task* arg0)
{
    GpEnemy* enemy;

    enemy                                                    = arg0->spawnArg2;
    ((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)->sub = &gGfxViewCoord;
    Gp_UnlinkObj(&((Actor503500Work*)arg0->work)->obj);
    enemy->recs = 0;
    arg0->work  = NULL;
    Gp_DestroyEnemy(enemy, arg0);
}

void func_actor_503500_8013F7D8(Task* arg0)
{
    GpEnemy* enemy;

    enemy = arg0->spawnArg2;
    if (enemy->reactionFlags & 1) {
        enemy->reactionFlags &= ~1;
    }
    if (enemy->reactionFlags & 2) {
        enemy->reactionFlags &= ~2;
    }
    if (enemy->reactionFlags & 0xC) {
        enemy->reactionFlags &= ~0xC;
    }
}

void func_actor_503500_8013F830(Task* arg0)
{
    Actor503500Work* work;
    s16              timer;

    work = arg0->work;
    if (work->field_E8 != 0) {
        timer          = (u16)work->field_E8 - 1;
        work->field_E8 = timer;
        if (timer < 0) {
            work->field_E8 = 0;
        }
    }
    if (func_actor_503500_80136208() == 0) {
        func_actor_503500_8013EE5C(arg0, work, &work->rec, 8);
    }
    Gp_ClearRec18Occupied(&work->rec);
}

void func_actor_503500_8013F8AC(Task* arg0)
{
    switch (((Actor503500Work*)arg0->work)->field_F0) {
        case 0:
            func_actor_503500_8013F948(arg0);
            break;
        case 1:
            func_actor_503500_8013F328(arg0);
            break;
        case 2:
            func_actor_503500_8013F4A4(arg0);
            break;
        case 3:
            func_actor_503500_8013F984(arg0);
            break;
    }
}

/// The 0xF4 block's counterpart of `func_actor_503500_80138454`: when a kill is
/// pending, hands the block to sub-state 1 and cancels the countdown.
void func_actor_503500_8013F948(Task* arg0)
{
    if (arg0->killCountdown == 2) {
        func_actor_503500_8013F9D4(arg0, 1);
        arg0->killCountdown = 0;
    }
}

/// The 0xF4 block's sub-state 3 handler: when a kill is pending, hands the
/// block to sub-state 0 and hides its display node.
void func_actor_503500_8013F984(Task* arg0)
{
    Actor503500Work* work;

    if (arg0->killCountdown == 8) {
        func_actor_503500_8013F9D4(arg0, 0);
        work             = arg0->work;
        work->obj.flags |= 0x8000;
    }
}

/// The 0xF4 block's counterpart of `func_actor_503500_80138490`: puts the block
/// into sub-state `arg1`, clears the phase and the two counters that go with
/// it, cancels a pending kill, and records the slot's halfword as "asked to
/// die" when the sub-state is non-zero.
void func_actor_503500_8013F9D4(Task* arg0, s32 arg1)
{
    Actor503500WorkF4* work = (Actor503500WorkF4*)arg0->work;

    work->field_F0      = arg1;
    work->field_F1      = 0;
    work->field_EA      = 0;
    work->field_EC      = 0;
    arg0->killCountdown = 0;
    func_actor_503500_80135F9C(arg0->parent, arg0->spawnArg1, arg1 != 0);
}

void func_actor_503500_8013FA1C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_801320D0;
    sp.funcs[task->state](task);
}

/// `Task::state` handlers `func_actor_503500_80142370` dispatches through.
const TaskFuncTable3 D_actor_503500_80132108 = {
    {
        func_actor_503500_8013FA74,
        func_actor_503500_8013FF0C,
        func_actor_503500_80141D04,
    },
};

void func_actor_503500_8013FA74(Task* arg0)
{
    GpEnemy*             enemy;
    TmdObject*           tmd;
    GsCOORDINATE2*       coord;
    GsCOORDINATE2*       part;
    Actor503500Work3D8*  work;
    GpRec18*             rec;
    GpRec18*             rec2;
    Actor503500IdentMat  m;
    Actor503500MatWords* ident;
    s32                  idx;
    s32                  i;

    idx   = arg0->spawnArg1 - 0xD;
    enemy = arg0->spawnArg2;
    work  = &D_actor_503500_80177B60[idx];
    coord = ((TmdObject*)arg0->extra)->coords;
    tmd   = arg0->extra;
    Mem_Set(work, 0, 0x3D8);
    arg0->work = (Actor503500Work*)work;

    coord->sub        = &((TmdObject*)arg0->parent->extra)->coords[1];
    coord->coord.t[0] = D_actor_503500_8016F3AC[idx].vx;
    coord->coord.t[1] = D_actor_503500_8016F3AC[idx].vy;
    coord->coord.t[2] = D_actor_503500_8016F3AC[idx].vz;
    m.ident.m00_m01   = 0x1000;
    ident             = &m.ident;
    ident->m02_m10    = 0;
    ident->m11_m12    = 0x1000;
    ident->m20_m21    = 0;
    ident->m22        = 0x1000;
    RotMatrix(&D_actor_503500_8016F3CC[idx], &m.mat);
    MulMatrix0(&coord->coord, &m.mat, &coord->coord);
    coord->flg      = 0;
    work->field_3D7 = -1;
    tmd->lightMtx   = &work->light;
    tmd->colorMtx   = &work->color;
    tmd->otOffset   = 0x12;

    enemy->field_4    = &coord->coord;
    part              = &coord[8];
    enemy->field_48   = 0;
    enemy->coord      = part;
    enemy->node.flags = (enemy->node.flags | 8) & 0xFE;
    enemy->bodyPos.vx = D_actor_503500_8016F3EC.vx;
    enemy->bodyPos.vy = D_actor_503500_8016F3EC.vy;
    enemy->bodyPos.vz = D_actor_503500_8016F3EC.vz;
    rec               = work->rec180;
    enemy->param      = &D_actor_503500_8016E7EC[arg0->spawnArg1];
    enemy->recs       = rec;

    work->obj160.coord    = part;
    work->obj160.ctx.recs = rec;
    work->obj160.pos.vx   = D_actor_503500_8016F3EC.vx;
    work->obj160.pos.vy   = D_actor_503500_8016F3EC.vy;
    work->obj160.pos.vz   = D_actor_503500_8016F3EC.vz;
    work->obj160.key      = 0x30023;
    work->obj160.radius   = 0x258;
    work->obj160.flags    = 1;
    Gp_LinkObj(2, &work->obj160);
    Gp_InitRec18Table(rec, 8, 0);
    rec2                  = work->rec260;
    work->obj240.coord    = part;
    work->obj240.ctx.recs = rec2;
    work->obj160.flags   &= 0x7FFF;
    work->obj240.pos.vx   = D_actor_503500_8016F3F4[idx].vx;
    work->obj240.pos.vy   = D_actor_503500_8016F3F4[idx].vy;
    work->obj240.pos.vz   = D_actor_503500_8016F3F4[idx].vz;
    work->obj240.key      = Gp_PackPair(enemy->param->pairTable, 0);
    work->obj240.radius   = 0x1F4;
    work->obj240.flags    = 1;
    Gp_LinkObj(3, &work->obj240);
    Gp_InitRec18Table(rec2, 4, 0);
    work->field_2C0.spawnArgLo = 0x600;
    work->field_2C0.coord      = part;
    work->field_2C0.spawnArgHi = 3;
    work->obj240.flags        &= 0x7FFF;

    copyVector(&work->field_368, &D_actor_503500_8016F414[arg0->spawnArg1 - 0xD]);
    copyVector(&work->field_358, &D_actor_503500_8016F414[arg0->spawnArg1 - 0xD]);
    work->field_39C.w = 0x600000;
    work->field_3B4   = 0x40;
    work->field_3B6   = 0x1000;
    work->field_3D5   = 1;
    for (i = 1; i < 9; i++) {
        work->phase[i] = (i << 9) & 0xFFF;
        work->mats[i]  = coord[i].coord;
    }
    Gp_UpdateCoord(coord);
    func_actor_503500_801421A8(arg0);
    switch (arg0->killCountdown) {
        case 8:
            func_actor_503500_80142310(arg0, 6);
            break;
        case 9:
            tmd->flags |= 0x80;
            func_actor_503500_80142310(arg0, 7);
            break;
        default:
            Gp_LinkNode(&enemy->node);
            enemy->hp           = D_actor_503500_8016E7EC[arg0->spawnArg1].hpMax;
            work->field_3B2     = 0x1000;
            work->obj160.flags |= 0x8000;
            func_actor_503500_80142310(arg0, 0);
            break;
    }
    arg0->exitCallback = func_actor_503500_80141D04;
    arg0->state       += 1;
}

void func_actor_503500_8013FF0C(Task* arg0)
{
    Actor503500Work* work;
    GpEnemy*         enemy;
    TmdObject*       tmd;
    s8               countdown;
    s32              slot;

    work      = arg0->work;
    enemy     = arg0->spawnArg2;
    countdown = work->field_3D7;
    tmd       = (TmdObject*)arg0->extra;
    if (countdown >= 0) {
        if (countdown == 0) {
            Tmd_FreeBuffers(tmd);
        }
        work->field_3D7 = (s8)((u8)work->field_3D7 - 1);
    }
    if (gGameSession->eventState != 0) {
        slot = 0xB;
        if (arg0->spawnArg1 < 0xF) {
            slot = 0xA;
        }
        if (func_actor_503500_80135E04(arg0->parent, slot) == 0) {
            tmd->flags |= 4;
        } else {
            goto tick;
        }
    } else {
    tick:
        func_actor_503500_80135828(arg0, &work->field_3D7);
    }

    switch (D_801153F4) {
        case 1:
            if (!(tmd->flags & 0x80)) {
                func_actor_503500_801421A8(arg0);
            }
            break;
        case 2:
            tmd->flags        |= 0x80;
            enemy->node.flags |= 1;
            break;
        default:
            if (enemy->reactionFlags != 0) {
                func_actor_503500_80140BE8(arg0);
            }
            func_actor_503500_801420C4(arg0);
            func_actor_503500_80141D7C(arg0);
            if (work->field_3D6 == 0) {
                func_actor_503500_80141248(arg0);
                func_actor_503500_80141448(arg0);
            }
            func_actor_503500_801421A8(arg0);
            func_actor_503500_80141B94(arg0);
            break;
    }
}

/// Sub-state of the 0x3D8 enemies. Phase 0 latches the position behind
/// `Player_Status.coordMtx` in `field_370` and rotates its offset from the
/// parent coordinate into `field_368`; phase 1 ramps `field_3CC` to 0x2000 and
/// re-aims once `field_3D4` is set; phases 2..4 ramp it back to 0. While in
/// phases 0..1, `func_actor_503500_80142310` ends the state after 120 frames
/// or when `func_actor_503500_80136218`'s reading leaves the window the slot
/// (and whether its partner slot is empty) allows.
void func_actor_503500_801400A4(Task* arg0)
{
    SVECTOR          v;
    SVECTOR          pos;
    MATRIX           mtx;
    MATRIX           rot;
    Actor503500Work* work;
    GsCOORDINATE2*   coord;
    s32              keep;
    s32              dist;

    work  = arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (func_actor_503500_8013608C(arg0->parent) != 0) {
        func_actor_503500_80142310(arg0, 0);
        func_actor_503500_8013611C(arg0->spawnArg1);
        return;
    }
    switch (work->field_3D0) {
        case 0:
            work->field_370.vx = Player_Status.coordMtx->t[0];
            work->field_370.vy = Player_Status.coordMtx->t[1];
            work->field_370.vz = Player_Status.coordMtx->t[2];
            Gp_ComposeParentWorld(coord->sub, &mtx, &pos);
            v.vx = work->field_370.vx - pos.vx;
            v.vy = work->field_370.vy - pos.vy - 5000;
            v.vz = work->field_370.vz - pos.vz;
            TRANSPOSE_ROT(&mtx, &rot);
            gte_SetRotMatrix(&rot);
            gte_ldv0(&v);
            gte_rtv0();
            gte_stsv(&work->field_368);
            func_actor_503500_80135FB4(arg0->parent, 0x12, 0x10);
            work->field_3D0++;
            break;
        case 1:
            work->field_3CC += 0x88;
            if ((s16)work->field_3CC > 0x2000) {
                work->field_3CC = 0x2000;
            }
            if (work->field_3D4 != 0 && work->field_3C8 > 2000) {
                Gp_ComposeParentWorld(coord->sub, &mtx, &pos);
                v.vx = work->field_370.vx - pos.vx;
                v.vy = work->field_370.vy - pos.vy + 500;
                v.vz = work->field_370.vz - pos.vz;
                TRANSPOSE_ROT(&mtx, &rot);
                gte_SetRotMatrix(&rot);
                gte_ldv0(&v);
                gte_rtv0();
                gte_stsv(&work->field_368);
                work->field_39C     = 0x4000000;
                work->obj240.flags |= 0x8000;
                work->field_3D0++;
            }
            break;
        case 2:
            work->field_3CC -= 0x200;
            if ((s16)work->field_3CC < 0) {
                work->field_3CC = 0;
            }
            work->field_3B6 -= 0x80;
            if ((s16)work->field_3B6 < 0) {
                work->field_3B6 = 0;
            }
            work->field_3B0++;
            if ((s16)work->field_3B0 > 30) {
                work->field_3B0 = 0;
                work->field_3D0++;
            }
            break;
        case 3:
            work->field_3CC -= 0x2AA;
            if ((s16)work->field_3CC < 0) {
                work->field_3CC = 0;
            }
            if (work->field_3D4 != 0) {
                work->field_3B0     = 0;
                work->obj240.flags &= 0x7FFF;
                work->field_3D0++;
            }
            break;
        case 4:
            work->field_3CC -= 0x400;
            if ((s16)work->field_3CC < 0) {
                work->field_3CC = 0;
            }
            work->field_3B0++;
            if ((s16)work->field_3B0 > 30) {
                func_actor_503500_80142310(arg0, 0);
            }
            break;
    }
    keep = 0;
    dist = func_actor_503500_80136218();
    switch (arg0->spawnArg1) {
        case 13:
            if (func_actor_503500_80135E04(arg0, 0xE) != 0) {
                if (dist < -1900 || dist > 1000) {
                    keep = 1;
                }
            } else if (dist > 1000 && dist < 1700) {
                keep = 1;
            }
            break;
        case 14:
            if (func_actor_503500_80135E04(arg0, 0xD) != 0) {
                if (dist < -1900 || dist > 1000) {
                    keep = 1;
                }
            } else if (dist < -1900 || dist > 1699) {
                keep = 1;
            }
            break;
        case 15:
            if (func_actor_503500_80135E04(arg0, 0x10) != 0) {
                if (dist < -1000 || dist > 1900) {
                    keep = 1;
                }
            } else if (dist < -1699 || dist > 1900) {
                keep = 1;
            }
            break;
        case 16:
            if (func_actor_503500_80135E04(arg0, 0xF) != 0) {
                if (dist < -1000 || dist > 1900) {
                    keep = 1;
                }
            } else if (dist < -1000) {
                if (dist >= -1699) {
                    keep = 1;
                }
            }
            break;
    }
    work->field_3AE++;
    if (work->field_3D0 < 2 && (work->field_3AE > 120 || keep == 0)) {
        func_actor_503500_80142310(arg0, 0);
    }
}

/// Death state of the 0x3D8 enemies, the same body as
/// `func_actor_503500_80139014` at this block's offsets: unlinks the enemy
/// node, waits for `field_3D4`, re-parents the root coordinate onto the view
/// and plays 0x40230004 at it. Phase 2 eases every part back to rest while the
/// body rises; past 1000 the pose is saved in `field_378` and phase 3 squashes
/// it vertically (`field_3AC`), firing the cues on frames 10/15/30/40.
/// Every twelfth frame of phases 0..2 sprays effects along parts 8..1.
void func_actor_503500_80140654(Task* arg0)
{
    MATRIX           m;
    VECTOR           scale;
    SVECTOR          rot;
    Actor503500Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   part;
    s16*             p;
    s32              phase;
    s32              i;
    s32              j;

    work  = arg0->work;
    enemy = arg0->spawnArg2;
    phase = work->field_3D0;
    coord = ((TmdObject*)arg0->extra)->coords;
    switch (phase) {
        case 0:
            work->obj160.flags &= 0x7FFF;
            enemy->recs         = 0;
            Gp_UnlinkNode(&enemy->node);
            func_actor_503500_80135CE8(arg0->parent, arg0->spawnArg1);
            work->field_3A8 = 0;
            ((void (*)(s32))Gp_IncStateF0Ref)(0);
            Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
            func_actor_503500_80136048(arg0->parent);
            enemy->reactionFlags &= 0xF0;
            work->field_368.vy    = 0x7D0;
            work->field_3D0++;
            break;
        case 1:
            if (work->field_3D4 != 0) {
                Gp_ComposeParentWorld(coord, &m, &rot);
                coord->coord      = m;
                coord->coord.t[0] = rot.vx;
                coord->coord.t[1] = rot.vy;
                coord->coord.t[2] = rot.vz;
                coord->sub        = &gGfxViewCoord;
                coord->flg        = 0;
                work->field_3D6   = phase;
                Gp_UpdateCoord(coord);
                SndEvt_EnqueueType6(0x40230004, (s8)Gp_GetObjPan(coord),
                                    (s8)(gpGetObjDepth(coord) / 2));
                work->field_3D0++;
            }
            break;
        case 2:
            for (i = 1; i < 9; i++) {
                part = &coord[i];
                Gp_ExtractEuler(&rot, &part->coord);
                p = &rot.vx;
                j = 1;
                do {
                    if (*p > 0) {
                        *p -= 2;
                        if (*p < 0) {
                            *p = 0;
                        }
                    } else {
                        *p += 2;
                        if (*p > 0) {
                            *p = 0;
                        }
                    }
                    p++;
                } while (j++ < 3);
                func_actor_503500_SetRotIdentity(&coord[i].coord);
                RotMatrix(&rot, &part->coord);
                part->flg = 0;
            }
            coord->flg         = 0;
            coord->coord.t[1] += 10;
            if (coord->coord.t[1] > 1000) {
                work->field_378 = coord->coord;
                work->field_3AC = 0x1000;
                work->field_3D0++;
            }
            break;
        case 3:
            if (work->field_3AC > 0x200) {
                work->field_3AC -= 0x20;
            }
            coord->coord = work->field_378;
            scale.vx     = 0x1000;
            scale.vy     = work->field_3AC;
            scale.vz     = 0x1000;
            ScaleMatrixL(&coord->coord, &scale);
            coord->flg = 0;
            switch (work->field_3AE) {
                case 10:
                    ((TmdObject*)arg0->extra)->flags |= 2;
                    Gp_SetLightMode((GpObj4C*)enemy, 1);
                    SndEvt_EnqueueType6(0xD, (s8)Gp_GetObjPan(coord),
                                        (s8)(gpGetObjDepth(coord) / 2));
                    break;
                case 15:
                    Gp_SpawnEff(0x600A5, coord, 1, NULL);
                    break;
                case 30:
                    Gp_SetLightMode((GpObj4C*)enemy, 2);
                    break;
                case 40:
                    SndEvt_EnqueueType7(0xD, 1);
                    arg0->state++;
                    break;
            }
            work->field_3AE++;
            break;
    }
    if (func_actor_503500_801360BC(arg0->spawnArg1, 4) != 0 && work->field_3D0 < 3 &&
        (u32)D_80070F70 % 12 == 0) {
        for (i = 8, j = 0; i > 0; i--) {
            Gp_SpawnEff(0x60070, &((TmdObject*)arg0->extra)->coords[i], 0xB0008600, &D_actor_503500_8016F448[j]);
            j++;
            j = (j < 3) ? j : 0;
        }
    }
    if (gGameSession->eventState != 0 && gGameSession->viewReady != 0 && work->field_3D0 > 0) {
        SndEvt_EnqueueType7(0xD, 1);
        arg0->state = 2;
    }
}

void func_actor_503500_80140BE8(Task* arg0)
{
    Actor503500Work* work;
    GpEnemy*         enemy;
    s32              dmg;
    u8               flags;

    enemy = arg0->spawnArg2;
    work  = arg0->work;
    if ((func_actor_503500_80136208() == 0) && (gGameSession->eventState == 0)) {
        flags = enemy->reactionFlags;
        if (flags & 1) {
            enemy->reactionFlags = flags & 0xFE;
            func_actor_503500_80142310(arg0, 0);
            work->field_3A6 = 5;
            work->field_3AA = 8;
        }
        if (enemy->reactionFlags & 2) {
            enemy->reactionFlags &= 0xFD;
        }
        if (enemy->reactionFlags & 0xC) {
            func_actor_503500_80142310(arg0, 4);
            if (Gp_ObjFlag4Expired((GpObj5C*)arg0->spawnArg2) != 0) {
                enemy->reactionFlags &= 0xF3;
                func_actor_503500_80142310(arg0, 0);
            } else {
                dmg = Gp_TickObjFlag4((GpObj5C*)enemy);
                if (dmg != 0) {
                    enemy->hp -= dmg;
                    func_800DA6E8(&enemy->node, dmg, 0);
                    work->field_3AA = 8;
                    if (enemy->hp <= 0) {
                        enemy->reactionFlags &= 0xF3;
                        func_actor_503500_80142310(arg0, 5);
                    } else {
                        func_actor_503500_80142310(arg0, 0);
                    }
                }
            }
        }
    }
}

/// Applies this frame's hits from `obj160`'s collision records `arg2[0..arg3)`
/// to the 0x3D8 block's enemy, like `func_actor_503500_8013EE5C`, but at model
/// part 8: each attack id is taken once, only type-2 ids land while the
/// `field_3A8` countdown is clear, and a hit that empties `field_40` enters
/// state 5 (unless already past it) instead of applying the id's status effect.
/// The hit effect is pulled to 500 units along the contact offset, and a hit
/// landing in state 1 drops back to state 0. `arg1` is passed but unused.
void func_actor_503500_80140D38(Task* arg0, GpObj* arg1, GpRec18* arg2, s32 arg3)
{
    SVECTOR             pos;
    MATRIX              rot;
    MATRIX              mtx;
    VECTOR              d;
    Actor503500Work3D8* work;
    GpEnemy*            enemy;
    GsCOORDINATE2*      coord;
    GsCOORDINATE2*      src;
    s16                 stun;
    u32                 id;
    s32                 dmg;
    s32                 crit;
    s32                 scale;
    s32                 i;
    s32                 j;

    enemy = arg0->spawnArg2;
    work  = (Actor503500Work3D8*)arg0->work;
    coord = &((TmdObject*)arg0->extra)->coords[8];
    for (i = 0; i < arg3; i++) {
        id = arg2[i].key;
        for (j = 0; j < i; j++) {
            if (arg2[j].key == id) {
                goto next;
            }
        }
        if ((id & 0xFFFF0000) == 0x10000) {
            continue;
        }
        if ((id & 0xFFFF0000) != 0x20000) {
            continue;
        }
        if (work->field_3A8 != 0) {
            continue;
        }
        Gp_ComposeParentWorld(coord, &mtx, &pos);
        src  = ((TmdObject*)Gp_ActorSlots[(id >> 7) & 1]->extra)->coords;
        d.vx = src->coord.t[0] - pos.vx;
        d.vy = src->coord.t[1] - pos.vy;
        d.vz = src->coord.t[2] - pos.vz;
        crit = 0;
        dmg  = Gp_ComputeDamage(id, SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz), crit, crit);
        if (Gp_RollEnemyChance(enemy, id, crit) != 0) {
            dmg *= 4;
            crit = 1;
        }
        func_800E2C78((GpObj40*)enemy, id, dmg, 0);
        func_800DA6E8(&enemy->node, dmg, 0);
        enemy->hp -= dmg;
        if (enemy->hp <= 0) {
            if (work->field_3A4 < 6) {
                func_actor_503500_80142310(arg0, 5);
            }
        } else {
            switch (Gp_GetIdParam0(id) & 0xFFFF) {
                case 0:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    break;
                case 1:
                    Gp_SetObjFlag1((GpObj4C*)enemy);
                    break;
                case 2:
                    Gp_SetObjFlag2((GpObj5D*)enemy, id, 0);
                    break;
                case 3:
                    Gp_SetObjFlag4((GpObj5C*)enemy, id, 0);
                    break;
            }
        }
        TRANSPOSE_ROT(&coord->workm, &rot);
        pos.vx = arg2[i].point.vx - coord->workm.t[0];
        pos.vy = arg2[i].point.vy - coord->workm.t[1];
        pos.vz = arg2[i].point.vz - coord->workm.t[2];
        scale  = 0x1F4000 / SquareRoot0(pos.vx * pos.vx + pos.vy * pos.vy + pos.vz * pos.vz);
        pos.vx = pos.vx * scale / 4096;
        pos.vy = pos.vy * scale / 4096;
        pos.vz = pos.vz * scale / 4096;
        gte_SetRotMatrix(&rot);
        gte_ldv0(&pos);
        gte_rtv0();
        gte_stsv(&pos);
        pos.vx += D_actor_503500_8016F3EC.vx;
        pos.vy += D_actor_503500_8016F3EC.vy;
        pos.vz += D_actor_503500_8016F3EC.vz;
        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &pos, &work->field_2C0);
        if (crit != 0) {
            Gp_SpawnEff(0x6009C, coord, 0, &pos);
        }
        stun = Gp_GetIdParam2(id);
        if (work->field_3A8 < stun) {
            work->field_3A8 = stun;
        }
        if (work->field_3A4 == 1) {
            func_actor_503500_80142310(arg0, 0);
        }
    next:;
    }
}

/// The 0x3D8 block's copy of `func_actor_503500_80139EFC`: saves `field_358`
/// into `field_360`, then steers it toward `field_368`. Inside the arrival
/// distance (the integer half of `field_39C`) it snaps onto the target and sets
/// `field_3D4`; otherwise the speed `field_398` accelerates toward
/// +/-`field_39C` while `field_3D5` is set, or decays to 0, and moves
/// `field_358` along the normalized offset (at a quarter speed while
/// `field_3AA` runs).
void func_actor_503500_80141248(Task* arg0)
{
    SVECTOR             d;
    SVECTOR             n;
    VECTOR              step;
    Actor503500Work3D8* work;
    s32                 lim;
    s32                 speed;

    work               = (Actor503500Work3D8*)arg0->work;
    work->field_360.vx = work->field_358.vx;
    work->field_360.vy = work->field_358.vy;
    work->field_360.vz = work->field_358.vz;
    d.vx               = work->field_368.vx - work->field_358.vx;
    d.vy               = work->field_368.vy - work->field_358.vy;
    d.vz               = work->field_368.vz - work->field_358.vz;
    if (ABS(d.vx) + ABS(d.vy) + ABS(d.vz) < work->field_39C.h.hi) {
        work->field_3D4    = 1;
        work->field_358.vx = work->field_368.vx;
        work->field_358.vy = work->field_368.vy;
        work->field_358.vz = work->field_368.vz;
        return;
    }
    lim             = work->field_39C.w;
    work->field_3D4 = 0;
    if (work->field_3D5 != 0) {
        speed = work->field_398 + lim / 32;
        if (speed > 0) {
            if (speed > lim) {
                speed = lim;
            }
        } else if (speed < -lim) {
            speed = -lim;
        }
    } else {
        speed = work->field_398 - lim / 32;
        if (speed < 0) {
            speed = 0;
        }
    }
    work->field_398 = speed;
    VectorNormalSS(&d, &n);
    if (work->field_3AA != 0) {
        speed >>= 2;
    }
    step.vx             = n.vx * (speed >> 12);
    step.vy             = n.vy * (speed >> 12);
    step.vz             = n.vz * (speed >> 12);
    work->field_358.vx += step.vx >> 16;
    work->field_358.vy += step.vy >> 16;
    work->field_358.vz += step.vz >> 16;
}

/// Lays the 0x3D8 enemy's nine-point chain along a cubic Bezier from the root
/// coordinate's world position to its parent's, with the near control point
/// offset 1000 units in the root's frame (X mirrored for spawn slots 15 / 16)
/// and both far control points at the parent plus its rotated `field_358`. The
/// samples land in `pts`, `func_actor_503500_8014176C` re-aims the links along
/// them, and links 2..8 get a pitch of a fading sine sway plus the scaled rest
/// pitch before `func_actor_503500_80142220` applies it.
void func_actor_503500_80141448(Task* arg0)
{
    SVECTOR                  ctrl[4];
    SVECTOR                  ofs;
    SVECTOR                  tmp;
    VECTOR                   out[9];
    MATRIX                   m;
    GsCOORDINATE2*           coord;
    Actor503500Work3D8Chain* work;
    s32                      i;
    s32                      v;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor503500Work3D8Chain*)arg0->work;
    Gp_ComposeParentWorld(coord, &m, &ctrl[0]);
    work->pts[0].vx = ctrl[0].vx;
    work->pts[0].vy = ctrl[0].vy;
    work->pts[0].vz = ctrl[0].vz;
    if ((u32)(arg0->spawnArg1 - 15) < 2) {
        ofs.vx = -1000;
        ofs.vy = 0;
        ofs.vz = 1000;
    } else {
        ofs.vx = 1000;
        ofs.vy = 0;
        ofs.vz = 1000;
    }
    gte_SetRotMatrix(&m);
    gte_ldv0(&ofs);
    gte_rtv0();
    gte_stsv(&ctrl[1]);
    ctrl[1].vx += ctrl[0].vx;
    ctrl[1].vy += ctrl[0].vy;
    ctrl[1].vz += ctrl[0].vz;
    Gp_ComposeParentWorld(coord->sub, &m, &tmp);
    gte_SetRotMatrix(&m);
    gte_ldv0(&work->field_358);
    gte_rtv0();
    gte_stsv(&ofs);
    tmp.vx    += ofs.vx;
    tmp.vy    += ofs.vy;
    tmp.vz    += ofs.vz;
    ctrl[2].vx = tmp.vx;
    ctrl[2].vy = tmp.vy;
    ctrl[2].vz = tmp.vz;
    ctrl[3].vx = tmp.vx;
    ctrl[3].vy = tmp.vy;
    ctrl[3].vz = tmp.vz;
    for (i = 8; i >= 0; i--) {
        func_actor_503500_80141A44(ctrl, &ctrl[3], 9, i, &out[i].vx);
        copyVector(&work->pts[8 - i], &out[i]);
    }
    func_actor_503500_8014176C(work->pts, ((TmdObject*)arg0->extra)->coords);
    for (i = 8; i >= 2; i--) {
        v                   = ((work->field_3B4 * work->field_3B6 >> 12) * rsin(work->phase[i])) >> 12;
        work->angles[i].vx  = v;
        work->angles[i].vx += D_actor_503500_8016F434[i] * work->field_3CC >> 12;
        work->angles[i].vy  = 0;
        work->angles[i].vz  = 0;
        work->phase[i]      = (work->phase[i] + 0x80) & 0xFFF;
    }
    func_actor_503500_80142220(work->angles, ((TmdObject*)arg0->extra)->coords);
}

/// Re-aims a chain of eight child coordinates along the polyline `pts[0..8]`.
/// `world` starts as the chain root's world rotation and accumulates each
/// link's local rotation; the segment `pts[i + 1] - pts[i]` is taken into that
/// frame, and the resulting direction becomes the next link's basis
/// (`Gfx_OrthonormalBasis`, up hint +Y) with the local segment as its
/// translation. Works in an `Actor503500ChainScratch` on the scratchpad stack.
void func_actor_503500_8014176C(SVECTOR* pts, GsCOORDINATE2* coords)
{
    Actor503500ChainScratch* s;
    MATRIX*                  inv;
    SVECTOR*                 dir;
    s32                      i;
    s32                      j;

    s        = (Actor503500ChainScratch*)(SCRATCH_SP -= sizeof(Actor503500ChainScratch));
    s->up.vx = 0;
    s->up.vy = 0x1000;
    s->up.vz = 0;
    Gp_ComposeParentWorld(coords->sub, &s->world, &s->rot);
    for (i = 0, j = 1; i < 8; i++, j++) {
        s->diff.vx = pts[j].vx - pts[i].vx;
        s->diff.vy = pts[j].vy - pts[i].vy;
        s->diff.vz = pts[j].vz - pts[i].vz;
        gte_SetRotMatrix(&s->world);
        inv = &s->inv;
        dir = &s->dir;
        gte_ldclmv(&coords[i].coord);
        gte_rtir();
        gte_stclmv(&s->world);
        gte_ldclmv((char*)&coords[i].coord + 2);
        gte_rtir();
        gte_stclmv((char*)&s->world + 2);
        gte_ldclmv((char*)&coords[i].coord + 4);
        gte_rtir();
        gte_stclmv((char*)&s->world + 4);
        TRANSPOSE_ROT(&s->world, inv);
        gte_SetRotMatrix(inv);
        gte_ldv0(&s->diff);
        gte_rtv0();
        gte_stlvnl(&s->pos);
        VectorNormalS(&s->pos, dir);
        Gfx_OrthonormalBasis(&coords[j].coord, dir, &s->up);
        coords[j].coord.t[0] = s->pos.vx;
        coords[j].coord.t[1] = s->pos.vy;
        coords[j].coord.t[2] = s->pos.vz;
    }
    SCRATCH_SP += sizeof(Actor503500ChainScratch);
}

/// Same cubic Bezier evaluation as `func_actor_503500_8013A7B0`: control points
/// `pts[0..2]` and `p3`, `t` running from 1 (0xFFFF) down to 0 as `pos`
/// reaches `len`. Writes the X/Y/Z result to `out`.
void func_actor_503500_80141A44(SVECTOR* pts, SVECTOR* p3, s32 len, s32 pos, s32* out)
{
    SVECTOR  coeff[3];
    SVECTOR* p1;
    SVECTOR* p2;
    s32      t;
    s32      i;
    s32*     o;

    if (len != 0) {
        t  = ((len - pos) * 0xFFFF) / len;
        p1 = &pts[1];
        p2 = &pts[2];
        func_actor_503500_801422B8(pts->vx, p1->vx, p2->vx, p3->vx, &coeff[0]);
        func_actor_503500_801422B8(pts->vy, p1->vy, p2->vy, p3->vy, &coeff[1]);
        func_actor_503500_801422B8(pts->vz, p1->vz, p2->vz, p3->vz, &coeff[2]);
        o = out;
        for (i = 0; i < 3; i++) {
            *o++ = ((((((coeff[i].vx * t) >> 16) + coeff[i].vy) * t >> 16) + coeff[i].vz) * t >> 16) + coeff[i].pad;
        }
    }
}

/// The fade-level counterpart of `func_actor_503500_8013AB38`: while
/// `field_3B2` is below 0x1000, blends parts 1..8 toward the 0x3D8 block's
/// matrix table, copying the lerped rotation back word-wise and keeping a
/// `field_3B2 / 0x1000` share of each part's offset from its table entry.
void func_actor_503500_80141B94(Task* arg0)
{
    MATRIX           m;
    VECTOR           d;
    Actor503500Work* work;
    GsCOORDINATE2*   coord;
    MATRIX*          mat;
    s32*             src;
    s32*             dst;
    s32              t;
    s32              i;
    s32              j;

    work  = arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords + 1;
    if ((s16)work->field_3B2 < 0x1000) {
        mat = &((Actor503500Work3D8Mtx*)work)->mats[1];
        t   = (s16)work->field_3B2;
        for (i = 1; i < 9; i++) {
            Gp_LerpOrthonormal(mat, &coord->coord, &m, t);
            dst = (s32*)&coord->coord;
            src = (s32*)&m;
            for (j = 0; j < 4; j++) {
                *dst++ = *src++;
            }
            coord->coord.m[2][2] = m.m[2][2];
            d.vx                 = ((coord->coord.t[0] - mat->t[0]) * t) >> 12;
            d.vy                 = ((coord->coord.t[1] - mat->t[1]) * t) >> 12;
            d.vz                 = ((coord->coord.t[2] - mat->t[2]) * t) >> 12;
            coord->coord.t[0]    = mat->t[0] + d.vx;
            coord->coord.t[1]    = mat->t[1] + d.vy;
            coord->coord.t[2]    = mat->t[2] + d.vz;
            mat++;
            coord++;
        }
    }
}

void func_actor_503500_80141D04(Task* arg0)
{
    GpEnemy* enemy;

    enemy = arg0->spawnArg2;
    func_actor_503500_8013611C(arg0->spawnArg1);
    ((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)->sub = &gGfxViewCoord;
    Gp_UnlinkObj(&((Actor503500Work*)arg0->work)->obj160);
    Gp_UnlinkObj(&((Actor503500Work*)arg0->work)->obj240);
    enemy->recs = 0;
    arg0->work  = NULL;
    Gp_DestroyEnemy(enemy, arg0);
}

void func_actor_503500_80141D7C(Task* arg0)
{
    Actor503500Work* work;

    work = arg0->work;
    switch (work->field_3A4) {
        case 0:
            func_actor_503500_80141E64(arg0);
            break;
        case 1:
            func_actor_503500_801400A4(arg0);
            break;
        case 2:
            work->field_3A6--;
            if (work->field_3A6 < 0) {
                func_actor_503500_80142310(arg0, 0);
            }
            break;
        case 5:
            func_actor_503500_80140654(arg0);
            break;
        case 6:
            func_actor_503500_80141F48(arg0);
            break;
        case 7:
            func_actor_503500_80141FC8(arg0);
            break;
    }
    work->field_3AA--;
    if (work->field_3AA < 0) {
        work->field_3AA = 0;
    }
}

void func_actor_503500_80141E64(Task* arg0)
{
    Actor503500Work* work;
    u16              level;

    work = arg0->work;
    if (arg0->killCountdown == 2) {
        func_actor_503500_80142310(arg0, 1);
        arg0->killCountdown = 0;
    }
    level           = work->field_3B6 + 0x20;
    work->field_3B6 = level;
    if ((s16)level >= 0x1001) {
        work->field_3B6 = 0x1000;
    }
    level           = work->field_3CC - 0x111;
    work->field_3CC = level;
    if ((s16)level < 0) {
        work->field_3CC = 0;
    }
    work->field_368.vx = D_actor_503500_8016F3AC[arg0->spawnArg1].vx;
    work->field_368.vy = D_actor_503500_8016F3AC[arg0->spawnArg1].vy;
    work->field_368.vz = D_actor_503500_8016F3AC[arg0->spawnArg1].vz;
}

void func_actor_503500_80141F48(Task* arg0)
{
    Actor503500Work* work;
    u16              level;

    work            = arg0->work;
    level           = work->field_3B2 + 0x10;
    work->field_3B2 = level;
    if ((s16)level >= 0x1001) {
        Gp_LinkNode(&((GpEnemy*)arg0->spawnArg2)->node);
        work->field_3B2     = 0x1000;
        work->obj160.flags |= 0x8000;
        func_actor_503500_80142310(arg0, 0);
    }
}

/// Sub-state 0 resets the matrix table to identity; sub-state 1 raises the
/// fade level by 0x20 a frame and, once it passes 0x1000, relinks the display
/// node and moves on like `func_actor_503500_80141F48`.
void func_actor_503500_80141FC8(Task* arg0)
{
    Actor503500Work* work;
    u16              level;
    s32              i;
    long*            t;

    work = arg0->work;
    switch (work->field_3D0) {
        case 0:
            for (i = 1; i < 9; i++) {
                func_actor_503500_SetRotIdentity(&((Actor503500Work3D8Mtx*)work)->mats[i]);
                // The view shifted by i matrices puts mats[i] at mats[0]; this
                // `(work + i) + offset` association is what lets the pointer
                // derive from the giv the indexed store below uses.
                t                                            = ((Actor503500Work3D8Mtx*)((MATRIX*)work + i))->mats[0].t;
                ((Actor503500Work3D8Mtx*)work)->mats[i].t[0] = 0;
                t[1]                                         = 0;
                t[2]                                         = 0;
            }
            work->field_3B2 = 0;
            work->field_3D0++;
        case 1:
            level           = work->field_3B2 + 0x20;
            work->field_3B2 = level;
            if ((s16)level >= 0x1001) {
                Gp_LinkNode(&((GpEnemy*)arg0->spawnArg2)->node);
                work->field_3B2     = 0x1000;
                work->obj160.flags |= 0x8000;
                func_actor_503500_80142310(arg0, 0);
            }
            break;
    }
}

/// Steps the 0x3D8 block's countdown at 0x3A8 down to zero, then, unless the
/// global freeze is on, runs both display nodes through their record tables
/// before releasing the tables. Same shape as `func_actor_503500_80144004`.
void func_actor_503500_801420C4(Task* arg0)
{
    Actor503500Work* work;
    s16              timer;

    work = arg0->work;
    if (work->field_3A8 != 0) {
        timer           = (u16)work->field_3A8 - 1;
        work->field_3A8 = timer;
        if (timer < 0) {
            work->field_3A8 = 0;
        }
    }
    if (func_actor_503500_80136208() == 0) {
        func_actor_503500_80140D38(arg0, &work->obj160, work->rec180, 8);
        func_actor_503500_8014215C(arg0, &work->obj240, work->rec260, 4);
    }
    Gp_ClearRec18Occupied(work->rec180);
    Gp_ClearRec18Occupied(work->rec260);
}

/// Scans `count` `GpRec18` slots and clears bit 0x8000 of `obj->flags` for
/// every slot whose `key` high half is 1.
void func_actor_503500_8014215C(Task* arg0, GpObj* obj, GpRec18* rec, s32 count)
{
    s32 i;

    for (i = 0; i < count; i++, rec++) {
        if ((rec->key & 0xFFFF0000) == 0x10000) {
            obj->flags &= 0x7FFF;
        }
    }
}

/// Copies the actor's attach-coordinate world position into a stack `VECTOR`
/// and hands it to `Gp_UpdateActorColor` with no blend parameters. Same body as
/// `func_actor_503500_80136AEC`.
void func_actor_503500_801421A8(Task* arg0)
{
    VECTOR vec;

    vec.vx = ((TmdObject*)arg0->extra)->coords->workm.t[0];
    vec.vy = ((TmdObject*)arg0->extra)->coords->workm.t[1];
    vec.vz = ((TmdObject*)arg0->extra)->coords->workm.t[2];
    Gp_UpdateActorColor(arg0->spawnArg2, &vec, 0, 0);
}

/// Re-aims coordinate nodes 2..7 of the model: each node's rotation is read
/// back as Euler angles, its pitch replaced with the caller's per-node angle,
/// and the matrix rebuilt from the result. The identity splat before
/// `RotMatrixZYX` clears the node's rotation with five aligned stores, the same
/// idiom `func_800B0928` uses.
void func_actor_503500_80142220(SVECTOR* angles, GsCOORDINATE2* nodes)
{
    SVECTOR ang;
    MATRIX* m;
    s32     i;

    for (i = 2; i < 8; i++) {
        m = &nodes[i].coord;
        Gp_ExtractEuler(&ang, m);
        ang.vx                 = angles[i].vx;
        *(s32*)&nodes[i].coord = ONE;
        *(s32*)&m->m[0][2]     = 0;
        *(s32*)&m->m[1][1]     = ONE;
        *(s32*)&m->m[2][0]     = 0;
        m->m[2][2]             = ONE;
        RotMatrixZYX(&ang, m);
    }
}

/// Converts one axis of a cubic Bezier segment (control points `p0`..`p3`) into
/// the polynomial coefficients of `B(t)`, stored high order first: `t^3`, `t^2`,
/// `t` and the constant term.
void func_actor_503500_801422B8(s32 p0, s32 p1, s32 p2, s32 p3, SVECTOR* coeff)
{
    coeff->vx  = -p0 + (p1 - p2) * 3 + p3;
    coeff->vy  = (p0 + p2) * 3 - p1 * 6;
    coeff->vz  = (-p0 + p1) * 3;
    coeff->pad = p0;
}

void func_actor_503500_80142310(Task* arg0, s32 arg1)
{
    Actor503500Work* work;

    work                = arg0->work;
    work->field_3A4     = arg1;
    work->field_3D0     = 0;
    work->field_3D1     = 0;
    work->field_3AE     = 0;
    work->field_3B0     = 0;
    work->field_39C     = 0x600000;
    work->obj240.flags &= 0x7FFF;
    arg0->killCountdown = 0;
    func_actor_503500_80135F9C(arg0->parent, arg0->spawnArg1, arg1 != 0);
}

void func_actor_503500_80142370(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132108;
    sp.funcs[task->state](task);
}

/// `Task::state` handlers `func_actor_503500_801442A8` dispatches through.
const TaskFuncTable3 D_actor_503500_80132178 = {
    {
        func_actor_503500_801423C8,
        func_actor_503500_80143EB4,
        func_actor_503500_80143F78,
    },
};

/// State-0 init of the 0x224 enemy at `D_actor_503500_80178AC0[spawnArg1 - 0xA]`:
/// clears the block, hangs the task's coordinate off the parent part picked by
/// `D_actor_503500_80171464`, republishes the parent's light and colour
/// matrices, and links three display nodes - `obj0` on the task's own
/// coordinate, `obj1` / `obj2` on parent parts 6 / 7 (slot 1) or 12 / 13
/// (slot 0) sharing `rec1` - before starting sub-state 0.
void func_actor_503500_801423C8(Task* arg0)
{
    GpEnemy*            enemy;
    TmdObject*          tmd;
    Task*               parent;
    s32                 slot;
    Actor503500Work224* work;
    GsCOORDINATE2*      coord;
    TmdObject*          parentTmd;
    SVECTOR*            ofs;
    GpRec18*            rec;
    GsCOORDINATE2*      parts;
    GsCOORDINATE2*      parts2;

    enemy     = arg0->spawnArg2;
    tmd       = arg0->extra;
    parent    = arg0->parent;
    slot      = arg0->spawnArg1 - 0xA;
    work      = &D_actor_503500_80178AC0[slot];
    coord     = tmd->coords;
    parentTmd = parent->extra;
    Mem_Set(work, 0, 0x224);
    arg0->work = (Actor503500Work*)work;

    coord->sub        = &((TmdObject*)parent->extra)->coords[D_actor_503500_80171464[slot]];
    coord->coord.t[0] = D_actor_503500_80171478.vx;
    coord->coord.t[1] = D_actor_503500_80171478.vy;
    coord->coord.t[2] = D_actor_503500_80171478.vz;
    work->field_220   = arg0->spawnArg1 - 0xA;
    tmd->lightMtx     = parentTmd->lightMtx;
    tmd->colorMtx     = parentTmd->colorMtx;
    tmd->otOffset     = 0x13;
    tmd->flags       |= 0x84;
    coord->flg        = 0;

    enemy->field_4    = &coord->coord;
    ofs               = &D_actor_503500_80171480[slot];
    enemy->field_48   = 0;
    enemy->coord      = coord;
    enemy->node.flags = (enemy->node.flags | 8) & 0xFE;
    enemy->bodyPos.vx = ofs->vx;
    enemy->bodyPos.vy = ofs->vy;
    enemy->bodyPos.vz = ofs->vz;
    rec               = work->rec0;
    enemy->param      = &D_actor_503500_8016E7EC[arg0->spawnArg1];
    enemy->recs       = rec;
    enemy->hp         = enemy->param->hpMax;

    work->obj0.coord    = coord;
    work->obj0.ctx.recs = rec;
    work->obj0.pos.vx   = ofs->vx;
    work->obj0.pos.vy   = ofs->vy;
    work->obj0.pos.vz   = ofs->vz;
    work->obj0.key      = 0x30023;
    work->obj0.radius   = 0x5DC;
    work->obj0.flags    = 1;
    Gp_LinkObj(2, &work->obj0);
    Gp_InitRec18Table(rec, 8, 0);
    work->obj0.flags &= 0x7FFF;

    parts = ((TmdObject*)parent->extra)->coords;
    if (slot != 0) {
        work->obj1.coord = &parts[6];
    } else {
        work->obj1.coord = &parts[12];
    }
    work->obj1.ctx.recs = work->rec1;
    work->obj1.pos.vx   = 0;
    work->obj1.pos.vy   = 0;
    work->obj1.pos.vz   = 0;
    work->obj1.key      = 0x30023;
    work->obj1.radius   = 0x320;
    work->obj1.flags    = 1;
    Gp_LinkObj(3, &work->obj1);
    Gp_InitRec18Table(work->rec1, 4, 0);
    work->obj1.flags &= 0x7FFF;

    parts2 = ((TmdObject*)parent->extra)->coords;
    if (slot != 0) {
        work->obj2.coord = &parts2[7];
    } else {
        work->obj2.coord = &parts2[13];
    }
    work->obj2.ctx.recs = work->rec1;
    work->obj2.pos.vx   = 0;
    work->obj2.pos.vy   = 0x190;
    work->obj2.pos.vz   = 0;
    work->obj2.key      = 0x30023;
    work->obj2.radius   = 0x4B0;
    work->obj2.flags    = 1;
    Gp_LinkObj(3, &work->obj2);
    Gp_InitRec18Table(work->rec1, 4, 0);
    work->field_1E0.spawnArgLo = 0x600;
    work->field_1E0.coord      = coord;
    work->field_1E0.spawnArgHi = 3;
    work->obj2.flags          &= 0x7FFF;
    func_actor_503500_80144238(arg0, 0);
    arg0->exitCallback = func_actor_503500_80143F78;
    arg0->state       += 1;
}

/// Sub-state 1 of the 0x224 enemy. Phase 0 hands the parent 0xC, or 0x12 when
/// `func_actor_503500_80135E04` accepts slot 4/5, and keeps the pick in
/// `field_223`. Phase 1 counts frames in `field_21A`: on frame 0x7A (0xC) or
/// 0x51 (0x12) it sets bit 0x8000 on `obj1` / `obj2` and plays 0x40230009 at
/// parent coordinate 6 or 12 (by `field_220`); on 0x90 / 0x60 it clears them,
/// and it moves on once `func_actor_503500_80136014` reports done.
void func_actor_503500_8014271C(Task* arg0)
{
    Actor503500Work224* work;
    GsCOORDINATE2*      coords;
    GsCOORDINATE2*      coord;
    s32                 side;
    s32                 anim;
    s16                 frame;

    work = (Actor503500Work224*)arg0->work;
    if (func_actor_503500_8013608C(arg0->parent) != 0) {
        func_actor_503500_80144238(arg0, 0);
        func_actor_503500_8013611C(arg0->spawnArg1);
        return;
    }
    side = 3;
    if (work->field_220 != 0) {
        side = 2;
    }
    switch (work->field_222) {
        case 0:
            anim = 0xC;
            if (func_actor_503500_80135E04(arg0->parent, side == 2 ? 5 : 4) != 0) {
                anim = 0x12;
            }
            func_actor_503500_80135FB4(arg0->parent, side, anim);
            work->field_223 = anim;
            work->field_222++;
            break;
        case 1:
            frame = ++work->field_21A;
            if (work->field_223 == 0xC) {
                switch (frame) {
                    case 0x7A:
                        work->obj1.flags |= 0x8000;
                        work->obj2.flags |= 0x8000;
                        coords            = ((TmdObject*)arg0->parent->extra)->coords;
                        if (work->field_220 != 0) {
                            coord = &coords[6];
                        } else {
                            coord = &coords[12];
                        }
                        SndEvt_EnqueueType6(0x40230009, (s8)Gp_GetObjPan(coord),
                                            (s8)(gpGetObjDepth(coord) / 2));
                        break;
                    case 0x90:
                        work->obj1.flags &= 0x7FFF;
                        work->obj2.flags &= 0x7FFF;
                        break;
                }
            } else {
                switch (frame) {
                    case 0x51:
                        work->obj1.flags |= 0x8000;
                        work->obj2.flags |= 0x8000;
                        coords            = ((TmdObject*)arg0->parent->extra)->coords;
                        if (work->field_220 != 0) {
                            coord = &coords[6];
                        } else {
                            coord = &coords[12];
                        }
                        SndEvt_EnqueueType6(0x40230009, (s8)Gp_GetObjPan(coord),
                                            (s8)(gpGetObjDepth(coord) / 2));
                        break;
                    case 0x60:
                        work->obj1.flags &= 0x7FFF;
                        work->obj2.flags &= 0x7FFF;
                        break;
                }
            }
            if (func_actor_503500_80136014(arg0->parent, side) != 0) {
                work->obj1.flags &= 0x7FFF;
                work->obj2.flags &= 0x7FFF;
                work->field_222++;
            }
            break;
        default:
            func_actor_503500_80144238(arg0, 0);
            break;
    }
}

/// Death state of the 0x224 enemy, the counterpart of the 0x160 enemy's
/// `func_actor_503500_80137678`: phase 0 unlinks the enemy node and clears the
/// 16.16 `rot` / `vel` / `pos`; phase 1 sprays effects for 31 frames, then
/// queues the side's CD load, re-parents the coordinate onto the view in world
/// space, copies the parent's parts 6/7 (or 12/13, by `field_220`) into its own
/// parts 2/3 and points `vel` along the coordinate; phase 2 plays 0x40230004; phases 3/4 accelerate `vel.vy`, and
/// phase 4 fires the light and sound cues on frames 10/30 and leaves on frame
/// 40. Every frame the angles and position are applied to the coordinate, and
/// every fourth frame sprays two effects from `D_actor_503500_80171564`.
void func_actor_503500_80142980(Task* arg0)
{
    SVECTOR              rot;
    Actor503500IdentMat  m;
    s8                   param1[8];
    s8                   param2[8];
    Actor503500MatWords* ident;
    Actor503500Work224*  work;
    GpEnemy*             enemy;
    GsCOORDINATE2*       coord;
    GsCOORDINATE2*       src;
    TmdObject*           tmd;
    s32*                 in;
    s32*                 out;
    s32*                 in2;
    s32*                 out2;
    s32                  side;
    s32                  i;
    s32                  j;
    s32                  k;

    work  = (Actor503500Work224*)arg0->work;
    enemy = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->coords;
    switch (work->field_222) {
        case 0:
            work->obj0.flags &= 0x7FFF;
            enemy->recs       = 0;
            Gp_UnlinkNode(&enemy->node);
            func_actor_503500_80135CE8(arg0->parent, arg0->spawnArg1);
            work->field_218 = 0;
            ((void (*)(s32))Gp_IncStateF0Ref)(0);
            Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
            func_actor_503500_80136048(arg0->parent);
            enemy->reactionFlags &= 0xF0;
            work->rot.vx.w        = 0;
            work->rot.vy.w        = 0;
            work->rot.vz.w        = 0;
            work->vel.vx.w        = 0;
            work->vel.vy.w        = 0;
            work->vel.vz.w        = 0;
            work->pos.vx.w        = 0;
            work->pos.vy.w        = 0;
            work->pos.vz.w        = 0;
            work->obj1.flags     &= 0x7FFF;
            work->obj2.flags     &= 0x7FFF;
            work->field_222++;
            break;
        case 1:
            side = work->field_220;
            if (func_actor_503500_801360BC(arg0->spawnArg1, 1) != 0) {
                rot.vx = side != 0 ? -300 : 300;
                rot.vy = (u32)(rcos((s16)work->field_21A << 7) * 375) >> 10;
                rot.vz = (u32)(rsin((s16)work->field_21A << 7) * 375) >> 10;
                Gp_SpawnEff(0x60055, coord->sub->sub, 0x01101600, &rot);
            }
            if ((s16)++work->field_21A >= 0x1F) {
                param1[3] = 4;
                param1[2] = 0x30;
                param2[0] = 1;
                param2[1] = 0;
                if (side != 0) {
                    param1[0] = 0x14;
                    param2[2] = -1;
                    param2[3] = 4;
                    MoveImage(&D_actor_503500_8017155C, 0, 0xFD);
                } else {
                    param1[0] = 0x13;
                    param2[2] = -1;
                    param2[3] = 2;
                }
                work->field_21E = CdCmd_Enqueue(0x21, (u8*)param1, (u8*)param2);
                Gp_ComposeParentWorld(coord, &m.mat, &rot);
                in  = (s32*)&m;
                out = (s32*)&coord->coord;
                for (k = 0; k < 4; k++) {
                    *out++ = *in++;
                }
                coord->coord.m[2][2] = m.mat.m[2][2];
                coord->coord.t[0]    = rot.vx;
                coord->coord.t[1]    = rot.vy;
                coord->coord.t[2]    = rot.vz;
                src                  = ((TmdObject*)arg0->parent->extra)->coords;
                if (side != 0) {
                    src += 4;
                } else {
                    src += 10;
                }
                for (i = 2; i < 4; i++) {
                    out2 = (s32*)&coord[i].coord;
                    in2  = (s32*)&src[i].coord;
                    for (k = 0; k < 4; k++) {
                        *out2++ = *in2++;
                    }
                    coord[i].coord.m[2][2] = src[i].coord.m[2][2];
                }
                tmd         = arg0->extra;
                tmd->flags &= 0xFF7B;
                Tmd_AllocBuffers(tmd);
                rot.vx = 0;
                rot.vy = 0;
                rot.vz = 0;
                if (side != 0) {
                    func_actor_503500_80135E20(arg0->parent, 5, &rot);
                    work->vel.vx.w = -0x100000;
                    work->vel.vy.w = 0;
                    work->vel.vz.w = 0;
                } else {
                    func_actor_503500_80135E20(arg0->parent, 0xB, &rot);
                    work->vel.vx.w = 0x100000;
                    work->vel.vy.w = 0;
                    work->vel.vz.w = 0;
                }
                ApplyMatrixLV(&coord->coord, (VECTOR*)&work->vel, (VECTOR*)&work->vel);
                coord->sub = &gGfxViewCoord;
                Gp_UpdateCoord(coord);
                work->field_21A = 0;
                work->field_222++;
            }
            break;
        case 2:
            ((TmdObject*)arg0->extra)->otOffset = 0x11;
            SndEvt_EnqueueType6(0x40230004, (s8)Gp_GetObjPan(coord),
                                (s8)(gpGetObjDepth(coord) / 2));
            work->field_222++;
            break;
        case 3:
            if (func_actor_503500_801360BC(arg0->spawnArg1, 2) != 0) {
                if (work->field_220 != 0) {
                    Gp_SpawnEff(0x60055, coord, 0x01101C00, &D_actor_503500_80171594);
                    work->rot.vz.w -= 0x2000;
                } else {
                    Gp_SpawnEff(0x60055, coord, 0x01101C00, &D_actor_503500_8017158C);
                    work->rot.vz.w += 0x2000;
                }
            }
            work->vel.vy.w += 0x8000;
            if ((s16)++work->field_21A >= 0x1F) {
                work->field_21A = 0;
                work->field_222++;
            }
            break;
        case 4:
            work->vel.vy.w += 0x8000;
            switch ((s16)work->field_21A) {
                case 10:
                    ((TmdObject*)arg0->extra)->flags |= 2;
                    Gp_SetLightMode((GpObj4C*)enemy, 1);
                    SndEvt_EnqueueType6(0xD, (s8)Gp_GetObjPan(coord),
                                        (s8)(gpGetObjDepth(coord) / 2));
                    break;
                case 30:
                    Gp_SetLightMode((GpObj4C*)enemy, 2);
                    break;
                case 40:
                    SndEvt_EnqueueType7(0xD, 1);
                    arg0->state++;
                    break;
            }
            work->field_21A++;
            break;
        default:
            arg0->state++;
            break;
    }
    rot.vx          = work->rot.vx.w >> 16;
    rot.vy          = work->rot.vy.w >> 16;
    rot.vz          = work->rot.vz.w >> 16;
    m.ident.m00_m01 = 0x1000;
    m.ident.m02_m10 = 0;
    ident           = &m.ident;
    ident->m11_m12  = 0x1000;
    m.ident.m20_m21 = 0;
    ident->m22      = 0x1000;
    RotMatrix(&rot, &m.mat);
    gte_SetRotMatrix(&coord->coord);
    gte_ldclmv(&m.mat);
    gte_rtir();
    gte_stclmv(&coord->coord);
    gte_ldclmv((char*)&m.mat + 2);
    gte_rtir();
    gte_stclmv((char*)&coord->coord + 2);
    gte_ldclmv((char*)&m.mat + 4);
    gte_rtir();
    gte_stclmv((char*)&coord->coord + 4);
    work->pos.vx.w    += work->vel.vx.w;
    work->pos.vy.w    += work->vel.vy.w;
    work->pos.vz.w    += work->vel.vz.w;
    coord->coord.t[0] += work->pos.vx.h.hi;
    coord->coord.t[1] += work->pos.vy.h.hi;
    coord->coord.t[2] += work->pos.vz.h.hi;
    work->pos.vx.w     = (u16)work->pos.vx.w;
    work->pos.vy.w     = (u16)work->pos.vy.w;
    work->pos.vz.w     = (u16)work->pos.vz.w;
    coord->flg         = 0;
    if (func_actor_503500_801360BC(arg0->spawnArg1, 3) != 0) {
        if (!(D_80070F70 & 3)) {
            for (i = 0, j = 0; i < 2; i++) {
                Gp_SpawnEff(0x60070, &((TmdObject*)arg0->extra)->coords[i], 0x81018A00, &D_actor_503500_80171564[j]);
                j++;
                j = j < 5 ? j : 0;
            }
        }
    }
    if (gGameSession->eventState != 0 && gGameSession->viewReady != 0 && work->field_222 >= 2) {
        SndEvt_EnqueueType7(0xD, 1);
        arg0->state = 2;
    }
}

/// Hit handler of the 0x224 enemy, one pass over `count` records. Duplicate
/// ids and anything but a type-2 hit are skipped, as is the whole record while
/// the stun countdown `field_218` runs. Damage is scaled by distance to the
/// attacker, quadrupled on a critical roll, and clamped so the health floors at
/// 0 - which also arms the 0x258-frame `field_21C` recovery. Id kinds 4/6 on a
/// dead enemy, and ids flagged in `D_actor_503500_80171490`, call
/// `func_actor_503500_80144238(arg0, 2)` and mark the hit as kind 2. The hit effect is placed
/// 0x5DC along the impact direction in the model's frame, offset by the side
/// vector. Once `field_21C` runs out the health is refilled to a tenth of the
/// spawn record's maximum.
void func_actor_503500_801431EC(Task* arg0, GpObj* arg1, GpRec18* arg2, s32 arg3)
{
    VECTOR              d;
    SVECTOR             pos;
    MATRIX              mtx;
    MATRIX              rot;
    Actor503500Work224* work;
    GpEnemy*            enemy;
    GsCOORDINATE2*      coord;
    GsCOORDINATE2*      src;
    s16                 stun;
    s16                 hp;
    u32                 id;
    s32                 dmg;
    s32                 crit;
    s32                 scale;
    s32                 i;
    s32                 j;

    enemy = arg0->spawnArg2;
    work  = (Actor503500Work224*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    for (i = 0; i < arg3; i++) {
        id = arg2[i].key;
        for (j = 0; j < i; j++) {
            if (arg2[j].key == id) {
                goto next;
            }
        }
        if ((id & 0xFFFF0000) == 0x10000) {
            continue;
        }
        if ((id & 0xFFFF0000) != 0x20000) {
            continue;
        }
        if (work->field_218 != 0) {
            continue;
        }
        src = ((TmdObject*)Gp_ActorSlots[(id >> 7) & 1]->extra)->coords;
        Gp_ComposeParentWorld(coord, &mtx, &pos);
        d.vx = src->coord.t[0] - pos.vx;
        d.vy = src->coord.t[1] - pos.vy;
        d.vz = src->coord.t[2] - pos.vz;
        crit = 0;
        dmg  = Gp_ComputeDamage(id, SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz), crit, crit);
        if (Gp_RollEnemyChance(enemy, id, crit) != 0) {
            dmg *= 4;
            crit = 1;
        }
        func_800E2C78((GpObj40*)enemy, id, dmg, 0);
        hp        = enemy->hp - dmg;
        enemy->hp = hp;
        if (hp <= 0) {
            enemy->hp = 0;
            dmg      += hp;
            if (work->field_21C == 0) {
                work->field_21C = 0x258;
            }
        }
        func_800DA6E8(&enemy->node, dmg, 0);
        switch (Gp_GetIdParam0(id) & 0xFFFF) {
            case 0:
            case 5:
            case 7:
            case 8:
            case 9:
                break;
            case 1:
                Gp_SetObjFlag1((GpObj4C*)enemy);
                break;
            case 2:
                Gp_SetObjFlag2((GpObj5D*)enemy, id, 0);
                break;
            case 3:
                Gp_SetObjFlag4((GpObj5C*)enemy, id, 0);
                break;
            case 4:
            case 6:
                if (enemy->hp <= 0) {
                    func_actor_503500_80144238(arg0, 2);
                    crit = 2;
                }
                break;
        }
        if ((id & 0x8000) && D_actor_503500_80171490[id & 0x7F] != 0) {
            func_actor_503500_80144238(arg0, 2);
            crit = 2;
        }
        TRANSPOSE_ROT(&coord->workm, &rot);
        pos.vx = arg2[i].point.vx - coord->workm.t[0];
        pos.vy = arg2[i].point.vy - coord->workm.t[1];
        pos.vz = arg2[i].point.vz - coord->workm.t[2];
        scale  = 0x5DC000 / SquareRoot0(pos.vx * pos.vx + pos.vy * pos.vy + pos.vz * pos.vz);
        pos.vx = pos.vx * scale / 4096;
        pos.vy = pos.vy * scale / 4096;
        pos.vz = pos.vz * scale / 4096;
        gte_SetRotMatrix(&rot);
        gte_ldv0(&pos);
        gte_rtv0();
        gte_stsv(&pos);
        pos.vx += D_actor_503500_80171480[work->field_220].vx;
        pos.vy += D_actor_503500_80171480[work->field_220].vy;
        pos.vz += D_actor_503500_80171480[work->field_220].vz;
        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, &pos, &work->field_1E0);
        if (crit != 0) {
            Gp_SpawnEff(0x6009C, coord, (crit == 2) * 2, &pos);
        }
        stun = Gp_GetIdParam2(id);
        if (work->field_218 < stun) {
            work->field_218 = stun;
        }
    next:;
    }
    if (--work->field_21C == 0) {
        enemy->hp = D_actor_503500_8016E7EC[arg0->spawnArg1].hpMax / 10;
    } else if (work->field_21C < 0) {
        work->field_21C = 0;
    }
}

/// Scans the 0x224 enemy's shared record table. For each record whose
/// `key` high half is 1 - unless the player task (`gameGetPtrSlot(3)`)
/// is in mode 2 or answers message 0x3F8 - copies the parent's root rotation
/// into `field_40` and turns it by +/-0x5DC with `func_8004BFF8` (sign from
/// `field_220`), then takes the world position of parent coordinate 5 or 11
/// into the player's frame. The sign of its z picks the 0x3FF payload and is
/// passed to the task spawned from `D_actor_503500_8017146C`; message 0x3F9
/// carries the enemy's packed pair, and sound 7 plays at the player.
void func_actor_503500_801437D0(Task* arg0, GpRec18* rec, s32 count)
{
    SVECTOR             vec;
    MATRIX              world;
    MATRIX              rot;
    GpEnemy*            enemy;
    Actor503500Work224* work;
    GsCOORDINATE2*      coord;
    Task*               player;
    GsCOORDINATE2*      pcoord;
    s32*                src;
    s32*                dst;
    s32                 i;
    s32                 j;
    s32                 side;
    s32                 pan;

    enemy = arg0->spawnArg2;
    work  = (Actor503500Work224*)arg0->work;
    for (i = 0; i < count; i++) {
        if ((rec[i].key & 0xFFFF0000) == 0x10000) {
            player = gameGetPtrSlot(3);
            pcoord = ((TmdObject*)player->extra)->coords;
            if (((GameActor*)player->work)->field_954 != 2 &&
                Gp_DispatchMsg(player, 0x3F8, (s32)&D_actor_503500_80171544, 0) == 0) {
                coord = ((TmdObject*)arg0->parent->extra)->coords;
                src   = (s32*)&coord->coord;
                dst   = (s32*)&work->field_40;
                for (j = 0; j < 4; j++) {
                    *dst++ = *src++;
                }
                work->field_40.m[2][2] = coord->coord.m[2][2];
                if (work->field_220 != 0) {
                    func_8004BFF8(0x5DC, &work->field_40);
                    coord = &((TmdObject*)arg0->parent->extra)->coords[5];
                } else {
                    func_8004BFF8(-0x5DC, &work->field_40);
                    coord = &((TmdObject*)arg0->parent->extra)->coords[11];
                }
                Gp_ComposeParentWorld(coord, &world, &vec);
                TRANSPOSE_ROT(&pcoord->coord, &rot);
                gte_SetRotMatrix(&rot);
                gte_ldv0(&vec);
                gte_rtv0();
                gte_stsv(&vec);
                side = vec.vz >= 0;
                Gp_DispatchMsg(player, 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 0), 0);
                Gp_DispatchMsg(player, 0x3FF, (s32)&D_actor_503500_801714E0[side], 0);
                Task_SpawnFromTable(&D_actor_503500_8017146C, 0, side, (s32)&work->field_40);
                Gp_StateC08.field_6 |= 1;
                pan                  = (s8)Gp_GetObjPan(pcoord);
                SndEvt_EnqueueType6(7, pan, (s8)(gpGetObjDepth(pcoord) / 2));
            }
        }
    }
}

/// Knock-back task spawned from `D_actor_503500_8017146C` by
/// `func_actor_503500_801437D0`, with the hit side in `spawnArg1` and the
/// enemy's turned rotation in `spawnArg2`. State 0 copies that rotation, starts
/// the push at speed 0x1000000 and shakes the camera for 8 frames; state 1 moves
/// the player by the rotated speed through message 0x3FE, decaying it by
/// 0x30000 a frame, and after 20 frames at rest moves on (or ends at -1 when the
/// player has no HP left). States 2-4 wait out message 0x3ED between the two
/// 0x3FF payloads and the closing 0x3F1. Frame 0x11 plays sound 0x54300002 at
/// the player.
void func_actor_503500_80143AC0(Task* arg0)
{
    VECTOR             vec;
    Actor503500Msg3FE  msg;
    Actor503500Work38* work;
    Task*              player;
    GsCOORDINATE2*     coord;
    s32*               src;
    s32*               dst;
    s32                i;
    s32                pan;
    s32                next;
    s32                shake;

    work   = &D_actor_503500_80178F10;
    player = gameGetPtrSlot(3);
    if (D_801153F4 != 0) {
        return;
    }
    switch (arg0->state) {
        case 0:
            if (D_80073BA0 <= 0) {
                taskKill(arg0);
                return;
            }
            Mem_Set(work, 0, sizeof(Actor503500Work38));
            work->speed    = 0x1000000;
            work->pos.vx.w = 0;
            work->pos.vy.w = 0;
            work->pos.vz.w = 0;
            src            = (s32*)arg0->spawnArg2;
            dst            = (s32*)&work->rot;
            for (i = 0; i < 4; i++) {
                *dst++ = *src++;
            }
            work->rot.m[2][2] = ((MATRIX*)arg0->spawnArg2)->m[2][2];
            Gp_SpawnScript18((s32)D_actor_503500_8017159C, (s32)D_actor_503500_801715A4);
            work->field_36 = 8;
            // An s32 temp: passed straight to the s8 parameter, the masked
            // expression is shortened into a byte load of the frame counter.
            shake = (D_80070F70 ^ 1) & 1;
            Display_ClampField126(shake);
            arg0->state++;
        case 1:
            vec.vx = 0;
            vec.vy = 0;
            vec.vz = work->speed;
            ApplyMatrixLV(&work->rot, &vec, &vec);
            work->pos.vx.w += vec.vx;
            work->pos.vy.w += vec.vy;
            work->pos.vz.w += vec.vz;
            msg.field_10    = 1;
            msg.field_12    = 1;
            msg.x           = work->pos.vx.h.hi;
            msg.y           = work->pos.vy.h.hi;
            msg.z           = work->pos.vz.h.hi;
            if (Gp_DispatchMsg(player, 0x3FE, (s32)&msg, 0) != 0) {
                work->speed = 0;
            }
            work->pos.vx.w = (u16)work->pos.vx.w;
            work->pos.vy.w = (u16)work->pos.vy.w;
            work->pos.vz.w = (u16)work->pos.vz.w;
            work->speed   -= 0x30000;
            if (work->speed < 0) {
                work->speed = 0;
                if (++work->field_34 > 20) {
                    // The -1 arm first: reorg inverts the branch around it and
                    // leaves the `li` in the delay slot, sharing $v0 with the load.
                    if (D_80073BA0 <= 0) {
                        next = -1;
                    } else {
                        next = arg0->state + 1;
                    }
                    arg0->state = next;
                }
            }
            if (work->field_36 > 0) {
                shake = (D_80070F70 ^ 1) & 1;
                Display_ClampField126(shake);
                work->field_36--;
            } else {
                Display_ClampField126(0);
            }
            break;
        case 2:
            if (Gp_DispatchMsg(player, 0x3ED, 0, 0) == 0) {
                Gp_DispatchMsg(player, 0x3FF, (s32)&D_actor_503500_80171508[arg0->spawnArg1], 0);
                arg0->state++;
            }
            break;
        case 3:
            if (Gp_DispatchMsg(player, 0x3ED, 0, 0) == 0) {
                D_actor_503500_801714DC =
                    Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[Mc_SaveData.characterId - 1] + Player_Status.weapon]
                        ->field_1C;
                Gp_DispatchMsg(player, 0x3FF, (s32)&D_actor_503500_80171530, 0);
                arg0->state++;
            }
            break;
        case 4:
            if (Gp_DispatchMsg(player, 0x3ED, 0, 0) == 0) {
                Gp_DispatchMsg(player, 0x3F1, 2, 0);
                taskKill(arg0);
            }
            break;
    }
    if (++arg0->killCountdown == 0x11) {
        coord = ((TmdObject*)player->extra)->coords;
        pan   = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(0x54300002, pan, (s8)gpGetObjDepth(coord));
    }
}

void func_actor_503500_80143EB4(Task* arg0)
{
    GpEnemy*   enemy;
    TmdObject* tmd;
    s32        mode;

    enemy = arg0->spawnArg2;
    mode  = D_801153F4;
    tmd   = arg0->extra;
    switch (mode) {
        case 1:
            if (!(tmd->flags & 0x80)) {
                func_actor_503500_80143FFC(arg0);
            }
            break;
        case 2:
            tmd->flags        |= 0x80;
            enemy->node.flags |= 1;
            break;
        default:
            if (enemy->reactionFlags != 0) {
                func_actor_503500_80144098(arg0, mode, enemy);
            }
            func_actor_503500_80143FFC(arg0);
            func_actor_503500_80144004(arg0);
            func_actor_503500_801440F0(arg0);
            break;
    }
}

void func_actor_503500_80143F78(Task* arg0)
{
    GpEnemy*            enemy;
    Actor503500Work224* work;

    enemy = arg0->spawnArg2;
    work  = (Actor503500Work224*)arg0->work;
    func_actor_503500_8013611C(arg0->spawnArg1);
    ((GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords)->sub = &gGfxViewCoord;
    Gp_UnlinkObj(&work->obj0);
    Gp_UnlinkObj(&work->obj1);
    Gp_UnlinkObj(&work->obj2);
    enemy->recs = 0;
    arg0->work  = NULL;
    Gp_DestroyEnemy(enemy, arg0);
}

void func_actor_503500_80143FFC(Task* arg0)
{
}

void func_actor_503500_80144004(Task* arg0)
{
    Actor503500Work224* work;
    s16                 timer;

    work = (Actor503500Work224*)arg0->work;
    if (work->field_218 != 0) {
        timer           = (u16)work->field_218 - 1;
        work->field_218 = timer;
        if (timer < 0) {
            work->field_218 = 0;
        }
    }
    if (func_actor_503500_80136208() == 0) {
        func_actor_503500_801431EC(arg0, &work->obj0, work->rec0, 8);
        func_actor_503500_801437D0(arg0, work->rec1, 4);
    }
    Gp_ClearRec18Occupied(work->rec0);
    Gp_ClearRec18Occupied(work->rec1);
}

void func_actor_503500_80144098(Task* arg0, s32 arg1, GpEnemy* arg2)
{
    GpEnemy* enemy = arg0->spawnArg2;

    if (enemy->reactionFlags & 1) {
        enemy->reactionFlags &= ~1;
    }
    if (enemy->reactionFlags & 2) {
        enemy->reactionFlags &= ~2;
    }
    if (enemy->reactionFlags & 0xC) {
        enemy->reactionFlags &= ~0xC;
    }
}

void func_actor_503500_801440F0(Task* arg0)
{
    switch (((Actor503500Work*)arg0->work)->field_221) {
        case 0:
            func_actor_503500_8014418C(arg0);
            break;
        case 1:
            func_actor_503500_8014271C(arg0);
            break;
        case 2:
            func_actor_503500_80142980(arg0);
            break;
        case 3:
            func_actor_503500_801441E8(arg0);
            break;
    }
}

void func_actor_503500_8014418C(Task* arg0)
{
    switch (arg0->killCountdown) {
        case 2:
            func_actor_503500_80144238(arg0, 1);
            arg0->killCountdown = 0;
            break;
        case 8:
            func_actor_503500_80144238(arg0, 3);
            arg0->killCountdown = 0;
            break;
    }
}

void func_actor_503500_801441E8(Task* arg0)
{
    Actor503500Work* work;

    work            = arg0->work;
    work->field_7E |= 0x8000;
    Gp_LinkNode(&((GpEnemy*)arg0->spawnArg2)->node);
    func_actor_503500_80144238(arg0, 0);
}

void func_actor_503500_80144238(Task* arg0, s32 arg1)
{
    Actor503500Work224* work;

    work                = (Actor503500Work224*)arg0->work;
    work->field_221     = arg1;
    work->field_222     = 0;
    work->field_21A     = 0;
    arg0->killCountdown = 0;
    func_actor_503500_80135F9C(arg0->parent, arg0->spawnArg1, arg1 != 0);
    work->obj1.flags &= 0x7FFF;
    work->obj2.flags &= 0x7FFF;
    SndEvt_EnqueueType7(0x40230009, 1);
}

void func_actor_503500_801442A8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_80132178;
    sp.funcs[task->state](task);
}

/// `Task::state` handlers `func_actor_503500_80144890` dispatches through.
const TaskFuncTable3 D_actor_503500_801321DC = {
    {
        func_actor_503500_80144300,
        func_actor_503500_801446E4,
        func_actor_503500_8014473C,
    },
};

void func_actor_503500_80144300(Task* arg0)
{
    Actor503500WorkC0* work;
    GsCOORDINATE2*     coord;
    GpRec18*           rec;
    GpEffWork*         eff;
    Task*              child;
    GpMtxWords*        m;
    VECTOR             v;
    s32                pan;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work = (TaskIdMap*)work;

    work->field_84.vx.w = coord->coord.t[0] << 16;
    work->field_84.vy.w = coord->coord.t[1] << 16;
    work->field_84.vz.w = coord->coord.t[2] << 16;
    work->field_94.vx   = work->field_84.vx.w;
    work->field_B8      = 0x1000;
    work->field_94.vy   = work->field_84.vy.w;
    work->field_94.vz   = work->field_84.vz.w;

    if (arg0->spawnArg2 != NULL) {
        v.vx = 0;
        v.vy = 0;
        v.vz = (s32)arg0->spawnArg2;
        ApplyMatrixLV(&coord->coord, &v, &work->field_A4);
    } else {
        m     = (GpMtxWords*)&coord->coord;
        m->w0 = 0x1000;
        m->w1 = 0;
        m->w2 = 0x1000;
        m->w3 = 0;
        m->h4 = 0x1000;
    }
    rec = work->rec;

    work->obj.coord    = coord;
    work->obj.ctx.recs = rec;
    work->obj.pos.vx   = D_actor_503500_801715AC.vx;
    work->obj.pos.vy   = D_actor_503500_801715AC.vy;
    work->obj.pos.vz   = D_actor_503500_801715AC.vz;
    work->obj.key      = Gp_PackPair(D_actor_503500_8016E7CC[0], arg0->spawnArg1);
    work->obj.radius   = 0x12C;
    work->obj.flags    = 1;
    Gp_LinkObj(3, &work->obj);
    Gp_InitRec18Table(rec, 4, 0);
    work->obj.flags |= 0xC000;

    eff = Gp_SpawnEff(0x60189, coord, 0, NULL);
    if (eff == NULL) {
        func_actor_503500_8014473C(arg0);
        return;
    }
    child          = eff->task;
    work->field_80 = child;
    Task_Reparent(arg0, child);
    pan = (s8)Gp_GetObjPan(coord);
    SndEvt_EnqueueType6(0x40230005, pan, (s8)(gpGetObjDepth(coord) / 2));
    func_actor_503500_80137290(1);
    arg0->exitCallback = func_actor_503500_8014473C;
    arg0->state       += 1;
}

void func_actor_503500_80144520(Task* arg0)
{
    Actor503500WorkC0* work;
    GsCOORDINATE2*     coord;

    work  = (Actor503500WorkC0*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (work->field_BE != 0) {
        work->field_80->spawnArg1 = 2;
        work->field_BC            = -1;
    }
    switch (work->field_BC) {
        case 0:
            work->field_A4.vy += 9.8 * 0x10000;
            if (work->field_B4 != 0) {
                work->obj.radius  = 0x258;
                work->field_A4.vx = 0;
                work->field_A4.vy = 0;
                work->field_A4.vz = 0;
                work->field_BF    = 1;
                work->field_B4    = 0;
                work->field_BA    = 0;
                work->field_BC++;
            } else {
                work->field_BA++;
                if ((s16)work->field_BA >= 0x3D) {
                    work->field_BC = -1;
                }
            }
            break;
        case 1:
            work->field_BA++;
            if ((s16)work->field_BA >= 6) {
                work->field_80->spawnArg1 = 2;
                work->field_BA            = 0;
                work->obj.flags          &= 0x7FFF;
                work->field_BC++;
            }
            break;
        default:
            arg0->state++;
            break;
    }
    work->field_84.vx.w += work->field_A4.vx;
    work->field_84.vy.w += work->field_A4.vy;
    work->field_84.vz.w += work->field_A4.vz;
    coord->coord.t[0]    = work->field_84.vx.h.hi;
    coord->coord.t[1]    = work->field_84.vy.h.hi;
    coord->coord.t[2]    = work->field_84.vz.h.hi;
}

void func_actor_503500_801446E4(Task* arg0)
{
    GsCOORDINATE2* coord;
    s32            state;

    coord = ((TmdObject*)arg0->extra)->coords;
    state = D_801153F4;
    if (state < 3) {
        if (state != 0) {
            return;
        }
    }
    coord->flg = 0;
    func_actor_503500_80144778(arg0);
    func_actor_503500_80144520(arg0);
}

void func_actor_503500_8014473C(Task* arg0)
{
    func_actor_503500_801372AC(1);
    Gp_UnlinkObj(&((Actor503500ObjWork*)arg0->work)->obj);
    taskKill(arg0);
}

void func_actor_503500_80144778(Task* arg0)
{
    GpDeltaScratch     delta;
    Actor503500WorkC0* work;
    GpRec18*           rec;
    s32                result;
    s32                i;

    work = (Actor503500WorkC0*)arg0->work;
    rec  = work->rec;
    if (work->field_BF == 0) {
        result         = func_800E0C10(rec, &delta, 4, NULL);
        work->field_B4 = result;
        switch (result) {
            case 0:
                break;
            case 1:
                work->field_84.vx.w += delta.vx.w;
                work->field_84.vy.w += delta.vy.w;
                work->field_84.vz.w += delta.vz.w;
                break;
            case 2:
                work->field_84.vx.w = work->field_94.vx;
                work->field_84.vy.w = work->field_94.vy;
                work->field_84.vz.w = work->field_94.vz;
                break;
        }
    }
    for (i = 0; i < 4; i++) {
        if ((rec[i].key & 0xFFFF0000) == 0x10000) {
            work->field_BE = 1;
        }
    }
    Gp_ClearRec18Occupied(work->rec);
}

void func_actor_503500_80144890(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_801321DC;
    sp.funcs[task->state](task);
}

/// `Task::state` handlers `func_actor_503500_80144E34` dispatches through.
const TaskFuncTable3 D_actor_503500_801321E8 = {
    {
        func_actor_503500_801448E8,
        func_actor_503500_80144D50,
        func_actor_503500_80144DA8,
    },
};

void func_actor_503500_801448E8(Task* arg0)
{
    Actor503500WorkB4* work;
    GsCOORDINATE2*     coord;
    GpRec18*           rec;
    GpEffWork*         eff;
    Task*              child;
    GpMtxWords*        m;
    s32                pan;
    s32                pan2;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    arg0->work = (TaskIdMap*)work;

    work->field_84.vx = coord->coord.t[0] << 16;
    work->field_84.vy = coord->coord.t[1] << 16;
    work->field_84.vz = coord->coord.t[2] << 16;
    work->field_94.vx = work->field_84.vx;
    work->field_AC    = 0x1000;
    work->field_94.vy = work->field_84.vy;
    work->field_94.vz = work->field_84.vz;

    if (arg0->spawnArg2 != NULL) {
        work->field_A8 = (s32)arg0->spawnArg2;
    } else {
        m              = (GpMtxWords*)&coord->coord;
        m->w0          = 0x1000;
        m->w1          = 0;
        m->w2          = 0x1000;
        m->w3          = 0;
        m->h4          = 0x1000;
        work->field_A8 = 0x100000;
    }
    rec = work->rec;

    work->obj.coord    = coord;
    work->obj.ctx.recs = rec;
    work->obj.pos.vx   = D_actor_503500_801715B4.vx;
    work->obj.pos.vy   = D_actor_503500_801715B4.vy;
    work->obj.pos.vz   = D_actor_503500_801715B4.vz;
    work->obj.key      = Gp_PackPair(D_actor_503500_8016E7D0[0], arg0->spawnArg1);
    work->obj.radius   = 0x898;
    work->obj.flags    = 1;
    Gp_LinkObj(3, &work->obj);
    Gp_InitRec18Table(rec, 4, 0);
    work->obj.flags |= 0x8000;

    if (arg0->spawnArg1 == 0) {
        eff = Gp_SpawnEff(0x6018E, coord, 0, NULL);
        pan = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(0x40230008, pan, (s8)(gpGetObjDepth(coord) / 2));
    } else {
        eff  = Gp_SpawnEff(0x6018D, coord, 0, NULL);
        pan2 = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(0x40230007, pan2, (s8)(gpGetObjDepth(coord) / 2));
    }
    if (eff == NULL) {
        func_actor_503500_80144DA8(arg0);
        return;
    }
    child          = eff->task;
    work->field_80 = child;
    Task_Reparent(arg0, child);
    func_actor_503500_80137290(3);
    arg0->exitCallback = func_actor_503500_80144DA8;
    arg0->state       += 1;
}

void func_actor_503500_80144B40(Task* arg0)
{
    Actor503500WorkB4* work;
    GsCOORDINATE2*     coord;
    VECTOR             v;

    work  = (Actor503500WorkB4*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    switch (work->field_B0) {
        case 0:
            work->field_A8 -= 0x80000;
            if (work->field_A8 < 0x80000) {
                work->field_80->spawnArg1 = 2;
                work->field_B0++;
            }
            break;
        case 1:
            work->field_AE++;
            if (D_actor_503500_801715BC[arg0->spawnArg1] < work->field_AE) {
                work->field_AE   = 0;
                work->obj.flags &= 0x7FFF;
                work->field_B0++;
            }
            break;
        default:
            arg0->state++;
            break;
    }
    v.vx = 0;
    v.vy = 0;
    v.vz = work->field_A8;
    ApplyMatrixLV(&coord->coord, &v, &v);
    work->field_84.vx += v.vx;
    work->field_84.vy += v.vy - 0x10000;
    work->field_84.vz += v.vz;
    work->field_84.vx += v.vx;
    work->field_84.vy += v.vy;
    work->field_84.vz += v.vz;
    if (work->field_84.vx > 0x36B00000) {
        work->field_84.vx = 0x36B00000;
    } else if (work->field_84.vx < 0x7D00000) {
        work->field_84.vx = 0x7D00000;
    }
    if (work->field_84.vz > 0x32C80000) {
        work->field_84.vz = 0x32C80000;
    } else if (work->field_84.vz < 0x3E80000) {
        work->field_84.vz = 0x3E80000;
    }
    coord->coord.t[0] = work->field_84.vx >> 16;
    coord->coord.t[1] = work->field_84.vy >> 16;
    coord->coord.t[2] = work->field_84.vz >> 16;
}

void func_actor_503500_80144D50(Task* arg0)
{
    GsCOORDINATE2* coord;
    s32            state;

    coord = ((TmdObject*)arg0->extra)->coords;
    state = D_801153F4;
    if (state < 3) {
        if (state != 0) {
            return;
        }
    }
    coord->flg = 0;
    func_actor_503500_80144E10(arg0);
    func_actor_503500_80144B40(arg0);
}

void func_actor_503500_80144DA8(Task* arg0)
{
    func_actor_503500_801372AC(3);
    if (arg0->spawnArg1 == 0) {
        SndEvt_EnqueueType7(0x40230008, 1);
    } else {
        SndEvt_EnqueueType7(0x40230007, 1);
    }
    Gp_UnlinkObj(&((Actor503500ObjWork*)arg0->work)->obj);
    taskKill(arg0);
}

void func_actor_503500_80144E10(Task* arg0)
{
    Gp_ClearRec18Occupied(&((Actor503500ObjWork*)arg0->work)->rec);
}

void func_actor_503500_80144E34(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_503500_801321E8;
    sp.funcs[task->state](task);
}
