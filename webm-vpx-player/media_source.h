#pragma once

#include "media_buffer.h"

class media_source : public media_buffer_node {
public:
	media_source(){}
	virtual ~media_source(){};
	virtual int QueueBuffer(_buffer_desc& buffer) override final {
		return E_INVALID_OPERATION;
	};
	virtual int AllocBuffer(_buffer_desc& buffer) override final {
		return E_INVALID_OPERATION;
	}
	virtual int Flush() override final {
		return E_INVALID_OPERATION;
	}
	virtual int GetInputs(const stream_desc*& desc, size_t& num) override final
	{
		num = 0;
		desc = nullptr;
		return S_OK;
	}
};




