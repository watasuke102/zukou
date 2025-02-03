#version 320 es
precision mediump float;

uniform sampler2D texture0;
uniform vec4 color;
out vec4 outputColor;
in vec2 uv;

void
main()
{
  outputColor = texture(texture0, uv)+color;
}
