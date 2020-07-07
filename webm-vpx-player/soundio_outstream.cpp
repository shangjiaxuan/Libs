#include "soundio_outstream.h"

using std::chrono::time_point;
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::duration;

inline static void write_same_fmt(void* dst, const void* src, size_t size) noexcept
{
	memcpy(dst, src, size);
}

inline static void write_float_from_s16(void* dst, const void* src, size_t size) noexcept
{
	const int16_t& _src = *(int16_t*)src;
	float& _dst = *(float*)src;
	_dst = (_src + 0.5f) / (0x7fff + 0.5f);
}

inline static void write_u16_from_float(void* dst, const void* src, size_t size) noexcept
{
	const float& _src = *(float*)src;
	int16_t& _dst = *(int16_t*)src;
	_dst = (_src + 1.0f) * (0x7fff + 0.5f);
}

inline static void write_double_from_s16(void* dst, const void* src, size_t size) noexcept
{
	const int16_t& _src = *(int16_t*)src;
	double& _dst = *(double*)src;
	_dst = (_src + 0.5) / (0x7fff + 0.5);
}

inline static void write_u16_from_double(void* dst, const void* src) noexcept
{
	const double& _src = *(double*)src;
	int16_t& _dst = *(int16_t*)src;
	_dst = (_src + 1.0) * (0x7fff + 0.5);
}

soundio_outstream::soundio_outstream(stream_desc* upstream, soundio_device& dev):audio_outstream(upstream), device(dev)
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
	//assume same sample format for now
	sameple_size = upstream->detail.audio.format.bitdepth>>3;
	write_sample_with_fmt_convert = write_same_fmt;
}

soundio_outstream::~soundio_outstream()
{
	soundio_outstream_destroy(handle);
}

soundio_outstream::operator SoundIoOutStream* ()
{
	return handle;
}

int soundio_outstream::SetVolume(double volume)
{
	return soundio_outstream_set_volume(handle, volume);
}

double soundio_outstream::GetLatency()
{
	return duration_cast<duration<double>>(deplete_time - high_resolution_clock::now()).count();
}

int soundio_outstream::SetPause(bool pause)
{
	return soundio_outstream_pause(handle, pause);
}

int soundio_outstream::Start()
{
	return soundio_outstream_start(handle);
}

int soundio_outstream::Reset()
{
	return soundio_outstream_clear_buffer(handle);
}

int soundio_outstream::QueueBuffer(_buffer_desc& buffer)
{
	if (!queue.try_emplace(buffer)) {
		return E_AGAIN;
	}
	return S_OK;
}

//user implement this
void soundio_outstream::write_frames(SoundIoChannelArea* areas, const channel_layout& channels, int frame_count) {
	uint8_t* src[max_channels]{};
	uint64_t zeros = 0;
	int src_stride = 0;
	int frames_left = frame_count;
	while (frames_left) {
		int this_round;
		bool need_pop = false;
		if (!queue.front()) {
			for (int i = 0; i < channels.channel_count; ++i) {
				src[i] = (uint8_t*)&zeros;
			}
			this_round = frames_left;
			src_stride = 0;
		}
		else {
			_buffer_desc& buf = *queue.front();
			if (desc_in->detail.audio.planar) {
				src_stride = sameple_size;
				uint8_t** _src = (uint8_t**)buf.detail.audio_frame.channels;
				for (int i = 0; i < channels.channel_count; ++i) {
					src[i] = _src[i] + sameple_size * frames_into_front;
				}
				if (frames_left >= buf.detail.audio_frame.nb_samples - frames_into_front) {
					need_pop = true;
					this_round = buf.detail.audio_frame.nb_samples - frames_into_front;
				}
				else {
					this_round = frames_left;
				}
			}
			else {
				src_stride = sameple_size * channels.channel_count;
				for (int i = 0; i < channels.channel_count; ++i) {
					src[i] = ((uint8_t*)buf.detail.audio_frame.channels[0]) + sameple_size * i + sameple_size * frames_into_front * channels.channel_count;
				}
			}
		}
		for (int i = 0; i < channels.channel_count; ++i) {
			for (int i = 0; i < this_round; ++i) {
				write_sample_with_fmt_convert(areas[i].ptr, src[i], sameple_size);
				areas[i].ptr += areas[i].step;
				src[i] += src_stride;
			}
		}
		if (need_pop) {
			frames_into_front = 0;
			queue.pop();
		}
		else {
			frames_into_front += this_round;
		}
	}

}

void soundio_outstream::write_callback(SoundIoOutStream* stream, int frame_count_min, int frame_count_max)
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

void soundio_outstream::underflow_callback(SoundIoOutStream*)
{
	fprintf(stderr, "Soundio Outstream underflow.");
}

void soundio_outstream::error_callback(SoundIoOutStream*, int err)
{
	fprintf(stderr, "Soundio Outstream error %d: %s.", err, soundio_strerror(err));
}
