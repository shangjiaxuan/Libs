#include "media_transform.h"

#include <vpx/vpx_codec.h>
#include <vpx/vpx_decoder.h>
#include <mutex>
#include <vpx/vp8dx.h>
#include <cassert>

//implemets the default vp9 decoder with internal
//framebuffers and frame by frame decoding.
//(Also with postprocessing by default, maybe
//add a flag to request)
class libvpx_vp9_ram_decoder: public video_decoder {
	const vpx_codec_iface_t* const iface;
	vpx_codec_ctx ctx;
	//not needed for vp9
	vpx_codec_iter_t iter;
	vpx_image_t* probe_frame = nullptr;
	const vpx_codec_dec_cfg cfg;
	const int mflags;
	const int init_err;
	std::once_flag once_flag;
	bool default_calling_probe = false;
	stream_desc out_stream;
public:
	libvpx_vp9_ram_decoder(stream_desc* upstream, uint32_t threads = 1, int flags = VPX_CODEC_USE_POSTPROC):
		video_decoder(decoder_type::VD_VP9_RAM_VPX_IMG_DECODER),iface(vpx_codec_vp9_dx()), 
		cfg{threads, upstream->detail.video.width, upstream->detail.video.height}, mflags(flags),
		init_err(vpx_codec_dec_init(&ctx, iface, &cfg, mflags)), iter(NULL) {
		assert(upstream->type == stream_desc::MTYPE_VIDEO);
		assert(upstream->detail.video.codec == stream_desc::video_info::VCODEC_VP9);
		out_stream.type =stream_desc::MTYPE_VIDEO;
		out_stream.detail.video = upstream->detail.video;
		out_stream.detail.video.codec = stream_desc::video_info::VCODEC_RAW;
		out_stream.upstream = this;
		upstream->downstream = this;
		desc_in = upstream;
		desc_out = &out_stream;
	}
	virtual ~libvpx_vp9_ram_decoder() override final
	{
		vpx_codec_destroy(&ctx);
	}
	//for decoders, this means sending a packet into decoder
	virtual int QueueBuffer(_buffer_desc& buffer) override final
	{
		return vpx_codec_decode(&ctx, (uint8_t*)buffer.data, buffer.data1, 0, 0);
	}
	//for decoders, this means getting a frame from decoder
	virtual int FetchBuffer(_buffer_desc& buffer) override final
	{
		default_calling_probe = true;
		std::call_once(once_flag, &libvpx_vp9_ram_decoder::Probe, this);
		//buffer.type checks here
		[[unlikely]] if (probe_frame) {
			buffer.data = probe_frame;
			probe_frame = nullptr;
			return S_OK;
		}
		vpx_image_t* img = vpx_codec_get_frame(&ctx, &iter);
		buffer.data = img;
		if(!img)
			return E_AGAIN;
		else
			return S_OK;
	}
	//This is for cases where the frame is owned or refed
	//by the user and needs to be freed
	virtual int ReleaseBuffer(_buffer_desc& buffer) override final
	{
		return S_OK;
	}
	//This is for cases where the frame is owned or refed
	//by the user and needs to be allocated manually
	virtual int AllocBuffer(_buffer_desc& buffer) override final
	{
		return E_INVALID_OPERATION;
	}
	//This means that the last packet is recieved.
	virtual int Flush() override final
	{
		return vpx_codec_decode(&ctx, nullptr, 0, 0, 0);
	}
	//This means that a packet is dropped and requests
	//an error concealment frame if supported
	//(mostly used in audio codecs)
	virtual int Dropped(int samples) override final
	{
		return E_UNIMPLEMENTED;
	}
	//Probe the format
	virtual int Probe() override final
	{
		assert(desc_out);
		if (probe_frame)
			return E_INVALID_OPERATION;
		vpx_image_t* img = vpx_codec_get_frame(&ctx, &iter);
		if (!img)
			return S_OK;
		auto do_probe = [this, img] () {
			probe_frame = img;
			switch (img->cs) {
				case VPX_CS_UNKNOWN:
					desc_in->detail.video.space = stream_desc::video_info::CS_UNKNOWN;
					desc_out->detail.video.space = stream_desc::video_info::CS_UNKNOWN;
					break;
				case VPX_CS_BT_601:
					desc_in->detail.video.space = stream_desc::video_info::CS_BT_601;
					desc_out->detail.video.space = stream_desc::video_info::CS_BT_601;
					break;
				case VPX_CS_BT_709:
					desc_in->detail.video.space = stream_desc::video_info::CS_BT_709;
					desc_out->detail.video.space = stream_desc::video_info::CS_BT_709;
					break;
				case VPX_CS_BT_2020:
					desc_in->detail.video.space = stream_desc::video_info::CS_BT_2020;
					desc_out->detail.video.space = stream_desc::video_info::CS_BT_2020;
					break;
				case VPX_CS_SMPTE_170:
					desc_in->detail.video.space = stream_desc::video_info::CS_SMPTE_170;
					desc_out->detail.video.space = stream_desc::video_info::CS_SMPTE_170;
					break;
				case VPX_CS_SMPTE_240:
					desc_in->detail.video.space = stream_desc::video_info::CS_SMPTE_240;
					desc_out->detail.video.space = stream_desc::video_info::CS_SMPTE_240;
					break;
				case VPX_CS_SRGB:
					desc_in->detail.video.space = stream_desc::video_info::CS_SRGB;
					desc_out->detail.video.space = stream_desc::video_info::CS_SRGB;
					break;
			}
			switch (img->fmt) {
				case VPX_IMG_FMT_NONE:
					desc_in->detail.video.bitdepth = 8;
					desc_in->detail.video.subsample_vert = 1;
					desc_in->detail.video.subsample_horiz = 1;
					desc_in->detail.video.invert_uv = 0;
					desc_out->detail.video.bitdepth = 8;
					desc_out->detail.video.subsample_vert = 1;
					desc_out->detail.video.subsample_horiz = 1;
					desc_out->detail.video.invert_uv = 0;
					break;
				case VPX_IMG_FMT_YV12:
					desc_in->detail.video.bitdepth = 8;
					desc_in->detail.video.subsample_vert = 1;
					desc_in->detail.video.subsample_horiz = 1;
					desc_in->detail.video.invert_uv = 1;
					desc_out->detail.video.bitdepth = 8;
					desc_out->detail.video.subsample_vert = 1;
					desc_out->detail.video.subsample_horiz = 1;
					desc_out->detail.video.invert_uv = 1;
					break;
				case VPX_IMG_FMT_I420:
					desc_in->detail.video.bitdepth = 8;
					desc_in->detail.video.subsample_vert = 1;
					desc_in->detail.video.subsample_horiz = 1;
					desc_in->detail.video.invert_uv = 0;
					desc_out->detail.video.bitdepth = 8;
					desc_out->detail.video.subsample_vert = 1;
					desc_out->detail.video.subsample_horiz = 1;
					desc_out->detail.video.invert_uv = 0;
					break;
				case VPX_IMG_FMT_I422:
					desc_in->detail.video.bitdepth = 8;
					desc_in->detail.video.subsample_vert = 0;
					desc_in->detail.video.subsample_horiz = 1;
					desc_in->detail.video.invert_uv = 0;
					desc_out->detail.video.bitdepth = 8;
					desc_out->detail.video.subsample_vert = 0;
					desc_out->detail.video.subsample_horiz = 1;
					desc_out->detail.video.invert_uv = 0;
					break;
				case VPX_IMG_FMT_I444:
					desc_in->detail.video.bitdepth = 8;
					desc_in->detail.video.subsample_vert = 0;
					desc_in->detail.video.subsample_horiz = 0;
					desc_in->detail.video.invert_uv = 0;
					desc_out->detail.video.bitdepth = 8;
					desc_out->detail.video.subsample_vert = 0;
					desc_out->detail.video.subsample_horiz = 0;
					desc_out->detail.video.invert_uv = 0;
					break;
				case VPX_IMG_FMT_I440:
					desc_in->detail.video.bitdepth = 8;
					desc_in->detail.video.subsample_vert = 1;
					desc_in->detail.video.subsample_horiz = 0;
					desc_in->detail.video.invert_uv = 0;
					desc_out->detail.video.bitdepth = 8;
					desc_out->detail.video.subsample_vert = 1;
					desc_out->detail.video.subsample_horiz = 0;
					desc_out->detail.video.invert_uv = 0;
					break;
				case VPX_IMG_FMT_I42016:
					desc_in->detail.video.bitdepth = 16;
					desc_in->detail.video.subsample_vert = 1;
					desc_in->detail.video.subsample_horiz = 1;
					desc_in->detail.video.invert_uv = 0;
					desc_out->detail.video.bitdepth = 16;
					desc_out->detail.video.subsample_vert = 1;
					desc_out->detail.video.subsample_horiz = 1;
					desc_out->detail.video.invert_uv = 0;
					break;
				case VPX_IMG_FMT_I42216:
					desc_in->detail.video.bitdepth = 16;
					desc_in->detail.video.subsample_vert = 0;
					desc_in->detail.video.subsample_horiz = 1;
					desc_in->detail.video.invert_uv = 0;
					desc_out->detail.video.bitdepth = 16;
					desc_out->detail.video.subsample_vert = 0;
					desc_out->detail.video.subsample_horiz = 1;
					desc_out->detail.video.invert_uv = 0;
					break;
				case VPX_IMG_FMT_I44416:
					desc_in->detail.video.bitdepth = 16;
					desc_in->detail.video.subsample_vert = 0;
					desc_in->detail.video.subsample_horiz = 0;
					desc_in->detail.video.invert_uv = 0;
					desc_out->detail.video.bitdepth = 16;
					desc_out->detail.video.subsample_vert = 0;
					desc_out->detail.video.subsample_horiz = 0;
					desc_out->detail.video.invert_uv = 0;
					break;
				case VPX_IMG_FMT_I44016:
					desc_in->detail.video.bitdepth = 16;
					desc_in->detail.video.subsample_vert = 1;
					desc_in->detail.video.subsample_horiz = 0;
					desc_in->detail.video.invert_uv = 0;
					desc_out->detail.video.bitdepth = 16;
					desc_out->detail.video.subsample_vert = 1;
					desc_out->detail.video.subsample_horiz = 0;
					desc_out->detail.video.invert_uv = 0;
					break;
			}
			switch (img->range) {
				case VPX_CR_STUDIO_RANGE:
					desc_in->detail.video.range = stream_desc::video_info::CR_STUDIO_RANGE;
					desc_out->detail.video.range = stream_desc::video_info::CR_STUDIO_RANGE;
					break;
				case VPX_CR_FULL_RANGE:
					desc_in->detail.video.range = stream_desc::video_info::CR_FULL_RANGE;
					desc_out->detail.video.range = stream_desc::video_info::CR_FULL_RANGE;
					break;
			}
		};
		if(!default_calling_probe)
			std::call_once(once_flag, do_probe);
		else
			do_probe();
		return S_OK;
	}
};


video_decoder* video_decoder_factory::CreateDefaultVP9Decoder(stream_desc* upstream, uint32_t threads, bool use_post_proc)
{
	assert(upstream);
	int flags = 0;
	if(use_post_proc)
		flags |= VPX_CODEC_USE_POSTPROC;
	if(threads>1)
		flags |= VPX_CODEC_USE_FRAME_THREADING;
	else 
		threads = 1;
	return new libvpx_vp9_ram_decoder(upstream,threads,flags);
}
