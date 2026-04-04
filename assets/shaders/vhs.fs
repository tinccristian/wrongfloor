#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform float time;            // cumulative replay time; drives jitter seed
uniform float glitch_strength; // 1.0 = normal replay, higher = glitch phase

out vec4 finalColor;

float hash(float n) {
    return fract(sin(n) * 43758.5453);
}

void main()
{
    vec2 uv = fragTexCoord;

    // Random horizontal screen jitter — amplitude scales with glitch_strength.
    float jitter_seed = floor(time * 30.0 * glitch_strength) * 7.3;
    float jx = (hash(jitter_seed) - 0.5) * (2.0 / 640.0) * glitch_strength;
    uv.x = clamp(uv.x + jx, 0.0, 1.0);

    // Chromatic aberration — offset scales with glitch_strength.
    float ca = (1.5 / 640.0) * glitch_strength;
    float r = texture(texture0, vec2(clamp(uv.x + ca, 0.0, 1.0), uv.y)).r;
    float g = texture(texture0, uv).g;
    float b = texture(texture0, vec2(clamp(uv.x - ca, 0.0, 1.0), uv.y)).b;

    vec3 col = vec3(r, g, b);

    // Heavy desaturation + blood-red tint.
    float lum = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(col, vec3(lum), 0.75);
    col *= vec3(1.25, 0.50, 0.50);
    col = clamp(col, 0.0, 1.0);

    // Random bright flash during glitch (rare, driven by hash).
    float flash = hash(floor(time * 60.0) * 3.7 + 1.1);
    if (glitch_strength > 1.5 && flash > 0.85)
        col = mix(col, vec3(1.0, 0.2, 0.2), (flash - 0.85) * 6.0);

    // Horizontal scanlines — every other screen pixel row is dimmer.
    float scanline = mod(floor(gl_FragCoord.y), 2.0) < 1.0 ? 0.78 : 1.0;
    col *= scanline;

    // Vignette.
    vec2 vc = fragTexCoord - 0.5;
    float vignette = 1.0 - dot(vc, vc) * 3.0;
    col *= clamp(vignette, 0.1, 1.0);

    finalColor = vec4(col, 1.0) * fragColor;
}
