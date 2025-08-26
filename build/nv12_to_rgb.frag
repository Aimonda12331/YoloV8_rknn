#version 300 es
precision mediump float;

uniform sampler2D texY;
uniform sampler2D texUV;

in vec2 TexCoord;
out vec3 outColor;

void main() {
    float y = texture(texY, TexCoord).r;
    vec2 uv = texture(texUV, TexCoord).ra - vec2(0.5, 0.5);  // U in R, V in A

    float r = y + 1.403 * uv.y;
    float g = y - 0.344 * uv.x - 0.714 * uv.y;
    float b = y + 1.770 * uv.x;

    outColor = vec3(r, g, b);
}
