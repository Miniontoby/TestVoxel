#ifndef FONTRENDERER_H
#define FONTRENDERER_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <stb_truetype.h>

struct Dimensions2D
{
    int width{};
    int height{};
};

class FontRenderer {
public:
    GLuint vao = 0, vbo = 0;
    GLuint fontTextureID = 0;
    stbtt_bakedchar cdata[96];

    FontRenderer();
    ~FontRenderer();

    bool init(const std::string& fontPath);
    void renderText(GLuint fontShader, const std::string& text, float x,
            float y, float scaleX, float scaleY, glm::vec4 color,
            Dimensions2D windowSize);

private:
    void drawGLFontQuads(GLuint fontShader, glm::vec4 color,
            Dimensions2D resolution,
            const std::vector<float>& vertices);
};

#endif // FONTRENDERER_H
