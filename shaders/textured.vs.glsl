#version 330 

// Input attributes
in vec2 in_position;
in vec2 in_texcoord;

// Passed to fragment shader
out vec2 texcoord;

// Application data
uniform mat3 projection;
uniform vec2 grid_position;

void main()
{
	texcoord = in_texcoord;
	mat3 translation = mat3(vec3(1.0, 0.0, 0.0),
							vec3(0.0, 1.0, 0.0),
							vec3(grid_position.y * 50.0, grid_position.x * 50.0, 1.0));
	vec3 pos = projection * translation * vec3(in_position, 1.0);
	gl_Position = vec4(pos.xy, 0.0, 1.0);
}