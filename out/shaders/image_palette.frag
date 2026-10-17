#version 410 core
out vec4 FragColor;

in vec2 frag_tex_coord;

uniform ivec4     in_visible_channels;
uniform sampler2D in_texture;
uniform sampler1D in_palette;
uniform int       in_palette_trans;

void main()
{
    float palette_index  = texture(in_texture, frag_tex_coord).r;
    //float palette_coord = palette_index / 256.0;
    FragColor = texture( in_palette, palette_index );

    FragColor.a = 1;

    //if ( in_palette_trans < 256 )
        if ( in_palette_trans == int(palette_index * 255.0) )
            FragColor.a = 0;

    //FragColor = texture(in_texture, frag_tex_coord);

    if (in_visible_channels.r == 0)
        FragColor.r = 0;

    if (in_visible_channels.g == 0)
        FragColor.g = 0;

    if (in_visible_channels.b == 0)
        FragColor.b = 0;

    if (in_visible_channels.a == 0)
        FragColor.a = 0;
}
