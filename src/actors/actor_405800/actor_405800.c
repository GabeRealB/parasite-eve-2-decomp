#include "common.h"
#include "main/stage.h"

#include "psyq/libgte.h"
#include "psyq/libgpu.h"
#include "psyq/inline_c.h"
#include "gte.h"

#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor.h"
#include "actors/actors_shared_80131fc8.h"
#include "actors/actors_shared_8013a0b0.h"

/// Psy-Q `RotMatrixY`, taking the angle as a `long`.
void func_8004BFF8(s32 angle, MATRIX* matrix);

/// Packed halfwords `func_actor_405800_80138514` reads as the desired root
/// translation. Only `x` and `z` are used; the middle halfword is kept so the
/// layout matches `Actor400600ViewPos`.
typedef struct Actor405800ViewPos {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
} Actor405800ViewPos;
STATIC_ASSERT_SIZEOF(Actor405800ViewPos, 0x6);

/// Status flags at + 0x83C of the work block, read through two widths: bit 0
/// as a halfword, then bits 0x102 as a word (`func_actor_405800_80137908`).
/// The high half is `field_83E`, the scale reset to 0x1000 on death.
typedef union Actor405800Flags83C {
    /* 0x0 */ u32 word;
    /* 0x0 */ u16 half;
    struct {
        /* 0x0 */ u16 pad;
        /* 0x2 */ s16 field_83E;
    } h;
} Actor405800Flags83C;
STATIC_ASSERT_SIZEOF(Actor405800Flags83C, 0x4);

/// 0x18-byte scratch from `G_SCRATCH_HEAD` used by `func_actor_405800_80133800`
/// to project the third model part's origin. `vec` is the zero vector fed to
/// RTPS through that part's `workm`; `sxy` is `gte_stsxy`, `p` is `gte_stdp`,
/// `flag` is `gte_stflg`, and `otz` is `gte_stszotz`.
typedef struct Actor405800PerspScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     sxy;
    /* 0x0C */ s32     p;
    /* 0x10 */ s32     flag;
    /* 0x14 */ s32     otz;
} Actor405800PerspScratch;
STATIC_ASSERT_SIZEOF(Actor405800PerspScratch, 0x18);

/// Per-actor work block for the `actor_405800` overlay.
///
/// `func_actor_405800_801334B8` allocates it with `memCalloc(0x89C, 0)` and
/// stores the result straight into the `Task::work` slot (0x1C), so the size
/// below is the allocation and not a guess: this actor reuses that pointer
/// field for its own work block and it is *not* a `TaskIdMap` here. It is the
/// only allocator in the overlay, so every function reaches the same block
/// with `(Actor405800Work*)task->work`.
///
/// `field_846` / `field_848` are the state and sub-state indices the handler
/// table walks and `field_842` is the per-state frame counter.
typedef struct Actor405800Work {
    /* 0x000 */ MATRIX              matrix_0;  // copy of the root coordinate's local matrix
    /* 0x020 */ MATRIX              matrix_20; // color matrix for the child models
    /* 0x040 */ MATRIX              matrix_40; // light matrix for the child models
    /* 0x060 */ byte                pad_60[0x10];
    /* 0x070 */ VECTOR              field_70;  // copy of the root coordinate's translation
    /* 0x080 */ u16                 field_80;  // pitch, see func_actor_405800_80139FC4
    /* 0x082 */ u16                 field_82;  // yaw, see func_actor_405800_80139FC4
    /* 0x084 */ u16                 field_84;  // roll, see func_actor_405800_80139FC4
    /* 0x086 */ byte                pad_86[2];
    /* 0x088 */ Actor405800ViewPos  field_88;
    /* 0x08E */ byte                pad_8E[2];
    /* 0x090 */ u16                 field_90; // spawn position X (low half)
    /* 0x092 */ u16                 field_92; // copied into field_86A on state entry
    /* 0x094 */ u16                 field_94; // spawn position Z (low half)
    /* 0x096 */ byte                pad_96[2];
    /* 0x098 */ u16                 field_98; // low half of the root coordinate's world X
    /* 0x09A */ s16                 field_9A;
    /* 0x09C */ u16                 field_9C; // low half of the root coordinate's world Z
    /* 0x09E */ byte                pad_9E[0xA];
    /* 0x0A8 */ SVECTOR             field_A8; // world point `func_actor_405800_801383CC` turns to face (it reads `vx` / `vz`)
    /* 0x0B0 */ GpAnimCtx           anim;     // slots 1..0x11 reset by func_actor_405800_80138224
    /* 0x0C4 */ GpAnimSlot          slots[0x12];
    /* 0x394 */ byte                pad_394[0x120];
    /* 0x4B4 */ GpObj               obj_4B4;    // collision node; unlinked on death
    /* 0x4D4 */ GpRec18             rec_4D4[8]; // obj_4B4 table
    /* 0x594 */ GpObj               obj_594;    // collision node; unlinked on death
    /* 0x5B4 */ GpRec18             rec_5B4[8]; // obj_594 table
    /* 0x674 */ GpObj               obj_674;    // collision node; unlinked on death
    /* 0x694 */ GpObj               obj_694;    // collision node; unlinked on death
    /* 0x6B4 */ GpObj               obj_6B4;    // collision node; unlinked on death
    /* 0x6D4 */ GpObj               obj_6D4;    // collision node; unlinked on death
    /* 0x6F4 */ GpRec18             rec_6F4[1]; // obj_6B4 / obj_674 table
    /* 0x70C */ GpRec18             rec_70C[1]; // obj_6D4 / obj_694 table
    /* 0x724 */ GpObj               obj_724;    // collision node; flags bit 0x4000 cleared by func_actor_405800_801379F8
    /* 0x744 */ GpActorD4Rec        rec_744;    // obj_724 payload (flags kind 3)
    /* 0x75C */ GpRec18             rec_75C[8]; // occupancy table behind rec_744
    /* 0x81C */ GpEffArg            eff_81C;    // fourth model part's coordinate
    /* 0x824 */ Task*               field_824;  // child task, killed on state exit
    /* 0x828 */ Task*               field_828;  // child task, killed on state exit
    /* 0x82C */ byte                pad_82C[4];
    /* 0x830 */ u16                 field_830;  // phase timer; lhu-incremented, compared as s16
    /* 0x832 */ s16                 field_832;
    /* 0x834 */ s16                 field_834;
    /* 0x836 */ s16                 field_836;
    /* 0x838 */ s16                 field_838;
    /* 0x83A */ s16                 field_83A; // nonzero: skip the field_895 / field_896 reset
    /* 0x83C */ Actor405800Flags83C flags_83C;
    /* 0x840 */ u16                 field_840; // LCG draw at spawn
    /* 0x842 */ u16                 field_842; // per-state frame counter
    /* 0x844 */ s16                 field_844; // cleared with field_842 on state entry
    /* 0x846 */ u16                 field_846; // state index
    /* 0x848 */ u16                 field_848; // sub-state index
    /* 0x84A */ s16                 field_84A;
    /* 0x84C */ s16                 field_84C;
    /* 0x84E */ s16                 field_84E;
    /* 0x850 */ s16                 field_850; // animation speed / step scale
    /* 0x852 */ s16                 field_852; // compared against 2000 to pick state 8 vs 0xD
    /* 0x854 */ u16                 field_854; // facing-delta halfword, range-checked vs 0x200..0xE00
    /* 0x856 */ u16                 field_856; // facing-delta halfword, range-checked vs 0x200..0xE00
    /* 0x858 */ s16                 field_858; // must be 1 for the pending field_85A transition
    /* 0x85A */ s16                 field_85A; // pending transition: 3 -> state 5, 5 -> state 0xF
    /* 0x85C */ s16                 field_85C; // countdown seeded by func_actor_405800_80137994, ticked by func_actor_405800_8013795C
    /* 0x85E */ s16                 field_85E; // countdown, ticked by func_actor_405800_8013795C
    /* 0x860 */ s16                 field_860; // model slot id handed to func_actor_405800_80138514
    /* 0x862 */ byte                pad_862[0x4];
    /* 0x866 */ s16                 field_866;
    /* 0x868 */ byte                pad_868[0x2];
    /* 0x86A */ s16                 field_86A; // seeded from field_92; set to -0x9C4 during the hop
    /* 0x86C */ byte                pad_86C[0x2];
    /* 0x86E */ s16                 field_86E; // animation request kind
    /* 0x870 */ s16                 field_870; // animation id now playing
    /* 0x872 */ s16                 field_872; // animation id
    /* 0x874 */ s16                 field_874; // sound step index
    /* 0x876 */ s16                 field_876; // damage cooldown
    /* 0x878 */ s16                 field_878;
    /* 0x87A */ s16                 field_87A;
    /* 0x87C */ u16                 field_87C; // down-counter
    /* 0x87E */ s16                 field_87E;
    /* 0x880 */ s16                 field_880;
    /* 0x882 */ s16                 field_882; // randomised hold, 0x5A .. 0x99 frames
    /* 0x884 */ s16                 field_884; // cleared on the state-entry path
    /* 0x886 */ byte                pad_886[0x2];
    /* 0x888 */ s8                  field_888; // damage-over-time reaction active
    /* 0x889 */ byte                pad_889;
    /* 0x88A */ u8                  field_88A;
    /* 0x88B */ s8                  field_88B;
    /* 0x88C */ u8                  field_88C;
    /* 0x88D */ s8                  field_88D;
    /* 0x88E */ s8                  field_88E;
    /* 0x88F */ s8                  field_88F;
    /* 0x890 */ u8                  field_890; // nonzero: allow the state-0xD transition when root X > 10000
    /* 0x891 */ u8                  field_891;
    /* 0x892 */ u8                  field_892; // distance mode: 0 none, 1 XZ, 2 XY
    /* 0x893 */ u8                  field_893;
    /* 0x894 */ byte                pad_894;
    /* 0x895 */ u8                  field_895;
    /* 0x896 */ u8                  field_896;
    /* 0x897 */ u8                  field_897; // 0: node.state.b.flags uses 4/5 instead of 0/1
    /* 0x898 */ u8                  field_898;
    /* 0x899 */ byte                pad_899[0x3];
} Actor405800Work;
STATIC_ASSERT_SIZEOF(Actor405800Work, 0x89C);

/* `D_800678F0` selects the model stream the next `Gp_SpawnEff` uses as the
 * source for the effect's own `TmdObject`.
 *
 * Storing to a bare `extern` pointer next to pointer-based struct traffic lets
 * GCC 2.8.1's `fixed_scalar_and_varying_struct_p` conclude the two cannot
 * alias, so the scheduler sinks the store past the loads that follow. The
 * one-element array is the remedy measured on `actor_400600`, where a
 * `SOFT_BARRIER()` was enough for a byte store but not for this pointer one. */
extern void* D_800678F0[1];

extern TaskDesc   D_actor_405800_801514B4;
extern GpPairSrcE D_actor_405800_801418FC;
extern u8         D_actor_405800_801513F8[];
extern u8         D_actor_405800_80151410[];
extern u8         D_actor_405800_8015149C[];
extern u8         D_actor_405800_801514D8[];

/* The records closing four of the overlay's model streams, selected through
   `D_800678F0`. */
extern TmdSource D_actor_405800_8013FB18;
extern TmdSource D_actor_405800_8014086C;
extern TmdSource D_actor_405800_80140F10;
extern TmdSource D_actor_405800_80141430;

void func_actor_405800_80131FC8(s32 otz);
void func_actor_405800_80132670(Task* arg0);
void func_actor_405800_801329C8(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s16 height, u8 shade);
void func_actor_405800_80132E3C(Task* arg0, s16 arg1, u8 arg2);
void func_actor_405800_80132FE0(Task* arg0);
void func_actor_405800_8013315C(Task* arg0);
void func_actor_405800_8013340C(Task* arg0);
void func_actor_405800_801334B8(Task* arg0);
void func_actor_405800_80133800(Task* arg0);
void func_actor_405800_80133CD0(Task* arg0);
void func_actor_405800_80133DB0(Task* arg0);
void func_actor_405800_80133F48(Task* arg0);
void func_actor_405800_801340E0(Task* arg0);
void func_actor_405800_80134314(Task* arg0);
void func_actor_405800_8013471C(Task* arg0);
void func_actor_405800_801348E4(Task* arg0);
void func_actor_405800_80134A64(Task* arg0);
void func_actor_405800_80134C00(Task* arg0);
void func_actor_405800_80134E80(Task* arg0);
void func_actor_405800_801351BC(Task* arg0);
void func_actor_405800_80135558(Task* arg0);
void func_actor_405800_801356A8(Task* arg0);
void func_actor_405800_80135780(Task* arg0);
void func_actor_405800_80135A3C(Task* arg0, s16 arg1);
void func_actor_405800_80135E28(Task* arg0);
void func_actor_405800_801361F8(Task* arg0);
void func_actor_405800_80136388(Task* arg0);
s32  func_actor_405800_80136A1C(Task* arg0);
s32  func_actor_405800_80136B94(Task* arg0);
s32  func_actor_405800_80136CE0(Task* arg0);
void func_actor_405800_80136E14(Task* task);
void func_actor_405800_8013706C(Task* arg0, s16 arg1);
s32  func_actor_405800_8013728C(Task* arg0);
s32  func_actor_405800_801373E0(Task* arg0);
void func_actor_405800_801375C4(Task* arg0);
s32  func_actor_405800_80137908(Task* arg0);
void func_actor_405800_80137948(Task* task);
void func_actor_405800_8013795C(Task* task);
void func_actor_405800_80137994(Task* arg0, s16 arg1);
void func_actor_405800_801379F8(Task* task);
void func_actor_405800_80137A14(Task* task);
void func_actor_405800_80137A60(Task* task);
void func_actor_405800_80137B34(Task* task);
void func_actor_405800_80137B9C(Task* task);
void func_actor_405800_80137C04(Task* task);
void func_actor_405800_80137C78(Task* task);
void func_actor_405800_80137CEC(Task* task);
void func_actor_405800_80137D60(Task* task);
void func_actor_405800_80137DE4(Task* task);
void func_actor_405800_80137E64(Task* task);
void func_actor_405800_80137EF0(Task* task);
void func_actor_405800_80137F58(Task* task);
void func_actor_405800_80137FCC(Task* task);
void func_actor_405800_80138040(Task* task);
void func_actor_405800_801380C0(Task* task);
void func_actor_405800_80138154(Task* task);
void func_actor_405800_801381BC(Task* task);
void func_actor_405800_80138224(Task* task);
void func_actor_405800_80138294(Task* arg0);
s16  func_actor_405800_8013836C(Task* arg0, s16 arg1);
void func_actor_405800_801383CC(Task* arg0, SVECTOR* target, s32 step);
void func_actor_405800_80138478(Task* task, s16 index, Actor405800ViewPos* out);
void func_actor_405800_80138514(Task* arg0, s16 arg1, Actor405800ViewPos* arg2);
s32  func_actor_405800_801385F4(Task* arg0);
void func_actor_405800_80138634(Task* task);
void func_actor_405800_80138698(Task* arg0);
void func_actor_405800_80138788(Task* arg0);
void func_actor_405800_801387DC(Task* task);
void func_actor_405800_80138854(Task* arg0, s32 arg1, u16* arg2);
void func_actor_405800_801388C4(Task* task);
void func_actor_405800_801388D4(void);
void func_actor_405800_801388DC(void);
void func_actor_405800_801388E4(Task* task);
void func_actor_405800_801389AC(Task* task);
void func_actor_405800_80138A18(Task* task);
void func_actor_405800_80138A70(Task* task);
void func_actor_405800_80138B50(Task* task);
void func_actor_405800_80138BD4(Task* task);
void func_actor_405800_80138BEC(Task* task);
void func_actor_405800_80138C30(Task* task);
void func_actor_405800_80138CF0(Task* task);
void func_actor_405800_80138D54(Task* task);
void func_actor_405800_80138E20(Task* task);
void func_actor_405800_80138EF0(Task* task);
void func_actor_405800_80138F54(Task* task);
void func_actor_405800_80138FA8(Task* task);
void func_actor_405800_8013902C(Task* task);
void func_actor_405800_801390FC(Task* arg0);
void func_actor_405800_80139188(Task* arg0);
void func_actor_405800_80139260(Task* arg0);
void func_actor_405800_801392EC(Task* arg0);
void func_actor_405800_80139358(Task* arg0);
void func_actor_405800_801393E8(Task* arg0);
void func_actor_405800_801394E4(Task* arg0);
void func_actor_405800_80139550(Task* task);
void func_actor_405800_801395E8(Task* task);
void func_actor_405800_8013967C(Task* task);
void func_actor_405800_80139700(Task* task);
void func_actor_405800_801397B8(Task* task);
void func_actor_405800_801397F0(Task* task);
void func_actor_405800_80139844(Task* task);
void func_actor_405800_80139880(Task* task);
void func_actor_405800_801398C0(Task* task);
void func_actor_405800_80139928(Task* task);
void func_actor_405800_801399C4(Task* arg0);
void func_actor_405800_80139AC4(Task* arg0);
void func_actor_405800_80139B3C(Task* arg0);
void func_actor_405800_80139BD8(Task* arg0);
void func_actor_405800_80139C98(Task* arg0);
void func_actor_405800_80139D24(Task* arg0);
void func_actor_405800_80139DC0(Task* arg0);
void func_actor_405800_80139E2C(Task* task);
void func_actor_405800_80139E48(Task* arg0);
void func_actor_405800_80139EAC(Task* arg0);
void func_actor_405800_80139F0C(Task* task, u8 arg1);
s32  func_actor_405800_80139F3C(Task* arg0);
void func_actor_405800_80139FB0(Task* task, s16 arg1);
void func_actor_405800_80139FC4(Task* arg0);
void func_actor_405800_8013A0F4(Task* arg0);
void func_actor_405800_8013A1E0(Task* task, s16 arg1, s16 arg2);
void func_actor_405800_8013A1F8(Task* task, s16 arg1, s16 arg2, s16 arg3);

/// `func_actor_405800_80139FC4`'s body, inlined: wrap the three angles to 12 bits and
/// rebuild the model root's rotation from them. Inlining is what keeps each
/// `G_SCRATCH_HEAD` access in the absolute `lui`/`lw` form instead of a
/// register CSE would otherwise hoist the address into.
static __inline__ void Actor405800_RebuildRotation(Task* arg0)
{
    Actor405800Work* work  = (Actor405800Work*)arg0->work;
    GpCoord*         coord = arg0->extra.tmd->coords;
    MATRIX*          m;
    MATRIX*          dst;

    work->field_80      &= 0xFFF;
    work->field_82      &= 0xFFF;
    work->field_84      &= 0xFFF;
    m                    = (MATRIX*)(SCRATCH_HEAD(u8) - 0x20);
    MATRIX_PAIR(m, 0, 0) = 0x1000;
    MATRIX_PAIR(m, 0, 2) = 0;
    MATRIX_PAIR(m, 1, 1) = 0x1000;
    MATRIX_PAIR(m, 2, 0) = 0;
    m->m[2][2]           = 0x1000;
    SCRATCH_HEAD(MATRIX) = m;
    RotMatrixZ((s16)work->field_84, m);
    RotMatrixX((s16)work->field_80, m);
    func_8004BFF8((s16)work->field_82, m);
    dst          = &coord->coord;
    dst->m[0][0] = m->m[0][0];
    dst->m[0][1] = m->m[0][1];
    dst->m[0][2] = m->m[0][2];
    dst->m[1][0] = m->m[1][0];
    dst->m[1][1] = m->m[1][1];
    dst->m[1][2] = m->m[1][2];
    dst->m[2][0] = m->m[2][0];
    dst->m[2][1] = m->m[2][1];
    SCRATCH_POP_BYTES(0x20);
    dst->m[2][2] = m->m[2][2];
}

/// `func_actor_405800_8013A0F4`'s body, inlined: advance the pending animation
/// request, then tick every model slot at the current rate.
static __inline__ void Actor405800_TickAnim(Task* arg0)
{
    Actor405800Work* work = (Actor405800Work*)arg0->work;
    s32              i;

    if (work->field_86E == 1) {
        if (work->field_870 != work->field_872) {
            work->field_874 = 0;
        } else {
            work->field_874 = func_actor_405800_8013836C(arg0, work->field_874);
        }
        func_actor_405800_80138294(arg0);
        work->field_86E = 3;
    } else if (work->field_86E == 2) {
        func_actor_405800_80138224(arg0);
        work->field_86E = 3;
        work->field_874 = 0;
    } else if (work->field_86E == 3) {
        work->field_874++;
    }
    i = 1;
    do {
        work->slots[i].rate = work->field_850;
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0x12);
}

/// Queues, at ordering-table entry `otz`, a copy of the frame in the current
/// draw buffer into the off-screen 320x240 VRAM area at (0x1C0, 0x100). In
/// execution order the chain retargets drawing there, fills it near-black
/// with the mask bit set, draws the frame's two 160-pixel halves over it as
/// 16-bit texture sprites, then restores the offset and the draw area. The
/// restored area is the whole draw buffer, or the current view's draw-area
/// rectangle when that rectangle's depth lies nearer than `otz`. The 0x14-byte
/// `ActorsDrawScratch` it works in is taken off `G_SCRATCH_HEAD` and given
/// back.
void func_actor_405800_80131FC8(s32 otz)
{
    u8*                head;
    u8*                allocated;
    ActorsDrawScratch* scratch;
    GpDrawAreaRec*     extra;
    DR_AREA*           area;
    DR_STP*            stp;
    DR_OFFSET*         off;
    SPRT*              sprt;
    DR_TPAGE*          tpage;
    TILE*              tile;
    RECT*              clip;
    u_short*           ofs;
    s32                val;
    s32                z;

    extra              = Gp_GetViewSprtExtra();
    head               = SCRATCH_HEAD(u8);
    area               = (DR_AREA*)gGpuPrimCursor;
    allocated          = head - 0x14;
    SCRATCH_HEAD(void) = allocated;
    gGpuPrimCursor     = (DR_TPAGE*)(area + 1);
    USE_REG(allocated);
    scratch      = (ActorsDrawScratch*)allocated;
    scratch->otz = otz;
    if (extra != NULL) {
        val = (extra->depth << gDisplayState.otDepthShift) & 0x3FFF;
        z   = otz;
        SOFT_TOUCH_REG(z);
        if ((val >> 4) < z) {
            scratch->rect   = extra->rect;
            scratch->rect.y = (u16)scratch->rect.y + gDisplayState.drawBuffer * 0x110;
        } else {
            goto block_4;
        }
    } else {
    block_4:
        scratch->rect.x = 0;
        scratch->rect.y = gDisplayState.drawBuffer * 0x110;
        scratch->rect.w = 0x140;
        scratch->rect.h = 0xF0;
    }
    clip = &scratch->rect;
    SetDrawArea(area, clip);
    addPrim(&gGpuCurrentOt[scratch->otz], area);

    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(stp + 1);
    SetDrawStp(stp, 0);
    addPrim(&gGpuCurrentOt[scratch->otz], stp);

    ofs             = scratch->ofs;
    off             = (DR_OFFSET*)gGpuPrimCursor;
    gGpuPrimCursor  = (DR_TPAGE*)(off + 1);
    scratch->ofs[0] = 0xA0;
    scratch->ofs[1] = gDisplayState.drawBuffer * 0x110 + 0x78;
    SetDrawOffset(off, ofs);
    addPrim(&gGpuCurrentOt[scratch->otz], off);

    sprt           = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(sprt + 1);
    sprt->x0       = -0xA0;
    sprt->y0       = -0x78;
    sprt->w        = 0xA0;
    sprt->h        = 0xF0;
    sprt->u0       = 0;
    sprt->v0       = gDisplayState.drawBuffer * 0x10;
    setlen(sprt, 4);
    setcode(sprt, 0x65);
    addPrim(&gGpuCurrentOt[scratch->otz], sprt);

    tpage          = gGpuPrimCursor;
    gGpuPrimCursor = tpage + 1;
    setDrawTPage(tpage, 1, 1, getTPage(2, 0, 0, gDisplayState.drawBuffer << 8));
    addPrim(&gGpuCurrentOt[scratch->otz], tpage);

    sprt           = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(sprt + 1);
    sprt->x0       = 0;
    sprt->y0       = -0x78;
    sprt->w        = 0xA0;
    sprt->h        = 0xF0;
    sprt->u0       = 0x20;
    sprt->v0       = gDisplayState.drawBuffer * 0x10;
    setlen(sprt, 4);
    setcode(sprt, 0x65);
    addPrim(&gGpuCurrentOt[scratch->otz], sprt);

    tpage          = gGpuPrimCursor;
    gGpuPrimCursor = tpage + 1;
    setDrawTPage(tpage, 1, 1, getTPage(2, 0, 0x80, gDisplayState.drawBuffer << 8));
    addPrim(&gGpuCurrentOt[scratch->otz], tpage);

    tile           = (TILE*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(tile + 1);
    setlen(tile, 3);
    setcode(tile, 0x60);
    tile->b0 = 2;
    tile->g0 = 2;
    tile->r0 = 2;
    tile->x0 = -0xA0;
    tile->y0 = -0x78;
    tile->w  = 0x140;
    tile->h  = 0xF0;
    addPrim(&gGpuCurrentOt[scratch->otz], tile);

    stp            = (DR_STP*)gGpuPrimCursor;
    gGpuPrimCursor = (DR_TPAGE*)(stp + 1);
    SetDrawStp(stp, 1);
    addPrim(&gGpuCurrentOt[scratch->otz], stp);

    off             = (DR_OFFSET*)gGpuPrimCursor;
    gGpuPrimCursor  = (DR_TPAGE*)(off + 1);
    scratch->ofs[0] = 0x260;
    scratch->ofs[1] = 0x178;
    SetDrawOffset(off, ofs);
    addPrim(&gGpuCurrentOt[scratch->otz], off);

    area            = (DR_AREA*)gGpuPrimCursor;
    gGpuPrimCursor  = (DR_TPAGE*)(area + 1);
    scratch->rect.x = 0x1C0;
    scratch->rect.y = 0x100;
    scratch->rect.w = 0x140;
    scratch->rect.h = 0xF0;
    SetDrawArea(area, clip);
    addPrim(&gGpuCurrentOt[scratch->otz], area);

    SCRATCH_POP_BYTES(0x14);
}

void func_actor_405800_80132670(Task* arg0)
{
    Actor405800Work* work;

    work                   = (Actor405800Work*)arg0->work;
    work->obj_4B4.coord    = &arg0->extra.tmd->coords[3];
    work->obj_4B4.ctx.recs = work->rec_4D4;
    work->obj_4B4.pos.vx   = 0;
    work->obj_4B4.pos.vy   = 0;
    work->obj_4B4.pos.vz   = 0x110;
    work->obj_4B4.key      = 0x3003A;
    work->obj_4B4.radius   = 0x2F0;
    work->obj_4B4.flags    = 1;
    Gp_LinkObj(2, &work->obj_4B4);
    Gp_InitRec18Table(work->rec_4D4, 8, 0);
    work->obj_4B4.flags   |= 0x8000;
    work->obj_594.coord    = arg0->extra.tmd->coords;
    work->obj_594.ctx.recs = work->rec_5B4;
    work->obj_594.pos.vx   = 0;
    work->obj_594.pos.vy   = -0x220;
    work->obj_594.pos.vz   = 0;
    work->obj_594.key      = 0x3003A;
    work->obj_594.radius   = 0x460;
    work->obj_594.flags    = 1;
    Gp_LinkObj(2, &work->obj_594);
    Gp_InitRec18Table(work->rec_5B4, 8, 0);
    work->rec_744.end0.vz    = 0xBB8;
    work->rec_744.end0Radius = 0xA;
    work->rec_744.end1Radius = 0xA;
    work->rec_744.end0.vx    = 0;
    work->rec_744.end1.vz    = 0;
    work->rec_744.end1.vx    = 0;
    work->rec_744.recs       = work->rec_75C;
    work->obj_594.flags     |= 0x4000;
    work->obj_724.coord      = arg0->extra.tmd->coords;
    work->obj_724.ctx.d4rec  = &work->rec_744;
    work->obj_724.pos.vx     = 0;
    work->obj_724.pos.vy     = -0x190;
    work->obj_724.pos.vz     = 0;
    work->obj_724.key        = 0x30005;
    work->obj_724.radius     = 0;
    work->obj_724.flags      = 3;
    Gp_LinkObj(2, &work->obj_724);
    Gp_InitRec18Table(work->rec_75C, 8, 0);
    work->obj_724.flags   &= 0x3FFF;
    work->obj_6B4.key      = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj_6B4.coord    = &arg0->extra.tmd->coords[7];
    work->obj_6B4.ctx.recs = work->rec_6F4;
    work->obj_6B4.pos.vx   = -0x460;
    work->obj_6B4.pos.vy   = 0;
    work->obj_6B4.pos.vz   = 0;
    work->obj_6B4.radius   = 0x290;
    work->obj_6B4.flags    = 1;
    Gp_LinkObj(3, &work->obj_6B4);
    Gp_InitRec18Table(work->rec_6F4, 1, 0);
    work->obj_6B4.flags   &= 0x7FFF;
    work->obj_674.key      = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj_674.coord    = &arg0->extra.tmd->coords[7];
    work->obj_674.ctx.recs = work->rec_6F4;
    work->obj_674.pos.vx   = -0x200;
    work->obj_674.pos.vy   = 0;
    work->obj_674.pos.vz   = 0;
    work->obj_674.radius   = 0x250;
    work->obj_674.flags    = 1;
    Gp_LinkObj(3, &work->obj_674);
    Gp_InitRec18Table(work->rec_6F4, 1, 0);
    work->obj_674.flags   &= 0x7FFF;
    work->obj_6D4.key      = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj_6D4.coord    = &arg0->extra.tmd->coords[10];
    work->obj_6D4.ctx.recs = work->rec_70C;
    work->obj_6D4.pos.vx   = 0x460;
    work->obj_6D4.pos.vy   = 0;
    work->obj_6D4.pos.vz   = 0;
    work->obj_6D4.radius   = 0x290;
    work->obj_6D4.flags    = 1;
    Gp_LinkObj(3, &work->obj_6D4);
    Gp_InitRec18Table(work->rec_70C, 1, 0);
    work->obj_6D4.flags   &= 0x7FFF;
    work->obj_694.key      = Gp_PackObjPair(arg0->spawnArg2, 0);
    work->obj_694.coord    = &arg0->extra.tmd->coords[10];
    work->obj_694.ctx.recs = work->rec_70C;
    work->obj_694.pos.vx   = 0x200;
    work->obj_694.pos.vy   = 0;
    work->obj_694.pos.vz   = 0;
    work->obj_694.radius   = 0x250;
    work->obj_694.flags    = 1;
    Gp_LinkObj(3, &work->obj_694);
    Gp_InitRec18Table(work->rec_70C, 1, 0);
    work->obj_694.flags &= 0x7FFF;
}

void func_actor_405800_801329C8(Task* task, s16 firstJoint, s16 secondJoint, s16 width, s16 height, u8 shade)
{
    ActorBeamScratch* s;
    s16               angle;
    GpCoord*          secondCoord;
    GpCoord*          firstCoord;
    s32               offset0;
    s32               offset1;
    s32               offset2;
    s32               offset3;
    s32               halfX;
    s32               halfZ;
    GpCoord*          coords;
    POLY_FT4*         poly;

    coords      = task->extra.tmd->coords;
    firstCoord  = coords + firstJoint;
    secondCoord = coords + secondJoint;
    if (firstJoint != secondJoint) {
        s = (ActorBeamScratch*)SCRATCH_PUSH_BYTES(sizeof(ActorBeamScratch));
        Gp_UpdateCoord(firstCoord);
        Gp_UpdateCoord(secondCoord);
        Gp_WorldToLocal(&gGfxViewCoord.workm, &firstCoord->workm, &s->firstMatrix);
        Gp_WorldToLocal(&gGfxViewCoord.workm, &secondCoord->workm, &s->secondMatrix);
        s->first.vy       = height;
        s->second.vy      = height;
        s->first.vx       = s->firstMatrix.t[0];
        s->first.vz       = s->firstMatrix.t[2];
        s->second.vx      = s->secondMatrix.t[0];
        s->second.vz      = s->secondMatrix.t[2];
        angle             = ratan2(s->second.vx - s->first.vx, s->second.vz - s->first.vz);
        halfX             = (s->first.vx - s->second.vx) / 2;
        halfZ             = (s->first.vz - s->second.vz) / 2;
        offset0           = rcos(angle) * width;
        s->corner0.vy     = height;
        s->corner0.vx     = halfX + (s->first.vx - (offset0 >> 0xC));
        s->corner0.vz     = halfZ + (s->first.vz + ((s32)(rsin(angle) * width) >> 0xC));
        offset1           = rcos(angle) * width;
        s->corner1.vy     = height;
        s->corner1.vx     = halfX + (s->first.vx + (offset1 >> 0xC));
        s->corner1.vz     = halfZ + (s->first.vz - ((s32)(rsin(angle) * width) >> 0xC));
        offset2           = rcos(angle) * width;
        s->corner2.vy     = height;
        s->corner2.vx     = (s->second.vx - (offset2 >> 0xC)) - halfX;
        s->corner2.vz     = (s->second.vz + ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        offset3           = rcos(angle) * width;
        s->corner3.vy     = height;
        s->corner3.vx     = (s->second.vx + (offset3 >> 0xC)) - halfX;
        s->corner3.vz     = (s->second.vz - ((s32)(rsin(angle) * width) >> 0xC)) - halfZ;
        gGfxViewCoord.flg = 0;
        Gp_UpdateCoord(&gGfxViewCoord);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        s->depth = RotTransPers4(&s->corner0, &s->corner1, &s->corner2, &s->corner3, &s->screen0, &s->screen1,
                                 &s->screen2, &s->screen3, &s->perspective, &s->flags);
        if (s->flags >= 0) {
            poly           = gGpuPrimCursor;
            gGpuPrimCursor = (u8*)poly + 0x28;
            setlen(poly, 9);
            poly->code            = 0x2E;
            PRIM_XY_WORD(poly, 0) = s->screen0;
            PRIM_XY_WORD(poly, 1) = s->screen1;
            PRIM_XY_WORD(poly, 2) = s->screen2;
            PRIM_XY_WORD(poly, 3) = s->screen3;
            setUV4(poly, 0xC0, 0x98, 0xF7, 0x98, 0xC0, 0xCF, 0xF7, 0xCF);
            poly->tpage = 0x48;
            poly->clut  = 0x4283;
            setRGB0(poly, shade, shade, shade);
            addPrim((u32*)((((u32)(s->depth << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (u32)gGpuCurrentOt), poly);
        }
        SCRATCH_POP_BYTES(sizeof(ActorBeamScratch));
    }
}

void func_actor_405800_80132E3C(Task* arg0, s16 arg1, u8 arg2)
{
    func_actor_405800_801329C8(arg0, 3, 9, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 9, 0xA, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 0xA, 0xB, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 3, 6, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 6, 7, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 7, 8, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 1, 5, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 1, 0xC, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 0xC, 0xD, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 0xD, 0xE, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 1, 0xF, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 0xF, 0x10, 0x100, arg1, arg2);
    func_actor_405800_801329C8(arg0, 0x10, 0x11, 0x100, arg1, arg2);
}

void func_actor_405800_80132FE0(Task* arg0)
{
    Actor405800Work* work;
    TmdObject*       model;
    GpCoord*         coord;
    VECTOR           scale;
    SVECTOR          rot;

    work              = (Actor405800Work*)arg0->work;
    model             = arg0->extra.tmd;
    coord             = model->coords;
    work->field_832   = (u16)work->field_832 + ((s16)(0xFF - (u16)work->field_832) >> 4);
    work->field_834   = (u16)work->field_834 + ((s16)(-(u16)work->field_834) >> 4);
    work->field_866   = (u16)work->field_866 + (-work->field_866 >> 2);
    model->lightLevel = work->field_834;
    func_8009EA50(work->field_832);
    work->flags_83C.h.field_83E -= 0x30;
    scale.vx                     = 0x1000;
    scale.vy                     = work->flags_83C.h.field_83E;
    scale.vz                     = 0x1000;
    coord->coord                 = work->matrix_0;
    ScaleMatrix(&coord->coord, &scale);
    coord->flg = 0;
    work->field_842++;
    if ((s16)work->field_842 == 8) {
        rot.vx = 0;
        rot.vy = 0;
        rot.vz = 0;
        Gp_SpawnEff(0x600A5, coord, 3, &rot);
    }
    if ((s16)work->field_842 >= 0x41) {
        model->flags |= 0x80;
        work->field_846++;
    }
}

void func_actor_405800_8013315C(Task* arg0)
{
    GpEnemy*         enemy;
    Actor405800Work* work;
    TmdObject*       model;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor405800Work*)arg0->work;
    model = arg0->extra.tmd;
    if ((s8)work->field_895 < 0) {
        if (!(work->field_895 & 1)) {
            switch ((s8)work->field_896) {
                case 0:
                    work->field_832 = (u16)work->field_832 + ((s16)(0xFF - (u16)work->field_832) >> 2);
                    if (work->field_832 >= 0xF8) {
                        work->field_832 = 0xFF;
                        work->field_830 = 0;
                        work->field_896++;
                    }
                    goto block_32;
                case 1:
                    work->field_830++;
                    if (work->field_836 < (s16)work->field_830) {
                        goto block_28;
                    }
                    break;
                case 2:
                    work->field_834 = (u16)work->field_834 + ((s16)(-(u16)work->field_834) >> 2);
                    work->field_866 = (u16)work->field_866 + (-work->field_866 >> 2);
                    if (work->field_834 == 0) {
                        enemy->node.state.b.flags = 1;
                        if (work->field_897 == 0) {
                            enemy->node.state.b.flags = 5;
                        }
                        work->field_866 = 0;
                        work->field_895 = 0;
                    }
                    goto block_26;
            }
        } else {
            switch ((s8)work->field_896) {
                case 0:
                    enemy->node.state.b.flags = 0;
                    if (work->field_897 == 0) {
                        enemy->node.state.b.flags = 4;
                    }
                    work->field_834 = (u16)work->field_834 + ((s16)(0x1000 - (u16)work->field_834) >> 2);
                    work->field_866 = (u16)work->field_866 + ((0xFF - work->field_866) >> 2);
                    if (work->field_834 >= 0xFF0) {
                        work->field_866 = 0xFF;
                        work->field_834 = 0x1000;
                        work->field_830 = 0;
                        work->field_896++;
                    }
                block_26:
                    model->lightLevel = work->field_834;
                    break;
                case 1:
                    work->field_830++;
                    if ((s16)work->field_830 >= 0x11) {
                    block_28:
                        work->field_896++;
                    }
                    break;
                case 2:
                    work->field_832 = (u16)work->field_832 + ((s16)(-(u16)work->field_832) >> 2);
                    if (work->field_832 < 9) {
                        work->field_832 = 0;
                        work->field_895 = 0;
                        func_actor_405800_8013340C(arg0);
                        if (work->field_83A == 0) {
                            work->field_83A = (u16)work->field_838;
                        }
                    }
                block_32:
                    func_8009EA50(work->field_832);
                    break;
            }
        }
    }
    if (work->field_83A > 0) {
        work->field_83A = (u16)work->field_83A - 1;
    }
}

void func_actor_405800_8013340C(Task* arg0)
{
    GpEnemy*         enemy;
    Actor405800Work* work;
    s16              hp;
    s32              maxHp;
    s32              quarter;

    enemy   = (GpEnemy*)arg0->spawnArg2;
    hp      = enemy->hp;
    work    = (Actor405800Work*)arg0->work;
    maxHp   = enemy->hpMax << 0x10;
    quarter = maxHp >> 0x12;
    if ((quarter + (maxHp >> 0x11)) < hp) {
        work->field_836 = 0x10;
        work->field_838 = 0;
        return;
    }
    if (quarter < hp) {
        work->field_836 = 0x20;
        work->field_838 = 0x40;
        return;
    }
    if ((maxHp >> 0x13) < hp) {
        work->field_836 = 0x30;
        work->field_838 = 0x80;
        return;
    }
    if ((maxHp >> 0x14) < hp) {
        work->field_836 = 0x40;
        work->field_838 = 0xC0;
        return;
    }
    work->field_836 = 0x50;
    work->field_838 = 0x100;
}

void func_actor_405800_801334B8(Task* arg0)
{
    TmdObject*       model;
    GpEnemy*         enemy;
    GpCoord*         coord;
    Actor405800Work* work;
    Actor405800Work* w2;
    Actor405800Work* w3;
    Actor405800Work* w4;
    TmdObject*       extra;
    u32              rnd;

    model = arg0->extra.tmd;
    enemy = (GpEnemy*)arg0->spawnArg2;
    coord = model->coords;
    if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) != GP_LOC_KEY(4, 8, 0, 0)) {
        Gp_DestroyEnemy(enemy, arg0);
        return;
    }
    arg0->work = memCalloc(0x89CU, false);
    work       = (Actor405800Work*)arg0->work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, arg0);
        return;
    }
    gStageSceneMusicEntry = 2;
    model->lightMtx       = &work->matrix_40;
    model->colorMtx       = &work->matrix_20;
    model->flags          = 0;
    arg0->msgTable        = D_actor_405800_8015149C;
    enemy->field_4        = &coord->coord;
    enemy->field_48       = 0;
    enemy->bodyPos.vx     = 0;
    enemy->bodyPos.vy     = 0;
    enemy->bodyPos.vz     = 0;
    enemy->coord          = &arg0->extra.tmd->coords[3];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.b.flags = 5;
    enemy->param              = &D_actor_405800_801418FC;
    enemy->recs               = work->rec_4D4;
    work->eff_81C.coord       = &arg0->extra.tmd->coords[3];
    work->eff_81C.spawnArgLo  = 0x100;
    work->eff_81C.spawnArgHi  = 2;
    enemy->hp = enemy->hpMax = D_actor_405800_801418FC.hpMax;
    func_800B3F84(&work->anim, D_actor_405800_80151410, model, work->pad_394, work->slots);

    w2            = (Actor405800Work*)arg0->work;
    w2->field_850 = 0x10;
    w2->field_872 = 1;
    w2->field_86E = 2;

    Actor405800_TickAnim(arg0);

    coord->sub = &gGfxViewCoord;
    func_actor_405800_80132670(arg0);
    func_actor_405800_80135780(arg0);
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    w3            = (Actor405800Work*)arg0->work;
    w3->field_846 = 0;
    w3->field_848 = 0;
    if (gGameSession->at4.loc.warp == 1) {
        coord->coord.t[0] = 0x14B4;
        coord->coord.t[2] = 0xD7A;
        coord->coord.t[1] = 0;
        work->field_82    = 0x400;
    } else {
        coord->coord.t[0] = 0x514;
        coord->coord.t[1] = 0;
        coord->coord.t[2] = 0x251C;
        work->field_82    = 0;
    }
    work->field_90  = coord->coord.t[0];
    work->field_92  = coord->coord.t[1];
    work->field_94  = coord->coord.t[2];
    rnd             = ((u32)Gp_LcgState * 5) + 0x71357911;
    Gp_LcgState     = rnd;
    work->field_840 = rnd >> 0x10;
    work->field_86A = work->field_92;
    w4              = (Actor405800Work*)arg0->work;
    extra           = arg0->extra.tmd;
    w4->field_832   = 0xFF;
    w4->field_834   = 0;
    w4->field_866   = 0;
    w4->field_836   = 0x10;
    func_8009EA50(w4->field_832);
    extra->lightLevel = w4->field_834;
    w3                = (Actor405800Work*)arg0->work;
    arg0->state       = 1;
    w3->field_846     = 0;
    w3->field_848     = 0;
}

/// Behaviour handlers `func_actor_405800_80138698` runs by `field_846`.
const TaskFuncTable12 D_actor_405800_80131E24 = {
    {
        func_actor_405800_801388E4,
        func_actor_405800_801389AC,
        func_actor_405800_80138A18,
        func_actor_405800_80138A70,
        func_actor_405800_80138B50,
        func_actor_405800_80132FE0,
        func_actor_405800_80138BD4,
        func_actor_405800_80138BEC,
        func_actor_405800_80138C30,
        func_actor_405800_80138CF0,
        func_actor_405800_80138D54,
        func_actor_405800_80138E20,
    },
};

/// The task's four state handlers, run by `func_actor_405800_80138634`.
const TaskFuncTable4 D_actor_405800_80131E54 = {
    {
        func_actor_405800_801334B8,
        func_actor_405800_80133800,
        func_actor_405800_80138698,
        func_actor_405800_80138788,
    },
};

/// Behaviour handlers `func_actor_405800_80133800` runs by `field_846`.
const TaskFuncTable18 D_actor_405800_80131E64 = {
    {
        func_actor_405800_80137A14,
        func_actor_405800_80133CD0,
        func_actor_405800_80137A60,
        func_actor_405800_80137B34,
        func_actor_405800_80137B9C,
        func_actor_405800_80137C04,
        func_actor_405800_80137C78,
        func_actor_405800_80137CEC,
        func_actor_405800_80137D60,
        func_actor_405800_80137DE4,
        func_actor_405800_80137E64,
        func_actor_405800_80137EF0,
        func_actor_405800_80137F58,
        func_actor_405800_80137FCC,
        func_actor_405800_80138040,
        func_actor_405800_801380C0,
        func_actor_405800_80138154,
        func_actor_405800_801381BC,
    },
};

/// Sub-state handlers of `func_actor_405800_80137C04`, by `field_848`.
const TaskFuncTable3 D_actor_405800_80131EAC = {
    {
        func_actor_405800_80139358,
        func_actor_405800_801393E8,
        func_actor_405800_801394E4,
    },
};

/// Sub-state handlers of `func_actor_405800_80137D60`, by `field_848`.
const TaskFuncTable5 D_actor_405800_80131EB8 = {
    {
        func_actor_405800_8013967C,
        func_actor_405800_801340E0,
        func_actor_405800_80134314,
        func_actor_405800_8013471C,
        func_actor_405800_80139700,
    },
};

/// Sub-state handlers of `func_actor_405800_80137DE4`, by `field_848`.
const TaskFuncTable4 D_actor_405800_80131ECC = {
    {
        func_actor_405800_801397B8,
        func_actor_405800_801348E4,
        func_actor_405800_80134A64,
        func_actor_405800_801397F0,
    },
};

/// Sub-state handlers of `func_actor_405800_80137F58`, by `field_848`.
const TaskFuncTable3 D_actor_405800_80131EDC = {
    {
        func_actor_405800_80139880,
        func_actor_405800_80134E80,
        func_actor_405800_801398C0,
    },
};

/// Sub-state handlers of `func_actor_405800_80137FCC`, by `field_848`.
const TaskFuncTable3 D_actor_405800_80131EE8 = {
    {
        func_actor_405800_80139928,
        func_actor_405800_801351BC,
        func_actor_405800_801399C4,
    },
};

/// Sub-state handlers of `func_actor_405800_80138040`, by `field_848`.
const TaskFuncTable4 D_actor_405800_80131EF4 = {
    {
        func_actor_405800_80139AC4,
        func_actor_405800_80139B3C,
        func_actor_405800_80135558,
        func_actor_405800_80139BD8,
    },
};

/// Sub-state handlers of `func_actor_405800_801380C0`, by `field_848`.
const TaskFuncTable3 D_actor_405800_80131F04 = {
    {
        func_actor_405800_80139C98,
        func_actor_405800_80139D24,
        func_actor_405800_80139DC0,
    },
};

static __inline__ void Actor405800_ProjectPart(GpCoord* part)
{
    void**                   scratch;
    u8*                      head;
    Actor405800PerspScratch* block;
    SVECTOR*                 vec;
    MATRIX*                  wm;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (Actor405800PerspScratch*)(head - 0x18);
    SCRATCH_HEAD_AT(scratch, void) = block;
    block->vec.vx                  = 0;
    block->vec.vy                  = 0;
    block->vec.vz                  = 0;
    Gp_UpdateCoord(part);
    vec = &block->vec;
    wm  = &part->workm;
    gte_SetRotMatrix(wm);
    gte_SetTransMatrix(wm);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((Actor405800PerspScratch*)(head - 0x18))->sxy);
    gte_stdp(&((Actor405800PerspScratch*)(head - 0x18))->p);
    gte_stflg(&((Actor405800PerspScratch*)(head - 0x18))->flag);
    gte_stszotz(&((Actor405800PerspScratch*)(head - 0x18))->otz);
    if (block->flag < 0) {
        block->otz = 0;
    }
    block->otz = (block->otz >> 4) + 0x1E;
    func_actor_405800_80131FC8(block->otz);
}

void func_actor_405800_80133800(Task* arg0)
{
    TmdObject*       model = arg0->extra.tmd;
    Actor405800Work* work  = (Actor405800Work*)arg0->work;
    GpCoord*         coord = model->coords;
    GpEnemy*         enemy = (GpEnemy*)arg0->spawnArg2;
    GpCoord*         part  = &coord[2];
    GpCoord*         root  = coord;
    TaskFuncTable18  fns   = D_actor_405800_80131E64;
    Actor405800Work* w;
    u8*              head;

    switch (Gp_StateF0.field_4) {
        case 2:
            model->flags |= 0x80;
            break;
        case 0:
            work->field_840++;
            func_actor_405800_801361F8(arg0);
            fns.funcs[(s16)work->field_846](arg0);
            func_actor_405800_8013795C(arg0);
            func_actor_405800_801375C4(arg0);
            func_actor_405800_8013315C(arg0);
            Actor405800_TickAnim(arg0);
            work->flags_83C.half = work->slots[1].flags;
            root->flg            = 0;
            Actor405800_RebuildRotation(arg0);
            func_actor_405800_80136388(arg0);
            if (enemy->hp <= 0 && (u8)work->field_88F == 0) {
                w            = (Actor405800Work*)arg0->work;
                arg0->state  = 2;
                w->field_846 = 0;
                w->field_848 = 0;
            }
        case 1:
            actorUpdateModelColor(arg0);
            func_actor_405800_80132E3C(arg0, work->field_86A, work->field_866);
            Actor405800_ProjectPart(part);
            __asm__ volatile("lui %0, 0x1F80" : "=r"(head));
            head             = *(u8**)(head + 0x3FC);
            head            += 0x18;
            SCRATCH_HEAD(u8) = head;
            model->flags    &= 0xFF7F;
            break;
    }
}

void func_actor_405800_80133CD0(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    u32              sound;
    s32              pan;

    if (((Actor405800Work*)arg0->work)->field_852 < 0x1450) {
        sound   = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x40050004;
        pan     = Gp_GetObjPan(arg0->extra.tmd->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        work = (Actor405800Work*)arg0->work;
        if (((s8)work->field_895 >= 0) || ((work->field_895 & 0x7F) != 1)) {
            work->field_895 = 0x81;
            work->field_896 = 0;
        }
        Gp_ArmStateF0(1);
        work2            = (Actor405800Work*)arg0->work;
        work2->field_846 = 0xC;
        work2->field_848 = 0;
    }
}

void func_actor_405800_80133DB0(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    s32              id;
    u32              sound;
    s32              pan;

    work = (Actor405800Work*)arg0->work;
    work->field_842++;
    if ((s16)work->field_842 == 0x16) {
        id = 0x40050005;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0005;
        }
        sound = id | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
        pan   = Gp_GetObjPan(arg0->extra.tmd->coords) << 24;
        pan >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        work->obj_6D4.flags |= 0x8000;
        work->obj_694.flags |= 0x8000;
    }
    if ((s16)work->field_842 == 0x1C) {
        work->obj_6D4.flags &= 0x7FFF;
        work->obj_694.flags &= 0x7FFF;
    }
    if ((func_actor_405800_801385F4(arg0) << 0x10) != 0) {
        func_actor_405800_80137994(arg0, 0);
        func_actor_405800_80137948(arg0);
        if (work->field_890 == 0 && work->field_852 < 0x578 && (u16)(work->field_856 - 0x200) > 0xC00 && (u16)(work->field_854 - 0x200) > 0xC00) {
            work2            = (Actor405800Work*)arg0->work;
            work2->field_846 = 9;
            work2->field_848 = 0;
        } else {
            work3            = (Actor405800Work*)arg0->work;
            work3->field_846 = 2;
            work3->field_848 = 0;
        }
    }
}

void func_actor_405800_80133F48(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    s32              id;
    u32              sound;
    s32              pan;

    work = (Actor405800Work*)arg0->work;
    work->field_842++;
    if ((s16)work->field_842 == 0x16) {
        id = 0x40050005;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0005;
        }
        sound = id | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
        pan   = Gp_GetObjPan(arg0->extra.tmd->coords) << 24;
        pan >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        work->obj_6B4.flags |= 0x8000;
        work->obj_674.flags |= 0x8000;
    }
    if ((s16)work->field_842 == 0x1C) {
        work->obj_6B4.flags &= 0x7FFF;
        work->obj_674.flags &= 0x7FFF;
    }
    if ((func_actor_405800_801385F4(arg0) << 0x10) != 0) {
        func_actor_405800_80137994(arg0, 0);
        func_actor_405800_80137948(arg0);
        if (work->field_890 == 0 && work->field_852 < 0x578 && (u16)(work->field_856 - 0x200) > 0xC00 && (u16)(work->field_854 - 0x200) > 0xC00) {
            work2            = (Actor405800Work*)arg0->work;
            work2->field_846 = 9;
            work2->field_848 = 0;
        } else {
            work3            = (Actor405800Work*)arg0->work;
            work3->field_846 = 2;
            work3->field_848 = 0;
        }
    }
}

void func_actor_405800_801340E0(Task* arg0)
{
    GpAnimArg        msg;
    GpDelayArg       query;
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    s32              base;
    s32              sound;
    s32              pan;

    work = (Actor405800Work*)arg0->work;
    if (((GameActor*)Gp_ActorSlots[0]->work)->field_954 == 2 || (func_actor_405800_8013728C(arg0) << 0x10) != 0) {
        func_actor_405800_801379F8(arg0);
        work3            = (Actor405800Work*)arg0->work;
        work3->field_846 = 2;
        work3->field_848 = 0;
        func_actor_405800_80135A3C(arg0, work->field_87E);
        return;
    }
    query.field_14 = 0x18;
    if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F8, (s32)&query, 0) != 0) {
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
        if (work->field_890 == 0) {
            work3            = (Actor405800Work*)arg0->work;
            work3->field_846 = 2;
            work3->field_848 = 0;
            return;
        }
        work2            = (Actor405800Work*)arg0->work;
        work2->field_846 = 0xD;
        work2->field_848 = 0;
        return;
    }
    work->field_86A = work->field_92;
    func_actor_405800_801379F8(arg0);
    work->field_890      = 0;
    Gp_StateC08.field_6 |= 1;
    work->field_88F      = 1;
    work->field_9A       = work->field_92;
    msg.animBlock.ptr    = D_actor_405800_801513F8;
    msg.field_8          = 0;
    msg.field_C          = 0;
    msg.field_10         = 0;
    msg.field_4          = 4;
    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FF, (s32)&msg, 0);
    work->obj_4B4.flags &= 0x7FFF;
    work->obj_594.flags &= 0xBFFF;
    work2                = (Actor405800Work*)arg0->work;
    work2->field_850     = 0x10;
    work2->field_872     = 0x21;
    work2->field_84A     = 4;
    work2->field_86E     = 1;
    work->field_842      = 0;
    work->field_844      = 0;
    base                 = 0x40050004;
    if ((arg0->spawnArg1 & 0xF0) == 0x10) {
        base = 0x404A0004;
    }
    sound = base | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
    pan   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    work->field_884 = 0;
    work->field_848++;
}

void func_actor_405800_80134314(Task* arg0)
{
    GpAnimArg        msg;
    SVECTOR          vec;
    Actor405800Work* work;
    Actor405800Work* work2;
    GpEnemy*         enemy;
    GpCoord*         coord;
    GpCoord*         player;
    GpCoord*         root;
    PlayerStatus*    cfg;
    s32              id;
    s32              sound;
    s32              pan;
    s32              sound2;
    s32              pan2;

    work               = (Actor405800Work*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    enemy              = (GpEnemy*)arg0->spawnArg2;
    player             = Gp_ActorSlots[0]->extra.tmd->coords;
    work->field_84    += -(s16)work->field_84 >> 2;
    coord->coord.t[0] += (player->coord.t[0] - coord->coord.t[0]) >> 2;
    coord->coord.t[2] += (player->coord.t[2] - coord->coord.t[2]) >> 2;
    coord->coord.t[1] += (player->coord.t[1] - coord->coord.t[1]) >> 2;
    work->field_842++;
    cfg = &Player_Status;
    if (++work->field_844 == 8) {
        sound = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 6;
        pan   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if ((u8)work->field_88B == 1 || work->field_88C == 1 || enemy->hp <= 0 || work->field_884 >= 4) {
        work->field_88B = 0;
        if (work->field_88C == 0) {
            msg.animBlock.ptr = D_actor_405800_801513F8;
            msg.field_8       = 1;
            msg.field_C       = 8;
            msg.field_10      = 0;
            msg.field_4       = 5;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F4, (s32)&msg, 0);
        }
        work2            = (Actor405800Work*)arg0->work;
        work2->field_84A = 8;
        work2->field_850 = 0x10;
        work2->field_872 = 0x22;
        work2->field_86E = 1;
        work->field_84C  = -0x2A;
        work->field_84E  = 0;
        work->field_842  = 0;
        work->field_848++;
        return;
    }
    if ((s16)work->field_842 == 1 || (s16)work->field_842 == 0x10 || (s16)work->field_842 == 0x25) {
        root = &Gp_ActorSlots[0]->extra.tmd->coords[4];
        Gp_SpawnPadLerp(0xA, 0xC0, 8);
        id = 0x40050009;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0009;
        }
        sound2 = id | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
        pan2   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound2, pan2, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F9, Gp_PackObjPair(enemy, 2), 0);
        vec.vx         = 0;
        vec.vy         = -200;
        vec.vz         = 0;
        work->field_98 = ((rsin((s16)work->field_82 + 0x800) * 3000) >> 12) / 20;
        work->field_9C = ((rcos((s16)work->field_82 + 0x800) * 3000) >> 12) / 20;
        Gp_SpawnEff(0x6009B, root, 0x10100, &vec);
        if (cfg->hp <= 0) {
            work->field_88C = 1;
        }
    }
    if ((func_actor_405800_801385F4(arg0) << 0x10) != 0) {
        work->field_842 = 0;
        work->field_884++;
    }
}

void func_actor_405800_8013471C(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    GpCoord*         coord;
    GpCoord*         player;
    s32              y;
    s32              id;
    s32              sound;
    s32              pan;

    work            = (Actor405800Work*)arg0->work;
    coord           = arg0->extra.tmd->coords;
    player          = Gp_ActorSlots[0]->extra.tmd->coords;
    work->field_84 += -(s16)work->field_84 >> 2;
    work->field_842++;
    if ((s16)work->field_842 >= 8) {
        work->obj_594.flags |= 0x4000;
        coord->coord.t[0]   += (s16)work->field_98;
        coord->coord.t[2]   += (s16)work->field_9C;
        work->obj_4B4.flags |= 0x8000;
        work->field_84C     += 6;
        work->field_84E     += work->field_84C;
        y                    = coord->coord.t[1] + work->field_84E;
        coord->coord.t[1]    = y;
        if (y >= work->field_9A) {
            coord->coord.t[1] = work->field_9A;
            player->flg       = 0;
            Gp_UpdateCoord(player);
            id = 0x40050003;
            if ((arg0->spawnArg1 & 0xF0) == 0x10) {
                id = 0x404A0003;
            }
            sound = id | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
            pan   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
            SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
            work->obj_594.flags |= 0x4000;
            work2                = (Actor405800Work*)arg0->work;
            work2->field_84A     = 2;
            work2->field_872     = 0x19;
            work2->field_850     = 0x10;
            work2->field_86E     = 1;
            work->field_848++;
        }
    }
}

void func_actor_405800_801348E4(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    GpCoord*         coord;
    s16              v;

    work  = (Actor405800Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    v     = func_actor_405800_8013728C(arg0);
    if (v != 0) {
        if (v < 0x4E9) {
            func_actor_405800_801379F8(arg0);
            work3            = (Actor405800Work*)arg0->work;
            work3->field_846 = 2;
            work3->field_848 = 0;
            return;
        }
        work->field_98 = (u16)coord->coord.t[0] + ((rsin((s16)work->field_82 + 0x800) * (v - 0x100)) >> 12);
        work->field_9C = (u16)coord->coord.t[2] + ((rcos((s16)work->field_82 + 0x800) * (v - 0x100)) >> 12);
    } else {
        work->field_98 = (u16)coord->coord.t[0] + ((rsin((s16)work->field_82 + 0x800) * 0x1770) >> 12);
        work->field_9C = (u16)coord->coord.t[2] + ((rcos((s16)work->field_82 + 0x800) * 0x1770) >> 12);
    }
    func_actor_405800_801379F8(arg0);
    work2            = (Actor405800Work*)arg0->work;
    work2->field_84A = 4;
    work2->field_850 = 0x10;
    work2->field_872 = 0x15;
    work2->field_86E = 1;
    work->field_84C  = -0x2A;
    work->field_84E  = 0;
    work->field_842  = 0;
    work->field_848++;
}

void func_actor_405800_80134A64(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    GpCoord*         coord;
    s32              y;
    s32              id;
    s32              sound;
    s32              pan;

    work  = (Actor405800Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    work->field_842++;
    if ((s16)work->field_842 < 0x11) {
        func_actor_405800_80136A1C(arg0);
        return;
    }
    if ((s16)work->field_842 == 0x11) {
        work->field_85A = 0;
        work->field_88F = 1;
    }
    coord->coord.t[0] += ((s16)work->field_98 - coord->coord.t[0]) >> 4;
    coord->coord.t[2] += ((s16)work->field_9C - coord->coord.t[2]) >> 4;
    work->field_84C   += 6;
    work->field_84E   += work->field_84C;
    y                  = coord->coord.t[1] + work->field_84E;
    coord->coord.t[1]  = y;
    if (y >= (s16)work->field_92) {
        coord->coord.t[1] = (s16)work->field_92;
        id                = 0x40050003;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0003;
        }
        sound = id | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
        pan   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        work2            = (Actor405800Work*)arg0->work;
        work2->field_84A = 2;
        work2->field_872 = 0x19;
        work2->field_850 = 0x10;
        work2->field_86E = 1;
        work->field_88F  = 0;
        work->field_848++;
    }
}

void func_actor_405800_80134C00(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    GpCoord*         coord;

    work  = (Actor405800Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if ((func_actor_405800_801385F4(arg0) << 0x10) != 0) {
        work->field_82   = (work->field_82 + 0x800) & 0xFFF;
        work2            = (Actor405800Work*)arg0->work;
        work2->field_850 = 0x10;
        work2->field_872 = 2;
        work2->field_86E = 2;
        Actor405800_RebuildRotation(arg0);
        Actor405800_TickAnim(arg0);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        func_actor_405800_80138478(arg0, 0xB, &work->field_88);
        work->field_891  = 0;
        work3            = (Actor405800Work*)arg0->work;
        work3->field_846 = 2;
        work3->field_848 = 0;
    }
}

void func_actor_405800_80134E80(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    GpCoord*         coord;
    s32              id;
    s32              sound;
    s32              pan;
    s32              y;

    work  = (Actor405800Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    work->field_842++;
    if ((s16)work->field_842 < 0x11) {
        func_actor_405800_80136A1C(arg0);
        return;
    }
    if ((s16)work->field_842 == 0x11) {
        work->field_85A      = 0;
        work->field_88F      = 1;
        work->obj_594.flags &= ~0x4000;
    }
    if ((u32)(work->field_842 - 0x11) < 4U) {
        work->field_866 = (u16)work->field_866 + (-work->field_866 >> 1);
    }
    if ((s16)work->field_842 == 0x15) {
        work->field_86A = -0x9C4;
    }
    if ((s16)work->field_842 >= 0x15) {
        work->field_866 = (u16)work->field_866 + ((0xFF - work->field_866) >> 1);
    }
    y                  = coord->coord.t[1] + 0x190;
    coord->coord.t[1] += ((s16)work->field_9A - y) >> 3;
    work->field_80    += (0x800 - (s16)work->field_80) >> 3;
    if ((s16)work->field_9A >= coord->coord.t[1]) {
        work->field_866 = 0xFF;
        id              = 0x40050003;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0003;
        }
        sound = id | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
        pan   = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        work->obj_594.flags |= 0x4000;
        coord->coord.t[1]    = (s16)work->field_9A;
        work->field_80       = 0;
        work->field_84       = 0x800;
        work->field_82      += 0x800;
        Actor405800_RebuildRotation(arg0);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        work2            = (Actor405800Work*)arg0->work;
        work2->field_84A = 2;
        work2->field_850 = 0x10;
        work2->field_872 = 0x19;
        work2->field_86E = 1;
        work->field_88F  = 0;
        work->field_890  = 1;
        work->field_848++;
    }
}

void func_actor_405800_801351BC(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    GpCoord*         coord;

    work  = (Actor405800Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    work->field_842++;
    if ((s16)work->field_842 < 8) {
        work->field_866 = (u16)work->field_866 + (-work->field_866 >> 1);
        func_actor_405800_80138478(arg0, 3, &work->field_88);
        return;
    }
    work->field_866   = (u16)work->field_866 + ((0xFF - work->field_866) >> 1);
    work->field_86A   = work->field_92;
    work->field_88.x += ((s16)work->field_98 - work->field_88.x) >> 2;
    work->field_88.z += ((s16)work->field_9C - work->field_88.z) >> 2;
    func_actor_405800_80138514(arg0, 3, &work->field_88);
    work->field_84C   += 2;
    work->field_84E   += work->field_84C;
    coord->coord.t[1] += work->field_84E;
    if ((work->field_80 & 0xFFF) != 0x800) {
        work->field_80 -= 0x80;
    }
    if ((s16)work->field_92 < coord->coord.t[1]) {
        work->field_890   = 0;
        work->field_866   = 0xFF;
        coord->coord.t[0] = (s16)work->field_98;
        coord->coord.t[1] = (s16)work->field_92;
        coord->coord.t[2] = (s16)work->field_9C;
        work->field_80    = 0;
        work->field_84    = 0;
        work->field_82   += 0x800;
        Actor405800_RebuildRotation(arg0);
        work2            = (Actor405800Work*)arg0->work;
        work2->field_84A = 2;
        work2->field_850 = 0x10;
        work2->field_872 = 0x19;
        work2->field_86E = 1;
        Actor405800_TickAnim(arg0);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        work->field_842 = 0;
        work->field_848++;
    }
}

void func_actor_405800_80135558(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    u32              sound;
    s32              pan;
    u32              rnd;

    work = (Actor405800Work*)arg0->work;
    if ((s16)work->field_842 == 0) {
        work->field_88F = 0;
        sound           = 0x40050006 | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
        pan             = Gp_GetObjPan(arg0->extra.tmd->coords) << 24;
        pan           >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        work->field_842++;
    }
    if ((func_actor_405800_801385F4(arg0) << 0x10) != 0) {
        if (work->field_85A != 3) {
            work2            = (Actor405800Work*)arg0->work;
            work2->field_84A = 2;
            work2->field_850 = 0x10;
            work2->field_872 = 0x14;
            work2->field_86E = 1;
            work->field_848++;
            return;
        }
        work->field_85A  = 0;
        rnd              = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState      = rnd;
        work->field_87C  = ((rnd >> 0x10) & 0x7F) + 0x1E;
        work3            = (Actor405800Work*)arg0->work;
        work3->field_846 = 5;
        work3->field_848 = 0;
    }
}

void func_actor_405800_801356A8(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    u16              count;
    u32              rnd;

    work = (Actor405800Work*)arg0->work;
    if (((func_actor_405800_80136A1C(arg0) << 0x10) == 0) && ((func_actor_405800_801373E0(arg0) << 0x10) == 0)) {
        count           = (u16)work->field_882 - 1;
        work->field_882 = count;
        if ((count << 0x10) == 0) {
            rnd             = ((u32)Gp_LcgState * 5) + 0x71357911;
            work->field_838 = ((rnd >> 0x10) & 0x3F) + 0x1E;
            work2           = (Actor405800Work*)arg0->work;
            Gp_LcgState     = rnd;
            if (((s8)work2->field_895 >= 0) || ((work2->field_895 & 0x7F) != 1)) {
                work2->field_895 = 0x81;
                work2->field_896 = 0;
            }
            work3            = (Actor405800Work*)arg0->work;
            work3->field_846 = 2;
            work3->field_848 = 0;
        }
    }
}

/// Spawn the two child models from `D_actor_405800_801514B4`, parent them to
/// root parts 10 and 7 at +/-0x400 along X, turn each by -/+0x180 from an
/// identity rotation, copy the parent's texture page and CLUT row, and point
/// their light / color matrices at this actor's own.
void func_actor_405800_80135780(Task* arg0)
{
    Actor405800Work* work;
    GpCoord*         coord;
    GpCoord*         root;
    GpCoord*         parent;
    GpCoord*         parent2;
    Task*            task;
    TmdObject*       obj;
    TmdObject*       dst;
    TmdObject*       src;
    MATRIX*          mdst;
    OverlayMat*      pm;
    OverlayMat*      pm2;
    OverlayMat       m;

    root              = arg0->extra.tmd->coords;
    work              = (Actor405800Work*)arg0->work;
    parent            = &root[7];
    parent2           = &root[10];
    task              = Task_SpawnFromTable(&D_actor_405800_801514B4, 0, 0, 0);
    work->field_824   = task;
    obj               = task->extra.tmd;
    coord             = obj->coords;
    obj->flags        = 0x80;
    coord->sub        = parent2;
    coord->coord.t[0] = 0x400;
    coord->coord.t[1] = 0;
    coord->coord.t[2] = 0;
    pm                = &m;
    pm->ident.m00_m01 = 0x1000;
    pm->ident.m02_m10 = 0;
    pm->ident.m11_m12 = 0x1000;
    pm->ident.m20_m21 = 0;
    pm->ident.m22     = 0x1000;
    func_8004BFF8(-0x180, &pm->mat);
    mdst          = &coord->coord;
    mdst->m[0][0] = pm->mat.m[0][0];
    mdst->m[0][1] = pm->mat.m[0][1];
    mdst->m[0][2] = pm->mat.m[0][2];
    mdst->m[1][0] = pm->mat.m[1][0];
    mdst->m[1][1] = pm->mat.m[1][1];
    mdst->m[1][2] = pm->mat.m[1][2];
    mdst->m[2][0] = pm->mat.m[2][0];
    mdst->m[2][1] = pm->mat.m[2][1];
    mdst->m[2][2] = pm->mat.m[2][2];
    src           = arg0->extra.tmd;
    dst           = task->extra.tmd;
    dst->tpage    = src->tpage;
    dst->clut     = src->clut;
    if (dst->buffer != NULL) {
        tmdProcessStream(dst);
        tmdProcessStream(dst);
    }
    obj->lightMtx = &work->matrix_40;
    obj->colorMtx = &work->matrix_20;
    task = work->field_828 = Task_SpawnFromTable(&D_actor_405800_801514B4, 1, 0, 0);
    obj                    = task->extra.tmd;
    coord                  = obj->coords;
    obj->flags             = 0x80;
    coord->sub             = parent;
    coord->coord.t[0]      = -0x400;
    coord->coord.t[1]      = 0;
    coord->coord.t[2]      = 0;
    src                    = arg0->extra.tmd;
    dst                    = task->extra.tmd;
    dst->tpage             = src->tpage;
    dst->clut              = src->clut;
    if (dst->buffer != NULL) {
        tmdProcessStream(dst);
        tmdProcessStream(dst);
    }
    pm2                = &m;
    pm2->ident.m00_m01 = 0x1000;
    pm2->ident.m02_m10 = 0;
    pm2->ident.m11_m12 = 0x1000;
    pm2->ident.m20_m21 = 0;
    pm2->ident.m22     = 0x1000;
    func_8004BFF8(0x180, &pm2->mat);
    mdst          = &coord->coord;
    mdst->m[0][0] = pm2->mat.m[0][0];
    mdst->m[0][1] = pm2->mat.m[0][1];
    mdst->m[0][2] = pm2->mat.m[0][2];
    mdst->m[1][0] = pm2->mat.m[1][0];
    mdst->m[1][1] = pm2->mat.m[1][1];
    mdst->m[1][2] = pm2->mat.m[1][2];
    mdst->m[2][0] = pm2->mat.m[2][0];
    mdst->m[2][1] = pm2->mat.m[2][1];
    mdst->m[2][2] = pm2->mat.m[2][2];
    obj->lightMtx = &work->matrix_40;
    obj->colorMtx = &work->matrix_20;
}

/// Animation state 2: the landing slam. Same two sound/tracking windows as
/// `func_actor_400600_80135998`, one frame-count pair per sound event.
void func_actor_405800_80135A3C(Task* arg0, s16 arg1)
{
    Actor405800Work* work;
    GpCoord*         coord;
    /* The first window starts at frame 0. `start0` is still its own `u8`: the
     * width is what folds both of its tests against a literal zero, and the
     * wider first temp below is what keeps the zero arm a fresh constant
     * instead of a copy of it. */
    u8  start0;
    u32 tmp0;
    u8  tmp1;
    u8  tmp2;
    u8  end0;
    u8  start1;
    u8  end1;
    s32 id;
    u32 sound;
    u32 voice;
    s32 pan;

    work  = (Actor405800Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_872 != 2) {
        work->field_850 = 0x10;
        work->field_872 = 2;
        work->field_86E = 2;
        Actor405800_TickAnim(arg0);
    }
    start0 = 0;
    if (((Actor405800Work*)arg0->work)->field_850 == 0) {
        tmp0 = 0;
    } else {
        tmp0 = (u32)(0xB00 / ((Actor405800Work*)arg0->work)->field_850) >> 4;
    }
    end0 = tmp0;
    if (((Actor405800Work*)arg0->work)->field_850 == 0) {
        tmp1 = 0;
    } else {
        tmp1 = (u32)(0xC00 / ((Actor405800Work*)arg0->work)->field_850) >> 4;
    }
    start1 = tmp1;
    if (((Actor405800Work*)arg0->work)->field_850 == 0) {
        tmp2 = 0;
    } else {
        tmp2 = (u32)(0x1500 / ((Actor405800Work*)arg0->work)->field_850) >> 4;
    }
    end1 = tmp2;
    if ((func_actor_405800_80137908(arg0) << 0x10) != 0) {
        work->field_874 = 0;
        work->field_850 = arg1;
    }
    if (work->field_874 == start0) {
        func_actor_405800_80138478(arg0, 0xB, &work->field_88);
        id = 0x40050001;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0001;
        }
        /* `voice` is a plain copy that the compiler propagates away; writing
         * `sound = id | sound` instead swaps the operands of the `or`. */
        sound   = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        voice   = sound;
        sound   = id | voice;
        pan     = Gp_GetObjPan(arg0->extra.tmd->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if (work->field_874 == start1) {
        func_actor_405800_80138478(arg0, 8, &work->field_88);
        id = 0x40050002;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0002;
        }
        sound   = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        voice   = sound;
        sound   = id | voice;
        pan     = Gp_GetObjPan(arg0->extra.tmd->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if (work->field_874 >= start0 && work->field_874 <= end0) {
        func_actor_405800_80138514(arg0, 0xB, &work->field_88);
        work->field_860 = 8;
    }
    if (work->field_874 >= start1 && work->field_874 <= end1) {
        func_actor_405800_80138514(arg0, 8, &work->field_88);
        work->field_860 = 0xB;
    }
    coord->flg = 0;
}

/// Animation state 4: drives the two sound/tracking windows the same way
/// `func_actor_400600_80135DDC` does, one frame-count pair per sound event.
void func_actor_405800_80135E28(Task* arg0)
{
    Actor405800Work* work;
    GpCoord*         coord;
    u8               start0;
    u32              tmp0;
    u8               tmp1;
    u8               tmp2;
    u8               end0;
    u8               start1;
    u8               end1;
    s32              id;
    u32              sound;
    u32              voice;
    s32              pan;

    work  = (Actor405800Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_872 != 4) {
        work->field_850 = 0x10;
        work->field_84A = 4;
        work->field_872 = 4;
        work->field_86E = 1;
        Actor405800_TickAnim(arg0);
    }
    start0 = 0;
    if (((Actor405800Work*)arg0->work)->field_850 == 0) {
        tmp0 = 0;
    } else {
        tmp0 = (u32)(0xD00 / ((Actor405800Work*)arg0->work)->field_850) >> 4;
    }
    end0 = tmp0;
    if (((Actor405800Work*)arg0->work)->field_850 == 0) {
        tmp1 = 0;
    } else {
        tmp1 = (u32)(0xE00 / ((Actor405800Work*)arg0->work)->field_850) >> 4;
    }
    start1 = tmp1;
    if (((Actor405800Work*)arg0->work)->field_850 == 0) {
        tmp2 = 0;
    } else {
        tmp2 = (u32)(0x1B00 / ((Actor405800Work*)arg0->work)->field_850) >> 4;
    }
    end1 = tmp2;
    if ((func_actor_405800_801385F4(arg0) << 0x10) != 0) {
        work->field_874 = 0;
    }
    if (work->field_874 == start0) {
        func_actor_405800_80138478(arg0, 8, &work->field_88);
        id = 0x40050001;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0001;
        }
        sound   = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        voice   = sound;
        sound   = id | voice;
        pan     = Gp_GetObjPan(arg0->extra.tmd->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if (work->field_874 == start1) {
        func_actor_405800_80138478(arg0, 0xB, &work->field_88);
        id = 0x40050002;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0002;
        }
        sound   = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        voice   = sound;
        sound   = id | voice;
        pan     = Gp_GetObjPan(arg0->extra.tmd->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if (work->field_874 >= start0 && work->field_874 <= end0) {
        func_actor_405800_80138514(arg0, 8, &work->field_88);
    }
    if (work->field_874 >= start1 && work->field_874 <= end1) {
        func_actor_405800_80138514(arg0, 0xB, &work->field_88);
    }
    coord->flg = 0;
}

void func_actor_405800_801361F8(Task* arg0)
{
    Actor405800Work* work;
    GpCoord*         coord;
    GpCoord*         player;
    GameActor*       actor;
    SVECTOR          v;
    s16              py;

    work              = (Actor405800Work*)arg0->work;
    coord             = arg0->extra.tmd->coords;
    arg0              = (Task*)Gp_ActorSlots[0];
    work->field_70.vx = coord->coord.t[0];
    work->field_70.vy = coord->coord.t[1];
    work->field_70.vz = coord->coord.t[2];
    if (arg0 == NULL) {
        return;
    }
    player = arg0->extra.tmd->coords;
    actor  = arg0->work;
    if (player->coord.t[0] < 0x3A98 || Gp_StateF0.field_0 == 0) {
        work->field_A8.vx = (u16)player->coord.t[0];
        work->field_A8.vy = (u16)player->coord.t[1];
        work->field_A8.vz = (u16)player->coord.t[2];
    } else {
        work->field_A8.vx = 0x834;
        py                = (u16)player->coord.t[1];
        work->field_A8.vz = 0xD48;
        work->field_85C   = 2;
        work->field_A8.vy = py;
    }
    v.vx            = (u16)work->field_A8.vx - (u16)coord->coord.t[0];
    v.vy            = (u16)work->field_A8.vy - (u16)coord->coord.t[1];
    v.vz            = (u16)work->field_A8.vz - (u16)coord->coord.t[2];
    work->field_852 = SquareRoot0(v.vx * v.vx + v.vz * v.vz);
    VectorNormalSS(&v, &v);
    work->field_856 = (ratan2(v.vx, v.vz) - work->field_82) & 0xFFF;
    work->field_854 = (ratan2(-v.vx, -v.vz) - actor->field_52) & 0xFFF;
}

void func_actor_405800_80136388(Task* arg0)
{
    GpDeltaScratch   delta;
    s16              maxX;
    s16              maxZ;
    s16              stepX;
    s16              stepZ;
    GpCoord*         coord;
    u8               blocked;
    Actor405800Work* work;
    GpEnemy*         enemy;
    s16              amount;
    s32              dmg;
    s32              tmp;
    s16              tick;
    s32              i;
    s32              two;

    maxX            = 0;
    maxZ            = 0;
    stepX           = 0;
    stepZ           = 0;
    two             = 2;
    blocked         = 0;
    coord           = arg0->extra.tmd->coords;
    work            = (Actor405800Work*)arg0->work;
    enemy           = (GpEnemy*)arg0->spawnArg2;
    work->field_858 = 0;

    for (i = 0; i < 8; i++) {
        if ((work->rec_4D4[i].key & 0xFFFF0000) == 0x20000) {
            if (work->field_876 == 0) {
                work->field_858 = 1;
                dmg             = Gp_ComputeDamage(work->rec_4D4[i].key, work->field_852, 0, 0);
                amount          = dmg;
                work->field_876 = Gp_GetIdParam2(work->rec_4D4[i].key);
                if (Gp_RollEnemyChance(enemy, work->rec_4D4[i].key, 0) != 0) {
                    amount = ((u32)dmg << 16) >> 14;
                    Gp_SpawnEff(0x6009C, &arg0->extra.tmd->coords[3], 0, NULL);
                }
                func_800E2C78(enemy, work->rec_4D4[i].key, amount, 0);
                func_800DA6E8(&enemy->node, amount, 0);
                enemy->hp -= amount;
                if (enemy->hp < 0) {
                    enemy->hp = 0;
                }
                func_800FDB18(Gp_GetIdParam1(work->rec_4D4[i].key) & 0xFFFF,
                              &arg0->extra.tmd->coords[4], NULL, &work->eff_81C);
                if (amount >= 0xB4) {
                    work->field_85A = two;
                } else if (amount >= 0x78) {
                    work->field_85A = 1;
                } else {
                    work->field_85A = 0;
                }
                SOFT_USE_REG(two);
                SOFT_USE_REG(two);
                switch (Gp_GetIdParam0(work->rec_4D4[i].key) & 0xFFFF) {
                    case 0:
                        break;
                    case 1:
                        Gp_SetObjFlag1(enemy);
                        break;
                    case 2:
                        Gp_SetObjFlag2(enemy, work->rec_4D4[i].key, 0);
                        work->field_898 = two;
                        break;
                    case 3:
                        Gp_SetObjFlag4(enemy, work->rec_4D4[i].key, 0);
                        break;
                    case 4:
                    case 6:
                        work->field_85A = 4;
                        break;
                    case 5:
                    case 7:
                        work->field_85A = two;
                        break;
                    case 8:
                    case 9:
                        if (work->field_898 != 2) {
                            work->field_898 = 1;
                            work->field_85A = 3;
                        }
                        break;
                }
            } else if ((Gp_GetIdParam1(work->rec_4D4[i].key) & 0xFFFF) == 0xD) {
                func_800FDB18(0xD, &arg0->extra.tmd->coords[1], NULL, &work->eff_81C);
            }
        }
    }

    if (enemy->reactionFlags & 1) {
        enemy->reactionFlags &= 0xFE;
        work->field_85A       = 5;
    }
    if (enemy->reactionFlags & 2) {
        enemy->reactionFlags &= 0xFD;
        work->field_85A       = 3;
        work->field_898       = 2;
    }
    if (enemy->reactionFlags & 0xC) {
        work->field_888 = 1;
        tmp             = Gp_TickObjFlag4(enemy);
        tick            = tmp;
        if (tick != 0) {
            enemy->hp -= tmp;
            func_800DA6E8(&enemy->node, tick, 0);
            if (enemy->hp < 0) {
                enemy->hp = 0;
            }
            work->field_858 = 1;
            work->field_85A = 2;
            SOFT_USE_REG(two);
        }
        if (Gp_ObjFlag4Expired(enemy) != 0) {
            enemy->reactionFlags &= 0xF3;
        }
    }

    switch (func_800E0C10(work->rec_5B4, &delta, 8, NULL)) {
        case 0:
            break;
        case 1:
            stepZ = delta.vz.h.hi;
            stepX = delta.vx.w >> 16;
            if (delta.vx.w & 0xFFFF) {
                if (delta.vx.w > 0) {
                    stepX++;
                } else {
                    stepX--;
                }
            }
            if (delta.vz.w & 0xFFFF) {
                if (delta.vz.w > 0) {
                    stepZ++;
                } else {
                    stepZ--;
                }
            }
            break;
        case 2:
            coord->coord.t[0] = work->field_70.vx;
            blocked           = 1;
            coord->coord.t[2] = work->field_70.vz;
            break;
    }

    Gp_ClearRec18Occupied(work->rec_4D4);
    Gp_ClearRec18Occupied(work->rec_5B4);
    if (work->field_876 > 0) {
        work->field_876--;
    } else {
        work->field_876 = 0;
    }
    if (blocked == 0) {
        work->field_88.x  += actorPickStep(stepX, maxX >> 3);
        work->field_88.z  += actorPickStep(stepZ, maxZ >> 3);
        coord->coord.t[0] += actorPickStep(stepX, (u16)maxX >> 3);
        coord->coord.t[2] += actorPickStep(stepZ, (u16)maxZ >> 3);
        coord->flg         = 0;
    }
}

s32 func_actor_405800_80136A1C(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work = (Actor405800Work*)arg0->work;
    if (work->field_858 == 1) {
        if (work->field_890 == 0) {
            switch (work->field_85A) {
                case 1:
                    work->field_85A  = 0;
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 3;
                    work2->field_848 = 0;
                    return 1;
                case 2:
                    work->field_85A  = 0;
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 4;
                    work2->field_848 = 0;
                    return 1;
                case 3:
                    work->field_85A  = 0;
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 5;
                    work2->field_848 = 0;
                    return 1;
                case 4:
                    work->field_85A  = 0;
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 4;
                    work2->field_848 = 0;
                    return 1;
                case 5:
                    work->field_85A  = 0;
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 0xF;
                    work2->field_848 = 0;
                    return 1;
            }
            return 0;
        } else {
            switch (work->field_85A) {
                case 1:
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 3;
                    work2->field_848 = 0;
                    work->field_85A  = 0;
                    return 1;
                case 2:
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 4;
                    work2->field_848 = 0;
                    work->field_85A  = 0;
                    return 1;
                case 3:
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 0xE;
                    work2->field_848 = 0;
                    return 1;
                case 4:
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 4;
                    work2->field_848 = 0;
                    work->field_85A  = 0;
                    return 1;
                case 5:
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 0xE;
                    work2->field_848 = 0;
                    work->field_85A  = 0;
                    return 1;
            }
            return 0;
        }
    }
    return 0;
}

s32 func_actor_405800_80136B94(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work = (Actor405800Work*)arg0->work;
    if (work->field_890 == 0) {
        switch (work->field_85A) {
            case 1:
                work2            = (Actor405800Work*)arg0->work;
                work2->field_846 = 3;
                work2->field_848 = 0;
                work->field_85A  = 0;
                return 1;
            case 2:
                work2            = (Actor405800Work*)arg0->work;
                work2->field_846 = 4;
                work2->field_848 = 0;
                work->field_85A  = 0;
                return 1;
            case 3:
                work2            = (Actor405800Work*)arg0->work;
                work2->field_846 = 5;
                work2->field_848 = 0;
                work->field_85A  = 0;
                return 1;
            case 4:
                work2            = (Actor405800Work*)arg0->work;
                work2->field_846 = 4;
                work2->field_848 = 0;
                work->field_85A  = 0;
                return 1;
            case 5:
                work2            = (Actor405800Work*)arg0->work;
                work2->field_846 = 0xF;
                work2->field_848 = 0;
                work->field_85A  = 0;
                return 1;
        }
        work->field_85A = 0;
        return 0;
    } else {
        switch (work->field_85A) {
            case 1:
                work2            = (Actor405800Work*)arg0->work;
                work2->field_846 = 3;
                work2->field_848 = 0;
                work->field_85A  = 0;
                return 1;
            case 2:
                work2            = (Actor405800Work*)arg0->work;
                work2->field_846 = 4;
                work2->field_848 = 0;
                work->field_85A  = 0;
                return 1;
            case 3:
                work2            = (Actor405800Work*)arg0->work;
                work2->field_846 = 0xE;
                work2->field_848 = 0;
                return 1;
            case 4:
                work2            = (Actor405800Work*)arg0->work;
                work2->field_846 = 4;
                work2->field_848 = 0;
                work->field_85A  = 0;
                return 1;
            case 5:
                work2            = (Actor405800Work*)arg0->work;
                work2->field_846 = 0xE;
                work2->field_848 = 0;
                work->field_85A  = 0;
                return 1;
        }
        work->field_85A = 0;
        return 0;
    }
}

s32 func_actor_405800_80136CE0(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work = (Actor405800Work*)arg0->work;
    if (work->field_858 == 1) {
        if (work->field_890 == 0) {
            switch (work->field_85A) {
                case 1:
                    work->field_85A = 0;
                    return 0;
                case 2:
                    work->field_85A  = 0;
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 4;
                    work2->field_848 = 0;
                    return 1;
                case 3:
                    work->field_85A  = 0;
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 5;
                    work2->field_848 = 0;
                    return 1;
                case 4:
                    work->field_85A  = 0;
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 4;
                    work2->field_848 = 0;
                    return 1;
                case 5:
                    work->field_85A  = 0;
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 0xF;
                    work2->field_848 = 0;
                    return 1;
            }
            work->field_85A = 0;
            return 0;
        } else {
            switch (work->field_85A) {
                case 1:
                    work->field_85A = 0;
                    return 0;
                case 2:
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 4;
                    work2->field_848 = 0;
                    work->field_85A  = 0;
                    return 1;
                case 3:
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 0xE;
                    work2->field_848 = 0;
                    return 1;
                case 4:
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 4;
                    work2->field_848 = 0;
                    work->field_85A  = 0;
                    return 1;
                case 5:
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 0xE;
                    work2->field_848 = 0;
                    work->field_85A  = 0;
                    return 1;
            }
            return 0;
        }
    }
    return 0;
}

void func_actor_405800_80136E14(Task* task)
{
    GpEffWork* eff;
    GpEffWork* eff2;
    GpEffWork* eff3;
    GpEffWork* eff4;
    TmdObject* dst;
    TmdObject* dst2;
    TmdObject* dst3;
    TmdObject* dst4;
    TmdObject* src;
    TmdObject* src2;
    TmdObject* src3;
    TmdObject* src4;

    D_800678F0[0] = &D_actor_405800_8013FB18;
    eff           = Gp_SpawnEff(0x20010, &task->extra.tmd->coords[5], 0x200, NULL);
    if (eff != NULL) {
        src        = task->extra.tmd;
        dst        = eff->task->extra.tmd;
        dst->tpage = src->tpage;
        dst->clut  = src->clut;
        if (dst->buffer != NULL) {
            tmdProcessStream(dst);
            tmdProcessStream(dst);
        }
    }
    D_800678F0[0] = &D_actor_405800_8014086C;
    eff2          = Gp_SpawnEff(0x20010, &task->extra.tmd->coords[13], 0x200, NULL);
    if (eff2 != NULL) {
        src2        = task->extra.tmd;
        dst2        = eff2->task->extra.tmd;
        dst2->tpage = src2->tpage;
        dst2->clut  = src2->clut;
        if (dst2->buffer != NULL) {
            tmdProcessStream(dst2);
            tmdProcessStream(dst2);
        }
    }
    D_800678F0[0] = &D_actor_405800_80140F10;
    eff3          = Gp_SpawnEff(0x20010, &task->extra.tmd->coords[16], 0x200, NULL);
    if (eff3 != NULL) {
        src3        = task->extra.tmd;
        dst3        = eff3->task->extra.tmd;
        dst3->tpage = src3->tpage;
        dst3->clut  = src3->clut;
        if (dst3->buffer != NULL) {
            tmdProcessStream(dst3);
            tmdProcessStream(dst3);
        }
    }
    D_800678F0[0] = &D_actor_405800_80141430;
    eff4          = Gp_SpawnEff(0x20010, &task->extra.tmd->coords[10], 0x200, NULL);
    if (eff4 != NULL) {
        src4        = task->extra.tmd;
        dst4        = eff4->task->extra.tmd;
        dst4->tpage = src4->tpage;
        dst4->clut  = src4->clut;
        if (dst4->buffer != NULL) {
            tmdProcessStream(dst4);
            tmdProcessStream(dst4);
        }
    }
    Gp_SpawnEff(0x60030, &task->extra.tmd->coords[1], 0x200, NULL);
    Gp_SpawnEff(0x60030, &task->extra.tmd->coords[2], 0x200, NULL);
    Gp_SpawnEff(0x60030, &task->extra.tmd->coords[3], 0x200, NULL);
}

void func_actor_405800_8013706C(Task* arg0, s16 arg1)
{
    Actor405800Work* work = (Actor405800Work*)arg0->work;
    SVECTOR          v;
    SVECTOR          out;
    OverlayMat       rot;
    s16              n;

    work->field_892 = arg1;
    switch (arg1) {
        case 0:
            if (work->field_890 == 0) {
                OverlayMat* m = &rot;

                v.vx              = work->field_A8.vx - arg0->extra.tmd->coords->coord.t[0];
                v.vy              = work->field_A8.vy - arg0->extra.tmd->coords->coord.t[1] - 0x384;
                v.vz              = work->field_A8.vz - arg0->extra.tmd->coords->coord.t[2];
                rot.ident.m00_m01 = 0x1000;
                rot.ident.m02_m10 = 0;
                m->ident.m11_m12  = 0x1000;
                rot.ident.m20_m21 = 0;
                m->ident.m22      = 0x1000;
                rot.mat.t[0]      = 0;
                rot.mat.t[1]      = 0;
                rot.mat.t[2]      = 0;
                func_8004BFF8(-(s16)work->field_82, &m->mat);
                ApplyMatrixSV(&m->mat, &v, &out);
            } else {
                OverlayMat* m = &rot;

                v.vx              = work->field_A8.vx - arg0->extra.tmd->coords->coord.t[0];
                v.vy              = work->field_A8.vy - arg0->extra.tmd->coords->coord.t[1] - 0x640;
                v.vz              = work->field_A8.vz - arg0->extra.tmd->coords->coord.t[2];
                rot.ident.m00_m01 = 0x1000;
                rot.ident.m02_m10 = 0;
                m->ident.m11_m12  = 0x1000;
                rot.ident.m20_m21 = 0;
                m->ident.m22      = 0x1000;
                rot.mat.t[0]      = 0;
                rot.mat.t[1]      = 0;
                rot.mat.t[2]      = 0;
                func_8004BFF8(-(s16)work->field_82, &m->mat);
                RotMatrixZ(-(s16)work->field_84, &m->mat);
                ApplyMatrixSV(&m->mat, &v, &out);
            }
            work->rec_744.end0.vx    = out.vx;
            work->rec_744.end0.vy    = out.vy;
            n                        = out.vz;
            work->rec_744.end0.vz    = n;
            n                        = 0xA;
            work->rec_744.end0Radius = n;
            work->rec_744.end1Radius = n;
            break;
        case 1:
            work->rec_744.end0.vx    = 0;
            work->rec_744.end0.vy    = 0x190;
            work->rec_744.end0.vz    = -0x1770;
            work->rec_744.end0Radius = 0x50;
            work->rec_744.end1Radius = 0x50;
            break;
    }
    work->rec_744.end1.vx = 0;
    work->rec_744.end1.vy = 0;
    work->rec_744.end1.vz = 0;
    Gp_ClearRec18Occupied(work->rec_75C);
    work->obj_724.flags |= 0x4000;
}

s32 func_actor_405800_8013728C(Task* arg0)
{
    Actor405800Work* work;
    GpCoord*         coord;
    SVECTOR          v;
    s16              dist;
    s32              i;

    dist  = 0;
    work  = (Actor405800Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    for (i = 0; i < 8; i++) {
        if ((work->rec_75C[i].key & 0xFFFF0000) != 0x100000) {
            dist = 0;
        } else {
            if (work->field_892 == 0) {
                dist = 1;
            } else if (work->field_892 == 1) {
                v.vx = work->rec_75C[i].point.vx - coord->workm.t[0];
                v.vy = 0;
                v.vz = work->rec_75C[i].point.vz - coord->workm.t[2];
                dist = SquareRoot0(v.vx * v.vx + v.vz * v.vz);
                if (dist == 0) {
                    dist = 1;
                }
            } else if (work->field_892 == 2) {
                v.vx = work->rec_75C[i].point.vx - coord->workm.t[0];
                v.vy = work->rec_75C[i].point.vy - coord->workm.t[1];
                v.vz = 0;
                dist = SquareRoot0(v.vx * v.vx + v.vy * v.vy);
                if (dist == 0) {
                    dist = 1;
                }
            }
            break;
        }
    }
    Gp_ClearRec18Occupied(work->rec_75C);
    return dist;
}

s32 func_actor_405800_801373E0(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    Actor405800Work* work4;
    Actor405800Work* work5;
    Actor405800Work* work6;
    Actor405800Work* work7;
    Actor405800Work* work8;
    GpCoord*         coord;
    u32              rnd;
    u32              bits;
    s16              ang;
    s32              ret;

    rnd         = ((u32)Gp_LcgState * 5) + 0x71357911;
    bits        = rnd >> 0x10;
    Gp_LcgState = rnd;
    work        = (Actor405800Work*)arg0->work;
    coord       = arg0->extra.tmd->coords;
    if (work->field_85C == 0) {
        if (work->field_890 == 0) {
            if ((bits & 0xF) == 0) {
                if ((work->field_85E == 0) && (coord->coord.t[0] < 0x2710)) {
                    work->field_846 = 0xC;
                    work->field_848 = 0;
                    return 1;
                }
                return 0;
            }
            if ((u32)((bits & 7) - 1) < 3U) {
                if ((work->field_852 < 0x7D0) && ((u32)(work->field_856 - 0x200) >= 0xC01U) && ((u32)(work->field_854 - 0x601) < 0x3FFU)) {
                    ret = 1;
                    TOUCH_REG_MEM(ret);
                    work2            = (Actor405800Work*)arg0->work;
                    work2->field_846 = 8;
                    work2->field_848 = 0;
                    return ret;
                }
            } else if (work->field_852 < 0x640) {
                ang = (s16)work->field_856;
                if (ang < 0x400) {
                    ret = 1;
                    TOUCH_REG_MEM(ret);
                    work3            = (Actor405800Work*)arg0->work;
                    work3->field_846 = 6;
                    work3->field_848 = 0;
                    return ret;
                }
                if (ang >= 0xC01) {
                    ret = 1;
                    TOUCH_REG_MEM(ret);
                    work4            = (Actor405800Work*)arg0->work;
                    work4->field_846 = 7;
                    work4->field_848 = 0;
                    return ret;
                }
            }
        } else if ((bits & 7) == 0) {
            if (work->field_85E == 0) {
                ret = 1;
                TOUCH_REG_MEM(ret);
                work5            = (Actor405800Work*)arg0->work;
                work5->field_846 = 0x10;
                work5->field_848 = 0;
                return ret;
            }
            return 0;
        } else if ((bits & 0xF) == 1) {
            if (work->field_85E == 0) {
                ret = 1;
                TOUCH_REG_MEM(ret);
                work6            = (Actor405800Work*)arg0->work;
                work6->field_846 = 0xD;
                work6->field_848 = 0;
                return ret;
            }
            return 0;
        } else if (work->field_852 < 0x640) {
            ang = (s16)work->field_856;
            if (ang >= 0xC01) {
                ret = 1;
                TOUCH_REG_MEM(ret);
                work7            = (Actor405800Work*)arg0->work;
                work7->field_846 = 6;
                work7->field_848 = 0;
                return ret;
            }
            if (ang < 0x400) {
                ret = 1;
                TOUCH_REG_MEM(ret);
                work8            = (Actor405800Work*)arg0->work;
                work8->field_846 = 7;
                work8->field_848 = 0;
                return ret;
            }
            return 0;
        } else {
            return 0;
        }
    }
    return 0;
}

void func_actor_405800_801375C4(Task* arg0)
{
    Actor405800Work* work;
    Task*            child;
    TmdObject*       extra;
    OverlayMat       rot;
    OverlayMat*      m;
    OverlayMat*      m2;
    s16              angle;
    s16*             p;

    work = (Actor405800Work*)arg0->work;
    if ((u8)work->field_88D != 0) {
        angle                   = (u16)work->field_87A + ((0x380 - work->field_87A) >> 2);
        work->field_87A         = angle;
        child                   = ((Actor405800Work*)arg0->work)->field_828;
        child->extra.tmd->flags = 0;
        extra                   = child->extra.tmd;
        p                       = (s16*)extra->coords;
        m                       = &rot;
        rot.ident.m00_m01       = 0x1000;
        rot.ident.m02_m10       = 0;
        m->ident.m11_m12        = 0x1000;
        rot.ident.m20_m21       = 0;
        m->ident.m22            = 0x1000;
        func_8004BFF8(angle, &m->mat);
        p[2] = rot.mat.m[0][0];
        p   += 2;
        p[1] = rot.mat.m[0][1];
        p[2] = rot.mat.m[0][2];
        p[3] = rot.mat.m[1][0];
        p[4] = rot.mat.m[1][1];
        p[5] = rot.mat.m[1][2];
        p[6] = rot.mat.m[2][0];
        p[7] = rot.mat.m[2][1];
        p[8] = rot.mat.m[2][2];
    } else {
        work->obj_6B4.flags &= 0x7FFF;
        work->obj_674.flags &= 0x7FFF;
        angle                = (u16)work->field_87A + (-work->field_87A >> 3);
        work->field_87A      = angle;
        if (angle < 9) {
            ((Actor405800Work*)arg0->work)->field_828->extra.tmd->flags = 0x80;
        } else {
            child                   = ((Actor405800Work*)arg0->work)->field_828;
            child->extra.tmd->flags = 0;
            extra                   = child->extra.tmd;
            p                       = (s16*)extra->coords;
            m                       = &rot;
            rot.ident.m00_m01       = 0x1000;
            rot.ident.m02_m10       = 0;
            m->ident.m11_m12        = 0x1000;
            rot.ident.m20_m21       = 0;
            m->ident.m22            = 0x1000;
            func_8004BFF8(angle, &m->mat);
            p[2] = rot.mat.m[0][0];
            p   += 2;
            p[1] = rot.mat.m[0][1];
            p[2] = rot.mat.m[0][2];
            p[3] = rot.mat.m[1][0];
            p[4] = rot.mat.m[1][1];
            p[5] = rot.mat.m[1][2];
            p[6] = rot.mat.m[2][0];
            p[7] = rot.mat.m[2][1];
            p[8] = rot.mat.m[2][2];
        }
    }

    if ((u8)work->field_88E != 0) {
        angle                   = (u16)work->field_878 + ((0x380 - work->field_878) >> 2);
        work->field_878         = angle;
        child                   = ((Actor405800Work*)arg0->work)->field_824;
        angle                   = -angle;
        child->extra.tmd->flags = 0;
        extra                   = child->extra.tmd;
        p                       = (s16*)extra->coords;
        m                       = &rot;
        rot.ident.m00_m01       = 0x1000;
        rot.ident.m02_m10       = 0;
        m->ident.m11_m12        = 0x1000;
        rot.ident.m20_m21       = 0;
        m->ident.m22            = 0x1000;
        func_8004BFF8(angle, &m->mat);
        p[2] = rot.mat.m[0][0];
        p   += 2;
        p[1] = rot.mat.m[0][1];
        p[2] = rot.mat.m[0][2];
        p[3] = rot.mat.m[1][0];
        p[4] = rot.mat.m[1][1];
        p[5] = rot.mat.m[1][2];
        p[6] = rot.mat.m[2][0];
        p[7] = rot.mat.m[2][1];
        p[8] = rot.mat.m[2][2];
    } else {
        work->obj_6D4.flags &= 0x7FFF;
        work->obj_694.flags &= 0x7FFF;
        angle                = (u16)work->field_878 + (-work->field_878 >> 3);
        work->field_878      = angle;
        if (angle < 9) {
            ((Actor405800Work*)arg0->work)->field_824->extra.tmd->flags = 0x80;
        } else {
            child                   = ((Actor405800Work*)arg0->work)->field_824;
            child->extra.tmd->flags = 0;
            extra                   = child->extra.tmd;
            p                       = (s16*)extra->coords;
            m2                      = &rot;
            rot.ident.m00_m01       = 0x1000;
            rot.ident.m02_m10       = 0;
            m2->ident.m11_m12       = 0x1000;
            rot.ident.m20_m21       = 0;
            m2->ident.m22           = 0x1000;
            func_8004BFF8((s16)-angle, &m2->mat);
            p[2] = rot.mat.m[0][0];
            p   += 2;
            p[1] = rot.mat.m[0][1];
            p[2] = rot.mat.m[0][2];
            p[3] = rot.mat.m[1][0];
            p[4] = rot.mat.m[1][1];
            p[5] = rot.mat.m[1][2];
            p[6] = rot.mat.m[2][0];
            p[7] = rot.mat.m[2][1];
            p[8] = rot.mat.m[2][2];
        }
    }

    if ((u32)(work->field_846 - 6) >= 2U) {
        work->obj_6B4.flags &= 0x7FFF;
        work->obj_6D4.flags &= 0x7FFF;
        work->obj_674.flags &= 0x7FFF;
        work->obj_694.flags &= 0x7FFF;
    }
}

s32 func_actor_405800_80137908(Task* arg0)
{
    Actor405800Work* work = (Actor405800Work*)arg0->work;

    if ((work->flags_83C.half & 1) || (work->flags_83C.word & 0x102)) {
        return 1;
    }
    return 0;
}

void func_actor_405800_80137948(Task* task)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->work;
    work->field_88E = 0;
    work->field_88D = 0;
}

void func_actor_405800_8013795C(Task* task)
{
    Actor405800Work* work;

    work = (Actor405800Work*)task->work;
    if (work->field_85C > 0) {
        work->field_85C = work->field_85C - 1;
    }
    if (work->field_85E > 0) {
        work->field_85E = work->field_85E - 1;
    }
}

void func_actor_405800_80137994(Task* arg0, s16 arg1)
{
    Actor405800Work* work;
    u32              rnd1;
    u32              rnd2;

    work = (Actor405800Work*)arg0->work;
    if ((arg1 << 16) != 0) {
        rnd1            = ((u32)Gp_LcgState * 5) + 0x71357911;
        rnd2            = (rnd1 * 5) + 0x71357911;
        Gp_LcgState     = rnd2;
        work->field_85C = arg1 + ((rnd1 >> 0x10) & 0x3F) + ((rnd2 >> 0x10) & 0xF);
        return;
    }
    work->field_85C = 0;
}

void func_actor_405800_801379F8(Task* task)
{
    Actor405800Work* work;

    work                = (Actor405800Work*)task->work;
    work->obj_724.flags = work->obj_724.flags & 0xBFFF;
}

void func_actor_405800_80137A14(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* cur;

    work = (Actor405800Work*)task->work;
    if (((s8)work->field_895 >= 0 || (work->field_895 & 0x7F)) && work->field_83A == 0) {
        work->field_895 = 0x80;
        work->field_896 = 0;
    }
    cur            = (Actor405800Work*)task->work;
    cur->field_846 = 1;
    cur->field_848 = 0;
}

/// Per-frame entry point for one of this actor's states: clears the animation
/// request flags, then runs the sub-state handler `field_848` selects unless
/// `func_actor_405800_80136A1C` or `func_actor_405800_801373E0` already
/// consumed the frame. After the handler, a set `field_890` plus a root
/// world X past 10000 switches to state 0xD. The two-entry table is small
/// enough that GCC materialises each callback with its own `lui`/`addiu`
/// pair instead of copying a `.rodata` pool.
void func_actor_405800_80137A60(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->work;
    TaskFunc         states[2] = { func_actor_405800_80138FA8, func_actor_405800_8013902C };

    func_actor_405800_80137948(task);
    if ((s16)func_actor_405800_80136A1C(task) == 0 && (s16)func_actor_405800_801373E0(task) == 0) {
        states[(s16)work->field_848](task);
        if (work->field_890 != 0 && task->extra.tmd->coords->coord.t[0] > 10000) {
            Actor405800Work* cur = (Actor405800Work*)task->work;

            cur->field_846 = 0xD;
            cur->field_848 = 0;
        }
    }
}

/// Per-frame entry point for one of this actor's states: clears the animation
/// request flags, then runs the sub-state handler `field_848` selects. The
/// two-entry table is small enough that GCC materialises each callback with its
/// own `lui`/`addiu` pair instead of copying a `.rodata` pool.
void func_actor_405800_80137B34(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->work;
    TaskFunc         states[2] = { func_actor_405800_801390FC, func_actor_405800_80139188 };

    func_actor_405800_80137948(task);
    states[(s16)work->field_848](task);
}

void func_actor_405800_80137B9C(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->work;
    TaskFunc         states[2] = { func_actor_405800_80139260, func_actor_405800_801392EC };

    func_actor_405800_80137948(task);
    states[(s16)work->field_848](task);
}

/// Per-frame handler for one of this actor's behaviours: clears `field_88D` /
/// `field_88E` through `func_actor_405800_80137948`, then runs the sub-state
/// handler of `D_actor_405800_80131EAC` that `field_848` selects.
void func_actor_405800_80137C04(Task* task)
{
    Actor405800Work* work   = (Actor405800Work*)task->work;
    TaskFuncTable3   states = D_actor_405800_80131EAC;

    func_actor_405800_80137948(task);
    states.funcs[(s16)work->field_848](task);
}

void func_actor_405800_80137C78(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->work;
    TaskFunc         states[2] = { func_actor_405800_80139550, func_actor_405800_80133DB0 };

    if ((s16)func_actor_405800_80136CE0(task) == 0) {
        states[(s16)work->field_848](task);
    }
}

void func_actor_405800_80137CEC(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->work;
    TaskFunc         states[2] = { func_actor_405800_801395E8, func_actor_405800_80133F48 };

    if ((s16)func_actor_405800_80136CE0(task) == 0) {
        states[(s16)work->field_848](task);
    }
}

void func_actor_405800_80137D60(Task* task)
{
    Actor405800Work* work   = (Actor405800Work*)task->work;
    TaskFuncTable5   states = D_actor_405800_80131EB8;

    func_actor_405800_80137948(task);
    states.funcs[(s16)work->field_848](task);
}

void func_actor_405800_80137DE4(Task* task)
{
    Actor405800Work* work   = (Actor405800Work*)task->work;
    TaskFuncTable4   states = D_actor_405800_80131ECC;

    func_actor_405800_80137948(task);
    states.funcs[(s16)work->field_848](task);
}

void func_actor_405800_80137E64(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    s16              count;

    work = (Actor405800Work*)task->work;
    func_actor_405800_80137948(task);
    if ((s16)func_actor_405800_80136A1C(task) == 0) {
        count           = work->field_87C - 1;
        work->field_87C = count;
        if (count == 0) {
            work2            = (Actor405800Work*)task->work;
            work2->field_846 = 0xB;
            work2->field_848 = 0;
            return;
        }
        func_actor_405800_801383CC(task, &work->field_A8, 0x18);
        func_actor_405800_80135E28(task);
    }
}

void func_actor_405800_80137EF0(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->work;
    TaskFunc         states[2] = { func_actor_405800_80139844, func_actor_405800_80134C00 };

    func_actor_405800_80137948(task);
    states[(s16)work->field_848](task);
}

void func_actor_405800_80137F58(Task* task)
{
    Actor405800Work* work   = (Actor405800Work*)task->work;
    TaskFuncTable3   states = D_actor_405800_80131EDC;

    func_actor_405800_80137948(task);
    states.funcs[(s16)work->field_848](task);
}

void func_actor_405800_80137FCC(Task* task)
{
    Actor405800Work* work   = (Actor405800Work*)task->work;
    TaskFuncTable3   states = D_actor_405800_80131EE8;

    func_actor_405800_80137948(task);
    states.funcs[(s16)work->field_848](task);
}

void func_actor_405800_80138040(Task* task)
{
    Actor405800Work* work   = (Actor405800Work*)task->work;
    TaskFuncTable4   states = D_actor_405800_80131EF4;

    func_actor_405800_80137948(task);
    states.funcs[(s16)work->field_848](task);
}

void func_actor_405800_801380C0(Task* task)
{
    Actor405800Work* work   = (Actor405800Work*)task->work;
    TaskFuncTable3   states = D_actor_405800_80131F04;

    func_actor_405800_80137948(task);
    if ((func_actor_405800_80139F3C(task) << 0x10) == 0) {
        states.funcs[(s16)work->field_848](task);
    }
}

void func_actor_405800_80138154(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->work;
    TaskFunc         states[2] = { func_actor_405800_80139E2C, func_actor_405800_80139E48 };

    func_actor_405800_80137948(task);
    states[(s16)work->field_848](task);
}

void func_actor_405800_801381BC(Task* task)
{
    Actor405800Work* work      = (Actor405800Work*)task->work;
    TaskFunc         states[2] = { func_actor_405800_80139EAC, func_actor_405800_801356A8 };

    func_actor_405800_80137948(task);
    states[(s16)work->field_848](task);
}

void func_actor_405800_80138224(Task* task)
{
    Actor405800Work* work;
    s32              i;

    work = (Actor405800Work*)task->work;
    i    = 1;
    do {
        work->slots[i].rate = work->field_850;
        Gp_AnimResetSlot(&work->anim, i, work->field_872);
        i++;
    } while (i < 0x12);
    work->field_870 = work->field_872;
}

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_405800_80138294(Task* arg0)
{
    Actor405800Work* work;
    s32              i;

    work = (Actor405800Work*)arg0->work;
    if (work->field_870 == work->field_872) {
        i = 1;
        do {
            work->slots[i].rate = work->field_850;
            func_800B4114(&work->anim, i, work->field_872, 0, work->field_84A);
            i++;
        } while (i < 0x12);
    } else {
        i = 1;
        do {
            work->slots[i].rate = work->field_850;
            func_800B4114(&work->anim, i, work->field_872, 0, work->field_84A);
            i++;
        } while (i < 0x12);
        work->field_84A = 0;
    }
    work->field_870 = work->field_872;
}

s16 func_actor_405800_8013836C(Task* arg0, s16 arg1)
{
    Actor405800Work* work = (Actor405800Work*)arg0->work;

    if (work->field_850 == 0) {
        return 0;
    }
    return ((arg1 << 8) / work->field_850 << 12) >> 16;
}

/// Turns the actor's yaw (`field_82`) by `step` toward the world point
/// `target`, of which only `vx` and `vz` are read, leaving it alone while the
/// heading error is within 0x100. Clears the model root's `flg` first so the
/// root is recomputed.
void func_actor_405800_801383CC(Task* arg0, SVECTOR* target, s32 step)
{
    Actor405800Work* work = (Actor405800Work*)arg0->work;
    GpCoord*         coords;
    SVECTOR          vec;
    s32              diff;
    s32              yaw;
    u16              angle;

    coords      = arg0->extra.tmd->coords;
    coords->flg = 0;
    vec.vx      = target->vx - coords->coord.t[0];
    vec.vy      = 0;
    vec.vz      = target->vz - coords->coord.t[2];
    VectorNormalSS(&vec, &vec);
    yaw   = ratan2(vec.vx, vec.vz);
    angle = work->field_82;
    diff  = ((angle - yaw) << 20) >> 20;
    if (diff > 0x100) {
        work->field_82 = angle - step;
    } else if (diff < -0x100) {
        work->field_82 = angle + step;
    }
}

/// Refreshes the view coordinate and coordinate `index` of the actor's model,
/// then stores that coordinate's view-space X and Z translation to `out`; `y`
/// is left untouched. Every caller passes the work block's `field_88`.
void func_actor_405800_80138478(Task* task, s16 index, Actor405800ViewPos* out)
{
    MATRIX   local;
    GpCoord* coord;
    GpCoord* coords;

    coords            = task->extra.tmd->coords;
    gGfxViewCoord.flg = 0;
    coord             = &coords[index];
    Gp_UpdateCoord(&gGfxViewCoord);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, &local);
    out->x     = local.t[0];
    out->z     = local.t[2];
    coord->flg = 0;
}

void func_actor_405800_80138514(Task* arg0, s16 arg1, Actor405800ViewPos* arg2)
{
    MATRIX   root;
    MATRIX   local;
    GpCoord* coord;
    GpCoord* coords;

    coords            = arg0->extra.tmd->coords;
    gGfxViewCoord.flg = 0;
    coord             = &coords[arg1];
    Gp_UpdateCoord(&gGfxViewCoord);
    coords[0].flg = 0;
    coord->flg    = 0;
    Gp_UpdateCoord(coord);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coords[0].workm, &root);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, &local);
    coords[0].coord.t[0] = arg2->x - (local.t[0] - root.t[0]);
    coords[0].coord.t[2] = arg2->z - (local.t[2] - root.t[2]);
    coords[0].flg        = 0;
    coord->flg           = 0;
    Gp_UpdateCoord(coord);
}

/// Returns 1 when any of bits 0, 1 or 8 of the second animation slot's
/// `slots[1].flags` is set - the walk took the clip's end, followed a control
/// entry, or the clip has settled on its last pose - and 0 otherwise. Bit 0 is
/// read as a halfword and the other two through the word starting there,
/// which is why the work block is seen through `ActorsShared8013a0b0Work`.
s32 func_actor_405800_801385F4(Task* arg0)
{
    ActorsShared8013a0b0Work* work = (ActorsShared8013a0b0Work*)arg0->work;

    if ((work->flags_FC.half & 1) || (work->flags_FC.word & 0x102)) {
        return 1;
    }
    return 0;
}

/// Per-frame entry point of the actor's task: runs whichever of the four
/// handlers in `D_actor_405800_80131E54` the task's `state` selects. The table
/// is a local, so GCC copies it from `.rodata` onto the stack every frame.
void func_actor_405800_80138634(Task* task)
{
    TaskFuncTable4 states;

    states = D_actor_405800_80131E54;
    states.funcs[task->state](task);
}

void func_actor_405800_80138698(Task* arg0)
{
    TmdObject*       model = arg0->extra.tmd;
    Actor405800Work* work  = (Actor405800Work*)arg0->work;
    TaskFuncTable12  fns   = D_actor_405800_80131E24;

    switch (Gp_StateF0.field_4) {
        case 2:
            model->flags |= 0x80;
            break;
        case 0:
            fns.funcs[(s16)work->field_846](arg0);
        case 1:
            func_actor_405800_801387DC(arg0);
            func_actor_405800_80132E3C(arg0, work->field_86A, work->field_866);
            break;
    }
}

void func_actor_405800_80138788(Task* arg0)
{
    Actor405800Work* work                = (Actor405800Work*)arg0->work;
    void             (*states[2])(Task*) = {
        func_actor_405800_80138EF0,
        func_actor_405800_80138F54,
    };

    states[(s16)work->field_846](arg0);
}

/// Colours the actor from its model's second coordinate: takes a 0x10-byte
/// `VECTOR` off `G_SCRATCH_HEAD`, fills it with that coordinate's world
/// position and hands it to `Gp_UpdateActorColor` for the task's `spawnArg2`,
/// with no blend parameters.
void func_actor_405800_801387DC(Task* task)
{
    GpCoord* coord;
    void**   scratch;
    u8*      head;
    VECTOR*  block;

    coord                          = &task->extra.tmd->coords[1];
    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    block                          = (VECTOR*)(head - 0x10);
    block->vx                      = coord->workm.t[0];
    block->vy                      = coord->workm.t[1];
    block->vz                      = coord->workm.t[2];
    SCRATCH_HEAD_AT(scratch, void) = block;
    Gp_UpdateActorColor(task->spawnArg2, block, 0, 0);
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

void func_actor_405800_80138854(Task* arg0, s32 arg1, u16* arg2)
{
    Actor405800Work* work = (Actor405800Work*)arg0->work;

    switch (arg2[1]) {
        case 0:
            work->field_88A = 1;
            break;
        case 1:
            work->field_88A = 2;
            break;
        case 2:
            work->field_88A = 3;
            break;
        case 3:
            work->field_88A = 4;
            break;
    }
}

void func_actor_405800_801388C4(Task* task)
{
    ((Actor405800Work*)task->work)->field_88B = 1;
}

void func_actor_405800_801388D4(void)
{
}

void func_actor_405800_801388DC(void)
{
}

void func_actor_405800_801388E4(Task* task)
{
    GpEnemy*         enemy;
    Actor405800Work* work;
    TmdObject*       model;

    enemy                = (GpEnemy*)task->spawnArg2;
    work                 = (Actor405800Work*)task->work;
    model                = task->extra.tmd;
    work->obj_6D4.flags &= 0x7FFF;
    work->obj_6B4.flags &= 0x7FFF;
    work->obj_694.flags &= 0x7FFF;
    work->obj_674.flags &= 0x7FFF;
    Gp_UnlinkNode(&enemy->node);
    if (work->field_85A == 4) {
        work->field_842 = 0;
        model->flags   |= 0x80;
        func_actor_405800_80139FB0(task, 7);
    } else if (work->field_890 == 0) {
        work->field_846 = work->field_846 + 1;
    } else {
        func_actor_405800_80139FB0(task, 9);
    }
}

void func_actor_405800_801389AC(Task* task)
{
    Actor405800Work* work = (Actor405800Work*)task->work;

    Gp_ReleaseStateF0Add(task, 0);
    func_actor_405800_8013A1E0(task, D_actor_405800_801514D8[work->field_872], 0x10);
    func_actor_405800_8013A0F4(task);
    work->field_846 = work->field_846 + 1;
}

void func_actor_405800_80138A18(Task* task)
{
    Actor405800Work* work;

    work = (Actor405800Work*)task->work;
    func_actor_405800_8013A0F4(task);
    if ((s16)func_actor_405800_801385F4(task) != 0) {
        work->field_846 = work->field_846 + 1;
    }
}

void func_actor_405800_80138A70(Task* task)
{
    Actor405800Work* work  = (Actor405800Work*)task->work;
    GpCoord*         coord = task->extra.tmd->coords;

    ((GpEnemy*)task->spawnArg2)->recs = 0;
    Gp_UnlinkObj(&work->obj_594);
    Gp_UnlinkObj(&work->obj_4B4);
    Gp_UnlinkObj(&work->obj_6B4);
    Gp_UnlinkObj(&work->obj_6D4);
    Gp_UnlinkObj(&work->obj_674);
    Gp_UnlinkObj(&work->obj_694);
    Gp_UnlinkObj(&work->obj_724);
    work->flags_83C.h.field_83E = 0x1000;
    work->matrix_0              = coord->coord;
    Gp_SetLightMode(task->spawnArg2, 1);
    work->field_842 = 0;
    work->field_846++;
}

void func_actor_405800_80138B50(Task* task)
{
    Actor405800Work* work;
    TmdObject*       ext;
    u16              count;

    work            = (Actor405800Work*)task->work;
    ext             = task->extra.tmd;
    count           = work->field_842 + 1;
    work->field_842 = count;
    if ((s16)count >= 0x18) {
        work->field_832 = 0;
        work->field_834 = 0x1000;
        work->field_866 = 0xFF;
        func_8009EA50(work->field_832);
        ext->lightLevel = work->field_834;
        work->field_842 = 0;
        work->field_846 = work->field_846 + 1;
    }
}

void func_actor_405800_80138BD4(Task* task)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->work;
    task->state     = 3;
    work->field_846 = 0;
    work->field_848 = 0;
}

void func_actor_405800_80138BEC(Task* task)
{
    Actor405800Work* work;
    u16              count;

    work            = (Actor405800Work*)task->work;
    count           = work->field_842 + 1;
    work->field_842 = count;
    if ((s16)count >= 2) {
        work->field_846 = work->field_846 + 1;
    }
}

void func_actor_405800_80138C30(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    TmdObject*       model;
    GpEnemy*         enemy;

    model = task->extra.tmd;
    work  = (Actor405800Work*)task->work;
    enemy = (GpEnemy*)task->spawnArg2;
    Tmd_FreeBuffers(model);
    model->flags |= 4;
    func_actor_405800_80136E14(task);
    work->field_866 = 0;
    Gp_ReleaseStateF0Add(task, 0);
    enemy->recs = 0;
    Gp_UnlinkObj(&work->obj_594);
    Gp_UnlinkObj(&work->obj_4B4);
    Gp_UnlinkObj(&work->obj_6B4);
    Gp_UnlinkObj(&work->obj_6D4);
    Gp_UnlinkObj(&work->obj_674);
    Gp_UnlinkObj(&work->obj_694);
    Gp_UnlinkObj(&work->obj_724);
    work2            = (Actor405800Work*)task->work;
    task->state      = 3;
    work2->field_846 = 0;
    work2->field_848 = 0;
}

void func_actor_405800_80138CF0(Task* task)
{
    Actor405800Work* work;

    work = (Actor405800Work*)task->work;
    func_actor_405800_8013A1F8(task, 9, 0x10, 2);
    work->field_84C = 0;
    work->field_84E = 0;
    work->field_86A = work->field_92;
    func_actor_405800_8013A0F4(task);
    work->field_846 = work->field_846 + 1;
}

void func_actor_405800_80138D54(Task* task)
{
    Actor405800Work* work;
    GpCoord*         coord;

    work               = (Actor405800Work*)task->work;
    coord              = task->extra.tmd->coords;
    work->field_84C   += 2;
    work->field_84E   += work->field_84C;
    coord->coord.t[1] += work->field_84E;
    if ((s16)work->field_92 < coord->coord.t[1]) {
        coord->coord.t[1] = (s16)work->field_92;
        func_actor_405800_8013A1E0(task, 0x13, 0x10);
        work->field_84 += 0x800;
        func_actor_405800_80139FC4(task);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        work->field_842 = 0;
        work->field_890 = 0;
        work->field_891 = 1;
        work->field_846++;
    }
    func_actor_405800_8013A0F4(task);
}

void func_actor_405800_80138E20(Task* task)
{
    Actor405800Work* work;
    u32              sound;
    s32              pan;

    work = (Actor405800Work*)task->work;
    if ((s16)work->field_842 == 0) {
        sound   = ((GpEnemy*)task->spawnArg2)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        sound  |= 0x40050006;
        pan     = Gp_GetObjPan(task->extra.tmd->coords) << 24;
        pan   >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(task->extra.tmd->coords));
        work->field_842++;
    }
    if ((func_actor_405800_801385F4(task) << 0x10) != 0) {
        func_actor_405800_80139FB0(task, 1);
    }
    func_actor_405800_8013A0F4(task);
}

void func_actor_405800_80138EF0(Task* task)
{
    Actor405800Work* work;

    work = (Actor405800Work*)task->work;
    if (work->field_824 != NULL) {
        taskKill(work->field_824);
    }
    if (work->field_828 != NULL) {
        taskKill(work->field_828);
    }
    work->field_842 = 0;
    work->field_846 = work->field_846 + 1;
}

void func_actor_405800_80138F54(Task* task)
{
    Actor405800Work* work;
    u16              count;

    work            = (Actor405800Work*)task->work;
    count           = work->field_842 + 1;
    work->field_842 = count;
    if ((s16)count >= 0x12D) {
        Gp_DestroyEnemy((GpEnemy*)task->spawnArg2, task);
    }
}

void func_actor_405800_80138FA8(Task* task)
{
    Actor405800Work* work;

    work = (Actor405800Work*)task->work;
    if (((s8)work->field_895 >= 0 || (work->field_895 & 0x7F)) && work->field_83A == 0) {
        work->field_895 = 0x80;
        work->field_896 = 0;
    }
    work->field_880 = 0x18;
    work->field_893 = 0;
    work->field_87E = 0x10;
    func_actor_405800_80135A3C(task, 0x10);
    work->field_848 = work->field_848 + 1;
}

void func_actor_405800_8013902C(Task* task)
{
    Actor405800Work* work;
    s16              min;
    s16              step;
    u32              rnd;

    work = (Actor405800Work*)task->work;
    min  = 0x10;
    if (work->field_852 > 0xBB8 && work->field_893 == 0) {
        rnd         = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState = rnd;
        if ((rnd >> 0x10) & 1) {
            min  = 0x18;
            step = 0x24;
        } else {
            min  = 0x14;
            step = 0x1E;
        }
        work->field_880 = step;
        work->field_893 = 1;
    }
    if (work->field_87E < min) {
        work->field_87E = min;
    }
    func_actor_405800_801383CC(task, &work->field_A8, work->field_880);
    func_actor_405800_80135A3C(task, work->field_87E);
}

void func_actor_405800_801390FC(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work = (Actor405800Work*)arg0->work;
    if (work->field_891 == 0) {
        work->field_84A = 2;
        work->field_850 = 0x20;
        work->field_872 = 9;
        work->field_86E = 1;
    } else {
        work->field_84A = 2;
        work->field_850 = 0x20;
        work->field_872 = 0xB;
        work->field_86E = 1;
    }
    work2 = (Actor405800Work*)arg0->work;
    if (((s8)work2->field_895 >= 0) || ((work2->field_895 & 0x7F) != 1)) {
        work2->field_895 = 0x81;
        work2->field_896 = 0;
    }
    work->field_848 = work->field_848 + 1;
}

void func_actor_405800_80139188(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;

    work = (Actor405800Work*)arg0->work;
    if (work->field_858 != 0 && work->field_85A == 1) {
        if (work->field_891 == 0) {
            work->field_850 = 0x20;
            work->field_872 = 9;
            work->field_86E = 2;
        } else {
            work->field_850 = 0x20;
            work->field_872 = 0xB;
            work->field_86E = 2;
        }
        return;
    }
    if ((func_actor_405800_80136A1C(arg0) << 0x10) == 0 && (func_actor_405800_801385F4(arg0) << 0x10) != 0) {
        if (work->field_891 == 0) {
            work2            = (Actor405800Work*)arg0->work;
            work2->field_846 = 2;
            work2->field_848 = 0;
        } else {
            work3            = (Actor405800Work*)arg0->work;
            work3->field_846 = 0xA;
            work3->field_848 = 0;
        }
    }
}

void func_actor_405800_80139260(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work = (Actor405800Work*)arg0->work;
    if (work->field_891 == 0) {
        work->field_84A = 3;
        work->field_850 = 0x10;
        work->field_872 = 0xA;
        work->field_86E = 1;
    } else {
        work->field_84A = 3;
        work->field_850 = 0x10;
        work->field_872 = 0xC;
        work->field_86E = 1;
    }
    work2 = (Actor405800Work*)arg0->work;
    if (((s8)work2->field_895 >= 0) || ((work2->field_895 & 0x7F) != 1)) {
        work2->field_895 = 0x81;
        work2->field_896 = 0;
    }
    work->field_848 = work->field_848 + 1;
}

void func_actor_405800_801392EC(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;

    work = (Actor405800Work*)arg0->work;
    if ((func_actor_405800_801385F4(arg0) << 0x10) != 0) {
        if (work->field_891 == 0) {
            work2            = (Actor405800Work*)arg0->work;
            work2->field_846 = 2;
            work2->field_848 = 0;
        } else {
            work3            = (Actor405800Work*)arg0->work;
            work3->field_846 = 0xA;
            work3->field_848 = 0;
        }
    }
}

void func_actor_405800_80139358(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work = (Actor405800Work*)arg0->work;
    if (work->field_891 == 0) {
        work->field_84A = 8;
        work->field_850 = 0x10;
        work->field_872 = 0xF;
        work->field_86E = 1;
    } else {
        work->field_84A = 3;
        work->field_850 = 0x10;
        work->field_872 = 0x11;
        work->field_86E = 1;
    }
    work2 = (Actor405800Work*)arg0->work;
    if (((s8)work2->field_895 >= 0) || ((work2->field_895 & 0x7F) != 1)) {
        work2->field_895 = 0x81;
        work2->field_896 = 0;
    }
    work->field_842 = 0;
    work->field_848 = work->field_848 + 1;
}

void func_actor_405800_801393E8(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;
    Actor405800Work* work4;
    Actor405800Work* work5;
    u16              count;

    work = (Actor405800Work*)arg0->work;
    if (work->field_898 == 2) {
        if (Gp_TickObjFlag2(arg0->spawnArg2) != 0) {
            if (work->field_891 == 0) {
                work2            = (Actor405800Work*)arg0->work;
                work2->field_84A = 8;
                work2->field_850 = 0x10;
                work2->field_872 = 0x10;
                work2->field_86E = 1;
            } else {
                work3            = (Actor405800Work*)arg0->work;
                work3->field_84A = 8;
                work3->field_850 = 0x10;
                work3->field_872 = 0x12;
                work3->field_86E = 1;
            }
            work->field_848 = work->field_848 + 1;
        }
    } else {
        count           = work->field_842 + 1;
        work->field_842 = count;
        if ((s16)count >= 0x15) {
            if (work->field_891 == 0) {
                work4            = (Actor405800Work*)arg0->work;
                work4->field_84A = 8;
                work4->field_850 = 0x10;
                work4->field_872 = 0x10;
                work4->field_86E = 1;
            } else {
                work5            = (Actor405800Work*)arg0->work;
                work5->field_84A = 8;
                work5->field_850 = 0x10;
                work5->field_872 = 0x12;
                work5->field_86E = 1;
            }
            work->field_848 = work->field_848 + 1;
        }
    }
}

void func_actor_405800_801394E4(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;

    work = (Actor405800Work*)arg0->work;
    if ((func_actor_405800_801385F4(arg0) << 0x10) != 0) {
        work->field_898 = 0;
        if (work->field_891 == 0) {
            work2            = (Actor405800Work*)arg0->work;
            work2->field_846 = 2;
            work2->field_848 = 0;
        } else {
            work3            = (Actor405800Work*)arg0->work;
            work3->field_846 = 0xA;
            work3->field_848 = 0;
        }
    }
}

void func_actor_405800_80139550(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work            = (Actor405800Work*)task->work;
    work->field_84A = 8;
    work->field_850 = 0x10;
    work->field_872 = 7;
    work->field_86E = 1;
    work->field_842 = 0;
    func_actor_405800_80139F0C(task, 0);
    work2 = (Actor405800Work*)task->work;
    if (((s8)work2->field_895 >= 0) || ((work2->field_895 & 0x7F) != 1)) {
        work2->field_895 = 0x81;
        work2->field_896 = 0;
    }
    work->field_848 = work->field_848 + 1;
}

void func_actor_405800_801395E8(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work            = (Actor405800Work*)task->work;
    work->field_84A = 8;
    work->field_850 = 0x10;
    work->field_872 = 8;
    work->field_86E = 1;
    work->field_842 = 0;
    func_actor_405800_80139F0C(task, 1);
    work2 = (Actor405800Work*)task->work;
    if (((s8)work2->field_895 >= 0) || ((work2->field_895 & 0x7F) != 1)) {
        work2->field_895 = 0x81;
        work2->field_896 = 0;
    }
    work->field_848 = work->field_848 + 1;
}

void func_actor_405800_8013967C(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work = (Actor405800Work*)task->work;
    func_actor_405800_8013706C(task, 0);
    work->field_88B = 0;
    work2           = (Actor405800Work*)task->work;
    if (((s8)work2->field_895 >= 0) || ((work2->field_895 & 0x7F) != 1)) {
        work2->field_895 = 0x81;
        work2->field_896 = 0;
    }
    func_actor_405800_80135A3C(task, work->field_87E);
    work->field_848 = work->field_848 + 1;
}

void func_actor_405800_80139700(Task* task)
{
    Actor405800Work* work = (Actor405800Work*)task->work;
    Actor405800Work* work2;

    work->field_84 += -(s16)work->field_84 >> 2;
    if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
        if (work->field_88C == 0) {
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
        }
        func_actor_405800_80137994(task, 0x3C);
        work->field_84   = 0;
        work2            = (Actor405800Work*)task->work;
        work2->field_846 = 2;
        work2->field_848 = 0;
        work->field_88F  = 0;
    }
}

void func_actor_405800_801397B8(Task* task)
{
    Actor405800Work* work;

    work = (Actor405800Work*)task->work;
    func_actor_405800_8013706C(task, 1);
    work->field_848 = work->field_848 + 1;
}

void func_actor_405800_801397F0(Task* task)
{
    Actor405800Work* work;

    if (((s16)func_actor_405800_80136B94(task) == 0) && ((s16)func_actor_405800_801385F4(task) != 0)) {
        work            = (Actor405800Work*)task->work;
        work->field_846 = 2;
        work->field_848 = 0;
    }
}

void func_actor_405800_80139844(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work             = (Actor405800Work*)task->work;
    work->field_842  = 0;
    work2            = (Actor405800Work*)task->work;
    work2->field_850 = 0x10;
    work2->field_872 = 0x16;
    work2->field_86E = 2;
    work->field_848  = work->field_848 + 1;
}

void func_actor_405800_80139880(Task* task)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->work;
    work->field_84A = 4;
    work->field_850 = 0x10;
    work->field_872 = 0x15;
    work->field_86E = 1;
    work->field_842 = 0;
    work->field_9A  = -0x9C4;
    work->field_848 = work->field_848 + 1;
}

void func_actor_405800_801398C0(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work = (Actor405800Work*)task->work;
    if (((s16)func_actor_405800_80136B94(task) == 0) && ((s16)func_actor_405800_801385F4(task) != 0)) {
        work->field_85E  = 0x12C;
        work2            = (Actor405800Work*)task->work;
        work2->field_846 = 2;
        work2->field_848 = 0;
    }
}

void func_actor_405800_80139928(Task* task)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    GpCoord*         coord;
    u16              next;

    work  = (Actor405800Work*)task->work;
    coord = task->extra.tmd->coords;
    func_actor_405800_801379F8(task);
    work2            = (Actor405800Work*)task->work;
    work2->field_84A = 4;
    work2->field_850 = 0x10;
    work2->field_872 = 0x20;
    work2->field_86E = 1;
    work->field_84C  = 0x40;
    work->field_84E  = 0;
    work->field_842  = 0;
    work->field_98   = coord->coord.t[0];
    next             = work->field_848;
    work->field_9C   = coord->coord.t[2];
    work->field_88F  = 1;
    work->field_848  = next + 1;
}

void func_actor_405800_801399C4(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    u32              sound;
    s32              id;
    s32              pan;

    work = (Actor405800Work*)arg0->work;
    if ((s16)work->field_842 == 0) {
        id = 0x40050003;
        if ((arg0->spawnArg1 & 0xF0) == 0x10) {
            id = 0x404A0003;
        }
        sound = id | ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8);
        pan   = Gp_GetObjPan(arg0->extra.tmd->coords) << 24;
        pan >>= 24;
        SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
        work->field_88F = 0;
        work->field_842++;
    }
    if ((func_actor_405800_80136B94(arg0) << 0x10) == 0 && (func_actor_405800_801385F4(arg0) << 0x10) != 0) {
        work->field_85E  = 0x12C;
        work2            = (Actor405800Work*)arg0->work;
        work2->field_846 = 2;
        work2->field_848 = 0;
    }
}

void func_actor_405800_80139AC4(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work = (Actor405800Work*)arg0->work;
    if (((s8)work->field_895 >= 0) || ((work->field_895 & 0x7F) != 1)) {
        work->field_895 = 0x81;
        work->field_896 = 0;
    }
    work2            = (Actor405800Work*)arg0->work;
    work2->field_84A = 2;
    work2->field_850 = 0x10;
    work2->field_872 = 9;
    work2->field_86E = 1;
    work->field_88F  = 1;
    work->field_84C  = 0;
    work->field_84E  = 0;
    work->field_848  = work->field_848 + 1;
    work->field_86A  = work->field_92;
}

void func_actor_405800_80139B3C(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    GpCoord*         coord;

    work               = (Actor405800Work*)arg0->work;
    coord              = arg0->extra.tmd->coords;
    work->field_84C    = work->field_84C + 2;
    work->field_84E    = work->field_84E + work->field_84C;
    coord->coord.t[1] += work->field_84E;
    if ((s16)work->field_92 < coord->coord.t[1]) {
        coord->coord.t[1] = (s16)work->field_92;
        work2             = (Actor405800Work*)arg0->work;
        work2->field_850  = 0x10;
        work2->field_872  = 0x13;
        work2->field_86E  = 2;
        work->field_891   = 1;
        work->field_842   = 0;
        work->field_890   = 0;
        work->field_84    = work->field_84 + 0x800;
        work->field_848   = work->field_848 + 1;
    }
}

void func_actor_405800_80139BD8(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    u32              rnd;

    work = (Actor405800Work*)arg0->work;
    if ((func_actor_405800_80136B94(arg0) << 0x10) != 0) {
        rnd             = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState     = rnd;
        work->field_87C = ((rnd >> 0x10) & 0x7F) + 0x1E;
    } else if ((func_actor_405800_801385F4(arg0) << 0x10) != 0) {
        rnd              = ((u32)Gp_LcgState * 5) + 0x71357911;
        Gp_LcgState      = rnd;
        work->field_87C  = ((rnd >> 0x10) & 0x7F) + 0x1E;
        work2            = (Actor405800Work*)arg0->work;
        work2->field_846 = 0xA;
        work2->field_848 = 0;
    }
}

void func_actor_405800_80139C98(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;

    work = (Actor405800Work*)arg0->work;
    if (work->field_891 == 0) {
        work->field_84A = 3;
        work->field_850 = 0x10;
        work->field_872 = 0x1A;
        work->field_86E = 1;
    } else {
        work->field_84A = 3;
        work->field_850 = 0x10;
        work->field_872 = 0x1B;
        work->field_86E = 1;
    }
    work2 = (Actor405800Work*)arg0->work;
    if (((s8)work2->field_895 >= 0) || ((work2->field_895 & 0x7F) != 1)) {
        work2->field_895 = 0x81;
        work2->field_896 = 0;
    }
    work->field_848 = work->field_848 + 1;
}

void func_actor_405800_80139D24(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;

    work = (Actor405800Work*)arg0->work;
    if ((func_actor_405800_801385F4(arg0) << 0x10) != 0) {
        if (work->field_891 == 0) {
            work2            = (Actor405800Work*)arg0->work;
            work2->field_84A = 0x1E;
            work2->field_850 = 0x10;
            work2->field_872 = 0x10;
            work2->field_86E = 1;
        } else {
            work3            = (Actor405800Work*)arg0->work;
            work3->field_84A = 0x1E;
            work3->field_850 = 8;
            work3->field_872 = 0x14;
            work3->field_86E = 1;
        }
        work->field_848 = work->field_848 + 1;
    }
}

void func_actor_405800_80139DC0(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;

    work = (Actor405800Work*)arg0->work;
    if ((func_actor_405800_801385F4(arg0) << 0x10) != 0) {
        if (work->field_891 == 0) {
            work2            = (Actor405800Work*)arg0->work;
            work2->field_846 = 2;
            work2->field_848 = 0;
        } else {
            work3            = (Actor405800Work*)arg0->work;
            work3->field_846 = 0xA;
            work3->field_848 = 0;
        }
    }
}

void func_actor_405800_80139E2C(Task* task)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->work;
    work->field_848 = work->field_848 + 1;
}

void func_actor_405800_80139E48(Task* arg0)
{
    Actor405800Work* work;
    Actor405800Work* work2;
    Actor405800Work* work3;

    work = (Actor405800Work*)arg0->work;
    func_actor_405800_801379F8(arg0);
    if (work->field_852 > 2000) {
        work2            = (Actor405800Work*)arg0->work;
        work2->field_846 = 0xD;
        work2->field_848 = 0;
    } else {
        work3            = (Actor405800Work*)arg0->work;
        work3->field_846 = 8;
        work3->field_848 = 0;
    }
}

void func_actor_405800_80139EAC(Task* arg0)
{
    Actor405800Work* work = (Actor405800Work*)arg0->work;
    u32              rnd;

    work->field_84A = 4;
    work->field_850 = 0x10;
    work->field_872 = 1;
    work->field_86E = 1;
    rnd             = ((u32)Gp_LcgState * 5) + 0x71357911;
    Gp_LcgState     = rnd;
    work->field_882 = ((rnd >> 0x10) & 0x3F) + 0x5A;
    work->field_848 = work->field_848 + 1;
}

void func_actor_405800_80139F0C(Task* task, u8 arg1)
{
    Actor405800Work* work = (Actor405800Work*)task->work;
    s32              mode = arg1;

    if (mode == 0) {
        work->field_88E = 1;
    } else if (mode == 1) {
        work->field_88D = mode;
    }
}

s32 func_actor_405800_80139F3C(Task* arg0)
{
    Actor405800Work* work = (Actor405800Work*)arg0->work;

    if (work->field_858 == 1 && work->field_890 == 0) {
        switch (work->field_85A) {
            case 3:
                work->field_846 = 5;
                work->field_848 = 0;
                work->field_85A = 0;
                return 1;
            case 5:
                work->field_846 = 0xF;
                work->field_848 = 0;
                work->field_85A = 0;
                return 1;
        }
    }
    work->field_85A = 0;
    return 0;
}

void func_actor_405800_80139FB0(Task* task, s16 arg1)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->work;
    work->field_846 = arg1;
    work->field_848 = 0;
}

/// Wraps the actor's pitch, yaw and roll (`field_80`, `field_82`, `field_84`)
/// to 12 bits and rebuilds the model root's rotation from them: an identity
/// matrix taken off `G_SCRATCH_HEAD` is turned by roll, pitch and then yaw,
/// and its 3x3 copied into the root coordinate, whose translation is left
/// alone.
void func_actor_405800_80139FC4(Task* arg0)
{
    Actor405800Work* work  = (Actor405800Work*)arg0->work;
    GpCoord*         coord = arg0->extra.tmd->coords;
    MATRIX*          m;
    MATRIX*          dst;

    work->field_80      &= 0xFFF;
    work->field_82      &= 0xFFF;
    work->field_84      &= 0xFFF;
    m                    = (MATRIX*)(SCRATCH_HEAD(u8) - 0x20);
    MATRIX_PAIR(m, 0, 0) = 0x1000;
    MATRIX_PAIR(m, 0, 2) = 0;
    MATRIX_PAIR(m, 1, 1) = 0x1000;
    MATRIX_PAIR(m, 2, 0) = 0;
    m->m[2][2]           = 0x1000;
    SCRATCH_HEAD(MATRIX) = m;
    RotMatrixZ((s16)work->field_84, m);
    RotMatrixX((s16)work->field_80, m);
    func_8004BFF8((s16)work->field_82, m);
    dst          = &coord->coord;
    dst->m[0][0] = m->m[0][0];
    dst->m[0][1] = m->m[0][1];
    dst->m[0][2] = m->m[0][2];
    dst->m[1][0] = m->m[1][0];
    dst->m[1][1] = m->m[1][1];
    dst->m[1][2] = m->m[1][2];
    dst->m[2][0] = m->m[2][0];
    dst->m[2][1] = m->m[2][1];
    SCRATCH_POP_BYTES(0x20);
    dst->m[2][2] = m->m[2][2];
}

void func_actor_405800_8013A0F4(Task* arg0)
{
    Actor405800Work* work = (Actor405800Work*)arg0->work;
    s32              i;

    if (work->field_86E == 1) {
        if (work->field_870 != work->field_872) {
            work->field_874 = 0;
        } else {
            work->field_874 = func_actor_405800_8013836C(arg0, work->field_874);
        }
        func_actor_405800_80138294(arg0);
        work->field_86E = 3;
    } else if (work->field_86E == 2) {
        func_actor_405800_80138224(arg0);
        work->field_86E = 3;
        work->field_874 = 0;
    } else if (work->field_86E == 3) {
        work->field_874++;
    }
    i = 1;
    do {
        work->slots[i].rate = work->field_850;
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0x12);
}

void func_actor_405800_8013A1E0(Task* task, s16 arg1, s16 arg2)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->work;
    work->field_850 = arg2;
    work->field_872 = arg1;
    work->field_86E = 2;
}

void func_actor_405800_8013A1F8(Task* task, s16 arg1, s16 arg2, s16 arg3)
{
    Actor405800Work* work;

    work            = (Actor405800Work*)task->work;
    work->field_84A = arg3;
    work->field_850 = arg2;
    work->field_872 = arg1;
    work->field_86E = 1;
}
