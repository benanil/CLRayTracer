#pragma once
#include "Math/Vector.hpp"
#include "Math/Vector4.hpp"
#include "ResourceManager.hpp"

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

static void UpdateNodeBounds(BVHNode* bvhNode, const Tri* tris, uint nodeIdx)
{
	BVHNode* node = bvhNode + nodeIdx;
	node->aabbMin = float3(1e30f);
	node->aabbMax = float3(-1e30f);
	for (uint first = node->leftFirst, i = 0; i < node->triCount; i++)
	{
		const Tri* leafTri = tris + first + i;
		node->aabbMin = fminf(node->aabbMin, leafTri->vertex0);
		node->aabbMin = fminf(node->aabbMin, leafTri->vertex1);
		node->aabbMin = fminf(node->aabbMin, leafTri->vertex2);
		node->aabbMax = fmaxf(node->aabbMax, leafTri->vertex0);
		node->aabbMax = fmaxf(node->aabbMax, leafTri->vertex1);
		node->aabbMax = fmaxf(node->aabbMax, leafTri->vertex2);
	}
}

FINLINE float CalculateCost(const BVHNode* node)
{ 
	float3 e = node->aabbMax - node->aabbMin;
	float surfaceArea = e.x * e.y + e.y * e.z + e.z * e.x;
	return node->triCount * surfaceArea;
}

static float EvaluateSAH(const BVHNode* node, const Tri* tris, const float3* centeroids, int axis, float pos)
{
	aabb leftBox, rightBox;
	int leftCount = 0, rightCount = 0;
	for (int i = 0; i < node->triCount; ++i)
	{
		const Tri* triangle = tris + node->leftFirst + i;
		if (centeroids[node->leftFirst + i][axis] < pos) {
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

static float FindBestSplitPlane(const BVHNode* node, const Tri* tris, const float3* centeroids, int* outAxis, float* splitPos)
{
	float bestCost = 1e30f;

	for (int axis = 0; axis < 3; ++axis)
	{
		for(int i = 0; i < node->triCount; ++i)
		{
			const Tri* triangle = tris + node->leftFirst + i;
			float candidatePos = centeroids[node->leftFirst + i][axis];
			float cost = EvaluateSAH(node, tris, centeroids, axis, candidatePos);
			if (cost < bestCost)
				*splitPos = candidatePos, *outAxis = axis, bestCost = cost;
		}	
	}
	return bestCost;
}

static void SubdivideBVH(BVHNode* bvhNode, Tri* tris, float3* centeroids, uint nodeIdx)
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
		if (centeroids[i][axis] < splitPos)
			i++;
		else {
			Swap(tris[i], tris[j]);
			Swap(centeroids[i], centeroids[j--]);
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
	SubdivideBVH(bvhNode, tris, centeroids, leftChildIdx);
	SubdivideBVH(bvhNode, tris, centeroids, rightChildIdx);
}

BVHNode* BuildBVH(Tri* tris, int numTriangles, int* _nodesUsed)
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
	*_nodesUsed = nodesUsed;
	return nodes;
}