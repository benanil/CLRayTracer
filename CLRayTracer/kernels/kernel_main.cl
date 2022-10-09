// software as is bla bla license bla bla gev gev gev
// Anilcan Gulkaya 10/03/2022

// ---- RANDOM ----

typedef struct { ulong state;  ulong inc; } RandState;

RandState GenRandomState()
{
	RandState state;
	state.state = 0x853c49e6748fea9bULL;
	state.inc   = 0xda3e39cb94b95bdbULL;
	return state;
}

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

float NextFloat(RandState* state, float min, float max) {
	return min + (NextFloat01(state) * fabs(min - max));
}

// ---- STRUCTURES ----

typedef struct _Ray {
	float3 origin;
	float3 direction;
} Ray;

typedef struct _Matrix {
	float4 x, y, z, w;
} Matrix4;

typedef struct _TraceArgs{
	float cameraPos[3];
	float time;
} TraceArgs;

typedef struct _RayHit {
	float3 position;
	float  distance;
	float3 normal;
} RayHit;

typedef struct _Sphere {
	float3 sphere;
	float radius;
} Sphere;

RayHit CreateRayHit()
{
    RayHit hit;
    hit.position = (float3)(0.0f, 0.0f, 0.0f);
    hit.distance = 9999.0f;
    hit.normal = (float3)(0.0f, 0.0f, 0.0f);
    return hit;
}

// ---- MATH ----

float4 MatMul(Matrix4 m, float4 v)
{
	return m.x * v.xxxx + m.y * v.yyyy + m.z * v.zzzz + m.w * v.wwww;
}

void IntersectSphere(float3 center, float radius, Ray ray, RayHit* besthit) 
{
	float3 origin = ray.origin - center;
	
	float a = dot(ray.direction, ray.direction);
	float b = 2.0f * dot(origin, ray.direction);
	float c = dot(origin, origin) - radius * radius;

	float discriminant = b * b - 4.0f * a * c;
	
	if (discriminant < 0.0f) return;

	float closestT = (-b - sqrt(discriminant)) / (2.0f * a);
	 
	if (closestT > 0.0f && closestT < besthit->distance)
	{
		besthit->distance = closestT;
		besthit->position = origin + ray.direction * closestT;
		besthit->normal = normalize(besthit->position);
	}
}

bool IntersectPlane(Ray ray, RayHit* besthit)
{
	float t = -ray.origin.y / ray.direction.y;
	if (t > 0.0f && t < besthit->distance)
	{
		besthit->distance = t;
		besthit->position = ray.origin + (ray.direction * t);
		besthit->normal   = (float3)(0.0f, 1.0f, 0.0f);
	}
}

float4 Shade(RayHit hit)
{
	const float3 sunDir = (float3)(sin(1.2f), cos(1.2f), 0.0f);
	constant float4 color  = (float4)(1.0f, 0.7f, 0.4f, 1.0f); 
	return color * max(dot(hit.normal, sunDir), 0.2f);
}

// ---- Kernels ----

kernel void RayGen
(
	int width,
	int height,
	global float* rays, const Matrix4 inverseView, const Matrix4 inverseProjection
)
{
	const int i = get_global_id(0), j = get_global_id(1);

	float2 coord = (float2)((float)i / (float)width, (float)j / (float)height);
	coord = coord * 2.0f - 1.0f;
	float4 target = MatMul(inverseProjection, (float4)(coord, 1.0f, 1.0f));
	target /= target.w;
	target.w = 0;
	target.xyz = normalize(target.xyz);
	float3 rayDir = MatMul(inverseView, target).xyz;
	vstore3(rayDir, i + j * width, rays);
}

kernel void Trace
(
	write_only image2d_t inout,
	read_only image2d_t skybox, 
	global const float* rays, TraceArgs trace_args
) 
{
	const int i = get_global_id(0), j = get_global_id(1);
	constant float gamma = 1.2f;
	constant sampler_t sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_ADDRESS_REPEAT  | CLK_FILTER_NEAREST;

	Ray ray;
	ray.origin = vload3(0, trace_args.cameraPos);
	ray.direction = vload3(i + j * get_image_width(inout), rays);

	RayHit besthit = CreateRayHit();
	
	IntersectSphere((float3)(0.0f, 0.0f, 0.0f), 1.0f, ray, &besthit);
	float4 pixel_color = pow(Shade(besthit), 1.0f / gamma);

	if (besthit.distance >= 9998.0f)
	{
	    float theta = acos(ray.direction.y) / M_PI_F;
        float phi = atan2(ray.direction.x, ray.direction.z) / -M_PI_F ;
		pixel_color = read_imagef(skybox, sampler, (float2)(phi, theta));;
	}

	write_imagef(inout, (int2)(i, j), pixel_color);
}