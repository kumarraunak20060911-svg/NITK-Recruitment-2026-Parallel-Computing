#define _POSIX_C_SOURCE 200809L

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl31.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define HEALTHY  0u
#define BURNING  1u
#define NOTHING  2u

#define FIRE_PROBABILITY 0.15f
#define WG_SIZE 16

static const char *CS_SRC =
    "#version 310 es\n"
    "layout(local_size_x=16,local_size_y=16) in;\n"
    "layout(std430,binding=0) readonly buffer A{uint in_grid[];};\n"
    "layout(std430,binding=1) writeonly buffer B{uint out_grid[];};\n"
    "layout(std430,binding=2) buffer C{uint burning_count;};\n"
    "uniform int u_m;\n"
    "uniform uint u_seed;\n"
    "uniform uint u_epoch;\n"
    "const uint HEALTHY=0u;\n"
    "const uint BURNING=1u;\n"
    "const uint NOTHING=2u;\n"
    "const uint FIRE_THRESHOLD=644245094u;\n"
    "shared uint tile[324];\n"
    "shared uint counts[256];\n"

    "uint load_cell(int x,int y){\n"
    "    if(x<0||y<0||x>=u_m||y>=u_m)return NOTHING;\n"
    "    return in_grid[y*u_m+x];\n"
    "}\n"

    "uint hash32(uint x){\n"
    "    x^=x>>16;\n"
    "    x*=0x7feb352du;\n"
    "    x^=x>>15;\n"
    "    x*=0x846ca68bu;\n"
    "    x^=x>>16;\n"
    "    return x;\n"
    "}\n"

    "uint random_value(ivec2 p){\n"
    "    uint h=uint(p.x)*0x9E3779B1u;\n"
    "    h^=uint(p.y)*0x85EBCA77u;\n"
    "    h^=u_seed;\n"
    "    h^=u_epoch*0xC2B2AE3Du;\n"
    "    return hash32(h);\n"
    "}\n"

    "void main(){\n"
    "    ivec2 gid=ivec2(gl_GlobalInvocationID.xy);\n"
    "    ivec2 lid=ivec2(gl_LocalInvocationID.xy);\n"
    "    int lx=lid.x;\n"
    "    int ly=lid.y;\n"
    "    int sx=lx+1;\n"
    "    int sy=ly+1;\n"
    "    int c=sy*18+sx;\n"

    "    tile[c]=load_cell(gid.x,gid.y);\n"

    "    if(lx==0)\n"
    "        tile[sy*18]=load_cell(gid.x-1,gid.y);\n"

    "    if(lx==15)\n"
    "        tile[sy*18+17]=load_cell(gid.x+1,gid.y);\n"

    "    if(ly==0)\n"
    "        tile[sx]=load_cell(gid.x,gid.y-1);\n"

    "    if(ly==15)\n"
    "        tile[17*18+sx]=load_cell(gid.x,gid.y+1);\n"

    "    if(lx==0&&ly==0)\n"
    "        tile[0]=load_cell(gid.x-1,gid.y-1);\n"

    "    if(lx==15&&ly==0)\n"
    "        tile[17]=load_cell(gid.x+1,gid.y-1);\n"

    "    if(lx==0&&ly==15)\n"
    "        tile[17*18]=load_cell(gid.x-1,gid.y+1);\n"

    "    if(lx==15&&ly==15)\n"
    "        tile[17*18+17]=load_cell(gid.x+1,gid.y+1);\n"

    "    barrier();\n"

    "    uint current=tile[c];\n"
    "    uint next=current;\n"

    "    if(gid.x<u_m&&gid.y<u_m){\n"

    "        if(current==BURNING){\n"
    "            next=NOTHING;\n"
    "        }\n"
    "        else if(current==HEALTHY){\n"
    "            bool fire=false;\n"

    "            fire=fire||(tile[(sy-1)*18+(sx-1)]==BURNING);\n"
    "            fire=fire||(tile[(sy-1)*18+sx]==BURNING);\n"
    "            fire=fire||(tile[(sy-1)*18+(sx+1)]==BURNING);\n"
    "            fire=fire||(tile[sy*18+(sx-1)]==BURNING);\n"
    "            fire=fire||(tile[sy*18+(sx+1)]==BURNING);\n"
    "            fire=fire||(tile[(sy+1)*18+(sx-1)]==BURNING);\n"
    "            fire=fire||(tile[(sy+1)*18+sx]==BURNING);\n"
    "            fire=fire||(tile[(sy+1)*18+(sx+1)]==BURNING);\n"

    "            if(fire&&random_value(gid)<FIRE_THRESHOLD)\n"
    "                next=BURNING;\n"
    "        }\n"

    "        out_grid[gid.y*u_m+gid.x]=next;\n"
    "    }\n"

    "    uint id=uint(ly*16+lx);\n"
    "    counts[id]=0u;\n"

    "    if(gid.x<u_m&&gid.y<u_m&&next==BURNING)\n"
    "        counts[id]=1u;\n"

    "    barrier();\n"

    "    for(uint s=128u;s>0u;s>>=1u){\n"
    "        if(id<s)\n"
    "            counts[id]+=counts[id+s];\n"
    "        barrier();\n"
    "    }\n"

    "    if(id==0u&&counts[0]!=0u)\n"
    "        atomicAdd(burning_count,counts[0]);\n"
    "}\n";


static void egl_fail(const char *s)
{
    fprintf(stderr,
            "EGL error at %s: 0x%04x\n",
            s,
            (unsigned)eglGetError());
    exit(EXIT_FAILURE);
}


static void gl_fail(const char *s)
{
    fprintf(stderr,
            "OpenGL ES error at %s: 0x%04x\n",
            s,
            (unsigned)glGetError());
    exit(EXIT_FAILURE);
}


static bool has_egl_extension(const char *extensions,
                              const char *name)
{
    if (!extensions || !name)
        return false;

    size_t len = strlen(name);
    const char *p = extensions;

    while ((p = strstr(p, name)) != NULL) {
        bool left_ok = (p == extensions || p[-1] == ' ');
        bool right_ok = (p[len] == '\0' || p[len] == ' ');

        if (left_ok && right_ok)
            return true;

        p += len;
    }

    return false;
}


static GLuint make_program(void)
{
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);

    if (!shader)
        gl_fail("glCreateShader");

    glShaderSource(shader, 1, &CS_SRC, NULL);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);

    if (!ok) {
        char log[4096];

        glGetShaderInfoLog(
            shader,
            sizeof(log),
            NULL,
            log
        );

        fprintf(stderr,
                "Shader error:\n%s\n",
                log);

        glDeleteShader(shader);
        exit(EXIT_FAILURE);
    }

    GLuint program = glCreateProgram();

    if (!program)
        gl_fail("glCreateProgram");

    glAttachShader(program, shader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &ok);

    if (!ok) {
        char log[4096];

        glGetProgramInfoLog(
            program,
            sizeof(log),
            NULL,
            log
        );

        fprintf(stderr,
                "Program error:\n%s\n",
                log);

        glDeleteProgram(program);
        glDeleteShader(shader);
        exit(EXIT_FAILURE);
    }

    glDeleteShader(shader);

    return program;
}


static void print_grid(const uint32_t *grid,
                       int m,
                       int epoch)
{
    printf("Epoch %d:\n", epoch);

    for (int y = 0; y < m; ++y) {
        for (int x = 0; x < m; ++x) {
            uint32_t state = grid[y * m + x];

            if (state == HEALTHY)
                putchar('H');
            else if (state == BURNING)
                putchar('B');
            else
                putchar('.');

            putchar(' ');
        }

        putchar('\n');
    }

    putchar('\n');
}


static void reset_counter(GLuint buffer)
{
    uint32_t zero = 0;

    glBindBuffer(
        GL_SHADER_STORAGE_BUFFER,
        buffer
    );

    glBufferSubData(
        GL_SHADER_STORAGE_BUFFER,
        0,
        sizeof(zero),
        &zero
    );

    if (glGetError() != GL_NO_ERROR)
        gl_fail("counter reset");
}


static uint32_t read_counter(GLuint buffer)
{
    uint32_t value = 0;

    glBindBuffer(
        GL_SHADER_STORAGE_BUFFER,
        buffer
    );

    void *ptr = glMapBufferRange(
        GL_SHADER_STORAGE_BUFFER,
        0,
        sizeof(value),
        GL_MAP_READ_BIT
    );

    if (!ptr)
        gl_fail("counter map");

    memcpy(&value, ptr, sizeof(value));

    if (!glUnmapBuffer(GL_SHADER_STORAGE_BUFFER))
        gl_fail("counter unmap");

    return value;
}


static void read_grid(GLuint buffer,
                      uint32_t *dst,
                      size_t bytes)
{
    glBindBuffer(
        GL_SHADER_STORAGE_BUFFER,
        buffer
    );

    void *ptr = glMapBufferRange(
        GL_SHADER_STORAGE_BUFFER,
        0,
        bytes,
        GL_MAP_READ_BIT
    );

    if (!ptr)
        gl_fail("grid map");

    memcpy(dst, ptr, bytes);

    if (!glUnmapBuffer(GL_SHADER_STORAGE_BUFFER))
        gl_fail("grid unmap");
}


static int simulate(GLuint program,
                    int m,
                    uint32_t seed)
{
    size_t cells = (size_t)m * (size_t)m;
    size_t bytes = cells * sizeof(uint32_t);

    uint32_t *grid = malloc(bytes);

    if (!grid)
        return -1;

    for (size_t i = 0; i < cells; ++i)
        grid[i] = HEALTHY;

    grid[(m / 2) * m + (m / 2)] = BURNING;

    GLuint ssbo[3];

    glGenBuffers(3, ssbo);

    for (int i = 0; i < 2; ++i) {
        glBindBuffer(
            GL_SHADER_STORAGE_BUFFER,
            ssbo[i]
        );

        glBufferData(
            GL_SHADER_STORAGE_BUFFER,
            bytes,
            grid,
            GL_DYNAMIC_COPY
        );
    }

    uint32_t zero = 0;

    glBindBuffer(
        GL_SHADER_STORAGE_BUFFER,
        ssbo[2]
    );

    glBufferData(
        GL_SHADER_STORAGE_BUFFER,
        sizeof(zero),
        &zero,
        GL_DYNAMIC_COPY
    );

    GLint u_m =
        glGetUniformLocation(program, "u_m");

    GLint u_seed =
        glGetUniformLocation(program, "u_seed");

    GLint u_epoch =
        glGetUniformLocation(program, "u_epoch");

    if (u_m < 0 ||
        u_seed < 0 ||
        u_epoch < 0) {

        fprintf(stderr,
                "Uniform lookup failed\n");

        exit(EXIT_FAILURE);
    }

    uint32_t local_seed =
        seed ^ ((uint32_t)m * 0x9E3779B9u);

    glUseProgram(program);

    glUniform1i(u_m, m);
    glUniform1ui(u_seed, local_seed);

    int read_index = 0;
    int epoch = 0;
    uint32_t burning = 1;

    bool show_grid = (m <= 20);

    if (show_grid)
        print_grid(grid, m, epoch);

    struct timespec start;
    struct timespec finish;

    clock_gettime(
        CLOCK_MONOTONIC,
        &start
    );

    while (burning != 0) {
        int write_index = 1 - read_index;

        reset_counter(ssbo[2]);

        glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            0,
            ssbo[read_index]
        );

        glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            1,
            ssbo[write_index]
        );

        glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            2,
            ssbo[2]
        );

        glUniform1ui(
            u_epoch,
            (GLuint)(epoch + 1)
        );

        GLuint groups =
            (GLuint)((m + WG_SIZE - 1) / WG_SIZE);

        glDispatchCompute(
            groups,
            groups,
            1
        );

        if (glGetError() != GL_NO_ERROR)
            gl_fail("compute dispatch");

        glMemoryBarrier(
            GL_SHADER_STORAGE_BARRIER_BIT |
            GL_BUFFER_UPDATE_BARRIER_BIT
        );

        burning = read_counter(ssbo[2]);

        ++epoch;

        if (show_grid) {
            read_grid(
                ssbo[write_index],
                grid,
                bytes
            );

            print_grid(
                grid,
                m,
                epoch
            );
        }

        read_index = write_index;
    }

    clock_gettime(
        CLOCK_MONOTONIC,
        &finish
    );

    double elapsed =
        (double)(finish.tv_sec - start.tv_sec) +
        (double)(finish.tv_nsec - start.tv_nsec) * 1e-9;

    printf(
        "[M=%d] Extinguished in %d epochs | Time: %.6f s\n",
        m,
        epoch,
        elapsed
    );

    glDeleteBuffers(3, ssbo);

    free(grid);

    return epoch;
}


int main(void)
{
    const int sizes[] = {
        16,
        64,
        256,
        512
    };

    const size_t size_count =
        sizeof(sizes) / sizeof(sizes[0]);

    const uint32_t seed = 12345u;

    EGLDisplay display =
        eglGetDisplay(EGL_DEFAULT_DISPLAY);

    if (display == EGL_NO_DISPLAY)
        egl_fail("eglGetDisplay");

    if (!eglInitialize(
            display,
            NULL,
            NULL))
        egl_fail("eglInitialize");

    if (!eglBindAPI(EGL_OPENGL_ES_API))
        egl_fail("eglBindAPI");

    const char *extensions =
        eglQueryString(
            display,
            EGL_EXTENSIONS
        );

    bool surfaceless =
        has_egl_extension(
            extensions,
            "EGL_KHR_surfaceless_context"
        );

    const EGLint config_attrs[] = {
        EGL_RENDERABLE_TYPE,
        EGL_OPENGL_ES3_BIT_KHR,
        EGL_SURFACE_TYPE,
        EGL_PBUFFER_BIT,
        EGL_NONE
    };

    EGLConfig config = NULL;
    EGLint config_count = 0;

    if (!eglChooseConfig(
            display,
            config_attrs,
            &config,
            1,
            &config_count))
        egl_fail("eglChooseConfig");

    if (config_count == 0) {
        fprintf(
            stderr,
            "No suitable EGL configuration\n"
        );

        return EXIT_FAILURE;
    }

    const EGLint context_attrs[] = {
        EGL_CONTEXT_CLIENT_VERSION,
        3,
        EGL_NONE
    };

    EGLContext context =
        eglCreateContext(
            display,
            config,
            EGL_NO_CONTEXT,
            context_attrs
        );

    if (context == EGL_NO_CONTEXT)
        egl_fail("eglCreateContext");

    EGLSurface surface = EGL_NO_SURFACE;

    if (surfaceless) {
        if (!eglMakeCurrent(
                display,
                EGL_NO_SURFACE,
                EGL_NO_SURFACE,
                context))
            egl_fail("eglMakeCurrent");
    } else {
        const EGLint pbuffer_attrs[] = {
            EGL_WIDTH,
            1,
            EGL_HEIGHT,
            1,
            EGL_NONE
        };

        surface =
            eglCreatePbufferSurface(
                display,
                config,
                pbuffer_attrs
            );

        if (surface == EGL_NO_SURFACE)
            egl_fail("eglCreatePbufferSurface");

        if (!eglMakeCurrent(
                display,
                surface,
                surface,
                context))
            egl_fail("eglMakeCurrent");
    }

    const char *version =
        (const char *)glGetString(GL_VERSION);

    const char *renderer =
        (const char *)glGetString(GL_RENDERER);

    printf(
        "OpenGL ES: %s\n",
        version ? version : "unknown"
    );

    printf(
        "Renderer: %s\n",
        renderer ? renderer : "unknown"
    );

    GLint major = 0;
    GLint minor = 0;

    glGetIntegerv(
        GL_MAJOR_VERSION,
        &major
    );

    glGetIntegerv(
        GL_MINOR_VERSION,
        &minor
    );

    if (major < 3 ||
        (major == 3 && minor < 1)) {

        fprintf(
            stderr,
            "OpenGL ES 3.1 required\n"
        );

        return EXIT_FAILURE;
    }

    GLint max_shared = 0;

    glGetIntegerv(
        GL_MAX_COMPUTE_SHARED_MEMORY_SIZE,
        &max_shared
    );

    const GLint required_shared =
        324 * (GLint)sizeof(uint32_t) +
        256 * (GLint)sizeof(uint32_t);

    if (max_shared < required_shared) {
        fprintf(
            stderr,
            "Insufficient compute shared memory\n"
        );

        return EXIT_FAILURE;
    }

    GLuint program = make_program();

    printf(
        "Ignition probability: %.2f\n"
        "Seed: %u\n"
        "Surfaceless: %s\n\n",
        FIRE_PROBABILITY,
        seed,
        surfaceless ? "yes" : "no"
    );

    for (size_t i = 0;
         i < size_count;
         ++i) {

        simulate(
            program,
            sizes[i],
            seed
        );
    }

    glDeleteProgram(program);

    eglMakeCurrent(
        display,
        EGL_NO_SURFACE,
        EGL_NO_SURFACE,
        EGL_NO_CONTEXT
    );

    if (surface != EGL_NO_SURFACE)
        eglDestroySurface(
            display,
            surface
        );

    eglDestroyContext(
        display,
        context
    );

    eglTerminate(display);

    return EXIT_SUCCESS;
}
