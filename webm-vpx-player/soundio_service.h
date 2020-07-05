#pragma once

#include "audio_serice.h"
#include <soundio/soundio.h>
#include <mutex>
#include <atomic>
#include <cassert>

class soundio_device:public audio_device_ref {
public:
	soundio_device(SoundIoDevice* dev);
	~soundio_device() override final;
	soundio_device(soundio_device& dev);
	soundio_device& operator=(soundio_device& dev);
	operator SoundIoDevice* ();
	virtual bool FormatSupported(SampleFormat fmt) override final;
	virtual int NearestSupportedSampleRate(int rate) override final;
	virtual int GetSampleRateCount() override final;
	virtual int GetCurrentSampleRate() override final;
	virtual int GetSampleRateFromIndex(int index, sample_rate_range& range) override final;
	virtual int GetFormatCount() override final;
	virtual SampleFormat GetCurrentFormat() override final;
	virtual int GetFormatFromIndex(int index, SampleFormat& fmt) override final;
	virtual int GetLayoutCount() override final;
	virtual void GetCurrentLayout(channel_layout& layout) override final;
	virtual int GetLayoutFromIndex(int index, channel_layout& layout) override final;
	virtual aim GetDeviceAim() override final;
	virtual bool isRaw() override final;
	virtual double GetLatency() override final;
private:
	SoundIoDevice* handle = nullptr;
protected:
	friend soundio_outstream;
	static SampleFormat translate_from_soundio_format(SoundIoFormat fmt);
	static SoundIoFormat translate_to_soundio_format(SampleFormat fmt);
	static channel_id translate_from_soundio_channel(SoundIoChannelId channel);
	static SoundIoChannelId translate_to_soundio_channel(channel_id channel);
	static void translate_from_soundio_layout(channel_layout& layout, SoundIoChannelLayout& from);
	static void translate_to_soundio_layout(SoundIoChannelLayout& layout, channel_layout& from);
};

typedef void (*SignalEventCallback)(backend_type backend);

template<backend_type backend>
class soundio_service {
public:
	soundio_service();
	~soundio_service();
	soundio_service(const soundio_service& other);
	soundio_service& operator=(const soundio_service& other)
	{
		//lifetime of the copied object makes sure
		//that this object will not interfere with
		//destruction
		if (inited.load(std::memory_order_relaxed))
			refs.fetch_add(1, std::memory_order_relaxed);
	}
private:
	static std::mutex init_mutex;
	static std::atomic_size_t refs;
	static std::atomic_bool inited;
protected:
	static SoundIo* handle;
public:
	static void FlushEvents();
	static void WaitEvents();
	static void Wakeup();
	static int GetInputDeviceCount();
	static int GetOutputDeviceCount();
	static int DefaultInput();
	static int DefaultOutput();
	static soundio_device GetInputDeviceFromIndex(int index);
	static soundio_device GetOutputDeviceFromIndex(int index);
	static SignalEventCallback SetEventSignal(SignalEventCallback);
private:
	static void on_devices_change(struct SoundIo*)noexcept;
	static void on_backend_disconnect(struct SoundIo*, int err) noexcept;
	static void emit_rtprio_warning() noexcept;
	static void jack_info_callback(const char* msg) noexcept;
	static void jack_error_callback(const char* msg) noexcept;
	static void on_events_signal(struct SoundIo*) noexcept;
	static volatile SignalEventCallback current_callback;
};

#include "soundio_service.ipp"
