#ifndef GAMEPLAY_PRIVATE_LINKED_ACTORS_H
#define GAMEPLAY_PRIVATE_LINKED_ACTORS_H

#include "types.h"

#include "hud.h"

/// Area-wireframe options; use 0..3 for upright areas or CYLINDER | PROJECTILE.
enum {
    ATTACHMENT_AREA_WIREFRAME_CYLINDER   = 1, // Clear selects the ellipsoid dome
    ATTACHMENT_AREA_WIREFRAME_AHEAD      = 2, // Centre one radius along the player's +Z axis
    ATTACHMENT_AREA_WIREFRAME_PROJECTILE = 4  // Centre at player part 4 plus a root-axis offset; axis follows player +Z
};

/// Flashes every scan-eligible enemy, or records the current PE attack on release.
///
/// `release` is 0 for preview, nonzero for contact insertion. Not-lockable
/// entries participate only with KEEP_SCANNED set. Requires an acyclic list
/// of live embedded enemy nodes; release also requires a valid current spell
/// id (hundreds 1..6, tens 1..3, level 1..3) and writable contact tables ending
/// in WORLD_COLLISION_CONTACT_LAST. No pointers are retained.
void attachmentTargetAll(s32 release);

/// Draws the animated wireframe of a Parasite Energy area around the player.
///
/// `radius` and `extent` are nonnegative world-unit dimensions: horizontal
/// radius and vertical semi-axis for a dome, or radius and length for a cylinder.
/// `options` uses ATTACHMENT_AREA_WIREFRAME_*; `unused` is ignored. Projectile
/// placement needs at least five player coordinates; other modes need the root.
/// Requires composed player/view matrices, initialized projection and scratch
/// stack, and frame-arena space for up to 144 line packets. Vertices narrow to
/// signed halfwords, and projection flags do not clip or reject lines. Scratch
/// storage is released before return; the GPU borrows packets until completion.
void attachmentDrawAreaWireframe(s32 unused, s32 radius, s32 extent, s32 options);

/// Previews or records PE contacts for enemies inside a player-relative ellipsoid.
///
/// `release` and list/contact requirements are those of `attachmentTargetAll`.
/// Dimensions are nonnegative world units; nonzero `ahead` shifts the centre
/// along player +Z by radius plus the 100-unit targeting allowance. Both axes
/// receive that allowance. Y spans -(extent + 100) through +100. The ellipsoid
/// test uses signed-halfword positions reduced by four bits and squared axes
/// reduced by eight bits. Requires refreshed player-relative enemy positions
/// and an initialized scratch stack; no pointers are retained.
void attachmentTargetEllipsoid(s32 release, s32 radius, s32 extent, s32 ahead);

/// Previews or records PE contacts for enemies inside an upright player-relative cylinder.
///
/// `release`, dimensions, placement, list and scratch requirements are those of
/// `attachmentTargetEllipsoid`. The test uses signed-halfword positions, accepts
/// Y from -(extent + 100) through +100, and includes the circular boundary.
void attachmentTargetCylinder(s32 release, s32 radius, s32 extent, s32 ahead);

/// Animates and draws player HP/MP bars, cast-cost preview and status icons.
///
/// Numeric values use live stats; bar values move one point toward them per
/// call. The preview spends MP, or twice its amount in HP under Berserker.
/// `hud` is borrowed live HUD state, with a nonnegative `previewCastCost`.
/// A debug HUD-hide flag suppresses drawing and animation. Also draws the live
/// companion's HP except for family 2. Requires HUD/UI textures, writable front
/// ordering-table tags and sufficient frame-arena capacity. Queued primitives
/// remain borrowed until GPU completion; no state pointer is retained.
void hudDrawStatusBlock(const HudState* hud);

/// Radar marker kinds; targeted enemies are queued one tag in front of other markers.
enum {
    HUD_RADAR_MARKER_PLAYER   = 0,
    HUD_RADAR_MARKER_ENEMY    = 1,
    HUD_RADAR_MARKER_TARGETED = 2
};

/// Queues one translucent, raw-textured 8-pixel radar marker at a screen-centered anchor.
///
/// `centerX`/`centerY` are pixel anchors; the packet starts six pixels left and
/// eight above them, narrowing to signed halfwords. `markerKind` uses
/// HUD_RADAR_MARKER_*; other values draw the targeted marker. Requires the HUD
/// atlas and palette, one SPRT_8 of aligned arena capacity and writable tags -2
/// and -3. The GPU borrows the packet until drawing completes.
void hudDrawRadarMarker(s32 centerX, s32 centerY, s32 markerKind);

#endif // GAMEPLAY_PRIVATE_LINKED_ACTORS_H
