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
	virtual ~media_tansform() {};
};

class media_decoder :public media_tansform {
public:
	virtual int Dropped(int samples) = 0;
	virtual int Probe() = 0;
	virtual ~media_decoder() {};
};

class video_decoder_factory;

class video_decoder: public media_decoder {
public:
friend video_decoder_factory;
	enum class decoder_type {
		VD_NONE,
		VD_VP9_RAM_VPX_IMG_DECODER,
		VD_VP9_GLTEXTURE_DECODER,
		VD_VP9_GLBUFFER_DECODER,
		VD_D3D11_VP9_DECODER,
		VD_LAST
	};
	const decoder_type vdecoder_type;
	video_decoder(decoder_type type) : vdecoder_type(type){}
	virtual ~video_decoder() {};
};

class video_decoder_factory {
public:
	//creates a decoder that uses the vpx_img_t* as desc.data, all other fields should be ignored
	//lifetime is valid only between calls to fetch buffer
	static video_decoder* CreateDefaultVP9Decoder(stream_desc* upstream, uint32_t threads = 1, bool use_post_proc = false);
	
};

class audio_decoder_factory;
class audio_decoder: public media_decoder {
public:
	friend audio_decoder_factory;
	enum class decoder_type {
		AD_NONE,
		AD_FLAC_DIRECT,
		AD_VORBIS_DIRECT,
		AD_OPUS_DIRECT,
		AD_LAST
	};
	const decoder_type adecoder_type;
	audio_decoder(decoder_type type): adecoder_type(type) {}
	virtual ~audio_decoder() {};
};

class audio_decoder_factory {
public:
	//creates a decoder that uses the vpx_img_t* as desc.data, all other fields should be ignored
	//lifetime is valid only between calls to fetch buffer
	static audio_decoder* CreateDefaultOpusDecoder(stream_desc* upstream);

};


