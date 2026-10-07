/* Private per-instance storage. Include at the original data position.
 * The configuration contract is documented in cap_captions.h. */

/// Font texture-page origin X in VRAM words (initially 384).
///
/// Resource selection replaces this signed halfword even if CAP relocation fails.
/// Glyph and title packets encode it with the current blend mode using `getTPage`.
static s16 _gCapCaptionTexturePageX = 384;

/// Font texture-page origin Y in VRAM rows (initially zero).
///
/// Paired with `_gCapCaptionTexturePageX`; resource selection replaces it even
/// on failure. This is a VRAM page coordinate, not a glyph-local texture V.
static s16 _gCapCaptionTexturePageY = 0;
