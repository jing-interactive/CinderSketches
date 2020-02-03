#version 300 es

uniform mat4	ciModelViewProjection;
uniform mat4    ciProjectionMatrix;
uniform mat4    ciViewMatrix;
uniform mat4    ciModelMatrix;
uniform mat4    uShadowMatrix;
uniform mat4    uTranslateMatrix;
uniform mat4    uTouchMatrix;

uniform vec2    uViewport;
uniform float   uOffset;

in vec4			ciPosition;
in vec3			iPositionOrg;
in vec3			iRandom;
in float        iLife;

out vec3        color;
out vec4        vShadowCoord;
out vec4        vScreenCoord;
out float       vLife;

const mat4 biasMatrix = mat4( 0.5, 0.0, 0.0, 0.0,
                                0.0, 0.5, 0.0, 0.0,
                                0.0, 0.0, 0.5, 0.0,
                                0.5, 0.5, 0.5, 1.0 );

const float radius = 0.005;

float exponentialInOut(float t) {
  return t == 0.0 || t == 1.0
    ? t
    : t < 0.5
      ? +0.5 * pow(2.0, (20.0 * t) - 10.0)
      : -0.5 * pow(2.0, 10.0 - (t * 20.0)) + 1.0;
}

void main( void )
{
	vec4 pos = ciPosition;
	// gl_Position	= ciModelViewProjection * uTranslateMatrix * pos;
	gl_Position	= ciProjectionMatrix * ciViewMatrix * uTranslateMatrix * ciModelMatrix * pos;
    color = vec3(1.0);

    float offset = smoothstep(0.0, 1.0, uOffset * 2.0 - iRandom.x);
    offset = exponentialInOut(offset);
    
    float distOffset = uViewport.y * ciProjectionMatrix[1][1] * radius / gl_Position.w;
    float scale = mix(0.5, 1.0, iRandom.x);
    float lifeScale = smoothstep(0.5, 0.4, abs(iLife - 0.5));
    gl_PointSize = distOffset * lifeScale * scale * offset;
    
    vShadowCoord    = ( biasMatrix * uShadowMatrix * ciModelMatrix ) * pos;
    vScreenCoord    = uTouchMatrix * uTranslateMatrix * ciModelMatrix * vec4(iPositionOrg, 1.0);

    vLife = iLife;
}
