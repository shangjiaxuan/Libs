#include "media_transform.h"

#include <opus/opus.h>
#include <cassert>
#include <rigtorp/SPSCQueue.h>

class opus_decoder: public audio_decoder {
	int err = 0;
	int chan;
	int32_t freq;
	OpusDecoder* handle;
	//for debugging without downstream
	float buffer[5760 * 2]{};
	int nb_samples_in_buffer = 0;
	int sample_offset_in_buffer = 0;
	stream_desc out_stream;
	uint64_t cur_frame = 0;
	//larger may be better, or perhaps a dynamically allocated queue
	rigtorp::SPSCQueue<_buffer_desc> in_queue{3};
	rigtorp::SPSCQueue<_buffer_desc> out_queue{10};
public:
	opus_decoder(stream_desc* upstream):
		audio_decoder(decoder_type::AD_OPUS_DIRECT), 
		err([upstream](){assert(upstream && upstream->type == stream_desc::MTYPE_AUDIO 
								&& upstream->detail.audio.codec == stream_desc::audio_info::ACODEC_OPUS);
								return 0; }()),
		chan(upstream->detail.audio.layout.channel_count == 1? 1: 2),
		freq((upstream->detail.audio.Hz== 48000||upstream->detail.audio.Hz == 24000||
			upstream->detail.audio.Hz == 16000||upstream->detail.audio.Hz == 12000||
			upstream->detail.audio.Hz == 8000)? upstream->detail.audio.Hz:48000),
		handle(opus_decoder_create(freq,chan,&err)){
		out_stream.type = stream_desc::MTYPE_AUDIO;
		out_stream.upstream = this;
		out_stream.format_info = upstream->format_info;
		out_stream.detail.audio = upstream->detail.audio;
		out_stream.detail.audio.Hz= freq;
		out_stream.detail.audio.codec = stream_desc::audio_info::ACODEC_PCM;
		out_stream.detail.audio.format.isfloat = 1;
		out_stream.detail.audio.format.isBE = 0;
		out_stream.detail.audio.format.isunsigned = 0;
		out_stream.detail.audio.format.bitdepth = 32;
		out_stream.time_base.num = 1;
		out_stream.time_base.den = freq;
		out_stream.mode = stream_desc::MODE_REACTIVE;
		desc_in = upstream;
		desc_out = &out_stream;
	}
	virtual ~opus_decoder() override final {
		if(handle)
			opus_decoder_destroy(handle);
	}
	virtual int QueueBuffer(_buffer_desc& in_buffer) override final
	{
		while (out_queue.front()) {
			_buffer_desc* to_pop = out_queue.front();
			to_pop->release(to_pop);
			out_queue.pop();
		}
		in_queue.push(in_buffer);
		in_buffer.detail.pkt.buffer = nullptr;
		in_buffer.release = nullptr;
	//	nb_samples = opus_decoder_get_nb_samples(handle, in_buffer.detail.pkt.buffer->buffer, in_buffer.detail.pkt.size);
	//	int err = opus_decode_float(handle, in_buffer.detail.pkt.buffer->buffer, in_buffer.detail.pkt.size, buffer, nb_samples, 0);
	//	if (err) nb_samples = 0;
	//	in_buffer.release(&in_buffer);
	//	return err;
		return S_OK;
	}
	virtual int FetchBuffer(_buffer_desc& out_buffer) override final
	{
		int write_request = out_buffer.detail.aframe.nb_samples;
		int written = 0;
		int rate = out_buffer.detail.aframe.sample_rate;
		float* to_fill = (float*)out_buffer.detail.aframe.channels[0];
		int channel_count = out_stream.detail.audio.layout.channel_count;
		if (nb_samples_in_buffer > sample_offset_in_buffer) {
			int copied_samples = (write_request < nb_samples_in_buffer - sample_offset_in_buffer)?
				write_request : nb_samples_in_buffer - sample_offset_in_buffer;
			memcpy(to_fill,&buffer[sample_offset_in_buffer* channel_count],sizeof(float)*(copied_samples)*channel_count);
			sample_offset_in_buffer += copied_samples;
			written = copied_samples;
			if (sample_offset_in_buffer >= nb_samples_in_buffer) {
				nb_samples_in_buffer = 0;
				sample_offset_in_buffer = 0;
			}
		}
		int err = S_OK;
		if (write_request - written) {
			if (!in_queue.front()) {
				if (written) {
					out_buffer.detail.aframe.nb_samples = written;
					return S_OK;
				}
				printf("concealment needed here. not yet implemented");
			}
			else {
				_buffer_desc& cur_input = *in_queue.front();
				refed_buffer_block* block = cur_input.detail.pkt.buffer;
				assert(block->refs.load() == 1);
				int samples_in_packet = opus_decoder_get_nb_samples(handle, block->buffer, cur_input.detail.pkt.size);
				if (samples_in_packet < write_request - written) {
					int decoded = opus_decode_float(handle, block->buffer, cur_input.detail.pkt.size, to_fill + channel_count * written, samples_in_packet, 0);
					assert(decoded == samples_in_packet);
					written += samples_in_packet;
					out_buffer.detail.aframe.nb_samples = written;
					//make sure no wait...
					out_queue.emplace(cur_input);
					in_queue.pop();
				}
				else {
					int decoded = opus_decode_float(handle, block->buffer, cur_input.detail.pkt.size, buffer, samples_in_packet, 0);
					assert(decoded == samples_in_packet);
					memcpy((to_fill + channel_count * written), buffer, (write_request-written)*sizeof(float)*channel_count);
					nb_samples_in_buffer = samples_in_packet;
					sample_offset_in_buffer = write_request - written;
					written = write_request;
					//make sure no wait...
					out_queue.emplace(cur_input);
					in_queue.pop();
				}
			}
		}
		out_buffer.detail.aframe.sample_rate = freq;
		out_buffer.stream = desc_out;
		out_buffer.release = nullptr;
		out_buffer.start_timestamp = cur_frame;
		out_buffer.end_timestamp = cur_frame + written;
		cur_frame += written;
		return S_OK;
	}
	virtual int Probe()
	{
		return S_OK;
	}
	virtual int Dropped(int samples) override final
	{
		return opus_decode_float(handle,nullptr,0,buffer,samples,1);
	}
	virtual int Flush() override final
	{
		return opus_decode_float(handle,nullptr,0,buffer,0,0);
	}
	virtual int ReleaseBuffer(_buffer_desc& buffer) override final
	{
		return S_OK;
	}
	virtual int AllocBuffer(_buffer_desc& buffer) override final
	{
		return E_INVALID_OPERATION;
	}
};

audio_decoder* audio_decoder_factory::CreateDefaultOpusDecoder(stream_desc* upstream)
{
	return  new opus_decoder(upstream);
}


