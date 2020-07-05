#pragma once

#include "audio_info.h"

template<backend_type backend = BACKEND_DEFAULT>
class soundio_service;
class soundio_outstream;

class audio_device_ref {
public:
	enum aim {
		AIM_NONE,
		AIM_OUT,
		AIM_IN,
		AIM_INOUT
	};
	audio_device_ref() {};
	virtual ~audio_device_ref(){};
	virtual bool FormatSupported(SampleFormat fmt) = 0;
	virtual int NearestSupportedSampleRate(int rate) = 0;
	virtual int GetSampleRateCount() = 0;
	virtual int GetCurrentSampleRate() = 0;
	virtual int GetSampleRateFromIndex(int index, sample_rate_range& range) = 0;
	virtual int GetFormatCount() = 0;
	virtual SampleFormat GetCurrentFormat() = 0;
	virtual int GetFormatFromIndex(int index, SampleFormat& fmt) = 0;
	virtual int GetLayoutCount() = 0;
	virtual void GetCurrentLayout(channel_layout& layout) = 0;
	virtual int GetLayoutFromIndex(int index, channel_layout& layout) = 0;
	virtual aim GetDeviceAim() = 0;
	virtual bool isRaw() = 0;
	virtual double GetLatency() = 0;
};


