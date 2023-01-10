#include "CPURayTrace.hpp"
#include "ResourceManager.hpp"
#include "Renderer.hpp"
#include "Math/Matrix.hpp"
#include <stdio.h>

typedef struct _RayHit {
	float3 position;
	float  distance;
	int    index;
} RayHit;

typedef struct _Triout {
	float t, u, v; uint triIndex;
} Triout;

#pragma pack(1)
struct RGB8 {
	unsigned char r, g, b;
};

namespace 
{
	uint* bvhIndices = nullptr;
	Tri* triangles  = nullptr;
	BVHNode* bvhNodes = nullptr;
	Material* materials = nullptr;
	MeshInstance* meshInstances = nullptr;

	RGB8* texturePixels = nullptr;
	Texture* textures = nullptr;
}

static inline RayHit CreateRayHit() {
	RayHit hit; 
	hit.distance = RayacastMissDistance;
	hit.index = 0;
	return hit;
}

static inline HitRecord CreateHitRecord() {
	HitRecord record;
	record.color  = float3(0.90f, 0.90f, 0.90f); // plane color
	record.normal = float3(0.0f, 1.0f, 0.0f);
	return record;
}

static bool IntersectPlane(const Ray& ray, RayHit* besthit)
{
	float t = -ray.origin.y / ray.direction.y;
	if (t > 0.0f && t < besthit->distance) 
	{
		float3 point = ray.origin + (ray.direction * t);
		if (fabsf(point.x) < 80.0f && fabsf(point.z) < 80.0f)
			besthit->distance = t;
		return true;
	}
	return false;
}

static bool IntersectTriangle(const Ray& ray, const Tri* tri, Triout* o, int i)
{
	const float3 edge1 = tri->vertex1 - tri->vertex0;
	const float3 edge2 = tri->vertex2 - tri->vertex0;
	const float3 h = Vector3f::Cross( ray.direction, edge2 );
	const float a = Vector3f::Dot( edge1, h );
	// if (fabs(a) < 0.0001f) return false; // ray parallel to triangle
	const float f = 1.0f / a;
	const float3 s = ray.origin - tri->vertex0;
	const float u = f * Vector3f::Dot( s, h );
	if (u < 0.0f || u > 1.0f) return false;
	const float3 q = Vector3f::Cross( s, edge1 );
	const float v = f * Vector3f::Dot( ray.direction, q );
	if (v < 0.0f || u + v > 1.0f) return false;
	const float t = f * Vector3f::Dot( edge2, q );
	if (t > 0.0001f && t < o->t) {
		o->u = u;  o->v = v; o->t = t;
		o->triIndex = i;
		return true;
	}
	return false;
}

static
float IntersectAABB(const float3& origin, const float3& invDir, const float3& aabbMin, const float3& aabbMax, float minSoFar)
{
	float3 tmin = (aabbMin - origin) * invDir;
	float3 tmax = (aabbMax - origin) * invDir;
	float tnear = Max3(fminf(tmin, tmax));
	float tfar  = Min3(fmaxf(tmin, tmax));
	// return tnear < tfar && tnear > 0.0f && tnear < minSoFar;
	if (tnear < tfar && tnear > 0.0f && tnear < minSoFar)
		return tnear; else return RayacastMissDistance;
}

#define SWAPF(x, y) float tf = x; x = y, y = tf;
#define SWAPUINT(x, y) uint tu = x; x = y, y = tu;

static 
int IntersectBVH(const Ray& ray, const BVHNode* nodes, uint rootNode, const Tri* tris, Triout* out)
{
	int nodesToVisit[32] = { rootNode };
	int currentNodeIndex = 1;
	float3 invDir = float3(1.0f) / ray.direction;
	int intersection = 0, protection = 0;
	
	while (currentNodeIndex > 0 && protection++ < 250)
	{	
		const BVHNode* node = nodes + nodesToVisit[--currentNodeIndex];
	traverse:
		uint triCount = node->triCount, leftFirst = node->leftFirst;
		if (triCount > 0) // is leaf 
		{
			for (int i = leftFirst, end = i + triCount; i < end; ++i)
				intersection |= IntersectTriangle(ray, tris + i, out, i);
			continue;
		}
	    
		uint leftIndex  = leftFirst;
		uint rightIndex = leftIndex + 1;
		BVHNode leftNode  = nodes[leftIndex];
		BVHNode rightNode = nodes[rightIndex];
		
		float dist1 = IntersectAABB(ray.origin, invDir, leftNode.aabbMin, leftNode.aabbMax, out->t);
		float dist2 = IntersectAABB(ray.origin, invDir, rightNode.aabbMin, rightNode.aabbMax, out->t);
	    
		if (dist1 > dist2) { SWAPF(dist1, dist2); SWAPUINT(leftIndex, rightIndex); }
		
		if (dist1 == RayacastMissDistance) continue;
		else {
			node = nodes + leftIndex;
			if (dist2 != RayacastMissDistance) nodesToVisit[currentNodeIndex++] = rightIndex;
			goto traverse;
		}
	}
	return intersection;
}

void CPU_RayTraceInitialize(MeshInstance* instances)
{
	meshInstances = instances;
	bvhNodes   = ResourceManager::GetBVHNodes();
	materials  = ResourceManager::GetMaterials();
	triangles  = ResourceManager::GetTriangles();
	bvhIndices = ResourceManager::GetBVHIndices();
	textures   = ResourceManager::GetTextures();
}

FINLINE float2 ConvertHalf2ToFloat2(const short* h) {
	float2 r;
	r.x = *h++ / 32766.0f;
	r.y = *h   / 32766.0f;
	return r;
}

FINLINE float3 ConvertHalf3ToFloat3(const short* h) {
	float3 r;
	r.x = *h++ / 32766.0f;
	r.y = *h++ / 32766.0f;
	r.z = *h   / 32766.0f;
	return r;
}

constexpr float UcharToFloat01 = 1.0f / 255.0f;

FINLINE float3 UnpackRGB8u(uint u)
{
	return float3(u & 255, u >> 8 & 255, u >> 16 & 255) * UcharToFloat01;
}

#define UNPACK_RGB8(rgb8) (float3(rgb8.r, rgb8.g, rgb8.b) * UcharToFloat01)

inline int SampleTexture(Texture texture, float2 uv)
{
	uv -= float2(floor(uv.x), floor(uv.y));
	int uScaled = (int)(texture.width * uv.x); // (0, 1) to (0, TextureWidth )
	int vScaled = (int)(texture.height * uv.y); // (0, 1) to (0, TextureHeight)
	return vScaled * texture.width + texture.offset + uScaled;
}

inline int SampleSkyboxPixel(float3 rayDirection, Texture texture)
{
	int theta = (int)((atan2f(rayDirection.x, -rayDirection.z) / PI) * 0.5f * (float)(texture.width));
	int phi = (int)((acosf(rayDirection.y) / PI) * (float)(texture.height));
	return (phi * texture.width) + theta + 2;
}

extern uint numMeshInstances;

HitRecord CPU_RayCast(Ray ray)
{
	RayHit besthit = CreateRayHit();
	HitRecord record = CreateHitRecord();
	
	if (IntersectPlane(ray, &besthit)) {
		record.color = float3(70.0f / 255.0f, 70.0f/ 255.0f, 70.0f/ 255.0f);
	}
	
	Ray meshRay;
	const uint numInstances = numMeshInstances;

	for (int i = 0; i < numInstances-1; ++i)
	{
		Triout triout;
		triout.t = besthit.distance;
		triout.triIndex = 0;
		MeshInstance& instance = meshInstances[i];
		// change ray position instead of mesh position for capturing in different positions
		meshRay.origin    = Matrix4::Vector4Transform(float4(ray.origin   , 1.0f), instance.inverseTransform).xyz();
		meshRay.direction = Matrix4::Vector4Transform(float4(ray.direction, 0.0f), instance.inverseTransform).xyz();
		
		Matrix3 inverseMat3 = Matrix4::ConvertToMatrix3(instance.inverseTransform);
	
		// instance.meshIndex = bvhIndex
		if (IntersectBVH(meshRay, bvhNodes, bvhIndices[instance.meshIndex], triangles, &triout))
		{
			Tri& triangle = triangles[triout.triIndex];
			// Material material = materials[instance.materialStart + triangle.materialIndex];
			float3 baryCentrics = float3(1.0f - triout.u - triout.v, triout.u, triout.v);
	        
			float3 n0 = Matrix3::Multiply(inverseMat3, ConvertHalf3ToFloat3(&triangle.normal0x));
			float3 n1 = Matrix3::Multiply(inverseMat3, ConvertHalf3ToFloat3(&triangle.normal1x));
			float3 n2 = Matrix3::Multiply(inverseMat3, ConvertHalf3ToFloat3(&triangle.normal2x));
	        
			record.normal = Vector3f::Normalize((n0 * baryCentrics.x) + (n1 * baryCentrics.y) + (n2 * baryCentrics.z));

			// record.uv = ConvertHalf2ToFloat2(&triangle.uv0x) * baryCentrics.x
			// 	      + ConvertHalf2ToFloat2(&triangle.uv1x) * baryCentrics.y
			// 	      + ConvertHalf2ToFloat2(&triangle.uv2x) * baryCentrics.z;
			
			// RGB8 pixel = texturePixels[SampleTexture(textures[material.albedoTextureIndex], record.uv)];
			// RGB8 specularPixel = texturePixels[SampleTexture(textures[material.specularTextureIndex], record.uv)];
	        // 
			// record.color = UnpackRGB8u(material.color) * UNPACK_RGB8(pixel);
			besthit.distance = triout.t;
			besthit.index = instance.meshIndex;
		}
	}
	
	if (besthit.distance == RayacastMissDistance) {
		// RGB8 pixel = texturePixels[SampleSkyboxPixel(ray.direction, textures[2])];
		// float3 skybox_sample = UNPACK_RGB8(pixel);			
		// record.color = skybox_sample;
		printf("miss\n");
	}
	record.distance = besthit.distance;
	record.index = besthit.index;

	return record;
}
