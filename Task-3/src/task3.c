#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <EGL/egl.h>
#include <GLES3/gl31.h>

#define M 10

const char *shader_code =
"#version 310 es\n"
"layout(local_size_x = 16, local_size_y = 16) in;\n"
"layout(std430, binding = 0) readonly buffer InBuf { int in_grid[]; };\n"
"layout(std430, binding = 1) writeonly buffer OutBuf { int out_grid[]; };\n"
"uniform int u_size;\n"
"uniform float u_seed;\n"
"\n"
"float rand(vec2 co) {\n"
"    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);\n"
"}\n"
"\n"
"void main() {\n"
"    int x = int(gl_GlobalInvocationID.x);\n"
"    int y = int(gl_GlobalInvocationID.y);\n"
"    if (x >= u_size || y >= u_size) return;\n"
"    int idx = y * u_size + x;\n"
"    int current = in_grid[idx];\n"
"    int next = current;\n"
"\n"
"    if (current == 1) {\n"
"        next = 2;\n"
"    } else if (current == 0) {\n"
"        bool near_fire = false;\n"
"        for (int dy = -1; dy <= 1; dy++) {\n"
"            for (int dx = -1; dx <= 1; dx++) {\n"
"                if (dx == 0 && dy == 0) continue;\n"
"                int nx = x + dx, ny = y + dy;\n"
"                if (nx >= 0 && nx < u_size && ny >= 0 && ny < u_size) {\n"
"                    if (in_grid[ny * u_size + nx] == 1) { near_fire = true; break; }\n"
"                }\n"
"            }\n"
"            if (near_fire) break;\n"
"        }\n"
"        if (near_fire && rand(vec2(float(idx), u_seed)) < 0.15) {\n"
"            next = 1;\n"
"        }\n"
"    }\n"
"    out_grid[idx] = next;\n"
"}\n";

void print_grid(int *grid, int epoch) {
    printf("Epoch %d:\n", epoch);
    for (int y = 0; y < M; y++) {
        for (int x = 0; x < M; x++) {
            int val = grid[y * M + x];
            if (val == 0) printf("H ");
            else if (val == 1) printf("B ");
            else printf("N ");
        }
        printf("\n");
    }
    printf("\n");
}

int main() {
    srand((unsigned int)time(NULL));

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, NULL, NULL);

    EGLConfig config;
    EGLint num_config;
    EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE
    };
    eglChooseConfig(display, attribs, &config, 1, &num_config);

    EGLint ctx_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctx_attribs);

    EGLint pbuf_attribs[] = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
    EGLSurface surface = eglCreatePbufferSurface(display, config, pbuf_attribs);
    eglMakeCurrent(display, surface, surface, context);

    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &shader_code, NULL);
    glCompileShader(shader);

    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    glUseProgram(program);

    int total = M * M;
    int *grid = (int*)calloc(total, sizeof(int));
    grid[(M / 2) * M + (M / 2)] = 1;

    GLuint ssbo[2];
    glGenBuffers(2, ssbo);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo[0]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, total * sizeof(int), grid, GL_DYNAMIC_COPY);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo[1]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, total * sizeof(int), NULL, GL_DYNAMIC_COPY);

    GLint u_size = glGetUniformLocation(program, "u_size");
    GLint u_seed = glGetUniformLocation(program, "u_seed");
    glUniform1i(u_size, M);

    int epoch = 0;
    int in_buf = 0, out_buf = 1;

    if (M <= 20) print_grid(grid, epoch);

    while (1) {
        int burning_count = 0;
        for (int i = 0; i < total; i++) {
            if (grid[i] == 1) burning_count++;
        }
        if (burning_count == 0) break;

        epoch++;
        glUniform1f(u_seed, (float)rand() / RAND_MAX);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo[in_buf]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo[out_buf]);

        GLuint groups = (M + 15) / 16;
        glDispatchCompute(groups, groups, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo[out_buf]);
        void *ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, total * sizeof(int), GL_MAP_READ_BIT);
        if (ptr) {
            memcpy(grid, ptr, total * sizeof(int));
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }

        if (M <= 20) print_grid(grid, epoch);

        int temp = in_buf;
        in_buf = out_buf;
        out_buf = temp;
    }

    printf("Fire extinguished in %d epochs for M = %d.\n", epoch, M);

    free(grid);
    glDeleteBuffers(2, ssbo);
    glDeleteProgram(program);
    eglDestroyContext(display, context);
    eglTerminate(display);

    return 0;
}
