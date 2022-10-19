#include "ResourceManager.hpp"
#define __SSE__
#define __SSE2__
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#include <stb_image.h>
#define FAST_OBJ_IMPLEMENTATION
#include <fast_obj.h>
#include <malloc.h>
#include "Logger.hpp"

template <class T> void Swap(T& x, T& y) { T t; t = x, x = y, y = t; }

static uint rootNodeIdx = 0, nodesUsed = 1;
static BVHNode* nodes;

static void UpdateNodeBounds(BVHNode* bvhNode, Tri* tris, uint nodeIdx)
{
	BVHNode& node = bvhNode[nodeIdx];
	node.aabbMin = float3(1e30f);
	node.aabbMax = float3(-1e30f);
	for (uint first = node.leftFirst, i = 0; i < node.triCount; i++)
	{
		Tri& leafTri = tris[first + i];
		node.aabbMin = fminf(node.aabbMin, leafTri.vertex0);
		node.aabbMin = fminf(node.aabbMin, leafTri.vertex1);
		node.aabbMin = fminf(node.aabbMin, leafTri.vertex2);
		node.aabbMax = fmaxf(node.aabbMax, leafTri.vertex0);
		node.aabbMax = fmaxf(node.aabbMax, leafTri.vertex1);
		node.aabbMax = fmaxf(node.aabbMax, leafTri.vertex2);
	}
}

static void SubdivideBVH(BVHNode* bvhNode, Tri* tris, float3* centeroids, uint nodeIdx)
{
	// terminate recursion
	BVHNode& node = bvhNode[nodeIdx];
	if (node.triCount <= 2) return;
	// determine split axis and position
	float3 extent = node.aabbMax - node.aabbMin;
	int axis = 0;
	if (extent.y > extent.x) axis = 1;
	if (extent.z > extent[axis]) axis = 2;
	float splitPos = node.aabbMin[axis] + extent[axis] * 0.5f;
	// in-place partition
	int i = node.leftFirst;
	int j = i + node.triCount - 1;
	while (i <= j)
	{
		if (centeroids[i][axis] < splitPos)
			i++;
		else {
			Swap(tris[i], tris[j]);
			Swap(centeroids[i], centeroids[j--]);
		}
	}
	// abort split if one of the sides is empty
	int leftCount = i - node.leftFirst;
	if (leftCount == 0 || leftCount == node.triCount) return;
	// create child nodes
	int leftChildIdx = nodesUsed++;
	int rightChildIdx = nodesUsed++;
	bvhNode[leftChildIdx].leftFirst = node.leftFirst;
	bvhNode[leftChildIdx].triCount = leftCount;
	bvhNode[rightChildIdx].leftFirst = i;
	bvhNode[rightChildIdx].triCount = node.triCount - leftCount;
	node.leftFirst = leftChildIdx;
	node.triCount = 0;
	UpdateNodeBounds(bvhNode, tris, leftChildIdx);
	UpdateNodeBounds(bvhNode, tris, rightChildIdx);
	// recurse
	SubdivideBVH(bvhNode, tris, centeroids, leftChildIdx);
	SubdivideBVH(bvhNode, tris, centeroids, rightChildIdx);
}

static void BuildBVH(Tri* tris, int numTriangles)
{
	float3* centeroids = new float3[numTriangles];
	// calculate triangle centroids for partitioning
	for (int i = 0; i < numTriangles; i++)
		centeroids[i] = (tris[i].vertex0 + tris[i].vertex1 + tris[i].vertex2) * 0.3333f;

	nodes = new BVHNode[numTriangles * 2ul];

	// assign all triangles to root node
	BVHNode& root = nodes[rootNodeIdx];
	root.leftFirst = 0, root.triCount = numTriangles;
	UpdateNodeBounds(nodes, tris, rootNodeIdx);
	// subdivide recursively
	SubdivideBVH(nodes, tris, centeroids, rootNodeIdx);
	delete[] centeroids;
}

typedef struct _Triout {
	float t, u, v; uint triIndex;
} Triout;

struct Ray { float3 origin, direction; };

float Max3(float3 a) { return fmax(fmax(a.x, a.y), a.z); }
float Min3(float3 a) { return fmin(fmin(a.x, a.y), a.z); }

bool IntersectAABB(float3 origin, float3 invDir, float3 aabbMin, float3 aabbMax, float minSoFar)
{
	float3 tmin = (aabbMin - origin) * invDir;
	float3 tmax = (aabbMax - origin) * invDir;
	float tnear = Max3(fminf(tmin, tmax));
	float tfar = Min3(fmaxf(tmin, tmax));
	return tnear < tfar&& tnear > 0.0f; // tnear < min so far 
}

bool IntersectTriangle(Ray ray, Tri tri, Triout* o, int i)
{
	const float3 edge1 = tri.vertex1 - tri.vertex0;
	const float3 edge2 = tri.vertex2 - tri.vertex0;
	const float3 h = float3::Cross(ray.direction, edge2);
	const float a = float3::Dot(edge1, h);
	//if (fabs(a) < 0.0001f) return false; // ray parallel to triangle
	const float f = 1.0f / a;
	const float3 s = ray.origin - tri.vertex0;
	const float u = f * float3::Dot(s, h);
	if (u < 0.0f || u > 1.0f) return false;
	const float3 q = float3::Cross(s, edge1);
	const float v = f * float3::Dot(ray.direction, q);
	if (v < 0.0f || u + v > 1.0f) return false;
	const float t = f * float3::Dot(edge2, q);
	if (t > 0.0001f && t < o->t) {
		o->u = u; o->v = v; o->t = t;
		o->triIndex = i;
		return true;
	}
	return false;
}

void IntersectBVH(Ray ray, const BVHNode* nodes, int nodeIndex, const Tri* tris, Triout* out)
{
	float3 invDir = (float3)(1.0f) / ray.direction;
	const BVHNode* node = nodes + nodeIndex;

	if (!IntersectAABB(ray.origin, invDir, node->aabbMin, node->aabbMax, out->t)) return;

	if (node->isLeaf()) // is leaf 
	{
		for (int i = node->leftFirst, end = i + node->triCount; i < end; ++i)
		{
			IntersectTriangle(ray, tris[i], out, i);
		}
	}
	else {
		uint leftNode = node->leftFirst;
		IntersectBVH(ray, nodes, leftNode, tris, out);
		IntersectBVH(ray, nodes, leftNode + 1, tris, out);
	}
}

// we will import textures and mesh to same memory and push it to the gpu
// this way we are avoiding more memory allocations
// 3 = num color channels

namespace ResourceManager
{
	cl_int clerr;
	cl_mem textureHandleMem, textureDataMem;
	cl_mem meshTriangleMem, bvhMem, meshMem;

	Texture textures[CL_MAX_RESOURCE];
	Mesh    meshes  [CL_MAX_RESOURCE];
	unsigned char* arena;

	size_t arenaOffset = 6;

	int numTextures = 2;
	int numMeshes = 0;

	cl_mem* GetTextureHandleMem() { return &textureHandleMem; }
	cl_mem* GetTextureDataMem()   { return &textureDataMem; }
	cl_mem* GetBVHMem()           { return &bvhMem; }
			
	cl_mem* GetMeshTriangleMem() { return &meshTriangleMem; }
	cl_mem* GetMeshesMem()       { return &meshMem;       }

	ushort GetNumMeshes() { return numMeshes; };
}

void ResourceManager::Initialize()
{
	arena = new unsigned char[CL_MAX_RESOURCE_MEMORY];
	// create default textures. white, black
	textures[0].width = 1;  textures[1].width = 1;
	textures[0].height = 1;  textures[1].height = 1;
	textures[0].offset = 0;  textures[1].offset = 3;
	arena[0] = 0xFF; arena[1] = 0xFF; arena[2] = 0xFF;
	arena[3] = 0;    arena[4] = 0;    arena[5] = 0;

	arenaOffset = 6; numTextures = 2;
}

TextureHandle ResourceManager::ImportTexture(const char* path)
{
	Texture& texture = textures[numTextures]; int channels;
    unsigned char* ptr = stbi_load(path, &texture.width, &texture.height, &channels, 3);
	unsigned long numBytes = texture.height * texture.width * 3;
	if (arenaOffset + numBytes >= CL_MAX_RESOURCE_MEMORY)
	{
		AXERROR("mesh importing failed! CL_MAX_RESOURCE_MEMORY is not enough!"); assert(0);
		exit(0); return 0;
	}
	memcpy(arena + arenaOffset, ptr, numBytes);
	texture.offset = arenaOffset / 3;
	arenaOffset += numBytes;
	free(ptr);
	return numTextures++;
}

void ResourceManager::PushTexturesToGPU(cl_context context)
{
	textureDataMem   = clCreateBuffer(context, 32, arenaOffset, arena, &clerr); assert(clerr == 0);
	textureHandleMem = clCreateBuffer(context, 32, sizeof(Texture) * numTextures, textures, &clerr); assert(clerr == 0);
}

void ResourceManager::PrepareMeshes() {
	arenaOffset = 0;
}

MeshHandle ResourceManager::ImportMesh(const char* path)
{
	// todo add texcoords, maybe normals
	Mesh& mesh = meshes[numMeshes];

	fastObjMesh* meshObj = fast_obj_read(path);

	mesh.numTriangles = meshObj->index_count / 3; mesh.triangleStart = arenaOffset / sizeof(Tri);
	int indexCount = meshObj->index_count;

	if (arenaOffset + meshObj->position_count > CL_MAX_RESOURCE_MEMORY)
	{
		AXERROR("mesh importing failed! CL_MAX_RESOURCE_MEMORY is not enough!"); assert(0);
		exit(0); return 0;
	}

	Tri* tris = (Tri*)(arena + arenaOffset);

	fastObjIndex* meshIdx = meshObj->indices;

	for (int i = 0; i < mesh.numTriangles; ++i)
	{
		float* posPtr = meshObj->positions + (meshIdx->p * 3);
		tris->vertex0 = float3(posPtr[0], posPtr[1], posPtr[2]);
		meshIdx++;

		posPtr = meshObj->positions + (meshIdx->p * 3);
		tris->vertex1 = float3(posPtr[0], posPtr[1], posPtr[2]);
		meshIdx++;	

		posPtr = meshObj->positions + (meshIdx->p * 3);
		tris->vertex2 = float3(posPtr[0], posPtr[1], posPtr[2]);
		meshIdx++; tris++;
	}

	arenaOffset += mesh.numTriangles * sizeof(Tri);

	fast_obj_destroy(meshObj);
	return numMeshes++;
}

void ResourceManager::PushMeshesToGPU(cl_context context)
{
	int numTriangles = arenaOffset / sizeof(Tri);
	Tri* tris = (Tri*)arena;
	BuildBVH(tris, numTriangles);
	printf("node min: %f, %f, %f max: %f, %f, %f\n", nodes->aabbMin.x, nodes->aabbMin.y, nodes->aabbMin.z, nodes->aabbMax.x, nodes->aabbMax.y, nodes->aabbMax.z);
	
	Ray ray;
	ray.origin    = float3(0.0f, 0.0f, -2.0f);
	ray.direction = float3(0.0f, 0.0f, 1.0f);

	Triout out;
	out.triIndex = 9999999999;
	out.t = 999999.0f;
	IntersectBVH(ray, nodes, 0, tris, &out);

	if (out.triIndex != 9999999999)
	{
		printf("hell yeah: %d", out.triIndex);
	}

	meshTriangleMem = clCreateBuffer(context, 32, arenaOffset, arena, &clerr); assert(clerr == 0);
	bvhMem = clCreateBuffer(context, 32, sizeof(BVHNode) * nodesUsed, nodes, &clerr); assert(clerr == 0);
	meshMem = clCreateBuffer(context, 32, sizeof(Mesh) * numMeshes, meshes, &clerr); assert(clerr == 0);
	delete[] arena;
}

void ResourceManager::Destroy()
{
	clReleaseMemObject(textureHandleMem);
	clReleaseMemObject(textureDataMem);
	clReleaseMemObject(bvhMem);
	clReleaseMemObject(meshTriangleMem);
	clReleaseMemObject(meshMem);
}
