/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2021      Rinnegatamante
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "utils/glutil.h"

#include "utils/utils.h"
#include "utils/dialog.h"
#include "utils/logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/io/stat.h>

// Helpers for our handling of shaders
GLboolean skip_next_compile = GL_FALSE;
char next_shader_fname[256];
static unsigned int shader_trace_count;
void load_shader(GLuint shader, const char * string, size_t length);

void gl_preload() {
    if (!file_exists("ur0:/data/libshacccg.suprx")
        && !file_exists("ur0:/data/external/libshacccg.suprx")) {
        fatal_error("Error: libshacccg.suprx is not installed. "
                    "Google \"ShaRKBR33D\" for quick installation.");
    }

#ifdef USE_GLSL_SHADERS
    vglSetSemanticBindingMode(VGL_MODE_POSTPONED);
#endif
}

void gl_init() {
    vglInitExtended(0, 960, 544, 6 * 1024 * 1024, SCE_GXM_MULTISAMPLE_4X);
}

void gl_swap() {
    vglSwapBuffers(GL_FALSE);
}

void glShaderSource_soloader(GLuint shader, GLsizei count,
                             const GLchar **string, const GLint *_length) {
#ifdef DEBUG_OPENGL
    sceClibPrintf("[gl_dbg] glShaderSource<%p>(shader: %i, count: %i, string: %p, length: %p)\n", __builtin_return_address(0), shader, count, string, _length);
#endif
    if (!string || count <= 0) {
        l_error("<%p> Shader source string is NULL, count: %i",
                   __builtin_return_address(0), count);
        skip_next_compile = GL_TRUE;
        return;
    } else if (!*string) {
        l_error("<%p> Shader source *string is NULL, count: %i",
                   __builtin_return_address(0), count);
        skip_next_compile = GL_TRUE;
        return;
    }

    if (shader_trace_count < 32) {
        l_warn("shader source #%u: id=%u count=%d strings=%p lengths=%p first=%p",
               shader_trace_count, (unsigned int)shader, (int)count,
               string, _length, string[0]);
    }

    size_t total_length = 0;

    for (int i = 0; i < count; ++i) {
        if (!string[i]) {
            l_error("<%p> Shader source string[%i] is NULL", __builtin_return_address(0), i);
            return;
        }
        size_t part_length = !_length || _length[i] < 0
            ? strlen(string[i])
            : (size_t)_length[i];
        if (part_length > SIZE_MAX - total_length - 1) {
            l_error("<%p> Shader source length overflow", __builtin_return_address(0));
            return;
        }
        total_length += part_length;
    }

    if (shader_trace_count < 32) {
        l_warn("shader source #%u: id=%u length=%u",
               shader_trace_count, (unsigned int)shader,
               (unsigned int)total_length);
    }
    shader_trace_count++;

    char * str = malloc(total_length+1);
    if (!str) {
        l_error("<%p> Shader source allocation failed (%u bytes)",
                   __builtin_return_address(0), (unsigned int)(total_length + 1));
        return;
    }
    size_t l = 0;

    for (int i = 0; i < count; ++i) {
        size_t part_length = !_length || _length[i] < 0
            ? strlen(string[i])
            : (size_t)_length[i];
        memcpy(str + l, string[i], part_length);
        l += part_length;
    }
    str[total_length] = '\0';

    load_shader(shader, str, total_length);

    free(str);
}

void glCompileShader_soloader(GLuint shader) {
#ifdef DEBUG_OPENGL
    sceClibPrintf("[gl_dbg] glCompileShader<%p>(shader: %i)\n", __builtin_return_address(0), shader);
#endif

#ifndef USE_GXP_SHADERS
    if (!skip_next_compile) {
        if (shader_trace_count < 32) {
            GLint source_length = 0;
            GLsizei copied_length = 0;
            char source_preview[257];
            glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &source_length);
            glGetShaderSource(shader, sizeof(source_preview), &copied_length,
                              source_preview);
            if (copied_length < 0 || copied_length >= (GLsizei)sizeof(source_preview))
                copied_length = (GLsizei)sizeof(source_preview) - 1;
            source_preview[copied_length] = '\0';
            l_warn("shader compile: id=%u source_length=%d copied=%d text=%.*s",
                   (unsigned int)shader, (int)source_length,
                   (int)copied_length, (int)sizeof(source_preview) - 1,
                   source_preview);
        }
        glCompileShader(shader);
#ifdef DUMP_COMPILED_SHADERS
        void *bin = vglMalloc(32 * 1024);
        GLsizei len;
        vglGetShaderBinary(shader, 32 * 1024, &len, bin);
        file_save(next_shader_fname, bin, len);
        vglFree(bin);
#endif
    }
    skip_next_compile = GL_FALSE;
#endif
}

void glGetActiveAttrib_soloader(GLuint program, GLuint index, GLsizei buf_size,
                                 GLsizei *length, GLint *size, GLenum *type,
                                 GLchar *name) {
    if (length) *length = 0;
    if (size) *size = 0;
    if (type) *type = 0;
    if (name && buf_size > 0) name[0] = '\0';
    glGetActiveAttrib(program, index, buf_size, length, size, type, name);
    l_info("glGetActiveAttrib(program=%u, index=%u): length=%d size=%d type=0x%x name=\"%s\" from %p",
           (unsigned int)program, (unsigned int)index, length ? (int)*length : 0,
           size ? (int)*size : 0, type ? (unsigned int)*type : 0,
           name ? name : "", __builtin_return_address(0));
}

void glGetActiveUniform_soloader(GLuint program, GLuint index, GLsizei buf_size,
                                  GLsizei *length, GLint *size, GLenum *type,
                                  GLchar *name) {
    if (length) *length = 0;
    if (size) *size = 0;
    if (type) *type = 0;
    if (name && buf_size > 0) name[0] = '\0';
    glGetActiveUniform(program, index, buf_size, length, size, type, name);
    l_info("glGetActiveUniform(program=%u, index=%u): length=%d size=%d type=0x%x name=\"%s\" from %p",
           (unsigned int)program, (unsigned int)index, length ? (int)*length : 0,
           size ? (int)*size : 0, type ? (unsigned int)*type : 0,
           name ? name : "", __builtin_return_address(0));
}

void glGetShaderiv_soloader(GLuint shader, GLenum pname, GLint *params) {
    if (params) *params = 0;
    glGetShaderiv(shader, pname, params);
    l_info("glGetShaderiv(shader=%u, pname=0x%x): %d from %p",
           (unsigned int)shader, (unsigned int)pname, params ? *params : 0,
           __builtin_return_address(0));
}

void glGetProgramiv_soloader(GLuint program, GLenum pname, GLint *params) {
    if (params) *params = 0;
    glGetProgramiv(program, pname, params);
    l_info("glGetProgramiv(program=%u, pname=0x%x): %d from %p",
           (unsigned int)program, (unsigned int)pname, params ? *params : 0,
           __builtin_return_address(0));
}

void glGetShaderPrecisionFormat_soloader(GLenum shader_type, GLenum precision_type,
                                         GLint *range, GLint *precision) {
    (void)shader_type;
    if (range) {
        range[0] = 127;
        range[1] = 127;
    }
    (void)precision_type;
    if (precision) {
        *precision = 23;
    }
}

// Diagnostic wrappers: log-only, same signatures as the real GL APIs.
// They run before/after the real call without altering arguments or results.
static unsigned int shader_source_trace_count;

void glShaderSource_trace(GLuint shader, GLsizei count,
                          const GLchar *const *string, const GLint *length) {
    if (shader_source_trace_count < 32) {
        char preview[129];
        preview[0] = '\0';
        if (count > 0 && string && string[0]) {
            size_t avail;
            if (length && length[0] >= 0)
                avail = (size_t)length[0];
            else
                avail = strlen(string[0]);
            if (avail > 128) avail = 128;
            memcpy(preview, string[0], avail);
            preview[avail] = '\0';
        }
        l_warn("shader source: id=%u count=%d lengths=%p first_len=%d text=%.128s",
               (unsigned int)shader, (int)count, length,
               (count > 0 && length) ? (int)length[0] : -1, preview);
        shader_source_trace_count++;
    }
    glShaderSource(shader, count, string, length);
}

GLuint glCreateShader_trace(GLenum type) {
    GLuint shader = glCreateShader(type);
    l_info("glCreateShader(type=0x%x): %u from %p",
           (unsigned int)type, (unsigned int)shader,
           __builtin_return_address(0));
    return shader;
}

void glCompileShader_trace(GLuint shader) {
    l_info("glCompileShader(shader=%u) from %p",
           (unsigned int)shader, __builtin_return_address(0));
    glCompileShader(shader);
    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    l_info("glCompileShader(shader=%u): compile_status=%d",
           (unsigned int)shader, (int)status);
    if (status == 0) {
        char log[512];
        GLsizei len = 0;
        log[0] = '\0';
        glGetShaderInfoLog(shader, sizeof(log), &len, log);
        if (len < 0 || len >= (GLsizei)sizeof(log))
            len = (GLsizei)sizeof(log) - 1;
        log[len] = '\0';
        l_error("compile-fail shader %u: %s", (unsigned int)shader, log);
    }
}
void glTexImage2D_soloader(GLenum target, GLint level, GLint internalformat,
                           GLsizei width, GLsizei height, GLint border,
                           GLenum format, GLenum type, const GLvoid *pixels) {
    l_info("glTexImage2D(target=0x%x, level=%d, internal=0x%x, %dx%d, border=%d, format=0x%x, type=0x%x, pixels=%p) from %p",
           (unsigned int)target, (int)level, (int)internalformat,
           (int)width, (int)height, (int)border, (unsigned int)format,
           (unsigned int)type, pixels, __builtin_return_address(0));
    glTexImage2D(target, level, internalformat, width, height, border,
                 format, type, pixels);
}

void glTexSubImage2D_soloader(GLenum target, GLint level, GLint xoffset,
                              GLint yoffset, GLsizei width, GLsizei height,
                              GLenum format, GLenum type,
                              const GLvoid *pixels) {
    l_info("glTexSubImage2D(target=0x%x, level=%d, off=(%d,%d), %dx%d, format=0x%x, type=0x%x, pixels=%p) from %p",
           (unsigned int)target, (int)level, (int)xoffset, (int)yoffset,
           (int)width, (int)height, (unsigned int)format,
           (unsigned int)type, pixels, __builtin_return_address(0));
    glTexSubImage2D(target, level, xoffset, yoffset, width, height,
                    format, type, pixels);
}

void glPixelStorei_soloader(GLenum pname, GLint param) {
    l_info("glPixelStorei(pname=0x%x, param=%d) from %p",
           (unsigned int)pname, (int)param, __builtin_return_address(0));
    glPixelStorei(pname, param);
}

const GLubyte *glGetString_soloader(GLenum name) {
    const GLubyte *ret = glGetString(name);
    l_info("glGetString(name=0x%x): \"%s\" from %p",
           (unsigned int)name, ret ? (const char *)ret : "(null)",
           __builtin_return_address(0));
    return ret;
}

// vitaGL encodes a uniform location as the negated address of its internal
// uniform record, so locations are huge numbers. Marmalade sizes its uniform
// tables by the largest location it sees, which turned into a multi-GB
// s3eMalloc after the splash shaders were linked. Give the game small
// per-program ids (0, 1, 2...) like a desktop driver would, and translate them
// back to vitaGL's value on every glUniform* call.
#define UNIFORM_MAP_MAX_PROGRAMS 1024 // vitaGL's MAX_CUSTOM_PROGRAMS

typedef struct {
    GLint *locations; // vitaGL location for each compact id
    GLint count;
    GLint capacity;
} UniformMap;

static UniformMap uniform_maps[UNIFORM_MAP_MAX_PROGRAMS];
static unsigned int uniform_unknown_warnings;

static UniformMap *uniform_map_get(GLuint program) {
    if (program == 0 || program > UNIFORM_MAP_MAX_PROGRAMS)
        return NULL;
    return &uniform_maps[program - 1];
}

static void uniform_map_reset(GLuint program) {
    UniformMap *map = uniform_map_get(program);
    if (!map)
        return;
    free(map->locations);
    map->locations = NULL;
    map->count = 0;
    map->capacity = 0;
}

static GLint uniform_map_add(UniformMap *map, GLint location) {
    for (GLint id = 0; id < map->count; id++) {
        if (map->locations[id] == location)
            return id;
    }
    if (map->count == map->capacity) {
        GLint capacity = map->capacity ? map->capacity * 2 : 16;
        GLint *locations = realloc(map->locations,
                                   (size_t)capacity * sizeof(*locations));
        if (!locations)
            fatal_error("Could not grow the uniform location table.");
        map->locations = locations;
        map->capacity = capacity;
    }
    map->locations[map->count] = location;
    return map->count++;
}

// Compact id of the current program -> vitaGL location (-1 is ignored by GL).
static GLint uniform_location_resolve(GLint id) {
    if (id < 0)
        return -1;
    GLint program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    UniformMap *map = uniform_map_get((GLuint)program);
    if (!map || id >= map->count) {
        if (uniform_unknown_warnings < 16) {
            l_warn("glUniform*: unknown location %d for program %d",
                   (int)id, (int)program);
            uniform_unknown_warnings++;
        }
        return -1;
    }
    return map->locations[id];
}

GLint glGetUniformLocation_soloader(GLuint program, const GLchar *name) {
    UniformMap *map = uniform_map_get(program);
    if (!map || !name)
        return -1;

    GLint location = glGetUniformLocation(program, name);
    size_t len = strlen(name);
    if (location == -1 && len > 3 && len < 128 &&
        !strcmp(name + len - 3, "[0]")) {
        // GL treats "name[0]" as "name"; vitaGL only knows the base name.
        char base[128];
        memcpy(base, name, len - 3);
        base[len - 3] = '\0';
        location = glGetUniformLocation(program, base);
    }
    if (location == -1)
        return -1;

    GLint count = map->count;
    GLint id = uniform_map_add(map, location);
    if (id == count) {
        l_info("glGetUniformLocation(program=%u, \"%s\"): id %d (vitaGL 0x%x)",
               (unsigned int)program, name, (int)id, (unsigned int)location);
    }
    return id;
}

void glDeleteProgram_soloader(GLuint program) {
    glDeleteProgram(program);
    uniform_map_reset(program);
}

void glUniform1f_soloader(GLint location, GLfloat v0) {
    glUniform1f(uniform_location_resolve(location), v0);
}

void glUniform1fv_soloader(GLint location, GLsizei count, const GLfloat *value) {
    glUniform1fv(uniform_location_resolve(location), count, value);
}

void glUniform1i_soloader(GLint location, GLint v0) {
    glUniform1i(uniform_location_resolve(location), v0);
}

void glUniform1iv_soloader(GLint location, GLsizei count, const GLint *value) {
    glUniform1iv(uniform_location_resolve(location), count, value);
}

void glUniform2f_soloader(GLint location, GLfloat v0, GLfloat v1) {
    glUniform2f(uniform_location_resolve(location), v0, v1);
}

void glUniform2fv_soloader(GLint location, GLsizei count, const GLfloat *value) {
    glUniform2fv(uniform_location_resolve(location), count, value);
}

void glUniform2i_soloader(GLint location, GLint v0, GLint v1) {
    glUniform2i(uniform_location_resolve(location), v0, v1);
}

void glUniform2iv_soloader(GLint location, GLsizei count, const GLint *value) {
    glUniform2iv(uniform_location_resolve(location), count, value);
}

void glUniform3f_soloader(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    glUniform3f(uniform_location_resolve(location), v0, v1, v2);
}

void glUniform3fv_soloader(GLint location, GLsizei count, const GLfloat *value) {
    glUniform3fv(uniform_location_resolve(location), count, value);
}

void glUniform3i_soloader(GLint location, GLint v0, GLint v1, GLint v2) {
    glUniform3i(uniform_location_resolve(location), v0, v1, v2);
}

void glUniform3iv_soloader(GLint location, GLsizei count, const GLint *value) {
    glUniform3iv(uniform_location_resolve(location), count, value);
}

void glUniform4f_soloader(GLint location, GLfloat v0, GLfloat v1, GLfloat v2,
                          GLfloat v3) {
    glUniform4f(uniform_location_resolve(location), v0, v1, v2, v3);
}

void glUniform4fv_soloader(GLint location, GLsizei count, const GLfloat *value) {
    glUniform4fv(uniform_location_resolve(location), count, value);
}

void glUniform4i_soloader(GLint location, GLint v0, GLint v1, GLint v2,
                          GLint v3) {
    glUniform4i(uniform_location_resolve(location), v0, v1, v2, v3);
}

void glUniform4iv_soloader(GLint location, GLsizei count, const GLint *value) {
    glUniform4iv(uniform_location_resolve(location), count, value);
}

void glUniformMatrix2fv_soloader(GLint location, GLsizei count,
                                 GLboolean transpose, const GLfloat *value) {
    glUniformMatrix2fv(uniform_location_resolve(location), count, transpose,
                       value);
}

void glUniformMatrix3fv_soloader(GLint location, GLsizei count,
                                 GLboolean transpose, const GLfloat *value) {
    glUniformMatrix3fv(uniform_location_resolve(location), count, transpose,
                       value);
}

void glUniformMatrix4fv_soloader(GLint location, GLsizei count,
                                 GLboolean transpose, const GLfloat *value) {
    glUniformMatrix4fv(uniform_location_resolve(location), count, transpose,
                       value);
}

// Diagnostic: when a program fails to link, dump the attached shader
// sources so the failing GLSL can be identified from boot.log.
void glLinkProgram_soloader(GLuint program) {
    l_info("glLinkProgram(program=%u) from %p",
           (unsigned int)program, __builtin_return_address(0));
    // Relinking invalidates every uniform location of the program.
    uniform_map_reset(program);
    glLinkProgram(program);
    GLint status = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    l_info("glLinkProgram(program=%u): link_status=%d",
           (unsigned int)program, (int)status);
    if (status == 0) {
        GLuint shaders[8] = {0};
        GLsizei count = 0;
        glGetAttachedShaders(program, 8, &count, shaders);
        l_error("glLinkProgram(program=%u): FAILED with %d attached shaders",
                (unsigned int)program, (int)count);
        for (GLsizei i = 0; i < count && i < 8; i++) {
            GLint src_len = 0;
            glGetShaderiv(shaders[i], GL_SHADER_SOURCE_LENGTH, &src_len);
            GLsizei dump_len = src_len > 2048 ? 2048 : src_len;
            if (dump_len > 0) {
                char *src = malloc((size_t)dump_len + 1);
                if (src) {
                    GLsizei copied = 0;
                    glGetShaderSource(shaders[i], dump_len + 1, &copied,
                                      src);
                    if (copied < 0 || copied > dump_len)
                        copied = dump_len;
                    src[copied] = '\0';
                    l_error("link-fail shader %u source (%d bytes): %s",
                            (unsigned int)shaders[i], (int)copied, src);
                    free(src);
                }
            } else {
                l_error("link-fail shader %u: empty source (len=%d)",
                        (unsigned int)shaders[i], (int)src_len);
            }
        }
    }
}

void glGetProgramInfoLog_soloader(GLuint program, GLsizei max_length,
                                  GLsizei *length, GLchar *info_log) {
    if (length) *length = 0;
    if (info_log && max_length > 0) info_log[0] = '\0';
    glGetProgramInfoLog(program, max_length, length, info_log);
    l_info("glGetProgramInfoLog(program=%u): length=%d log=\"%s\"",
           (unsigned int)program, length ? (int)*length : 0,
           info_log ? info_log : "");
}

#if defined(USE_GLSL_SHADERS) && defined(DUMP_COMPILED_SHADERS)
void load_shader(GLuint shader, const char * string, size_t length) {
    char* sha_name = str_sha1sum(string, length);

    char gxp_path[256];
    snprintf(gxp_path, sizeof(gxp_path), DATA_PATH"gxp/%s.gxp", sha_name);

    if (file_exists(gxp_path)) {
        uint8_t *buffer;
        size_t size;

        file_load(gxp_path, &buffer, &size);

        glShaderBinary(1, &shader, 0, buffer, (int32_t) size);

        free(buffer);
        skip_next_compile = GL_TRUE;
    } else {
        glShaderSource(shader, 1, &string, &length);
        strcpy(next_shader_fname, gxp_path);
    }

    free(sha_name);
}
#elif defined(USE_GLSL_SHADERS)
void load_shader(GLuint shader, const char * string, size_t length) {
    glShaderSource(shader, 1, &string, &length);
}
#elif defined(USE_CG_SHADERS) && defined(DUMP_COMPILED_SHADERS)
void load_shader(GLuint shader, const char * string, size_t length) {
    char* sha_name = str_sha1sum(string, length);

    char gxp_path[256];
    char cg_path[256];
    snprintf(gxp_path, sizeof(gxp_path), DATA_PATH"gxp/%s.gxp", sha_name);
    snprintf(cg_path, sizeof(cg_path), DATA_PATH"cg/%s.cg", sha_name);

    if (file_exists(gxp_path)) {
        uint8_t *buffer;
        size_t size;

        file_load(gxp_path, &buffer, &size);

        glShaderBinary(1, &shader, 0, buffer, (int32_t) size);

        free(buffer);
        skip_next_compile = GL_TRUE;
    } else if (file_exists(cg_path)) {
        char *buffer;
        size_t size;

        file_load(cg_path, (uint8_t **) &buffer, &size);

        glShaderSource(shader, 1, &string, &size);
        strcpy(next_shader_fname, gxp_path);

        free(buffer);
        skip_next_compile = GL_FALSE;
    } else {
        l_warn("Encountered an untranslated shader %s, saving GLSL "
               "and using a dummy shader.", sha_name);

        char glsl_path[256];
        snprintf(glsl_path, sizeof(glsl_path), DATA_PATH"glsl/%s.glsl", sha_name);
        file_mkpath(glsl_path, 0777);
        file_save(glsl_path, (const uint8_t *) string, length);

        if (strstr(string, "gl_FragColor")) {
            const char *dummy_shader = "float4 main() { return float4(1.0,1.0,1.0,1.0); }";
            int32_t dummy_shader_len = (int32_t) strlen(dummy_shader);
            glShaderSource(shader, 1, &dummy_shader, &dummy_shader_len);
        } else {
            const char *dummy_shader = "void main(float4 out gl_Position : POSITION ) { gl_Position = float4(1.0,1.0,1.0,1.0); }";
            int32_t dummy_shader_len = (int32_t) strlen(dummy_shader);
            glShaderSource(shader, 1, &dummy_shader, &dummy_shader_len);
        }

        skip_next_compile = GL_FALSE;
    }

    free(sha_name);
}
#elif defined(USE_CG_SHADERS) || defined(USE_GXP_SHADERS)
void load_shader(GLuint shader, const char * string, size_t length) {
    char* sha_name = str_sha1sum(string, length);

    char path[256];
#ifdef USE_CG_SHADERS
    snprintf(path, sizeof(path), DATA_PATH"cg/%s.cg", sha_name);
#else
    snprintf(path, sizeof(path), DATA_PATH"gxp/%s.gxp", sha_name);
#endif

    if (file_exists(path)) {
#ifdef USE_CG_SHADERS
        char *buffer;
        size_t size;

        file_load(path, (uint8_t **) &buffer, &size);

        glShaderSource(shader, 1, &string, &size);

        free(buffer);
#else
        uint8_t *buffer;
        size_t size;

        file_load(path, &buffer, &size);

        glShaderBinary(1, &shader, 0, buffer, (int32_t) size);

        free(buffer);
#endif
    } else {
        l_warn("Encountered an untranslated shader %s, saving GLSL "
               "and using a dummy shader.", sha_name);

        char glsl_path[256];
        snprintf(glsl_path, sizeof(glsl_path), DATA_PATH"glsl/%s.glsl", sha_name);
        file_mkpath(glsl_path, 0777);
        file_save(glsl_path, (const uint8_t *) string, length);

        if (strstr(string, "gl_FragColor")) {
            const char *dummy_shader = "float4 main() { return float4(1.0,1.0,1.0,1.0); }";
            int32_t dummy_shader_len = (int32_t) strlen(dummy_shader);
            glShaderSource(shader, 1, &dummy_shader, &dummy_shader_len);
        } else {
            const char *dummy_shader = "void main(float4 out gl_Position : POSITION ) { gl_Position = float4(1.0,1.0,1.0,1.0); }";
            int32_t dummy_shader_len = (int32_t) strlen(dummy_shader);
            glShaderSource(shader, 1, &dummy_shader, &dummy_shader_len);
        }
    }

    free(sha_name);
}
#else
#error "Define one of (USE_GLSL_SHADERS, USE_CG_SHADERS, USE_GXP_SHADERS)"
#endif
