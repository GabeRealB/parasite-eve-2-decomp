#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"

#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/acropolis_sanctuary.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// `rtps` / `rtpt` / `mvmva` / `gpf`. The `inline_c.h` macros of those names
/// assemble to different words, so spell the instructions out.
#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real()  __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real()  __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")
/// Main-executable globals with no module header yet: `D_80073BA9` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on,
/// `D_80071075` and `D_80114C12` gate the cutscene task's setup (the latter is
/// the cutscene/among-us mode flag) and `D_8007218A` picks which of the two
/// weapon-id bases that record uses. `D_80071076` is set to 1 alongside the
/// save writes when the task hands off to task 0x11, the same way the fountain
/// and helicopter-pad rooms set it.
extern u8              D_80073BA9;
extern u8              D_80071075;
extern s16             D_80071076;
extern s8              D_8007218A;
extern s8              D_80114C12;
extern SVECTOR         D_acropolis_sanctuary_8017D5D0;
extern AcsSpriteLevels D_acropolis_sanctuary_8017D5D8;
extern AcsSpriteLevels D_acropolis_sanctuary_8017D5DC;
extern GpMsgEntry      D_acropolis_sanctuary_8018081C[];
extern RoomPlacement   D_acropolis_sanctuary_801808BC;
extern GpRec14         D_acropolis_sanctuary_801809F8;
extern GpRec14         D_acropolis_sanctuary_80180A0C;
extern s32             D_acropolis_sanctuary_80180AE8;
extern u8              D_acropolis_sanctuary_80181814[];
extern TaskDesc        D_acropolis_sanctuary_80182240;
extern AcsBlockerSet   D_acropolis_sanctuary_801822EC;
extern GpMsgEntry      D_acropolis_sanctuary_80182310[];
extern AcsTile         D_acropolis_sanctuary_80182320[];
extern AcsQuad         D_acropolis_sanctuary_80182710[];
extern s16             D_acropolis_sanctuary_80182750[];
extern s32             D_acropolis_sanctuary_80182770;
extern SVECTOR         D_acropolis_sanctuary_80182774[];
extern u16             D_acropolis_sanctuary_801827D4[];
extern AcsBlockerSet   D_acropolis_sanctuary_80183568;
extern Task*           D_acropolis_sanctuary_80186C90;
/// Gameplay's LCG seed; it has no module header yet.
extern s32 Gp_LcgState;
/// Payloads the sanctuary cutscene task sends: `..._801820E4` is the record
/// slot-3 msg 0x3F4 takes and `..._801820F0` / `..._801821C8` the script pair
/// `func_800E8634` is started on.
extern s32  D_acropolis_sanctuary_801820E4;
extern s32  D_acropolis_sanctuary_801820F0;
extern s32  D_acropolis_sanctuary_801821C8;
extern void func_acropolis_sanctuary_8017DD78(void);
extern void func_acropolis_sanctuary_8017DF88(s32 arg0, s32 arg1);

/// The sanctuary cutscene task. State 0 allocates the task's `AcsCutsceneWork`
/// block, captures slot 3 in it, publishes the task itself in
/// `D_acropolis_sanctuary_80186C90` and cues the scene: slot 3 is sent the
/// 0x3E8 weapon record for the equipped weapon, the scene's sound event is
/// enqueued and its script pair is started.
///
/// State 1 drives the scene. `GameSession::field_1` reaching 0 instead stops
/// the sound, writes the room's exit into the save and hands off to task 0x11
/// before the task kills itself. Otherwise the scene fires exactly
/// once, when `func_acropolis_sanctuary_8017DCE0` has armed `phase` at 2 and
/// `step` is still 0: the player's effects are dropped, slot 3 is given the
/// 0x3F4 record and then warped to the scene's mark with a 0x3E9 placement, and
/// `step` is bumped so the next frame does nothing.

/// Per-frame visibility gate for the sanctuary's item object: hides the model
/// (`field_C` bit 0x80) while the camera sits on view 0xB or 0xD, or once the
/// item's 2-bit pickup flag has reached 2; otherwise shows it again with the
/// default flags.
void func_acropolis_sanctuary_80180264(Task* task)
{
    GpItemObj8* obj = task->spawnArg2;
    TmdObject*  tmd = task->extra;
    s32         flag;
    s32         view;

    flag = Gp_GetCurBit2Flag(obj->field_8);
    view = Gp_GetViewIndex();
    if (view == 0xB || view == 0xD || flag == 2) {
        tmd->field_C = 0x80;
    } else {
        tmd->field_C = 8;
        tmd->field_E = 0;
    }
}
