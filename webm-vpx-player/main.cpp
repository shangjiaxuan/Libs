#include "Graphics.h"
#include <cstdio>
#include <cstring>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <SDL/SDL.h>
#undef main
#define GLEW_STATIC
#include <GL/glew.h>
#include <glm/glm.hpp>

size_t num = 0;

void GLAPIENTRY GLdebug(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
	if (id == 131154 && severity == GL_DEBUG_SEVERITY_MEDIUM && type == GL_DEBUG_TYPE_PERFORMANCE)++num;
	puts(message);
}

class yuv420_tex {
	GLuint texture_yuv[3];
	GLint yuv_locations[3];
	int width, height;

	GLuint vert;
	GLuint frag;
	GLuint program;

	GLint color_matrix_location;
	GLint clamp_location;
public:
	yuv420_tex(int w, int h)
	{
		glCreateTextures(GL_TEXTURE_2D, 3, texture_yuv);
		glTextureStorage2D(texture_yuv[0], 1, GL_R8, w, h);
		glTextureStorage2D(texture_yuv[1], 1, GL_R8, (w + 1) / 2, (h + 1) / 2);
		glTextureStorage2D(texture_yuv[2], 1, GL_R8, (w + 1) / 2, (h + 1) / 2);
		for (int i = 0; i < 3; ++i) {
			float border_color[] = {0.0, 0.0, 0.0, 1.0};
			glTextureParameterfv(texture_yuv[i], GL_TEXTURE_BORDER_COLOR, border_color);
			glTextureParameteri(texture_yuv[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glTextureParameteri(texture_yuv[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
			glTextureParameteri(texture_yuv[i], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(texture_yuv[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		const char vert_shader[] =
			"#version 450 compatibility\n"
			"in vec2 vertex;\n"
			"out vec2 TexCoords;\n"
			"void main(void){\n"
			"TexCoords = vec2((vertex.x+1.0)/2.0,-(vertex.y-1.0)/2.0);\n"
			"gl_Position = vec4(vertex.xy, 1.0, 1.0);}";
		const char frag_shader[] =
			"#version 450 compatibility\n"
			"out vec4 out_color;\n"
			"in vec2 TexCoords;\n"
			"uniform mat4x3 color_matrix;\n"
			"uniform vec2 clamp;\n"
	//		"uniform float index;\n"
			"uniform sampler2D Texture_y;\n"
			"uniform sampler2D Texture_u;\n"
			"uniform sampler2D Texture_v;\n"
			"void main(void){\n"
			"float y = clamp(texture(Texture_y, TexCoords).r, clamp.x, clamp.y);"
			"float u = clamp(texture(Texture_u, TexCoords).r, clamp.x, clamp.y);"
			"float v = clamp(texture(Texture_v, TexCoords).r, clamp.x, clamp.y);"
			"out_color = vec4(color_matrix*vec4(y,u,v,1.0), 1.0);}";
		//studio range, bt601 colorspace
		constexpr double Kr = 0.299;
		constexpr double Kg = 0.587;
		constexpr double Kb = 0.114;
		constexpr double clamp[2]{16.0 / 255, 235.0 / 255};
		constexpr float bt601_to_rgb_sf_matrix[3][4]{
			{1 / (clamp[1] - clamp[0]), 0, 0.5 * (1 - Kr), 0.5 * (1 - Kr) - (clamp[0] / (clamp[1] - clamp[0]))},
			{1 / (clamp[1] - clamp[0]), -0.5 * (1 - Kb) * Kb / Kg, -0.5 * (1 - Kr) * Kr / Kg, -0.5 * (1 - Kb) * Kb / Kg - 0.5 * (1 - Kr) * Kr / Kg - (clamp[0] / (clamp[1] - clamp[0]))},
			{1 / (clamp[1] - clamp[0]), 0.5 * (1 - Kb), 0, 0.5 * (1 - Kb) - (clamp[0] / (clamp[1] - clamp[0]))}
		};
		const GLint vert_len = sizeof(vert_shader) - 1;
		const GLint frag_len = sizeof(frag_shader) - 1;
		const char* vert_s = vert_shader;
		const char* frag_s = frag_shader;
		vert = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vert, 1, &vert_s, &vert_len);
		glCompileShader(vert);
		frag = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(frag, 1, &frag_s, &frag_len);
		glCompileShader(frag);
		program = glCreateProgram();
		glAttachShader(program, vert);
		glAttachShader(program, frag);
		glLinkProgram(program);
		glUseProgram(program);
		yuv_locations[0] = glGetUniformLocation(program, "Texture_y");
		yuv_locations[1] = glGetUniformLocation(program, "Texture_u");
		yuv_locations[2] = glGetUniformLocation(program, "Texture_v");
		color_matrix_location = glGetUniformLocation(program, "color_matrix");
		clamp_location = glGetUniformLocation(program, "clamp");
		//This needs to be done everywhere if no managerpresent
		for (int i = 0; i < 3; ++i) {
			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(GL_TEXTURE_2D, texture_yuv[0]);
		}
		glUniformMatrix4x3fv(color_matrix_location, 1, 1, bt601_to_rgb_sf_matrix[0]);
		glUniform2f(clamp_location, clamp[0], clamp[1]);
		glFlush();
	}
};

//temporary, only for testing purposes
class Graphics {
	SDL_Window* hwd;
	SDL_GLContext gl_ctx;
	union atomic_viewport_size {
		struct {
			uint32_t display_width, dispaly_height;
		};
		std::atomic_uint64_t packed;
	} display_state;
	std::condition_variable start_cond;
	std::mutex start_mtx;
	std::thread thread_handle;
	std::condition_variable open_cond;
	std::atomic_bool open_finished = false;
public:
	Graphics(int w, int h)
	{
		hwd = SDL_CreateWindow("TestWindow", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, SDL_WINDOW_OPENGL);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
		SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetSwapInterval(-1);
		gl_ctx = SDL_GL_CreateContext(hwd);
		SDL_GL_MakeCurrent(hwd, gl_ctx);
		int err = glewInit();
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(GLdebug, nullptr);
		const char* vendor = (const char*)glGetString(GL_VENDOR);
		if (!strcmp(vendor, "Intel")) {
	//		objects.upload_need_flush = true;
		}
		if (!strcmp(vendor, "NVIDIA Corporation")) {
	//		objects.upload_need_flush = false;
		}
		else {
	//		objects.upload_need_flush = true;
		}
		puts((const char*)glGetString(GL_VENDOR));
		puts((const char*)glGetString(GL_RENDERER));
		puts((const char*)glGetString(GL_VERSION));
		puts((const char*)glGetString(GL_EXTENSIONS));
		SDL_GL_MakeCurrent(hwd,nullptr);
		display_state.dispaly_height = w;
		display_state.dispaly_height = h;
		thread_handle = std::move(std::thread(thread_proc_proxy, this));
		while(!open_finished.load(std::memory_order_relaxed)){
			std::unique_lock<std::mutex> lck(start_mtx);
			open_cond.wait(lck);
		}
	}
	void Start()
	{
		start_cond.notify_one();
	}
private:
	void thread_proc()
	{
		{
			open_finished.store(true, std::memory_order_relaxed);
			open_cond.notify_one();
			std::unique_lock<std::mutex> lck(start_mtx);
			start_cond.wait(lck);
		}


	}
	static void thread_proc_proxy(Graphics* This)
	{
		This->thread_proc();
	}
};

int main()
{
	Graphics g(1280, 720);
	bool running = true;
	while (running) {
		SDL_Event event;
		SDL_WaitEventTimeout(&event, 16);
		switch (event.type) {
			case SDL_QUIT:
				//TODO: gracefully notify threads
				//and shut down
				exit(0);
			case SDL_WINDOWEVENT:
				SDL_WindowEvent& win_event=event.window;
				switch (win_event.event) {

				}
		}
	}
	return 0;
}
