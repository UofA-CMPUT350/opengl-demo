#include "glad/glad.h"
#define GL_SILENCE_DEPRECATION

#include <filesystem>
#include <iostream>

#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

// helper macro which checks if errors have occurred.
#define glCheck(expr)                                                                              \
    expr;                                                                                          \
    if (int e = glGetError()) {                                                                    \
        std::cerr << "OpenGL error: " << e << " in file " __FILE__ << ", line number " << __LINE__ \
                  << '\n';                                                                         \
        abort();                                                                                   \
    }

int main() {
    // OpenGL settings we request for:
    sf::ContextSettings settings;
    settings.depthBits = 24;   // 24-bit depth buffer
    settings.stencilBits = 8;  // 8-bit stencil buffer
    // Request OpenGL 2.1 for compatibility with both SFML/Graphics AND macOS:
    //  - SFML's Graphics module implementation relies on legacy fixed-function pipeline features
    //     of OpenGL, which means only up to OpenGL 2.1. Any newer version requires Compatibility
    //     profile (i.e., maintaining legacy features).
    //  - macOS has deprecated OpenGL support, meaning for anything newer than 2.1 (i.e., 3.0 and
    //      newer), only Core profile is supported, i.e., REMOVING legacy fixed-function features.
    // As a result, if you want to both support macOS AND use SFML's Graphics features (e.g.,
    // sf::RenderWindow), you can use at latest OpenGL 2.1, which is a nightmare.
    settings.majorVersion = 2;
    settings.minorVersion = 1;
    sf::RenderWindow window(sf::VideoMode({1280, 720}), "Shaders", sf::State::Windowed, settings);
    (void)window.setActive();  // set window as OpenGL target

    if (!gladLoadGLLoader((GLADloadproc)sf::Context::getFunction)) {
        // Handle GLAD initialization error
        return -1;
    }

    // Query the settings we actually received (may be different):
    const auto& actualSettings = window.getSettings();
    std::cout << "OpenGL version " << actualSettings.majorVersion << "."
              << actualSettings.minorVersion << " flags: " << actualSettings.attributeFlags << '\n';
    std::cout << "Maximum texture width (or height): " << sf::Texture::getMaximumSize() << '\n';

    // Compile shader program.
    // This gets us shader shader.getNativeHandle() as the OpenGL shader program name.
    sf::Shader shader(std::filesystem::path{"main.vert"}, std::filesystem::path{"main.frag"});

    // get vertex attribute locations from shader program
    GLint positionAttribLocation = glGetAttribLocation(shader.getNativeHandle(), "position");
    glCheck();
    GLint texCoordsInAttribLocation = glGetAttribLocation(shader.getNativeHandle(), "texCoordsIn");
    glCheck();
    // get uniform locations from shader program
    GLint windowSizeUniformLocation = glGetUniformLocation(shader.getNativeHandle(), "windowSize");
    glCheck();
    GLint textureUniformLocation = glGetUniformLocation(shader.getNativeHandle(), "texture");
    glCheck();

    // Prepare texture and vertices on CPU side.
    // We could just store these directly in [-1, 1]^2 range but we are intentionally storing
    // in pixels so that we can show how to use windowSize uniform to normalize.
    sf::Image pixels("background_fade_desert.png");
    sf::VertexArray vertices{};
    // bottom-left
    vertices.append(sf::Vertex{.position = {0, 0}, .texCoords = {0, 0}});
    // bottom-right
    vertices.append(
        sf::Vertex{.position = {static_cast<float>(window.getSize().x), 0}, .texCoords = {1, 0}});
    // top-right
    vertices.append(sf::Vertex{.position = sf::Vector2f(window.getSize()), .texCoords = {1, 1}});

    // Generate vertex buffer object name
    GLuint vbo;
    glCheck(glGenBuffers(1, &vbo));
    // Bind it to the GL_ARRAY_BUFFER target
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, vbo));
    // buffer the data in to vbo
    glCheck(
        glBufferData(GL_ARRAY_BUFFER, vertices.getVertexCount() * sizeof(sf::Vertex), &vertices[0],
                     GL_STATIC_DRAW));  // in this demo we won't be modifying the buffer => static.

    // Generate texture name
    GLuint texture;
    glCheck(glGenTextures(1, &texture));
    // Set texture unit 0 as active texture
    glCheck(glActiveTexture(GL_TEXTURE0));
    // Bind texture to GL_TEXTURE_2D target of texture unit 0
    glCheck(glBindTexture(GL_TEXTURE_2D, texture));
    // clamp to nearest valid pixel if you sample outside of [0, 1]^2
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    // nearest neighbour sampling, can also use GL_LINEAR for linear interpolation.
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    // store 2D image on GPU in bound texture -- note sf::Image stores internally channels as
    // uint8_t's.
    glCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pixels.getSize().x, pixels.getSize().y, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, pixels.getPixelsPtr()));

    // set up transparency
    glCheck(glEnable(GL_BLEND));
    glCheck(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    // clear color black
    glCheck(glClearColor(0, 0, 0, 1));
    while (window.isOpen()) {
        while (auto e = window.pollEvent()) {
            if (e->is<sf::Event::Closed>()) {
                glDeleteTextures(1, &texture);
                glDeleteBuffers(1, &vbo);
                return 0;
            }
        }
        // Must specify current program in use
        glUseProgram(shader.getNativeHandle());
        // set position attribute layout
        glCheck(glVertexAttribPointer(positionAttribLocation, 2, GL_FLOAT, GL_FALSE,
                                      sizeof(sf::Vertex), (const void*)0));
        // enable position attribute
        glEnableVertexAttribArray(positionAttribLocation);
        // set texCoordsIn attribute layout
        glCheck(glVertexAttribPointer(texCoordsInAttribLocation, 2, GL_FLOAT, GL_FALSE,
                                      sizeof(sf::Vertex),
                                      (const void*)(sizeof(sf::Vector2f) + sizeof(sf::Color))));
        // enable texCoordsIn attribute
        glEnableVertexAttribArray(texCoordsInAttribLocation);
        glCheck();

        // set texture sampler2D uniform
        glCheck(glUniform1i(textureUniformLocation, 0));  // 0 corresponding to texture unit 0
        // set windowSize uniform
        glCheck(glUniform2f(windowSizeUniformLocation, window.getSize().x, window.getSize().y));

        glCheck(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
        glCheck(glDrawArrays(GL_TRIANGLES, 0, 3));  // draw 3 vertices
        window.display();                           // display
        // unbind program
        glUseProgram(0);
    }
    return 0;
}
