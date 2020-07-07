#include <soundio/soundio.h>

#include <Windows.h>
#include <stdio.h>
void write(SoundIoOutStream* ost, int _min, int _max){}

int main()
{
	SoundIo* sio = soundio_create();
	int err = soundio_connect(sio);
	soundio_flush_events(sio);
	SoundIoDevice* dev = soundio_get_output_device(sio, soundio_default_output_device_index(sio));
	while(true)
	{
		SoundIoOutStream* stream =soundio_outstream_create(dev);
		stream->format = SoundIoFormatFloat32NE;
		stream->sample_rate = 48000;
		stream->layout = *soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdStereo);
		stream->write_callback = write;
		err = soundio_outstream_open(stream);
		err = soundio_outstream_start(stream);
		Sleep(500);
		soundio_flush_events(sio);
		soundio_outstream_pause(stream,1);
		soundio_outstream_clear_buffer(stream);
		soundio_outstream_destroy(stream);
		soundio_flush_events(sio);
	}
	soundio_device_unref(dev);
	soundio_flush_events(sio);
	soundio_disconnect(sio);
	soundio_destroy(sio);
	ExitProcess(0);
}


