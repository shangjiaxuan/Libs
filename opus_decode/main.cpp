#include "../webm-vpx-player/mkv_source.h"
#include <soundio/soundio.h>
#include <Windows.h>
#include <cstdint>
#include <cmath>
#include <opus/opus.h>


int64_t index = 0;

void write_callback(struct SoundIoOutStream* ost, int frame_count_min, int frame_count_max)
{
	SoundIoChannelArea* areas;
	int frame_count = frame_count_max;
	soundio_outstream_begin_write(ost,&areas,&frame_count);
	for (int i = 0; i < 2; ++i) {
		for (int j = 0; j < frame_count; ++j) {
			*(float*)areas[i].ptr = sinf(0.0625f*(index+j));
			areas[i].ptr+=areas[i].step;
		}
	}
	index+=frame_count;
	soundio_outstream_end_write(ost);
}


int main()
{
	SoundIo*  sio = soundio_create();
	soundio_connect(sio);
	soundio_flush_events(sio);
	SoundIoDevice* dev = soundio_get_output_device(sio,soundio_default_output_device_index(sio));
	SoundIoOutStream* ost = soundio_outstream_create(dev);
	ost->format = SoundIoFormatFloat32LE;
	ost->layout = *soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdStereo);
	ost->write_callback = write_callback;
	soundio_outstream_open(ost);
	mkv_source* source = mkv_source_factory::CreateFromFile("D:\\GamePlatforms\\Steam\\steamapps\\common\\Neptunia Rebirth1\\data\\MOVIE00000\\movie\\direct.webm");
	stream_desc* desc;
	size_t num;
	source->GetOutputs(desc,num);
	float abuffer[5760 * 2]{};
	_buffer_desc buffer{};
	int err = 0;
	OpusDecoder* opus_dec= opus_decoder_create(48000,2,&err);
	while (!source->FetchBuffer(buffer)) {
		if (buffer.stream->type == stream_desc::MTYPE_AUDIO) {
			int len = opus_packet_get_nb_samples((uint8_t*)buffer.detail.pkt.buffer, buffer.detail.pkt.size, 48000);
			err= opus_decode_float(opus_dec,(uint8_t*)buffer.detail.pkt.buffer,buffer.detail.pkt.size,abuffer, len,0);
		}
	}

	soundio_outstream_start(ost);
	Sleep(INFINITE);
	return 0;
}