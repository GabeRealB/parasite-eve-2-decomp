#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "main/areas.h"
#include "main/display.h"
#include "display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "fs_types.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "text.h"

/* Define BSS before API headers to preserve first-declaration order. */
s16 Fs_BootLoadSlot;

u16 Fs_BootLoadPhase;

u16 D5B498_8006AC9C;

static u8 D_8006AC9E;

static u8 D_8006AC9F;

static s16 D_8006ACA0;

static s16 D_8006ACA2;

static s16 D_8006ACA4;

static s16 D_8006ACA6;

static s16 D_8006ACA8;

void* Fs_BootTimSecondary;

void* Fs_BootTimPrimary;

// Fade/clear color; written as halfword, often re-read as byte for TILE RGB.
static s16 D_8006ACB4;

BootLoadDestination Fs_LoadParams;

/// Unreferenced.
static s32 D_8006ACBC;

s16 D5B498_8006ACC0;

#include "fs.h"

/// September 3,  1999  7:07PM\nM.I.S.T.  Center, Los Angeles
static u8 BootCaptionText_MistEvening[];

static TextStream BootCaption_MistEvening;

/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_MistEveningSecondary;

/// September 3,  1999  7:45PM\nAkropolis Tower,  Los Angeles
static u8 BootCaptionText_AkropolisEvening[];

static TextStream BootCaption_AkropolisEvening;

/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_AkropolisEveningSecondary;

/// September 4,  1999  01:02PM\nMojave Desert,  Nevada
static u8 BootCaptionText_DesertAfternoon[];

static TextStream BootCaption_DesertAfternoon;

/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_DesertAfternoonSecondary;

/// September 5,  1999  01:95AM\nMesa on the outskirts of \nDryfield, Mojave Desert
static u8 BootCaptionText_DryfieldMesaNight[];

static TextStream BootCaption_DryfieldMesaNight;

/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_DryfieldMesaNightSecondary;

/// September 5,  1999  5:26PM\nWhite House,  Washington D.C.
static u8 BootCaptionText_WhiteHouseEvening[];

static TextStream BootCaption_WhiteHouseEvening;

/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_WhiteHouseEveningSecondary;

/// September 5,  1999  2:41PM\nMojave Desert,  Nevada
static u8 BootCaptionText_DesertReturn[];

static TextStream BootCaption_DesertReturn;

/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_DesertReturnSecondary;

/// September 6,  1999  8:94PM\nWhite House,  Washington D.C.
static u8 BootCaptionText_WhiteHouseAftermath[];

static TextStream BootCaption_WhiteHouseAftermath;

/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_WhiteHouseAftermathSecondary;

/// September 0, 1990  6:05PM\nNature Museum,  New York
static u8 BootCaptionText_MuseumFlashback[];

static TextStream BootCaption_MuseumFlashback;

/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_MuseumFlashbackSecondary;

/// September 3,  1999  \nM.I.S.T.  Center, Los Angeles
static u8 BootCaptionText_MistArrival[];

static TextStream BootCaption_MistArrival;

/// September 3,  1999\nM.I.S.T.  Center, Los Angeles
static u8 BootCaptionText_MistUnused[];

/// Retained unused caption variant.
static TextStream BootCaption_MistUnused;

/// September 3,  1999\nAkropolis Tower,  Los Angeles
static u8 BootCaptionText_Akropolis[];

static TextStream BootCaption_Akropolis;

/// September 4,  1999\nM.I.S.T.  Center, Los Angeles
static u8 BootCaptionText_MistDeparture[];

static TextStream BootCaption_MistDeparture;

/// September 4,  1999\nDryfield, Mojave Desert
static u8 BootCaptionText_Dryfield[];

static TextStream BootCaption_Dryfield;

/// September 5,  1999\nMine shaft, Mojave Desert
static u8 BootCaptionText_Mine[];

static TextStream BootCaption_Mine;

/// September 5,  1999\nDwelling level, Shelter
static u8 BootCaptionText_ShelterDwelling[];

static TextStream BootCaption_ShelterDwelling;

/// September 5,  1999\nWaste level, Shelter
static u8 BootCaptionText_ShelterWaste[];

static TextStream BootCaption_ShelterWaste;

/// September 5,  1999\nDryfield, Mojave Desert
static u8 BootCaptionText_DryfieldReturn[];

static TextStream BootCaption_DryfieldReturn;

/// September 5,  1999\nLaboratory level, Shelter
static u8 BootCaptionText_ShelterLaboratory[];

static TextStream BootCaption_ShelterLaboratory;

/// September 5,  1999\nPod shaft, Shelter
static u8 BootCaptionText_ShelterPod[];

static TextStream BootCaption_ShelterPod;

/// September 5,  1999\nExperiment level, Shelter
static u8 BootCaptionText_ShelterExperiment[];

static TextStream BootCaption_ShelterExperiment;

/// September 5,  1999\nHeliport, Shelter
static u8 BootCaptionText_ShelterHeliport[];

static TextStream BootCaption_ShelterHeliport;

static s32 Fade_StepIn(s16 arg0);

static void Fade_StartWhite(void);

static s32 Fade_StepOut(s32 arg0);

static void Fs_SelectLoadHandlers0(u8* arg0);

static void Fs_SelectLoadHandlers1(u8* arg0);

static void Fs_SelectLoadHandlers2(u8* arg0);

static void Fs_SelectLoadHandlers3(u8* arg0);

/// September 3,  1999  7:07PM\nM.I.S.T.  Center, Los Angeles
static u8 BootCaptionText_MistEvening[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x37,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    0x45,
    0x45,
    0xBB,
    0xBE,
    0xB4,
    0xBB,
    0x8F,
    0x8C,
    TEXT_STREAM_LINE_BREAK,
    0x0C,
    0x42,
    0x08,
    0x42,
    0x12,
    0x42,
    0x13,
    0x42,
    0x45,
    0x45,
    0x02,
    0x1E,
    0x27,
    0x2D,
    0x1E,
    0x2B,
    0x41,
    0x45,
    0x0B,
    0x28,
    0x2C,
    0x45,
    0x00,
    0x27,
    0x20,
    0x1E,
    0x25,
    0x1E,
    0x2C,
    TEXT_STREAM_END,
};
static TextStream BootCaption_MistEvening = { -150, -90, 960, 0, 0, 260, 1, 0, BootCaptionText_MistEvening, Caption_Glyphs, 13, 150, 216, 29 };
/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_MistEveningSecondary = { 0 };
/// September 3,  1999  7:45PM\nAkropolis Tower,  Los Angeles
static u8 BootCaptionText_AkropolisEvening[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x37,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    0x45,
    0x45,
    0xBB,
    0xBE,
    0xB8,
    0xB9,
    0x8F,
    0x8C,
    TEXT_STREAM_LINE_BREAK,
    0x00,
    0x24,
    0x2B,
    0x28,
    0x29,
    0x28,
    0x25,
    0x22,
    0x2C,
    0x45,
    0x13,
    0x28,
    0x30,
    0x1E,
    0x2B,
    0x41,
    0x45,
    0x45,
    0x0B,
    0x28,
    0x2C,
    0x45,
    0x00,
    0x27,
    0x20,
    0x1E,
    0x25,
    0x1E,
    0x2C,
    TEXT_STREAM_END,
};
static TextStream BootCaption_AkropolisEvening = { -150, -90, 960, 0, 0, 260, 1, 0, BootCaptionText_AkropolisEvening, Caption_Glyphs, 13, 150, 216, 29 };
/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_AkropolisEveningSecondary = { 0 };
/// September 4,  1999  01:02PM\nMojave Desert,  Nevada
static u8 BootCaptionText_DesertAfternoon[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x38,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    0x45,
    0x45,
    0xB4,
    0xB5,
    0xBE,
    0xB4,
    0xB6,
    0x8F,
    0x8C,
    TEXT_STREAM_LINE_BREAK,
    0x0C,
    0x28,
    0x23,
    0x1A,
    0x2F,
    0x1E,
    0x45,
    0x03,
    0x1E,
    0x2C,
    0x1E,
    0x2B,
    0x2D,
    0x41,
    0x45,
    0x45,
    0x0D,
    0x1E,
    0x2F,
    0x1A,
    0x1D,
    0x1A,
    TEXT_STREAM_END,
};
static TextStream BootCaption_DesertAfternoon = { -150, -90, 960, 0, 0, 260, 1, 0, BootCaptionText_DesertAfternoon, Caption_Glyphs, 13, 150, 216, 29 };
/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_DesertAfternoonSecondary = { 0 };
/// September 5,  1999  01:95AM\nMesa on the outskirts of \nDryfield, Mojave Desert
static u8 BootCaptionText_DryfieldMesaNight[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    0x45,
    0x45,
    0xB4,
    0xB5,
    0xBE,
    0xBD,
    0xB9,
    0x80,
    0x8C,
    TEXT_STREAM_LINE_BREAK,
    0x0C,
    0x1E,
    0x2C,
    0x1A,
    0x45,
    0x28,
    0x27,
    0x45,
    0x2D,
    0x21,
    0x1E,
    0x45,
    0x28,
    0x2E,
    0x2D,
    0x2C,
    0x24,
    0x22,
    0x2B,
    0x2D,
    0x2C,
    0x45,
    0x28,
    0x1F,
    0x45,
    TEXT_STREAM_LINE_BREAK,
    0x03,
    0x2B,
    0x32,
    0x1F,
    0x22,
    0x1E,
    0x25,
    0x1D,
    0x41,
    0x45,
    0x0C,
    0x28,
    0x23,
    0x1A,
    0x2F,
    0x1E,
    0x45,
    0x03,
    0x1E,
    0x2C,
    0x1E,
    0x2B,
    0x2D,
    TEXT_STREAM_END,
};
static TextStream BootCaption_DryfieldMesaNight = { -150, -90, 960, 0, 0, 260, 1, 0, BootCaptionText_DryfieldMesaNight, Caption_Glyphs, 13, 150, 216, 40 };
/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_DryfieldMesaNightSecondary = { 0 };
/// September 5,  1999  5:26PM\nWhite House,  Washington D.C.
static u8 BootCaptionText_WhiteHouseEvening[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    0x45,
    0x45,
    0xB9,
    0xBE,
    0xB6,
    0xBA,
    0x8F,
    0x8C,
    TEXT_STREAM_LINE_BREAK,
    0x16,
    0x21,
    0x22,
    0x2D,
    0x1E,
    0x45,
    0x07,
    0x28,
    0x2E,
    0x2C,
    0x1E,
    0x41,
    0x45,
    0x45,
    0x16,
    0x1A,
    0x2C,
    0x21,
    0x22,
    0x27,
    0x20,
    0x2D,
    0x28,
    0x27,
    0x45,
    0x03,
    0x42,
    0x02,
    0x42,
    TEXT_STREAM_END,
};
static TextStream BootCaption_WhiteHouseEvening = { -150, -90, 960, 0, 0, 260, 1, 0, BootCaptionText_WhiteHouseEvening, Caption_Glyphs, 13, 150, 216, 29 };
/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_WhiteHouseEveningSecondary = { 0 };
/// September 5,  1999  2:41PM\nMojave Desert,  Nevada
static u8 BootCaptionText_DesertReturn[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    0x45,
    0x45,
    0xB6,
    0xBE,
    0xB8,
    0xB5,
    0x8F,
    0x8C,
    TEXT_STREAM_LINE_BREAK,
    0x0C,
    0x28,
    0x23,
    0x1A,
    0x2F,
    0x1E,
    0x45,
    0x03,
    0x1E,
    0x2C,
    0x1E,
    0x2B,
    0x2D,
    0x41,
    0x45,
    0x45,
    0x0D,
    0x1E,
    0x2F,
    0x1A,
    0x1D,
    0x1A,
    TEXT_STREAM_END,
};
static TextStream BootCaption_DesertReturn = { -150, -90, 960, 0, 0, 260, 1, 0, BootCaptionText_DesertReturn, Caption_Glyphs, 13, 150, 216, 29 };
/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_DesertReturnSecondary = { 0 };
/// September 6,  1999  8:94PM\nWhite House,  Washington D.C.
static u8 BootCaptionText_WhiteHouseAftermath[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x3A,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    0x45,
    0x45,
    0xBC,
    0xBE,
    0xBD,
    0xB8,
    0x8F,
    0x8C,
    TEXT_STREAM_LINE_BREAK,
    0x16,
    0x21,
    0x22,
    0x2D,
    0x1E,
    0x45,
    0x07,
    0x28,
    0x2E,
    0x2C,
    0x1E,
    0x41,
    0x45,
    0x45,
    0x16,
    0x1A,
    0x2C,
    0x21,
    0x22,
    0x27,
    0x20,
    0x2D,
    0x28,
    0x27,
    0x45,
    0x03,
    0x42,
    0x02,
    0x42,
    TEXT_STREAM_END,
};
static TextStream BootCaption_WhiteHouseAftermath = { -150, -90, 960, 0, 0, 260, 1, 0, BootCaptionText_WhiteHouseAftermath, Caption_Glyphs, 13, 150, 216, 29 };
/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_WhiteHouseAftermathSecondary = { 0 };
/// September 0, 1990  6:05PM\nNature Museum,  New York
static u8 BootCaptionText_MuseumFlashback[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x34,
    0x41,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x34,
    0x45,
    0x45,
    0x3A,
    0x3E,
    0x34,
    0x39,
    0x0F,
    0x0C,
    TEXT_STREAM_LINE_BREAK,
    0x0D,
    0x1A,
    0x2D,
    0x2E,
    0x2B,
    0x1E,
    0x45,
    0x0C,
    0x2E,
    0x2C,
    0x1E,
    0x2E,
    0x26,
    0x41,
    0x45,
    0x45,
    0x0D,
    0x1E,
    0x30,
    0x45,
    0x18,
    0x28,
    0x2B,
    0x24,
    TEXT_STREAM_END,
};
static TextStream BootCaption_MuseumFlashback = { -150, -90, 960, 0, 0, 260, 1, 0, BootCaptionText_MuseumFlashback, Caption_Glyphs, 13, 300, 216, 29 };
/// Legacy secondary caption; the current draw path ignores its second argument.
static TextStream BootCaption_MuseumFlashbackSecondary = { 0 };
/// September 3,  1999  \nM.I.S.T.  Center, Los Angeles
static u8 BootCaptionText_MistArrival[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x37,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    0x45,
    0x45,
    TEXT_STREAM_LINE_BREAK,
    0x0C,
    0x42,
    0x08,
    0x42,
    0x12,
    0x42,
    0x13,
    0x42,
    0x45,
    0x45,
    0x02,
    0x1E,
    0x27,
    0x2D,
    0x1E,
    0x2B,
    0x41,
    0x45,
    0x0B,
    0x28,
    0x2C,
    0x45,
    0x00,
    0x27,
    0x20,
    0x1E,
    0x25,
    0x1E,
    0x2C,
    TEXT_STREAM_END,
};
static TextStream BootCaption_MistArrival = { -150, -90, 960, 0, 0, 260, 1, 0, BootCaptionText_MistArrival, Caption_Glyphs, 13, 107, 216, 29 };
/// September 3,  1999\nM.I.S.T.  Center, Los Angeles
static u8 BootCaptionText_MistUnused[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x37,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x0C,
    0x42,
    0x08,
    0x42,
    0x12,
    0x42,
    0x13,
    0x42,
    0x45,
    0x45,
    0x02,
    0x1E,
    0x27,
    0x2D,
    0x1E,
    0x2B,
    0x41,
    0x45,
    0x0B,
    0x28,
    0x2C,
    0x45,
    0x00,
    0x27,
    0x20,
    0x1E,
    0x25,
    0x1E,
    0x2C,
    TEXT_STREAM_END,
};
/// Retained unused caption variant.
static TextStream BootCaption_MistUnused = { -150, -90, 960, 0, 0, 260, 3, 0, BootCaptionText_MistUnused, Caption_Glyphs, 13, 300, 216, 29 };
/// September 3,  1999\nAkropolis Tower,  Los Angeles
static u8 BootCaptionText_Akropolis[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x37,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x00,
    0x24,
    0x2B,
    0x28,
    0x29,
    0x28,
    0x25,
    0x22,
    0x2C,
    0x45,
    0x13,
    0x28,
    0x30,
    0x1E,
    0x2B,
    0x41,
    0x45,
    0x45,
    0x0B,
    0x28,
    0x2C,
    0x45,
    0x00,
    0x27,
    0x20,
    0x1E,
    0x25,
    0x1E,
    0x2C,
    TEXT_STREAM_END,
};
static TextStream BootCaption_Akropolis = { -150, -90, 960, 0, 0, 260, 3, 0, BootCaptionText_Akropolis, Caption_Glyphs, 13, 300, 216, 29 };
/// September 4,  1999\nM.I.S.T.  Center, Los Angeles
static u8 BootCaptionText_MistDeparture[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x38,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x0C,
    0x42,
    0x08,
    0x42,
    0x12,
    0x42,
    0x13,
    0x42,
    0x45,
    0x45,
    0x02,
    0x1E,
    0x27,
    0x2D,
    0x1E,
    0x2B,
    0x41,
    0x45,
    0x0B,
    0x28,
    0x2C,
    0x45,
    0x00,
    0x27,
    0x20,
    0x1E,
    0x25,
    0x1E,
    0x2C,
    TEXT_STREAM_END,
};
static TextStream BootCaption_MistDeparture = { -150, -90, 960, 0, 0, 260, 3, 0, BootCaptionText_MistDeparture, Caption_Glyphs, 13, 300, 216, 29 };
/// September 4,  1999\nDryfield, Mojave Desert
static u8 BootCaptionText_Dryfield[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x38,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x03,
    0x2B,
    0x32,
    0x1F,
    0x22,
    0x1E,
    0x25,
    0x1D,
    0x41,
    0x45,
    0x0C,
    0x28,
    0x23,
    0x1A,
    0x2F,
    0x1E,
    0x45,
    0x03,
    0x1E,
    0x2C,
    0x1E,
    0x2B,
    0x2D,
    TEXT_STREAM_END,
};
static TextStream BootCaption_Dryfield = { -150, -90, 960, 0, 0, 260, 3, 0, BootCaptionText_Dryfield, Caption_Glyphs, 13, 300, 216, 29 };
/// September 5,  1999\nMine shaft, Mojave Desert
static u8 BootCaptionText_Mine[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x0C,
    0x22,
    0x27,
    0x1E,
    0x45,
    0x2C,
    0x21,
    0x1A,
    0x1F,
    0x2D,
    0x41,
    0x45,
    0x0C,
    0x28,
    0x23,
    0x1A,
    0x2F,
    0x1E,
    0x45,
    0x03,
    0x1E,
    0x2C,
    0x1E,
    0x2B,
    0x2D,
    TEXT_STREAM_END,
};
static TextStream BootCaption_Mine = { -150, -90, 960, 0, 0, 260, 3, 0, BootCaptionText_Mine, Caption_Glyphs, 13, 300, 216, 29 };
/// September 5,  1999\nDwelling level, Shelter
static u8 BootCaptionText_ShelterDwelling[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x03,
    0x30,
    0x1E,
    0x25,
    0x25,
    0x22,
    0x27,
    0x20,
    0x45,
    0x25,
    0x1E,
    0x2F,
    0x1E,
    0x25,
    0x41,
    0x45,
    0x12,
    0x21,
    0x1E,
    0x25,
    0x2D,
    0x1E,
    0x2B,
    TEXT_STREAM_END,
};
static TextStream BootCaption_ShelterDwelling = { -150, -90, 960, 0, 0, 260, 3, 0, BootCaptionText_ShelterDwelling, Caption_Glyphs, 13, 300, 216, 29 };
/// September 5,  1999\nWaste level, Shelter
static u8 BootCaptionText_ShelterWaste[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x16,
    0x1A,
    0x2C,
    0x2D,
    0x1E,
    0x45,
    0x25,
    0x1E,
    0x2F,
    0x1E,
    0x25,
    0x41,
    0x45,
    0x12,
    0x21,
    0x1E,
    0x25,
    0x2D,
    0x1E,
    0x2B,
    TEXT_STREAM_END,
};
static TextStream BootCaption_ShelterWaste = { -150, -90, 960, 0, 0, 260, 3, 0, BootCaptionText_ShelterWaste, Caption_Glyphs, 13, 300, 216, 29 };
/// September 5,  1999\nDryfield, Mojave Desert
static u8 BootCaptionText_DryfieldReturn[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x03,
    0x2B,
    0x32,
    0x1F,
    0x22,
    0x1E,
    0x25,
    0x1D,
    0x41,
    0x45,
    0x0C,
    0x28,
    0x23,
    0x1A,
    0x2F,
    0x1E,
    0x45,
    0x03,
    0x1E,
    0x2C,
    0x1E,
    0x2B,
    0x2D,
    TEXT_STREAM_END,
};
static TextStream BootCaption_DryfieldReturn = { -150, -90, 960, 0, 0, 260, 3, 0, BootCaptionText_DryfieldReturn, Caption_Glyphs, 13, 300, 216, 29 };
/// September 5,  1999\nLaboratory level, Shelter
static u8 BootCaptionText_ShelterLaboratory[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x0B,
    0x1A,
    0x1B,
    0x28,
    0x2B,
    0x1A,
    0x2D,
    0x28,
    0x2B,
    0x32,
    0x45,
    0x25,
    0x1E,
    0x2F,
    0x1E,
    0x25,
    0x41,
    0x45,
    0x12,
    0x21,
    0x1E,
    0x25,
    0x2D,
    0x1E,
    0x2B,
    TEXT_STREAM_END,
};
static TextStream BootCaption_ShelterLaboratory = { -150, -90, 960, 0, 0, 260, 3, 0, BootCaptionText_ShelterLaboratory, Caption_Glyphs, 13, 300, 216, 29 };
/// September 5,  1999\nPod shaft, Shelter
static u8 BootCaptionText_ShelterPod[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x0F,
    0x28,
    0x1D,
    0x45,
    0x2C,
    0x21,
    0x1A,
    0x1F,
    0x2D,
    0x41,
    0x45,
    0x12,
    0x21,
    0x1E,
    0x25,
    0x2D,
    0x1E,
    0x2B,
    TEXT_STREAM_END,
};
static TextStream BootCaption_ShelterPod = { -150, -90, 960, 0, 0, 260, 3, 0, BootCaptionText_ShelterPod, Caption_Glyphs, 13, 300, 216, 29 };
/// September 5,  1999\nExperiment level, Shelter
static u8 BootCaptionText_ShelterExperiment[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x04,
    0x31,
    0x29,
    0x1E,
    0x2B,
    0x22,
    0x26,
    0x1E,
    0x27,
    0x2D,
    0x45,
    0x25,
    0x1E,
    0x2F,
    0x1E,
    0x25,
    0x41,
    0x45,
    0x12,
    0x21,
    0x1E,
    0x25,
    0x2D,
    0x1E,
    0x2B,
    TEXT_STREAM_END,
};
static TextStream BootCaption_ShelterExperiment = { -150, -90, 960, 0, 0, 260, 3, 0, BootCaptionText_ShelterExperiment, Caption_Glyphs, 13, 300, 216, 29 };
/// September 5,  1999\nHeliport, Shelter
static u8 BootCaptionText_ShelterHeliport[] = {
    0x12,
    0x1E,
    0x29,
    0x2D,
    0x1E,
    0x26,
    0x1B,
    0x1E,
    0x2B,
    0x45,
    0x39,
    0x41,
    0x45,
    0x45,
    0x35,
    0x3D,
    0x3D,
    0x3D,
    TEXT_STREAM_LINE_BREAK,
    0x07,
    0x1E,
    0x25,
    0x22,
    0x29,
    0x28,
    0x2B,
    0x2D,
    0x41,
    0x45,
    0x12,
    0x21,
    0x1E,
    0x25,
    0x2D,
    0x1E,
    0x2B,
    TEXT_STREAM_END,
};
static TextStream BootCaption_ShelterHeliport = { -150, -90, 960, 0, 0, 260, 3, 0, BootCaptionText_ShelterHeliport, Caption_Glyphs, 13, 300, 216, 29 };

static s32 Fade_StepIn(s16 arg0)
{
    TILE*     p;
    DR_TPAGE* dr;
    u8        color;
    RECT      rect;

    color             = D_8006ACB4;
    p                 = (TILE*)Gpu_SysPrimCursor;
    Gpu_SysPrimCursor = (u8*)(p + 1);
    setlen(p, 3);
    setcode(p, 0x62);
    p->r0 = color;
    p->g0 = color;
    p->b0 = color;
    p->x0 = -0xA0;
    p->y0 = -0x78;
    p->w  = 0x140;
    p->h  = 0xF0;
    addPrim(gGpuCurrentOt - 0x10, p);

    dr                = (DR_TPAGE*)Gpu_SysPrimCursor;
    Gpu_SysPrimCursor = (u8*)(dr + 1);
    setDrawTPage(dr, 0, 1, 0x40);
    addPrim(gGpuCurrentOt - 0x10, dr);

    if (D_8006ACB4 > 0x100) {
        rect.y = 0;
        rect.x = 0;
        rect.w = 0x140;
        rect.h = 0xF0;
        ClearImage(&rect, 0, 0, 0);
        rect.y = 0x110;
        ClearImage(&rect, 0, 0, 0);
        gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
        return 1;
    }
    D_8006ACB4 += arg0;
    return 0;
}

static void Fade_StartWhite(void)
{
    TILE*     p;
    DR_TPAGE* dr;
    u8        color;

    Display_SetMode(DISPLAY_SETUP_DEFAULT | DISPLAY_SETUP_NO_CLEAR | DISPLAY_SETUP_KEEP_VIEW);
    SetDispMask(1);

    D_8006ACB4        = 0xFF;
    color             = *(volatile u8*)&D_8006ACB4;
    p                 = (TILE*)Gpu_SysPrimCursor;
    Gpu_SysPrimCursor = (u8*)(p + 1);
    setlen(p, 3);
    setcode(p, 0x62);
    p->r0 = color;
    p->g0 = color;
    p->b0 = color;
    p->x0 = -0xA0;
    p->y0 = -0x78;
    p->w  = 0x140;
    p->h  = 0xF0;
    addPrim(gGpuCurrentOt - 0x10, p);

    dr                = (DR_TPAGE*)Gpu_SysPrimCursor;
    Gpu_SysPrimCursor = (u8*)(dr + 1);
    setDrawTPage(dr, 0, 1, 0x40);
    addPrim(gGpuCurrentOt - 0x10, dr);
}

static s32 Fade_StepOut(s32 arg0)
{
    TILE*     p;
    DR_TPAGE* dr;
    u8        color;
    s16       val;

    gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_STRIPS;
    color                                   = *(volatile u8*)&D_8006ACB4;
    p                                       = (TILE*)Gpu_SysPrimCursor;
    Gpu_SysPrimCursor                       = (u8*)(p + 1);
    setlen(p, 3);
    setcode(p, 0x62);
    p->r0 = color;
    p->g0 = color;
    p->b0 = color;
    p->x0 = -0xA0;
    p->y0 = -0x78;
    p->w  = 0x140;
    p->h  = 0xF0;
    addPrim(gGpuCurrentOt - 0x10, p);

    dr                = (DR_TPAGE*)Gpu_SysPrimCursor;
    Gpu_SysPrimCursor = (u8*)(dr + 1);
    setDrawTPage(dr, 0, 1, 0x40);
    addPrim(gGpuCurrentOt - 0x10, dr);

    val        = D_8006ACB4 - arg0;
    D_8006ACB4 = val;
    return val < 0;
}

static void Fs_SelectLoadHandlers0(u8* arg0)
{
    switch (Fs_LoadParams.area) {
        case GAME_AREA_ACROPOLIS_SQUARE:
            Fs_BootTimPrimary = &BootCaption_Akropolis;
            *arg0             = 5;
            break;
        case GAME_AREA_ACROPOLIS_PLAZA:
            Fs_BootTimPrimary   = &BootCaption_AkropolisEvening;
            Fs_BootTimSecondary = &BootCaption_AkropolisEveningSecondary;
            *arg0               = 3;
            break;
        case 2:
        case 3:
        case 4:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        default:
            Fs_BootTimPrimary = &BootCaption_Akropolis;
            *arg0             = 6;
            break;
        case GAME_AREA_MIST_PARKING:
            if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) == 0) {
                Fs_BootTimPrimary = &BootCaption_MistArrival;
                *arg0             = 7;
            } else {
                Fs_BootTimPrimary = &BootCaption_MistDeparture;
                *arg0             = 8;
            }
            break;
        case GAME_AREA_MIST_SHOOTING_GALLERY:
            Fs_BootTimPrimary   = &BootCaption_MistEvening;
            Fs_BootTimSecondary = &BootCaption_MistEveningSecondary;
            *arg0               = 3;
            break;
    }
}

static void Fs_SelectLoadHandlers1(u8* arg0)
{
    switch (Fs_LoadParams.area) {
        case GAME_AREA_DRYFIELD_GAS_STATION:
            if (D5B498_8006ACC0 == 0) {
                Fs_BootTimPrimary = &BootCaption_Dryfield;
                *arg0             = 0xA;
            } else {
                Fs_BootTimPrimary   = &BootCaption_DesertAfternoon;
                Fs_BootTimSecondary = &BootCaption_DesertAfternoonSecondary;
                *arg0               = 3;
            }
            break;
        case GAME_AREA_DRYFIELD_MAIN_STREET:
            Fs_BootTimPrimary = &BootCaption_Dryfield;
            *arg0             = 0xA;
            break;
        case GAME_AREA_DRYFIELD_TRAILER_COACH:
            Fs_BootTimPrimary = &BootCaption_Dryfield;
            *arg0             = 0xB;
            break;
        case GAME_AREA_DRYFIELD_MOTEL_ROOM_6:
            Fs_BootTimPrimary = &BootCaption_Dryfield;
            *arg0             = 4;
            break;
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 28:
        case 29:
        case 31:
        case 32:
        case 33:
        case 34:
        case 35:
        case 36:
        case 37:
        case 38:
        default:
            Fs_BootTimPrimary = &BootCaption_Dryfield;
            *arg0             = 0x27;
            break;
    }
}

static void Fs_SelectLoadHandlers2(u8* arg0)
{
    s32 temp_v1;
    s32 area;
    s32 val;

    temp_v1 = gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER);
    area    = Fs_LoadParams.area;

    if (area == GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH) {
        goto case_1B;
    }
    if (area <= GAME_AREA_DRYFIELD_NIGHT_TRAILER_COACH) {
        if (area != GAME_AREA_DRYFIELD_NIGHT_MOTEL_LOBBY) {
            goto case_default;
        }
    } else {
        goto case_default;
    }

    val = temp_v1 & 0xFFFF;
    if (val == 4) {
        goto case_11_4;
    }
    if (val < 5) {
        goto case_11_def;
    }
    if (val == 5) {
        goto case_11_5;
    }
case_11_def:
    Fs_BootTimPrimary = &BootCaption_Dryfield;
    *arg0             = 0xC;
    return;
case_11_4:
    Fs_BootTimPrimary = &BootCaption_DryfieldReturn;
    *arg0             = 0x13;
    return;
case_11_5:
    Fs_BootTimPrimary = &BootCaption_DryfieldReturn;
    *arg0             = 0x1A;
    return;

case_1B:
    val = temp_v1 & 0xFFFF;
    if (val == 4) {
        goto case_1B_4;
    }
    if (val < 5) {
        goto case_1B_def;
    }
    if (val == 5) {
        goto case_1B_5;
    }
case_1B_def:
    Fs_BootTimPrimary = &BootCaption_Dryfield;
    *arg0             = 0xD;
    return;
case_1B_4:
    Fs_BootTimPrimary = &BootCaption_DryfieldReturn;
    *arg0             = 0x14;
    return;
case_1B_5:
    Fs_BootTimPrimary = &BootCaption_DryfieldReturn;
    *arg0             = 0x1B;
    return;

case_default:
    val = temp_v1 & 0xFFFF;
    if (val == 4) {
        goto case_def_4;
    }
    if (val < 5) {
        goto case_def_def;
    }
    if (val == 5) {
        goto case_def_5;
    }
case_def_def:
    Fs_BootTimPrimary = &BootCaption_Dryfield;
    *arg0             = 0xE;
    return;
case_def_4:
    Fs_BootTimPrimary = &BootCaption_DryfieldReturn;
    *arg0             = 0xF;
    return;
case_def_5:
    Fs_BootTimPrimary = &BootCaption_DryfieldReturn;
    *arg0             = 0x20;
}

static void Fs_SelectLoadHandlers3(u8* arg0)
{
    s32 temp_v1;
    s32 val;

    temp_v1 = gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER);
    switch (Fs_LoadParams.area) {
        case GAME_AREA_MINE_MESA:
            Fs_BootTimPrimary   = &BootCaption_DryfieldMesaNight;
            Fs_BootTimSecondary = &BootCaption_DryfieldMesaNightSecondary;
            *arg0               = 3;
            break;
        case GAME_AREA_MINE_REFUGE:
            val = temp_v1 & 0xFFFF;
            if (val == 4) {
                goto case_6_4;
            }
            if (val == 5) {
                goto case_6_5;
            }
        case_6_4:
            Fs_BootTimPrimary = &BootCaption_Mine;
            *arg0             = 0x10;
            return;
        case_6_5:
            Fs_BootTimPrimary = &BootCaption_Mine;
            *arg0             = 0x17;
            return;
        case GAME_AREA_SHELTER_B1_STERILIZATION_ROOM:
            val = temp_v1 & 0xFFFF;
            if (val == 5) {
                goto case_16_5;
            }
            if (val < 6) {
                goto case_16_def;
            }
            if (val == 6) {
                goto case_16_6;
            }
        case_16_def:
            Fs_BootTimPrimary = &BootCaption_ShelterDwelling;
            *arg0             = 0x11;
            return;
        case_16_5:
            Fs_BootTimPrimary = &BootCaption_ShelterDwelling;
            *arg0             = 0x18;
            return;
        case_16_6:
            Fs_BootTimPrimary = &BootCaption_ShelterDwelling;
            *arg0             = 0x24;
            return;
        case GAME_AREA_SHELTER_B1_UNDERGROUND_PARKING:
            val = temp_v1 & 0xFFFF;
            if (val == 5) {
                goto case_20_5;
            }
            if (val < 6) {
                goto case_20_def;
            }
            if (val == 6) {
                goto case_20_6;
            }
        case_20_def:
            Fs_BootTimPrimary = &BootCaption_ShelterDwelling;
            *arg0             = 0x28;
            return;
        case_20_5:
            Fs_BootTimPrimary = &BootCaption_ShelterDwelling;
            *arg0             = 0x1F;
            return;
        case_20_6:
            Fs_BootTimPrimary = &BootCaption_ShelterDwelling;
            *arg0             = 0x26;
            return;
        case GAME_AREA_SHELTER_R36:
            if (D5B498_8006ACC0 == 0) {
                Fs_BootTimPrimary   = &BootCaption_WhiteHouseAftermath;
                Fs_BootTimSecondary = &BootCaption_WhiteHouseAftermathSecondary;
                *arg0               = 3;
            } else {
                Fs_BootTimPrimary   = &BootCaption_MuseumFlashback;
                Fs_BootTimSecondary = &BootCaption_MuseumFlashbackSecondary;
                *arg0               = 3;
            }
            break;
        case GAME_AREA_SHELTER_B3_INCINERATOR_CONTROL_ROOM:
            val = temp_v1 & 0xFFFF;
            if (val == 4) {
                goto case_41_4;
            }
            if (val == 5) {
                goto case_41_5;
            }
        case_41_4:
            Fs_BootTimPrimary = &BootCaption_ShelterWaste;
            *arg0             = 0x12;
            return;
        case_41_5:
            Fs_BootTimPrimary = &BootCaption_ShelterWaste;
            *arg0             = 0x19;
            return;
        case GAME_AREA_SHELTER_B2_LABORATORY:
            val = temp_v1 & 0xFFFF;
            if (val == 5) {
                goto case_31_5;
            }
            if (val < 6) {
                goto case_31_def;
            }
            if (val == 6) {
                goto case_31_6;
            }
        case_31_def:
            Fs_BootTimPrimary = &BootCaption_ShelterLaboratory;
            *arg0             = 0x15;
            return;
        case_31_5:
            Fs_BootTimPrimary = &BootCaption_ShelterLaboratory;
            *arg0             = 0x1C;
            return;
        case_31_6:
            Fs_BootTimPrimary = &BootCaption_ShelterLaboratory;
            *arg0             = 0x25;
            return;
        case 2:
        case 3:
        case 4:
        case 5:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 17:
        case 18:
        case 19:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
        case 32:
        case 33:
        case 34:
        case 35:
        case 37:
        case 38:
        case 39:
        case 40:
        case 42:
        case 43:
        case 44:
        case 45:
        case 46:
        case 47:
        default:
            val = temp_v1 & 0xFFFF;
            if (val == 4) {
                goto case_def_4;
            }
            if (val == 5) {
                goto case_def_5;
            }
        case_def_4:
            Fs_BootTimPrimary = &BootCaption_ShelterPod;
            *arg0             = 0x16;
            return;
        case_def_5:
            Fs_BootTimPrimary = &BootCaption_ShelterPod;
            *arg0             = 0x1D;
            return;
    }
}

/* Pad after 47-entry jtbl for Fs_SelectLoadHandlers3 (original had trailing .word 0). */

void Fs_SetupBootLoad(void)
{
    u8    sp10[8];
    u8    sp18[8];
    RECT  rect;
    RECT* r;
    s32   area;

    memFillBytes(Fs_ImgBuffers, 0, sizeof(*Fs_ImgBuffers));
    rect.y = 0;
    rect.x = 0;
    r      = &rect;
    r->w   = 0x140;
    r->h   = 0xF0;
    ClearImage(r, 0, 0, 0);
    r->y = 0x110;
    ClearImage(r, 0, 0, 0);
    gDisplayState.control.flags.imageSource = DISPLAY_IMAGE_NONE;
    SetDispMask(0);
    sp10[3]             = 0;
    sp10[2]             = 0;
    Fs_BootTimSecondary = NULL;
    switch (Fs_LoadParams.stage) {
        case GAME_STAGE_ACROPOLIS:
            Fs_SelectLoadHandlers0(sp10);
            break;
        case GAME_STAGE_DRYFIELD:
            Fs_SelectLoadHandlers1(sp10);
            break;
        case GAME_STAGE_DRYFIELD_NIGHT:
            Fs_SelectLoadHandlers2(sp10);
            break;
        case GAME_STAGE_MINE_SHELTER:
            Fs_SelectLoadHandlers3(sp10);
            break;
        case GAME_STAGE_SHELTER_NEO_ARK:
        default:
            gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER);
            area = Fs_LoadParams.area;
            if (area == GAME_AREA_SHELTER_B6_NURSERY) {
                goto case_16;
            }
            if (area <= GAME_AREA_SHELTER_B6_NURSERY) {
                goto case_default;
            }
            if (area == GAME_AREA_NEO_ARK_R26) {
                goto case_1a;
            }
        case_default:
            if (D5B498_8006ACC0 == 0) {
                Fs_BootTimPrimary = &BootCaption_ShelterHeliport;
                sp10[0]           = 0x23;
            } else {
                Fs_BootTimPrimary   = &BootCaption_DesertReturn;
                Fs_BootTimSecondary = &BootCaption_DesertReturnSecondary;
                sp10[0]             = 3;
            }
            break;
        case_16:
            Fs_BootTimPrimary = &BootCaption_ShelterExperiment;
            sp10[0]           = 0x1E;
            break;
        case_1a:
            Fs_BootTimPrimary   = &BootCaption_WhiteHouseEvening;
            Fs_BootTimSecondary = &BootCaption_WhiteHouseEveningSecondary;
            sp10[0]             = 3;
            break;
    }
    sp18[0]         = 0;
    sp18[1]         = 0;
    sp18[2]         = 0;
    sp18[3]         = 0;
    Fs_BootLoadSlot = cdCmdEnqueue(CD_COMMAND_LOAD_FILE, sp10, sp18);
}

/* Alignment pad after the 5-entry Fs_SetupBootLoad jump table. */

void Fs_BootImageMachine(void* arg0, void* arg1)
{
    CdCmdQueue* queue;
    void*       secondary;
    s32         ret;
    s32         temp;

    queue     = &gCdCmdQueue;
    secondary = NULL;
    switch ((s16)D5B498_8006AC9C) {
        case 0:
            D_8006ACA0 = 0;
            D_8006AC9F = 0;
            D_8006AC9E = 0;
            D_8006ACA4 = 0;
            D_8006ACA2 = 0;
            D_8006ACA8 = 0;
            D_8006ACA6 = 0;
            Fade_StartWhite();
            D5B498_8006AC9C++;
            /* fallthrough */
        case 1:
            if ((Fade_StepOut(0x10) & 0xFFFF) != 0) {
                D5B498_8006AC9C++;
            }
            break;
        case 2:
            if (D_8006ACA6 < 0) {
                D_8006ACA2 = 1;
            }
            temp = TextStream_Draw(arg0, &D_8006AC9E, &D_8006ACA2, 0);
            if (D_8006ACA6 >= 0) {
                D_8006ACA6 = temp;
            }
            if (D_8006ACA0 >= 0x3D) {
                if (D_8006ACA8 < 0) {
                    D_8006ACA4 = 1;
                }
                if (secondary != NULL) {
                    ret = TextStream_Draw(secondary, &D_8006AC9F, &D_8006ACA4, 0);
                } else {
                    D_8006ACA8 = -1;
                }
                if (D_8006ACA8 >= 0) {
                    D_8006ACA8 = ret;
                } else {
                    goto check_done;
                }
            } else {
                D_8006ACA0++;
            }
            if (D_8006ACA8 >= 0) {
                break;
            }
        check_done:
            if (D_8006ACA6 >= 0) {
                break;
            }
            D_8006ACA0 = 0;
            D5B498_8006AC9C++;
            break;
        case 3:
            if (D_8006ACA0 >= 0x3C) {
                if (queue->holdBootImage != 0) {
                    goto draw;
                }
                D_8006ACB4 = 0;
                D5B498_8006AC9C++;
            } else {
                D_8006ACA0++;
            }
            goto draw;
        case 4:
            if ((Fade_StepIn(0x10) & 0xFFFF) != 0) {
                Fs_BootLoadPhase           = 0;
                gCdCmdQueue.bootLoadActive = 0;
            }
        draw:
            D_8006ACA4 = 1;
            D_8006ACA2 = 1;
            TextStream_Draw(arg0, &D_8006AC9E, &D_8006ACA2, 0);
            if (secondary != NULL) {
                TextStream_Draw(secondary, &D_8006AC9F, &D_8006ACA4, 0);
            }
            break;
    }
}
