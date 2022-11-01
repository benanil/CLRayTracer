// software as is bla bla license bla bla gev gev gev
// Anilcan Gulkaya 10/03/2022
// #include "MathAndSTL.cl"

// todo seperate 

// ---- STRUCTURES ----

typedef struct _TraceArgs{
	float cameraPos[3];
	float time;
	uint numSpheres, numMeshes, padd;
} TraceArgs;

typedef struct _RayHit {
	float3 position;
	float  distance;
	int    index;
} RayHit;

typedef struct _Sphere {
	float position[3]; 
	float radius;
	float roughness;
	uint  color; // w texture index
	float rotationx, rotationy;
} Sphere;

typedef struct _HitRecord {
	float3 color, normal, point;
} HitRecord;

typedef struct _Material {
	uint color; 
	uint specularColor;
	ushort albedoTextureIndex;
	ushort specularTextureIndex;  
	ushort shininess, roughness;
} Material;

typedef struct _Triangle {
	float3 x, y, z; // triangle positions
	short uv0[2];
	short uv1[2];
	short uv2[2];
	ushort materialIndex;
	short normal0[3];
	short normal1[3];
	short normal2[3];
} Triangle;

typedef struct _Triout {
	float t, u, v; uint triIndex;
} Triout;

typedef struct _MeshInstance { 
	float position[3]; // todo add rotation and scale aka matrix
	ushort meshIndex, materialStart; 
} MeshInstance;

typedef struct _BVHNode {
	float4 min, max; 
} BVHNode;

// ---- CONSTRUCTORS ----

RayHit CreateRayHit() {
	RayHit hit; hit.distance = Infinite; hit.index = 0;
	return hit;
}

HitRecord CreateHitRecord() {
	HitRecord record;
	record.color  = (float3)(0.90f, 0.90f, 0.90f); // plane color
	record.normal = (float3)(0.0f, 1.0f, 0.0f);
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
	if (t > 0 && t < besthit->distance) 
	{
		float3 point = ray.origin + (ray.direction * t);
		if (fabs(point.x) < 80.0f && fabs(point.z) < 80.0f)
			besthit->distance = t;
		return true;
	}
	return false;
}

bool IntersectTriangle(Ray ray, const global Triangle* tri, Triout* o, int i)
{
	const float3 edge1 = tri->y - tri->x;
	const float3 edge2 = tri->z - tri->x;
	const float3 h = cross( ray.direction, edge2 );
	const float a = dot( edge1, h );
	// if (fabs(a) < 0.0001f) return false; // ray parallel to triangle
	const float f = 1.0f / a;
	const float3 s = ray.origin - tri->x.xyz;
	const float u = f * dot( s, h );
	if (u < 0.0f || u > 1.0f) return false;
	const float3 q = cross( s, edge1 );
	const float v = f * dot( ray.direction, q );
	if (v < 0.0f || u + v > 1.0f) return false;
	const float t = f * dot( edge2, q );
	if (t > 0.0001f && t < o->t) {
		o->u = u;  o->v = v; o->t = t;
		o->triIndex = i;
		return true;
	}
	return false;
}

float IntersectAABB(float3 origin, float3 invDir, float3 aabbMin, float3 aabbMax, float minSoFar)
{
	float3 tmin = (aabbMin - origin) * invDir;
	float3 tmax = (aabbMax - origin) * invDir;
	float tnear = Max3(fmin(tmin, tmax));
	float tfar  = Min3(fmax(tmin, tmax));
	// return tnear < tfar && tnear > 0.0f && tnear < minSoFar;
	if (tnear < tfar && tnear > 0.0f && tnear < minSoFar)
		return tnear; else return 1e30f;
}

#define SWAPF(x, y) float tf = x; x = y, y = tf;
#define SWAPUINT(x, y) uint tu = x; x = y, y = tu;
#define GetLeftFirst(nod) (as_uint((nod)->min.w))
#define GetTriCount(nod)  (as_uint((nod)->max.w))

int IntersectBVH(Ray ray, const global BVHNode* nodes, uint rootNode, const global Triangle* tris, Triout* out)
{
	int nodesToVisit[32] = { rootNode };
	int currentNodeIndex = 1;
	float3 invDir = (float3)(1.0f) / ray.direction;
	int intersection = 0, protection = 0;
	
	while (currentNodeIndex > 0 && protection++ < 250)
	{	
		const global BVHNode* node = nodes + nodesToVisit[--currentNodeIndex];
		traverse:
		if (GetTriCount(node) > 0) // is leaf 
		{
			for (int i = GetLeftFirst(node), end = i + GetTriCount(node); i < end; ++i)
				intersection |= IntersectTriangle(ray, tris + i, out, i);
			continue;
		}
	    
		uint leftIndex  = GetLeftFirst(node);
		uint rightIndex = leftIndex + 1;
		BVHNode leftNode  = nodes[leftIndex];
		BVHNode rightNode = nodes[rightIndex];
		
		float dist1 = IntersectAABB(ray.origin, invDir, leftNode.min.xyz , leftNode.max.xyz , out->t);
		float dist2 = IntersectAABB(ray.origin, invDir, rightNode.min.xyz, rightNode.max.xyz, out->t);
	    
		if (dist1 > dist2) { SWAPF(dist1, dist2); SWAPUINT(leftIndex, rightIndex); }
		
		if (dist1 == 1e30f) continue;
		else {
			node = nodes + leftIndex;
			if (dist2 != 1e30f) nodesToVisit[currentNodeIndex++] = rightIndex;
			goto traverse;
		}
	}
	return intersection;
}

// ---- KERNELS ----

kernel void Trace(
	write_only image2d_t screen,
	global const Texture* textures,
	global const RGB8* texturePixels,
	global const uint* bvhIndices,
	global const Triangle* triangles,
	global const float* rays, 
	global const Sphere* spheres,
	TraceArgs trace_args,
	global const BVHNode* nodes,
	global const Material* materials,
	global const MeshInstance* meshInstances
) 
{
	const int i = get_global_id(0), j = get_global_id(1);
	
	// assert
	if (sizeof(Triangle) != 64 + (16)) {
		return write_imagef(screen, (int2)(i, j), (float4)(0.5f, 0.0f, 0.0f, 1.0f));
	}

	Ray ray = CreateRay(vload3(0, trace_args.cameraPos), vload3(mad24(j, get_image_width(screen), i), rays));
	
	float3 lightDir = (float3)(0.0f, sin(331.01), cos(331.01)); // sun dir
	float3 result = (float3)(0.0f, 0.0f, 0.0f);
	float3 energy = (float3)(1.0f, 1.0f, 1.0f);
	float3 atmosphericLight = (float3)(0.30f, 0.25f, 0.35f) * 0.2f;

	for (int j = 0; j < 3; ++j)// num bounces
	{
		RayHit besthit = CreateRayHit();
		HitRecord record = CreateHitRecord();
		float roughness = 0.75f; // plane roughness
		float shininess = 20.0f;
		float3 specularColor = (float3)(0.8f, 0.7f, 0.6f);
		// find intersectedsphere
		for (int i = 0; i < trace_args.numSpheres; ++i)  {
			IntersectSphere(vload3(0, spheres[i].position), spheres[i].radius, ray, &besthit, i);
		}
		
		if (IntersectPlane(ray, &besthit)) {
			record.point = ray.origin + ray.direction * besthit.distance;
			record.color = (float3)(70.0f / 255.0f, 70.0f/ 255.0f, 70.0f/ 255.0f);
		}
		else {
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

		Ray meshRay;
		meshRay.direction = ray.direction;

		for (int i = 0; i < trace_args.numMeshes; ++i)
		{
			Triout triout;
			triout.t = besthit.distance;
			triout.triIndex = 0;
			MeshInstance instance = meshInstances[i];
			// change ray position instead of mesh position for capturing in different positions
			meshRay.origin = ray.origin - vload3(0, instance.position);
			// instance.meshIndex = bvhIndex
			if (IntersectBVH(meshRay, nodes, bvhIndices[instance.meshIndex], triangles, &triout)) 
			{
				Triangle triangle = triangles[triout.triIndex];
				Material material = materials[instance.materialStart + triangle.materialIndex];
				float3 baryCentrics = (float3)(1.0f - triout.u - triout.v, triout.u, triout.v);

				record.point = meshRay.origin + triout.t * meshRay.direction;
				record.normal = convert_float3(vload3(0, triangle.normal0)) / 32766.0f * baryCentrics.x // fast_normalize(cross(triangle.y - triangle.z, triangle.z - triangle.x));
					          + convert_float3(vload3(0, triangle.normal1)) / 32766.0f * baryCentrics.y
					          + convert_float3(vload3(0, triangle.normal2)) / 32766.0f * baryCentrics.z;

				float2 uv = convert_float2(vload2(0, triangle.uv0)) / 32766.0f * baryCentrics.x 
					      + convert_float2(vload2(0, triangle.uv1)) / 32766.0f * baryCentrics.y
					      + convert_float2(vload2(0, triangle.uv2)) / 32766.0f * baryCentrics.z;
				
				RGB8 pixel = texturePixels[SampleTexture(textures[material.albedoTextureIndex], uv)];
				RGB8 specularPixel = texturePixels[SampleTexture(textures[material.specularTextureIndex], uv)];
				// uint pixel = WangHash(triangle.materialIndex * (triangle.materialIndex << 8) + (triangle.materialIndex << 16));

				record.color = UnpackRGB8u(material.color) * UNPACK_RGB8(pixel);
				specularColor = UNPACK_RGB8(specularPixel) * UnpackRGB8u(material.specularColor) * 1.1f;
				besthit.distance = triout.t;	
				roughness = material.roughness / 65000.0f;
				shininess = material.shininess / 65000.0f * 100.0f;
			}
		}	
		
		if (besthit.distance > InfMinusOne) {
			RGB8 pixel = texturePixels[SampleSkyboxPixel(ray.direction, textures[2])];
			float3 skybox_sample = UNPACK_RGB8(pixel);			
			result += skybox_sample * energy;
			break;
		}
	
		ray.origin = record.point;
		ray.origin += record.normal * 0.001f;
		ray.direction = reflect(ray.direction, record.normal); // wo = ray.direction now = outgoing ray direction
	
		// check Shadow
		// Ray shadowRay = CreateRay(ray.origin, -lightDir);
		float shadow = 1.0f; 
		// todo shadow for only directional light
	
		// Shade
		float ndl = fmax(dot(record.normal, -lightDir), 0.0f);
		float3 specular = (float3)((1.0f - roughness) * ndl * shadow) * specularColor; 
		float3 specularLighting = ndl * pow(fmax(dot(reflect(-lightDir, record.normal), meshRay.direction), 0.0f), shininess)*0.5f; // meshray direction = wi
		float3 ambient = (1.0f - ndl) * atmosphericLight;

		result += energy * (record.color * ndl) + ambient + specularLighting;
		energy *= specular;
		atmosphericLight *= 0.4f;
		
		lightDir = ray.direction;
	}
	result = Saturation(result, 1.2f);
	result = ACESFilm(result);

	write_imagef(screen, (int2)(i, j), (float4)(pow(result, oneDivGamma), 1.0f));
}

kernel void RayGen(int width, int height, global float* rays, Matrix4 inverseView, Matrix4 inverseProjection)
{
	const int i = get_global_id(0), j = get_global_id(1);
	float2 coord = (float2)((float)i / (float)width, (float)j / (float)height);
	coord = coord * 2.0f - 1.0f;
	float4 target = MatMul(inverseProjection, (float4)(coord, 1.0f, 1.0f));
	target /= target.w;
	float3 rayDir = normalize(MatMul(inverseView, target).xyz);
	vstore3(rayDir, i + j * width, rays);
}