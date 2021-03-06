#include "mkv_source.h"
#include "media_buffer.h"

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <matroska/matroska2.h>
#include <atomic>
#include <cassert>

mkv_source::mkv_source()
{
	NodeContext_Init(&ctx, nullptr, nullptr, nullptr);
	istream.ptr = this;
	istream.AnyNode = &ctx;
	EBML_RegisterAll((nodemodule*)&ctx);
	MATROSKA_RegisterAll((nodemodule*)&ctx);
	MATROSKA_Init(&ctx);
	NodeRegisterClassEx((nodemodule*)&ctx, HaaliStream_Class);
	NodeRegisterClassEx((nodemodule*)&ctx, WriteStream_Class);
}

int mkv_source::finish_init()
{
	char err[2048]{};
	istream.progress(&istream, 0, 0);
	file = mkv_OpenInput(&istream, err, 2048);
	if (!file) {
		return 1;
	}
	num_out = mkv_GetNumTracks(file);
	desc_out = new stream_desc[num_out]();
	for (size_t i = 0; i < num_out; ++i) {
		desc_out[i].mode = stream_desc::MODE_DOWN_NOTIFY_UP;
		TrackInfo* info = mkv_GetTrackInfo(file,i);
		if (info->Type == TRACK_TYPE_VIDEO) {
			desc_out[i].type = stream_desc::MTYPE_VIDEO;
			stream_desc::video_info oinfo{};
			oinfo.width = info->AV.Video.PixelWidth;
			oinfo.height = info->AV.Video.PixelHeight;
			oinfo.fmt.subsample_horiz = 1;
			oinfo.fmt.subsample_vert = 1;
			oinfo.fmt.bitdepth = 8;
			if (info->AV.Video.Colour.CbSubsamplingHorz || info->AV.Video.Colour.CbSubsamplingVert ||
				info->AV.Video.Colour.ChromaSubsamplingHorz || info->AV.Video.Colour.ChromaSubsamplingVert) {
				if (!info->AV.Video.Colour.CbSubsamplingHorz && info->AV.Video.Colour.ChromaSubsamplingHorz)
					info->AV.Video.Colour.CbSubsamplingHorz = info->AV.Video.Colour.ChromaSubsamplingHorz;
				if (!info->AV.Video.Colour.ChromaSubsamplingHorz && info->AV.Video.Colour.CbSubsamplingHorz)
					info->AV.Video.Colour.ChromaSubsamplingHorz = info->AV.Video.Colour.CbSubsamplingHorz;
				if (!info->AV.Video.Colour.CbSubsamplingVert && info->AV.Video.Colour.ChromaSubsamplingVert)
					info->AV.Video.Colour.CbSubsamplingVert = info->AV.Video.Colour.ChromaSubsamplingVert;
				if (!info->AV.Video.Colour.ChromaSubsamplingVert && info->AV.Video.Colour.CbSubsamplingVert)
					info->AV.Video.Colour.ChromaSubsamplingVert = info->AV.Video.Colour.CbSubsamplingVert;
				if ((info->AV.Video.Colour.CbSubsamplingHorz != info->AV.Video.Colour.ChromaSubsamplingHorz) ||
					info->AV.Video.Colour.CbSubsamplingVert != info->AV.Video.Colour.CbSubsamplingVert) {
					//not matching uv subsampling, defaulting to 420, although it still should be decodable.
				}
				else {
					//only 2-1 subsampling supported
					if (info->AV.Video.Colour.CbSubsamplingHorz) {
						info->AV.Video.Colour.CbSubsamplingHorz = 1;
						info->AV.Video.Colour.ChromaSubsamplingHorz = 1;
					}
					if (info->AV.Video.Colour.CbSubsamplingVert) {
						info->AV.Video.Colour.CbSubsamplingVert = 1;
						info->AV.Video.Colour.ChromaSubsamplingVert = 1;
					}
				}
				oinfo.fmt.subsample_horiz = info->AV.Video.Colour.CbSubsamplingHorz;
				oinfo.fmt.subsample_vert = info->AV.Video.Colour.CbSubsamplingVert;
			}
			if (info->AV.Video.Colour.BitsPerChannel) {
				oinfo.fmt.bitdepth = info->AV.Video.Colour.BitsPerChannel;
				oinfo.fmt.bitdepth = info->AV.Video.Colour.BitsPerChannel;
			}
			if (info->AV.Video.Colour.ChromaSitingHorz && info->AV.Video.Colour.ChromaSitingVert) {
				oinfo.fmt.location = SUBSAMP_LEFT_CORNER;
			}
			if (strcmp(info->CodecID, "V_VP9") == 0) {
				oinfo.codec = stream_desc::video_info::VCODEC_VP9;
				//decoder creation is defered to topology building
				//decoders[i] = new libvpx_vp9_decoder(sinfo, oinfo, 4);
				desc_out[i].detail = std::move(stream_desc::detailed_info(oinfo));
			}
		}
		if (info->Type == TRACK_TYPE_AUDIO) {
			desc_out[i].type = stream_desc::MTYPE_AUDIO;
			stream_desc::audio_info oinfo;
			if (info->AV.Audio.SamplingFreq) {
				oinfo.Hz = info->AV.Audio.SamplingFreq;
			}
			if (info->AV.Audio.OutputSamplingFreq) {
				oinfo.Hz = info->AV.Audio.OutputSamplingFreq;
			}
			switch (info->AV.Audio.Channels) {
				case 0:
					oinfo.layout = *stream_desc::audio_info::GetBuiltinLayoutFromType(stream_desc::audio_info::CH_LAYOUT_STEREO);
					break;
				case 1:
					oinfo.layout = *stream_desc::audio_info::GetBuiltinLayoutFromType(stream_desc::audio_info::CH_LAYOUT_MONO);
					break;
				case 2:
					oinfo.layout = *stream_desc::audio_info::GetBuiltinLayoutFromType(stream_desc::audio_info::CH_LAYOUT_STEREO);
					break;
				default:
					oinfo.layout = *stream_desc::audio_info::GetBuiltinLayoutFromType(stream_desc::audio_info::CH_LAYOUT_STEREO);
			}
			if (strcmp(info->CodecID, "A_OPUS") == 0) {
				oinfo.Hz = 48000;
				oinfo.layout = *stream_desc::audio_info::GetBuiltinLayoutFromType(stream_desc::audio_info::CH_LAYOUT_STEREO);
				oinfo.matrix = stream_desc::audio_info::MATRIX_ENCODING_NONE;
				oinfo.planar = false;
				oinfo.codec = stream_desc::audio_info::ACODEC_OPUS;
				//decoder creation is defered to topology building
				//decoders[i] = new libopus_opus_decoder(sinfo, sinfo);
				desc_out[i].detail = std::move(stream_desc::detailed_info(oinfo));
			}
		}
		desc_out[i].upstream = desc_out[i].upstream = this;
		desc_out[i].downstream = nullptr;
		desc_out[i].format_info.CodecDelay = info->CodecDelay;
		desc_out[i].format_info.CodecPrivate = info->CodecPrivate;
		desc_out[i].format_info.CodecPrivateSize = info->CodecPrivateSize;
		desc_out[i].format_info.meta.mkv.Default = info->Default;
		desc_out[i].format_info.meta.mkv.Enabled = info->Enabled;
		desc_out[i].format_info.meta.mkv.Forced = info->Forced;
		memcpy(desc_out[i].format_info.meta.mkv.Language,info->Language,4);
		desc_out[i].format_info.Name = info->Name;
		desc_out[i].upstream = this;
	}
	istream.progress(&istream, file->pFirstCluster, 0);
//	file_pos = file->pFirstCluster;
	return 0;
}

mkv_source::~mkv_source()
{
	delete[] desc_out;
	mkv_CloseInput(file);
	MATROSKA_Done(&ctx);
	MATROSKA_UnRegisterAll((nodemodule*)&ctx);
	EBML_UnRegisterAll((nodemodule*)&ctx);
	NodeContext_Done(&ctx);
}

class mkv_file_source:public mkv_source {
	FILE* mfile_handle = nullptr;
	uint64_t file_pos = 0;
public:
	virtual int FetchBuffer(_buffer_desc& buffer) override final
	{
		//check protocol error here
		if (buffer.detail.pkt.buffer && buffer.release) {
			buffer.release(&buffer);
			buffer.detail.pkt.buffer = nullptr;
		}
		unsigned int flags;
		uint32_t track, size;
		uint64_t start, end;
		int err = mkv_ReadFrame(file, 0, &track, &start, &end, &file_pos, &size, (void**)&buffer.detail.pkt.buffer, &flags);
		if (err)
			return err;
		assert(buffer.detail.pkt.buffer->refs == 1);
		buffer.detail.pkt.track = track;
		buffer.detail.pkt.size = size;
		buffer.start_timestamp = start;
		buffer.end_timestamp = end;
		if (!(flags & FRAME_UNKNOWN_END && flags & FRAME_UNKNOWN_START)) {
			if(flags & FRAME_UNKNOWN_END)
				buffer.end_timestamp = buffer.start_timestamp;
			else if(flags & FRAME_UNKNOWN_START)
				buffer.start_timestamp = buffer.end_timestamp;
		}
		if (flags & FRAME_KF)
			buffer.detail.pkt.key_frame = 1;
		else
			buffer.detail.pkt.key_frame = 0;
		buffer.release = release_frame;
		buffer.stream=&desc_out[track];
		buffer.release_private_ptr = this;
		if(desc_out[track].downstream)
			desc_out[track].downstream->QueueBuffer(buffer);
		return 0;
	}
	virtual int ReleaseBuffer(_buffer_desc& buffer) override final
	{
		if (buffer.detail.pkt.buffer && buffer.release) {
			buffer.release(&buffer);
			buffer.detail.pkt.buffer = nullptr;
		}
		return 0;
	}
	virtual ~mkv_file_source() override final
	{
		if (mfile_handle) {
			fclose(mfile_handle);
			mfile_handle = nullptr;
		}
	}
private:
	static void release_frame(_buffer_desc* buffer)
	{
		mkv_file_source* __this = (mkv_file_source*)buffer->release_private_ptr;
		__this->istream.releaseref(&__this->istream, buffer->detail.pkt.buffer);
		buffer->detail.pkt.buffer = nullptr;
	}
protected:
	friend mkv_source_factory;
	mkv_file_source(const char* path)
	{
		istream.geterror = geterror;
		istream.getfilesize = getfilesize;
		istream.ioread = ioread;
		istream.ioreadch = ioreadch;
		istream.ioseek = ioseek;
		istream.iotell = iotell;
		istream.makeref = makeref;
		istream.memalloc = memalloc;
		istream.memfree = memfree;
		istream.memrealloc = memrealloc;
		istream.progress = progress;
		istream.read = read;
		istream.releaseref = releaseref;
		istream.scan = nullptr;
		FILE* file_handle = fopen(path, "rb");
		if (!file_handle) {
			printf("Cannot open file\n");
			return;
		}
		mfile_handle = file_handle;
	}
	void cleanup_if_failed()
	{
		if (mfile_handle) {
			fclose(mfile_handle);
			mfile_handle = nullptr;
		}
	}
	void finish_open()
	{
		istream.progress(&istream, file->pFirstCluster, 0);
		file_pos = file->pFirstCluster;
	}
private:
	static const char* geterror(InputStream* cc) noexcept
	{
		return "dummy error";
	}
	static filepos_t getfilesize(InputStream* cc) noexcept
	{
		__int64 cur = _ftelli64(((mkv_file_source*)cc->ptr)->mfile_handle);
		fseek(((mkv_file_source*)cc->ptr)->mfile_handle, 0L, SEEK_END);
		__int64 size = _ftelli64(((mkv_file_source*)cc->ptr)->mfile_handle);
		_fseeki64(((mkv_file_source*)cc->ptr)->mfile_handle, cur, SEEK_SET);
		return size;
	}
	static int ioread(InputStream* inf, void* buffer, int count) noexcept
	{
		return fread(buffer, 1, count, ((mkv_file_source*)inf->ptr)->mfile_handle);
	}
	static int ioreadch(InputStream* inf) noexcept
	{
		char ch;
		fread(&ch, 1, 1, ((mkv_file_source*)inf->ptr)->mfile_handle);
		return ch;
	}
	static void ioseek(InputStream* inf, longlong wher, int how) noexcept
	{
		_fseeki64(((mkv_file_source*)inf->ptr)->mfile_handle, wher, how);
	}
	static filepos_t iotell(InputStream* inf) noexcept
	{
		return _ftelli64(((mkv_file_source*)inf->ptr)->mfile_handle);
	}
	static void* makeref(InputStream* inf, int count)
	{
		mkv_file_source& reading = *(mkv_file_source*)inf->ptr;
		refed_buffer_block* block = (refed_buffer_block*)inf->memalloc(inf, sizeof(refed_buffer_block) + count);
		new(block)refed_buffer_block();
		block->ref();
		fread(block->buffer, 1, count, reading.mfile_handle);
		return block;
	}
	static void* memalloc(InputStream* inf, size_t count) noexcept
	{
		return malloc(count);
	}
	static void memfree(InputStream* inf, void* mem) noexcept
	{
		free(mem);
	}
	static void* memrealloc(InputStream* inf, void* mem, size_t count) noexcept
	{
		return realloc(mem, count);
	}
	static int progress(InputStream* inf, filepos_t cur, filepos_t max) noexcept
	{
		return _fseeki64(((mkv_file_source*)inf->ptr)->mfile_handle, cur, SEEK_SET);
	}
	static int read(InputStream* inf, filepos_t pos, void* buffer, size_t count) noexcept
	{
		__int64 cur = _ftelli64(((mkv_file_source*)inf->ptr)->mfile_handle);
		fseek(((mkv_file_source*)inf->ptr)->mfile_handle, pos, SEEK_SET);
		uint64_t read = fread(buffer, 1, count, ((mkv_file_source*)inf->ptr)->mfile_handle);
		_fseeki64(((mkv_file_source*)inf->ptr)->mfile_handle, cur, SEEK_SET);
		return read;
	}
	static void releaseref(InputStream* inf, void* ref)
	{
		refed_buffer_block* block = (refed_buffer_block*)ref;
		block->unref();
	}
};

mkv_source* mkv_source_factory::CreateFromFile(const char* path)
{
	mkv_file_source* source = new mkv_file_source(path);
	int err = source->finish_init();
	if (err) {
		source->cleanup_if_failed();
		delete source;
		source = nullptr;
	}
	else {
		source->finish_open();
	}
	return source;
}
