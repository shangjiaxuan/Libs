#include "soundio_service.h"


#include <mutex>
#include <cassert>
#include <atomic>
#include <chrono>



#include "media_sink.h"

soundio_device::soundio_device(SoundIoDevice* dev):handle(dev)
{
	if (dev) {
		soundio_device_ref(dev);
		handle = dev;
	}
}

soundio_device::~soundio_device()
{
	soundio_device_unref(handle);
}

soundio_device::soundio_device(soundio_device& dev)
{
	if (dev.handle) {
		soundio_device_ref(dev);
		handle = dev.handle;
	}
}

soundio_device& soundio_device::operator=(soundio_device & dev)
{
	if (dev.handle) {
		soundio_device_ref(dev);
		handle = dev.handle;
	}
	return *this;
}

soundio_device::operator SoundIoDevice* ()
{
	return handle;
}

bool soundio_device::FormatSupported(SampleFormat fmt)
{
	return soundio_device_supports_format(handle, translate_to_soundio_format(fmt));
}

int soundio_device::NearestSupportedSampleRate(int rate)
{
	return soundio_device_nearest_sample_rate(handle, rate);
}

int soundio_device::GetSampleRateCount()
{
	return handle->sample_rate_count;
}

int soundio_device::GetCurrentSampleRate()
{
	return handle->sample_rate_current;
}

int soundio_device::GetSampleRateFromIndex(int index, sample_rate_range& range)
{
	if (index >= handle->sample_rate_count)
		return E_INVALID_OPERATION;
	range.min = handle->sample_rates[index].min;
	range.max = handle->sample_rates[index].max;
	return S_OK;
}

int soundio_device::GetFormatCount()
{
	return handle->format_count;
}

SampleFormat soundio_device::GetCurrentFormat()
{
	return translate_from_soundio_format(handle->current_format);
}

int soundio_device::GetFormatFromIndex(int index, SampleFormat& fmt)
{
	if (index >= handle->format_count)
		return E_INVALID_OPERATION;
	fmt = translate_from_soundio_format(handle->formats[index]);
	return S_OK;
}

int soundio_device::GetLayoutCount()
{
	return handle->layout_count;
}

void soundio_device::GetCurrentLayout(channel_layout& layout)
{
	translate_from_soundio_layout(layout, handle->current_layout);
}

int soundio_device::GetLayoutFromIndex(int index, channel_layout& layout)
{
	if (index >= handle->layout_count)
		return E_INVALID_OPERATION;
	translate_from_soundio_layout(layout, handle->layouts[index]);
	return S_OK;
}


audio_device_ref::aim soundio_device::GetDeviceAim()
{
	switch (handle->aim) {
		case SoundIoDeviceAimOutput:
			return AIM_OUT;
		case SoundIoDeviceAimInput:
			return AIM_IN;
		default:
			return  AIM_NONE;
	}
}

bool soundio_device::isRaw()
{
	return handle->is_raw;
}

double soundio_device::GetLatency()
{
	return handle->software_latency_current;
}

SampleFormat soundio_device::translate_from_soundio_format(SoundIoFormat fmt)
{
	SampleFormat native_fmt{};
	switch (fmt) {
		case SoundIoFormatInvalid:
			return native_fmt;
		case SoundIoFormatS8:
			native_fmt.bitdepth = 8;
			return native_fmt;
		case SoundIoFormatU8:
			native_fmt.bitdepth = 8;
			native_fmt.isunsigned = 1;
			return native_fmt;
		case SoundIoFormatS16LE:
			native_fmt.bitdepth = 16;
			return native_fmt;
		case SoundIoFormatS16BE:
			native_fmt.bitdepth = 16;
			native_fmt.isBE = 1;
			return native_fmt;
		case SoundIoFormatU16LE:
			native_fmt.bitdepth = 16;
			native_fmt.isunsigned = 1;
			return native_fmt;
		case SoundIoFormatU16BE:
			native_fmt.bitdepth = 16;
			native_fmt.isunsigned = 1;
			native_fmt.isBE = 1;
			return native_fmt;
		case SoundIoFormatS24LE:
			native_fmt.bitdepth = 24;
			return native_fmt;
		case SoundIoFormatS24BE:
			native_fmt.bitdepth = 24;
			native_fmt.isBE = 1;
			return native_fmt;
		case SoundIoFormatU24LE:
			native_fmt.bitdepth = 24;
			native_fmt.isunsigned = 1;
			return native_fmt;
		case SoundIoFormatU24BE:
			native_fmt.bitdepth = 24;
			native_fmt.isunsigned = 1;
			native_fmt.isBE = 1;
			return native_fmt;
		case SoundIoFormatS32LE:
			native_fmt.bitdepth = 32;
			return native_fmt;
		case SoundIoFormatS32BE:
			native_fmt.bitdepth = 32;
			native_fmt.isBE = 1;
			return native_fmt;
		case SoundIoFormatU32LE:
			native_fmt.bitdepth = 32;
			native_fmt.isunsigned = 1;
			return native_fmt;
		case SoundIoFormatU32BE:
			native_fmt.bitdepth = 32;
			native_fmt.isunsigned = 1;
			native_fmt.isBE = 1;
			return native_fmt;
		case SoundIoFormatFloat32LE:
			native_fmt.bitdepth = 32;
			native_fmt.isfloat = 1;
			return native_fmt;
		case SoundIoFormatFloat32BE:
			native_fmt.bitdepth = 32;
			native_fmt.isfloat = 1;
			native_fmt.isBE = 1;
			return native_fmt;
		case SoundIoFormatFloat64LE:
			native_fmt.bitdepth = 64;
			native_fmt.isfloat = 1;
			return native_fmt;
		case SoundIoFormatFloat64BE:
			native_fmt.bitdepth = 64;
			native_fmt.isfloat = 1;
			native_fmt.isBE = 1;
			return native_fmt;
		default:
			return native_fmt;
	}
}

SoundIoFormat soundio_device::translate_to_soundio_format(SampleFormat fmt)
{
	if (fmt.isfloat) {
		switch (fmt.bitdepth) {
			case 32:
				if (fmt.isBE)
					return SoundIoFormatFloat32BE;
				else
					return SoundIoFormatFloat32LE;
			case 64:
				if (fmt.isBE)
					return SoundIoFormatFloat64BE;
				else
					return SoundIoFormatFloat64LE;
			default:
				return SoundIoFormatInvalid;
		}
	}
	else {
		if (fmt.isunsigned) {
			switch (fmt.bitdepth) {
				case 8:
					return SoundIoFormatU8;
				case 16:
					if (fmt.isBE)
						return SoundIoFormatU16BE;
					else
						return SoundIoFormatU16LE;
				case 24:
					if (fmt.isBE)
						return SoundIoFormatU24BE;
					else
						return SoundIoFormatU24LE;
				case 32:
					if (fmt.isBE)
						return SoundIoFormatU32BE;
					else
						return SoundIoFormatU32LE;
				case 64:
					return SoundIoFormatInvalid;
				default:
					return SoundIoFormatInvalid;
			}
		}
		else {
			switch (fmt.bitdepth) {
				case 8:
					return SoundIoFormatS8;
				case 16:
					if (fmt.isBE)
						return SoundIoFormatS16BE;
					else
						return SoundIoFormatS16LE;
				case 24:
					if (fmt.isBE)
						return SoundIoFormatS24BE;
					else
						return SoundIoFormatS24LE;
				case 32:
					if (fmt.isBE)
						return SoundIoFormatS32BE;
					else
						return SoundIoFormatS32LE;
				case 64:
					return SoundIoFormatInvalid;
				default:
					return SoundIoFormatInvalid;
			}
		}
	}
}

channel_id soundio_device::translate_from_soundio_channel(SoundIoChannelId channel)
{
	switch (channel) {
		case SoundIoChannelIdInvalid:
			return CH_NONE;
		case SoundIoChannelIdFrontLeft:
			return CH_FRONT_LEFT;
		case SoundIoChannelIdFrontRight:
			return CH_FRONT_RIGHT;
		case SoundIoChannelIdFrontCenter:
			return CH_FRONT_CENTER;
		case SoundIoChannelIdFrontLeftCenter:
			return CH_FRONT_LEFT_OF_CENTER;
		case SoundIoChannelIdFrontRightCenter:
			return CH_FRONT_RIGHT_OF_CENTER;
		case SoundIoChannelIdBackLeft:
			return CH_BACK_LEFT;
		case SoundIoChannelIdBackRight:
			return CH_BACK_RIGHT;
		case SoundIoChannelIdBackCenter:
			return CH_BACK_CENTER;
		case SoundIoChannelIdSideLeft:
			return CH_SIDE_LEFT;
		case SoundIoChannelIdSideRight:
			return CH_SIDE_RIGHT;
		case SoundIoChannelIdTopCenter:
			return CH_TOP_CENTER;
		case SoundIoChannelIdTopFrontLeft:
			return CH_TOP_FRONT_LEFT;
		case SoundIoChannelIdTopFrontRight:
			return CH_TOP_FRONT_RIGHT;
		case SoundIoChannelIdTopFrontCenter:
			return CH_TOP_FRONT_CENTER;
		case SoundIoChannelIdTopBackLeft:
			return CH_TOP_BACK_LEFT;
		case SoundIoChannelIdTopBackRight:
			return CH_TOP_BACK_RIGHT;
		case SoundIoChannelIdTopBackCenter:
			return CH_TOP_BACK_CENTER;
		case SoundIoChannelIdFrontLeftWide:
			return CH_WIDE_LEFT;
		case SoundIoChannelIdFrontRightWide:
			return CH_WIDE_RIGHT;
		case SoundIoChannelIdLfe2:
			return CH_LOW_FREQUENCY_2;
		case SoundIoChannelIdHeadphonesLeft:
			return CH_STEREO_LEFT;
		case SoundIoChannelIdHeadphonesRight:
			return CH_STEREO_RIGHT;
		default:
			return CH_NONE;
	}
}

SoundIoChannelId soundio_device::translate_to_soundio_channel(channel_id channel)
{
	switch (channel) {
		case CH_NONE:
			return SoundIoChannelIdInvalid;
		case CH_FRONT_LEFT:
			return SoundIoChannelIdFrontLeft;
		case CH_FRONT_RIGHT:
			return SoundIoChannelIdFrontRight;
		case CH_FRONT_CENTER:
			return SoundIoChannelIdFrontCenter;
		case CH_FRONT_LEFT_OF_CENTER:
			return SoundIoChannelIdFrontLeftCenter;
		case CH_FRONT_RIGHT_OF_CENTER:
			return SoundIoChannelIdFrontRightCenter;
		case CH_BACK_LEFT:
			return SoundIoChannelIdBackLeft;
		case CH_BACK_RIGHT:
			return SoundIoChannelIdBackRight;
		case CH_BACK_CENTER:
			return SoundIoChannelIdBackCenter;
		case CH_SIDE_LEFT:
			return SoundIoChannelIdSideLeft;
		case CH_SIDE_RIGHT:
			return SoundIoChannelIdSideRight;
		case CH_TOP_CENTER:
			return SoundIoChannelIdTopCenter;
		case CH_TOP_FRONT_LEFT:
			return SoundIoChannelIdTopFrontLeft;
		case CH_TOP_FRONT_RIGHT:
			return SoundIoChannelIdTopFrontRight;
		case CH_TOP_FRONT_CENTER:
			return SoundIoChannelIdTopFrontCenter;
		case CH_TOP_BACK_LEFT:
			return SoundIoChannelIdTopBackLeft;
		case CH_TOP_BACK_RIGHT:
			return SoundIoChannelIdTopBackRight;
		case CH_TOP_BACK_CENTER:
			return SoundIoChannelIdTopBackCenter;
		case CH_WIDE_LEFT:
			return SoundIoChannelIdFrontLeftWide;
		case CH_WIDE_RIGHT:
			return SoundIoChannelIdFrontRightWide;
		case CH_LOW_FREQUENCY_2:
			return SoundIoChannelIdLfe2;
		case CH_STEREO_LEFT:
			return SoundIoChannelIdHeadphonesLeft;
		case CH_STEREO_RIGHT:
			return SoundIoChannelIdHeadphonesRight;
		default:
			return SoundIoChannelIdInvalid;
	}
}

void soundio_device::translate_from_soundio_layout(channel_layout& layout, SoundIoChannelLayout& from)
{
	layout.channel_count = from.channel_count;
	layout.name = from.name;
	for (int i = 0; i < layout.channel_count; ++i) {
		layout.channels[i] = translate_from_soundio_channel(from.channels[i]);
	}
}

void soundio_device::translate_to_soundio_layout(SoundIoChannelLayout& layout, channel_layout& from)
{
	layout.channel_count = from.channel_count;
	layout.name = from.name;
	for (int i = 0; i < layout.channel_count; ++i) {
		layout.channels[i] = translate_to_soundio_channel(from.channels[i]);
	}
}

template<>
std::atomic_size_t soundio_service<>::refs = 0;
template<>
std::atomic_bool soundio_service<>::inited = false;
template<>
volatile SignalEventCallback soundio_service<>::current_callback = nullptr;
template<>
std::mutex soundio_service<>::init_mutex{};
template<>
SoundIo* soundio_service<>::handle = nullptr;



