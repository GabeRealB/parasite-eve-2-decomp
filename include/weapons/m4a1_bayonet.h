#ifndef M4A1_BAYONET_H
#define M4A1_BAYONET_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgs.h>

/// The two translations the bayonet's coordinate frames sit at inside their
/// parent (the muzzle): `[0]` `(0, 0x300, 0x40)` is the blade tip used while
/// the thrust is winding up and `[1]` `(0, 0x180, 0x40)` the hilt. The second
/// element is also labelled `D_m4a1_bayonet_8011DED0` because the sweep state
/// addresses it as a symbol of its own.
extern SVECTOR D_m4a1_bayonet_8011DEC8[2];

/// `D_m4a1_bayonet_8011DEC8[1]` under its own label.
extern SVECTOR D_m4a1_bayonet_8011DED0;

/// The blade's motion trail: eight tip and eight hilt coordinate frames,
/// parented to `Gfx_ViewCoord`. The sweep state overwrites slot
/// `GpEffWork::field_22 & 7` each frame and the ribbon is drawn between the
/// two rings.
extern GsCOORDINATE2 D_m4a1_bayonet_8012D398[8];
extern GsCOORDINATE2 D_m4a1_bayonet_8012D618[8];

/// Links the blade trail's polygons for the slot `index`, with `arg1` the
/// packed colour/size the ribbon is drawn at.
void func_m4a1_bayonet_8011D69C(s32 index, s32 arg1);

#endif
