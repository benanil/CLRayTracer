// software as is bla bla license bla bla gev gev gev
// Anilcan Gulkaya 10/03/2022
// #include "MathAndSTL.cl"

// ---- STRUCTURES ----

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

// ---- CONSTANTS ----

constant float Infinite = 99999.0f;
constant float InfMinusOne = 99998.0f;

// ---- CONSTRUCTORS ----

RayHit CreateRayHit() {
    RayHit hit; hit.distance = Infinite; hit.index = 0;
    return hit;
}

HitRecord CreateHitRecord()
{
	HitRecord record;
	record.color  = (float3)(0.90f, 0.90f, 0.90f); // plane color
	record.normal = (float3)(0.0f, 1.0f, 0.0f);// w = roughness
	return record;
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

	float3 lightDir = (float3)(0.5f, -0.5f, 0.0f);//(float3)(sin(0.45f), cos(0.45f), 0.0f); // sun dir
	float3 result = (float3)(0.0f, 0.0f, 0.0f);
	float atmosphericLight = 0.2f;

	RandState randState = GenRandomState();

	for (int j = 0; j < 3; ++j)// num bounces
	{
		RayHit besthit = CreateRayHit();
		HitRecord record = CreateHitRecord();
		float roughness = 0.80f; // plane roughness
		// find intersectedsphere
		for (int i = 0; i < trace_args.numSpheres; ++i)  {
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
			float3 skybox_sample = read_imagef(skybox, sampler, GetSkyboxUV(ray.direction)).xyz * 1.20f;
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

		for (int i = 0; i < trace_args.numSpheres; ++i) 
		IntersectSphere(vload3(0, spheres[i].position), spheres[i].radius, ray, &besthit, i);

		if (besthit.distance < InfMinusOne || IntersectPlane(ray, &besthit)) shadow = atmosphericLight; 

		// Shade
		float ndl = max(dot(record.normal, -lightDir), atmosphericLight);
		atmosphericLight *= 0.4f;
		float3 specular = (float3)((1.0f - roughness) * ndl * shadow); // color.w = roughness

		result += ray.energy * (record.color * ndl);
		ray.energy *= specular;
		
		lightDir = ray.direction;
		
		if ((ray.energy.x + ray.energy.y + ray.energy.z) < 0.015f) break;
	}
	
	write_imagef(inout, (int2)(i, j), (float4)(pow(result, oneDivGamma), 0.1f));
}