#ifndef ROOMS_MINE_SECRET_PASSAGE_H
#define ROOMS_MINE_SECRET_PASSAGE_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// Draws a glow joining the two world-space points `arg0[0]` and `arg0[1]`;
/// `arg1` is the half-extent and `arg2` the colour, four bits per channel.
void func_mine_secret_passage_8017DC84(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Draws a four-wedge gouraud disc around the world-space point `arg0`;
/// `arg1` is the half-extent and `arg2` the centre colour, four bits per
/// channel.
void func_mine_secret_passage_8017E4C8(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Draws a camera-facing textured square at the projection of `arg0`; `arg1`
/// and `arg2` pick the texture cell and size, `arg3` the grey level and CLUT.
void func_mine_secret_passage_8017EB34(GsCOORDINATE2* arg0, u16 arg1, u16 arg2, u16 arg3);

/// Draws a gouraud ring of sixteen wedges around the projection of `coord`,
/// black at the half-extent `inner` and tinted by `rgb` at `inner + width`,
/// both scaled by depth.
void func_mine_secret_passage_8017EDF8(GsCOORDINATE2* coord, s32 inner, s32 width, u8* rgb);

/// Draws a gouraud disc of eight wedges around the projection of `coord`, lit
/// by `rgb` at the centre and fading to black; `radius` is a signed
/// half-extent scaled by depth.
void func_mine_secret_passage_8017F21C(GsCOORDINATE2* coord, s32 radius, u8* rgb);

/// Draws a two-ring glow of gouraud wedges around the projection of `coord`,
/// tinted by `rgb`; `radius` is a signed half-extent scaled by depth.
void func_mine_secret_passage_80180398(GsCOORDINATE2* coord, s16 radius, u8* rgb);

#endif // ROOMS_MINE_SECRET_PASSAGE_H
