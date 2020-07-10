#include "mkv_source.h"
#include "mkv_sink.h"
#include "media_transform.h"

#include "soundio_service.h"
#include "soundio_outstream.h"
int main()
{
	//check memory leak
	while(true){
		soundio_service<> serv;
		soundio_device dev = serv.GetOutputDeviceFromIndex(serv.DefaultOutput());
		mkv_source* mkv_file =
			mkv_source_factory::CreateFromFile("D:\\GamePlatforms\\Steam\\steamapps\\common\\Neptunia Rebirth1\\data\\MOVIE00000\\movie\\direct.webm");
		stream_desc* streams; size_t num;
		mkv_file->GetOutputs(streams,num);
		_buffer_desc desc{};
		//Building topology. Topology resolver not yet implemented
		for (size_t i = 0; i < num; ++i) {
			if (streams[i].type == stream_desc::MTYPE_VIDEO) {
				if (streams[i].detail.video.codec == stream_desc::video_info::VCODEC_VP9) {
					streams[i].downstream = video_decoder_factory::CreateDefaultVP9Decoder(&streams[i]);
					
				}
			}
			else if (streams[i].type == stream_desc::MTYPE_AUDIO) {
				if (streams[i].detail.video.codec == stream_desc::audio_info::ACODEC_OPUS) {
					streams[i].downstream = audio_decoder_factory::CreateDefaultOpusDecoder(&streams[i]);
					stream_desc* outs;
					size_t num;
					streams[i].downstream->GetOutputs(outs,num);
					outs->downstream = new soundio_outstream(outs, dev);
				}
			}
		}
		bool audio_started = false;
		//Decode all the frames. This should be managed by topology
		while(!mkv_file->FetchBuffer(desc)){
//			mkv_ofile->QueueBuffer(desc);
			_buffer_desc DecodedFrame{};
			if (desc.stream) {
				if (desc.stream->type == stream_desc::MTYPE_AUDIO) {
					stream_desc* _desc; size_t size;
					(desc.stream->downstream)->GetOutputs(_desc, size);
					if (!audio_started) {
						audio_started = true;
						((soundio_outstream*)_desc->downstream)->Start();
					}
				}
				if (desc.stream->mode == stream_desc::MODE_PROACTIVE)
				while (!desc.stream->downstream->FetchBuffer(DecodedFrame)) {
				}
			}
		}
		//Tearing down topology. Topology resolver not yet implemented
		for (size_t i = 0; i < num; ++i) {
			if (streams[i].type == stream_desc::MTYPE_VIDEO) {
				if (streams[i].detail.video.codec == stream_desc::video_info::VCODEC_VP9) {
					delete streams[i].downstream;
				}
			}
			else if (streams[i].type == stream_desc::MTYPE_AUDIO) {
				if (streams[i].detail.video.codec == stream_desc::audio_info::ACODEC_OPUS) {
					stream_desc* sio_desc;
					size_t num;
					streams[i].downstream->GetOutputs(sio_desc, num);
					delete sio_desc->downstream;
					delete streams[i].downstream;
				}
			}
		}
		_sleep(0xffffffff);
		delete mkv_file;
	}
}
