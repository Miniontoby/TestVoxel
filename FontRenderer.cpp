#include "FontRenderer.h"
#include <iostream>
#include <fstream>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifndef GL_TEXTURE_SWIZZLE_RGBA
#define GL_TEXTURE_SWIZZLE_RGBA 0x8E46
#endif

FontRenderer::FontRenderer() {}

FontRenderer::~FontRenderer() {
    if (vao != 0) glDeleteVertexArrays(1, &vao);
    if (vbo != 0) glDeleteBuffers(1, &vbo);
    if (fontTextureID != 0) glDeleteTextures(1, &fontTextureID);
}

bool FontRenderer::init(const std::string& fontPath) {
    std::ifstream file(fontPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cout << "[Font Error] Failed to open font file: " << fontPath << std::endl;
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<unsigned char> fontBuffer(size);
    if (!file.read((char*)fontBuffer.data(), size)) {
        std::cout << "[Font Error] Failed to read font file data." << std::endl;
        return false;
    }

    int bitmapWidth = 512;
    int bitmapHeight = 512;
    std::vector<unsigned char> bitmap(bitmapWidth * bitmapHeight);

    stbtt_BakeFontBitmap(fontBuffer.data(), 0, 32.0f, bitmap.data(), bitmapWidth, bitmapHeight, 32, 96, cdata);

    glGenTextures(1, &fontTextureID);
    glBindTexture(GL_TEXTURE_2D, fontTextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, bitmapWidth, bitmapHeight, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap.data());
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    std::cout << "[Font Success] Loaded TrueType font: " << fontPath << std::endl;
    return true;
}

void FontRenderer::renderText(GLuint fontShader, const std::string& text,
        float x, float y, float scaleX, float scaleY, glm::vec4 color,
        Dimensions2D resolution) {
    if (text.empty() || fontTextureID == 0) return;

    // The vertices are flattened quads, in the following format:
    // { x, y, tx, ty, ...}
    std::vector<float> vertices = this->generateFontVertices(x, y, scaleX,
            scaleY, resolution, text);

    if (vertices.empty()) return;

    this->drawGLFontQuads(fontShader, color, resolution, vertices);
}

std::vector<float> FontRenderer::generateFontVertices(float x, float y,
        float scaleX, float scaleY, Dimensions2D resolution,
        std::string_view text) const
{
    std::vector<float> vertices;
    float rx = 0.0f;
    float ry = 0.0f;

    float aspect = static_cast<float>(resolution.width) / static_cast<float>(resolution.height);
    float charWidth = scaleX / aspect;
    float charHeight = scaleY;

    for (size_t i = 0; i < text.length(); ++i) {
        int charIndex = text[i];
        if (charIndex < 32 || charIndex >= 128) continue;

        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(cdata, 512, 512, charIndex - 32, &rx, &ry, &q, 1);

        float x0 = x + (i * charWidth);
        float x1 = x0 + charWidth;
        float y0 = y;
        float y1 = y0 + charHeight;

        float quad[] = {
            x0, y0, q.s0, q.t0,
            x1, y0, q.s1, q.t0,
            x1, y1, q.s1, q.t1,

            x0, y0, q.s0, q.t0,
            x1, y1, q.s1, q.t1,
            x0, y1, q.s0, q.t1
        };
        vertices.insert(vertices.end(), std::begin(quad), std::end(quad));
    }

    return vertices;
}

void FontRenderer::drawGLFontQuads(GLuint fontShader, glm::vec4 color,
        Dimensions2D resolution, const std::vector<float>& vertices)
{
    glDisable(GL_DEPTH_TEST);
    glUseProgram(fontShader);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fontTextureID);
    glUniform1i(glGetUniformLocation(fontShader, "fontTexture"), 0);
    glUniform4f(glGetUniformLocation(fontShader, "textColor"), color.r, color.g, color.b, color.a);

    glm::mat4 projection = glm::ortho(0.0f,
            static_cast<float>(resolution.width),
            static_cast<float>(resolution.height), 0.0f);
    glUniformMatrix4fv(glGetUniformLocation(fontShader, "projection"), 1,
            GL_FALSE, glm::value_ptr(projection));

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);

    GLsizei stride = 4 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 4));

    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}
