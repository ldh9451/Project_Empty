shader_type spatial;

uniform sampler2D diffuse_texture : source_color, filter_linear_mipmap_anisotropic;
uniform sampler2D specular_texture : source_color, filter_linear_mipmap_anisotropic;
uniform float diffuse_factor : hint_range(0.0, 1.0) = 0.8;
uniform float specular_strength : hint_range(0.0, 1.0) = 0.5;
// Approximate the source Phong exponent 10 with GGX; this is not an exact BRDF conversion.
uniform float roughness : hint_range(0.0, 1.0) = 0.64;

void fragment() {
    ALBEDO = texture(diffuse_texture, UV).rgb * diffuse_factor;
    SPECULAR = dot(texture(specular_texture, UV).rgb, vec3(0.2126, 0.7152, 0.0722)) * specular_strength;
    ROUGHNESS = roughness;
    METALLIC = 0.0;
}
