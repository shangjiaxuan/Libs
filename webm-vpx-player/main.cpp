#include "mkv_source.h"
#include "mkv_sink.h"
#include "media_transform.h"

int main()
{
	//check memory leak
	while(true){
		mkv_source* mkv_file =
			mkv_source_factory::CreateFromFile("D:\\GamePlatforms\\Steam\\steamapps\\common\\Neptunia Rebirth1\\data\\MOVIE00000\\movie\\direct.webm");
		stream_desc* streams; size_t num;
		mkv_file->GetOutputs(streams,num);
//		mkv_sink* mkv_ofile = 
//			mkv_sink_factory::CreateFromFile(streams, num, "D:\\GamePlatforms\\Steam\\steamapps\\common\\Neptunia Rebirth1\\data\\MOVIE00000\\movie\\direct_remux.mkv");
		_buffer_desc desc{};
		for (size_t i = 0; i < num; ++i) {
			if (streams[i].type == stream_desc::MTYPE_VIDEO) {
				if (streams[i].detail.video.codec == stream_desc::video_info::VCODEC_VP9) {
					streams[i].downstream = video_decoder_factory::CreateDefaultVP9Decoder(&streams[i]);
				}
			}
		}
		while(!mkv_file->FetchBuffer(desc)){
//			mkv_ofile->QueueBuffer(desc);
			mkv_file->ReleaseBuffer(desc);
		}
//		mkv_ofile->Flush();
		for (size_t i = 0; i < num; ++i) {
			if (streams[i].type == stream_desc::MTYPE_VIDEO) {
				if (streams[i].detail.video.codec == stream_desc::video_info::VCODEC_VP9) {
					delete streams[i].downstream;
				}
			}
		}
		delete mkv_file;
//		delete mkv_ofile;
	}
}
