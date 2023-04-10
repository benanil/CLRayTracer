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

// from ResourceManager.cpp
extern uint* g_BVHIndices;
extern Tri* g_Triangles  ;
extern BVHNode* g_BVHNodes ;
extern Material* g_Materials;
extern RGB8* g_TexturePixels;
extern Texture* g_Textures;
// from Renderer.cpp
extern MeshInstance* g_MeshInstances;
extern uint g_NumMeshInstances;

static inline RayHit CreateRayHit() {
	RayHit hit; 
	hit.distance = RayacastMissDistance;
	hit.index = 0;
	return hit;
}

static inline HitRecord CreateHitRecord() {
	HitRecord record;
	record.normal = float3(0.0f, 1.0f, 0.0f);
	record.distance = RayacastMissDistance;
	return record;
}

static bool VECTORCALL IntersectTriangle(const RaySSE& ray, const Tri* tri, Triout* o, int i)
{
	const __m128 edge1 = _mm_and_ps(_mm_sub_ps(tri->v1, tri->v0), g_XMSelect1110);
	const __m128 edge2 = _mm_and_ps(_mm_sub_ps(tri->v2, tri->v0), g_XMSelect1110);
	const __m128 h = SSEVector3Cross(ray.direction, edge2);
	const __m128 a = _mm_dp_ps(edge1, h, 0xff);
	// if (fabs(a) < 0.0001f) return false; // ray parallel to triangle
	const __m128 f = _mm_rcp_ps(a);
	const __m128 s = _mm_sub_ps(ray.origin, tri->v0);
	const float  u = _mm_cvtss_f32(_mm_mul_ps(f, _mm_dp_ps(s, h, 0xff)));
	const __m128 q = SSEVector3Cross( s, edge1 );
	const float  v = _mm_cvtss_f32(_mm_mul_ps(f, _mm_dp_ps(ray.direction, q, 0xff)));
	const float  t = _mm_cvtss_f32(_mm_mul_ps(f, _mm_dp_ps(edge2, q, 0xff)));
	// if passed we are going to draw triangle
	int passed = (((t > 0.0000f) ^ (t < o->t)) + (u < 0.0f) + (u > 1.0f) + (v < 0.0f) + (u + v > 1.0f)) == 0;
	int notPassed = 1-passed;
	o->u = u * passed + (notPassed * o->u);
	o->v = v * passed + (notPassed * o->v); 
	o->t = t * passed + (notPassed * o->t);
	o->triIndex = i * passed + (notPassed * o->triIndex);
	return passed;
}

FINLINE float VECTORCALL Min3(__m128 ab)
{
	__m128 xy = _mm_min_ps(SSESplatX(ab), SSESplatY(ab));
	return _mm_cvtss_f32(_mm_min_ps(xy, SSESplatZ(ab)));
}

FINLINE float VECTORCALL Max3(__m128 ab)
{
	__m128 xy = _mm_max_ps(SSESplatX(ab), SSESplatY(ab));
	return _mm_cvtss_f32(_mm_max_ps(xy, SSESplatZ(ab)));
}

static float IntersectAABB(__m128 origin, const __m128 invDir, const __m128 aabbMin, const __m128& aabbMax, float minSoFar)
{
	__m128 tmin = _mm_mul_ps(_mm_sub_ps(aabbMin, origin), invDir);
	__m128 tmax = _mm_mul_ps(_mm_sub_ps(aabbMax, origin), invDir);
	float tnear = Max3(_mm_min_ps(tmin, tmax));
	float tfar  = Min3(_mm_max_ps(tmin, tmax));
	// return tnear < tfar && tnear > 0.0f && tnear < minSoFar;
	if (tnear < tfar && tnear > 0.0f && tnear < minSoFar)
		return tnear; else return RayacastMissDistance;
}

#define SWAPF(x, y) float tf = x; x = y, y = tf;
#define SWAPUINT(x, y) uint tu = x; x = y, y = tu;

static bool IntersectBVH(const RaySSE& ray, const BVHNode* nodes, uint rootNode, const Tri* tris, Triout* out)
{
	int nodesToVisit[32] = { (int)rootNode };
	int currentNodeIndex = 1;
	__m128 invDir = _mm_rcp_ps(ray.direction);
	bool intersection = 0; int protection = 0;
	
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
		
		float dist1 = IntersectAABB(ray.origin, invDir, leftNode.minv, leftNode.maxv, out->t);
		float dist2 = IntersectAABB(ray.origin, invDir, rightNode.minv, rightNode.maxv, out->t);
	    
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

void CPU_RayTraceInitialize()
{
	// maybe fill later
}

_NODISCARD FINLINE float2 ConvertToFloat2(const half* h)
{
	return float2(ConvertHalfToFloat(h[0]), ConvertHalfToFloat(h[1]));
}

_NODISCARD FINLINE float3 ConvertToFloat3(const half* h) {
	return float3(
		ConvertHalfToFloat(h[0]),
		ConvertHalfToFloat(h[1]),
		ConvertHalfToFloat(h[2])
    );
}

_NODISCARD FINLINE uint MultiplyU32Colors(uint a, RGB8 b)
{
	uint result = 0u;
	result |= ((a & 0xffu) * b.r) >> 8u;
	result |= ((((a >> 8u) & 0xffu) * b.g) >> 8u) << 8u;
	result |= ((((a >> 16u) & 0xffu) * b.b) >> 8u) << 16u;
	return result;
}

constexpr float UcharToFloat01 = 1.0f / 255.0f;

_NODISCARD FINLINE float3 UnpackRGB8u(uint u)
{
	return float3(
		float(u & 255u)        * UcharToFloat01,
		float(u >> 8u  & 255u) * UcharToFloat01,
		float(u >> 16u & 255u) * UcharToFloat01
	);
}

#define UNPACK_RGB8(rgb8) (float3(rgb8.r, rgb8.g, rgb8.b) * UcharToFloat01)

inline int SampleTexture(Texture texture, float2 uv)
{
	uv -= float2(floorf(uv.x), floorf(uv.y));
	int uScaled = (int)(texture.width * uv.x); // (0, 1) to (0, TextureWidth )
	int vScaled = (int)(texture.height * uv.y); // (0, 1) to (0, TextureHeight)
	return vScaled * texture.width + texture.offset + uScaled;
}

inline int SampleSkyboxPixel(float3 rayDirection, Texture texture)
{
	int theta = (int)((ATan2(rayDirection.x, -rayDirection.z) / PI) * 0.5f * (float)(texture.width));
	int phi = (int)((ACos(rayDirection.y) / PI) * (float)(texture.height));
	return (phi * texture.width) + theta + 2;
}

// todo ignore mask
HitRecord CPU_RayCast(RaySSE ray)
{
	RayHit besthit = CreateRayHit();
	HitRecord record = CreateHitRecord();
	
	Triout hitOut;
	uint hitInstanceIndex = 0u;
	ray.origin.m128_f32[3] = 1.0f;
	ray.direction.m128_f32[3] = 0.0f;

	for (uint i = 0; i < g_NumMeshInstances; ++i)
	{
		Triout triout;
		triout.t = besthit.distance;
		triout.triIndex = 0;
		MeshInstance& instance = g_MeshInstances[i];
		RaySSE meshRay;
		// change ray position&oriantation instead of mesh position for capturing in different positions
		
		meshRay.origin    = Vector4Transform(ray.origin, instance.inverseTransform);
		meshRay.direction = Vector4Transform(ray.direction, instance.inverseTransform);
		// instance.meshIndex = bvhIndex
		if (IntersectBVH(meshRay, g_BVHNodes, g_BVHIndices[instance.meshIndex], g_Triangles, &triout))
		{
			hitOut = triout;
			hitInstanceIndex = i;
			besthit.distance = triout.t;
			besthit.index = instance.meshIndex;
		}
	}
	
	if (besthit.distance == RayacastMissDistance) {
		union u { uint color; RGB8 rgb; } us;
		float3 v3;
		SSEStoreVector3(&v3.x, ray.direction);
		us.rgb = g_TexturePixels[SampleSkyboxPixel(v3, g_Textures[2])];
		record.color = us.color; // convert skybox color to uint32sa
		return record;
	}

	MeshInstance hitInstance = g_MeshInstances[hitInstanceIndex];
	Tri& triangle = g_Triangles[hitOut.triIndex];
	Material material = g_Materials[hitInstance.materialStart + triangle.materialIndex];
	float3 baryCentrics = float3(1.0f - hitOut.u - hitOut.v, hitOut.u, hitOut.v);
	Matrix3 inverseMat3 = Matrix4::ConvertToMatrix3(hitInstance.inverseTransform);
			
	float3 n0 = Matrix3::Multiply(inverseMat3, ConvertToFloat3(&triangle.normal0x));
	float3 n1 = Matrix3::Multiply(inverseMat3, ConvertToFloat3(&triangle.normal1x));
	float3 n2 = Matrix3::Multiply(inverseMat3, ConvertToFloat3(&triangle.normal2x));
	        
	record.normal = Vector3f::Normalize((n0 * baryCentrics.x) + (n1 * baryCentrics.y) + (n2 * baryCentrics.z));

	record.uv = ConvertToFloat2(&triangle.uv0x) * baryCentrics.x
		        + ConvertToFloat2(&triangle.uv1x) * baryCentrics.y
		        + ConvertToFloat2(&triangle.uv2x) * baryCentrics.z;
	
	RGB8 pixel = g_TexturePixels[SampleTexture(g_Textures[material.albedoTextureIndex], record.uv)];
	
	record.color = MultiplyU32Colors(material.color, pixel);

	record.distance = besthit.distance;
	record.index = besthit.index;
	return record;
}
