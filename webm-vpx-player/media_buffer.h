#pragma once

#include "audio_info.h"
#include "video_info.h"

class media_buffer_node;

struct stream_desc {
	const size_t desc_size = sizeof(stream_desc);
	enum major_type: uint32_t {
		MTYPE_NONE,
		MTYPE_VIDEO,
		MTYPE_AUDIO,
		MTYPE_LAST
	};
	major_type type{MTYPE_NONE};
	struct video_info {
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
		video_sample_format fmt;
		unsigned width, height;
	};
	struct audio_info {
		enum channel_layout_type {
			CH_LAYOUT_NONE,
			CH_LAYOUT_MONO,
			CH_LAYOUT_STEREO,
			CH_LAYOUT_2POINT1,
			CH_LAYOUT_2_1,
			CH_LAYOUT_SURROUND,
			CH_LAYOUT_3POINT1,
			CH_LAYOUT_4POINT0,
			CH_LAYOUT_4POINT1,
			CH_LAYOUT_2_2,
			CH_LAYOUT_QUAD,
			CH_LAYOUT_5POINT0,
			CH_LAYOUT_5POINT1,
			CH_LAYOUT_5POINT0_BACK,
			CH_LAYOUT_5POINT1_BACK,
			CH_LAYOUT_6POINT0,
			CH_LAYOUT_6POINT0_FRONT,
			CH_LAYOUT_HEXAGONAL,
			CH_LAYOUT_6POINT1,
			CH_LAYOUT_6POINT1_BACK,
			CH_LAYOUT_6POINT1_FRONT,
			CH_LAYOUT_7POINT0,
			CH_LAYOUT_7POINT0_FRONT,
			CH_LAYOUT_7POINT1,
			CH_LAYOUT_7POINT1_WIDE,
			CH_LAYOUT_7POINT1_WIDE_BACK,
			CH_LAYOUT_OCTAGONAL,
			CH_LAYOUT_HEXADECAGONAL,
			CH_LAYOUT_STEREO_DOWNMIX
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
		SampleFormat format;
		channel_layout layout;
		double Hz;
		bool planar;
		static const channel_layout* GetDefaultLayoutFromCount(int ch_count);
		static const channel_layout* GetBuiltinLayoutFromType(channel_layout_type type);
		static const char* GetChannelName(channel_id id);
		static const char* GetBuiltinLayoutDefaultName(channel_layout_type type);
	};
	rational time_base{};
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
	media_buffer_node* upstream{};
	media_buffer_node* downstream{};
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
			} mkv;
		}meta;
		char* Name;
	}format_info{};
};

#include <vpx/vpx_image.h>
struct _buffer_desc {
	//for abi and compatibility
	const uint32_t desc_size = sizeof(_buffer_desc);
	//the flag for protocol (input)
	const uint32_t type;
	//the stream the buffer is in
	stream_desc* stream;
	//supplemental data and flags (depends on protocol)
	uint64_t start_timestamp;
	uint64_t end_timestamp;
	union buffer_detail {
		struct packet {
			uint8_t* buffer;
			uint32_t size;
			uint32_t track;
			bool key_frame;
		} pkt;
		struct image_frame {
			bool planar;
			void* planes[4]{};
			int line_size[4]{};
			int width, height;
			int crop_left, crop_right, crop_top, crop_bottom;
			color_space space;
			color_range range;
		} image;
		struct audio_frame {
			void* channels[max_channels]{};
			int nb_samples;
			int sample_rate;
		} audio_frame;
	} detail;
	//implemented by protocol.
	//eg.  free(_this->data) or perhaps
	//glDeleteTextures(1,(GLuint*)&_this->data)
	void (*release)(_buffer_desc* _this, void* ptr) = nullptr;
};

constexpr size_t size = sizeof(_buffer_desc);

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
