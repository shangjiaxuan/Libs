#pragma once

#include <cstdint>

struct rational {
	uint32_t num;
	uint32_t den;
};

enum error_type {
	//OK = 0
	S_OK,
	//Signals end of stream, must be implemented
	E_EOF,
	//Signals need of more input, passed downstream
	E_AGAIN,
	//Not yet implemented
	E_UNIMPLEMENTED,
	//Sanity check for invalid usage
	E_INVALID_OPERATION,
	//Sanity check for invalid topology building
	E_PROTOCOL_MISMATCH
};
struct SampleFormat {
	int8_t bitdepth;
	uint8_t isfloat : 1;
	uint8_t isunsigned : 1;
	uint8_t isBE : 1;
	SampleFormat():bitdepth(0),isfloat(0),isunsigned(0),isBE(0){}
};

enum channel_id: uint8_t {
	CH_NONE,
	CH_FRONT_LEFT,
	CH_FRONT_RIGHT,
	CH_FRONT_CENTER,
	CH_FRONT_LEFT_OF_CENTER,
	CH_FRONT_RIGHT_OF_CENTER,
	CH_BACK_LEFT,
	CH_BACK_RIGHT,
	CH_BACK_CENTER,
	CH_SIDE_LEFT,
	CH_SIDE_RIGHT,
	CH_LOW_FREQUENCY,
	CH_TOP_CENTER,
	CH_TOP_FRONT_LEFT,
	CH_TOP_FRONT_RIGHT,
	CH_TOP_FRONT_CENTER,
	CH_TOP_BACK_LEFT,
	CH_TOP_BACK_RIGHT,
	CH_TOP_BACK_CENTER,
	CH_WIDE_LEFT,
	CH_WIDE_RIGHT,
	CH_LOW_FREQUENCY_2,
	//stereo downmix
	CH_STEREO_LEFT,
	//stereo downmix
	CH_STEREO_RIGHT,
};

static constexpr int max_channels = 24;

struct channel_layout {
	const char* name;
	int channel_count;
	enum channel_id channels[max_channels];
};

struct sample_rate_range {
	int min;
	int max;
};

enum backend_type {
	BACKEND_DEFAULT,
	BACKEND_DUMMY,
	//Windows api
	BACKEND_ASIO,
	BACKEND_WASAPI,
	BACKEND_DSOUND,
	//Linux api
	BACKEND_ALSA,
	BACKEND_JACK,
	BACKEND_PULSE,
	//Some BSD release api
	BACKEND_OSS,
	//MacOS api
	BACKEND_COREAUDIO,
	//Perhaps needed somewhere
	BACKEND_OPENAL,
	BACKEND_OPENSL
};

