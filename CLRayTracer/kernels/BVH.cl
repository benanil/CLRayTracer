
// software as is bla bla license bla bla gev gev gev
// Anilcan Gulkaya 10/03/2022
// #include<MathAndSTL.cl>

typedef struct _BVHNode{
	float4 min; // w triStart + triCount
	float4 max; // w leftNode
} BVHNode;

#define GetLeftNode(nod)  (as_uint((nod)->max.w))
#define GetTriStart(nod)  (as_uint((nod)->min.w) & 0xFFFFu)
#define GetTriCount(nod)  (as_uint((nod)->min.w) >> 16u)

#define SetLeftNode(nod, x)  (nod)->max.w = as_float(x)
#define SetTriStartTriCount(nod, start, count)  (nod)->min.w = as_float(start | ((count) << 16))
#define SetTriCount0(nod) (nod)->min.w = as_float(as_uint((nod)->min.w) & 0xFFFFu)

void SetCenteroid(global Tri* tris, float3 vec) {
	tris->vert0.w = vec.x; tris->vert1.w = vec.y; tris->vert2.w = vec.z;
}

float3 GetCenteroid(const global Tri* tris) {
	return (float3)(tris->vert0.w, tris->vert1.w, tris->vert2.w);
}

void SwapTri(global Tri* a, global Tri* b)
{
	Tri temp;
    temp.vert0 = a->vert0;
    temp.vert1 = a->vert1;
    temp.vert2 = a->vert2;
    *a = *b;
    *b = temp;
}

float IntersectAABB(float3 origin, float3 invDir, float3 aabbMin, float3 aabbMax, float minSoFar)
{
	float3 tmin = (aabbMin - origin) * invDir;
	float3 tmax = (aabbMax - origin) * invDir;
	float tnear = Max3(min(tmin, tmax));
	float tfar  = Min3(max(tmin, tmax));
	return tnear < tfar && tnear > 0.0f && tnear < minSoFar;
}

bool IntersectTri(Ray* ray, const global Tri* tri, int i)
{
	float3 edge1 = tri->vert1.xyz - tri->vert0.xyz;
	float3 edge2 = tri->vert2.xyz - tri->vert0.xyz;
	float3 h = cross( ray->direction, edge2 );
	float a = dot( edge1, h );
	if (fabs( a ) < 0.00001f) return false; // ray parallel to triangle
	float f = 1 / a;
	float3 s = ray->origin - tri->vert0.xyz;
	float u = f * dot( s, h );
	if (u < 0 | u > 1) return false;
	const float3 q = cross( s, edge1 );
	const float v = f * dot( ray->direction, q );
	if (v < 0 | u + v > 1) return false;
	const float t = f * dot( edge2, q );
	if (t > 0.0001f && t < ray->t)
	{
		ray->t = t, ray->u = u,
		ray->v = v, ray->instanceIndex = i;
		return true;
	}
	return false;
}

int IntersectBVH(Ray* ray, const global BVHNode* nodes, const global Tri* tris)
{
	int nodesToVisit[32];
	int currentNodeIndex = 1;
	float3 invDir = (float3)(1.0f) / ray->direction;
	int protection = 0, print1 = 0;
	bool intersected = 0;

	for (int i = GetTriStart(nodes), end = i + GetTriCount(nodes); i < end; ++i)
	{
		intersected |= IntersectTri(&ray, tris + i, i);
	}

	if (intersected) printf("triangle");
	
	return intersected;

	while (currentNodeIndex > 0 && protection++ < 900)
	{
		const global BVHNode* node = &nodes[--currentNodeIndex];
		
		if (!IntersectAABB(ray->origin, invDir, node->min.xyz, node->max.xyz, ray->t)) continue;

		if (GetTriCount(node) > 0) // is leaf 
		{
			for (int i = GetTriStart(node), end = i + GetTriCount(node); i < end; ++i)
			{
				if(IntersectTri(ray, tris + i, i))
				{
					if (!print1++) printf("leaf");
				}
			}
		}
		else {
			uint leftNode = GetLeftNode(node);
			nodesToVisit[currentNodeIndex++] = leftNode;
			nodesToVisit[currentNodeIndex++] = leftNode + 1;
		}
	}
	return ray->instanceIndex;
}

void UpdateNodeBounds(global BVHNode* node, const global Tri* tris)
{
	node->min.xyz = (float3)(Infinite);
	node->max.xyz = (float3)(NegInfinite);
	int i = GetTriStart(node), end = i + GetTriCount(node);
	while (i < end)
	{
		const Tri leafTri = tris[i++];
        node->min.xyz = min(node->min.xyz, leafTri.vert0.xyz);
        node->min.xyz = min(node->min.xyz, leafTri.vert1.xyz);
        node->min.xyz = min(node->min.xyz, leafTri.vert2.xyz);
        
		node->max.xyz = max(node->max.xyz, leafTri.vert0.xyz);
		node->max.xyz = max(node->max.xyz, leafTri.vert1.xyz);
		node->max.xyz = max(node->max.xyz, leafTri.vert2.xyz);
	}
}

void Subdivide(global BVHNode* bvhNodes, global Tri* tris)
{
	uchar currNodeIdx = 0;
	int nodeStack[50] = {0};
	int nodesUsed = 1, numStack = 1; 
	// recurse
	while(numStack > 0)
	{
		global BVHNode* node = bvhNodes + nodeStack[--numStack];
		float3 extent = node->max.xyz - node->min.xyz;
		int axis = 0;
		// find longest axis
		if (extent.y > extent.x) axis = 1;
		if (extent.z > extent[axis]) axis = 2;
		float splitPos = node->min[axis] + extent[axis] * 0.5f;
		// divide 
		int i = GetTriStart(node);
		int j = i + GetTriCount(node) - 1;
		
		while(i <= j)
		{
			if (GetCenteroid(tris + i)[axis] < splitPos) i++;
			else SwapTri(tris + i, &tris[j--]);
		}
		int leftCount = i - GetTriStart(node);

		if (leftCount == 0 || leftCount == GetTriCount(node)) continue;
	
		// create child nodes
		int leftChildIdx  = nodeStack[numStack++] = nodesUsed++;
		int rightChildIdx = nodeStack[numStack++] = nodesUsed++;
		
		SetLeftNode(node, leftChildIdx);	
		SetTriStartTriCount(bvhNodes + leftChildIdx, GetTriStart(node), leftCount);
		SetTriStartTriCount(bvhNodes + rightChildIdx, i, GetTriCount(node) - leftCount);
		SetTriCount0(node);

		UpdateNodeBounds(bvhNodes + leftChildIdx , tris);
		UpdateNodeBounds(bvhNodes + rightChildIdx, tris);
	}
	printf("bvh nodes used: %d", nodesUsed*2);
}

kernel void BuildBVH(global Tri* tris, global BVHNode* bvhNodes, global Mesh* meshes, int numMeshes)
{
    int numTris = 0;
    for (int i = 0; i < numMeshes; ++i) numTris += meshes[i].numTris;

	for (int i = 0; i < numTris; ++i) {
		float3 centeroid = (tris[i].vert0.xyz + tris[i].vert1.xyz + tris[i].vert2.xyz) * 0.3333f; 
		SetCenteroid(tris + i, centeroid); 
	}
	SetTriStartTriCount(bvhNodes, 0, numTris);
	bvhNodes->max.w = as_float(0); // left node
	UpdateNodeBounds(bvhNodes, tris);
	Subdivide(bvhNodes, tris);
}