#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl31.h>

static const char *CS_SRC =
    "#version 310 es\n"
    "layout(local_size_x = 16, local_size_y = 16) in;\n"
    "layout(std430, binding = 0) readonly buffer I { uint g_in[]; };\n"
    "layout(std430, binding = 1) writeonly buffer O { uint g_out[]; };\n"
    "uniform int u_m;\n"
    "uniform float u_s;\n"
    "\n"
    "float rnd(vec2 p) {\n"
    "    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    ivec2 p = ivec2(gl_GlobalInvocationID.xy);\n"
    "    if (p.x >= u_m || p.y >= u_m) return;\n"
    "    int idx = p.y * u_m + p.x;\n"
    "    uint st = g_in[idx];\n"
    "    uint nxt = st;\n"
    "\n"
    "    if (st == 1u) {\n"
    "        nxt = 2u;\n"
    "    } else if (st == 0u) {\n"
    "        bool f = false;\n"
    "        for (int dy = -1; dy <= 1 && !f; ++dy) {\n"
    "            for (int dx = -1; dx <= 1 && !f; ++dx) {\n"
    "                if (dx == 0 && dy == 0) continue;\n"
    "                int nx = p.x + dx, ny = p.y + dy;\n"
    "                if (nx >= 0 && nx < u_m && ny >= 0 && ny < u_m) {\n"
    "                    if (g_in[ny * u_m + nx] == 1u) f = true;\n"
    "                }\n"
    "            }\n"
    "        }\n"
    "        if (f && rnd(vec2(p) + vec2(u_s, u_s * 1.618)) < 0.15) nxt = 1u;\n"
    "    }\n"
    "    g_out[idx] = nxt;\n"
    "}\n";

static void dump_grid(const uint32_t *g, int m, int ep) {
    printf("Epoch %d:\n", ep);
    for (int r = 0; r < m; ++r) {
        for (int c = 0; c < m; ++c) {
            uint32_t v = g[r * m + c];
            putchar(v == 0 ? 'H' : (v == 1 ? 'B' : '.'));
            putchar(' ');
        }
        putchar('\n');
    }
    putchar('\n');
}

static GLuint make_shader(const char *src) {
    GLuint s = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "Shader compile failed:\n%s\n", log);
        exit(1);
    }
    return s;
}

static void sim_grid(EGLDisplay dpy, EGLContext ctx, int m) {
    size_t sz = (size_t)m * m * sizeof(uint32_t);
    uint32_t *buf = (uint32_t *)malloc(sz);
    for (int i = 0; i < m * m; ++i) buf[i] = 0;
    
    buf[(m / 2) * m + (m / 2)] = 1;

    GLuint b[2];
    glGenBuffers(2, b);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, b[0]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sz, buf, GL_DYNAMIC_COPY);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, b[1]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sz, buf, GL_DYNAMIC_COPY);

    GLuint cs = make_shader(CS_SRC);
    GLuint pg = glCreateProgram();
    glAttachShader(pg, cs);
    glLinkProgram(pg);

    GLint u_m_loc = glGetUniformLocation(pg, "u_m");
    GLint u_s_loc = glGetUniformLocation(pg, "u_s");

    glUseProgram(pg);
    glUniform1i(u_m_loc, m);

    int pp = 0, ep = 0;
    bool active = true;

    if (m <= 20) dump_grid(buf, m, ep);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    while (active) {
        ep++;
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, b[pp]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, b[1 - pp]);

        glUniform1f(u_s_loc, (float)rand() / (float)RAND_MAX + (float)ep);

        glDispatchCompute((m + 15) / 16, (m + 15) / 16, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, b[1 - pp]);
        void *ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sz, GL_MAP_READ_BIT);
        if (ptr) {
            memcpy(buf, ptr, sz);
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }

        active = false;
        for (int i = 0; i < m * m; ++i) {
            if (buf[i] == 1) { active = true; break; }
        }

        if (m <= 20) dump_grid(buf, m, ep);
        pp = 1 - pp;
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double dt = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;

    printf("[M=%d] Extinguished in %d epochs | GPU Compute Time: %.4f s\n", m, ep, dt);

    glDeleteBuffers(2, b);
    glDeleteProgram(pg);
    glDeleteShader(cs);
    free(buf);
}

int main(void) {
    srand((unsigned int)time(NULL));

    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (dpy == EGL_NO_DISPLAY) return 1;

    EGLint maj, min;
    if (!eglInitialize(dpy, &maj, &min)) return 1;

    EGLint cfg_attr[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR, EGL_NONE };
    EGLConfig cfg;
    EGLint n_cfg;
    eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n_cfg);

    EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
    if (ctx == EGL_NO_CONTEXT) return 1;

    if (!eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) return 1;

    int test_m[] = {16, 64, 256, 512};
    for (size_t i = 0; i < sizeof(test_m) / sizeof(test_m[0]); ++i) {
        sim_grid(dpy, ctx, test_m[i]);
    }

    eglDestroyContext(dpy, ctx);
    eglTerminate(dpy);
    return 0;
}
