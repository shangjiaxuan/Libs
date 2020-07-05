#include "media_buffer.h"

static struct channel_layout builtin_channel_layouts[] = {
	{ "None", 0, {}},
	{ "Mono", 1, {
		CH_FRONT_CENTER,
	}},
	{ "Stereo", 2, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
	}},
	{ "2.1", 3, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_LOW_FREQUENCY,
	}},
	{ "2_1 (3.0 (back))", 3, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_BACK_CENTER
	}},
	{ "Surround (3.0)", 3, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_FRONT_CENTER
	}},
	{ "3.1", 4, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_FRONT_CENTER,
		CH_LOW_FREQUENCY
	}},
	{ "4.0", 4, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_FRONT_CENTER,
		CH_BACK_CENTER
	}},
	{ "4.1", 5, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_FRONT_CENTER,
		CH_BACK_CENTER,
		CH_LOW_FREQUENCY
	}},
	{ "2_2 (Quad (side))", 4, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_SIDE_LEFT,
		CH_SIDE_RIGHT
	}},
	{ "Quad", 4, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_BACK_LEFT,
		CH_BACK_RIGHT
	}},
	{ "5.0", 5, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_FRONT_CENTER,
		CH_SIDE_LEFT,
		CH_SIDE_RIGHT
	}},
	{ "5.1", 6, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_FRONT_CENTER,
		CH_SIDE_LEFT,
		CH_SIDE_RIGHT,
		CH_LOW_FREQUENCY
	}},
	{ "5.0 (back)", 5, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_FRONT_CENTER,
		CH_BACK_LEFT,
		CH_BACK_RIGHT
	}},
	{ "5.1 (back)", 6, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_FRONT_CENTER,
		CH_BACK_LEFT,
		CH_BACK_RIGHT,
		CH_LOW_FREQUENCY
	}},
	{ "6.0", 6, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_FRONT_CENTER,
		CH_SIDE_LEFT,
		CH_SIDE_RIGHT,
		CH_BACK_CENTER,
	}},
	{ "6.0 (front)", 6, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_SIDE_LEFT,
		CH_SIDE_RIGHT,
		CH_FRONT_LEFT_OF_CENTER,
		CH_FRONT_RIGHT_OF_CENTER
	}},
	{ "Hexagonal", 6, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_FRONT_CENTER,
		CH_BACK_LEFT,
		CH_BACK_RIGHT,
		CH_BACK_CENTER,
	}},
	{ "6.1", 7, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_FRONT_CENTER,
		CH_SIDE_LEFT,
		CH_SIDE_RIGHT,
		CH_BACK_CENTER,
		CH_LOW_FREQUENCY
	}},
	{ "6.1 (back)", 7, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_FRONT_CENTER,
		CH_BACK_LEFT,
		CH_BACK_RIGHT,
		CH_BACK_CENTER,
		CH_LOW_FREQUENCY
	}},
	{ "6.1 (front)", 7, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_SIDE_LEFT,
		CH_SIDE_RIGHT,
		CH_FRONT_LEFT_OF_CENTER,
		CH_FRONT_RIGHT_OF_CENTER,
		CH_LOW_FREQUENCY
	}},
	{ "7.0", 7, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_FRONT_CENTER,
		CH_SIDE_LEFT,
		CH_SIDE_RIGHT,
		CH_BACK_RIGHT,
		CH_BACK_CENTER,
	}},
	{ "7.0 (front)", 7, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_FRONT_CENTER,
		CH_SIDE_LEFT,
		CH_SIDE_RIGHT,
		CH_FRONT_LEFT_OF_CENTER,
		CH_FRONT_RIGHT_OF_CENTER,
	}},
	{ "7.1", 8, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_FRONT_CENTER,
		CH_SIDE_LEFT,
		CH_SIDE_RIGHT,
		CH_BACK_LEFT,
		CH_BACK_RIGHT,
		CH_LOW_FREQUENCY
	}},
	{ "7.1 (wide)", 8, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_FRONT_CENTER,
		CH_SIDE_LEFT,
		CH_SIDE_RIGHT,
		CH_FRONT_LEFT_OF_CENTER,
		CH_FRONT_RIGHT_OF_CENTER,
		CH_LOW_FREQUENCY
	}},
	{ "7.1 (wide) (back)", 8, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_FRONT_CENTER,
		CH_BACK_LEFT,
		CH_BACK_RIGHT,
		CH_FRONT_LEFT_OF_CENTER,
		CH_FRONT_RIGHT_OF_CENTER,
		CH_LOW_FREQUENCY
	}},
	{ "Octagonal", 8, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_FRONT_CENTER,
		CH_SIDE_LEFT,
		CH_SIDE_RIGHT,
		CH_BACK_LEFT,
		CH_BACK_RIGHT,
		CH_BACK_CENTER,
	}},
	{ "Hexadecagonal", 16, {
		CH_FRONT_LEFT,
		CH_FRONT_RIGHT,
		CH_FRONT_CENTER,
		CH_SIDE_LEFT,
		CH_SIDE_RIGHT,
		CH_BACK_LEFT,
		CH_BACK_RIGHT,
		CH_BACK_CENTER,
		CH_WIDE_LEFT,
		CH_WIDE_RIGHT,
		CH_TOP_FRONT_LEFT,
		CH_TOP_FRONT_RIGHT,
		CH_TOP_FRONT_CENTER,
		CH_TOP_BACK_LEFT,
		CH_TOP_BACK_RIGHT,
		CH_TOP_BACK_CENTER
	}},
	{ "Stereo Downmix", 2, {
		CH_STEREO_LEFT,
		CH_STEREO_RIGHT
	}}
};

#define CHANNEL_NAME_ALIAS_COUNT 3
typedef const char* channel_names_t[CHANNEL_NAME_ALIAS_COUNT];
static channel_names_t channel_names[] = {
	{"(Invalid Channel)", NULL, NULL},
	{"Front Left", "FL", "front-left"},
	{"Front Right", "FR", "front-right"},
	{"Front Center", "FC", "front-center"},
	{"Front Left Center", "FLC", "front-left-of-center"},
	{"Front Right Center", "FRC", "front-right-of-center"},
	{"Back Left", "BL", "rear-left"},
	{"Back Right", "BR", "rear-right"},
	{"Back Center", "BC", "rear-center"},
	{"Side Left", "SL", "side-left"},
	{"Side Right", "SR", "side-right"},
	{"LFE", "LFE", "lfe"},
	{"Top Center", "TC", "top-center"},
	{"Top Front Left", "TFL", "top-front-left"},
	{"Top Front Right", "TFR", "top-front-right"},
	{"Top Front Center", "TFC", "top-front-center"},
	{"Top Back Left", "TBL", "top-rear-left"},
	{"Top Back Right", "TBR", "top-rear-right"},
	{"Top Back Center", "TBC", "top-rear-center"},
	{"Front Left Wide", NULL, NULL},
	{"Front Right Wide", NULL, NULL},
	{"LFE 2", NULL, NULL},
	{"Stereo Downmix Left", "Headphones Left", NULL},
	{"Stereo Downmix Right", "Headphones Right", NULL},

	//Not in enunumeration
	{"Back Left Center", NULL, NULL},
	{"Back Right Center", NULL, NULL},
	{"Front Left High", NULL, NULL},
	{"Front Center High", NULL, NULL},
	{"Front Right High", NULL, NULL},
	{"Top Front Left Center", NULL, NULL},
	{"Top Front Right Center", NULL, NULL},
	{"Top Side Left", NULL, NULL},
	{"Top Side Right", NULL, NULL},
	{"Left LFE", NULL, NULL},
	{"Right LFE", NULL, NULL},

	{"Bottom Center", NULL, NULL},
	{"Bottom Left Center", NULL, NULL},
	{"Bottom Right Center", NULL, NULL},
	{"Mid/Side Mid", NULL, NULL},
	{"Mid/Side Side", NULL, NULL},
	{"Ambisonic W", NULL, NULL},
	{"Ambisonic X", NULL, NULL},
	{"Ambisonic Y", NULL, NULL},
	{"Ambisonic Z", NULL, NULL},
	{"X-Y X", NULL, NULL},
	{"X-Y Y", NULL, NULL},
	{"Click Track", NULL, NULL},
	{"Foreign Language", NULL, NULL},
	{"Hearing Impaired", NULL, NULL},
	{"Narration", NULL, NULL},
	{"Haptic", NULL, NULL},
	{"Dialog Centric Mix", NULL, NULL},
	{"Aux", NULL, NULL},
	{"Aux 0", NULL, NULL},
	{"Aux 1", NULL, NULL},
	{"Aux 2", NULL, NULL},
	{"Aux 3", NULL, NULL},
	{"Aux 4", NULL, NULL},
	{"Aux 5", NULL, NULL},
	{"Aux 6", NULL, NULL},
	{"Aux 7", NULL, NULL},
	{"Aux 8", NULL, NULL},
	{"Aux 9", NULL, NULL},
	{"Aux 10", NULL, NULL},
	{"Aux 11", NULL, NULL},
	{"Aux 12", NULL, NULL},
	{"Aux 13", NULL, NULL},
	{"Aux 14", NULL, NULL},
	{"Aux 15", NULL, NULL},
};

const channel_layout* stream_desc::audio_info::GetDefaultLayoutFromCount(int ch_count)
{
	switch (ch_count) {
		case 0:
			return GetBuiltinLayoutFromType(CH_LAYOUT_NONE);
		case 1:
			return GetBuiltinLayoutFromType(CH_LAYOUT_MONO);
		case 2:
			return GetBuiltinLayoutFromType(CH_LAYOUT_STEREO);
		case 3:
			return GetBuiltinLayoutFromType(CH_LAYOUT_SURROUND);
		case 4:
			return GetBuiltinLayoutFromType(CH_LAYOUT_4POINT0);
		case 5:
			return GetBuiltinLayoutFromType(CH_LAYOUT_5POINT0_BACK);
		case 6:
			return GetBuiltinLayoutFromType(CH_LAYOUT_5POINT1_BACK);
		case 7:
			return GetBuiltinLayoutFromType(CH_LAYOUT_6POINT1);
		case 8:
			return GetBuiltinLayoutFromType(CH_LAYOUT_7POINT1);
		case 16:
			return GetBuiltinLayoutFromType(CH_LAYOUT_HEXADECAGONAL);
		default:
			return nullptr;
	}
}

const channel_layout* stream_desc::audio_info::GetBuiltinLayoutFromType(channel_layout_type type)
{
	return &builtin_channel_layouts[type];
}

const char* stream_desc::audio_info::GetChannelName(channel_id id)
{
	return channel_names[id][0];
}

const char* stream_desc::audio_info::GetBuiltinLayoutDefaultName(channel_layout_type type)
{
	return builtin_channel_layouts[type].name;
}
