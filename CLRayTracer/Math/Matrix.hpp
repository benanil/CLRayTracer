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
		__m128 r[4];
		float m[4][4];
	};

	Matrix4()
	{
		r[0] = g_XMIdentityR0;
		r[1] = g_XMIdentityR1;
		r[2] = g_XMIdentityR2;
		r[3] = g_XMIdentityR3;
	}

	Matrix4(__m128 x, __m128 y, __m128 z, __m128 w)
	{
		r[0] = x; r[1] = y; r[2] = z; r[3] = w;
	}

	const float* operator [] (int index) const { return m[index]; }
	float* operator [] (int index) { return m[index]; }
	
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
	FINLINE static Matrix4 VECTORCALL LookAtLH(Vector3f EyePosition, Vector3f EyeDirection, const Vector3f& UpDirection)
	{
		__m128 R2 = _mm_loadu_ps(&EyeDirection.x);

		__m128 R0 = SSEVector3Cross(_mm_loadu_ps(&UpDirection.x), R2);
		R0 = SSEVector3Normalize(R0);

		__m128 R1 = SSEVector3Cross(R2, R0);

		__m128 NegEyePosition = _mm_sub_ps(_mm_setzero_ps(), _mm_loadu_ps(&EyePosition.x)); // negate

		__m128 D0 = SSEVector3Dot(R0, NegEyePosition);
		__m128 D1 = SSEVector3Dot(R1, NegEyePosition);
		__m128 D2 = SSEVector3Dot(R2, NegEyePosition);

		Matrix4 M;
		M.r[0] = SSESelect(D0, R0, g_XMSelect1110.vec);
		M.r[1] = SSESelect(D1, R1, g_XMSelect1110.vec);
		M.r[2] = SSESelect(D2, R2, g_XMSelect1110.vec);
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


	FINLINE static Matrix4 VECTORCALL PerspectiveFovLH(float FovAngleY, float AspectRatio, float NearZ, float FarZ)
	{
		const float SinFov = sinf(0.5f * FovAngleY);
		const float CosFov = cosf(0.5f * FovAngleY);

		//ScalarSinCos(&SinFov, &CosFov, 0.5f * FovAngleY);

		float fRange = FarZ / (FarZ - NearZ);
		// Note: This is recorded on the stack
		float Height = CosFov / SinFov;
		__m128 rMem = {
			Height / AspectRatio,
			Height,
			fRange,
			-fRange * NearZ
		};
		// Copy from memory to SSE register
		__m128 vValues = rMem;
		__m128 vTemp = _mm_setzero_ps();
		// Copy x only
		vTemp = _mm_move_ss(vTemp, vValues);
		// CosFov / SinFov,0,0,0
		Matrix4 M;
		M.r[0] = vTemp;
		// 0,Height / AspectRatio,0,0
		vTemp = vValues;
		vTemp = _mm_and_ps(vTemp, g_XMMaskY);
		M.r[1] = vTemp;
		// x=fRange,y=-fRange * NearZ,0,1.0f
		vTemp = _mm_setzero_ps();
		vValues = _mm_shuffle_ps(vValues, g_XMIdentityR3, _MM_SHUFFLE(3, 2, 3, 2));
		// 0,0,fRange,1.0f
		vTemp = _mm_shuffle_ps(vTemp, vValues, _MM_SHUFFLE(3, 0, 0, 0));
		M.r[2] = vTemp;
		// 0,0,-fRange * NearZ,0.0f
		vTemp = _mm_shuffle_ps(vTemp, vValues, _MM_SHUFFLE(2, 1, 0, 0));
		M.r[3] = vTemp;
		return M;
	}

	FINLINE static Matrix4 VECTORCALL Transpose(const Matrix4 M)
	{
		__m128 vTemp1 = _mm_shuffle_ps(M.r[0], M.r[1], _MM_SHUFFLE(1, 0, 1, 0));
		__m128 vTemp3 = _mm_shuffle_ps(M.r[0], M.r[1], _MM_SHUFFLE(3, 2, 3, 2));
		__m128 vTemp2 = _mm_shuffle_ps(M.r[2], M.r[3], _MM_SHUFFLE(1, 0, 1, 0));
		__m128 vTemp4 = _mm_shuffle_ps(M.r[2], M.r[3], _MM_SHUFFLE(3, 2, 3, 2));
		Matrix4 mResult;
		mResult.r[0] = _mm_shuffle_ps(vTemp1, vTemp2, _MM_SHUFFLE(2, 0, 2, 0));
		mResult.r[1] = _mm_shuffle_ps(vTemp1, vTemp2, _MM_SHUFFLE(3, 1, 3, 1));
		mResult.r[2] = _mm_shuffle_ps(vTemp3, vTemp4, _MM_SHUFFLE(2, 0, 2, 0));
		mResult.r[3] = _mm_shuffle_ps(vTemp3, vTemp4, _MM_SHUFFLE(3, 1, 3, 1));
		return mResult;
	}

	// from directx math:  xnamathmatrix.inl
	inline Matrix4 static VECTORCALL Inverse(const Matrix4 M) noexcept
	{
		__m128 vTemp1 = _mm_shuffle_ps(M.r[0], M.r[1], _MM_SHUFFLE(1, 0, 1, 0));
		__m128 vTemp3 = _mm_shuffle_ps(M.r[0], M.r[1], _MM_SHUFFLE(3, 2, 3, 2));
		__m128 vTemp2 = _mm_shuffle_ps(M.r[2], M.r[3], _MM_SHUFFLE(1, 0, 1, 0));
		__m128 vTemp4 = _mm_shuffle_ps(M.r[2], M.r[3], _MM_SHUFFLE(3, 2, 3, 2));

		Matrix4 MT;
		MT.r[0] = _mm_shuffle_ps(vTemp1, vTemp2, _MM_SHUFFLE(2, 0, 2, 0));
		MT.r[1] = _mm_shuffle_ps(vTemp1, vTemp2, _MM_SHUFFLE(3, 1, 3, 1));
		MT.r[2] = _mm_shuffle_ps(vTemp3, vTemp4, _MM_SHUFFLE(2, 0, 2, 0));
		MT.r[3] = _mm_shuffle_ps(vTemp3, vTemp4, _MM_SHUFFLE(3, 1, 3, 1));

		__m128 V00 = _mm_permute_ps(MT.r[2], _MM_SHUFFLE(1, 1, 0, 0));
		__m128 V10 = _mm_permute_ps(MT.r[3], _MM_SHUFFLE(3, 2, 3, 2));
		__m128 V01 = _mm_permute_ps(MT.r[0], _MM_SHUFFLE(1, 1, 0, 0));
		__m128 V11 = _mm_permute_ps(MT.r[1], _MM_SHUFFLE(3, 2, 3, 2));
		__m128 V02 = _mm_shuffle_ps(MT.r[2], MT.r[0], _MM_SHUFFLE(2, 0, 2, 0));
		__m128 V12 = _mm_shuffle_ps(MT.r[3], MT.r[1], _MM_SHUFFLE(3, 1, 3, 1));

		__m128 D0 = _mm_mul_ps(V00, V10);
		__m128 D1 = _mm_mul_ps(V01, V11);
		__m128 D2 = _mm_mul_ps(V02, V12);

		V00 = _mm_permute_ps(MT.r[2], _MM_SHUFFLE(3, 2, 3, 2));
		V10 = _mm_permute_ps(MT.r[3], _MM_SHUFFLE(1, 1, 0, 0));
		V01 = _mm_permute_ps(MT.r[0], _MM_SHUFFLE(3, 2, 3, 2));
		V11 = _mm_permute_ps(MT.r[1], _MM_SHUFFLE(1, 1, 0, 0));
		V02 = _mm_shuffle_ps(MT.r[2], MT.r[0], _MM_SHUFFLE(3, 1, 3, 1));
		V12 = _mm_shuffle_ps(MT.r[3], MT.r[1], _MM_SHUFFLE(2, 0, 2, 0));

		D0 = _mm_fmadd_ps(V00, V10, D0);
		D1 = _mm_fmadd_ps(V01, V11, D1);
		D2 = _mm_fmadd_ps(V02, V12, D2);
		// V11 = D0Y,D0W,D2Y,D2Y
		V11 = _mm_shuffle_ps(D0, D2, _MM_SHUFFLE(1, 1, 3, 1));
		V00 = _mm_permute_ps(MT.r[1], _MM_SHUFFLE(1, 0, 2, 1));
		V10 = _mm_shuffle_ps(V11, D0, _MM_SHUFFLE(0, 3, 0, 2));
		V01 = _mm_permute_ps(MT.r[0], _MM_SHUFFLE(0, 1, 0, 2));
		V11 = _mm_shuffle_ps(V11, D0, _MM_SHUFFLE(2, 1, 2, 1));
		// V13 = D1Y,D1W,D2W,D2W
		__m128 V13 = _mm_shuffle_ps(D1, D2, _MM_SHUFFLE(3, 3, 3, 1));
		V02 = _mm_permute_ps(MT.r[3], _MM_SHUFFLE(1, 0, 2, 1));
		V12 = _mm_shuffle_ps(V13, D1, _MM_SHUFFLE(0, 3, 0, 2));
		__m128 V03 = _mm_permute_ps(MT.r[2], _MM_SHUFFLE(0, 1, 0, 2));
		V13 = _mm_shuffle_ps(V13, D1, _MM_SHUFFLE(2, 1, 2, 1));

		__m128 C0 = _mm_mul_ps(V00, V10);
		__m128 C2 = _mm_mul_ps(V01, V11);
		__m128 C4 = _mm_mul_ps(V02, V12);
		__m128 C6 = _mm_mul_ps(V03, V13);

		// V11 = D0X,D0Y,D2X,D2X
		V11 = _mm_shuffle_ps(D0, D2, _MM_SHUFFLE(0, 0, 1, 0));
		V00 = _mm_permute_ps(MT.r[1], _MM_SHUFFLE(2, 1, 3, 2));
		V10 = _mm_shuffle_ps(D0, V11, _MM_SHUFFLE(2, 1, 0, 3));
		V01 = _mm_permute_ps(MT.r[0], _MM_SHUFFLE(1, 3, 2, 3));
		V11 = _mm_shuffle_ps(D0, V11, _MM_SHUFFLE(0, 2, 1, 2));
		// V13 = D1X,D1Y,D2Z,D2Z
		V13 = _mm_shuffle_ps(D1, D2, _MM_SHUFFLE(2, 2, 1, 0));
		V02 = _mm_permute_ps(MT.r[3], _MM_SHUFFLE(2, 1, 3, 2));
		V12 = _mm_shuffle_ps(D1, V13, _MM_SHUFFLE(2, 1, 0, 3));
		V03 = _mm_permute_ps(MT.r[2], _MM_SHUFFLE(1, 3, 2, 3));
		V13 = _mm_shuffle_ps(D1, V13, _MM_SHUFFLE(0, 2, 1, 2));

		C0 = _mm_fnmadd_ps(V00, V10, C0);
		C2 = _mm_fnmadd_ps(V01, V11, C2);
		C4 = _mm_fnmadd_ps(V02, V12, C4);
		C6 = _mm_fnmadd_ps(V03, V13, C6);

		V00 = _mm_permute_ps(MT.r[1], _MM_SHUFFLE(0, 3, 0, 3));
		// V10 = D0Z,D0Z,D2X,D2Y
		V10 = _mm_shuffle_ps(D0, D2, _MM_SHUFFLE(1, 0, 2, 2));
		V10 = _mm_permute_ps(V10, _MM_SHUFFLE(0, 2, 3, 0));
		V01 = _mm_permute_ps(MT.r[0], _MM_SHUFFLE(2, 0, 3, 1));
		// V11 = D0X,D0W,D2X,D2Y
		V11 = _mm_shuffle_ps(D0, D2, _MM_SHUFFLE(1, 0, 3, 0));
		V11 = _mm_permute_ps(V11, _MM_SHUFFLE(2, 1, 0, 3));
		V02 = _mm_permute_ps(MT.r[3], _MM_SHUFFLE(0, 3, 0, 3));
		// V12 = D1Z,D1Z,D2Z,D2W
		V12 = _mm_shuffle_ps(D1, D2, _MM_SHUFFLE(3, 2, 2, 2));
		V12 = _mm_permute_ps(V12, _MM_SHUFFLE(0, 2, 3, 0));
		V03 = _mm_permute_ps(MT.r[2], _MM_SHUFFLE(2, 0, 3, 1));
		// V13 = D1X,D1W,D2Z,D2W
		V13 = _mm_shuffle_ps(D1, D2, _MM_SHUFFLE(3, 2, 3, 0));
		V13 = _mm_permute_ps(V13, _MM_SHUFFLE(2, 1, 0, 3));

		V00 = _mm_mul_ps(V00, V10);
		V01 = _mm_mul_ps(V01, V11);
		V02 = _mm_mul_ps(V02, V12);
		V03 = _mm_mul_ps(V03, V13);
		__m128 C1 = _mm_sub_ps(C0, V00);
		C0 = _mm_add_ps(C0, V00);
		__m128 C3 = _mm_add_ps(C2, V01);
		C2 = _mm_sub_ps(C2, V01);
		__m128 C5 = _mm_sub_ps(C4, V02);
		C4 = _mm_add_ps(C4, V02);
		__m128 C7 = _mm_add_ps(C6, V03);
		C6 = _mm_sub_ps(C6, V03);

		C0 = _mm_shuffle_ps(C0, C1, _MM_SHUFFLE(3, 1, 2, 0));
		C2 = _mm_shuffle_ps(C2, C3, _MM_SHUFFLE(3, 1, 2, 0));
		C4 = _mm_shuffle_ps(C4, C5, _MM_SHUFFLE(3, 1, 2, 0));
		C6 = _mm_shuffle_ps(C6, C7, _MM_SHUFFLE(3, 1, 2, 0));
		C0 = _mm_permute_ps(C0, _MM_SHUFFLE(3, 1, 2, 0));
		C2 = _mm_permute_ps(C2, _MM_SHUFFLE(3, 1, 2, 0));
		C4 = _mm_permute_ps(C4, _MM_SHUFFLE(3, 1, 2, 0));
		C6 = _mm_permute_ps(C6, _MM_SHUFFLE(3, 1, 2, 0));
		// Get the determinant
		__m128 vTemp = Vector4::Dot(C0, MT.r[0]);
		vTemp = _mm_div_ps(g_XMOne, vTemp);
		Matrix4 mResult;
		mResult.r[0] = _mm_mul_ps(C0, vTemp);
		mResult.r[1] = _mm_mul_ps(C2, vTemp);
		mResult.r[2] = _mm_mul_ps(C4, vTemp);
		mResult.r[3] = _mm_mul_ps(C6, vTemp);
		return mResult;
	}

	inline Matrix4 static VECTORCALL Multiply(const Matrix4 M1, const Matrix4& M2)
	{
		Matrix4 mResult;
		__m128 vW = M1.r[0];
		__m128 vX = _mm_permute_ps(vW, _MM_SHUFFLE(0, 0, 0, 0));
		__m128 vY = _mm_permute_ps(vW, _MM_SHUFFLE(1, 1, 1, 1));
		__m128 vZ = _mm_permute_ps(vW, _MM_SHUFFLE(2, 2, 2, 2));

		vW = _mm_permute_ps(vW, _MM_SHUFFLE(3, 3, 3, 3));

		// Perform the operation on the first row
		vX = _mm_mul_ps(vX, M2.r[0]);
		vY = _mm_mul_ps(vY, M2.r[1]);
		vZ = _mm_mul_ps(vZ, M2.r[2]);
		vW = _mm_mul_ps(vW, M2.r[3]);

		vX = _mm_add_ps(vX, vZ);
		vY = _mm_add_ps(vY, vW);
		vX = _mm_add_ps(vX, vY);
		mResult.r[0] = vX;

		vW = M1.r[1];
		vX = _mm_permute_ps(vW, _MM_SHUFFLE(0, 0, 0, 0));
		vY = _mm_permute_ps(vW, _MM_SHUFFLE(1, 1, 1, 1));
		vZ = _mm_permute_ps(vW, _MM_SHUFFLE(2, 2, 2, 2));
		vW = _mm_permute_ps(vW, _MM_SHUFFLE(3, 3, 3, 3));

		vX = _mm_mul_ps(vX, M2.r[0]);
		vY = _mm_mul_ps(vY, M2.r[1]);
		vZ = _mm_mul_ps(vZ, M2.r[2]);
		vW = _mm_mul_ps(vW, M2.r[3]);
		vX = _mm_add_ps(vX, vZ);
		vY = _mm_add_ps(vY, vW);
		vX = _mm_add_ps(vX, vY);
		mResult.r[1] = vX;

		vW = M1.r[2];
		vX = _mm_permute_ps(vW, _MM_SHUFFLE(0, 0, 0, 0));
		vY = _mm_permute_ps(vW, _MM_SHUFFLE(1, 1, 1, 1));
		vZ = _mm_permute_ps(vW, _MM_SHUFFLE(2, 2, 2, 2));
		vW = _mm_permute_ps(vW, _MM_SHUFFLE(3, 3, 3, 3));

		vX = _mm_mul_ps(vX, M2.r[0]);
		vY = _mm_mul_ps(vY, M2.r[1]);
		vZ = _mm_mul_ps(vZ, M2.r[2]);
		vW = _mm_mul_ps(vW, M2.r[3]);
		vX = _mm_add_ps(vX, vZ);
		vY = _mm_add_ps(vY, vW);
		vX = _mm_add_ps(vX, vY);
		mResult.r[2] = vX;

		vW = M1.r[3];
		vX = _mm_permute_ps(vW, _MM_SHUFFLE(0, 0, 0, 0));
		vY = _mm_permute_ps(vW, _MM_SHUFFLE(1, 1, 1, 1));
		vZ = _mm_permute_ps(vW, _MM_SHUFFLE(2, 2, 2, 2));
		vW = _mm_permute_ps(vW, _MM_SHUFFLE(3, 3, 3, 3));

		vX = _mm_mul_ps(vX, M2.r[0]);
		vY = _mm_mul_ps(vY, M2.r[1]);
		vZ = _mm_mul_ps(vZ, M2.r[2]);
		vW = _mm_mul_ps(vW, M2.r[3]);
		vX = _mm_add_ps(vX, vZ);
		vY = _mm_add_ps(vY, vW);
		vX = _mm_add_ps(vX, vY);
		mResult.r[3] = vX;
		return mResult;
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
		__m128 v0 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(0, 0, 0, 0));
		__m128 v1 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 1, 1));
		__m128 v2 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 2, 2));
		__m128 v3 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 3, 3, 3));

		v0 = _mm_mul_ps(m.r[0], v0);
		v1 = _mm_mul_ps(m.r[1], v1);
		v2 = _mm_mul_ps(m.r[2], v2);
		v3 = _mm_mul_ps(m.r[3], v3);

		__m128 a0 = _mm_add_ps(v0, v1);
		__m128 a1 = _mm_add_ps(v2, v3);
		__m128 a2 = _mm_add_ps(a0, a1);
		return a2;
	}
};
   