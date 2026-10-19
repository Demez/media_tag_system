#version 410 core
out vec4 FragColor;

in vec2 frag_tex_coord;

uniform ivec4     in_visible_channels;
uniform sampler2D in_texture;
uniform sampler1D in_palette;
uniform int       in_palette_trans;
uniform int       in_palette_count;

void main()
{
    float palette_index = texture(in_texture, frag_tex_coord).r;
    //float palette_index_fl  = texture(in_texture, frag_tex_coord).r;
    //int palette_index_255 = palette_index_fl * 255;
    //float palette_index = in_palette_count / in_palette_count;

    FragColor = texture( in_palette, palette_index );
    FragColor.a = 1;

    if ( in_palette_trans == int(palette_index * 255.0) )
        FragColor.a = 0;

    //FragColor = texture(in_texture, frag_tex_coord);
}
