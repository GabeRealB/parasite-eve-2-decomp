#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/energyshot.h"

extern s32 Gp_LcgState;

void func_energyshot_8012FA50(GsCOORDINATE2* arg0, s16 arg1, s32 arg2, u8* arg3);
void PeShared801305c0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);

/// Energy shot PE. `Task::spawnArg2` is the `GpEffWork` block; `Task::extra`
/// reaches the coordinate. Cancel (`Gp_StateC08.field_3 == -2` or
/// `Gp_State1C->field_E >= 4`) releases the work block.
///
/// State 0 parents the coordinate, seeds 16 texture-frame offsets and 16 wedge
/// yaws from `Gp_LcgState`, and plays the combo-indexed cue. State 1 grows
/// brightness / radius, draws three rings plus `field_0` wedges and the beam,
/// and parents a `0x600F4` spark; once brightness exceeds the row cap it
/// advances to state 2, which shrinks brightness until it drops below 0x11.
void func_energyshot_8012EF34(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpStateC08*    state;
    s32            i;
    u8             rgb[3];

    state = &Gp_StateC08;
    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if ((state->field_3 != -2) && (Gp_State1C->field_E < 4)) {
        mem->field_22 = (u16)mem->field_22 + 1;
        switch (arg0->state) {
            case 0: {
                GpMtxWords* rot;
                GpState1C*  st1c;
                s16         count;
                u16         level;

                rot               = (GpMtxWords*)&coord->coord;
                coord->sub        = mem->field_8;
                rot->w0           = 0x1000;
                rot->w1           = 0;
                rot->w2           = 0x1000;
                rot->w3           = 0;
                rot->h4           = 0x1000;
                coord->coord.t[2] = 0;
                coord->coord.t[1] = 0;
                coord->coord.t[0] = 0;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                state->field_6 |= 8;
                st1c            = Gp_State1C;
                st1c->field_14  = 0;
                st1c->field_12 &= 0xFBFF;
                arg0->state     = 1;
                mem->field_20   = (Gp_StateC08.field_0 % 10) - 1;
                i               = 0;
                {
                    s16* frames;

                    frames = D_energyshot_80130108;
                    do {
                        s32 rng;

                        i          += 1;
                        rng         = Gp_LcgState * 5 + 0x71357911;
                        *frames     = ((u32)rng >> 16) & 0xFF;
                        frames     += 1;
                        Gp_LcgState = rng;
                    } while (i < 0x10);
                }
                i = 0;
                {
                    EnergyShotScale* tbl;

                    tbl   = D_energyshot_801300E4;
                    count = tbl[mem->field_20].field_0;
                    level = mem->field_20;
                    if (count > 0) {
                        do {
                            s32 lo;
                            s32 rng;

                            lo                       = i * (0x1000 / D_energyshot_801300E4[(s16)level].field_0);
                            rng                      = Gp_LcgState * 5 + 0x71357911;
                            D_energyshot_80130128[i] = lo + (((u32)rng >> 16) & 0x1FF);
                            i                       += 1;
                            Gp_LcgState              = rng;
                            count                    = D_energyshot_801300E4[mem->field_20].field_0;
                            level                    = mem->field_20;
                        } while (i < count);
                    }
                }
                {
                    s32 pan;

                    pan = (s8)Gp_GetObjPan((GpObj38*)coord);
                    SndEvt_EnqueueType6(D_energyshot_801300FC[mem->field_20], pan,
                                        (s8)Gp_GetObjDepth((GpObj38*)coord));
                }
                SOFT_USE_REG(arg0);
                return;
            }
            case 1: {
                EnergyShotScale* table;
                EnergyShotScale* t2;
                s32              rng;
                s16              ang;
                s16*             p;
                s16              count;

                table             = D_energyshot_801300E4;
                mem->field_24     = (u16)mem->field_24 + table[mem->field_20].field_4;
                rgb[0]            = *(u8*)&mem->field_24;
                rgb[1]            = (u16)mem->field_24 >> 1;
                rgb[2]            = *(u8*)&mem->field_24;
                coord->coord.t[1] = -table[mem->field_20].field_6;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                Gp_DrawRing(coord, (s16)(mem->field_24 * 4), rgb);
                Gp_DrawRing(coord, (s16)(mem->field_24 * 8), rgb);
                Gp_DrawRing(coord, (s16)(mem->field_24 * 0xC), rgb);
                i     = 0;
                count = table[mem->field_20].field_0;
                if (count > 0) {
                    t2 = table;
                    p  = D_energyshot_80130128;
                    do {
                        PeShared801305c0(coord, (s16)(mem->field_24 * 6), *p, rgb);
                        p += 1;
                    } while (++i < t2[mem->field_20].field_0);
                }
                coord->coord.t[1] = 0;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                if (mem->field_20 != 0) {
                    if (mem->field_20 == 2) {
                        func_energyshot_8012FA50(coord, (s16)(mem->field_24 * 8),
                                                 (s32)(D_energyshot_801300FA << 16) >> 17, rgb);
                    }
                    func_energyshot_8012FA50(
                        coord, (s16)(mem->field_24 * 4),
                        (s32)((u16)D_energyshot_801300E4[mem->field_20].field_6 << 17) >> 16, rgb);
                }
                func_energyshot_8012FA50(
                    coord, (s16)(mem->field_24 * 6),
                    (s16)((u16)D_energyshot_801300E4[mem->field_20].field_6 - 0x100), rgb);
                rng           = Gp_LcgState * 5 + 0x71357911;
                ang           = ((u32)rng >> 16) & 0xFFF;
                Gp_LcgState   = rng;
                mem->field_26 = ang;
                mem->field_10 = (u32)(rsin(ang) * mem->field_24 * 3) >> 11;
                mem->field_14 = (u32)(rcos(mem->field_26) * mem->field_24 * 3) >> 11;
                Gp_SpawnEff(0x600F4, coord,
                            D_energyshot_801300E4[mem->field_20].field_6 | 0x8000,
                            (SVECTOR*)&mem->field_10);
                if (D_energyshot_801300E4[mem->field_20].field_2 < mem->field_24) {
                    Gp_SpawnEff(0x800600F3, coord, 0, 0);
                    mem->field_28 = (u16)mem->field_24;
                    arg0->state   = 2;
                }
                return;
            }
            case 2: {
                EnergyShotScale* table;
                EnergyShotScale* t2;
                s16*             p;
                s16              count;

                if (mem->field_24 < 0x11) {
                    goto release;
                }
                mem->field_24     = (u16)mem->field_24 - 0x10;
                rgb[0]            = *(u8*)&mem->field_24;
                rgb[1]            = (u16)mem->field_24 >> 1;
                rgb[2]            = *(u8*)&mem->field_24;
                table             = D_energyshot_801300E4;
                coord->coord.t[1] = -table[mem->field_20].field_6;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                Gp_DrawRing(coord, (s16)(table[mem->field_20].field_2 * 4), rgb);
                Gp_DrawRing(coord, (s16)(table[mem->field_20].field_2 * 8), rgb);
                Gp_DrawRing(coord, (s16)(table[mem->field_20].field_2 * 0xC), rgb);
                i     = 0;
                count = table[mem->field_20].field_0;
                if (count > 0) {
                    t2 = table;
                    p  = D_energyshot_80130128;
                    do {
                        PeShared801305c0(coord, (s16)(mem->field_28 * 6), *p, rgb);
                        p += 1;
                    } while (++i < t2[mem->field_20].field_0);
                }
                coord->coord.t[1] = 0;
                coord->flg        = 0;
                Gp_UpdateCoord(coord);
                if (mem->field_20 != 0) {
                    if (mem->field_20 == 2) {
                        mem->field_28 =
                            (u16)mem->field_28 + D_energyshot_801300E4[2].field_4;
                        func_energyshot_8012FA50(
                            coord, (s16)(mem->field_28 * 8),
                            (s32)((u16)D_energyshot_801300E4[mem->field_20].field_6 << 16) >> 17,
                            rgb);
                    }
                    func_energyshot_8012FA50(
                        coord, (s16)(mem->field_28 * 4),
                        (s32)((u16)D_energyshot_801300E4[mem->field_20].field_6 << 17) >> 16, rgb);
                }
                func_energyshot_8012FA50(
                    coord, (s16)(mem->field_28 * 6),
                    (s16)((u16)D_energyshot_801300E4[mem->field_20].field_6 - 0x100), rgb);
                return;
            }
        }
        return;
    }
release:
    Gp_ReleaseState1CMem(mem, arg0);
}

INCLUDE_RODATA("pe/nonmatchings/energyshot/energyshot", D_energyshot_8012EF30);
