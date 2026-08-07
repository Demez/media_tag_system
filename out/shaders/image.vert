#version 330 core
layout (location = 0) in vec2 in_tex_coord;

uniform mat4 projection;

uniform vec2  window_size;
uniform vec2  image_size;
uniform vec2  image_pos;
uniform float image_rotation;

out vec2 frag_tex_coord;

precision highp float;

mat2 rotate_mat2( float angle )
{
	float s = sin( angle );
	float c = cos( angle );

	return mat2( c, -s, s, c );
}

void main()
{
    frag_tex_coord = in_tex_coord;

    mat2 rot_mat = rotate_mat2( image_rotation );


    vec2 pos_arr[4] = vec2[4](
        image_pos,
        vec2( image_pos.x + image_size.x, image_pos.y ),
        vec2( image_pos.x + image_size.x, image_pos.y + image_size.y ),
        vec2( image_pos.x, image_pos.y + image_size.y )
    );

    //const vec2 tex_coord_arr[4] = vec2[4](
    //    vec2( 0, 0 ),
    //    vec2( 1, 0 ),
    //    vec2( 1, 1 ),
    //    vec2( 0, 1 )
    //);

    vec2 base_pos  = pos_arr[ gl_VertexID ];
    //frag_tex_coord = tex_coord_arr[ gl_VertexID ];

    // get the center of the image
    vec2 image_center = image_pos + (image_size * 0.5);

    // bring center to origin of screen (0,0)
    vec2 centered_pos = base_pos - image_center;

    // rotate and translate back
    vec2 rotated_pos = rot_mat * centered_pos;
    vec2 final_pos = rotated_pos + image_center;

    gl_Position = projection * vec4( final_pos, 0.0, 1.0 );


    // used for the center of the image
    // we can apply the rotation to the center of the image instantly without having to offset it, rotate, then offset back
//    vec2 image_center = image_size * 0.5;
//    
//    vec2 local_offsets[4] = vec2[4](
//        vec2(-image_center.x, -image_center.y),
//        vec2( image_center.x, -image_center.y),
//        vec2( image_center.x,  image_center.y),
//        vec2(-image_center.x,  image_center.y)
//    );
//    
//    // translate the image
//    vec2 center_offset = image_pos + image_center;
//
//    // rotate around the center of the image
//    // then move the image origin back to the top left
//    vec2 world_pos = (rot_mat * local_offsets[gl_VertexID]) + center_offset;
//
//    gl_Position = projection * vec4(world_pos, 0.0, 1.0);
}

