#version 330 core
out vec4 FragColor;

in vec2 frag_tex_coord;

uniform ivec4     in_visible_channels;
uniform sampler2D in_texture;

void main()
{
    //FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);
    FragColor = texture(in_texture, frag_tex_coord);

    if (in_visible_channels.r == 0)
        FragColor.r = 0;

    if (in_visible_channels.g == 0)
        FragColor.g = 0;

    if (in_visible_channels.b == 0)
        FragColor.b = 0;

    if (in_visible_channels.a == 0)
        FragColor.a = 0;
}
