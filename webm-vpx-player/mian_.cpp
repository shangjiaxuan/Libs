#include "mkv_source.h"
#include  "mkv_sink.h"


int main()
{
	while(true){
		mkv_source* mkv_file =
			mkv_source_factory::CreateFromFile("D:\\GamePlatforms\\Steam\\steamapps\\common\\Neptunia Rebirth1\\data\\MOVIE00000\\movie\\direct.webm");
		const stream_desc* streams; size_t num;
		mkv_file->GetOutputs(streams,num);
		mkv_sink* mkv_ofile = 
			mkv_sink_factory::CreateFromFile(streams, num, "D:\\GamePlatforms\\Steam\\steamapps\\common\\Neptunia Rebirth1\\data\\MOVIE00000\\movie\\direct_remux.mkv");
		_buffer_desc desc{};
		while(!mkv_file->FetchBuffer(desc)){
			mkv_ofile->QueueBuffer(desc);
			mkv_file->ReleaseBuffer(desc);
		}
		mkv_ofile->Flush();
		delete mkv_file;
		delete mkv_ofile;
	}
}
