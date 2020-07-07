#pragma once

enum color_space {
	CS_UNKNOWN = 0,   /**< Unknown */
	CS_BT_601 = 1,    /**< BT.601 */
	CS_BT_709 = 2,    /**< BT.709 */
	CS_SMPTE_170 = 3, /**< SMPTE.170 */
	CS_SMPTE_240 = 4, /**< SMPTE.240 */
	CS_BT_2020 = 5,   /**< BT.2020 */
	CS_RESERVED = 6,  /**< Reserved */
	CS_SRGB = 7       /**< sRGB */
};
enum color_range {
	CR_STUDIO_RANGE = 0, /**< Y [16..235], UV [16..240] */
	CR_FULL_RANGE = 1    /**< YUV/RGB [0..255] */
};

enum subsample_location: uint8_t {
	SUBSAMP_CENTER,
	SUBSAMP_LEFT_CORNER
};
struct video_sample_format {
	subsample_location location;
	uint8_t subsample_horiz, subsample_vert;
	uint8_t invert_uv;
	uint8_t planar;
	int bitdepth;
};
