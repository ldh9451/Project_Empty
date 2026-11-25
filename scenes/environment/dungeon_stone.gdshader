shader_type spatial;
render_mode ambient_light_disabled, specular_disabled;

uniform vec4 tint : source_color = vec4(1.0);
uniform bool terrain_palette = true;
uniform vec4 floor_color : source_color = vec4(0.80, 0.72, 0.49, 1.0);
uniform vec4 wall_color : source_color = vec4(0.62, 0.45, 0.29, 1.0);
uniform vec4 ink_color : source_color = vec4(0.18, 0.10, 0.24, 1.0);
uniform float texture_strength : hint_range(0.0, 1.0) = 0.45;
varying vec3 surface_position;
varying vec3 surface_normal;

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void vertex()
{
    // Object coordinates keep falling-block marks attached to their surface.
    surface_position = VERTEX;
    surface_normal = NORMAL;
}

void fragment()
{
    bool floor_surface = terrain_palette && COLOR.r < 0.1;
    vec3 stone = tint.rgb;
    if (terrain_palette)
    {
        stone *= floor_surface ? floor_color.rgb : wall_color.rgb;
        if (COLOR.r > 0.9)
        {
            stone = vec3(1.0, 0.38, 0.025);
        }
        else if (COLOR.r > 0.5)
        {
            stone = mix(stone, vec3(0.48, 0.13, 0.045), 0.55);
        }
    }
    vec3 n = abs(surface_normal);
    vec2 p = n.y > 0.5 ? surface_position.xz : n.x > 0.5 ? surface_position.zy : surface_position.xy;
    vec2 cell = floor(p * 5.0);
    vec2 q = fract(p * 5.0);
    float seed = hash(cell);
    float scratch_distance = abs(q.y - 0.35 - sin(q.x * 3.8 + seed * 6.0) * 0.12);
    float antialias = max(fwidth(scratch_distance), 0.025);
    float scratch = (1.0 - smoothstep(0.025, 0.025 + antialias, scratch_distance));
    scratch *= step(0.78, seed) * smoothstep(0.05, 0.2, q.x) * (1.0 - smoothstep(0.7, 0.95, q.x));
    float patch = hash(floor(p * 1.7));
    stone *= 0.94 + patch * 0.06;
    stone = mix(stone, ink_color.rgb, scratch * texture_strength);
    if (!floor_surface)
    {
        vec2 edge = min(UV, vec2(1.0) - UV);
        float joint = 1.0 - smoothstep(0.008, 0.022, min(edge.x, edge.y));
        stone = mix(stone, ink_color.rgb, joint * 0.65);
    }
    ALBEDO = stone;
    ROUGHNESS = 1.0;
    SPECULAR = 0.0;
}

void light()
{
    float facing = dot(NORMAL, LIGHT);
    float band = facing > 0.65 ? 1.0 : facing > 0.15 ? 0.72 : 0.4;
    band *= smoothstep(0.15, 0.55, ATTENUATION);
    vec3 shaded = mix(ink_color.rgb, ALBEDO, band);
    DIFFUSE_LIGHT += shaded / max(ALBEDO, vec3(0.001)) * LIGHT_COLOR / PI;
}
