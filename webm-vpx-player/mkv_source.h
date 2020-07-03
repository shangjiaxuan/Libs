#pragma once

#include "media_source.h"

class mkv_source_factory;

#include <matroska/MatroskaParser.h>
//mkv_source:
//_buffer_desc.data1: buffer size
//_buffer_desc.data2: track
//_buffer_desc.data3: keyframe
class mkv_source:public media_source {
protected:
	friend mkv_source_factory;
	mkv_source();
	InputStream istream{};
	nodecontext ctx{};
	MatroskaFile* file = nullptr;
	int finish_init();

//	uint64_t file_pos;
public:
	virtual ~mkv_source();
	virtual int GetOutputs(stream_desc *& desc, size_t& num) override final
	{
		num = num_out;
		desc = desc_out;
		return S_OK;
	}
//	virtual int FetchBuffer(_buffer_desc& buffer) override final;
};

class mkv_source_factory {
public:
	static mkv_source* CreateFromFile(const char* path);
};

