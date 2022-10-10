// software as is bla bla license bla bla gev gev gev
// Anilcan Gulkaya 10/03/2022

// ---- RANDOM ----

typedef struct { ulong state;  ulong inc; } RandState;

RandState GenRandomState() {
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
	float3 energy;
} Ray;

typedef struct _Matrix {
	float4 x, y, z, w;
} Matrix4;

typedef struct _TraceArgs{
	float cameraPos[3];
	int numSpheres;
} TraceArgs;

typedef struct _RayHit {
	float3 position;
	float  distance;
	int    index;
} RayHit;

typedef struct __attribute__((packed)) _Sphere {
	float position[3]; // w radius
	float radius;
	float roughness;
	uint  color;
} Sphere;

typedef struct _HitRecord
{
	float3 color, normal, point;
} HitRecord;

constant float Infinite = 99999.0f;
constant float InfMinusOne = 99998.0f;

HitRecord CreateHitRecord()
{
	HitRecord record;
	record.color  = (float3)(0.90f, 0.90f, 0.90f); // plane color
	record.normal = (float3)(0.0f, 1.0f, 0.0f);// w = roughness
	return record;
}

RayHit CreateRayHit() {
    RayHit hit; hit.distance = Infinite; hit.index = 0;
    return hit;
}

Ray CreateRay(float3 origin, float3 dir)
{
	Ray ray;
	ray.origin = origin;
	ray.direction = dir;
	ray.energy = (float3)(1.0f, 1.0f, 1.0f);
	return ray;
}

// ---- MATH ----

float4 MatMul(Matrix4 m, float4 v) {
	return m.x * v.xxxx + m.y * v.yyyy + m.z * v.zzzz + m.w * v.wwww;
}

float3 reflect(float3 v, float3 n) {
	return v - n * dot(n, v) * 2.0f;
}

float3 UnpackRGB8(unsigned u)  {
	constant float normalizer = 1.0f / 255.0f;
	// I might use union or other technique
	return (float3)((float)((u >> 0 ) & 255) * normalizer, (float)((u >> 8 ) & 255) * normalizer, (float)((u >> 16) & 255) * normalizer);
}

bool IntersectSphere(float3 position, float radius, Ray ray, RayHit* besthit, int i) 
{
	float3 origin = ray.origin - position;
	
	float a = dot(ray.direction, ray.direction);
	float b = 2.0f * dot(origin, ray.direction);
	float c = dot(origin, origin) - radius * radius;

	float discriminant = b * b - 4.0f * a * c;
	
	if (discriminant < 0.0f) return false;

	float closestT = (-b - sqrt(discriminant)) / (2.0f * a);
	 
	if (closestT > 0.0f && closestT < besthit->distance)
	{
		besthit->distance = closestT;
		besthit->index = i;
		return true;
	}
	return false;
}

bool IntersectPlane(Ray ray, RayHit* besthit)
{
	float t = -ray.origin.y / ray.direction.y;
    if (t > 0 && t < besthit->distance) {
        besthit->distance = t;
    	return true;
	}
	return false;
}

// ---- Kernels ----
kernel void RayGen (
	int width, int height,
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

kernel void Trace (
	write_only image2d_t inout,
	read_only image2d_t skybox, 
	global const float* rays, 
	global const Sphere* spheres,
	TraceArgs trace_args,
	float time
) 
{
	const int i = get_global_id(0), j = get_global_id(1);
	constant float oneDivGamma = 1.0f / 1.2f;
	constant sampler_t sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_ADDRESS_REPEAT | CLK_FILTER_NEAREST;

	Ray ray = CreateRay(vload3(0, trace_args.cameraPos), vload3(i + j * get_image_width(inout), rays));

	float3 lightDir = (float3)(-0.5f, 0.5f, 0.0f);//(float3)(sin(0.45f), cos(0.45f), 0.0f); // sun dir
	float3 result = (float3)(0.0f, 0.0f, 0.0f);
	float atmosphericLight = 0.2f;

	for (int j = 0; j < 3; ++j)// num bounces
	{
		RayHit besthit = CreateRayHit();
		HitRecord record = CreateHitRecord();
		float roughness = 0.80f; // plane roughness
		// find intersectedsphere
		for (int i = 0; i < 10; ++i)  {
			IntersectSphere(vload3(0, spheres[i].position), spheres[i].radius, ray, &besthit, i);
		}
	
		if (IntersectPlane(ray, &besthit))
		{
			record.point = ray.origin + ray.direction * besthit.distance;
		}
		else
		{
			Sphere currentSphere = spheres[besthit.index];
			record.point = ray.origin + ray.direction * besthit.distance;
			record.normal.xyz = normalize(record.point - vload3(0, currentSphere.position));
			record.color.xyz = UnpackRGB8(currentSphere.color);
			roughness = currentSphere.roughness;
		}

		if (besthit.distance > InfMinusOne) {
			float theta = acos(ray.direction.y) / M_PI_F;
			float phi   = atan2(ray.direction.x, -ray.direction.z) / -M_PI_F ;
			float3 skybox_sample = read_imagef(skybox, sampler, (float2)(phi, theta)).xyz * 1.20f;
			result += skybox_sample * ray.energy;
			break;
		}

		ray.origin = record.point;
		ray.origin += record.normal * 0.001f;
		ray.direction = reflect(ray.direction, record.normal); // wo = ray.direction now = outgoing ray direction
	
		// check Shadow
		Ray shadowRay = CreateRay(ray.origin, -lightDir);
		besthit = CreateRayHit();
		float shadow = 1.0f; 

		for (int i = 0; i < 10; ++i) 
		IntersectSphere(vload3(0, spheres[i].position), spheres[i].radius, ray, &besthit, i);

		if (besthit.distance < InfMinusOne || IntersectPlane(ray, &besthit)) shadow = atmosphericLight; 

		// Shade
		float ndl = max(dot(record.normal, lightDir), atmosphericLight);
		atmosphericLight *= 0.4f;
		float3 specular = (float3)((1.0f - roughness) * ndl * shadow); // color.w = roughness

		result += ray.energy * (record.color * ndl);
		ray.energy *= specular;
		
		lightDir = ray.direction;
		
		if ((ray.energy.x + ray.energy.y + ray.energy.z) < 0.015f) break;
	}
	
	write_imagef(inout, (int2)(i, j), (float4)(pow(result, oneDivGamma), 0.1f));
}