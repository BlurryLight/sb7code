/*
* Copyright © 2012-2015 Graham Sellers
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice (including the next
* paragraph) shall be included in all copies or substantial portions of the
* Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

#include <sb7.h>
#include <shader.h>
#include <sb7ktx.h>
#include <sb7color.h>
#include <iostream>

#define TEXTURE_WIDTH 512
#define TEXTURE_HEIGHT 512

class rgtccompressor : public sb7::application
{
public:
    rgtccompressor();
    void init();
    void startup();
    void render(double T);
    void onKey(int key, int action);

private:
    GLuint          compress_program;
    GLuint          render_program;
    GLuint          input_texture;
    GLuint          output_texture;
    GLuint          output_buffer;
    GLuint          output_buffer_texture;
    GLuint          dummy_vao;

    enum
    {
        SHOW_INPUT = 0,
        SHOW_OUTPUT,
    };

    int display_mode;

    void            load_shaders();
};

rgtccompressor::rgtccompressor()
    : compress_program(0),
      render_program(0),
      display_mode(SHOW_INPUT)
{
}

void rgtccompressor::init()
{
    static const char title[] = "OpenGL SuperBible - RGTC Compressor";

    sb7::application::init();

    memcpy(info.title, title, sizeof(title));
    info.windowWidth = 512;
    info.windowHeight = 512;
}

void rgtccompressor::startup()
{
    input_texture = sb7::ktx::file::load("media/textures/gllogodistsm.ktx");

    glGenTextures(1, &output_texture);
    glBindTexture(GL_TEXTURE_2D, output_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // glTexStorage2D(GL_TEXTURE_2D, 1, GL_COMPRESSED_RED_RGTC1, TEXTURE_WIDTH / 4, TEXTURE_HEIGHT / 4);

    glGenBuffers(1, &output_buffer);
    glBindBuffer(GL_TEXTURE_BUFFER, output_buffer);
    // 初始化 immutable 空间，只读。 // 这个只读应该指的是cpu端只读，在gpu端是可以通过cs写的。
    // 如果要 放mvp之类的东西，可以映射成 GL_DYNAMIC_STORAGE_BIT 以方便使用glBufferSubData
    // https://registry.khronos.org/OpenGL-Refpages/gl4/html/glBufferStorage.xhtml
    glBufferStorage(GL_TEXTURE_BUFFER, TEXTURE_WIDTH * TEXTURE_HEIGHT / 2, NULL, GL_MAP_READ_BIT);

    glGenTextures(1, &output_buffer_texture);
    glBindTexture(GL_TEXTURE_BUFFER, output_buffer_texture);
    // gl_texture_buffer 的存储空间来自某一个buffer object
    // https://www.khronos.org/opengl/wiki/Buffer_Texture
    // 指定output_buffer_texture的内容来自output_buffer
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RG32UI, output_buffer);

    load_shaders();

    glGenVertexArrays(1, &dummy_vao);
    display_mode = SHOW_OUTPUT;
}

void rgtccompressor::render(double T)
{
    glClearBufferfv(GL_COLOR, 0, sb7::color::Black);

    Sleep(1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, input_texture);
    // 把一个纹理的某一层绑定到某个*image*上，*image*可以通过shader读写
    // https://www.khronos.org/opengl/wiki/Texture
    // image和texture是完全区分开的，不能混用。 Texture指的是一个对象，包含许多状态。而Image存储在Texture里面，保持着数据。
    // layout(binding = 0) 这种被叫做texture image unit
    // 比如一个GL_TEXTURE_2D_ARRAY Texture对象，里面可能包含很多个image unit,绑定的时候可以指定只需要绑定某一个image
    glBindImageTexture(0, output_buffer_texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG32UI);

    glUseProgram(compress_program);
    glDispatchCompute(TEXTURE_WIDTH / 4, TEXTURE_WIDTH / 4, 1);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // 把这个buffer映射回cpu端，没有用
    // unsigned char * ptr = (unsigned char *)glMapBufferRange(GL_TEXTURE_BUFFER, 0, TEXTURE_WIDTH * TEXTURE_HEIGHT / 2, GL_MAP_READ_BIT);
    // glUnmapBuffer(GL_TEXTURE_BUFFER);

    // https://registry.khronos.org/OpenGL-Refpages/es2.0/xhtml/glCompressedTexImage2D.xml
    // https://registry.khronos.org/OpenGL-Refpages/es2.0/xhtml/glCompressedTexImage2D.xml
    // 这三个指令是连着用的。 当`GL_PIXEL_UNPACK_BUFFER`绑定到一个BUFFER对象时，glCompressTexImage2D的最后一项的含义发生改变，原来的含义是指向一个数据来源
    // 改变后的含义指的在Buffer对象内的偏移量
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, output_buffer);
    glBindTexture(GL_TEXTURE_2D, output_texture);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, TEXTURE_WIDTH * TEXTURE_HEIGHT / 2, NULL);

    // output_buffer, output_texture, output_buffer_texture指的都是同一个东西。

    glViewport(0, 0, info.windowWidth, info.windowHeight);

    glBindVertexArray(dummy_vao);
    glUseProgram(render_program);
    // glBindTexture(GL_TEXTURE_2D, input_texture);
    switch (display_mode)
    {
        case SHOW_INPUT:
            glBindTexture(GL_TEXTURE_2D, input_texture);
            break;
        case SHOW_OUTPUT:
            glBindTexture(GL_TEXTURE_2D, output_texture);
            break;
    }
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}


void rgtccompressor::onKey(int key, int action)
{
    if (action)
    {
        switch (key)
        {
            case 'M':
                display_mode = (display_mode + 1) % 2;
                if(display_mode == SHOW_INPUT)
                {
                    std::cout << "show input" << std::endl;
                }
                else
                {
                    std::cout << "show output" << std::endl;
                }
                break;
            case 'R':
                load_shaders();
                break;
        }
    }
}

void rgtccompressor::load_shaders()
{
    glDeleteProgram(compress_program);
    glDeleteProgram(render_program);

    GLuint cs;

    cs = sb7::shader::load("media/shaders/rgtc/rgtccompress.cs.glsl", GL_COMPUTE_SHADER);

    compress_program = sb7::program::link_from_shaders(&cs, 1, true);

    GLuint shaders[2];

    shaders[0] = sb7::shader::load("media/shaders/rgtc/drawquad.fs.glsl", GL_FRAGMENT_SHADER);
    shaders[1] = sb7::shader::load("media/shaders/rgtc/drawquad.vs.glsl", GL_VERTEX_SHADER);

    render_program = sb7::program::link_from_shaders(shaders, 2, true);
}

DECLARE_MAIN(rgtccompressor)
