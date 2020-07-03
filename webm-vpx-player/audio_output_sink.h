#pragma once

#include "media_sink.h"

class audio_output_sink  : public media_sink{
	virtual double GetLatency() = 0;
};



