#include "common.h"

#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflow.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017ecb4.h"
#include "rooms/shelter_r47.h"

extern u8 D_shelter_r47_8018A696;
extern u8 D_shelter_r47_8018A697;

INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47_7", D_shelter_r47_8017D7DC);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_8018431C);

/// Idle state of the second cap script. It counts `field_2C` down (with a
/// sound on reaching zero), runs `func_shelter_r47_801851B8`, and while no cap
/// is running and `field_2B` is clear, hit-tests the cursor against `hotspots`.
/// A confirmed hit (`buttons[0].state == 2`) latches the hotspot's `id` and
/// `promptKind` and advances to state 3; `buttons[1].state == 2` advances to
/// state 5 when `field_2A` is 0 and otherwise starts cap slot 0x25. `field_2A`
/// of 3 moves straight to state 9.
void func_shelter_r47_801844A0(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    ShelterR47State2* st     = (ShelterR47State2*)task->work;
    RoomHotspot*      hs     = st->hotspots;

    if (st->field_2C != 0) {
        if (--st->field_2C == 0) {
            SndEvt_EnqueueType6(0x542F0003, 0, 0);
        }
    }
    func_shelter_r47_801851B8(task);
    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if ((Gp_CapBusy() != 0) || (st->field_2B != 0)) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    if (st->field_2A == 3) {
        task->state = 9;
        return;
    }
    prompt->targetId = 0x80;
    if (RoomsShared8017ecb4(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if (prompt->buttons[0].state == 2) {
            for (; hs->id != -1; hs++) {
                if (hs->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    st->field_1A     = hs->id;
                    st->field_28     = hs->promptKind;
                    task->state      = 3;
                    return;
                }
            }
        }
    } else {
        prompt->mode = 1;
    }
    if (prompt->buttons[1].state == 2) {
        if (st->field_2A == 0) {
            task->state = 5;
            return;
        }
        Gp_StartCapSlot(0x25, 0, 0);
    }
}

/// State 4 of the second cap script: acts on the hotspot latched in
/// `field_1A` once `func_800D4EC0` reports non-zero. Hotspots 1 and 4 start one
/// of five cap slots picked by `field_1C`. Hotspots 2 and 3 start a cap while
/// their latch (`D_shelter_r47_8018A696` / `D_shelter_r47_8018A697`) is clear,
/// setting it, or otherwise set `field_1E` to -0x104, clear
/// `field_E`, `field_10`, `field_16` and `field_18`, play sound 0x542F0004 and
/// move to state 6. Every other path returns to state 2, except an unknown
/// hotspot id, which leaves the state unchanged.
void func_shelter_r47_80184658(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    ShelterR47State2* st     = (ShelterR47State2*)task->work;

    func_shelter_r47_801851B8(task);
    prompt->mode     = 0;
    prompt->targetId = 0;
    if (func_800D4EC0() != 0) {
        switch (st->field_1A) {
            case 1:
                switch (st->field_1C) {
                    case 0:
                        Gp_StartCapSlot(0x1D, 0, 0);
                        break;
                    case 1:
                        Gp_StartCapSlot(0x1E, 0, 0);
                        break;
                    case 2:
                        Gp_StartCapSlot(0x1F, 0, 0);
                        break;
                    case 3:
                        Gp_StartCapSlot(0x20, 0, 0);
                        break;
                    case 4:
                        Gp_StartCapSlot(0x21, 0, 0);
                        break;
                }
                break;
            case 2:
                if (st->field_2A != 0) {
                    Gp_StartCapSlot(0x24, 0, 0);
                    task->state = 2;
                    return;
                }
                if (D_shelter_r47_8018A696 == 0) {
                    Gp_StartCapSlot(0x35, 0, 0);
                    D_shelter_r47_8018A696 = 1;
                    task->state            = 2;
                    return;
                }
                goto snd;
            case 3:
                if (st->field_2A == 0 && D_shelter_r47_8018A697 == 0) {
                    Gp_StartCapSlot(0x34, 0, 0);
                    D_shelter_r47_8018A697 = 1;
                    task->state            = 2;
                    return;
                }
            snd:
                st->field_1E = -0x104;
                st->field_E  = 0;
                st->field_10 = 0;
                st->field_16 = 0;
                st->field_18 = 0;
                task->state  = 6;
                SndEvt_EnqueueType6(0x542F0004, 0, 0);
                SndEvt_EnqueueType7(0x542F0005, 1);
                return;
            case 4:
                switch (st->field_1C) {
                    case 0:
                        Gp_StartCapSlot(0x2F, 0, 0);
                        break;
                    case 1:
                        Gp_StartCapSlot(0x30, 0, 0);
                        break;
                    case 2:
                        Gp_StartCapSlot(0x31, 0, 0);
                        break;
                    case 3:
                        Gp_StartCapSlot(0x32, 0, 0);
                        break;
                    case 4:
                        Gp_StartCapSlot(0x33, 0, 0);
                        break;
                }
                break;
            default:
                return;
        }
    }
    task->state = 2;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_8018489C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80184AE0);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80184F40);

void func_shelter_r47_80185028(Task* task)
{
    ShelterR47State2* state;

    state = (ShelterR47State2*)task->work;
    func_shelter_r47_80183B84(task);
    func_shelter_r47_80183E24();
    func_shelter_r47_80183F0C();
    func_shelter_r47_80183FF4(task, state->field_1C);
    func_shelter_r47_80184124(task, state->field_1C);
    state->fade = 0;
    task->state++;
}

void func_shelter_r47_80185098(Task* task)
{
    ShelterR47State2* state;
    u16               fade;
    u8                level;

    state = (ShelterR47State2*)task->work;
    func_shelter_r47_80183B84(task);
    func_shelter_r47_80183E24();
    func_shelter_r47_80183F0C();
    func_shelter_r47_80183FF4(task, state->field_1C);
    func_shelter_r47_80184124(task, state->field_1C);
    fade        = state->fade + 0x10;
    state->fade = fade;
    if ((s16)fade >= 0x100) {
        state->fade = 0xFF;
        if (task->spawnArg1 != 1) {
            Gp_MsgPlayerWeapon(1);
        }
        Gp_MsgPlayer3F3(1);
        Display_ReleaseRef();
        if (state->field_2A != 1) {
            gGameSession->eventState = 0;
        }
        gGameSession->cutsceneHold = 0;
        taskKill((Task*)task->spawnArg2);
        Task_RequestKill(task, 0);
    }
    SndEvt_EnqueueType7(0x542F0005, 1);
    level = (u8)state->fade;
    Fade_DrawOverlay(level, level, level, 2);
}

void func_shelter_r47_801851B8(Task* task)
{
    ShelterR47State2* state;

    state = (ShelterR47State2*)task->work;
    func_shelter_r47_80183B84(task);
    func_shelter_r47_80183E24();
    func_shelter_r47_80183F0C();
    func_shelter_r47_80183FF4(task, state->field_1C);
    func_shelter_r47_80184124(task, state->field_1C);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_7", func_shelter_r47_80185214);
