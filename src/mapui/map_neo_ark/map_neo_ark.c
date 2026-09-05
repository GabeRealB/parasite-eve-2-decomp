#include "common.h"

extern s32 D_800820E0;
extern s16 D_800820E4;
extern s16 D_800820E6;

#include "main/fs.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "mapui/map_neo_ark.h"

/// MDEC buffer layout hook for the Neo Ark map, reached from
/// `Mdec_SetupBuffers` (main) for stream kinds 6 and 9. Both kinds park the
/// two decode buffers (`D_8006AC48`) and the two VLC buffers (`D_8006AC50`)
/// around the frame allocation, differing only in how far apart the halves sit;
/// kind 9 additionally rewinds the CD queue and asks for the 0x180 x 0x100
/// display. `D_8006AC44` always ends up one full frame past the second decode
/// buffer.
void func_map_neo_ark_801799BC(u8* arg0)
{
    CdCmdQueue* q = &CdCmd_Queue;
    s32         stride;

    switch (arg0[2]) {
        case 6:
            stride        = D_8006AC5A * D_8006AC6C * 2;
            D_8006AC50[0] = (u_long*)((u8*)D_8006AC60 + 0x10000);
            D_8006AC50[1] = (u_long*)D_8006AC40;
            D_8006AC48[0] = (u_long*)((u8*)D_8006AC40 + stride);
            D_8006AC48[1] = (u_long*)((u8*)D_8006AC48[0] + stride);
            break;
        case 9:
            q->field_22C           = 1;
            q->field_230           = 0x180;
            q->field_232           = 0x100;
            D_8006AC5C             = 1;
            D_8006AC50[0]          = (u_long*)((u8*)D_8006AC60 + 0x10000);
            D_8006AC48[1]          = (u_long*)D_8006AC40;
            D_8006AC48[0]          = (u_long*)D_8006AC40;
            D_8006AC50[1]          = (u_long*)((u8*)D_8006AC50[0] + D_8006AC5A * D_8006AC6C);
            Game_Session->field_80 = 0;
            q->field_24A           = 1;
            break;
    }
    D_8006AC44             = (u8*)D_8006AC48[1] + D_8006AC5A * D_8006AC6C * 2;
    Game_Session->field_7C = 0;
    Game_Session->field_7E = 0;
}

/// Fills in the marker state for one Neo Ark map room. Most rooms have no
/// marker; the five that do read a GameFlag nibble, either straight (plus one,
/// rooms 7 / 13 / 32) or folded into a fixed set of states (rooms 20 and 21).
s32 func_map_neo_ark_80179B14(MapNeoArkRec* arg0, MapNeoArkOut* arg1)
{
    if (arg0->field_5 == 0) {
        switch (arg0->field_0) {
            case 7:
                arg1->field_3 = GameFlag_GetNibble(0xE1) + 1;
                break;
            case 13:
                arg1->field_3 = GameFlag_GetNibble(0xD9) + 1;
                break;
            case 20:
                arg1->field_3 = 1;
                if (GameFlag_GetNibble(0xDD) != 0) {
                    if (GameFlag_GetNibble(0xDC) != 0) {
                        arg1->field_3 = 3;
                    } else {
                        arg1->field_3 = 2;
                    }
                }
                break;
            case 21:
                if (GameFlag_GetNibble(0xE9) != 0) {
                    arg1->field_3 = 4;
                } else {
                    arg1->field_3 = 1;
                }
                break;
            case 32:
                arg1->field_3 = GameFlag_GetNibble(0xDD) + 1;
                break;
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
            case 19:
            case 22:
            case 23:
            case 24:
            case 25:
            case 26:
            case 27:
            case 28:
            case 29:
            case 30:
            case 31:
            default:
                break;
        }
    }
    return 1;
}

/// Music-volume hook the Midi driver calls for the Neo Ark map (via
/// `func_80179BE4`, see `Midi_UpdateVoiceVolumes`). `arg1` is the cue type:
/// 0x21 ducks the song down to half `arg0`, 0x10 brings it back up to `arg0`,
/// and anything else plays at `arg0` with both ramps reset. Each ramp waits out
/// its own counter (0x79 / 0xF1 frames), then walks `D_800820E0` by 0x300 a
/// frame until it reaches the target and the counter is parked at 0xFF.
s32 func_map_neo_ark_80179BE4(u32 arg0, u8 arg1, LinInterp* arg2)
{
    s32 volume;
    u32 temp;

    if (arg1 == 0x21) {
        if (D_800820E4 == 0) {
            D_800820E4 = 1;
            D_800820E6 = 0;
            D_800820E0 = arg0;
        } else if (D_800820E4 < 0x79) {
            D_800820E4 = D_800820E4 + 1;
        } else if ((arg0 >> 1) < (u32)D_800820E0) {
            D_800820E0 = D_800820E0 - 0x300;
        } else {
            D_800820E4 = 0xFF;
            D_800820E6 = 0;
            D_800820E0 = arg0 >> 1;
        }
        temp   = Midi_GetMasterVolume() & 0xFF;
        temp   = temp * D_800820E0;
        volume = LinInterp_Apply(arg2, temp / 127U);
    } else if (arg1 == 0x10) {
        if (D_800820E6 == 0) {
            D_800820E4 = 0;
            D_800820E6 = 1;
            D_800820E0 = arg0 >> 1;
        } else if (D_800820E6 < 0xF1) {
            D_800820E6 = D_800820E6 + 1;
        } else if ((u32)D_800820E0 < arg0) {
            D_800820E0 = D_800820E0 + 0x300;
        } else {
            D_800820E4 = 0;
            D_800820E6 = 0xFF;
            D_800820E0 = arg0;
        }
        temp   = Midi_GetMasterVolume() & 0xFF;
        temp   = temp * D_800820E0;
        volume = LinInterp_Apply(arg2, temp / 127U);
    } else {
        temp       = Midi_GetMasterVolume() & 0xFF;
        temp       = temp * arg0;
        volume     = LinInterp_Apply(arg2, temp / 127U);
        D_800820E4 = 0;
        D_800820E6 = 0;
    }
    return volume;
}
