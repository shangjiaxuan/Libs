#include "mkv_source.h"
#include "mkv_sink.h"
#include "media_transform.h"

#include "soundio_service.h"
int main()
{
	//check memory leak
	while(true){
		soundio_service<> serv;
		mkv_source* mkv_file =
			mkv_source_factory::CreateFromFile("D:\\GamePlatforms\\Steam\\steamapps\\common\\Neptunia Rebirth1\\data\\MOVIE00000\\movie\\direct.webm");
		stream_desc* streams; size_t num;
		mkv_file->GetOutputs(streams,num);
//		mkv_sink* mkv_ofile = 
//			mkv_sink_factory::CreateFromFile(streams, num, "D:\\GamePlatforms\\Steam\\steamapps\\common\\Neptunia Rebirth1\\data\\MOVIE00000\\movie\\direct_remux.mkv");
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
				}
			}
		}
		//Decode all the frames. This should be managed by topology
		while(!mkv_file->FetchBuffer(desc)){
//			mkv_ofile->QueueBuffer(desc);
			_buffer_desc DecodedFrame{};
			if (desc.stream) {
				if (desc.stream->downstream) {
					desc.stream->downstream->QueueBuffer(desc);
					while (!desc.stream->downstream->FetchBuffer(DecodedFrame)) {
						//Present or queue frame
						if(DecodedFrame.release)
							DecodedFrame.release(&DecodedFrame, desc.stream->downstream);
					}
				}
			}
			if(desc.release)
				desc.release(&desc,mkv_file);
		}
//		mkv_ofile->Flush();
		//Tearing down topology. Topology resolver not yet implemented
		for (size_t i = 0; i < num; ++i) {
			if (streams[i].type == stream_desc::MTYPE_VIDEO) {
				if (streams[i].detail.video.codec == stream_desc::video_info::VCODEC_VP9) {
					delete streams[i].downstream;
				}
			}
			else if (streams[i].type == stream_desc::MTYPE_AUDIO) {
				if (streams[i].detail.video.codec == stream_desc::audio_info::ACODEC_OPUS) {
					delete streams[i].downstream;
				}
			}
		}
		delete mkv_file;
//		delete mkv_ofile;
	}
}
