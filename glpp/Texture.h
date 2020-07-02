#pragma once
#include <GL/glew.h>
#include <initializer_list>
#include <cassert>
#include <atomic>

enum texture_type: GLenum {
	Texture_1D = GL_TEXTURE_1D,
	Texture_2D = GL_TEXTURE_2D,
	Texture_3D = GL_TEXTURE_3D,
	Texture_1D_Arr = GL_TEXTURE_1D_ARRAY,
	Texture_2D_Arr = GL_TEXTURE_2D_ARRAY,
	Texture_Rect = GL_TEXTURE_RECTANGLE,
	Texture_Cube = GL_TEXTURE_CUBE_MAP,
	Texture_Cube_Arr = GL_TEXTURE_CUBE_MAP_ARRAY,
	Texture_2D_MSAA = GL_TEXTURE_2D_MULTISAMPLE,
	Texture_2D_MSAA_Arr = GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
	Texture_Buffer = GL_TEXTURE_BUFFER
};

enum class texture_format:GLenum {
	U8 = GL_R8,
	S8 = GL_R8_SNORM,
	U16 = GL_R16,
	S16 = GL_R16_SNORM,
	U88 = GL_RG8,

};

class Texture {
public:
	Texture(){};
	~Texture() {};
	GLuint tex;
	unsigned width, height;
};

class Texture_2D : Texture {
	Texture_2D(unsigned width, unsigned height) {
		glCreateTextures(texture_type::Texture_2D,1,&tex);
	}
};
