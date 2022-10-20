#pragma once
#include "Math/Vector.hpp"
#include "Math/Vector4.hpp"
#include "ResourceManager.hpp"

#include <chrono>
#include <iostream>

#ifndef NDEBUG
#	define CSTIMER(message) Timer timer = Timer(message);
#else
#   define CSTIMER(message) // Timer timer = Timer(message);
#endif

struct Timer
{
	std::chrono::time_point<std::chrono::high_resolution_clock> start_point;
	bool printMilisecond;

	const char* message;

	Timer(const char* _message) : message(_message), printMilisecond(true)
	{
		start_point = std::chrono::high_resolution_clock::now();
	}

	const double GetTime()
	{
		using namespace std::chrono;
		auto end_point = high_resolution_clock::now();
		auto start = time_point_cast<microseconds>(start_point).time_since_epoch().count();
		auto end = time_point_cast<microseconds>(end_point).time_since_epoch().count();
		printMilisecond = false;
		return (end - start) * 0.001;
	}

	~Timer()
	{
		using namespace std::chrono;
		if (!printMilisecond) return;
		auto end_point = high_resolution_clock::now();
		auto start = time_point_cast<microseconds>(start_point).time_since_epoch().count();
		auto end = time_point_cast<microseconds>(end_point).time_since_epoch().count();
		auto _duration = end - start;
		std::cout << message << (_duration * 0.001) << "ms" << std::endl;
	}
};

// todo use tri.vertx.w as centeroid.x, use simd
// todo binning again

template <class T> void Swap(T& x, T& y) { T t; t = x, x = y, y = t; }

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
	float area() 
	{ 
		float3 e = bmax - bmin; // box extent
		return e.x * e.y + e.y * e.z + e.z * e.x; 
	}
};

static uint rootNodeIdx = 0, nodesUsed = 1;
static BVHNode* nodes;

FINLINE float GetCenteroid(Tri* tri, int axis)
{ 
	return *((&tri->centeroidx) + (axis * 4));
}

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
    	leafPtr += 3;
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

	for (int axis = 0; axis < 3; ++axis)
	{
		for(int i = 0; i < node->triCount; ++i)
		{
			Tri* triangle = tris + node->leftFirst + i;
			float candidatePos = GetCenteroid(triangle, axis);
			float cost = EvaluateSAH(node, tris, axis, candidatePos);
			if (cost < bestCost)
				*splitPos = candidatePos, *outAxis = axis, bestCost = cost;
		}	
	}
	return bestCost;
}

static void SubdivideBVH(BVHNode* bvhNode, Tri* tris, uint nodeIdx)
{
	// terminate recursion
	BVHNode* node = bvhNode + nodeIdx;
	if (node->triCount <= 2) return;
	
	// determine split axis and position
	// int axis;
	// float splitPos;
	// float splitCost = FindBestSplitPlane(bvhNode, tris, centeroids, &axis, &splitPos);
	// float nosplitCost = CalculateCost(node);
	// 
	// if (splitCost >= nosplitCost) return;
	
	// determine split axis and position
	float3 extent = node->aabbMax - node->aabbMin;
	int axis = 0;
	if (extent.y > extent.x) axis = 1;
	if (extent.z > extent[axis]) axis = 2;
	float splitPos = node->aabbMin[axis] + extent[axis] * 0.5f;

	// in-place partition
	int i = node->leftFirst;
	int j = i + node->triCount - 1;
	while (i <= j)
	{
		if (GetCenteroid(tris + i, axis) < splitPos)
			i++;
		else {
			// swap elements with simd 2x faster
			__m128* a = (__m128*)(tris + i);
			__m128* b = (__m128*)(tris + j);

			__m128 t[3] {*a, a[1], a[2] };
			a[0] = b[0];
			a[1] = b[1];
			a[2] = b[2];
			
			b[0] = t[0];
			b[1] = t[1];
			b[2] = t[2];
			j--;
		}
	}
	// abort split if one of the sides is empty
	int leftCount = i - node->leftFirst;
	if (leftCount == 0 || leftCount == node->triCount) return;
	// create child nodes
	int leftChildIdx = nodesUsed++;
	int rightChildIdx = nodesUsed++;
	bvhNode[leftChildIdx].leftFirst = node->leftFirst;
	bvhNode[leftChildIdx].triCount = leftCount;
	bvhNode[rightChildIdx].leftFirst = i;
	bvhNode[rightChildIdx].triCount = node->triCount - leftCount;
	node->leftFirst = leftChildIdx;
	node->triCount = 0;
	UpdateNodeBounds(bvhNode, tris, leftChildIdx);
	UpdateNodeBounds(bvhNode, tris, rightChildIdx);
	// recurse
	SubdivideBVH(bvhNode, tris, leftChildIdx);
	SubdivideBVH(bvhNode, tris, rightChildIdx);
}

BVHNode* BuildBVH(Tri* tris, int numTriangles, BVHNode* nodes, int* _nodesUsed)
{
	// 1239.74ms SIMD
	// 556.51ms  SIMD with custom swap
	// 6511.79ms withut
	CSTIMER("BVH build Time: ");

	// calculate triangle centroids for partitioning
	for (int i = 0; i < numTriangles; i++) // this loop vectorized by compiler
	{
		// set centeroids
		Tri* tri = tris + i;
		tri->centeroidx = (tri->vertex0.x + tri->vertex1.x + tri->vertex2.x) * 0.333333f;
		tri->centeroidy = (tri->vertex0.y + tri->vertex1.y + tri->vertex2.y) * 0.333333f;
		tri->centeroidz = (tri->vertex0.z + tri->vertex1.z + tri->vertex2.z) * 0.333333f;
	}

	// assign all triangles to root node
	BVHNode& root = nodes[rootNodeIdx];
	root.leftFirst = 0, root.triCount = numTriangles;
	UpdateNodeBounds(nodes, tris, rootNodeIdx);
	// subdivide recursively
	SubdivideBVH(nodes, tris, rootNodeIdx);
	*_nodesUsed = nodesUsed;
	return nodes;
}