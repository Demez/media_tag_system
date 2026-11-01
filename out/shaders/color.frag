#version 410 core
out vec4 FragColor;

uniform vec4 in_bg_color;

void main()
{
    FragColor = in_bg_color;
}
