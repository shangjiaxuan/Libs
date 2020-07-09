#pragma once

#include "soundio_service.h"
#include "media_sink.h"

#include <chrono>
#include <rigtorp/SPSCQueue.h>

class audio_outstream:public media_sink {
public:
	audio_outstream(stream_desc* upstream)
	{
		assert(upstream);
		assert(upstream->type == stream_desc::MTYPE_AUDIO);
		assert(upstream->detail.audio.codec = stream_desc::audio_info::ACODEC_PCM);
		desc_in = upstream;
		upstream->downstream = this;
	}
	virtual ~audio_outstream() {}
	virtual int SetVolume(double volume) = 0;
	virtual double GetLatency() = 0;
	virtual int SetPause(bool pause) = 0;
	virtual int Start() = 0;
	virtual int Reset() = 0;
	virtual void write_frames(SoundIoChannelArea* areas, const channel_layout& channels, int frame_count) = 0;
	virtual int AllocBuffer(_buffer_desc& buffer) override final
	{
		return E_INVALID_OPERATION;
	}
	virtual int Flush() override final
	{
		return E_INVALID_OPERATION;
	}
	virtual int GetInputs(stream_desc*& desc, size_t& num) override final
	{
		desc = desc_in;
		num = 1;
		return S_OK;
	}
};

class soundio_outstream: public audio_outstream {
public:
	soundio_outstream(stream_desc* upstream, soundio_device& dev);
	virtual ~soundio_outstream() override final;
	operator SoundIoOutStream* ();
	virtual int SetVolume(double volume) override final;
	virtual double GetLatency() override final;
	virtual int SetPause(bool pause) override final;
	virtual int Start() override final;
	virtual int Reset() override final;
	virtual int QueueBuffer(_buffer_desc& buffer) override final;

	//user implement this
	virtual void write_frames(SoundIoChannelArea* areas, const channel_layout& channels, int frame_count);
private:
	int open_err = 0;
	SoundIoOutStream* handle;
	soundio_device device;
	std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
	std::chrono::time_point<std::chrono::high_resolution_clock> deplete_time;
	static void write_callback(struct SoundIoOutStream* stream, int frame_count_min, int frame_count_max);
	static void underflow_callback(struct SoundIoOutStream*);
	static void error_callback(struct SoundIoOutStream*, int err);
	void (*write_sample_with_fmt_convert)(void* dst, const void* src, size_t size) noexcept = nullptr;
	size_t sameple_size = 0;
	//TODO: match this with ouput dynamically
	uint8_t* buffer[max_channels]{};
	size_t frame_num = 0;
	size_t cur_frame = 0;
	bool need_convert_now = true;
	static bool need_convert(SoundIoChannelArea* areas, SampleFormat in_fmt, SampleFormat out_fmt, const channel_layout& in_channels, const channel_layout& out_channels, bool in_planar, int sample_num);
};
