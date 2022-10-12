
// ---- MATH ----

typedef struct _Ray {
	float3 origin;
	float3 direction;
	float3 energy;
} Ray;

typedef struct _Matrix {
	float4 x, y, z, w;
} Matrix4;

typedef struct _float3x3 {
	float3 x, y, z;
} float3x3;

float4 MatMul(Matrix4 m, float4 v) {
	return m.x * v.xxxx + m.y * v.yyyy + m.z * v.zzzz + m.w * v.wwww;
}

float3 Mat3Mul(float3x3 m, float3 v) {
	return m.x * v.xxx + m.y * v.yyy + m.z * v.zzz;
}

float3 reflect(float3 v, float3 n) {
	return v - n * dot(n, v) * 2.0f;
}

float3x3 GetTangentSpace(float3 normal)
{
    // Choose a helper vector for the cross product
    float3 helper = (float3)(1.0f, 0.0f, 0.0f);
    if (fabs(normal.x) > 0.99f) helper = (float3)(0.0f, 0.0f, 1.0f);
    // Generate vectors
    float3 tangent  = normalize(cross(normal, helper));
    float3 binormal = normalize(cross(normal, tangent));
	float3x3 mat;
	mat.x = tangent; mat.y = binormal; mat.z = normal;
	return mat;
}

// ---- CONSTANTS ----

constant float Infinite = 99999.0f;
constant float InfMinusOne = 99998.0f;
constant float UcharToFloat01 = 1.0f / 255.0f;

float3 UnpackRGB8u(unsigned u)  
{
	return (float3)(u & 255, u >> 8 & 255, u >> 16 & 255) * UcharToFloat01;
}

#define UNPACK_RGB8(rgb8) ((float3)(rgb8.r, rgb8.g, rgb8.b) * UcharToFloat01)

// ---- RANDOM ----

typedef struct _RandState { ulong state;  ulong inc; } RandState;

unsigned Next(RandState* rng) {
    ulong oldstate = rng->state;
    rng->state = oldstate * 6364136223846793005ULL + (rng->inc | 1);
    ulong xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    ulong rot = oldstate >> 59u;
    rot  = (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
	return (unsigned)(rot >> 16);
}

float NextFloat01(RandState* state) {
	constant float c_FMul = (1.0 / 16777216.0f);
	return as_float(Next(state) >> 8) * c_FMul;
}

float3 HemisphereSample(RandState* random, float3 normal)
{
	float cosTheta = NextFloat01(random); 
    float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
    float phi = 2.0f * M_PI_F * NextFloat01(random);
    float3 tangentSpaceDir = (float3)(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    return Mat3Mul(GetTangentSpace(normal), tangentSpaceDir);
}

// ---- CONSTRUCTORS ----

Ray CreateRay(float3 origin, float3 dir)
{
    Ray ray;
    ray.origin = origin;
    ray.direction = dir;
    ray.energy = (float3)(1.0f, 1.0f, 1.0f);
    return ray;
}

RandState GenRandomState() {
	RandState state;
	state.state = 0x853c49e6748fea9bULL;
	state.inc   = 0xda3e39cb94b95bdbULL;
	return state;
}

// ---- TEXTURE ----

typedef struct _Texture {
	int width, height, offset, padd;
} Texture;

typedef struct __attribute__((packed)) _RGBA8 
{
	unsigned char r,g,b;
} RGB8;

int SampleSkyboxPixel(float3 rayDirection)
{
	int theta = (int)(atan2pi(rayDirection.x, -rayDirection.z) * 0.5f * 4096.0f); 
	int phi = (int)(acospi(rayDirection.y) * 2048.0f); 
	return phi * 4096 + theta + 2;
}

int SampleSphereTexture(float3 position, float3 center, Texture texture)
{
	float3 direction = fast_normalize(position - center);
	int theta = (int)(atan2pi(direction.x, direction.z) * .5f * (float)(texture.width)); 
	int phi   = (int)(acospi(direction.y) * (float)(texture.height)); 
	return phi * texture.width + theta + texture.offset;
}

#define SAMPLE_SPHERE_TEXTURE(pos, center, texture) texturePixels[SampleSphereTexture(pos, center, texture)]

