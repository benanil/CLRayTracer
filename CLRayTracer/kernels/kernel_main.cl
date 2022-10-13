// software as is bla bla license bla bla gev gev gev
// Anilcan Gulkaya 10/03/2022
// #include "MathAndSTL.cl"

// ---- STRUCTURES ----

typedef struct _TraceArgs{
	float cameraPos[3];
	ushort numSpheres, numMeshes;
} TraceArgs;

typedef struct _RayHit {
	float3 position;
	float  distance;
	int    index;
} RayHit;

typedef struct __attribute__((packed)) _Sphere {
	float position[3]; 
	float radius;
	float roughness;
	uint  color; // w texture index
} Sphere;

typedef struct _HitRecord
{
	float3 color, normal, point;
} HitRecord;

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
	return false;
	float t = -ray.origin.y / ray.direction.y;
    if (t > 0 && t < besthit->distance) 
	{
		float3 point = ray.origin + (ray.direction * t);
    	if (fabs(point.x) < 15.0f && fabs(point.z) < 15.0f)
			besthit->distance = t;
    	return true;
	}
	return false;
}

typedef struct __attribute((packed)) _Triangle
{
	float3 x, y, z;
} Triangle;

typedef struct __attribute((packed)) _Vertex {
	float x, y, z;
} Vertex;

typedef struct __attribute((packed)) _Index {
	unsigned p, t, n;
} Index;

typedef struct _Triout {
	float t, u, v;
} Triout;

typedef struct _Mesh {
	int indexCount;
} Mesh;

constant float EPSILON = 1e-8;

bool IntersectTriangle(Ray ray, Triangle triangle, Triout* o, RayHit* besthit)
{
    // find vectors for two edges sharing vert0
	float3 edge1 = triangle.y - triangle.x;
	float3 edge2 = triangle.z - triangle.x;
    // begin calculating determinant - also used to calculate U parameter
    float3 pvec = cross(ray.direction, edge2);
    // if determinant is near zero, ray lies in plane of triangle
    float det = dot(edge1, pvec);
    // use backface culling
    //if (det < EPSILON)  return false;
    float inv_det = 1.0f / det;
    // calculate distance from vert0 to ray origin
    float3 tvec = ray.origin - triangle.x;
    // calculate U parameter and test bounds
    o->u = dot(tvec, pvec) * inv_det;
    if (o->u < 0.0f || o->u > 1.0f)
        return false;
    // prepare to test V parameter
    float3 qvec = cross(tvec, edge1);
    // calculate V parameter and test bounds
    o->v = dot(ray.direction, qvec) * inv_det;
    if (o->v < 0.0f || o->u + o->v > 1.0f)
        return false;
    // calculate t, ray intersects triangle
    o->t = dot(edge2, qvec) * inv_det;
    return o->t < besthit->distance;
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
	float3 rayDir = normalize(MatMul(inverseView, target).xyz);
	vstore3(rayDir, i + j * width, rays);
}

kernel void Trace (
	write_only image2d_t screen,
	global const Texture* textures,
	global const RGB8* texturePixels,
	global const Mesh* meshes,
	global const unsigned* indices,
	global const Vertex* vertices,
	global const float* rays, 
	global const Sphere* spheres,
	TraceArgs trace_args
) 
{
	const int i = get_global_id(0), j = get_global_id(1);
	constant float oneDivGamma = 1.0f / 1.2f;
	
	Ray ray = CreateRay(vload3(0, trace_args.cameraPos), vload3(i + j * get_image_width(screen), rays));
	
	float3 lightDir = (float3)(0.5f, -0.5f, 0.0f); // sun dir
	float3 result = (float3)(0.0f, 0.0f, 0.0f);
	float atmosphericLight = 0.2f;
	
	RandState randState = GenRandomState();
	
	for (int j = 0; j < 1; ++j)// num bounces
	{
		RayHit besthit = CreateRayHit();
		HitRecord record = CreateHitRecord();
		float roughness = 0.5f; // plane roughness
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
			float3 center = vload3(0, currentSphere.position);
			record.point = ray.origin + ray.direction * besthit.distance;
			record.normal.xyz = normalize(record.point - center);
			float3 color = UnpackRGB8u(currentSphere.color);
			uchar textureIndex = currentSphere.color >> 24;

			RGB8 pixel = SAMPLE_SPHERE_TEXTURE(record.point, center, textures[textureIndex]);
			record.color.xyz = color * UNPACK_RGB8(pixel);
			roughness = currentSphere.roughness;
		}

		for (int m = 0; m < trace_args.numMeshes; ++m)
		{
			Mesh mesh = meshes[m];
			for (int i = 0; i < mesh.indexCount; i += 3)
			{
				Triangle triangle;
				triangle.x = vload3(0, &vertices[indices[i + 0]].x);
				triangle.y = vload3(0, &vertices[indices[i + 1]].x);
				triangle.z = vload3(0, &vertices[indices[i + 2]].x);
				
				Triout triout;
				if (IntersectTriangle(ray, triangle, &triout, &besthit))
				{
					record.point = ray.origin + triout.t * ray.direction;
					record.normal = fast_normalize(cross(triangle.y - triangle.z, triangle.z - triangle.x)); // maybe precalculate
					record.color = (float3)(0.8f, 0.8f,0.8f);
					besthit.distance = triout.t;
				}
			}
		}

		if (besthit.distance > InfMinusOne) 
		{
 			RGB8 pixel = texturePixels[SampleSkyboxPixel(ray.direction, textures[2])];
			float3 skybox_sample = UNPACK_RGB8(pixel);			
			result += skybox_sample * ray.energy;
			break;
		}
	
		ray.origin = record.point;
		ray.origin += record.normal * 0.001f;
		ray.direction = reflect(ray.direction, record.normal); // wo = ray.direction now = outgoing ray direction
	
		// check Shadow
		// Ray shadowRay = CreateRay(ray.origin, -lightDir);
		// besthit = CreateRayHit();
		float shadow = 1.0f; 
	
		// todo shadow for only directional light
	
		// Shade
		float ndl = max(dot(record.normal, -lightDir), atmosphericLight);
		atmosphericLight *= 0.4f;
		float3 specular = (float3)((1.0f - roughness) * ndl * shadow); // color.w = roughness
	
		result += ray.energy * (record.color * ndl);
		ray.energy *= specular;
		
		lightDir = ray.direction;
		
		if ((ray.energy.x + ray.energy.y + ray.energy.z) < 0.015f) break;
	}
	
	write_imagef(screen, (int2)(i, j), (float4)(pow(result, oneDivGamma), 0.1f));
}