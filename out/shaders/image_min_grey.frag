#version 410 core
out vec4 FragColor;

in vec2 frag_tex_coord;

uniform sampler2D in_texture;
uniform int       in_min_is_white;
uniform int       in_invert_colors;

void main()
{
    float grey_color  = texture(in_texture, frag_tex_coord).r;

    if ( in_min_is_white == 1 )
        grey_color = 1.f - grey_color;

    // separate intentionally, otherwise min_is_white wont invert
    if ( in_invert_colors == 1 )
        grey_color = 1.f - grey_color;

    FragColor.r  = grey_color;
    FragColor.gb = FragColor.rr;
    FragColor.a  = 1;
}
