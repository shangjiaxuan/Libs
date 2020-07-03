#pragma once

#include "media_buffer.h"

class media_tansform: public media_buffer_node {
protected:
	media_tansform() {
		num_in = 1;
		num_out = 1;
	}
public:
	virtual int GetInputs(stream_desc *& desc, size_t& num) override final
	{
		desc = desc_in;
		num = 1;
		return S_OK;
	}
	virtual int GetOutputs(stream_desc *& desc, size_t& num) override final
	{
		desc = desc_out;
		num = 1;
		return S_OK;
	}
};

typedef media_tansform media_decoder;

class video_decoder: public media_decoder {
public:
	enum class decoder_type {
		VD_NONE,
		VD_VP9_RAM_VPX_IMG_DECODER,
		VD_VP9_GLTEXTURE_DECODER,
		VD_VP9_GLBUFFER_DECODER,
		VD_D3D11_VP9_DECODER
	};
	const decoder_type vdecoder_type;
	video_decoder(decoder_type type) : vdecoder_type(type){}
	virtual int Dropped() = 0;
	virtual int Probe() = 0;
};

class video_decoder_factory {
public:
	//creates a decoder that uses the vpx_img_t* as desc.data, all other fields should be ignored
	//lifetime is valid only between calls to fetch buffer
	static video_decoder* CreateDefaultVP9Decoder(stream_desc* upstream, uint32_t threads = 1, bool use_post_proc = true);
	
};


