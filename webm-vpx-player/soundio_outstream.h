#pragma once

#include "soundio_service.h"
#include "media_sink.h"

#include <chrono>
using std::chrono::time_point;
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::duration;

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
	soundio_outstream(stream_desc* upstream, soundio_device& dev):audio_outstream(upstream), device(dev)
	{
		desc_in = upstream;
		//verify protocol here.
		assert(upstream->type == stream_desc::MTYPE_AUDIO);
		assert(upstream->detail.audio.codec = stream_desc::audio_info::ACODEC_PCM);
		handle = soundio_outstream_create(dev);
		handle->userdata = this;
		handle->error_callback = error_callback;
		handle->underflow_callback = underflow_callback;
		handle->write_callback = write_callback;
		handle->volume = 1.0;
		handle->name = desc_in->format_info.Name;
		handle->format = soundio_device::translate_to_soundio_format(upstream->detail.audio.format);
		//translate the layout. Adaptation of channels can be done
		//in derived classes's write callback later
		soundio_device::translate_to_soundio_layout(handle->layout, upstream->detail.audio.layout);
		upstream->downstream = this;
		handle->sample_rate = upstream->detail.audio.Hz;
		open_err = soundio_outstream_open(handle);
	}
	virtual ~soundio_outstream() override final
	{
		soundio_outstream_destroy(handle);
	}
	operator SoundIoOutStream* ()
	{
		return handle;
	}
	virtual int SetVolume(double volume) override final
	{
		return soundio_outstream_set_volume(handle, volume);
	}
	virtual double GetLatency() override final
	{
		return duration_cast<duration<double>>(deplete_time - high_resolution_clock::now()).count();
	}
	virtual int SetPause(bool pause) override final
	{
		return soundio_outstream_pause(handle, pause);
	}
	virtual int Start() override final
	{
		return soundio_outstream_start(handle);
	}
	virtual int Reset() override final
	{
		return soundio_outstream_clear_buffer(handle);
	}
	virtual int QueueBuffer(_buffer_desc& buffer) override final
	{
		return S_OK;
	}

	//user implement this
	virtual void write_frames(SoundIoChannelArea* areas, const channel_layout& channels, int frame_count) {}
private:
	int open_err = 0;
	SoundIoOutStream* handle;
	soundio_device device;
	std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
	std::chrono::time_point<std::chrono::high_resolution_clock> deplete_time;
	static void write_callback(struct SoundIoOutStream* stream, int frame_count_min, int frame_count_max)
	{
		if (!frame_count_max) return;
		double float_sample_rate = stream->sample_rate;
		double seconds_per_frame = 1.0 / float_sample_rate;
		struct SoundIoChannelArea* areas;
		const struct SoundIoChannelLayout* layout = &stream->layout;
		int err;
		soundio_outstream& ost = *(soundio_outstream*)stream->userdata;
		int frame_count = frame_count_min ? frame_count_min : frame_count_max;
	//	frame_count = 481;
		if ((err = soundio_outstream_begin_write(stream, &areas, &frame_count))) {
			fprintf(stderr, "unrecoverable stream error: %s\n", soundio_strerror(err));
			exit(1);
		}
		channel_layout mlayout;
		soundio_device::translate_from_soundio_layout(mlayout, stream->layout);
		ost.write_frames(areas, mlayout, frame_count);
		//render frames here.
		/*
		while (frame_left > 0 && !ost.state.s_state.in_queque.empty()) {
			AVFrame* avframe = *(ost.state.s_state.in_queque.front());
			pts = avframe->pts;
			nb_samples = avframe->nb_samples;
			rendered = true;
			reset = false;
			//Update current pts in state and notify
			//the presentation controll thread. Syncing video
			//to audio is simpler on the audio side with
			//no need for resampling on time change, etc.
			//Also get smoother audio when playing since
			//audio is a strict realtime constraint
			//while video is not. Dropping audio currently
			//fills the buffer with 0, but may employ a
			//swr_context to drop the time, and compensate.
			//Note: allocate the buffer before doing anything
			//		else, and use the direct swr function.
			//      Not using the direct conversion to write
			//      to the buffer here is becase while on
			//      most backends the buffer is packed,
			//      jack goes with planar, and alsa has both.
			//      Allowing to specify a stride on swr_convert
			//      may work around this, but will mean work
			//      on the ffmpeg side and a separate code path
			//      for preparing such data.
			int64_t cur_write = min(avframe->nb_samples - ost.state.s_state.samples_read, frame_left);
			for (int64_t frame = 0; frame < cur_write; frame += 1) {
				for (int channel = 0; channel < layout->channel_count; channel += 1) {
					if (avframe->data[channel]) {
						memcpy(areas[channel].ptr, avframe->data[channel] + size_t(4) * (frame + ost.state.s_state.samples_read), 4);
						areas[channel].ptr += areas[channel].step;
					}
				}
			}
			if (avframe->nb_samples - ost.state.s_state.samples_read == cur_write) {
				cur_samples = (ost.state.s_state.samples_read);
				ost.state.s_state.samples_read = 0;
				ost.state.s_state.recycle_queque.push(avframe);
				ost.state.s_state.in_queque.pop();
				++num_recieved;
				reset = true;
				ost.state.s_state.in_queue_cond.notify_one();
			}
			else {
				cur_samples = (ost.state.s_state.samples_read + cur_write);
				ost.state.s_state.samples_read += frame_left;
			}
			frame_left -= cur_write;
		}
		if (frame_left) {
			if (ost.state.s_state.audio_ready) {
				fprintf(stderr, "No audio frame recieved!\n");
			}
			for (int frame = 0; frame < frame_left; frame += 1) {
				for (int channel = 0; channel < layout->channel_count; channel += 1) {
					memset(areas[channel].ptr, 0, 4);
					areas[channel].ptr += areas[channel].step;
				}
			}
		}*/
		if ((err = soundio_outstream_end_write(stream))) {
			if (err == SoundIoErrorUnderflow)
				return;
			fprintf(stderr, "unrecoverable stream error: %s\n", soundio_strerror(err));
			exit(1);
		}
		double latency;
		soundio_outstream_get_latency(ost.handle, &latency);
		ost.deplete_time = high_resolution_clock::now() + duration_cast<high_resolution_clock::duration>(duration<double>(latency));
		//update latency here
		/*
		if (rendered) {
			ost.state.s_state.prev_nb_samples = nb_samples;
			ost.state.s_state.prev_pts = pts;
			ost.state.s_state.cur_pts = pts + ((cur_samples * ost.state.s_state.time_base.den) / (float_sample_rate * ost.state.s_state.time_base.num)) - ((latency * ost.state.s_state.time_base.den) / ost.state.s_state.time_base.num);
			ost.state.s_state.time_since_epoch = timer::since_epoch() - latency * 1000000000;
		}
		soundio_outstream_pause(stream, want_pasue);
		ost.state.wait_audio_cond.notify_one();*/
	}
	static void underflow_callback(struct SoundIoOutStream*)
	{
		fprintf(stderr, "Soundio Outstream underflow.");
	}
	static void error_callback(struct SoundIoOutStream*, int err)
	{
		fprintf(stderr, "Soundio Outstream error %d: %s.", err, soundio_strerror(err));
	}
};
