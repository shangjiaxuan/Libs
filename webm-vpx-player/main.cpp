#include <vpx/vp8.h>
#include <vpx/vp8cx.h>
#include <vpx/vp8dx.h>
#include <vpx/vpx_decoder.h>
#include <vpx/vpx_image.h>
#include <vpx/vpx_codec.h>
#include <matroska/matroska2.h>
#include <matroska/MatroskaParser.h>
#include <matroska/MatroskaWriter.h>
#include <opus/opus.h>
#include <soundio/soundio.h>

#include <cstdio>
#include <vector>
#include <mutex>

#include "types.h"

template<typename child>
class mkv_reader {
protected:
	std::vector<std::vector<char>> buffers;
	InputStream istream{};
	nodecontext ctx{};
public:
	struct frame {
		unsigned size;
		unsigned track;
		uint64_t start;
		uint64_t end;
		uint8_t* data;
		bool key_frame;
	};
	MatroskaFile* file = nullptr;
	uint64_t file_pos;
	mkv_reader() noexcept
	{
		NodeContext_Init(&ctx,nullptr,nullptr,nullptr);
		istream.ptr = this;
		istream.AnyNode = &ctx;
		EBML_RegisterAll((nodemodule*)&ctx);
		MATROSKA_RegisterAll((nodemodule*)&ctx);
		MATROSKA_Init(&ctx);
		NodeRegisterClassEx((nodemodule*)&ctx, HaaliStream_Class);
		NodeRegisterClassEx((nodemodule*)&ctx, WriteStream_Class);
	}
	virtual ~mkv_reader() noexcept
	{
		mkv_CloseInput(file);
		MATROSKA_Done(&ctx);
		MATROSKA_UnRegisterAll((nodemodule*)&ctx);
		EBML_UnRegisterAll((nodemodule*)&ctx);
		NodeContext_Done(&ctx);
	}
private:
/*
	virtual static const char* geterror(InputStream* cc) noexcept
	{
		return "dummy error";
	}
	virtual static filepos_t getfilesize(InputStream* cc) noexcept
	{
		__int64 cur = _ftelli64(((mkv_reader*)cc->ptr)->mfile_handle);
		fseek(((mkv_reader*)cc->ptr)->mfile_handle, 0L, SEEK_END);
		__int64 size = _ftelli64(((mkv_reader*)cc->ptr)->mfile_handle);
		_fseeki64(((mkv_reader*)cc->ptr)->mfile_handle, cur, SEEK_SET);
		return size;
	}
	virtual static int ioread(InputStream* inf, void* buffer, int count) noexcept
	{
		return fread(buffer, 1, count, ((mkv_reader*)inf->ptr)->mfile_handle);
	}
	virtual static int ioreadch(InputStream* inf) noexcept
	{
		char ch;
		fread(&ch, 1, 1, ((mkv_reader*)inf->ptr)->mfile_handle);
		return ch;
	}
	virtual static void ioseek(InputStream* inf, longlong wher, int how) noexcept
	{
		_fseeki64(((mkv_reader*)inf->ptr)->mfile_handle, wher, how);
	}
	virtual static filepos_t iotell(InputStream* inf) noexcept
	{
		return _ftelli64(((mkv_reader*)inf->ptr)->mfile_handle);
	}
	virtual static void* makeref(InputStream* inf, int count)
	{
		mkv_reader& reading = *(mkv_reader*)inf->ptr;
		std::vector<char> temp(count);
		fread(&temp[0], 1, count, reading.mfile_handle);
		reading.buffers.emplace_back(std::move(temp));
		return &reading.buffers.back()[0];
	}
	virtual static void* memalloc(InputStream* inf, size_t count) noexcept
	{
		return malloc(count);
	}
	virtual static void memfree(InputStream* inf, void* mem) noexcept
	{
		free(mem);
	}
	virtual static void* memrealloc(InputStream* inf, void* mem, size_t count) noexcept
	{
		return realloc(mem, count);
	}
	virtual static int progress(InputStream* inf, filepos_t cur, filepos_t max) noexcept
	{
		return _fseeki64(((mkv_reader*)inf->ptr)->mfile_handle, cur, SEEK_SET);
	}
	virtual static int read(InputStream* inf, filepos_t pos, void* buffer, size_t count) noexcept
	{
		__int64 cur = _ftelli64(((mkv_reader*)inf->ptr)->mfile_handle);
		fseek(((mkv_reader*)inf->ptr)->mfile_handle, pos, SEEK_SET);
		uint64_t read = fread(buffer, 1, count, ((mkv_reader*)inf->ptr)->mfile_handle);
		_fseeki64(((mkv_reader*)inf->ptr)->mfile_handle, cur, SEEK_SET);
		return read;
	}
	virtual static void releaseref(InputStream* inf, void* ref)
	{
		mkv_reader& reading = *(mkv_reader*)inf->ptr;
		for (uint64_t i = 0; i < reading.buffers.size(); ++i) {
			if (&reading.buffers[i][0] == ref) {
				for (uint64_t j = i; j < reading.buffers.size() - 1; ++j) {
					reading.buffers[j] = std::move(reading.buffers[j + 1]);
				}
				//need this for valid buffer list.
				reading.buffers.pop_back();
				break;
			}
		}
	}
*/
};

class file_reader : public mkv_reader<file_reader> {
	FILE* mfile_handle;
public:
	struct frame {
		unsigned size;
		unsigned track;
		uint64_t start;
		uint64_t end;
		uint8_t* data;
		bool key_frame;
	};
	uint64_t file_pos;
	file_reader(const char* filename)
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
		FILE* file_handle = fopen(filename, "rb");
		if (!file_handle) {
			printf("Cannot open file\n");
			return;
		}
		mfile_handle = file_handle;
		char err[2048]{};
		istream.progress(&istream, 0, 0);
		file = mkv_OpenInput(&istream, err, 2048);
		if (!file) {
			fclose(mfile_handle);
			mfile_handle=nullptr;
			return;
		}
		istream.progress(&istream, file->pFirstCluster, 0);
		file_pos = file->pFirstCluster;
	}
	virtual ~file_reader() override
	{
		fclose(mfile_handle);
	}
	int read_frame(frame& frame)
	{
		if (frame.data) {
			release_frame(frame);
		}
		unsigned int flags;
		int err = mkv_ReadFrame(file,0,&frame.track,&frame.start,&frame.end,&file_pos,&frame.size,(void**)&frame.data,&flags);
		if(err)
			return err;
		if(flags&FRAME_KF)
			frame.key_frame = true;
		else
			frame.key_frame = false;
		return 0;
	}
	void release_frame(frame& frame)
	{
		istream.releaseref(&istream,frame.data);
		frame.data=nullptr;
	}
private:
	static const char* geterror(InputStream* cc) noexcept
	{
		return "dummy error";
	}
	static filepos_t getfilesize(InputStream* cc) noexcept
	{
		__int64 cur = _ftelli64(((file_reader*)cc->ptr)->mfile_handle);
		fseek(((file_reader*)cc->ptr)->mfile_handle, 0L, SEEK_END);
		__int64 size = _ftelli64(((file_reader*)cc->ptr)->mfile_handle);
		_fseeki64(((file_reader*)cc->ptr)->mfile_handle, cur, SEEK_SET);
		return size;
	}
	static int ioread(InputStream* inf, void* buffer, int count) noexcept
	{
		return fread(buffer, 1, count, ((file_reader*)inf->ptr)->mfile_handle);
	}
	static int ioreadch(InputStream* inf) noexcept
	{
		char ch;
		fread(&ch, 1, 1, ((file_reader*)inf->ptr)->mfile_handle);
		return ch;
	}
	static void ioseek(InputStream* inf, longlong wher, int how) noexcept
	{
		_fseeki64(((file_reader*)inf->ptr)->mfile_handle, wher, how);
	}
	static filepos_t iotell(InputStream* inf) noexcept
	{
		return _ftelli64(((file_reader*)inf->ptr)->mfile_handle);
	}
	static void* makeref(InputStream* inf, int count)
	{
		file_reader& reading = *(file_reader*)inf->ptr;
		std::vector<char> temp(count);
		fread(&temp[0], 1, count, reading.mfile_handle);
		reading.buffers.emplace_back(std::move(temp));
		return &reading.buffers.back()[0];
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
		return _fseeki64(((file_reader*)inf->ptr)->mfile_handle, cur, SEEK_SET);
	}
	static int read(InputStream* inf, filepos_t pos, void* buffer, size_t count) noexcept
	{
		__int64 cur = _ftelli64(((file_reader*)inf->ptr)->mfile_handle);
		fseek(((file_reader*)inf->ptr)->mfile_handle, pos, SEEK_SET);
		uint64_t read = fread(buffer, 1, count, ((file_reader*)inf->ptr)->mfile_handle);
		_fseeki64(((file_reader*)inf->ptr)->mfile_handle, cur, SEEK_SET);
		return read;
	}
	static void releaseref(InputStream* inf, void* ref)
	{
		file_reader& reading = *(file_reader*)inf->ptr;
		for (uint64_t i = 0; i < reading.buffers.size(); ++i) {
			if (&reading.buffers[i][0] == ref) {
				for (uint64_t j = i; j < reading.buffers.size() - 1; ++j) {
					reading.buffers[j] = std::move(reading.buffers[j + 1]);
				}
				//need this for valid buffer list.
				reading.buffers.pop_back();
				break;
			}
		}
	}
};

//A pin that has only one input and one
//output. Typically a decoder or encoder
//or resampler, etc.
class media_transform : public media_pin {
public:
	enum class media_type : int {
		MTYPE_NOTSUPPORTED,
		MTYPE_VIDEO,
		MTYPE_AUDIO,
		MTYPE_LAST
	};
	//composing could be done bitwise
	media_type media_type_;
	union stream_info  {
		struct video_info {
			enum class color_space {
				CS_UNKNOWN = 0,   /**< Unknown */
				CS_BT_601 = 1,    /**< BT.601 */
				CS_BT_709 = 2,    /**< BT.709 */
				CS_SMPTE_170 = 3, /**< SMPTE.170 */
				CS_SMPTE_240 = 4, /**< SMPTE.240 */
				CS_BT_2020 = 5,   /**< BT.2020 */
				CS_RESERVED = 6,  /**< Reserved */
				CS_SRGB = 7       /**< sRGB */
			};
			enum class color_range {
				CR_STUDIO_RANGE = 0, /**< Y [16..235], UV [16..240] */
				CR_FULL_RANGE = 1    /**< YUV/RGB [0..255] */
			};
			enum class video_codec {
				VCODEC_NOTSUPPORTED,
				VCODEC_RAW,
				VCODEC_VP8,
				VCODEC_VP9,
				VCODEC_LAST
			} codec;
			color_space space;
			color_range range;
			enum subsample_location : uint8_t {
				SUBSAMP_CENTER,
				SUBSAMP_LEFT_CORNER
			};
			uint8_t subsample_horiz, subsample_vert;
			subsample_location location;
			int bitdepth, invert_uv;
			unsigned width, height;
		};
		struct audio_info {
			static constexpr int64_t CH_FRONT_LEFT            = 0x00000001;
			static constexpr int64_t CH_FRONT_RIGHT           = 0x00000002;
			static constexpr int64_t CH_FRONT_CENTER          = 0x00000004;
			static constexpr int64_t CH_LOW_FREQUENCY         = 0x00000008;
			static constexpr int64_t CH_BACK_LEFT             = 0x00000010;
			static constexpr int64_t CH_BACK_RIGHT            = 0x00000020;
			static constexpr int64_t CH_FRONT_LEFT_OF_CENTER  = 0x00000040;
			static constexpr int64_t CH_FRONT_RIGHT_OF_CENTER = 0x00000080;
			static constexpr int64_t CH_BACK_CENTER           = 0x00000100;
			static constexpr int64_t CH_SIDE_LEFT             = 0x00000200;
			static constexpr int64_t CH_SIDE_RIGHT            = 0x00000400;
			static constexpr int64_t CH_TOP_CENTER            = 0x00000800;
			static constexpr int64_t CH_TOP_FRONT_LEFT        = 0x00001000;
			static constexpr int64_t CH_TOP_FRONT_CENTER      = 0x00002000;
			static constexpr int64_t CH_TOP_FRONT_RIGHT       = 0x00004000;
			static constexpr int64_t CH_TOP_BACK_LEFT         = 0x00008000;
			static constexpr int64_t CH_TOP_BACK_CENTER       = 0x00010000;
			static constexpr int64_t CH_TOP_BACK_RIGHT        = 0x00020000;
			static constexpr int64_t CH_STEREO_LEFT           = 0x20000000;  ///< Stereo downmix.
			static constexpr int64_t CH_STEREO_RIGHT          = 0x40000000;  ///< Stereo downmix.
			static constexpr int64_t CH_WIDE_LEFT             = 0x0000000080000000ULL;
			static constexpr int64_t CH_WIDE_RIGHT            = 0x0000000100000000ULL;
			static constexpr int64_t CH_SURROUND_DIRECT_LEFT  = 0x0000000200000000ULL;
			static constexpr int64_t CH_SURROUND_DIRECT_RIGHT = 0x0000000400000000ULL;
			static constexpr int64_t CH_LOW_FREQUENCY_2       = 0x0000000800000000ULL;
			enum class channel_layout : int64_t {
				CH_LAYOUT_NONE              = 0,
				CH_LAYOUT_MONO              = (CH_FRONT_CENTER),
				CH_LAYOUT_STEREO            = (CH_FRONT_LEFT|CH_FRONT_RIGHT),
				CH_LAYOUT_2POINT1           = (CH_LAYOUT_STEREO|CH_LOW_FREQUENCY),
				CH_LAYOUT_2_1               = (CH_LAYOUT_STEREO|CH_BACK_CENTER),
				CH_LAYOUT_SURROUND          = (CH_LAYOUT_STEREO|CH_FRONT_CENTER),
				CH_LAYOUT_3POINT1           = (CH_LAYOUT_SURROUND|CH_LOW_FREQUENCY),
				CH_LAYOUT_4POINT0           = (CH_LAYOUT_SURROUND|CH_BACK_CENTER),
				CH_LAYOUT_4POINT1           = (CH_LAYOUT_4POINT0|CH_LOW_FREQUENCY),
				CH_LAYOUT_2_2               = (CH_LAYOUT_STEREO|CH_SIDE_LEFT|CH_SIDE_RIGHT),
				CH_LAYOUT_QUAD              = (CH_LAYOUT_STEREO|CH_BACK_LEFT|CH_BACK_RIGHT),
				CH_LAYOUT_5POINT0           = (CH_LAYOUT_SURROUND|CH_SIDE_LEFT|CH_SIDE_RIGHT),
				CH_LAYOUT_5POINT1           = (CH_LAYOUT_5POINT0|CH_LOW_FREQUENCY),
				CH_LAYOUT_5POINT0_BACK      = (CH_LAYOUT_SURROUND|CH_BACK_LEFT|CH_BACK_RIGHT),
				CH_LAYOUT_5POINT1_BACK      = (CH_LAYOUT_5POINT0_BACK|CH_LOW_FREQUENCY),
				CH_LAYOUT_6POINT0           = (CH_LAYOUT_5POINT0|CH_BACK_CENTER),
				CH_LAYOUT_6POINT0_FRONT     = (CH_LAYOUT_2_2|CH_FRONT_LEFT_OF_CENTER|CH_FRONT_RIGHT_OF_CENTER),
				CH_LAYOUT_HEXAGONAL         = (CH_LAYOUT_5POINT0_BACK|CH_BACK_CENTER),
				CH_LAYOUT_6POINT1           = (CH_LAYOUT_5POINT1|CH_BACK_CENTER),
				CH_LAYOUT_6POINT1_BACK      = (CH_LAYOUT_5POINT1_BACK|CH_BACK_CENTER),
				CH_LAYOUT_6POINT1_FRONT     = (CH_LAYOUT_6POINT0_FRONT|CH_LOW_FREQUENCY),
				CH_LAYOUT_7POINT0           = (CH_LAYOUT_5POINT0|CH_BACK_LEFT|CH_BACK_RIGHT),
				CH_LAYOUT_7POINT0_FRONT     = (CH_LAYOUT_5POINT0|CH_FRONT_LEFT_OF_CENTER|CH_FRONT_RIGHT_OF_CENTER),
				CH_LAYOUT_7POINT1           = (CH_LAYOUT_5POINT1|CH_BACK_LEFT|CH_BACK_RIGHT),
				CH_LAYOUT_7POINT1_WIDE      = (CH_LAYOUT_5POINT1|CH_FRONT_LEFT_OF_CENTER|CH_FRONT_RIGHT_OF_CENTER),
				CH_LAYOUT_7POINT1_WIDE_BACK = (CH_LAYOUT_5POINT1_BACK|CH_FRONT_LEFT_OF_CENTER|CH_FRONT_RIGHT_OF_CENTER),
				CH_LAYOUT_OCTAGONAL         = (CH_LAYOUT_5POINT0|CH_BACK_LEFT|CH_BACK_CENTER|CH_BACK_RIGHT),
				CH_LAYOUT_HEXADECAGONAL     = (CH_LAYOUT_OCTAGONAL|CH_WIDE_LEFT|CH_WIDE_RIGHT|CH_TOP_BACK_LEFT|CH_TOP_BACK_RIGHT|CH_TOP_BACK_CENTER|CH_TOP_FRONT_CENTER|CH_TOP_FRONT_LEFT|CH_TOP_FRONT_RIGHT),
				CH_LAYOUT_STEREO_DOWNMIX    = (CH_STEREO_LEFT|CH_STEREO_RIGHT)
			};
			enum class audio_codec {
				ACODEC_NOTSUPPORTED,
				ACODEC_PCM,
				ACODEC_FLAC,
				ACODEC_OPUS,
				ACODEC_VORBIS,
				ACODEC_LAST
			} codec;
			enum MatrixEncoding {
				MATRIX_ENCODING_NONE,
				MATRIX_ENCODING_DOLBY,
				MATRIX_ENCODING_DPLII,
				MATRIX_ENCODING_DPLIIX,
				MATRIX_ENCODING_DPLIIZ,
				MATRIX_ENCODING_DOLBYEX,
				MATRIX_ENCODING_DOLBYHEADPHONE,
				MATRIX_ENCODING_NB
			} matrix;
			channel_layout layout;
			double Hz;
			bool planar;
		};
		video_info video;
		audio_info audio;
		stream_info(video_info info){
			new(this)video_info(info);
		}
		stream_info(audio_info info){
			new(this)audio_info(info);
		}
		stream_info(){}
	};
	stream_info in_info;
	stream_info out_info;
	media_transform(stream_info& in, stream_info& out, media_type _media_type):
		media_pin(1, 1), media_type_(_media_type), in_info(in),out_info(out) {}
	virtual ~media_transform() {}
};

class decoder : public media_transform {
public:
	enum class decoder_type {
		DECODER_NONE,
		DECODER_RAW_VIDEO,
		DECODER_LIBVPX_VP8,
		DECODER_LIBVPX_VP9,
		DECODER_VIDEO_LAST,
		DECODER_RAW_PCM,
		DECODER_LIBFLAC,
		DECODER_LIBVORBIS,
		DECODER_LIBOPUS,
		DECODER_AUDIO_LAST
	};
	decoder(stream_info&& in, stream_info&& out, media_type m_type, decoder_type type): media_transform(in,out,m_type), dec_type(type){}
	virtual ~decoder() {};
	virtual pin_error Probe(buffer_desc* inputs) = 0;
	const decoder_type dec_type;
};

class libvpx_vp9_decoder: public decoder {
	const vpx_codec_iface_t* iface;
	vpx_codec_ctx ctx;
	//not needed for vp9
	vpx_codec_iter_t iter;
	vpx_image_t* probe_frame = nullptr;
	std::once_flag flag;
public:
	const vpx_codec_dec_cfg cfg;
	const int mflags;
	const int init_err;
	libvpx_vp9_decoder(stream_info::video_info& in, stream_info::video_info& out, unsigned thread = 1, int flags = 0):
		decoder(stream_info{in}, stream_info{out}, media_type::MTYPE_VIDEO,decoder_type::DECODER_LIBVPX_VP9), iter{nullptr},
		iface(vpx_codec_vp9_dx()), cfg{thread, in.width, in.height}, mflags(flags),
		init_err{vpx_codec_dec_init(&ctx, iface, &cfg, mflags)}{
	};
	virtual ~libvpx_vp9_decoder() override final
	{
		vpx_codec_destroy(&ctx);
	}
	virtual const char* strerr(pin_error erorr) override final
	{
		return vpx_codec_error_detail(&ctx);
	}
	virtual pin_error OnInput(buffer_desc* packet) override final
	{
		if(vpx_codec_decode(&ctx,(uint8_t*)packet->data,packet->size,0,0))
			return pin_error::E_FATAL;
		return pin_error::NO_ERROR;
	}
	virtual pin_error OnOutput(buffer_desc* frame) override final
	{
		[[unlikely]] if (probe_frame) {
			frame->data = probe_frame;
			probe_frame = nullptr;
			return pin_error::NO_ERROR;
		}
		vpx_image_t* img = vpx_codec_get_frame(&ctx, &iter);
		std::call_once(flag,
			[this,img](){
				switch (img->cs) {
					case VPX_CS_UNKNOWN:
						in_info.video.space = stream_info::video_info::color_space::CS_UNKNOWN;
						out_info.video.space = stream_info::video_info::color_space::CS_UNKNOWN;
						break;
					case VPX_CS_BT_601:
						in_info.video.space = stream_info::video_info::color_space::CS_BT_601;
						out_info.video.space = stream_info::video_info::color_space::CS_BT_601;
						break;
					case VPX_CS_BT_709:
						in_info.video.space = stream_info::video_info::color_space::CS_BT_709;
						out_info.video.space = stream_info::video_info::color_space::CS_BT_709;
						break;
					case VPX_CS_BT_2020:
						in_info.video.space = stream_info::video_info::color_space::CS_BT_2020;
						out_info.video.space = stream_info::video_info::color_space::CS_BT_2020;
						break;
					case VPX_CS_SMPTE_170:
						in_info.video.space = stream_info::video_info::color_space::CS_SMPTE_170;
						out_info.video.space = stream_info::video_info::color_space::CS_SMPTE_170;
						break;
					case VPX_CS_SMPTE_240:
						in_info.video.space = stream_info::video_info::color_space::CS_SMPTE_240;
						out_info.video.space = stream_info::video_info::color_space::CS_SMPTE_240;
						break;
					case VPX_CS_SRGB:
						in_info.video.space = stream_info::video_info::color_space::CS_SRGB;
						out_info.video.space = stream_info::video_info::color_space::CS_SRGB;
						break;
				}
				switch (img->fmt) {
					case VPX_IMG_FMT_NONE:
						in_info.video.bitdepth = 8;
						in_info.video.subsample_vert = 1;
						in_info.video.subsample_horiz = 1;
						in_info.video.invert_uv = 0;
						out_info.video.bitdepth = 8;
						out_info.video.subsample_vert = 1;
						out_info.video.subsample_horiz = 1;
						out_info.video.invert_uv = 0;
						break;
					case VPX_IMG_FMT_YV12:
						in_info.video.bitdepth = 8;
						in_info.video.subsample_vert = 1;
						in_info.video.subsample_horiz = 1;
						in_info.video.invert_uv = 1;
						out_info.video.bitdepth = 8;
						out_info.video.subsample_vert = 1;
						out_info.video.subsample_horiz = 1;
						out_info.video.invert_uv = 1;
						break;
					case VPX_IMG_FMT_I420:
						in_info.video.bitdepth = 8;
						in_info.video.subsample_vert = 1;
						in_info.video.subsample_horiz = 1;
						in_info.video.invert_uv = 0;
						out_info.video.bitdepth = 8;
						out_info.video.subsample_vert = 1;
						out_info.video.subsample_horiz = 1;
						out_info.video.invert_uv = 0;
						break;
					case VPX_IMG_FMT_I422:
						in_info.video.bitdepth = 8;
						in_info.video.subsample_vert = 0;
						in_info.video.subsample_horiz = 1;
						in_info.video.invert_uv = 0;
						out_info.video.bitdepth = 8;
						out_info.video.subsample_vert = 0;
						out_info.video.subsample_horiz = 1;
						out_info.video.invert_uv = 0;
						break;
					case VPX_IMG_FMT_I444:
						in_info.video.bitdepth = 8;
						in_info.video.subsample_vert = 0;
						in_info.video.subsample_horiz = 0;
						in_info.video.invert_uv = 0;
						out_info.video.bitdepth = 8;
						out_info.video.subsample_vert = 0;
						out_info.video.subsample_horiz = 0;
						out_info.video.invert_uv = 0;
						break;
					case VPX_IMG_FMT_I440:
						in_info.video.bitdepth = 8;
						in_info.video.subsample_vert = 1;
						in_info.video.subsample_horiz = 0;
						in_info.video.invert_uv = 0;
						out_info.video.bitdepth = 8;
						out_info.video.subsample_vert = 1;
						out_info.video.subsample_horiz = 0;
						out_info.video.invert_uv = 0;
						break;
					case VPX_IMG_FMT_I42016:
						in_info.video.bitdepth = 16;
						in_info.video.subsample_vert = 1;
						in_info.video.subsample_horiz = 1;
						in_info.video.invert_uv = 0;
						out_info.video.bitdepth = 16;
						out_info.video.subsample_vert = 1;
						out_info.video.subsample_horiz = 1;
						out_info.video.invert_uv = 0;
						break;
					case VPX_IMG_FMT_I42216:
						in_info.video.bitdepth = 16;
						in_info.video.subsample_vert = 0;
						in_info.video.subsample_horiz = 1;
						in_info.video.invert_uv = 0;
						out_info.video.bitdepth = 16;
						out_info.video.subsample_vert = 0;
						out_info.video.subsample_horiz = 1;
						out_info.video.invert_uv = 0;
						break;
					case VPX_IMG_FMT_I44416:
						in_info.video.bitdepth = 16;
						in_info.video.subsample_vert = 0;
						in_info.video.subsample_horiz = 0;
						in_info.video.invert_uv = 0;
						out_info.video.bitdepth = 16;
						out_info.video.subsample_vert = 0;
						out_info.video.subsample_horiz = 0;
						out_info.video.invert_uv = 0;
						break;
					case VPX_IMG_FMT_I44016:
						in_info.video.bitdepth = 16;
						in_info.video.subsample_vert = 1;
						in_info.video.subsample_horiz = 0;
						in_info.video.invert_uv = 0;
						out_info.video.bitdepth = 16;
						out_info.video.subsample_vert = 1;
						out_info.video.subsample_horiz = 0;
						out_info.video.invert_uv = 0;
						break;
				}
				switch (img->range) {
					case VPX_CR_STUDIO_RANGE:
						in_info.video.range = stream_info::video_info::color_range::CR_STUDIO_RANGE;
						out_info.video.range = stream_info::video_info::color_range::CR_STUDIO_RANGE;
						break;
					case VPX_CR_FULL_RANGE:
						in_info.video.range = stream_info::video_info::color_range::CR_FULL_RANGE;
						out_info.video.range = stream_info::video_info::color_range::CR_FULL_RANGE;
						break;
				}
			}
		);
		frame->data = img;
		return pin_error::E_AGAIN;
	}
	virtual pin_error Probe(buffer_desc* inputs) override final
	{
		vpx_image_t* img = vpx_codec_get_frame(&ctx, &iter);
		if(!img)
			return pin_error::E_FATAL;
		std::call_once(flag,
			[this, img]() {
				probe_frame = img;
				switch (img->cs) {
					case VPX_CS_UNKNOWN:
						in_info.video.space = stream_info::video_info::color_space::CS_UNKNOWN;
						out_info.video.space = stream_info::video_info::color_space::CS_UNKNOWN;
						break;
					case VPX_CS_BT_601:
						in_info.video.space = stream_info::video_info::color_space::CS_BT_601;
						out_info.video.space = stream_info::video_info::color_space::CS_BT_601;
						break;
					case VPX_CS_BT_709:
						in_info.video.space = stream_info::video_info::color_space::CS_BT_709;
						out_info.video.space = stream_info::video_info::color_space::CS_BT_709;
						break;
					case VPX_CS_BT_2020:
						in_info.video.space = stream_info::video_info::color_space::CS_BT_2020;
						out_info.video.space = stream_info::video_info::color_space::CS_BT_2020;
						break;
					case VPX_CS_SMPTE_170:
						in_info.video.space = stream_info::video_info::color_space::CS_SMPTE_170;
						out_info.video.space = stream_info::video_info::color_space::CS_SMPTE_170;
						break;
					case VPX_CS_SMPTE_240:
						in_info.video.space = stream_info::video_info::color_space::CS_SMPTE_240;
						out_info.video.space = stream_info::video_info::color_space::CS_SMPTE_240;
						break;
					case VPX_CS_SRGB:
						in_info.video.space = stream_info::video_info::color_space::CS_SRGB;
						out_info.video.space = stream_info::video_info::color_space::CS_SRGB;
						break;
				}
				switch (img->fmt) {
					case VPX_IMG_FMT_NONE:
						in_info.video.bitdepth = 8;
						in_info.video.subsample_vert = 1;
						in_info.video.subsample_horiz = 1;
						in_info.video.invert_uv = 0;
						out_info.video.bitdepth = 8;
						out_info.video.subsample_vert = 1;
						out_info.video.subsample_horiz = 1;
						out_info.video.invert_uv = 0;
						break;
					case VPX_IMG_FMT_YV12:
						in_info.video.bitdepth = 8;
						in_info.video.subsample_vert = 1;
						in_info.video.subsample_horiz = 1;
						in_info.video.invert_uv = 1;
						out_info.video.bitdepth = 8;
						out_info.video.subsample_vert = 1;
						out_info.video.subsample_horiz = 1;
						out_info.video.invert_uv = 1;
						break;
					case VPX_IMG_FMT_I420:
						in_info.video.bitdepth = 8;
						in_info.video.subsample_vert = 1;
						in_info.video.subsample_horiz = 1;
						in_info.video.invert_uv = 0;
						out_info.video.bitdepth = 8;
						out_info.video.subsample_vert = 1;
						out_info.video.subsample_horiz = 1;
						out_info.video.invert_uv = 0;
						break;
					case VPX_IMG_FMT_I422:
						in_info.video.bitdepth = 8;
						in_info.video.subsample_vert = 0;
						in_info.video.subsample_horiz = 1;
						in_info.video.invert_uv = 0;
						out_info.video.bitdepth = 8;
						out_info.video.subsample_vert = 0;
						out_info.video.subsample_horiz = 1;
						out_info.video.invert_uv = 0;
						break;
					case VPX_IMG_FMT_I444:
						in_info.video.bitdepth = 8;
						in_info.video.subsample_vert = 0;
						in_info.video.subsample_horiz = 0;
						in_info.video.invert_uv = 0;
						out_info.video.bitdepth = 8;
						out_info.video.subsample_vert = 0;
						out_info.video.subsample_horiz = 0;
						out_info.video.invert_uv = 0;
						break;
					case VPX_IMG_FMT_I440:
						in_info.video.bitdepth = 8;
						in_info.video.subsample_vert = 1;
						in_info.video.subsample_horiz = 0;
						in_info.video.invert_uv = 0;
						out_info.video.bitdepth = 8;
						out_info.video.subsample_vert = 1;
						out_info.video.subsample_horiz = 0;
						out_info.video.invert_uv = 0;
						break;
					case VPX_IMG_FMT_I42016:
						in_info.video.bitdepth = 16;
						in_info.video.subsample_vert = 1;
						in_info.video.subsample_horiz = 1;
						in_info.video.invert_uv = 0;
						out_info.video.bitdepth = 16;
						out_info.video.subsample_vert = 1;
						out_info.video.subsample_horiz = 1;
						out_info.video.invert_uv = 0;
						break;
					case VPX_IMG_FMT_I42216:
						in_info.video.bitdepth = 16;
						in_info.video.subsample_vert = 0;
						in_info.video.subsample_horiz = 1;
						in_info.video.invert_uv = 0;
						out_info.video.bitdepth = 16;
						out_info.video.subsample_vert = 0;
						out_info.video.subsample_horiz = 1;
						out_info.video.invert_uv = 0;
						break;
					case VPX_IMG_FMT_I44416:
						in_info.video.bitdepth = 16;
						in_info.video.subsample_vert = 0;
						in_info.video.subsample_horiz = 0;
						in_info.video.invert_uv = 0;
						out_info.video.bitdepth = 16;
						out_info.video.subsample_vert = 0;
						out_info.video.subsample_horiz = 0;
						out_info.video.invert_uv = 0;
						break;
					case VPX_IMG_FMT_I44016:
						in_info.video.bitdepth = 16;
						in_info.video.subsample_vert = 1;
						in_info.video.subsample_horiz = 0;
						in_info.video.invert_uv = 0;
						out_info.video.bitdepth = 16;
						out_info.video.subsample_vert = 1;
						out_info.video.subsample_horiz = 0;
						out_info.video.invert_uv = 0;
						break;
				}
				switch (img->range) {
					case VPX_CR_STUDIO_RANGE:
						in_info.video.range = stream_info::video_info::color_range::CR_STUDIO_RANGE;
						out_info.video.range = stream_info::video_info::color_range::CR_STUDIO_RANGE;
						break;
					case VPX_CR_FULL_RANGE:
						in_info.video.range = stream_info::video_info::color_range::CR_FULL_RANGE;
						out_info.video.range = stream_info::video_info::color_range::CR_FULL_RANGE;
						break;
				}
			}
		);
		return pin_error::NO_ERROR;
	}
};

class libopus_opus_decoder: public decoder {
	OpusDecoder* handle;
	int err;
	const int chan;
	const int32_t freq;
	float buffer[5760*2]{};
	int nb_samples = 0;
public:
	//libopus only supports interleaved stereo and mono (can be downmix)
	//libopus only supports frequencies of 48000, 24000, 16000, 12000, and 8000
	libopus_opus_decoder(stream_info::audio_info& in, stream_info::audio_info& out):
		decoder(stream_info{in}, stream_info{out}, media_type::MTYPE_AUDIO,decoder_type::DECODER_LIBOPUS),
		chan(out.layout == stream_info::audio_info::channel_layout::CH_LAYOUT_STEREO?2:1),
		freq(out.Hz),err(0),handle(opus_decoder_create(out.Hz, 
			out.layout == stream_info::audio_info::channel_layout::CH_LAYOUT_STEREO ? 2 : 1, &err)){}
	virtual ~libopus_opus_decoder() override final {
		opus_decoder_destroy(handle);
	}
	virtual const char* strerr(pin_error erorr) override final {
		return opus_strerror(int(erorr));
	}
	virtual pin_error OnInput(buffer_desc* packet) override final {
		nb_samples = opus_decoder_get_nb_samples(handle, (uint8_t*)packet->data, packet->size);
		int err = opus_decode_float(handle, (uint8_t*)packet->data, packet->size, buffer, nb_samples, 0);
		if(err) nb_samples = 0;
		return (pin_error)err;
	}
	virtual pin_error OnOutput(buffer_desc* packet) override final {
		if(!nb_samples)
			return pin_error::E_AGAIN;
		packet->data = buffer;
		packet->size = nb_samples;
		nb_samples = 0;
		return pin_error::NO_ERROR;
	}
	virtual pin_error Probe(buffer_desc* inputs) override final {
		return pin_error::NO_ERROR;
	}
};

int main()
{
	{
		file_reader reader("D:\\GamePlatforms\\Steam\\steamapps\\common\\Neptunia Rebirth1\\data\\MOVIE00000\\movie\\direct.webm");
		int num_tracks = mkv_GetNumTracks(reader.file);
		if(!num_tracks) return 0;
		decoder** decoders = new decoder*[num_tracks]{};
		for (int i = 0; i < num_tracks; ++i) {
			TrackInfo* info = mkv_GetTrackInfo(reader.file,i);
			if (info->Type == TRACK_TYPE_VIDEO) {
				media_transform::stream_info::video_info sinfo;
				sinfo.width = info->AV.Video.PixelWidth;
				sinfo.height = info->AV.Video.PixelHeight;
				media_transform::stream_info::video_info oinfo;
				oinfo.width = info->AV.Video.PixelWidth;
				oinfo.height = info->AV.Video.PixelHeight;
				oinfo.codec = media_transform::stream_info::video_info::video_codec::VCODEC_RAW;
				sinfo.subsample_horiz = 1;
				sinfo.subsample_vert = 1;
				sinfo.bitdepth = 8;
				oinfo.subsample_horiz = 1;
				oinfo.subsample_vert = 1;
				oinfo.bitdepth = 8;
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
					//only 2-1 subsampling supported
					if (info->AV.Video.Colour.CbSubsamplingHorz) {
						info->AV.Video.Colour.CbSubsamplingHorz = 1;
						info->AV.Video.Colour.ChromaSubsamplingHorz = 1;
					}
					if (info->AV.Video.Colour.CbSubsamplingVert) {
						info->AV.Video.Colour.CbSubsamplingVert = 1;
						info->AV.Video.Colour.ChromaSubsamplingVert = 1;
					}
					sinfo.subsample_horiz = info->AV.Video.Colour.CbSubsamplingHorz;
					sinfo.subsample_vert = info->AV.Video.Colour.CbSubsamplingVert;
					oinfo.subsample_horiz = info->AV.Video.Colour.CbSubsamplingHorz;
					oinfo.subsample_vert = info->AV.Video.Colour.CbSubsamplingVert;
				}
				if (info->AV.Video.Colour.BitsPerChannel) {
					sinfo.bitdepth = info->AV.Video.Colour.BitsPerChannel;
					oinfo.bitdepth = info->AV.Video.Colour.BitsPerChannel;
				}
				if (info->AV.Video.Colour.ChromaSitingHorz && info->AV.Video.Colour.ChromaSitingVert) {
					sinfo.location = media_transform::stream_info::video_info::subsample_location::SUBSAMP_LEFT_CORNER;
				}
				if (strcmp(info->CodecID, "V_VP9") == 0) {
					sinfo.codec = media_transform::stream_info::video_info::video_codec::VCODEC_VP9;
					decoders[i] = new libvpx_vp9_decoder(sinfo, oinfo, 4);
				}
			}
			if (info->Type == TRACK_TYPE_AUDIO) {
				media_transform::stream_info::audio_info sinfo;
				media_transform::stream_info::audio_info oinfo;
				oinfo.codec = media_transform::stream_info::audio_info::audio_codec::ACODEC_PCM;
				sinfo.Hz = 48000;
				oinfo.Hz = 48000;
				sinfo.matrix = media_transform::stream_info::audio_info::MatrixEncoding::MATRIX_ENCODING_NONE;
				oinfo.matrix = media_transform::stream_info::audio_info::MatrixEncoding::MATRIX_ENCODING_NONE;
				sinfo.planar = true;
				oinfo.planar = true;
				if (info->AV.Audio.SamplingFreq) {
					sinfo.Hz = info->AV.Audio.SamplingFreq;
					oinfo.Hz = info->AV.Audio.SamplingFreq;
				}
				if (info->AV.Audio.OutputSamplingFreq) {
					oinfo.Hz = info->AV.Audio.OutputSamplingFreq;
				}
				switch (info->AV.Audio.Channels) {
					case 0:
						sinfo.layout = media_transform::stream_info::audio_info::channel_layout::CH_LAYOUT_MONO;
						oinfo.layout = media_transform::stream_info::audio_info::channel_layout::CH_LAYOUT_MONO;
						break;
					case 1:
						sinfo.layout = media_transform::stream_info::audio_info::channel_layout::CH_LAYOUT_MONO;
						oinfo.layout = media_transform::stream_info::audio_info::channel_layout::CH_LAYOUT_MONO;
						break;
					case 2:
						sinfo.layout = media_transform::stream_info::audio_info::channel_layout::CH_LAYOUT_STEREO;
						oinfo.layout = media_transform::stream_info::audio_info::channel_layout::CH_LAYOUT_STEREO;
						break;
					default:
						sinfo.layout = media_transform::stream_info::audio_info::channel_layout::CH_LAYOUT_STEREO;
						oinfo.layout = media_transform::stream_info::audio_info::channel_layout::CH_LAYOUT_STEREO;
				}
				if (strcmp(info->CodecID, "A_OPUS") == 0) {
					sinfo.Hz = 48000;
					oinfo.Hz = 48000;
					sinfo.layout = media_transform::stream_info::audio_info::channel_layout::CH_LAYOUT_STEREO;
					oinfo.layout = media_transform::stream_info::audio_info::channel_layout::CH_LAYOUT_STEREO;
					sinfo.matrix = media_transform::stream_info::audio_info::MatrixEncoding::MATRIX_ENCODING_NONE;
					oinfo.matrix = media_transform::stream_info::audio_info::MatrixEncoding::MATRIX_ENCODING_NONE;
					sinfo.planar = false;
					oinfo.planar = false;
					sinfo.codec = media_transform::stream_info::audio_info::audio_codec::ACODEC_OPUS;
					decoders[i] = new libopus_opus_decoder(sinfo, sinfo);
				}
			}
		}
		file_reader::frame frame{};
		while (!reader.read_frame(frame)) {
			if (decoders[frame.track]) {
				buffer_desc desc;
				desc.data = frame.data;
				desc.size = frame.size;
				decoders[frame.track]->OnInput(&desc);
				buffer_desc out;
				while(decoders[frame.track]->OnOutput(&out)==media_pin::pin_error::NO_ERROR);
			}
			reader.release_frame(frame);
		}
		for (int i = 0; i < num_tracks; ++i) {
			if (decoders[i]) {
				delete decoders[i];
			}
		}
		delete[] decoders;
	}
	return 0;
}

int _main()
{
	while(true) _main();
}