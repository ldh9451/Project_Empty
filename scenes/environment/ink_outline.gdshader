shader_type spatial;
render_mode unshaded, fog_disabled, cull_disabled, depth_draw_never, depth_test_disabled;

uniform sampler2D scene_color : hint_screen_texture, repeat_disable, filter_nearest;
uniform sampler2D scene_depth : hint_depth_texture, repeat_disable, filter_nearest;
uniform vec4 ink_color : source_color = vec4(0.13, 0.065, 0.18, 1.0);
uniform float width_pixels : hint_range(0.0, 3.0) = 1.0;
uniform float depth_threshold = 0.16;

void vertex()
{
    // Reverse-Z near plane; this pass draws before translucent combat effects.
    POSITION = vec4(VERTEX.xy, 1.0, 1.0);
}

float view_depth(vec2 uv, mat4 inverse_projection)
{
    float depth = texture(scene_depth, uv).r;
    vec4 position = inverse_projection * vec4(uv * 2.0 - 1.0, depth, 1.0);
    return -position.z / position.w;
}

void fragment()
{
    vec2 step_uv = width_pixels / VIEWPORT_SIZE;
    float depth = view_depth(SCREEN_UV, INV_PROJECTION_MATRIX);
    float neighbor = max(max(view_depth(SCREEN_UV + vec2(step_uv.x, 0.0), INV_PROJECTION_MATRIX),
        view_depth(SCREEN_UV - vec2(step_uv.x, 0.0), INV_PROJECTION_MATRIX)),
        max(view_depth(SCREEN_UV + vec2(0.0, step_uv.y), INV_PROJECTION_MATRIX),
        view_depth(SCREEN_UV - vec2(0.0, step_uv.y), INV_PROJECTION_MATRIX)));
    float edge = smoothstep(depth_threshold, depth_threshold * 1.5, neighbor - depth);
    ALBEDO = mix(texture(scene_color, SCREEN_UV).rgb, ink_color.rgb, edge * 0.85);
    ALPHA = 1.0;
}
