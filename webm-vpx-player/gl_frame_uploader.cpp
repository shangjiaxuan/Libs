#include "media_transform.h"

#define GLEW_STATIC
#include <GL/glew.h>
#include <glm/glm.hpp>

struct GL_420_Frame {
	GLuint texture_yuv[3];
	GLint yuv_locations[3];
	GLuint program;
};

class GL_Frame_Uploader: public media_tansform {
	GL_Frame_Uploader(stream_desc* upstream)
	{
		w = desc_in->detail.video.width;
		h = desc_in->detail.video.height;
		const char vert_shader[] =
			//dummy user shader code
			"#version 450 compatibility\n"
			"in vec3 vertex;\n"
			//reusable part
			//the correction for odd number of pixels
			//actual texture coordinate of actual frame
			"in vec2 tex_coord;\n"
			"uniform vec2 uv_correction_wh;\n"
			"out vec2 YCoords;\n"
			"out vec2 UVCoords;\n"
			"void set_uv_locations(void){\n"
			"YCoords = tex_coord;\n"
			"UVCoords = vec2((tex_coord-uv_correction_wh.x)/(1-2*uv_correction_wh.x),(tex_coord-uv_correction_wh.y)/(1-2*uv_correction_wh.y));}";
			//user calling
			"void main(void){\n"
			"set_uv_locations();\n"
			"gl_Position = vec4(vertex.xy, 1.0, 1.0);}";
		const char frag_shader[] =
			//shader header
			"#version 450 compatibility\n"
			"out vec4 out_color;\n"
			//reusable shader code, managed
			"vec4 rgba_color;\n"
			"in vec2 YCoords;\n"
			"in vec2 UVCoords;\n"
			"uniform mat4x3 color_matrix;\n"
			"uniform vec2 clamp;\n"
			"uniform sampler2D Texture_y;\n"
			"uniform sampler2D Texture_u;\n"
			"uniform sampler2D Texture_v;\n"
			"uniform sampler2D Texture_a;\n"
			"vec4 get_rgba_color(void){\n"
			"float y = clamp(texture(Texture_y, YCoords).r, clamp.x, clamp.y);"
			"float u = clamp(texture(Texture_u, UVCoords).r, clamp.x, clamp.y);"
			"float v = clamp(texture(Texture_v, UVCoords).r, clamp.x, clamp.y);"
			"float a = texture(Texture_v, YCoords).r"
			"out_color = vec4(color_matrix*vec4(y,u,v,1.0), a);}"
			//user shader code
			"void main(void){\n"
			"out_color = get_rgba_color();}";
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
		yuv_locations[0] = glGetUniformLocation(program, "Texture_y");
		yuv_locations[1] = glGetUniformLocation(program, "Texture_u");
		yuv_locations[2] = glGetUniformLocation(program, "Texture_v");
		yuv_locations[3] = glGetUniformLocation(program, "Texture_a");
		color_matrix_location = glGetUniformLocation(program, "color_matrix");
		clamp_location = glGetUniformLocation(program, "clamp");
		uv_correction_wh = glGetUniformLocation(program, "uv_correction_wh");
		vertex_location = glGetAttribLocation(program, "vertex");
		tex_coord_location = glGetAttribLocation(program, "tex_coord");
		init_rendering_vertices();
	}
	void set_cs_cr_8bit(color_space cs, color_range cr)
	{
		constexpr double clamp[2]{16.0 / 255, 235.0 / 255};
		constexpr float srgb_to_srgb_ff8_matrix[3][4]{
			{1, 0, 0, 0},
			{0, 1, 0, 0},
			{0, 0, 1, 0}
		};
		constexpr float srgb_to_srgb_sf8_matrix[3][4]{
			{1 / (clamp[1] - clamp[0]), 0, 0, -(clamp[0] / (clamp[1] - clamp[0]))},
			{0, 1 / (clamp[1] - clamp[0]), 0, -(clamp[0] / (clamp[1] - clamp[0]))},
			{0, 0, 1 / (clamp[1] - clamp[0]), -(clamp[0] / (clamp[1] - clamp[0]))}
		};
		constexpr double Kr_bt601 = 0.299;
		constexpr double Kg_bt601 = 0.587;
		constexpr double Kb_bt601 = 0.114;
		constexpr float bt601_to_rgb_sf8_matrix[3][4]{
			{1 / (clamp[1] - clamp[0]), 0, 0.5 * (1 - Kr_bt601), 0.5 * (1 - Kr_bt601) - (clamp[0] / (clamp[1] - clamp[0]))},
			{1 / (clamp[1] - clamp[0]), -0.5 * (1 - Kb_bt601) * Kb_bt601 / Kg_bt601, -0.5 * (1 - Kr_bt601) * Kr_bt601 / Kg_bt601, -0.5 * (1 - Kb_bt601) * Kb_bt601 / Kg_bt601 - 0.5 * (1 - Kr_bt601) * Kr_bt601 / Kg_bt601 - (clamp[0] / (clamp[1] - clamp[0]))},
			{1 / (clamp[1] - clamp[0]), 0.5 * (1 - Kb_bt601), 0, 0.5 * (1 - Kb_bt601) - (clamp[0] / (clamp[1] - clamp[0]))}
		};
		constexpr float bt601_to_rgb_ff8_matrix[3][4]{
			{1 , 0, 0.5 * (1 - Kr_bt601), 0.5 * (1 - Kr_bt601)},
			{1 , -0.5 * (1 - Kb_bt601) * Kb_bt601 / Kg_bt601, -0.5 * (1 - Kr_bt601) * Kr_bt601 / Kg_bt601, -0.5 * (1 - Kb_bt601) * Kb_bt601 / Kg_bt601 - 0.5 * (1 - Kr_bt601) * Kr_bt601 / Kg_bt601},
			{1 , 0.5 * (1 - Kb_bt601), 0, 0.5 * (1 - Kb_bt601)}
		};
		constexpr double Kr_bt709 = 0.2126;
		constexpr double Kg_bt709 = 0.7152;
		constexpr double Kb_bt709 = 0.0722;
		constexpr float bt709_to_rgb_sf8_matrix[3][4]{
			{1 / (clamp[1] - clamp[0]), 0, 0.5 * (1 - Kr_bt709), 0.5 * (1 - Kr_bt709) - (clamp[0] / (clamp[1] - clamp[0]))},
			{1 / (clamp[1] - clamp[0]), -0.5 * (1 - Kb_bt709) * Kb_bt709 / Kg_bt709, -0.5 * (1 - Kr_bt709) * Kr_bt709 / Kg_bt709, -0.5 * (1 - Kb_bt709) * Kb_bt709 / Kg_bt709 - 0.5 * (1 - Kr_bt709) * Kr_bt709 / Kg_bt709 - (clamp[0] / (clamp[1] - clamp[0]))},
			{1 / (clamp[1] - clamp[0]), 0.5 * (1 - Kb_bt709), 0, 0.5 * (1 - Kb_bt709) - (clamp[0] / (clamp[1] - clamp[0]))}
		};
		constexpr float bt709_to_rgb_ff8_matrix[3][4]{
			{1, 0, 0.5 * (1 - Kr_bt709), 0.5 * (1 - Kr_bt709)},
			{1, -0.5 * (1 - Kb_bt709) * Kb_bt709 / Kg_bt709, -0.5 * (1 - Kr_bt709) * Kr_bt709 / Kg_bt709, -0.5 * (1 - Kb_bt709) * Kb_bt709 / Kg_bt709 - 0.5 * (1 - Kr_bt709) * Kr_bt709 / Kg_bt709},
			{1, 0.5 * (1 - Kb_bt709), 0, 0.5 * (1 - Kb_bt709)}
		};
		//other 8bit color matrices here
		glUseProgram(program);
		if(cr==CR_FULL_RANGE) {
			glUniform2f(clamp_location, 0.0, 1.0);
			switch (cs) {
				case CS_BT_601:
					glUniformMatrix4x3fv(color_matrix_location, 1, 1, bt601_to_rgb_sf8_matrix[0]);
					break;
				case CS_BT_709:
					glUniformMatrix4x3fv(color_matrix_location, 1, 1, bt709_to_rgb_sf8_matrix[0]);
					break;
				case CS_SRGB:
					glUniformMatrix4x3fv(color_matrix_location, 1, 1, srgb_to_srgb_sf8_matrix[0]);
					break;
				default:
					glUniformMatrix4x3fv(color_matrix_location, 1, 1, bt601_to_rgb_sf8_matrix[0]);
					break;
			}
		}
		else if (cr == CR_STUDIO_RANGE) {
			glUniform2f(clamp_location, clamp[0], clamp[1]);
			switch (cs) {
				case CS_BT_601:
					glUniformMatrix4x3fv(color_matrix_location, 1, 1, bt601_to_rgb_ff8_matrix[0]);
					break;
				case CS_BT_709:
					glUniformMatrix4x3fv(color_matrix_location, 1, 1, bt709_to_rgb_ff8_matrix[0]);
					break;
				case CS_SRGB:
					glUniformMatrix4x3fv(color_matrix_location, 1, 1, srgb_to_srgb_ff8_matrix[0]);
					break;
				default:
					glUniformMatrix4x3fv(color_matrix_location, 1, 1, bt601_to_rgb_ff8_matrix[0]);
					break;
			}
		}
	}
	void init_yuv420_textures(int w, int h)
	{
		int num_to_delete = 0;
		for (; num_to_delete < 4 && texture_yuv[num_to_delete]; ++num_to_delete) {}
		if (num_to_delete) {
			glDeleteTextures(num_to_delete,texture_yuv);
		}
		glCreateTextures(GL_TEXTURE_2D, 4, texture_yuv);
		glTextureStorage2D(texture_yuv[0], 1, GL_R8, w, h);
		glTextureStorage2D(texture_yuv[1], 1, GL_R8, (w + 1) / 2, (h + 1) / 2);
		glTextureStorage2D(texture_yuv[2], 1, GL_R8, (w + 1) / 2, (h + 1) / 2);
		glTextureStorage2D(texture_yuv[3], 1, GL_R8, 1, 1);
		for (int i = 0; i < 3; ++i) {
			float border_color[] = {0.0, 0.0, 0.0, 1.0};
			glTextureParameterfv(texture_yuv[i], GL_TEXTURE_BORDER_COLOR, border_color);
			glTextureParameteri(texture_yuv[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glTextureParameteri(texture_yuv[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
			glTextureParameteri(texture_yuv[i], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(texture_yuv[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		uint8_t byte = 255;
		glTextureSubImage2D(texture_yuv[3], 0, 0, 0, 1, 1, GL_RED, GL_UNSIGNED_BYTE, &byte);
		float border_color[] = {0.0, 0.0, 0.0, 0.0};
		glTextureParameterfv(texture_yuv[3], GL_TEXTURE_BORDER_COLOR, border_color);
		glTextureParameteri(texture_yuv[3], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTextureParameteri(texture_yuv[3], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTextureParameteri(texture_yuv[3], GL_TEXTURE_MIN_FILTER, GL_POINT);
		glTextureParameteri(texture_yuv[3], GL_TEXTURE_MAG_FILTER, GL_POINT);
		for (int i = 0; i < 4; ++i) {
			glActiveTexture(GL_TEXTURE0 + yuv_locations[i]);
			glBindTexture(GL_TEXTURE_2D, texture_yuv[i]);
		}
		glUniform2f(uv_correction_wh, float(w & 0x1) / (2*(w + 1)), float(h & 0x1) / (2*(h + 1)));
	}
	void init_rendering_vertices()
	{
		points[0].x = -1.0;
		points[0].y = 1.0;
		points[0].z = 0.0;
		points[0].u = 0.0;
		points[0].v = 1.0;
		points[1].x = -1.0;
		points[1].y = -1.0;
		points[1].z = 0.0;
		points[1].u = 0.0;
		points[1].v = 0.0;
		points[2].x = 1.0;
		points[2].y = 1.0;
		points[2].z = 0.0;
		points[2].u = 1.0;
		points[2].v = 1.0;
		points[3].x = 1.0;
		points[3].y = -1.0;
		points[3].z = 0.0;
		points[3].u = 1.0;
		points[3].v = 0.0;
		if (!vertices_with_uv) {
			glGenVertexArrays(1, &vao);
			glBindVertexArray(vao);
			glGenBuffers(1, &vertices_with_uv);
			glVertexAttribPointer(vertex_location, sizeof(float) * 3, GL_FLOAT, GL_FALSE,sizeof(textured_points), (void*)0);
			glEnableVertexAttribArray(vertex_location);
			glVertexAttribPointer(tex_coord_location, sizeof(float) * 2, GL_FLOAT, GL_FALSE, sizeof(textured_points), (void*)offsetof(textured_points,textured_points::u));
			glEnableVertexAttribArray(tex_coord_location);
		}
		glBindBuffer(GL_ARRAY_BUFFER, vertices_with_uv);
		glBufferData(GL_ARRAY_BUFFER, sizeof(textured_points) * 4, points, GL_STATIC_DRAW);
	}
	int w, h;
	GLuint vert;
	GLuint frag;
	GLuint program;
	GLint color_matrix_location;
	GLint clamp_location;
	GLint uv_correction_wh;
	GLint yuv_locations[4];
	GLuint texture_yuv[4]{};

	//for demo usage, the vertex and texture coords.
	GLuint vertices_with_uv = 0;
	GLuint vao = 0;
	struct textured_points {
		float x, y, z;
		float u, v;
	};
	//the cpu side buffer for updating
	textured_points points[4];
	GLint vertex_location;
	GLint tex_coord_location;
};
