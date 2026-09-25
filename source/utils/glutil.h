/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2021      Rinnegatamante
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  glutil.h
 * @brief OpenGL API initializer, related functions.
 */

#ifndef SOLOADER_GLUTIL_H
#define SOLOADER_GLUTIL_H

#include <vitaGL.h>

#ifdef __cplusplus
extern "C" {
#endif

void gl_init();

void gl_preload();

void gl_swap();

void glCompileShader_soloader(GLuint shader);
void glGetActiveAttrib_soloader(GLuint program, GLuint index, GLsizei buf_size,
                                 GLsizei *length, GLint *size, GLenum *type,
                                 GLchar *name);
void glGetActiveUniform_soloader(GLuint program, GLuint index, GLsizei buf_size,
                                 GLsizei *length, GLint *size, GLenum *type,
                                 GLchar *name);
void glGetShaderiv_soloader(GLuint shader, GLenum pname, GLint *params);
void glGetProgramiv_soloader(GLuint program, GLenum pname, GLint *params);
void glGetShaderPrecisionFormat_soloader(GLenum shader_type, GLenum precision_type,
                                         GLint *range, GLint *precision);

void glShaderSource_soloader(GLuint shader, GLsizei count,
                             const GLchar **string, const GLint *_length);
void glTexImage2D_soloader(GLenum target, GLint level, GLint internalformat,
                           GLsizei width, GLsizei height, GLint border,
                           GLenum format, GLenum type, const GLvoid *pixels);
void glTexSubImage2D_soloader(GLenum target, GLint level, GLint xoffset,
                              GLint yoffset, GLsizei width, GLsizei height,
                              GLenum format, GLenum type,
                              const GLvoid *pixels);
void glPixelStorei_soloader(GLenum pname, GLint param);
const GLubyte *glGetString_soloader(GLenum name);
void glLinkProgram_soloader(GLuint program);
void glShaderSource_trace(GLuint shader, GLsizei count,
                          const GLchar *const *string, const GLint *length);
GLuint glCreateShader_trace(GLenum type);
void glCompileShader_trace(GLuint shader);
void glGetProgramInfoLog_soloader(GLuint program, GLsizei max_length,
                                  GLsizei *length, GLchar *info_log);

#ifdef __cplusplus
};
#endif

#endif // SOLOADER_GLUTIL_H
