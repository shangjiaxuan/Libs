#include "soundio_outstream.h"

#include <cmath>
#include <algorithm>

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
	assert(desc_in->mode = stream_desc::MODE_REACTIVE);
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
	frame_num = (handle->software_latency*handle->sample_rate);
	if (upstream->detail.audio.planar) {
		for (int i = 0; i < upstream->detail.audio.layout.channel_count; ++i) {
			buffer[i]=(uint8_t*)malloc((frame_num)* sizeof(float));
		}
	}
	else {
		buffer[0] = (uint8_t*)malloc((frame_num) * sizeof(float) * upstream->detail.audio.layout.channel_count);
	}
}

soundio_outstream::~soundio_outstream()
{
	soundio_outstream_destroy(handle);
	if (desc_in->detail.audio.planar) {
		for (int i = 0; i < desc_in->detail.audio.layout.channel_count; ++i) {
			free(buffer[i]);
		}
	}
	else {
		free(buffer[0]);
	}
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
	/*if (!queue.try_emplace(buffer)) {
		return E_AGAIN;
	}
	return S_OK;*/
	return E_INVALID_OPERATION;
}

int cmp(void const* ptr1, void const* ptr2)
{
	return ptr1<ptr2;
}

bool soundio_outstream::need_convert(SoundIoChannelArea* areas, SampleFormat in_fmt, SampleFormat out_fmt, const channel_layout& in_channels, const channel_layout& out_channels, bool in_planar, int sample_num)
{
	return false;
	//simple check of channel types
	if(in_channels.channel_count!=out_channels.channel_count)
		return false;
	if(!memcmp(&in_fmt,&out_fmt, sizeof(SampleFormat)))
		return false;
	if (in_planar) {
		//CHECK IF OUTPUT IS VALID PLANAR
		//MAY ALSO CHECK THE LAYOUT ORDER
		//PERHAPS REORDER THE AREAS
		if (areas->step == (in_fmt.bitdepth / 8)) {
			bool valid = false;
			uint8_t* ids[max_channels];
			//sort the output in order and see if overllaped
			for (int i = 0; i < out_channels.channel_count; ++i) {
				ids[i] = (uint8_t*)areas[i].ptr;
			}
			qsort(ids, out_channels.channel_count,sizeof(uint8_t*), cmp);
			if (ids[1] - ids[0] < sample_num * soundio_get_bytes_per_sample(soundio_device::translate_to_soundio_format(in_fmt))) {
				return false;
			}
			//check channel order here
			return false;
			return true;
		}
	}
	else {
		//my format do not have defined size yet
		int stride = areas[0].step;
		uint8_t* ptr[max_channels]{};
		for (int i = 0; i < out_channels.channel_count; ++i) {
			if(areas[i].step != stride)		return false;
			ptr[i]=(uint8_t*)areas[i].ptr;
		}
		qsort(ptr, out_channels.channel_count, sizeof(uint8_t*), cmp);
		for (int i = 0; i < out_channels.channel_count - 1; ++i) {
			if (ptr[i+1] - ptr[i] != (in_fmt.bitdepth / 8))	return false;
		}
		//check channel order here
		return false;
		return true;
	}
}

//user implement this
void soundio_outstream::write_frames(SoundIoChannelArea* areas, const channel_layout& channels, int frame_count) {
	uint8_t* src[max_channels]{};
	uint64_t zeros = 0;
	int src_stride = 0;
	int frames_left = frame_count;
	_buffer_desc fetching{};
	if (desc_in->detail.audio.planar) {
		for (int i = 0; i < channels.channel_count; ++i) {
			fetching.detail.aframe.channels[i] = buffer[i];
			src[i] = (uint8_t*)buffer[i];
		}
		src_stride = sameple_size;
	}
	else {
		fetching.detail.aframe.channels[0] = buffer[0];
		for (int i = 0; i < channels.channel_count; ++i) {
			src[i] = (uint8_t*)(buffer[0] + i * sameple_size);
		}
		src_stride = sameple_size * channels.channel_count;
	}
	while (frames_left) {
		int this_round;
		bool need_pop = false;
		fetching.detail.aframe.nb_samples = frames_left;
		fetching.detail.aframe.sample_rate = desc_in->detail.audio.Hz;
		desc_in->upstream->FetchBuffer(fetching);
		this_round = fetching.detail.aframe.nb_samples;
		for (int i = 0; i < channels.channel_count; ++i) {
			for (int j = 0; j < this_round; ++j) {
				write_sample_with_fmt_convert(areas[i].ptr, src[i], sameple_size);
				areas[i].ptr += areas[i].step;
				src[i] += src_stride;
			}
		}
		frames_left -= this_round;
	}

//	printf("%f\n",GetLatency());
}

void soundio_outstream::write_callback(SoundIoOutStream* stream, int frame_count_min, int frame_count_max)
{
	auto release_start = high_resolution_clock::now();
	if (!frame_count_max) return;
	double float_sample_rate = stream->sample_rate;
	double seconds_per_frame = 1.0 / float_sample_rate;
	struct SoundIoChannelArea* areas;
	const struct SoundIoChannelLayout* layout = &stream->layout;
	int err;
	soundio_outstream& ost = *(soundio_outstream*)stream->userdata;
	int frame_count = frame_count_min ? frame_count_min : frame_count_max;
//		frame_count = (frame_count+1>frame_count_max)?frame_count:frame_count+1;
	frame_count = frame_count_max;
	channel_layout mlayout;
	soundio_device::translate_from_soundio_layout(mlayout, stream->layout);
	if ((err = soundio_outstream_begin_write(stream, &areas, &frame_count))) {
		fprintf(stderr, "unrecoverable stream error: %s\n", soundio_strerror(err));
		exit(1);
	}
	ost.write_frames(areas, mlayout, frame_count);
	if ((err = soundio_outstream_end_write(stream))) {
		if (err == SoundIoErrorUnderflow)
			return;
		fprintf(stderr, "unrecoverable stream error: %s\n", soundio_strerror(err));
		exit(1);
	}
	double latency;
	soundio_outstream_get_latency(ost.handle, &latency);
	ost.deplete_time = high_resolution_clock::now() + duration_cast<high_resolution_clock::duration>(duration<double>(latency));
	ost.cur_frame+=frame_count;
}

void soundio_outstream::underflow_callback(SoundIoOutStream*)
{
	//notify to buffer more
	fprintf(stderr, "Soundio Outstream underflow.");
}

void soundio_outstream::error_callback(SoundIoOutStream*, int err)
{
	fprintf(stderr, "Soundio Outstream error %d: %s.", err, soundio_strerror(err));
}
