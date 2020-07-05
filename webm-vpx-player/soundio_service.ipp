#include "soundio_service.h"
#pragma once

template<backend_type backend>
soundio_service<backend>::soundio_service()
{
	std::lock_guard<std::mutex> guard(init_mutex);
	if (!refs.fetch_add(1, std::memory_order_relaxed) || !inited.load(std::memory_order_relaxed)) {
		handle = soundio_create();
		assert(handle);
		{
			handle->app_name = "soundio backend";
			handle->on_devices_change = on_devices_change;
			handle->on_backend_disconnect = on_backend_disconnect;
			handle->emit_rtprio_warning = emit_rtprio_warning;
			handle->jack_info_callback = jack_info_callback;
			handle->jack_error_callback = jack_error_callback;
			handle->on_events_signal = on_events_signal;
			handle->userdata = (void*)backend;
		}
		int err = 0;
		switch (backend) {
			case BACKEND_DEFAULT:
				err = soundio_connect(handle);
				break;
			case BACKEND_DUMMY:
				err = soundio_connect_backend(handle, SoundIoBackendDummy);
				break;
				break;
			case BACKEND_WASAPI:
				err = soundio_connect_backend(handle, SoundIoBackendWasapi);
				break;
				break;
			case BACKEND_ALSA:
				err = soundio_connect_backend(handle, SoundIoBackendAlsa);
				break;
			case BACKEND_JACK:
				err = soundio_connect_backend(handle, SoundIoBackendJack);
				break;
			case BACKEND_PULSE:
				err = soundio_connect_backend(handle, SoundIoBackendPulseAudio);
				break;
			case BACKEND_COREAUDIO:
				err = soundio_connect_backend(handle, SoundIoBackendPulseAudio);
				break;
			default:
				err = E_UNIMPLEMENTED;
				break;
		}
		if (err) {
			if (handle) {
				soundio_destroy(handle);
				handle = 0;
			}
			refs.fetch_sub(std::memory_order_relaxed);
		}
		else {
			inited.store(true, std::memory_order_relaxed);
		}
		soundio_flush_events(handle);
	}
}

template<backend_type backend>
soundio_service<backend>::~soundio_service()
{
	std::lock_guard<std::mutex> guard(init_mutex);
	if (refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
		if (handle) {
			soundio_disconnect(handle);
			soundio_destroy(handle);
			handle = 0;
		}
	}
}

template<backend_type backend>
soundio_service<backend>::soundio_service(const soundio_service& other)
{
	//lifetime of the copied object makes sure
	//that this object will not interfere with
	//destruction
	if (inited.load(std::memory_order_relaxed))
		refs.fetch_add(1, std::memory_order_relaxed);
}

template<backend_type backend>
void soundio_service<backend>::FlushEvents()
{
	soundio_flush_events(handle);
}

template<backend_type backend>
void soundio_service<backend>::WaitEvents()
{
	soundio_wait_events(handle);
}

template<backend_type backend>
void soundio_service<backend>::Wakeup()
{
	soundio_wakeup(handle);
}

template<backend_type backend>
int soundio_service<backend>::GetInputDeviceCount()
{
	return soundio_input_device_count(handle);
}

template<backend_type backend>
int soundio_service<backend>::GetOutputDeviceCount()
{
	return soundio_output_device_count(handle);
}

template<backend_type backend>
int soundio_service<backend>::DefaultInput()
{
	return soundio_default_input_device_index(handle);
}

template<backend_type backend>
int soundio_service<backend>::DefaultOutput()
{
	return soundio_default_output_device_index(handle);
}

template<backend_type backend>
soundio_device soundio_service<backend>::GetInputDeviceFromIndex(int index)
{
	soundio_device dev = soundio_get_input_device(handle, index);
	soundio_device_sort_channel_layouts(dev);
	soundio_device_unref(dev);
	return std::move(dev);
}

template<backend_type backend>
soundio_device soundio_service<backend>::GetOutputDeviceFromIndex(int index)
{
	soundio_device dev = soundio_get_output_device(handle, index);
	soundio_device_sort_channel_layouts(dev);
	soundio_device_unref(dev);
	return std::move(dev);
}

template<backend_type backend>
SignalEventCallback soundio_service<backend>::SetEventSignal(SignalEventCallback callback)
{
	SignalEventCallback cb = current_callback;
	current_callback = callback;
	return cb;
}

template<backend_type backend>
void soundio_service<backend>::on_devices_change(SoundIo*) noexcept
{
	//recheck default device and change to the new device.
}

template<backend_type backend>
void soundio_service<backend>::on_backend_disconnect(SoundIo*, int err) noexcept
{
	const char* err_str;
	switch (err) {
		case SoundIoErrorBackendDisconnected:
			err_str = "soundio backend disconnect: SoundIoErrorBackendDisconnected";
			break;
		case SoundIoErrorNoMem:
			err_str = "soundio backend disconnect: SoundIoErrorNoMem";
			break;
		case SoundIoErrorSystemResources:
			err_str = "soundio backend disconnect: SoundIoErrorSystemResources";
			break;
		case SoundIoErrorOpeningDevice:
			err_str = "soundio backend disconnect: SoundIoErrorOpeningDevice";
			break;
		default:
			err_str = "soundio backend disconnect: unkown error";
			break;
	}
	fprintf(stderr, err_str);
	//notify outside here.

}

template<backend_type backend>
void soundio_service<backend>::emit_rtprio_warning() noexcept
{
	fprintf(stderr, "soundio failed to set realtime priority");
}

template<backend_type backend>
void soundio_service<backend>::jack_info_callback(const char* msg) noexcept
{
	fprintf(stderr, "Soundio Jack info: %s", msg);
}

template<backend_type backend>
void soundio_service<backend>::jack_error_callback(const char* msg) noexcept
{
	fprintf(stderr, "Soundio Jack error: %s", msg);
}

template<backend_type backend>
void soundio_service<backend>::on_events_signal(SoundIo*) noexcept
{	
	//atomic load
	void (*cb)(backend_type) = current_callback;
	if (cb) {
		cb(backend);
	}
}

