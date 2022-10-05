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

unsigned Next(RandState* rng)
{
    unsigned long oldstate = rng->state;
    // Advance internal state
    rng->state = oldstate * 6364136223846793005ULL + (rng->inc | 1);
    // Calculate output function (XSH RR), uses old state for max ILP
    unsigned long xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    unsigned long rot = oldstate >> 59u;
    rot  = (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
	return (unsigned)(rot >> 16);
}

float NextFloat01(RandState* state) 
{
	const float c_FMul = (1.0 / 16777216.0f);
	unsigned result = Next(state) >> 8;
	return *(float*)(&result) * c_FMul;
}

float NextFloat(RandState* state, float min, float max)
{
	return min + (NextFloat01(state) * fabs(min - max));
}

// ---- STRUCTURES ----

typedef struct __attribute__((packed))
{
	float3 origin;
	float3 dir;
} Ray;

typedef struct 
{
	float4 r[4];
} Matrix4;

Matrix4 Identity()
{
	Matrix4 result;
	result.r[0] = (float4)(1.0f, 0.0f, 0.0f, 0.0f);
	result.r[1] = (float4)(0.0f, 1.0f, 0.0f, 0.0f);
	result.r[2] = (float4)(0.0f, 0.0f, 1.0f, 0.0f);
	result.r[3] = (float4)(0.0f, 0.0f, 0.0f, 1.0f);
	return result;
}

float4 MatMul(Matrix4 m, float4 v)
{
	float4 v0 = shuffle(v, (uint4)(0, 0, 0, 0));
	float4 v1 = shuffle(v, (uint4)(1, 1, 1, 1));
	float4 v2 = shuffle(v, (uint4)(2, 2, 2, 2));
	float4 v3 = shuffle(v, (uint4)(3, 3, 3, 3));
	
	v0 = m.r[0] * v0;
	v1 = m.r[1] * v1;
	v2 = m.r[2] * v2;
	v3 = m.r[3] * v3;
	
	float4 a0 = v0 + v1;
	float4 a1 = v2 + v3;
	float4 a2 = a0 + a1;

	return a2;
}

// ---- MATH ----

float LengthSquared(float3 vec)
{
	return vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2];
}

bool hit_sphere(float3 center, float radius, Ray ray, float4* color) 
{
    float3 oc = ray.origin - center;
    float a = LengthSquared(ray.dir);
    float half_b = dot(oc, ray.dir);
    float c = LengthSquared(oc) - (radius * radius);
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
	float ndl = max(dot(normal, sun_dir), 0.15f);
	*color = (float4)(0.8f, 0.62f, 0.45f, 1.0f) * ndl;
	return (discriminant > 0.0f);
}

float4 ray_color(Ray ray) 
{
	float4 out_color;
	if (hit_sphere((float3)(0.0f,0.0f,-1.0f), 0.5f, ray, &out_color))
        return out_color;
    
	float3 unit_direction = normalize(ray.dir);
    float t = 0.5f * (unit_direction[1] + 1.0f);
    return (1.0f - t) * (float4)(1.0f, 1.0f, 1.0f, 1.0f) + t * (float4)(0.5f, 0.7f, 1.0f, 1.0f);
}

kernel void RayGen
(
	int width,
	int height,
	global float* rays, const Matrix4 inverseView, const Matrix4 inverseProjection
)
{
	const int i = get_global_id(0);
  	const int j = get_global_id(1);

	float2 coord = (float2)((float)i / (float)width, (float)j / (float)height);
	coord = coord * 2.0f - 1.0f;
	float4 target = MatMul(inverseProjection, (float4)(coord, 1.0f, 1.0f));
	target /= target.w;
	target = MatMul(inverseView, normalize(target));
	target.y = 0.0f - target.y;
	vstore3(target.xyz, i + j * width, rays);
}

kernel void texture(write_only image2d_t inout, global const float* rays, float time) 
{
	const int i = get_global_id(0);
  	const int j = get_global_id(1);
	
	int width  = get_image_width(inout); // __local int height = getImageHeight(inout);

	const float gamma = 1.8f;
	
	Ray ray;
	ray.origin = (float3)(0.0f, 0.0f, 3.0f);
	ray.dir = vload3(i + j * width, rays);

	float4 pixel_color = pow(ray_color(ray), 1.0f / gamma);

	write_imagef(inout, (int2)(i, j), pixel_color);
}
