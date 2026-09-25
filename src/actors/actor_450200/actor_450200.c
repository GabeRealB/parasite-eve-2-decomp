#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/rand.h>

extern u8 D_8007216D;

extern void func_8017FA98(s32);
extern void func_80180DAC(s32);

extern u8       D_actor_450200_8013885C[];
extern SVECTOR  D_actor_450200_80138868;
extern s32      D_actor_450200_80138870;
extern s32      D_actor_450200_80138A68;
extern s32      D_actor_450200_80138C60;
extern s32      D_actor_450200_80138E88;
extern s32      D_actor_450200_80139098;
extern TaskDesc D_actor_450200_80137A60;
extern s16      D_actor_450200_80137DD6;
extern TaskDesc D_actor_450200_8013FB40;
extern Task*    D_actor_450200_801401E0;
extern Task*    D_actor_450200_801401E4;
extern u16      D_actor_450200_801401E8[256];
extern u16      D_actor_450200_801403E8[256];
extern u16      D_actor_450200_801405E8[256];
extern u16      D_actor_450200_801407E8[256];

/// Head-aim record `func_actor_450200_80131FA8` allocates and parks in
/// `Task::work`, then hands to `func_800B17D4` each frame: the yaw and pitch
/// clamps that function widens against the head's current pose, the `rate`
/// fraction of the remaining angle this actor ramps up and down, and the
/// previous pitch with its valid flag, which `func_800B17D4` keeps itself.
/// The allocation is 12 bytes, two more than gameplay's `GpHeadAim` spans.
/// `rate` is read unsigned and reinterpreted as signed for the clamp.
typedef struct Actor450200HeadAim {
    s16  yawLimit;
    s16  pitchLimit;
    u16  rate;
    s16  lastPitch;
    s8   inited;
    byte pad_9[0x3];
} Actor450200HeadAim;
STATIC_ASSERT_SIZEOF(Actor450200HeadAim, 0xC);

/// Effect state machine of this actor's first sub-task: state 0 arms the
/// self-destruct countdown at 0x64 and state 2 re-arms it at 0x80, both then
/// stepping the state on; state 1 throws effect 0x60080 on every other frame,
/// state 3 splits into an odd branch that bursts 0x60080 with the countdown
/// scaled into the spawn argument while it is still positive and an even
/// branch that spawns a 0x60070 only every eighth frame -- the other two bits
/// of the odd/even split the two effects see. The part the effects hang off is
/// picked at random from the model's coordinate array: the 11-entry byte table
/// holds indices into it, which is why the load is unsigned and the stride is
/// `GsCOORDINATE2`.
void func_actor_450200_80131E24(Task* task)
{
    Task*          slot;
    GsCOORDINATE2* coord;
    s16            countdown;

    slot  = gameGetPtrSlot(0xA);
    coord = &((TmdObject*)slot->extra)->coords[D_actor_450200_8013885C[(rand() * 11) >> 15]];
    switch (task->state) {
        case 0:
            task->killCountdown = 0x64;
            task->state++;
            return;
        case 1:
            countdown           = (u16)task->killCountdown - 1;
            task->killCountdown = countdown;
            if ((countdown & 1) == 0) {
                Gp_SpawnEff(0x60080, coord, 0x80000300, NULL);
            }
            return;
        case 2:
            task->killCountdown = 0x80;
            task->state++;
            return;
        case 3:
            countdown           = (u16)task->killCountdown - 1;
            task->killCountdown = countdown;
            if (countdown & 1) {
                if (countdown > 0) {
                    Gp_SpawnEff(0x60080, coord, countdown * 2 + 0x80000080,
                                &D_actor_450200_80138868);
                }
            } else if (countdown >= -0x1F && (countdown & 7) == 0) {
                Gp_SpawnEff(0x60070, coord, 0xF0010100, &D_actor_450200_80138868);
            }
            return;
    }
}

/// Head-aim state of this actor's second sub-task: state 0 allocates the
/// `Actor450200HeadAim` record into `Task::work` and seeds both clamps to
/// 0x100, state 1 ramps its `rate` up toward 0x1000 while `Task::spawnArg1` is
/// set and back down toward 0 while it is not, then hands the record to
/// `func_800B17D4` between the slot-3 task whose head turns and the
/// `gameGetPtrSlot(0xA)` task it turns toward. A failed allocation, and every
/// state past 1, kill the task; only the latter clears
/// `D_actor_450200_801401E0`, which is why the two `taskKill` calls are
/// distinct.
void func_actor_450200_80131FA8(Task* arg0)
{
    Task*               looker;
    Actor450200HeadAim* aim;
    u16                 rate;

    looker = gameGetPtrSlot(3);
    switch (arg0->state) {
        case 0:
            aim = memCalloc(sizeof(Actor450200HeadAim), false);
            if (aim == NULL) {
                taskKill(arg0);
                return;
            }
            arg0->work      = (TaskIdMap*)aim;
            aim->yawLimit   = 0x100;
            aim->pitchLimit = 0x100;
            arg0->state++;
            /* fallthrough */
        case 1:
            aim = (Actor450200HeadAim*)arg0->work;
            if (arg0->spawnArg1 != 0) {
                rate      = aim->rate + 0x200;
                aim->rate = rate;
                if ((s16)rate >= 0x1001) {
                    aim->rate = 0x1000;
                }
            } else {
                rate      = aim->rate - 0x100;
                aim->rate = rate;
                if ((s16)rate < 0) {
                    aim->rate = 0;
                }
            }
            func_800B17D4(looker, gameGetPtrSlot(0xA), (GpHeadAim*)aim);
            return;
        default:
            taskKill(arg0);
            D_actor_450200_801401E0 = NULL;
            return;
    }
}

/// Three-way control for the second spawned sub-task: 0 tears the live one
/// down, 1 spawns it fresh, anything else is a state write the sub-task sees.
/// Spawning is skipped when the sub-task is already running.
void func_actor_450200_801320D4(s32 arg0)
{
    if (arg0 == 0) {
        if (D_actor_450200_801401E4 != NULL) {
            taskKill(D_actor_450200_801401E4);
            D_actor_450200_801401E4 = NULL;
        }
    } else if (arg0 == 1) {
        D_actor_450200_801401E4 = Task_SpawnFromTable(&D_actor_450200_80137A60, 1, 0, 0);
    } else if (D_actor_450200_801401E4 != NULL) {
        D_actor_450200_801401E4->state = arg0;
    }
}

void func_actor_450200_8013215C(void)
{
    Gp_PulseState1C();
}

void func_actor_450200_8013217C(s32 arg0)
{
    if (D_actor_450200_801401E0 != NULL) {
        D_actor_450200_801401E0->spawnArg1 = arg0;
    }
}

/// Stores in `D_actor_450200_80137DD6` the heading, as a 12-bit angle, from
/// the slot-3 task's root coordinate to the `gameGetPtrSlot(0xA)` task's,
/// refreshing both coordinates first so the X/Z offset is current.
void func_actor_450200_8013219C(void)
{
    GsCOORDINATE2* target;
    GsCOORDINATE2* looker;

    target = ((TmdObject*)(gameGetPtrSlot(0xA))->extra)->coords;
    looker = ((TmdObject*)(gameGetPtrSlot(3))->extra)->coords;
    Gp_UpdateCoord(target);
    Gp_UpdateCoord(looker);
    D_actor_450200_80137DD6 =
        ratan2(target->coord.t[0] - looker->coord.t[0], target->coord.t[2] - looker->coord.t[2]) & 0xFFF;
}

void func_actor_450200_80132220(void)
{
    switch (GameFlag_GetNibble(0x101)) {
        case 0:
            func_800E8614((s32)&D_actor_450200_80138870, 0);
            GameFlag_SetNibble(0x101, 1);
            break;
        case 1:
            func_800E8614((s32)&D_actor_450200_80138A68, 0);
            GameFlag_SetNibble(0x101, 2);
            break;
        case 2:
            func_800E8614((s32)&D_actor_450200_80138C60, 0);
            GameFlag_SetNibble(0x101, 3);
            break;
        case 3:
            func_800E8614((s32)&D_actor_450200_80138E88, 0);
            break;
    }
}

void func_actor_450200_801322F8(void)
{
    if (GameFlag_GetNibble(0xD7) != 0) {
        func_800E8614((s32)&D_actor_450200_80139098, 1);
    } else {
        func_8017FA98(0);
    }
    if (gameGetPtrSlot(0xA) != NULL) {
        D_actor_450200_801401E0 = Task_SpawnFromTable(&D_actor_450200_80137A60, 2, 0, 0);
    }
}

void func_actor_450200_80132368(s32 x, s32 tpageX, s32 clutY, s32 semiTrans, s32 rgb, s32 shadeTex)
{
    SPRT*    p;
    DR_MODE* dr;
    s32      i;

    for (i = 0; i < 2; i++) {
        p              = (SPRT*)gGpuPrimCursor;
        gGpuPrimCursor = p + 1;
        setSprt(p);
        setShadeTex(p, shadeTex);
        setSemiTrans(p, semiTrans);
        p->x0   = x - 0xA0;
        p->y0   = -0x78;
        p->w    = 0x100;
        p->u0   = 0;
        p->v0   = 0;
        p->h    = 0xF0;
        p->r0   = rgb;
        p->g0   = rgb;
        p->b0   = rgb;
        p->clut = GetClut(0, clutY);
        addPrim(&gGpuCurrentOt[0x3FE], p);

        dr             = (DR_MODE*)gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setDrawTPage(dr, 0, 1, getTPage(1, 1, tpageX, 0x100));
        addPrim(&gGpuCurrentOt[0x3FE], dr);

        tpageX += 0x80;
        x      += 0x100;
    }
}

void func_actor_450200_80132538(Task* task)
{
    RECT    rect0;
    RECT    rect1;
    RECT*   rp;
    RECT*   ap;
    s32     i;
    s32     j;
    u16*    src;
    u16*    dst;
    u_long* buf;
    s32     scale;
    s32     state;
    s32     level;
    u32     r;
    u32     g;
    u32     b;
    u32     col;

    if (gGameSession->at4.loc.view == 8) {
        taskKill(task);
        return;
    }

    state = task->state;
    switch (state) {
        case 0:
            task->killCountdown = 0x80;
            task->state        += 1;
            setRECT(&rect0, 0, 0xF7, 0x100, 1);
            StoreImage(&rect0, (u_long*)D_actor_450200_801401E8);
            rect0.y = 0xF8;
            StoreImage(&rect0, (u_long*)D_actor_450200_801403E8);
            break;

        case 1:
            if (task->killCountdown >= 0) {
                func_actor_450200_80132368(-0x40, 0x1C0, 0xFA, 1, 0x80, state);
                func_actor_450200_80132368(0, 0x140, 0xF9, 0, 0x80, state);

                rp    = &rect1;
                src   = D_actor_450200_801401E8;
                dst   = D_actor_450200_801405E8;
                scale = task->killCountdown;
                for (i = 0; i < 0x100; i++) {
                    r      = ((src[i] >> 10) & 0x1F) * scale;
                    g      = ((src[i] >> 5) & 0x1F) * scale;
                    b      = (*(u8*)&src[i] & 0x1F) * scale;
                    col    = r >> 7;
                    g    >>= 7;
                    col   &= 0xFF;
                    col  <<= 10;
                    col   |= ~0x7FFF;
                    g     &= 0xFF;
                    g    <<= 5;
                    col   |= g;
                    r      = b >> 7;
                    r     &= 0xFF;
                    r     |= col;
                    dst[i] = r;
                }
                rect1.x = 0;
                rect1.y = 0xF9;
                rp->w   = 0x100;
                rp->h   = 1;
                ap      = &rect1;
                LoadImage(ap, (u_long*)dst);
                SOFT_DEF_REG(ap);
                rp = &rect1;

                src   = D_actor_450200_801403E8;
                dst   = D_actor_450200_801407E8;
                scale = 0x80 - task->killCountdown;
                buf   = (u_long*)dst;
                for (j = 0; j < 0x100; j++) {
                    r      = ((src[j] >> 10) & 0x1F) * scale;
                    g      = ((src[j] >> 5) & 0x1F) * scale;
                    b      = (*(u8*)&src[j] & 0x1F) * scale;
                    col    = r >> 7;
                    g    >>= 7;
                    col   &= 0xFF;
                    col  <<= 10;
                    col   |= ~0x7FFF;
                    g     &= 0xFF;
                    g    <<= 5;
                    col   |= g;
                    r      = b >> 7;
                    r     &= 0xFF;
                    r     |= col;
                    dst[j] = r;
                }
                rect1.x = 0;
                rect1.y = 0xFA;
                rp->w   = 0x100;
                rp->h   = 1;
                LoadImage(&rect1, buf);
            } else {
                func_actor_450200_80132368(-0x40, 0x1C0, 0xFA, 0, 0x80, state);
            }

            level = 0xA0 - (u16)task->killCountdown;
            if ((u32)(level & 0xFFFF) >= 0xA0U) {
                level = 0xA0;
            }
            func_80180DAC(level & 0xFFFF);
            task->killCountdown = (u16)task->killCountdown - 4;
            break;
    }
}

void func_actor_450200_80132848(s32 arg0)
{
    if (arg0 == 1) {
        Task_SpawnFromTable(&D_actor_450200_8013FB40, 0, 0, 0);
    }
}

void func_actor_450200_80132880(s32 arg0)
{
    func_80180DAC(arg0 & 0xFFFF);
}

void func_actor_450200_801328A0(u8 arg0)
{
    gGameSession->at4.loc.room = arg0;
    D_8007216D                 = arg0;
}
