#include "mkv_sink.h"

#include <matroska/matroska2.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

mkv_sink::mkv_sink()
{
	NodeContext_Init(&ctx, nullptr, nullptr, nullptr);
	ostream.ptr = this;
	ostream.AnyNode = &ctx;
	EBML_RegisterAll((nodemodule*)&ctx);
	MATROSKA_RegisterAll((nodemodule*)&ctx);
	MATROSKA_Init(&ctx);
	NodeRegisterClassEx((nodemodule*)&ctx, HaaliStream_Class);
	NodeRegisterClassEx((nodemodule*)&ctx, WriteStream_Class);
}

int mkv_sink::finish_init()
{
	char err[2048]{};
//	ostream.progress(&ostream, 0, 0);
	file = mkv_OpenOutput(&ostream, MATROSKA_OUTPUT_NOSEEK,err, 2048);
	if (!file) {
		return 1;
	}
	return 0;
}

int mkv_sink::AddTrack(const stream_desc& info)
{
	TrackInfo track{};
	switch (info.type) {

		case stream_desc::MTYPE_NONE:
			break;
		case stream_desc::MTYPE_VIDEO:
			track.Type = TRACK_TYPE_VIDEO;
			track.AV.Video.PixelWidth = info.detail.video.width;
			track.AV.Video.PixelHeight = info.detail.video.height;
			switch (info.detail.video.codec) {
				case stream_desc::video_info::VCODEC_NOTSUPPORTED:
					return E_INVALID_OPERATION;
					break;
				case stream_desc::video_info::VCODEC_RAW:
					return E_UNIMPLEMENTED;
					track.CodecID = "V_UNCOMPRESSED";
					break;
				case stream_desc::video_info::VCODEC_VP8:
					track.CodecID = "V_VP8";
					break;
				case stream_desc::video_info::VCODEC_VP9:
					track.CodecID = "V_VP9";
					break;
				default:
					return E_UNIMPLEMENTED;
			}
			track.AV.Video.Colour.BitsPerChannel = info.detail.video.bitdepth;
			track.AV.Video.Colour.CbSubsamplingHorz  = info.detail.video.subsample_horiz;
			track.AV.Video.Colour.CbSubsamplingVert = info.detail.video.subsample_vert;
			track.AV.Video.Colour.ChromaSubsamplingHorz = info.detail.video.subsample_horiz;
			track.AV.Video.Colour.ChromaSubsamplingVert = info.detail.video.subsample_vert;
			if (info.detail.video.location) {
				track.AV.Video.Colour.ChromaSitingHorz = 1;
				track.AV.Video.Colour.ChromaSitingVert = 1;
			}
			switch (info.detail.video.range) {
				case stream_desc::video_info::CR_STUDIO_RANGE:
					track.AV.Video.Colour.Range = 1;
					break;
				case stream_desc::video_info::CR_FULL_RANGE:
					track.AV.Video.Colour.Range = 2;
					break;
				default:
					track.AV.Video.Colour.Range = 0;
			}
			switch (info.detail.video.space) {
				case stream_desc::video_info::CS_UNKNOWN:
					break;
				default:
					return E_UNIMPLEMENTED;
			}
			//unimplemented yet
			/*
			if (info.detail.video.invert_uv) {
				return E_UNIMPLEMENTED;
			}
			switch (info.detail.video.space) {
				
			}
			switch (info.detail.video.range) {

			}
			*/
			break;
		case stream_desc::MTYPE_AUDIO:
			track.Type = TRACK_TYPE_AUDIO;
			track.AV.Audio.SamplingFreq = info.detail.audio.Hz;
			//TODO: put bitdepth and other format info into the container here.
			switch (info.detail.audio.codec) {
				case stream_desc::audio_info::ACODEC_NONE:
				case stream_desc::audio_info::ACODEC_PCM:
					if (info.detail.audio.format.isfloat) {
						track.CodecID = "A_PCM/FLOAT/IEEE";
					}
					else if (info.detail.audio.format.isBE) {
						track.CodecID = "A_PCM/INT/BIG";
					}
					else {
						track.CodecID = "A_PCM/INT/LIT";
					}
					break;
				case stream_desc::audio_info::ACODEC_FLAC:
					track.CodecID = "A_FLAC";
					break;
				case stream_desc::audio_info::ACODEC_VORBIS:
					track.CodecID = "A_VORBIS";
					break;
				case stream_desc::audio_info::ACODEC_OPUS:
					track.CodecID = "A_OPUS";
					break;
				default:
					return E_UNIMPLEMENTED;
			}
			track.AV.Audio.Channels = info.detail.audio.layout.channel_count;
			break;
		case stream_desc::MTYPE_LAST:
			break;
	}
	track.CodecDelay = info.format_info.CodecDelay;
	track.CodecPrivate = info.format_info.CodecPrivate;
	track.CodecPrivateSize = info.format_info.CodecPrivateSize;
	track.Default = info.format_info.meta.mkv.Default;
	track.Enabled = info.format_info.meta.mkv.Enabled;
	track.Forced = info.format_info.meta.mkv.Forced;
	memcpy(track.Language, info.format_info.meta.mkv.Language, 4);
	track.Name = info.format_info.Name;
	return mkv_AddTrack(file, &track);
}

int mkv_sink::write_headers()
{
	char err[2048]{};
	mkv_WriteHeader(file, err, 2048);
	mkv_TempHeaders(file);
	writing = true;
	return S_OK;
}



mkv_sink::~mkv_sink()
{
	mkv_CloseOutput(file);
	MATROSKA_Done(&ctx);
	MATROSKA_UnRegisterAll((nodemodule*)&ctx);
	EBML_UnRegisterAll((nodemodule*)&ctx);
	NodeContext_Done(&ctx);
}

class mkv_file_sink: public mkv_sink {
	FILE* mfile_handle = nullptr;
	uint64_t file_pos = 0;
protected:
friend mkv_sink_factory;
	mkv_file_sink(const char* path) {
		ostream.geterror = geterror;
		ostream.getfilesize = getfilesize;
		ostream.iowrite = iowrite;
		ostream.iowritech = iowritech;
		ostream.ioseek = ioseek;
		ostream.iotell = iotell;
		ostream.memalloc = memalloc;
		ostream.memfree = memfree;
		ostream.memrealloc = memrealloc;
		ostream.progress = progress;
		ostream.write = write;
		FILE* file_handle = fopen(path, "wb");
		if (!file_handle) {
			printf("Cannot open file\n");
			return;
		}
		mfile_handle = file_handle;
	}
public:
	virtual ~mkv_file_sink() override final
	{
		if(writing)
			Flush();
		if (mfile_handle) {
			fclose(mfile_handle);
			mfile_handle = nullptr;
		}
	}
	virtual int QueueBuffer(_buffer_desc& buffer) override final
	{
		if(!writing) 
			return E_INVALID_OPERATION;
		mkv_WriteFrame(file, buffer.data2, buffer.start_time, buffer.end_time, buffer.data1, buffer.data, buffer.data3, 0);
		return S_OK;
	}
	virtual int AllocBuffer(_buffer_desc& buffer) override final
	{
		return E_PROTOCOL_MISMATCH;
	}
	virtual int Flush() override final
	{
		if(!writing)
			return E_INVALID_OPERATION;
		mkv_WriteTail(file);
		fflush(mfile_handle);
		writing = false;
		return S_OK;
	}

private:
	static const char* geterror(OutputStream* cc) noexcept 
	{
		return "dummy error";
	}
	static filepos_t getfilesize(OutputStream* cc) noexcept
	{
		__int64 cur = _ftelli64(((mkv_file_sink*)cc->ptr)->mfile_handle);
		fseek(((mkv_file_sink*)cc->ptr)->mfile_handle, 0L, SEEK_END);
		__int64 size = _ftelli64(((mkv_file_sink*)cc->ptr)->mfile_handle);
		_fseeki64(((mkv_file_sink*)cc->ptr)->mfile_handle, cur, SEEK_SET);
		return size;
	}
	static int iowrite(OutputStream* inf, void* buffer, int count) noexcept
	{
		return fwrite(buffer, 1, count, ((mkv_file_sink*)inf->ptr)->mfile_handle);
	}
	static bool_t iowritech(OutputStream* inf, int ch) noexcept
	{
		char temp = ch;
		return fwrite(&temp, 1, 1, ((mkv_file_sink*)inf->ptr)->mfile_handle);
	}
	static void ioseek(OutputStream* inf, longlong wher, int how) noexcept
	{
		_fseeki64(((mkv_file_sink*)inf->ptr)->mfile_handle, wher, how);
	}
	static filepos_t iotell(OutputStream* inf) noexcept
	{
		return _ftelli64(((mkv_file_sink*)inf->ptr)->mfile_handle);
	}
	static void* memalloc(OutputStream* inf, size_t count) noexcept
	{
		return malloc(count);
	}
	static void memfree(OutputStream* inf, void* mem) noexcept
	{
		free(mem);
	}
	static void* memrealloc(OutputStream* inf, void* mem, size_t count) noexcept
	{
		return realloc(mem, count);
	}
	static int progress(OutputStream* inf, filepos_t cur, filepos_t max) noexcept
	{
		return 0;
	}
	static int write(OutputStream* inf, filepos_t pos, void* buffer, size_t count) noexcept
	{
		__int64 cur = _ftelli64(((mkv_file_sink*)inf->ptr)->mfile_handle);
		fseek(((mkv_file_sink*)inf->ptr)->mfile_handle, pos, SEEK_SET);
		uint64_t write = fwrite(buffer, 1, count, ((mkv_file_sink*)inf->ptr)->mfile_handle);
		_fseeki64(((mkv_file_sink*)inf->ptr)->mfile_handle, cur, SEEK_SET);
		return write;
	}
};

mkv_sink* mkv_sink_factory::CreateFromFile(const stream_desc* tracks, size_t num, const char* path)
{
	mkv_file_sink* rtn = new mkv_file_sink(path);
	rtn->finish_init();
	for (int i = 0; i < num; ++i) {
		rtn->AddTrack(tracks[i]);
	}
	rtn->write_headers();
//		NodeContext_Init(&ctx, NULL, NULL, NULL);

	return rtn;
}