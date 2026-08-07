#version 330 core
out vec4 FragColor;

in vec2 frag_tex_coord;
uniform sampler2D in_texture;

void main()
{
    //FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);
    FragColor = texture(in_texture, frag_tex_coord);
}
