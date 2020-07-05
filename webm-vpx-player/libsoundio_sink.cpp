#include "audio_output_sink.h"

#include <soundio/soundio.h>
#include <cstdio>
#include <cassert>

class soundio_sink;
class soundio_global {
protected:
	friend soundio_sink;
	static void on_devices_change(struct SoundIo*) {
		//recheck default device and change to the new device.
	}
	static void on_backend_disconnect(struct SoundIo*, int err) noexcept
	{
		const char* err_str;
		switch (err) {
			case SoundIoErrorBackendDisconnected:
				err_str = "soundio backend disconnect: SoundIoErrorBackendDisconnected";
				break;
			case SoundIoErrorNoMem:
				err_str = "soundio backend disconnect: SoundIoErrorNoMem";
				break;
			case SoundIoErrorSystemResources:
				err_str = "soundio backend disconnect: SoundIoErrorSystemResources";
				break;
			case SoundIoErrorOpeningDevice:
				err_str = "soundio backend disconnect: SoundIoErrorOpeningDevice";
				break;
			default:
				err_str = "soundio backend disconnect: unkown error";
				break;
		}
		fprintf(stderr, err_str);
		//notify outside here.

	}
	static void emit_rtprio_warning() noexcept
	{
		fprintf(stderr, "soundio failed to set realtime priority");
	}
	static void jack_info_callback(const char* msg) noexcept
	{
		fprintf(stderr, "Soundio Jack info: %s", msg);
	};
	static void jack_error_callback(const char* msg) noexcept
	{
		fprintf(stderr, "Soundio Jack error: %s", msg);
	}
};

class soundio_sink: public audio_output_sink {
	SoundIo* sio;
	SoundIoDevice* dev;
	SoundIoOutStream* ost;
	int err = 0;
	soundio_sink(stream_desc* in_stream): 
		sio([in_stream](){assert(in_stream); return soundio_create();}()),
		dev([this]()->SoundIoDevice* {
				assert(sio);
				sio->app_name = "soundio backend";
				sio->on_devices_change = soundio_global::on_devices_change;
				sio->on_backend_disconnect = soundio_global::on_backend_disconnect;
				sio->emit_rtprio_warning = soundio_global::emit_rtprio_warning;
				sio->jack_info_callback = soundio_global::jack_info_callback;
				sio->jack_error_callback = soundio_global::jack_error_callback;
				err = soundio_connect(sio);
				if (err) {
					soundio_destroy(sio);
					return nullptr;
				}
				int index = soundio_default_output_device_index(sio);
				return soundio_get_output_device(sio,index);
			}()
		),
		ost([this]()->SoundIoOutStream* {
				assert(dev);
				assert(!dev->probe_error);
				assert(dev->aim == SoundIoDeviceAimOutput);
				return soundio_outstream_create(dev);
			}()
		)
	{
		assert(ost);
		ost->volume = 1.0;
		ost->userdata = this;
		SoundIoFormat format = SoundIoFormatFloat32NE;
		switch (in_stream->detail.audio.format.bitdepth) {
			case 8:
				if (in_stream->detail.audio.format.isunsigned)
					format = SoundIoFormatU8;
				else
					format = SoundIoFormatS8;
				break;
			case 16:
				if (in_stream->detail.audio.format.isunsigned) {
					if(in_stream->detail.audio.format.isBE)
						format = SoundIoFormatU16BE;
					else
						format = SoundIoFormatU16LE;
				}
				else {
					if (in_stream->detail.audio.format.isBE)
						format = SoundIoFormatS16BE;
					else
						format = SoundIoFormatS16LE;
				}
				break;
			case 24:
				if (in_stream->detail.audio.format.isunsigned) {
					if (in_stream->detail.audio.format.isBE)
						format = SoundIoFormatU24BE;
					else
						format = SoundIoFormatU24LE;
				}
				else {
					if (in_stream->detail.audio.format.isBE)
						format = SoundIoFormatS24BE;
					else
						format = SoundIoFormatS24LE;
				}
				break;
			case 32:
				if (in_stream->detail.audio.format.isfloat) {
					if (in_stream->detail.audio.format.isBE)
						format = SoundIoFormatFloat32BE;
					else
						format = SoundIoFormatFloat32LE;
				}
				else {
					if (in_stream->detail.audio.format.isunsigned) {
						if (in_stream->detail.audio.format.isBE)
							format = SoundIoFormatU32BE;
						else
							format = SoundIoFormatU32LE;
					}
					else {
						if (in_stream->detail.audio.format.isBE)
							format = SoundIoFormatS32BE;
						else
							format = SoundIoFormatS32LE;
					}
				}
				break;
			case 64:
				if (in_stream->detail.audio.format.isfloat) {
					if (in_stream->detail.audio.format.isBE)
						format = SoundIoFormatFloat64BE;
					else
						format = SoundIoFormatFloat64LE;
				}
				else {
					err = E_PROTOCOL_MISMATCH;
					goto failed;
				}
				break;
			default:
				err = E_PROTOCOL_MISMATCH;
				goto failed;
		}
		ost->format = format;
		ost->layout.channel_count = in_stream->detail.audio.layout.channel_count;
		for (int i = 0; i < in_stream->detail.audio.layout.channel_count; ++i) {
			switch (in_stream->detail.audio.layout.channels[i]) {
				case CH_FRONT_CENTER:
					ost->layout.channels[i]=SoundIoChannelIdFrontCenter;
					break;
				case CH_FRONT_LEFT:
					ost->layout.channels[i] = SoundIoChannelIdFrontLeft;
					break;
				case CH_FRONT_RIGHT:
					ost->layout.channels[i] = SoundIoChannelIdFrontRight;
					break;
				case CH_STEREO_LEFT:
					ost->layout.channels[i] = SoundIoChannelIdFrontLeft;
					break;
				case CH_STEREO_RIGHT:
					ost->layout.channels[i] = SoundIoChannelIdFrontRight;
					break;
				default:
					err = E_UNIMPLEMENTED;
					goto failed;
					break;
			}
		}
		ost->name = "soundio output stream";
		ost->sample_rate = in_stream->detail.audio.Hz;



		err = soundio_outstream_open(ost);
		desc_in = in_stream;
		return;
		failed:
		if (err) {
			soundio_outstream_destroy(ost);
			ost = nullptr;
			soundio_device_unref(dev);
			dev = nullptr;
			soundio_destroy(sio);
			sio = nullptr;
		}
	}
	~soundio_sink()
	{

	}
private:
	

};


