#include "common.h"

#include "main/sound.h"
#include "main/task.h"

#include "rooms/dryfield_night_motel_lobby.h"

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_lobby/dryfield_night_motel_lobby_4", func_dryfield_night_motel_lobby_801802A8);

void func_dryfield_night_motel_lobby_80180440(Task* task, s16 key)
{
    DnmlExamineWork* work = (DnmlExamineWork*)task->work;
    s32              i;

    switch (key) {
        case 0:
            SndEvt_EnqueueType6(0x53110007, 0, 0);
            if (work->field_2 < 7) {
                if (D_dryfield_night_motel_lobby_801844D8[0] != 0 || D_dryfield_night_motel_lobby_801844D8[1] != 0xA) {
                    D_dryfield_night_motel_lobby_801844D8[6] = D_dryfield_night_motel_lobby_801844D8[5];
                    D_dryfield_night_motel_lobby_801844D8[5] = D_dryfield_night_motel_lobby_801844D8[4];
                    D_dryfield_night_motel_lobby_801844D8[4] = D_dryfield_night_motel_lobby_801844D8[3];
                    D_dryfield_night_motel_lobby_801844D8[3] = D_dryfield_night_motel_lobby_801844D8[2];
                    D_dryfield_night_motel_lobby_801844D8[2] = D_dryfield_night_motel_lobby_801844D8[1];
                    D_dryfield_night_motel_lobby_801844D8[1] = D_dryfield_night_motel_lobby_801844D8[0];
                    D_dryfield_night_motel_lobby_801844D8[0] = key;
                    work->field_2++;
                }
            }
            break;
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
            SndEvt_EnqueueType6(0x53110007, 0, 0);
            if (work->field_2 < 7) {
                D_dryfield_night_motel_lobby_801844D8[work->field_2] = 0xA;
                D_dryfield_night_motel_lobby_801844D8[6]             = D_dryfield_night_motel_lobby_801844D8[5];
                D_dryfield_night_motel_lobby_801844D8[5]             = D_dryfield_night_motel_lobby_801844D8[4];
                D_dryfield_night_motel_lobby_801844D8[4]             = D_dryfield_night_motel_lobby_801844D8[3];
                D_dryfield_night_motel_lobby_801844D8[3]             = D_dryfield_night_motel_lobby_801844D8[2];
                D_dryfield_night_motel_lobby_801844D8[2]             = D_dryfield_night_motel_lobby_801844D8[1];
                D_dryfield_night_motel_lobby_801844D8[1]             = D_dryfield_night_motel_lobby_801844D8[0];
                D_dryfield_night_motel_lobby_801844D8[0]             = key;
                work->field_2++;
            }
            break;
        case 10:
            SndEvt_EnqueueType6(0x53110007, 0, 0);
            if (work->field_2 < 7) {
                if (D_dryfield_night_motel_lobby_801844D8[0] != 0 || D_dryfield_night_motel_lobby_801844D8[1] != 0xA) {
                    D_dryfield_night_motel_lobby_801844D8[6] = D_dryfield_night_motel_lobby_801844D8[5];
                    D_dryfield_night_motel_lobby_801844D8[5] = D_dryfield_night_motel_lobby_801844D8[4];
                    D_dryfield_night_motel_lobby_801844D8[4] = D_dryfield_night_motel_lobby_801844D8[3];
                    D_dryfield_night_motel_lobby_801844D8[3] = D_dryfield_night_motel_lobby_801844D8[2];
                    D_dryfield_night_motel_lobby_801844D8[2] = D_dryfield_night_motel_lobby_801844D8[1];
                    D_dryfield_night_motel_lobby_801844D8[1] = D_dryfield_night_motel_lobby_801844D8[0];
                    D_dryfield_night_motel_lobby_801844D8[0] = 0;
                    work->field_2++;
                    if (work->field_2 < 7) {
                        D_dryfield_night_motel_lobby_801844D8[6] = D_dryfield_night_motel_lobby_801844D8[5];
                        D_dryfield_night_motel_lobby_801844D8[5] = D_dryfield_night_motel_lobby_801844D8[4];
                        D_dryfield_night_motel_lobby_801844D8[4] = D_dryfield_night_motel_lobby_801844D8[3];
                        D_dryfield_night_motel_lobby_801844D8[3] = D_dryfield_night_motel_lobby_801844D8[2];
                        D_dryfield_night_motel_lobby_801844D8[2] = D_dryfield_night_motel_lobby_801844D8[1];
                        D_dryfield_night_motel_lobby_801844D8[1] = D_dryfield_night_motel_lobby_801844D8[0];
                        D_dryfield_night_motel_lobby_801844D8[0] = 0;
                        work->field_2++;
                    }
                }
            }
            break;
        case 11:
            SndEvt_EnqueueType6(0x53110007, 0, 0);
            work->field_2 = 0;
            work->field_7 = 1;
            for (i = 0; i < 7; i++) {
                D_dryfield_night_motel_lobby_801844D8[i] = 0xA;
            }
            break;
        case 12:
            SndEvt_EnqueueType6(0x53110007, 0, 0);
            work->field_2 = 0;
            work->field_7 = 1;
            for (i = 0; i < 7; i++) {
                D_dryfield_night_motel_lobby_801844D8[i] = 0xA;
            }
            break;
        case 13:
            if (func_dryfield_night_motel_lobby_80180734() != 0) {
                work->field_8 = 1;
            } else {
                SndEvt_EnqueueType6(0x53110009, 0, 0);
            }
            break;
    }
}

s16 func_dryfield_night_motel_lobby_80180734(void)
{
    u8* p = D_dryfield_night_motel_lobby_801844D8;

    if (p[6] != 0xA) {
        return 0;
    }
    if (p[5] != p[6]) {
        return 0;
    }
    if (p[4] != p[5]) {
        return 0;
    }
    if (p[3] != 3) {
        return 0;
    }
    if (p[2] != 0) {
        return 0;
    }
    /* Compares the third digit with the first rather than against a repeated
       literal: the earlier test leaves that load live, and re-testing it is
       what keeps it in one register instead of a fresh `addiu`. */
    if (p[1] != p[3]) {
        return 0;
    }
    return p[0] == 3;
}
