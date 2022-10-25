#pragma once
#include "Math/Vector.hpp"
#include "Math/Vector4.hpp"
#include "ResourceManager.hpp"
#include <exception>

// todo use tri.vertx.w as centeroid.x, use simd
// todo binning again

struct aabb 
{ 
	float3 bmin = 1e30f, bmax = -1e30f;
	
	void grow(float3 p) 
	{ 
		bmin = fminf(bmin, p), bmax = fmaxf(bmax, p); 
	}

	void grow(const Tri* tri) { 
		bmin = fminf(bmin, tri->vertex0), bmax = fmaxf(bmax, tri->vertex0);
		bmin = fminf(bmin, tri->vertex1), bmax = fmaxf(bmax, tri->vertex1); 
		bmin = fminf(bmin, tri->vertex2), bmax = fmaxf(bmax, tri->vertex2); 
	}
	
	void grow(aabb other)
	{
		bmin = fminf(bmin, other.bmin), bmax = fmaxf(bmax, other.bmax);
	}

	float area() 
	{ 
		float3 e = bmax - bmin; // box extent
		return e.x * e.y + e.y * e.z + e.z * e.x; 
	}
};

struct Bin {
	aabb bounds; uint triCount = 0;
};

static uint nodesUsed = 0;
static BVHNode* nodes;

#define GetCenteroid(tri, axis) *((&(tri)->centeroidx) + (axis * 4))

static void UpdateNodeBounds(BVHNode* bvhNode, const Tri* tris, uint nodeIdx)
{
	BVHNode* node = bvhNode + nodeIdx;
	__m128 nodeMin = _mm_set1_ps(1e30f), nodeMax = _mm_set1_ps(-1e30f);
    const __m128* leafPtr = (const __m128*)(tris + node->leftFirst);
    
    for (uint i = 0; i < node->triCount; i++)
    {
    	nodeMin = _mm_min_ps(nodeMin, leafPtr[0]);
    	nodeMin = _mm_min_ps(nodeMin, leafPtr[1]);
    	nodeMin = _mm_min_ps(nodeMin, leafPtr[2]);
    	
    	nodeMax = _mm_max_ps(nodeMax, leafPtr[0]);
    	nodeMax = _mm_max_ps(nodeMax, leafPtr[1]);
    	nodeMax = _mm_max_ps(nodeMax, leafPtr[2]);
    	leafPtr += 4; // +3 for vertex + 1 for texcoords
    }
    
    SSEStoreVector3(&node->aabbMin.x, nodeMin);
    SSEStoreVector3(&node->aabbMax.x, nodeMax);
}

FINLINE float CalculateCost(const BVHNode* node)
{ 
	float3 e = node->aabbMax - node->aabbMin;
	float surfaceArea = e.x * e.y + e.y * e.z + e.z * e.x;
	return node->triCount * surfaceArea;
}

static float EvaluateSAH(const BVHNode* node, Tri* tris, int axis, float pos)
{
	aabb leftBox, rightBox;
	int leftCount = 0, rightCount = 0;
	for (int i = 0; i < node->triCount; ++i)
	{
		Tri* triangle = tris + node->leftFirst + i;
		if (GetCenteroid(triangle, axis) < pos) {
			leftCount++;
			leftBox.grow(triangle);
		}
		else {
			rightCount++;
			rightBox.grow(triangle);
		}
	}
	float cost = leftCount * leftBox.area() + rightCount * rightBox.area();
	return cost > 0.0f ? cost : 1e30f;
}

static float FindBestSplitPlane(const BVHNode* node, Tri* tris, int* outAxis, float* splitPos)
{
	float bestCost = 1e30f;
	uint triCount = node->triCount, leftFirst = node->leftFirst;

	for (int axis = 0; axis < 3; ++axis)
	{
		float boundsMin = 1e30f, boundsMax = -1e30f;
		
		for (int i = 0; i < triCount; ++i)
		{
			Tri* triangle = tris + i;
			float val = GetCenteroid(triangle, axis);
			boundsMin = Min(boundsMin, val);
			boundsMax = Max(boundsMax, val);
		}

		if (boundsMax == boundsMin) continue;

		constexpr int BINS = 8;
		Bin bin[BINS] = {};
		float scale = float(BINS) / (boundsMax - boundsMin);
		for (uint i = 0; i < triCount; i++)
		{
			Tri* triangle = tris + leftFirst + i;
			float centeroid = GetCenteroid(triangle, axis);
			int binIdx = Clamp((int)((centeroid - boundsMin) * scale), 0, BINS - 1);

			bin[binIdx].triCount++;
			bin[binIdx].bounds.grow(triangle);
		}
		
		float leftArea[BINS - 1], rightArea[BINS - 1];
		int leftCount[BINS - 1], rightCount[BINS - 1];

		int leftSum = 0, rightSum = 0;
		aabb leftBox{}, rightBox{};

		for (int i = 0; i < BINS - 1; i++)
		{
			leftSum += bin[i].triCount;
			leftCount[i] = leftSum;
			leftBox.grow( bin[i].bounds );
			leftArea[i] = leftBox.area();
			rightSum += bin[BINS - 1 - i].triCount;
			rightCount[BINS - 2 - i] = rightSum;
			rightBox.grow( bin[BINS - 1 - i].bounds );
			rightArea[BINS - 2 - i] = rightBox.area();
		}

		scale = (boundsMax - boundsMin) / float(BINS);
		for (int i = 0; i < BINS - 1; i++)
		{
			float planeCost = leftCount[i] * leftArea[i] + rightCount[i] * rightArea[i];
			if (planeCost < bestCost)
				*splitPos = boundsMin + scale * (i + 1),
				*outAxis = axis, bestCost = planeCost;
		}
	}
	return bestCost;
}


static void SubdivideBVH(BVHNode* bvhNode, Tri* tris, uint nodeIdx)
{
	// terminate recursion
	BVHNode* node = bvhNode + nodeIdx;
	uint leftFirst = node->leftFirst, triCount = node->triCount;
	if (triCount <= 2) return;
	// determine split axis and position
	// int axis;
	// float splitPos;
	// float splitCost = FindBestSplitPlane(node, tris, &axis, &splitPos);
	// float nosplitCost = CalculateCost(node);
	// 
	// if (splitCost >= nosplitCost) return;
	
	// // determine split axis and position
	float3 extent = node->aabbMax - node->aabbMin; 
	int axis = extent.y > extent.x;            // if (extent.y > extent.x) axis = 1; // premature optimization :D
	axis = extent.z > extent[axis] ? 2 : axis; // if (extent.z > extent[axis]) axis = 2;
	
	float splitPos = node->aabbMin[axis] + extent[axis] * 0.5f;

	// in-place partition
	int i = leftFirst;
	int j = i + triCount - 1;
	while (i <= j)
	{
		if (GetCenteroid(tris + i, axis) < splitPos)
			i++;
		else {
			// swap elements with simd 2x faster
			__m128* a = (__m128*)(tris + i);
			__m128* b = (__m128*)(tris + j);
			
			__m128 t = *a;
			*a++ = *b, *b++ = t, t = *a; // swap a[0], b[0] 
			*a++ = *b, *b++ = t, t = *a; // swap a[1], b[1]
			*a++ = *b, *b++ = t, t = *a; // swap a[2], b[2]
			*a = *b, *b = t;             // swap a[3], b[3]
			
			j--;
		}
	}
	// abort split if one of the sides is empty
	int leftCount = i - leftFirst;
	if (leftCount == 0 || leftCount == triCount) return;
	// create child nodes
	int leftChildIdx = nodesUsed++;
	int rightChildIdx = nodesUsed++;
	bvhNode[leftChildIdx].leftFirst = leftFirst;
	bvhNode[leftChildIdx].triCount = leftCount;
	bvhNode[rightChildIdx].leftFirst = i;
	bvhNode[rightChildIdx].triCount = triCount - leftCount;
	node->leftFirst = leftChildIdx;
	node->triCount = 0;
	UpdateNodeBounds(bvhNode, tris, leftChildIdx);
	UpdateNodeBounds(bvhNode, tris, rightChildIdx);
	// recurse
	SubdivideBVH(bvhNode, tris, leftChildIdx);
	SubdivideBVH(bvhNode, tris, rightChildIdx);
}

BVHNode* BuildBVH(Tri* tris, MeshInfo* meshes, int numMeshes, BVHNode* nodes, uint* bvhIndices, uint* _nodesUsed)
{
	// 1239.74ms SIMD
	// 556.51ms  SIMD with custom swap
	// 6511.79ms withut
	// CSTIMER("BVH build Time ms: ");
	uint numTriangles = 0;
	for (int i = 0; i < numMeshes; ++i) {
		numTriangles += meshes[i].numTriangles;
	}

	// calculate triangle centroids for partitioning
	for (int i = 0; i < numTriangles; i++) { // this loop will automaticly vectorized by compiler
		// set centeroids
		Tri* tri = tris + i;
		tri->centeroidx = (tri->vertex0.x + tri->vertex1.x + tri->vertex2.x) * 0.333333f;
		tri->centeroidy = (tri->vertex0.y + tri->vertex1.y + tri->vertex2.y) * 0.333333f;
		tri->centeroidz = (tri->vertex0.z + tri->vertex1.z + tri->vertex2.z) * 0.333333f;
	}
	uint currTriangle = 0;
	for (int i = 0; i < numMeshes; ++i) {
		// assign all triangles to root node
		uint rootNodeIndex = nodesUsed++;

		bvhIndices[i] = rootNodeIndex;
		
		BVHNode& root = nodes[rootNodeIndex];
		root.leftFirst = currTriangle, root.triCount = meshes[i].numTriangles;

		UpdateNodeBounds(nodes, tris, rootNodeIndex);
		// subdivide recursively
		SubdivideBVH(nodes, tris, rootNodeIndex);	
		currTriangle += meshes[i].numTriangles;
	}
	
	*_nodesUsed = nodesUsed;
	return nodes;
}