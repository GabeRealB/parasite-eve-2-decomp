#include "common.h"

#include "actors/actor_120300.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern s8             D_8007218A;
extern u8             D_80073BA9;
extern u8             D_80073BAC;
extern u8             D_80071075;
extern s8             D_8007272D;
extern s8             D_80114C12;
extern s32            D_actor_120300_801409A8;
extern s32            D_actor_120300_801409C0;
extern s32            D_actor_120300_80140A20;
extern s32            D_actor_120300_80140B5C;
extern s32            D_actor_120300_80140B94;
extern s32            D_actor_120300_80141524;
extern s32            D_8017DD6C;
extern s32            D_8017DEA4;
extern s32            D_8017E1F4;
extern GpAreaApplyRec D_80180204;

void func_actor_120300_80132C60(Task* arg0);
void func_actor_120300_80133330(s32 arg0);
s32  func_actor_120300_801334A4(Actor120300* arg0);
void func_actor_120300_801335D8(Task* arg0);

/// Fill part-1 translation and hand it to `func_800D7A9C`. `vec` is a
/// parameter rather than a local so its address stays out of the CSE class of
/// the `ScaleMatrix` argument that follows.
static inline void func_actor_120300_FillLight(Task* arg0, TmdObject* tmd, VECTOR* vec)
{
    vec->vx = tmd->coords[1].workm.t[0];
    vec->vy = ((TmdObject*)arg0->extra)->coords[1].workm.t[1];
    vec->vz = ((TmdObject*)arg0->extra)->coords[1].workm.t[2];
    func_800D7A9C(tmd, vec, 0, 3);
}

/// Main tick of the cutscene actor. State 0 waits until no other cutscene is
/// up (`D_80114C12` / `D_80071075`), builds the work block, then either arms
/// play (`func_actor_120300_80133330`) once flag nibble 0x2D is set or sends
/// the slot-3 weapon record and starts the script. States 1-4 step the area
/// records, the pending `Gp_TakePendingObj4C` cue, and the overlay-load
/// phases. Every path but the cutscene-busy early-out then ticks the two
/// animation helpers, draws the floor quad, and scales the model.
void func_actor_120300_801337C4(Task* arg0)
{
    union {
        struct {
            SVECTOR rot;
            VECTOR  vec;
        } draw;
        GpRec14 rec;
    } scratch;
    Actor120300Work* work;
    Actor120300Work* temp;
    TmdObject*       tmd;
    s32              state;
    s32              weaponId;
    s32              scale;
    s16              ready;
    s32              take;
    u16              scaleRaw;
    u16              evtId;
    u8               evtKind;
    u8               evtSub;

    state = arg0->state;
    work  = (Actor120300Work*)arg0->work;
    switch (state) {
        case 0:
            if ((D_80114C12 != 1) && (D_80071075 == 0)) {
                func_actor_120300_801335D8(arg0);
                work = (Actor120300Work*)arg0->work;
                if (GameFlag_GetNibble(0x2D) != 0) {
                    func_actor_120300_80133330(1);
                    if (GameFlag_GetNibble(0x2E) != 0) {
                        work->field_4D6 = 1;
                    }
                    arg0->state = 4;
                } else {
                    weaponId = D_80073BA9;
                    if (D_8007218A == 1) {
                        weaponId = weaponId + 1;
                    } else {
                        weaponId = weaponId + 0x22;
                    }
                    scratch.rec.field_0  = weaponId;
                    scratch.rec.field_4  = 1;
                    scratch.rec.field_8  = 0;
                    scratch.rec.field_C  = 0;
                    scratch.rec.field_10 = 0;
                    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&scratch.rec, 0);
                    GameFlag_SetNibble(0x2C, 1);
                    GameFlag_SetNibble(0x2D, 1);
                    func_800E3FAC(0xA2, 0xB);
                    func_800E8634((s32)&D_actor_120300_80140B94, 0, (s32)&D_actor_120300_80141524);
                    D_8007272D = 2;
                    SCHED_BARRIER();
                    arg0->state += 1;
                }
                Mem_CopyUnaligned(&D_actor_120300_801409A8, &D_8017DD6C, 0x18);
                Mem_CopyUnaligned(&D_actor_120300_80140A20, &D_8017E1F4, 0x24);
                Mem_CopyUnaligned(&D_actor_120300_801409C0, &D_8017DEA4, 0x60);
                break;
            }
            return;
        case 1:
            if (gGameSession->eventState == 0) {
                Gp_ApplyAreaRecs(&D_80180204);
                arg0->state += 1;
            }
            break;
        case 2:
            ready = 0;
            temp  = (Actor120300Work*)arg0->work;
            if ((s16)Gp_TakePendingObj4C(&evtId, &evtKind, &evtSub) != 0) {
                if (!((s16)evtId & 0x8000)) {
                    if ((evtId & 0x7FFF) == 5) {
                        ready = D_80073BAC != 0;
                    }
                }
            }
            if (ready != 0) {
                if ((s8)evtKind == 1) {
                    temp->field_4DA = 0;
                }
                if ((s8)evtKind == 2) {
                    temp->field_4DA = 1;
                }
                temp->field_4D8 = 0;
                take            = 1;
            } else {
                take = 0;
            }
            if (take != 0) {
                arg0->state += 1;
            }
            break;
        case 3:
            if ((s16)func_actor_120300_801334A4((Actor120300*)arg0) != 0) {
                arg0->state -= 1;
            }
            break;
        case 4:
            Gp_DispatchMsg(work->field_4BC, 0x7D4, (s32)&D_actor_120300_80140B5C, 0);
            arg0->state = 2;
            break;
    }

    func_actor_120300_80132338(arg0);
    func_actor_120300_80132C60(arg0);
    scratch.draw.rot.vx = 0;
    scratch.draw.rot.vy = 0x380;
    scratch.draw.rot.vz = 0;
    Gp_DrawFloorQuad(&((TmdObject*)arg0->extra)->coords[1], 0x300, &scratch.draw.rot);
    tmd      = arg0->extra;
    scaleRaw = work->field_4E0;
    func_actor_120300_FillLight(arg0, tmd, &scratch.draw.vec);
    scale               = scaleRaw & 0xFFFF;
    scratch.draw.vec.vz = scale;
    scratch.draw.vec.vy = scale;
    scratch.draw.vec.vx = scale;
    ScaleMatrix(tmd->colorMtx, &scratch.draw.vec);
}
