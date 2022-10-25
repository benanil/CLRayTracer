
// ---- MATH ----

typedef struct _Ray {
	float3 origin;
	float3 direction;
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

float Max3(float3 a) { return fmax(fmax(a.x, a.y), a.z); }
float Min3(float3 a) { return fmin(fmin(a.x, a.y), a.z); }

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
constant float oneDivGamma = 1.0f / 1.2f;
constant float c_FMul = (1.0 / 16777216.0f);

float3 UnpackRGB8u(uint u)  
{
	return (float3)(u & 255, u >> 8 & 255, u >> 16 & 255) * UcharToFloat01;
}

#define UNPACK_RGB8(rgb8) ((float3)(rgb8.r, rgb8.g, rgb8.b) * UcharToFloat01)

// ---- RANDOM ----

uint WangHash(uint s) { 
	s = (s ^ 61u) ^ (s >> 16u);
	s *= 9, s = s ^ (s >> 4u);
	s *= 0x27d4eb2du;
	s = s ^ (s >> 15u); 
	return s; 
}

unsigned Next(uint* rng) {
	*rng ^= *rng << 13u;
	*rng ^= *rng >> 17u;
	*rng ^= *rng << 5u; 
	return *rng;
}

float NextFloat01(uint* state) {
	return as_float(Next(state) >> 8) * c_FMul;
}

float3 HemisphereSample(uint* random, float3 normal)
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
	return ray;
}

// ---- TEXTURE ----

typedef struct _Texture {
	int width, height, offset, padd;
} Texture;

typedef struct __attribute__((packed)) _RGB8 
{
	unsigned char r,g,b;
} RGB8;

int SampleSkyboxPixel(float3 rayDirection, Texture texture)
{
	int theta = (int)(atan2pi(rayDirection.x, -rayDirection.z) * 0.5f * (float)(texture.width)); 
	int phi = (int)(acospi(rayDirection.y) * (float)(texture.height)); 
	return mad24(phi, texture.width, theta + 2);
}

int SampleSphereTexture(float3 position, float3 center, Texture texture)
{
	float3 direction = fast_normalize(position - center);
	int theta = (int)(atan2pi(direction.x, direction.z) * .5f * (float)(texture.width)); 
	int phi   = (int)(acospi(direction.y) * (float)(texture.height)); 
	return mad24(phi, texture.width, theta + texture.offset);
}

int SampleRotatedSphereTexture(float3 position, float3 center, float rotationx, float rotationy, Texture texture)
{
	float3 direction = fast_normalize(position - center);

	float cosTheta = rotationx; 
	float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
	float phi = 2.0f * M_PI_F * rotationy;

	float3 tangentSpaceDir = (float3)(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
	
	direction = Mat3Mul(GetTangentSpace(direction), tangentSpaceDir);
	
	int theta = (int)(atan2pi(direction.x, direction.z) * .5f * (float)(texture.width)); 
	phi   = (int)(acospi(direction.y) * (float)(texture.height)); 
	
	return mad24(phi, texture.width, theta + texture.offset);
}

#define SAMPLE_SPHERE_TEXTURE(pos, center, texture) texturePixels[SampleSphereTexture(pos, center, texture)]

int SampleTexture(Texture texture, half2 uv)
{
	int uScaled = clamp((int)(texture.width  * uv.x), 0, texture.width  - 1); // (0, 1) to (0, TextureWidth )
	int vScaled = clamp((int)(texture.height * uv.y), 0, texture.height - 1); // (0, 1) to (0, TextureHeight)
	return vScaled * texture.width + texture.offset + uScaled;
}