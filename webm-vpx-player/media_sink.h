#pragma once

#include "media_buffer.h"

class media_sink: public media_buffer_node {
public:
	media_sink() {}
	virtual ~media_sink() {};
	virtual int FetchBuffer(_buffer_desc& buffer) override final
	{
		return E_INVALID_OPERATION;
	};
	virtual int ReleaseBuffer(_buffer_desc& buffer) override final
	{
		return E_INVALID_OPERATION;
	}
	virtual int GetOutputs(stream_desc *& desc, size_t& num) override final
	{
		num = 0;
		desc = nullptr;
		return S_OK;
	}
};
