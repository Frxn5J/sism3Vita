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

// Presents one frame with system dialogs (IME, messages) on top.
void gl_dialog_frame();

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
void glDeleteProgram_soloader(GLuint program);

// Uniform locations handed to the game are compact per-program ids; these
// wrappers translate them back to vitaGL locations.
GLint glGetUniformLocation_soloader(GLuint program, const GLchar *name);
void glUniform1f_soloader(GLint location, GLfloat v0);
void glUniform1fv_soloader(GLint location, GLsizei count, const GLfloat *value);
void glUniform1i_soloader(GLint location, GLint v0);
void glUniform1iv_soloader(GLint location, GLsizei count, const GLint *value);
void glUniform2f_soloader(GLint location, GLfloat v0, GLfloat v1);
void glUniform2fv_soloader(GLint location, GLsizei count, const GLfloat *value);
void glUniform2i_soloader(GLint location, GLint v0, GLint v1);
void glUniform2iv_soloader(GLint location, GLsizei count, const GLint *value);
void glUniform3f_soloader(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
void glUniform3fv_soloader(GLint location, GLsizei count, const GLfloat *value);
void glUniform3i_soloader(GLint location, GLint v0, GLint v1, GLint v2);
void glUniform3iv_soloader(GLint location, GLsizei count, const GLint *value);
void glUniform4f_soloader(GLint location, GLfloat v0, GLfloat v1, GLfloat v2,
                          GLfloat v3);
void glUniform4fv_soloader(GLint location, GLsizei count, const GLfloat *value);
void glUniform4i_soloader(GLint location, GLint v0, GLint v1, GLint v2,
                          GLint v3);
void glUniform4iv_soloader(GLint location, GLsizei count, const GLint *value);
void glUniformMatrix2fv_soloader(GLint location, GLsizei count,
                                 GLboolean transpose, const GLfloat *value);
void glUniformMatrix3fv_soloader(GLint location, GLsizei count,
                                 GLboolean transpose, const GLfloat *value);
void glUniformMatrix4fv_soloader(GLint location, GLsizei count,
                                 GLboolean transpose, const GLfloat *value);
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
