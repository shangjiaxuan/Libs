#pragma once

#include "media_sink.h"

#include <matroska/MatroskaWriter.h>

class mkv_sink_factory;

//mkv_source:
//_buffer_desc.data1: buffer size
//_buffer_desc.data2: track
//_buffer_desc.data3: keyframe
class mkv_sink:	public media_sink {
protected:
friend mkv_sink_factory;
	mkv_sink();
	OutputStream ostream{};
	nodecontext ctx{};
	MatroskaFile* file = nullptr;
	bool writing = false;
protected:
	int AddTrack(const stream_desc& info);
	int write_headers();
	int finish_init();
	//	uint64_t file_pos;
	stream_desc* desc_in;
	size_t num_in;
public:
	virtual ~mkv_sink();
	//	virtual int FetchBuffer(_buffer_desc& buffer) override final;
protected:
	virtual int GetInputs(stream_desc *& desc, size_t& num) override final
	{
		desc = desc_in;
		num = num_in;
		return S_OK;
	}
};

class mkv_sink_factory {
public:
	static mkv_sink* CreateFromFile(const stream_desc* tracks, size_t num, const char* path);
};

