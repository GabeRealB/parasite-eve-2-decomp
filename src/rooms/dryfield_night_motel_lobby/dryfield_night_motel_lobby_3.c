#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/dryfield_night_motel_lobby.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017ecb4.h"

extern s32 D_dryfield_night_motel_lobby_801844D4;

extern TaskDesc D_dryfield_night_motel_lobby_801827FC;

/// Runs one frame of the lobby's examine prompt. A busy cap suspends the whole
/// scan for that frame; otherwise the cursor is hit-tested against the room's
/// hotspot table and a confirm press on a hit hotspot is handed to the keypad
/// (`func_dryfield_night_motel_lobby_80180440`) or, while the prompt is idle,
/// latches the hotspot for the next prompt state. Hotspot id 0xB is the panel
/// the keypad is read from, and a code that checks out ends the sequence in
/// state 6. A cancel press ends it in state 5.
///
/// The two paths that leave early call the cursor draw themselves and return
/// rather than jumping to a shared label: the three identical call-and-epilogue
/// tails are what GCC's cross jumping folds into one, and that fold is what
/// leaves the argument setup standing before the merged call with the branches
/// landing past it. Writing a `goto` there compiles to a different tail.
void func_dryfield_night_motel_lobby_8017FE90(Task* task)
{
    DnmlExamineWork*  work   = (DnmlExamineWork*)task->work;
    RoomHotspot*      hs     = D_dryfield_night_motel_lobby_80182820;
    RoomActionPrompt* prompt = &D_80114D28;

    work->field_7          = 0;
    gGameSession->field_68 = 1;
    gGameSession->field_1  = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
    } else {
        prompt->targetId = 0x80;
        if (RoomsShared8017ecb4(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
            prompt->mode = 2;
            if (prompt->buttons[0].state == 2) {
                while (hs->id != -1) {
                    if (hs->hit != 0) {
                        if (work->promptBusy == 0) {
                            prompt->mode     = 0;
                            prompt->targetId = 0;
                            work->field_0    = hs->id;
                            work->promptKind = hs->promptKind;
                            task->state      = 3;
                            func_dryfield_night_motel_lobby_801802A8(task);
                            return;
                        }
                        if (work->field_6 == 0) {
                            if (hs->id == 0xB) {
                                work->field_6 = 1;
                                work->field_7 = 1;
                                SndEvt_EnqueueType6(0x53110007, 0, 0);
                            }
                            break;
                        }
                        func_dryfield_night_motel_lobby_80180440(task, hs->id);
                        if (work->field_8 != 0) {
                            Mc_SaveData.field_4 = 7;
                            task->state         = 6;
                            func_dryfield_night_motel_lobby_801802A8(task);
                            return;
                        }
                        break;
                    }
                    hs++;
                }
            }
        } else {
            prompt->mode = 1;
        }
        if (prompt->buttons[1].state == 2) {
            task->state = 5;
        }
    }
    func_dryfield_night_motel_lobby_801802A8(task);
}
