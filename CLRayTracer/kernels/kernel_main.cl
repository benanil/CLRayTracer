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

typedef struct {
	float3 origin;
	float3 dir;
} Ray;

typedef struct {
	float4 x, y, z, w;
} Matrix4;

typedef struct {
	float cameraPos[3];
	float time;
} TraceArgs;

typedef struct _RayHit {
	float3 position;
	float depth;
	float3 normal;
	bool hit;
};

// ---- MATH ----

float4 MatMul(Matrix4 m, float4 v)
{
	return m.x * v.xxxx + m.y * v.yyyy + m.z * v.zzzz + m.w * v.wwww;
}

bool hit_sphere(float3 center, float radius, Ray ray, float4* color) 
{
    float3 oc = ray.origin - center;
    float a = dot(ray.dir, ray.dir);
    float half_b = dot(oc, ray.dir);
    float c = dot(oc, oc) - (radius * radius);
    float discriminant = half_b * half_b -  a * c;
    
	if (discriminant < 0) return false;

	float sqr = sqrt(discriminant);
	float root = (-half_b - sqr) / a;
	// Find the nearest root that lies in the acceptable range.
	if (root < 0.01f || 500.0f < root)
	{
		root = (-half_b + sqr) / a;
		if (root < 0.01f || 500.0f < root) return false;
	}
	float3 hit_point = ray.origin + (ray.dir * root);

	float3 normal = (hit_point - center) / radius;
	float3 sun_dir =  (float3)(sin(1.2f), cos(1.0f), 0.0f);
	float ndl = max(dot(normal, sun_dir), 0.20f);
	*color = (float4)(0.8f, 0.62f, 0.45f, 1.0f) * ndl;
	return (discriminant > 0.0f);
}

float4 ray_color(Ray ray, bool* hit) 
{
	float4 out_color;
	if (hit_sphere((float3)(0.0f,0.0f, 0.0f), 1.0f, ray, &out_color)) {
    	*hit = true;
		return out_color;
	}
	float3 unit_direction = normalize(ray.dir);
    float t = 0.5f * (unit_direction.y + 1.0f);
    return (1.0f - t) * (float4)(1.0f, 1.0f, 1.0f, 1.0f) + t * (float4)(0.5f, 0.7f, 1.0f, 1.0f);
}

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
	target = MatMul(inverseView, normalize(target));
	target.y = 0.0f - target.y;
	vstore3(target.xyz, i + j * width, rays);
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
	constant sampler_t sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;

	Ray ray;
	ray.origin = vload3(0, trace_args.cameraPos);
	ray.dir = vload3(i + j * get_image_width(inout), rays);

	bool hit = false;
	float4 pixel_color = pow(ray_color(ray, &hit), 1.0f / gamma);

	if (!hit)
	{
	    float theta = acos(ray.dir.y) / M_PI_F;
        float phi = fabs(atan2(ray.dir.x, -ray.dir.z)) / M_PI_F ;
		pixel_color = read_imagef(skybox, sampler, (float2)(phi, theta));//(float4)(theta, phi, 0.0f, 1.0f);
		if (phi < 0.0f) pixel_color = (float4)(1.0f, 0.0f, 0.0f, 1.0f); 
	}

	write_imagef(inout, (int2)(i, j), pixel_color);
}