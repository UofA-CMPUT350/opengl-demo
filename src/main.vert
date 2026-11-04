#version 120

uniform vec2 windowSize;

attribute vec2 position;
attribute vec2 texCoordsIn;

varying vec2 texCoords;  // passed on to the fragshader

void main() {
    // remember that sfml's texture memory layout is different from what OpenGL expects...
    // how can we fix this?
    texCoords = texCoordsIn;
    gl_Position = vec4((position / windowSize) * 2 - 1,  // rescale to [-1, 1]^2
                       0, 1);
}
