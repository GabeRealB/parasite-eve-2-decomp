#ifndef MAIN_DEVKIT_H
#define MAIN_DEVKIT_H

/* Code and data of the development kit's extra memory, above 0x80700000.
 * The retail console has 2MB and never maps it; the descriptor tables still
 * name these entries, which the debug environment provided.
 */

#include "common.h"

#include "main/task.h"
#include "main/tmd.h"
#include "main/fs.h"

void func_807011D8(Task* arg0);
void func_80701400(Task* arg0);
void func_80701470(Task* arg0);
void func_80703FE8(Task* arg0);
void func_80704A78(Task* arg0);
void func_80704AD0(Task* arg0);
void func_80704BC8(Task* arg0);
void func_80707534(Task* arg0);
void func_807075A0(Task* arg0);
void func_807077C0(Task* arg0);
void func_80707870(Task* arg0);
void func_80707980(Task* arg0);
void func_80707B14(Task* arg0);
void func_80707C38(Task* arg0);
void func_80707F84(Task* arg0);
void func_80708070(Task* arg0);
void func_807080C8(Task* arg0);
void func_80708778(Task* arg0);
void func_8070A6E8(Task* arg0);
void func_807127A8(Task* arg0);
void func_807146AC(Task* arg0);
void func_8071473C(Task* arg0);
void func_8071489C(Task* arg0);
void func_807149F0(Task* arg0);
void func_80714A48(Task* arg0);
void func_8071E24C(Task* arg0);
void func_80722624(Task* arg0);
void func_80723944(Task* arg0);
void func_807257A0(Task* arg0);
void func_80725BB8(Task* arg0);

extern TmdSource D_80725F44;
extern TmdSource D_8072C8F0;
extern TmdSource D_8075BED4;

/// Fixed addresses no image defines, which main points at: the image-buffer
/// region, the work area at 0x801FD000, the load addresses of the three actor
/// slots, and 4MB into the dev kit's memory.
extern FsImgBuffers D_801D7000;
extern u8           D_801FD000[];
extern u8           D_80131E20[];
extern u8           D_80149E20[];
extern u8           D_80161E20[];
extern u8           D_80400000[];

#endif /* MAIN_DEVKIT_H */
