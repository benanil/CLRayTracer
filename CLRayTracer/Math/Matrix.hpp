#pragma once
#include "Quaternion.hpp"

struct Matrix3
{
	union
	{
		float m[3][3];
		Vector3f vec[3];
	};

	Matrix3() {}

	inline static Matrix3 LookAt(Vector3f direction, Vector3f up)
	{
		Matrix3 result{};
		result.vec[2] = direction;
		Vector3f const& Right = Vector3f::Cross(up, result.vec[2]);
		result.vec[0] = Right * rsqrt(Max(0.00001f, Vector3f::Dot(Right, Right)));
		result.vec[1] = Vector3f::Cross(result.vec[2], result.vec[0]);
		return result;
	}

	Quaternion ToQuaternion() const
	{
		float fourXSquaredMinus1 = m[0][0] - m[1][1] - m[2][2];
		float fourYSquaredMinus1 = m[1][1] - m[0][0] - m[2][2];
		float fourZSquaredMinus1 = m[2][2] - m[0][0] - m[1][1];
		float fourWSquaredMinus1 = m[0][0] + m[1][1] + m[2][2];

		int biggestIndex = 0;
		float fourBiggestSquaredMinus1 = fourWSquaredMinus1;
		if(fourXSquaredMinus1 > fourBiggestSquaredMinus1)
		{
			fourBiggestSquaredMinus1 = fourXSquaredMinus1;
			biggestIndex = 1;
		}
		if(fourYSquaredMinus1 > fourBiggestSquaredMinus1)
		{
			fourBiggestSquaredMinus1 = fourYSquaredMinus1;
			biggestIndex = 2;
		}
		if(fourZSquaredMinus1 > fourBiggestSquaredMinus1)
		{
			fourBiggestSquaredMinus1 = fourZSquaredMinus1;
			biggestIndex = 3;
		}

		float biggestVal = sqrt(fourBiggestSquaredMinus1 + 1.0f) * 0.5f;
		float mult = 0.25f / biggestVal;
		
		switch(biggestIndex)
		{
			case 0:
				return Quaternion(biggestVal, (m[1][2] - m[2][1]) * mult, (m[2][0] - m[0][2]) * mult, (m[0][1] - m[1][0]) * mult);
			case 1:
				return Quaternion((m[1][2] - m[2][1]) * mult, biggestVal, (m[0][1] + m[1][0]) * mult, (m[2][0] + m[0][2]) * mult);
			case 2:
				return Quaternion((m[2][0] - m[0][2]) * mult, (m[0][1] + m[1][0]) * mult, biggestVal, (m[1][2] + m[2][1]) * mult);
			case 3:
				return Quaternion((m[0][1] - m[1][0]) * mult, (m[2][0] + m[0][2]) * mult, (m[1][2] + m[2][1]) * mult, biggestVal);
		}
	}
};

struct Matrix4
{
	union
	{
		struct { __m128 r[4]; };
		struct { float m[4][4]; };
	};

	Matrix4()
	{
		r[0] = g_XMIdentityR0;
		r[1] = g_XMIdentityR1;
		r[2] = g_XMIdentityR2;
		r[3] = g_XMIdentityR3;
	}

	explicit Matrix4(float s) 
	{
		for (int i = 0; i < 16; ++i) m[0][i] = s;
	}

	Matrix4(__m128 x, __m128 y, __m128 z, __m128 w)
	{
		r[0] = x; r[1] = y; r[2] = z; r[3] = w;
	}

	const __m128& operator [] (int index) const { return r[index]; }
	__m128& operator [] (int index) { return r[index]; }
	
	Vector4 VECTORCALL  operator *  (const Vector3f v)  noexcept { return Vector3Transform(v, *this); };
	Vector4 VECTORCALL  operator *  (const Vector4 v)   noexcept { return Vector4Transform(v, *this); };

	Matrix4 VECTORCALL  operator *  (const Matrix4 M)  noexcept { return Matrix4::Multiply(*this, M); };
	Matrix4& VECTORCALL operator *= (const Matrix4 M)  noexcept { *this = Matrix4::Multiply(*this, M); return *this; };

	FINLINE static Matrix4 Identity()
	{
		Matrix4 M;
		M.r[0] = g_XMIdentityR0;
		M.r[1] = g_XMIdentityR1;
		M.r[2] = g_XMIdentityR2;
		M.r[3] = g_XMIdentityR3;
		return M;
	}

	FINLINE static Matrix4 FromPosition(const float x, const float y, const float z)
	{
		Matrix4 M;
		M.r[0] = g_XMIdentityR0;
		M.r[1] = g_XMIdentityR1;
		M.r[2] = g_XMIdentityR2;
		M.r[3] = _mm_set_ps(1.0f, z, y, x);
		return M;
	}

	FINLINE static Matrix4 FromPosition(const Vector3f& vec3)
	{
		return FromPosition(vec3.x, vec3.y, vec3.z);
	}

	FINLINE static Matrix4 CreateScale(const float ScaleX, const float ScaleY, const float ScaleZ)
	{
		Matrix4 M;
		M.r[0] = _mm_set_ps(0, 0, 0, ScaleX);
		M.r[1] = _mm_set_ps(0, 0, ScaleY, 0);
		M.r[2] = _mm_set_ps(0, ScaleZ, 0, 0);
		M.r[3] = g_XMIdentityR3;
		return M;
	}

	FINLINE static Matrix4 CreateScale(const Vector3f& vec3)
	{
		return CreateScale(vec3.x, vec3.y, vec3.z);
	}

	// please assign normalized vectors
	FINLINE static Matrix4 VECTORCALL LookAtRH(Vector3f eye, Vector3f center, const Vector3f& up)
	{
		Vector4 NegEyePosition;
		Vector4 D0, D1, D2;
		Vector4 R0, R1;

		Vector4 EyePosition  = _mm_loadu_ps(&eye.x);
		Vector4 EyeDirection = _mm_sub_ps(_mm_setzero_ps(), _mm_loadu_ps(&center.x));
		Vector4 UpDirection  = _mm_loadu_ps(&up.x);

		R0 = SSEVector3Cross(UpDirection, EyeDirection);
		R1 = SSEVector3Cross(EyeDirection, R0);

		NegEyePosition = _mm_sub_ps(_mm_setzero_ps(), EyePosition);

		D0 = SSEVector3Dot(R0, NegEyePosition);
		D1 = SSEVector3Dot(R1, NegEyePosition);
		D2 = SSEVector3Dot(EyeDirection, NegEyePosition);
		Matrix4 M;
		M.r[0] = SSESelect(D0, R0, g_XMSelect1110);
		M.r[1] = SSESelect(D1, R1, g_XMSelect1110);
		M.r[2] = SSESelect(D2, EyeDirection, g_XMSelect1110);
		M.r[3] = g_XMIdentityR3;
		return Matrix4::Transpose(M);
	}

	FINLINE static Matrix4 VECTORCALL FromQuaternion(const Quaternion quaternion)
	{
		static const Vector4 Constant1110 = { { { 1.0f, 1.0f, 1.0f, 0.0f } } };

		__m128  Q0 = _mm_add_ps(quaternion.vec, quaternion.vec);
		__m128  Q1 = _mm_mul_ps(quaternion.vec, Q0);

		__m128  V0 = _mm_permute_ps(Q1, _MM_SHUFFLE(3, 0, 0, 1));
		V0 = _mm_and_ps(V0, g_XMMask3);
		__m128  V1 = _mm_permute_ps(Q1, _MM_SHUFFLE(3, 1, 2, 2));
		V1 = _mm_and_ps(V1, g_XMMask3);
		__m128  R0 = _mm_sub_ps(Constant1110.vec, V0);
		R0 = _mm_sub_ps(R0, V1);

		V0 = _mm_permute_ps(quaternion.vec, _MM_SHUFFLE(3, 1, 0, 0));
		V1 = _mm_permute_ps(Q0, _MM_SHUFFLE(3, 2, 1, 2));
		V0 = _mm_mul_ps(V0, V1);

		V1 = _mm_permute_ps(quaternion.vec, _MM_SHUFFLE(3, 3, 3, 3));
		__m128  V2 = _mm_permute_ps(Q0, _MM_SHUFFLE(3, 0, 2, 1));
		V1 = _mm_mul_ps(V1, V2);

		__m128  R1 = _mm_add_ps(V0, V1);
		__m128  R2 = _mm_sub_ps(V0, V1);

		V0 = _mm_shuffle_ps(R1, R2, _MM_SHUFFLE(1, 0, 2, 1));
		V0 = _mm_permute_ps(V0, _MM_SHUFFLE(1, 3, 2, 0));
		V1 = _mm_shuffle_ps(R1, R2, _MM_SHUFFLE(2, 2, 0, 0));
		V1 = _mm_permute_ps(V1, _MM_SHUFFLE(2, 0, 2, 0));

		Q1 = _mm_shuffle_ps(R0, V0, _MM_SHUFFLE(1, 0, 3, 0));
		Q1 = _mm_permute_ps(Q1, _MM_SHUFFLE(1, 3, 2, 0));

		Matrix4 M;
		M.r[0] = Q1;
		Q1 = _mm_shuffle_ps(R0, V0, _MM_SHUFFLE(3, 2, 3, 1));
		Q1 = _mm_permute_ps(Q1, _MM_SHUFFLE(1, 3, 0, 2));
		M.r[1] = Q1;
		Q1 = _mm_shuffle_ps(V1, R0, _MM_SHUFFLE(3, 2, 1, 0));
		M.r[2] = Q1;
		M.r[3] = g_XMIdentityR3;
		return M;
	}

	FINLINE static Matrix4 VECTORCALL  OrthographicOffCenterLH(float ViewLeft, float ViewRight, float ViewBottom, float ViewTop, float NearZ, float FarZ)
	{
		Matrix4 M;
		float fReciprocalWidth = 1.0f / (ViewRight - ViewLeft);
		float fReciprocalHeight = 1.0f / (ViewTop - ViewBottom);
		float fRange = 1.0f / (FarZ - NearZ);
		// Note: This is recorded on the stack
		__m128 rMem = {
			fReciprocalWidth,
			fReciprocalHeight,
			fRange,
			1.0f
		};
		__m128 rMem2 = {
			-(ViewLeft + ViewRight),
			-(ViewTop + ViewBottom),
			-NearZ,
			1.0f
		};
		// Copy from memory to SSE register
		__m128 vValues = rMem;
		__m128 vTemp = _mm_setzero_ps();
		// Copy x only
		vTemp = _mm_move_ss(vTemp, vValues);
		// fReciprocalWidth*2,0,0,0
		vTemp = _mm_add_ss(vTemp, vTemp);
		M.r[0] = vTemp;
		// 0,fReciprocalHeight*2,0,0
		vTemp = vValues;
		vTemp = _mm_and_ps(vTemp, g_XMMaskY);
		vTemp = _mm_add_ps(vTemp, vTemp);
		M.r[1] = vTemp;
		// 0,0,fRange,0.0f
		vTemp = vValues;
		vTemp = _mm_and_ps(vTemp, g_XMMaskZ);
		M.r[2] = vTemp;
		// -(ViewLeft + ViewRight)*fReciprocalWidth,-(ViewTop + ViewBottom)*fReciprocalHeight,fRange*-NearZ,1.0f
		vValues = _mm_mul_ps(vValues, rMem2);
		M.r[3] = vValues;
		return M;
	}


	FINLINE static Matrix4 VECTORCALL PerspectiveFovRH(float fov, float width, float height, float zNear, float zFar)
	{
		const float rad = fov;
		const float h = cosf(0.5f * rad) / sinf(0.5f * rad);
		const float w = h * height / width; ///todo max(width , Height) / min(width , Height)?
		Matrix4 M(0.0f);
		M.m[0][0] = w;
		M.m[1][1] = h;
		M.m[2][2] = -(zFar + zNear) / (zFar - zNear);
		M.m[2][3] = -1.0f;
		M.m[3][2] = -(2.0f * zFar * zNear) / (zFar - zNear);
		return M;
	}

	FINLINE static Matrix4 VECTORCALL Transpose(const Matrix4 M)
	{
		const __m128 vTemp1 = _mm_shuffle_ps(M.r[0], M.r[1], _MM_SHUFFLE(1, 0, 1, 0));
		const __m128 vTemp3 = _mm_shuffle_ps(M.r[0], M.r[1], _MM_SHUFFLE(3, 2, 3, 2));
		const __m128 vTemp2 = _mm_shuffle_ps(M.r[2], M.r[3], _MM_SHUFFLE(1, 0, 1, 0));
		const __m128 vTemp4 = _mm_shuffle_ps(M.r[2], M.r[3], _MM_SHUFFLE(3, 2, 3, 2));
		Matrix4 mResult;
		mResult.r[0] = _mm_shuffle_ps(vTemp1, vTemp2, _MM_SHUFFLE(2, 0, 2, 0));
		mResult.r[1] = _mm_shuffle_ps(vTemp1, vTemp2, _MM_SHUFFLE(3, 1, 3, 1));
		mResult.r[2] = _mm_shuffle_ps(vTemp3, vTemp4, _MM_SHUFFLE(2, 0, 2, 0));
		mResult.r[3] = _mm_shuffle_ps(vTemp3, vTemp4, _MM_SHUFFLE(3, 1, 3, 1));
		return mResult;
	}

	// from directx math:  xnamathmatrix.inl
	inline Matrix4 static VECTORCALL Inverse(const Matrix4 in) noexcept
	{
		__m128 Fac0;
		{
			__m128 Swp0a = _mm_shuffle_ps(in[3], in[2], _MM_SHUFFLE(3, 3, 3, 3));
			__m128 Swp0b = _mm_shuffle_ps(in[3], in[2], _MM_SHUFFLE(2, 2, 2, 2));
		
			__m128 Swp00 = _mm_shuffle_ps(in[2], in[1], _MM_SHUFFLE(2, 2, 2, 2));
			__m128 Swp01 = _mm_shuffle_ps(Swp0a, Swp0a, _MM_SHUFFLE(2, 0, 0, 0));
			__m128 Swp02 = _mm_shuffle_ps(Swp0b, Swp0b, _MM_SHUFFLE(2, 0, 0, 0));
			__m128 Swp03 = _mm_shuffle_ps(in[2], in[1], _MM_SHUFFLE(3, 3, 3, 3));
		
			__m128 Mul00 = _mm_mul_ps(Swp00, Swp01);
			__m128 Mul01 = _mm_mul_ps(Swp02, Swp03);
			Fac0 = _mm_sub_ps(Mul00, Mul01);
		}
		
		__m128 Fac1;
		{
			__m128 Swp0a = _mm_shuffle_ps(in[3], in[2], _MM_SHUFFLE(3, 3, 3, 3));
			__m128 Swp0b = _mm_shuffle_ps(in[3], in[2], _MM_SHUFFLE(1, 1, 1, 1));
		
			__m128 Swp00 = _mm_shuffle_ps(in[2], in[1], _MM_SHUFFLE(1, 1, 1, 1));
			__m128 Swp01 = _mm_shuffle_ps(Swp0a, Swp0a, _MM_SHUFFLE(2, 0, 0, 0));
			__m128 Swp02 = _mm_shuffle_ps(Swp0b, Swp0b, _MM_SHUFFLE(2, 0, 0, 0));
			__m128 Swp03 = _mm_shuffle_ps(in[2], in[1], _MM_SHUFFLE(3, 3, 3, 3));
		
			__m128 Mul00 = _mm_mul_ps(Swp00, Swp01);
			__m128 Mul01 = _mm_mul_ps(Swp02, Swp03);
			Fac1 = _mm_sub_ps(Mul00, Mul01);
		}
		
		__m128 Fac2;
		{
			__m128 Swp0a = _mm_shuffle_ps(in[3], in[2], _MM_SHUFFLE(2, 2, 2, 2));
			__m128 Swp0b = _mm_shuffle_ps(in[3], in[2], _MM_SHUFFLE(1, 1, 1, 1));
		
			__m128 Swp00 = _mm_shuffle_ps(in[2], in[1], _MM_SHUFFLE(1, 1, 1, 1));
			__m128 Swp01 = _mm_shuffle_ps(Swp0a, Swp0a, _MM_SHUFFLE(2, 0, 0, 0));
			__m128 Swp02 = _mm_shuffle_ps(Swp0b, Swp0b, _MM_SHUFFLE(2, 0, 0, 0));
			__m128 Swp03 = _mm_shuffle_ps(in[2], in[1], _MM_SHUFFLE(2, 2, 2, 2));
		
			__m128 Mul00 = _mm_mul_ps(Swp00, Swp01);
			__m128 Mul01 = _mm_mul_ps(Swp02, Swp03);
			Fac2 = _mm_sub_ps(Mul00, Mul01);
		}
		
		__m128 Fac3;
		{
			__m128 Swp0a = _mm_shuffle_ps(in[3], in[2], _MM_SHUFFLE(3, 3, 3, 3));
			__m128 Swp0b = _mm_shuffle_ps(in[3], in[2], _MM_SHUFFLE(0, 0, 0, 0));
		
			__m128 Swp00 = _mm_shuffle_ps(in[2], in[1], _MM_SHUFFLE(0, 0, 0, 0));
			__m128 Swp01 = _mm_shuffle_ps(Swp0a, Swp0a, _MM_SHUFFLE(2, 0, 0, 0));
			__m128 Swp02 = _mm_shuffle_ps(Swp0b, Swp0b, _MM_SHUFFLE(2, 0, 0, 0));
			__m128 Swp03 = _mm_shuffle_ps(in[2], in[1], _MM_SHUFFLE(3, 3, 3, 3));
		
			__m128 Mul00 = _mm_mul_ps(Swp00, Swp01);
			__m128 Mul01 = _mm_mul_ps(Swp02, Swp03);
			Fac3 = _mm_sub_ps(Mul00, Mul01);
		}
		
		__m128 Fac4;
		{
			__m128 Swp0a = _mm_shuffle_ps(in[3], in[2], _MM_SHUFFLE(2, 2, 2, 2));
			__m128 Swp0b = _mm_shuffle_ps(in[3], in[2], _MM_SHUFFLE(0, 0, 0, 0));
		
			__m128 Swp00 = _mm_shuffle_ps(in[2], in[1], _MM_SHUFFLE(0, 0, 0, 0));
			__m128 Swp01 = _mm_shuffle_ps(Swp0a, Swp0a, _MM_SHUFFLE(2, 0, 0, 0));
			__m128 Swp02 = _mm_shuffle_ps(Swp0b, Swp0b, _MM_SHUFFLE(2, 0, 0, 0));
			__m128 Swp03 = _mm_shuffle_ps(in[2], in[1], _MM_SHUFFLE(2, 2, 2, 2));
		
			__m128 Mul00 = _mm_mul_ps(Swp00, Swp01);
			__m128 Mul01 = _mm_mul_ps(Swp02, Swp03);
			Fac4 = _mm_sub_ps(Mul00, Mul01);
		}
		
		__m128 Fac5;
		{
			__m128 Swp0a = _mm_shuffle_ps(in[3], in[2], _MM_SHUFFLE(1, 1, 1, 1));
			__m128 Swp0b = _mm_shuffle_ps(in[3], in[2], _MM_SHUFFLE(0, 0, 0, 0));
		
			__m128 Swp00 = _mm_shuffle_ps(in[2], in[1], _MM_SHUFFLE(0, 0, 0, 0));
			__m128 Swp01 = _mm_shuffle_ps(Swp0a, Swp0a, _MM_SHUFFLE(2, 0, 0, 0));
			__m128 Swp02 = _mm_shuffle_ps(Swp0b, Swp0b, _MM_SHUFFLE(2, 0, 0, 0));
			__m128 Swp03 = _mm_shuffle_ps(in[2], in[1], _MM_SHUFFLE(1, 1, 1, 1));
		
			__m128 Mul00 = _mm_mul_ps(Swp00, Swp01);
			__m128 Mul01 = _mm_mul_ps(Swp02, Swp03);
			Fac5 = _mm_sub_ps(Mul00, Mul01);
		}
		
		__m128 SignA = _mm_set_ps(1.0f, -1.0f, 1.0f, -1.0f);
		__m128 SignB = _mm_set_ps(-1.0f, 1.0f, -1.0f, 1.0f);
		
		__m128 Temp0 = _mm_shuffle_ps(in[1], in[0], _MM_SHUFFLE(0, 0, 0, 0));
		__m128 Vec0 = _mm_shuffle_ps(Temp0, Temp0, _MM_SHUFFLE(2, 2, 2, 0));
		
		__m128 Temp1 = _mm_shuffle_ps(in[1], in[0], _MM_SHUFFLE(1, 1, 1, 1));
		__m128 Vec1 = _mm_shuffle_ps(Temp1, Temp1, _MM_SHUFFLE(2, 2, 2, 0));
		
		__m128 Temp2 = _mm_shuffle_ps(in[1], in[0], _MM_SHUFFLE(2, 2, 2, 2));
		__m128 Vec2 = _mm_shuffle_ps(Temp2, Temp2, _MM_SHUFFLE(2, 2, 2, 0));
		
		__m128 Temp3 = _mm_shuffle_ps(in[1], in[0], _MM_SHUFFLE(3, 3, 3, 3));
		__m128 Vec3 = _mm_shuffle_ps(Temp3, Temp3, _MM_SHUFFLE(2, 2, 2, 0));
		
		__m128 Mul00 = _mm_mul_ps(Vec1, Fac0);
		__m128 Mul01 = _mm_mul_ps(Vec2, Fac1);
		__m128 Mul02 = _mm_mul_ps(Vec3, Fac2);
		__m128 Sub00 = _mm_sub_ps(Mul00, Mul01);
		__m128 Add00 = _mm_add_ps(Sub00, Mul02);
		__m128 Inv0 = _mm_mul_ps(SignB, Add00);
		
		__m128 Mul03 = _mm_mul_ps(Vec0, Fac0);
		__m128 Mul04 = _mm_mul_ps(Vec2, Fac3);
		__m128 Mul05 = _mm_mul_ps(Vec3, Fac4);
		__m128 Sub01 = _mm_sub_ps(Mul03, Mul04);
		__m128 Add01 = _mm_add_ps(Sub01, Mul05);
		__m128 Inv1 = _mm_mul_ps(SignA, Add01);
		
		__m128 Mul06 = _mm_mul_ps(Vec0, Fac1);
		__m128 Mul07 = _mm_mul_ps(Vec1, Fac3);
		__m128 Mul08 = _mm_mul_ps(Vec3, Fac5);
		__m128 Sub02 = _mm_sub_ps(Mul06, Mul07);
		__m128 Add02 = _mm_add_ps(Sub02, Mul08);
		__m128 Inv2 = _mm_mul_ps(SignB, Add02);
		
		__m128 Mul09 = _mm_mul_ps(Vec0, Fac2);
		__m128 Mul10 = _mm_mul_ps(Vec1, Fac4);
		__m128 Mul11 = _mm_mul_ps(Vec2, Fac5);
		__m128 Sub03 = _mm_sub_ps(Mul09, Mul10);
		__m128 Add03 = _mm_add_ps(Sub03, Mul11);
		__m128 Inv3 = _mm_mul_ps(SignA, Add03);
		
		__m128 Row0 = _mm_shuffle_ps(Inv0, Inv1, _MM_SHUFFLE(0, 0, 0, 0));
		__m128 Row1 = _mm_shuffle_ps(Inv2, Inv3, _MM_SHUFFLE(0, 0, 0, 0));
		__m128 Row2 = _mm_shuffle_ps(Row0, Row1, _MM_SHUFFLE(2, 0, 2, 0));
		
		__m128 Det0 = Vector4::Dot(in[0], Row2);
		__m128 Rcp0 = _mm_div_ps(_mm_set1_ps(1.0f), Det0);
		Matrix4 out;
		out[0] = _mm_mul_ps(Inv0, Rcp0);
		out[1] = _mm_mul_ps(Inv1, Rcp0);
		out[2] = _mm_mul_ps(Inv2, Rcp0);
		out[3] = _mm_mul_ps(Inv3, Rcp0);
		return out;
	}

	inline Matrix4 static VECTORCALL Multiply(const Matrix4 in1, const Matrix4 in2)
	{
		Matrix4 out;
		{
			__m128 e0 = _mm_shuffle_ps(in2[0], in2[0], _MM_SHUFFLE(0, 0, 0, 0));
			__m128 e1 = _mm_shuffle_ps(in2[0], in2[0], _MM_SHUFFLE(1, 1, 1, 1));
			__m128 e2 = _mm_shuffle_ps(in2[0], in2[0], _MM_SHUFFLE(2, 2, 2, 2));
			__m128 e3 = _mm_shuffle_ps(in2[0], in2[0], _MM_SHUFFLE(3, 3, 3, 3));

			__m128 m0 = _mm_mul_ps(in1[0], e0);
			__m128 m1 = _mm_mul_ps(in1[1], e1);
			__m128 m2 = _mm_mul_ps(in1[2], e2);
			__m128 m3 = _mm_mul_ps(in1[3], e3);

			__m128 a0 = _mm_add_ps(m0, m1);
			__m128 a1 = _mm_add_ps(m2, m3);
			__m128 a2 = _mm_add_ps(a0, a1);

			out[0] = a2;
		}
		{
			__m128 e0 = _mm_shuffle_ps(in2[1], in2[1], _MM_SHUFFLE(0, 0, 0, 0));
			__m128 e1 = _mm_shuffle_ps(in2[1], in2[1], _MM_SHUFFLE(1, 1, 1, 1));
			__m128 e2 = _mm_shuffle_ps(in2[1], in2[1], _MM_SHUFFLE(2, 2, 2, 2));
			__m128 e3 = _mm_shuffle_ps(in2[1], in2[1], _MM_SHUFFLE(3, 3, 3, 3));

			__m128 m0 = _mm_mul_ps(in1[0], e0);
			__m128 m1 = _mm_mul_ps(in1[1], e1);
			__m128 m2 = _mm_mul_ps(in1[2], e2);
			__m128 m3 = _mm_mul_ps(in1[3], e3);

			__m128 a0 = _mm_add_ps(m0, m1);
			__m128 a1 = _mm_add_ps(m2, m3);
			__m128 a2 = _mm_add_ps(a0, a1);

			out[1] = a2;
		}
		{
			__m128 e0 = _mm_shuffle_ps(in2[2], in2[2], _MM_SHUFFLE(0, 0, 0, 0));
			__m128 e1 = _mm_shuffle_ps(in2[2], in2[2], _MM_SHUFFLE(1, 1, 1, 1));
			__m128 e2 = _mm_shuffle_ps(in2[2], in2[2], _MM_SHUFFLE(2, 2, 2, 2));
			__m128 e3 = _mm_shuffle_ps(in2[2], in2[2], _MM_SHUFFLE(3, 3, 3, 3));

			__m128 m0 = _mm_mul_ps(in1[0], e0);
			__m128 m1 = _mm_mul_ps(in1[1], e1);
			__m128 m2 = _mm_mul_ps(in1[2], e2);
			__m128 m3 = _mm_mul_ps(in1[3], e3);

			__m128 a0 = _mm_add_ps(m0, m1);
			__m128 a1 = _mm_add_ps(m2, m3);
			__m128 a2 = _mm_add_ps(a0, a1);

			out[2] = a2;
		}
		{
			__m128 e0 = _mm_shuffle_ps(in2[3], in2[3], _MM_SHUFFLE(0, 0, 0, 0));
			__m128 e1 = _mm_shuffle_ps(in2[3], in2[3], _MM_SHUFFLE(1, 1, 1, 1));
			__m128 e2 = _mm_shuffle_ps(in2[3], in2[3], _MM_SHUFFLE(2, 2, 2, 2));
			__m128 e3 = _mm_shuffle_ps(in2[3], in2[3], _MM_SHUFFLE(3, 3, 3, 3));

			__m128 m0 = _mm_mul_ps(in1[0], e0);
			__m128 m1 = _mm_mul_ps(in1[1], e1);
			__m128 m2 = _mm_mul_ps(in1[2], e2);
			__m128 m3 = _mm_mul_ps(in1[3], e3);

			__m128 a0 = _mm_add_ps(m0, m1);
			__m128 a1 = _mm_add_ps(m2, m3);
			__m128 a2 = _mm_add_ps(a0, a1);

			out[3] = a2;
		}
		return out;
	}

	FINLINE static Vector3f VECTORCALL ExtractPosition(const Matrix4 matrix) noexcept
	{
		Vector3f res;
		_mm_storeu_ps(&res.x, matrix.r[3]);
		return res;
	}

	FINLINE static Vector3f VECTORCALL ExtractScale(const Matrix4 matrix) noexcept
	{
		return Vector3f(SSEVector3Length(matrix.r[0]), SSEVector3Length(matrix.r[2]), SSEVector3Length(matrix.r[1]));
	}

	inline static Quaternion VECTORCALL ExtractRotation(const Matrix4 matrix, bool rowNormalize = true) noexcept
	{
		Vector4 row0 = matrix.r[0];
		Vector4 row1 = matrix.r[1];
		Vector4 row2 = matrix.r[2];

		if (rowNormalize) {
			row0 = SSEVector3Normalize(row0.vec);
			row1 = SSEVector3Normalize(row1.vec);
			row2 = SSEVector3Normalize(row2.vec);
		}

		// code below adapted from Blender
		Quaternion q;
		const float trace = 0.25 * (row0.x + row1.y + row2.z + 1.0);

		if (trace > 0) {
			const float sq = sqrtf(trace);
			const float oneDivSq = 1.0 / (4.0 * sq);

			q = Quaternion((row1.z - row2.y) * oneDivSq,
				(row2.x - row0.z) * oneDivSq,
				(row0.y - row1.x) * oneDivSq,
				sq);
		}
		else if (row0.x > row1.y && row0.x > row2.z)
		{
			float sq = 2.0 * sqrtf(1.0 + row0.x - row1.y - row2.z);
			const float oneDivSq = 1.0 / sq;

			q = Quaternion(0.25 * sq,
				(row1.x + row0.y) * oneDivSq,
				(row2.x + row0.z) * oneDivSq,
				(row2.y - row1.z) * oneDivSq);
		}
		else if (row1.y > row2.z)
		{
			float sq = 2.0 * sqrtf(1.0f + row1.y - row0.x - row2.z);
			const float oneDivSq = 1.0 / sq;

			q = Quaternion((row1.x + row0.y) * oneDivSq,
				0.25 * sq,
				(row2.y + row1.z) * oneDivSq,
				(row2.x - row0.z) * oneDivSq);
		}
		else {
			float sq = 2.0 * sqrtf(1.0f + row2.z - row0.x - row1.y);
			const float oneDivSq = 1.0 / sq;

			q = Quaternion((row2.x + row0.z) * oneDivSq,
				(row2.y + row1.z) * oneDivSq,
				0.25 * sq,
				(row1.x - row0.y) * oneDivSq);
		}
		q = Vector4::Normalize(q.vec);
		return q;
	}

	FINLINE static Vector4 VECTORCALL Vector3Transform(const Vector3f V, const Matrix4 M) noexcept
	{
		__m128 vec = _mm_loadu_ps(&V.x);
		__m128 vResult = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(0, 0, 0, 0));
		vResult = _mm_mul_ps(vResult, M.r[0]);
		__m128 vTemp = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(1, 1, 1, 1));
		vTemp = _mm_mul_ps(vTemp, M.r[1]);
		vResult = _mm_add_ps(vResult, vTemp);
		vTemp = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(2, 2, 2, 2));
		vTemp = _mm_mul_ps(vTemp, M.r[2]);
		vResult = _mm_add_ps(vResult, vTemp);
		vResult = _mm_add_ps(vResult, M.r[3]);
		return vResult;
	}

	FINLINE static Vector4 VECTORCALL Vector4Transform(Vector4 v, const Matrix4 m)
	{
		__m128 v0 = _mm_mul_ps(m.r[0], _mm_shuffle_ps(v, v, _MM_SHUFFLE(0, 0, 0, 0)));
		__m128 v1 = _mm_mul_ps(m.r[1], _mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 1, 1)));
		__m128 v2 = _mm_mul_ps(m.r[2], _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 2, 2)));
		__m128 v3 = _mm_mul_ps(m.r[3], _mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 3, 3, 3)));
		__m128 a0 = _mm_add_ps(v0, v1);
		__m128 a1 = _mm_add_ps(v2, v3);
		__m128 a2 = _mm_add_ps(a0, a1);
		return a2;
	}
};
 
