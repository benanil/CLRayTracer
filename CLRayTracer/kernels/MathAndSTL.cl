
// ---- MATH ----

typedef struct _Ray {
	float3 origin;
	float3 direction;
} Ray;

typedef struct _Matrix {
	float4 x, y, z, w;
} Matrix4;

typedef struct _Matrix3 {
	float3 x, y, z;
} Matrix3;

Matrix4 MatMatMul(Matrix4 a, Matrix4 b) {
	Matrix4 result;
	float4 vx = a.x.xxxx * b.x;
	float4 vy = a.x.yyyy * b.y;
	float4 vz = a.x.zzzz * b.z;
	float4 vw = a.x.wwww * b.w;

	vx += vz; vy += vw; vx += vy;
	result.x = vx;

	vx = a.y.xxxx * b.x;
	vy = a.y.yyyy * b.y;
	vz = a.y.zzzz * b.z;
	vw = a.y.wwww * b.w;
	vx += vz; vy += vw; vx += vy;
	result.y = vx;

	vx = a.z.xxxx * b.x;
	vy = a.z.yyyy * b.y;
	vz = a.z.zzzz * b.z;
	vw = a.z.wwww * b.w;
	vx += vz; vy += vw; vx += vy;
	result.z = vx;

	vx = a.w.xxxx * b.x;
	vy = a.w.yyyy * b.y;
	vz = a.w.zzzz * b.z;
	vw = a.w.wwww * b.w;
	vx += vz; vy += vw; vx += vy;
	result.w = vx;

	return result;
}

Matrix4 Transpose(Matrix4 M)
{
	float4 vTemp1 = shuffle2(M.x, M.y, (uint4)(0, 1, 4, 5));
	float4 vTemp3 = shuffle2(M.x, M.y, (uint4)(2, 3, 6, 7));
	float4 vTemp2 = shuffle2(M.z, M.w, (uint4)(0, 1, 4, 5));
	float4 vTemp4 = shuffle2(M.z, M.w, (uint4)(2, 3, 6, 7));
	Matrix4 mResult;
	mResult.x = shuffle2(vTemp1, vTemp2, (uint4)(0, 2, 4, 6));
	mResult.y = shuffle2(vTemp1, vTemp2, (uint4)(1, 3, 5, 7));
	mResult.z = shuffle2(vTemp3, vTemp4, (uint4)(0, 2, 4, 6));
	mResult.w = shuffle2(vTemp3, vTemp4, (uint4)(1, 3, 5, 7));
	return mResult;
}

Matrix3 TransposeToMatrix3(Matrix4 M)
{
	float4 vTemp1 = shuffle2(M.x, M.y, (uint4)(0, 1, 4, 5));
	float4 vTemp3 = shuffle2(M.x, M.y, (uint4)(2, 3, 6, 7));
	float4 vTemp2 = shuffle2(M.z, M.w, (uint4)(0, 1, 4, 5));
	float4 vTemp4 = shuffle2(M.z, M.w, (uint4)(2, 3, 6, 7));
	Matrix3 mResult;
	mResult.x = shuffle2(vTemp1, vTemp2, (uint4)(0, 2, 4, 6)).xyz;
	mResult.y = shuffle2(vTemp1, vTemp2, (uint4)(1, 3, 5, 7)).xyz;
	mResult.z = shuffle2(vTemp3, vTemp4, (uint4)(0, 2, 4, 6)).xyz;
	return mResult;
}

float4 MatMul(Matrix4 m, float4 v) {
	return m.x * v.xxxx + m.y * v.yyyy + m.z * v.zzzz + m.w * v.wwww;
}

float3 Mat3Mul(Matrix3 m, float3 v) {
	return m.x * v.xxx + m.y * v.yyy + m.z * v.zzz;
}

// transform vector
float3 MatVecMul(Matrix4 m, float3 v) {
	return (float3)(dot(m.x.xyz, v), dot(m.y.xyz, v), dot(m.z.xyz, v));
}

float3 MatPosMul(Matrix4 m, float3 v) {
	return m.x.xyz * v.xxx + m.y.xyz * v.yyy + m.z.xyz * v.zzz;
}

float3 reflect(float3 v, float3 n) {
	return v - n * dot(n, v) * 2.0f;
}

float3 ACESFilm(float3 x) {
	return (x * (2.51f * x + 0.03f)) / (x * (2.43f * x + 0.59f) + 0.14f);
}

float3 Saturation(float3 in, float change)
{
	float3 P = (float3)(sqrt(in.x * in.x * 0.299f + (in.y * in.y * 0.587f) + (in.z * in.z * 0.114f)));
	return P + (in - P) * change; 
}

float Max3(float3 a) { return fmax(fmax(a.x, a.y), a.z); }
float Min3(float3 a) { return fmin(fmin(a.x, a.y), a.z); }

Matrix3 GetTangentSpace(float3 normal)
{
	// Choose a helper vector for the cross product
	float3 helper = (float3)(1.0f, 0.0f, 0.0f);
	if (fabs(normal.x) > 0.99f) helper = (float3)(0.0f, 0.0f, 1.0f);
	// Generate vectors
	float3 tangent  = normalize(cross(normal, helper));
	float3 binormal = normalize(cross(normal, tangent));
	Matrix3 mat;
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
	return phi * texture.width + (theta + texture.offset);
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

int SampleTexture(Texture texture, float2 uv)
{
	uv -= floor(uv);
	int uScaled = (int)(texture.width  * uv.x); // (0, 1) to (0, TextureWidth )
	int vScaled = (int)(texture.height * uv.y); // (0, 1) to (0, TextureHeight)
	return vScaled * texture.width + texture.offset + uScaled;
}