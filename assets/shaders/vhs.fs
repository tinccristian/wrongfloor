#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform float time;  // cumulative replay time; drives jitter seed

out vec4 finalColor;

float hash(float n) {
    return fract(sin(n) * 43758.5453);
}

void main()
{
    vec2 uv = fragTexCoord;

    // Per-frame 1px random horizontal screen jitter
    float jx = (hash(floor(time * 30.0) * 7.3) - 0.5) * (1.0 / 640.0);
    uv.x = clamp(uv.x + jx, 0.0, 1.0);

    // Chromatic aberration: R shifted right, B shifted left
    const float ca = 1.0 / 640.0;
    float r = texture(texture0, vec2(clamp(uv.x + ca, 0.0, 1.0), uv.y)).r;
    float g = texture(texture0, uv).g;
    float b = texture(texture0, vec2(clamp(uv.x - ca, 0.0, 1.0), uv.y)).b;

    vec3 col = vec3(r, g, b);

    // Heavy desaturation + blood-red tint
    float lum = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(col, vec3(lum), 0.75);
    col *= vec3(1.25, 0.50, 0.50);
    col = clamp(col, 0.0, 1.0);

    // Horizontal scanlines — every other screen pixel row is dimmer
    float scanline = mod(floor(gl_FragCoord.y), 2.0) < 1.0 ? 0.78 : 1.0;
    col *= scanline;

    // Vignette
    vec2 vc = fragTexCoord - 0.5;
    float vignette = 1.0 - dot(vc, vc) * 3.0;
    col *= clamp(vignette, 0.1, 1.0);

    finalColor = vec4(col, 1.0) * fragColor;
}
