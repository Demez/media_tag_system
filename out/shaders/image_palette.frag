#version 410 core
out vec4 FragColor;

in vec2 frag_tex_coord;

uniform ivec4      in_visible_channels;
uniform usampler2D in_texture;
uniform usampler1D in_palette;
uniform int        in_palette_trans;
uniform int        in_palette_count;

void main()
{
    uint palette_index   = texture(in_texture, frag_tex_coord).r;
    //uint palette_index   =  texelFetch(in_texture, frag_tex_coord, 0).r;
    //int palette_index_int = int(round(palette_index * (in_palette_count - 1)));

    // texelFetch works with integers
    //int raw_color = texture( in_palette, uint(palette_index) );
    uvec4 raw_color = texelFetch(in_palette, int(palette_index), 0);

    // convert from 0-255 range, to 0-1 range
    FragColor = vec4(raw_color) / 255.f;
    FragColor.a = 1;

    if ( in_palette_trans == int(palette_index) )
        FragColor.a = 0;
}
