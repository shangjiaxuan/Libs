#pragma once

#include <cstdint>

class media_buffer_node;

struct stream_desc {
	const size_t desc_size = sizeof(stream_desc);
	enum major_type: uint32_t {
		MTYPE_NONE,
		MTYPE_VIDEO,
		MTYPE_AUDIO,
		MTYPE_LAST
	};
	major_type type{};
	struct video_info {
		enum color_space {
			CS_UNKNOWN = 0,   /**< Unknown */
			CS_BT_601 = 1,    /**< BT.601 */
			CS_BT_709 = 2,    /**< BT.709 */
			CS_SMPTE_170 = 3, /**< SMPTE.170 */
			CS_SMPTE_240 = 4, /**< SMPTE.240 */
			CS_BT_2020 = 5,   /**< BT.2020 */
			CS_RESERVED = 6,  /**< Reserved */
			CS_SRGB = 7       /**< sRGB */
		};
		enum color_range {
			CR_STUDIO_RANGE = 0, /**< Y [16..235], UV [16..240] */
			CR_FULL_RANGE = 1    /**< YUV/RGB [0..255] */
		};
		enum video_codec {
			VCODEC_NONE,
			VCODEC_NOTSUPPORTED,
			VCODEC_RAW,
			VCODEC_VP8,
			VCODEC_VP9,
			VCODEC_LAST
		} codec;
		color_space space;
		color_range range;
		enum subsample_location: uint8_t {
			SUBSAMP_CENTER,
			SUBSAMP_LEFT_CORNER
		};
		uint8_t subsample_horiz, subsample_vert;
		subsample_location location;
		int bitdepth, invert_uv;
		unsigned width, height;
	};
	struct audio_info {
		static constexpr int64_t CH_FRONT_LEFT = 0x00000001;
		static constexpr int64_t CH_FRONT_RIGHT = 0x00000002;
		static constexpr int64_t CH_FRONT_CENTER = 0x00000004;
		static constexpr int64_t CH_LOW_FREQUENCY = 0x00000008;
		static constexpr int64_t CH_BACK_LEFT = 0x00000010;
		static constexpr int64_t CH_BACK_RIGHT = 0x00000020;
		static constexpr int64_t CH_FRONT_LEFT_OF_CENTER = 0x00000040;
		static constexpr int64_t CH_FRONT_RIGHT_OF_CENTER = 0x00000080;
		static constexpr int64_t CH_BACK_CENTER = 0x00000100;
		static constexpr int64_t CH_SIDE_LEFT = 0x00000200;
		static constexpr int64_t CH_SIDE_RIGHT = 0x00000400;
		static constexpr int64_t CH_TOP_CENTER = 0x00000800;
		static constexpr int64_t CH_TOP_FRONT_LEFT = 0x00001000;
		static constexpr int64_t CH_TOP_FRONT_CENTER = 0x00002000;
		static constexpr int64_t CH_TOP_FRONT_RIGHT = 0x00004000;
		static constexpr int64_t CH_TOP_BACK_LEFT = 0x00008000;
		static constexpr int64_t CH_TOP_BACK_CENTER = 0x00010000;
		static constexpr int64_t CH_TOP_BACK_RIGHT = 0x00020000;
		static constexpr int64_t CH_STEREO_LEFT = 0x20000000;  ///< Stereo downmix.
		static constexpr int64_t CH_STEREO_RIGHT = 0x40000000;  ///< Stereo downmix.
		static constexpr int64_t CH_WIDE_LEFT = 0x0000000080000000ULL;
		static constexpr int64_t CH_WIDE_RIGHT = 0x0000000100000000ULL;
		static constexpr int64_t CH_SURROUND_DIRECT_LEFT = 0x0000000200000000ULL;
		static constexpr int64_t CH_SURROUND_DIRECT_RIGHT = 0x0000000400000000ULL;
		static constexpr int64_t CH_LOW_FREQUENCY_2 = 0x0000000800000000ULL;
		enum channel_layout: int64_t {
			CH_LAYOUT_NONE = 0,
			CH_LAYOUT_MONO = (CH_FRONT_CENTER),
			CH_LAYOUT_STEREO = (CH_FRONT_LEFT | CH_FRONT_RIGHT),
			CH_LAYOUT_2POINT1 = (CH_LAYOUT_STEREO | CH_LOW_FREQUENCY),
			CH_LAYOUT_2_1 = (CH_LAYOUT_STEREO | CH_BACK_CENTER),
			CH_LAYOUT_SURROUND = (CH_LAYOUT_STEREO | CH_FRONT_CENTER),
			CH_LAYOUT_3POINT1 = (CH_LAYOUT_SURROUND | CH_LOW_FREQUENCY),
			CH_LAYOUT_4POINT0 = (CH_LAYOUT_SURROUND | CH_BACK_CENTER),
			CH_LAYOUT_4POINT1 = (CH_LAYOUT_4POINT0 | CH_LOW_FREQUENCY),
			CH_LAYOUT_2_2 = (CH_LAYOUT_STEREO | CH_SIDE_LEFT | CH_SIDE_RIGHT),
			CH_LAYOUT_QUAD = (CH_LAYOUT_STEREO | CH_BACK_LEFT | CH_BACK_RIGHT),
			CH_LAYOUT_5POINT0 = (CH_LAYOUT_SURROUND | CH_SIDE_LEFT | CH_SIDE_RIGHT),
			CH_LAYOUT_5POINT1 = (CH_LAYOUT_5POINT0 | CH_LOW_FREQUENCY),
			CH_LAYOUT_5POINT0_BACK = (CH_LAYOUT_SURROUND | CH_BACK_LEFT | CH_BACK_RIGHT),
			CH_LAYOUT_5POINT1_BACK = (CH_LAYOUT_5POINT0_BACK | CH_LOW_FREQUENCY),
			CH_LAYOUT_6POINT0 = (CH_LAYOUT_5POINT0 | CH_BACK_CENTER),
			CH_LAYOUT_6POINT0_FRONT = (CH_LAYOUT_2_2 | CH_FRONT_LEFT_OF_CENTER | CH_FRONT_RIGHT_OF_CENTER),
			CH_LAYOUT_HEXAGONAL = (CH_LAYOUT_5POINT0_BACK | CH_BACK_CENTER),
			CH_LAYOUT_6POINT1 = (CH_LAYOUT_5POINT1 | CH_BACK_CENTER),
			CH_LAYOUT_6POINT1_BACK = (CH_LAYOUT_5POINT1_BACK | CH_BACK_CENTER),
			CH_LAYOUT_6POINT1_FRONT = (CH_LAYOUT_6POINT0_FRONT | CH_LOW_FREQUENCY),
			CH_LAYOUT_7POINT0 = (CH_LAYOUT_5POINT0 | CH_BACK_LEFT | CH_BACK_RIGHT),
			CH_LAYOUT_7POINT0_FRONT = (CH_LAYOUT_5POINT0 | CH_FRONT_LEFT_OF_CENTER | CH_FRONT_RIGHT_OF_CENTER),
			CH_LAYOUT_7POINT1 = (CH_LAYOUT_5POINT1 | CH_BACK_LEFT | CH_BACK_RIGHT),
			CH_LAYOUT_7POINT1_WIDE = (CH_LAYOUT_5POINT1 | CH_FRONT_LEFT_OF_CENTER | CH_FRONT_RIGHT_OF_CENTER),
			CH_LAYOUT_7POINT1_WIDE_BACK = (CH_LAYOUT_5POINT1_BACK | CH_FRONT_LEFT_OF_CENTER | CH_FRONT_RIGHT_OF_CENTER),
			CH_LAYOUT_OCTAGONAL = (CH_LAYOUT_5POINT0 | CH_BACK_LEFT | CH_BACK_CENTER | CH_BACK_RIGHT),
			CH_LAYOUT_HEXADECAGONAL = (CH_LAYOUT_OCTAGONAL | CH_WIDE_LEFT | CH_WIDE_RIGHT | CH_TOP_BACK_LEFT | CH_TOP_BACK_RIGHT | CH_TOP_BACK_CENTER | CH_TOP_FRONT_CENTER | CH_TOP_FRONT_LEFT | CH_TOP_FRONT_RIGHT),
			CH_LAYOUT_STEREO_DOWNMIX = (CH_STEREO_LEFT | CH_STEREO_RIGHT)
		};
		enum audio_codec {
			ACODEC_NONE,
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
		struct SampleFormat {
			int8_t bitdepth_minus_one : 5;
			int8_t isfloat : 1;
			int8_t isunsigned :1;
			int8_t isBE :1;
		} format;
	};
	union detailed_info {
		video_info video;
		audio_info audio;
		detailed_info(video_info info)
		{
			new(this)video_info(info);
		}
		detailed_info(audio_info info)
		{
			new(this)audio_info(info);
		}
		detailed_info() {}
	} detail{};
	const media_buffer_node* upstream{};
	const media_buffer_node* downstream{};
	struct format_detail {
		uint8_t* CodecPrivate;
		size_t CodecPrivateSize;
		size_t CodecDelay;
		union format_meta {
			struct mkv_meta {
				bool Enabled;
				bool Default;
				bool Forced;
				char Language[4];
				char* Name;
			} mkv;
		}meta;
	}format_info{};
};

struct _buffer_desc {
	//for abi and compatibility
	const uint32_t desc_size = sizeof(_buffer_desc);
	//the flag for protocol (input)
	const uint32_t type;
	//the stream the buffer is in
	const stream_desc* stream;
	//the pointer to memory or struct
	//that holds the actual data.
	void* data;
	void* ptr;
	//implemented by protocol.
	//eg.  free(_this->data) or perhaps
	//glDeleteTextures(1,(GLuint*)&_this->data)
	void (*release)(_buffer_desc* _this, void* ptr) = nullptr;
	//supplemental data and flags (depends on protocol)
	uint64_t start_time;
	uint64_t end_time;
	uint64_t data1;
	uint64_t data2;
	uint64_t data3;
};


struct _output_desc {
	//for abi and compatibility
	const uint32_t desc_size = sizeof(_output_desc);
	//the flag for protocol (input)
	const uint32_t type;
	size_t num;
	stream_desc* desc;
};

//contains either pointer to array of
//input/output descriptors or a descriptor
//itself. The one that allocates the descriptor
//is responsible fpr freeing it.
class media_buffer_node {
public:
	enum buffer_error {
		//OK = 0
		S_OK,
		//Signals end of stream, must be implemented
		E_EOF,
		//Signals need of more input, passed downstream
		E_AGAIN,
		//Not yet implemented
		E_UNIMPLEMENTED,
		//Sanity check for invalid usage
		E_INVALID_OPERATION,
		//Sanity check for invalid topology building
		E_PROTOCOL_MISMATCH
	};
protected:
	//only allocate this if final overrirde
	stream_desc* desc_out= nullptr;
	size_t num_out = 0;
	//only allocate this if final overrirde
	stream_desc* desc_in = nullptr;
	size_t num_in = 0;
	//Topology building finished
	bool running = false;
public:
	media_buffer_node(){};
	virtual int FetchBuffer(_buffer_desc& buffer) = 0;
	virtual int QueueBuffer(_buffer_desc& buffer) = 0;
	virtual int ReleaseBuffer(_buffer_desc& buffer) = 0;
	virtual int AllocBuffer(_buffer_desc& buffer) = 0;
	virtual int Flush() = 0;
	virtual int GetInputs(stream_desc *& desc, size_t& num) = 0;
	virtual int GetOutputs(stream_desc *& desc, size_t& num) = 0;
	//for building topology
//	virtual int AddUpstream() = 0;
//	virtual int AddDownstream() = 0;
	virtual ~media_buffer_node() {};
};
