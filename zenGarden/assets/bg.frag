#version 300 es

precision highp float;

in vec3    Normal;
in vec2    TexCoord0;

uniform sampler2D uEnvMap;
uniform float uTime;
uniform float uRatio;

out highp vec4  oColor;

#include "./fragments/rand.glsl"
#include "./fragments/curlNoise.glsl"
#include "./fragments/contrast.glsl"

#define EXPOSURE 1.2


#define uNoiseScaleBase 1.56
#define uNoiseScale 30.0

#define uGrainStrength 1.154288846259249
#define uOffset 0.020718004932858317
#define uSeed 0.0
#define uExposureOffset 1.7718553028226913

#define uColor0 vec3(11.0, 0.0, 97.5)/255.0
#define uColor1 vec3(200.0, 0.0, 11.7)/255.0
#define uGradientStrength 1.0


float luma (vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

vec3 greyscale (vec3 color) {
    float grey = luma(color);
    return vec3(grey);
}


vec3 gradientMap(vec3 color, vec3 color1, vec3 color2, float strength) {
    float l = luma(color);
    vec3 colorGradient = mix(color1, color2, l);
    return mix(color, colorGradient, strength);
}

vec3 gradientMap(vec3 color, vec3 color1, vec3 color2) {
    return gradientMap(color, color1, color2, 1.0);
}


void main( void ) {
//    float grain = random(TexCoord0.xy + uTime);

    vec2 uv = TexCoord0;
    uv.y = 1.0 - uv.y;
    
    
    vec2 uvInverted = vec2(uv);
    
    vec2 diff = uv - 0.5;
    diff.y /= uRatio;
    float t = length(diff);
    t = smoothstep(0.2, 0.9, t);

    
    // noise
    uv.y /= uRatio;
    float noiseBase = snoise(vec3(uSeed, uv * uNoiseScaleBase));
    vec3 noise = curlNoise(vec3(uv * uNoiseScale, uSeed) * noiseBase);
    
    float rnd = random(uv);
    rnd = mix(1.0, rnd, uGrainStrength);
    
    vec2 uvOffset = noise.xy * noise.z;
    
    
    
    
    uv = uvInverted + uvOffset * uOffset * rnd;

    vec4 color = texture(uEnvMap, uv);
//    colorNoise.rgb = pow(colorNoise.rgb, vec3(1.0/uExposureOffset));
//    colorNoise.rgb = gradientMap(colorNoise.rgb, uColor0, uColor1, uGradientStrength);

//    oColor = colorNoise;
     
    
//    vec4 color = texture(uEnvMap, uv);
    color.rgb = pow(color.rgb, vec3(1.0/uExposureOffset));
    
    color.rgb = gradientMap(color.rgb, uColor0, uColor1, uGradientStrength);
    
    oColor = color;
}
