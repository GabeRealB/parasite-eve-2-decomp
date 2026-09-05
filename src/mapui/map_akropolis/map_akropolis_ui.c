#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "main/display.h"
#include "main/task.h"
#include "main/ui.h"

void func_map_akropolis_80179C50(DialogPrompt* arg0, UiObject* arg1);
void func_map_akropolis_80179D78(Task* task);
void func_map_akropolis_80179E8C(Task* task);

/// Which key-item row the panel last had selected. Lives here rather than in
/// the panel's own unit because it sits inside this data run.
s32 D_map_akropolis_8017A9A8 = 0;

/// The four text ids the key-item rows draw, in row order.
s32 D_map_akropolis_8017A9AC[4] = { 0x109, 0x10A, 0x10B, 0x10C };

/// One-entry row-draw table for the list below; `UiList.funcs` points here.
UiListItemFunc D_map_akropolis_8017A9BC[1] = { func_map_akropolis_80179C50 };

/// The key-item list: four rows of one line each, 0x0F tall, everything else
/// filled in at runtime by the list reset.
UiList D_map_akropolis_8017A9C0 = {
    D_map_akropolis_8017A9BC,
    4,
    4,
    1,
    0x0F,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    { 0 },
    0,
    0,
};

/// The panel `Ui_SpawnFromDesc` builds for the key-item view: 0xFF9C x 0xFFD8,
/// 0xC8 x 0x3C, drawn by func_map_akropolis_80179D78.
UiObjectDesc D_map_akropolis_8017A9E4 = {
    2,
    0xFF9C,
    0xFFD8,
    0xC8,
    0x3C,
    0x3C,
    0,
    0,
    0xC0,
    (s32)func_map_akropolis_80179D78,
    0,
};

/// The display-mode task `Display_InitModeObj` seeds for this map.
TaskDesc D_map_akropolis_8017AA00 = { 0, 0xC0, func_map_akropolis_80179E8C, 0 };
