#include "soundio_outstream.h"

int main()
{
	soundio_service<> serv;
	while (true) {
		soundio_device dev = serv.GetOutputDeviceFromIndex(serv.DefaultOutput());
		stream_desc desc{};
		desc.type = stream_desc::MTYPE_AUDIO;
		desc.detail.audio.codec = stream_desc::audio_info::ACODEC_PCM;
		desc.detail.audio.layout = *stream_desc::audio_info::GetBuiltinLayoutFromType(stream_desc::audio_info::CH_LAYOUT_STEREO);
		desc.detail.audio.Hz = 48000;
		desc.detail.audio.format.isfloat = 1;
		desc.detail.audio.format.bitdepth = 32;
		desc.detail.audio.format.isBE = 0;
		{
			soundio_outstream ost(&desc, dev);
			int err = ost.Start();
			_sleep(1000);
		}
		_sleep(1000);
	}

}
