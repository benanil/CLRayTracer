
float2 philox(uint2 st, uint k)
{
  ulong p;
  int   i;

  for (i = 0 ; i < 3 ; i += 1)
    {
      p = st.x * 0xcd9e8d57ul;

      st.x = ((uint)(p >> 32)) ^ st.y ^ k;
      st.y = (uint)p;

      k += 0x9e3779b9u;
    }

  return convert_float2(st) / 2147483648.0f - 1.0f;
}

float SampleSimplexNoise(float2 uv, float2 offset)
{
	float  c, d, m;
  	float2 p;
  	int    j;

  	for (j = 0, m = 0, c = 1, d = 2.0f; j < 4; c *= 2, d *= 2, j += 1)
    {
    	float s, t, n;
      	float2 g[3], u[3], i, di;
      	int k;

      	p = offset + uv * d;

	  	s = (p.x + p.y) * (sqrt(3.0f) - 1) / 2;
        i = floor(p + s);
	    
        s = (i.x + i.y) * (3 - sqrt(3.0f)) / 6;
        u[0] = p - i + s;
	    
        di = u[0].x >= u[0].y ? (float2)(1, 0) : (float2)(0, 1);
	    
        u[1] = u[0] - di + (3 - sqrt(3.0f)) / 6;
        u[2] = u[0] - 1 + (3 - sqrt(3.0f)) / 3;
	    
        g[0] = philox(convert_uint2(convert_int2(i)), 3169);
        g[1] = philox(convert_uint2(convert_int2(i + di)), 3169);
        g[2] = philox(convert_uint2(convert_int2(i + 1)), 3169);
	    
        for (k = 0, n = 0 ; k < 3 ; k += 1)
        {
            t = 0.5f - dot(u[k], u[k]);
	    
            if (t > 0)
            {
             	t *= t;
            	n += t * t * dot(g[k], u[k]);
            }
        }

      m += 70 * n / c;
    }
	return m;
}

__kernel void texture(__write_only image2d_t inout, float time) 
{
	const int gidx = get_global_id(0);
  	const int gidy = get_global_id(1);

	float2 uv = (float2)(gidx, gidy) / (float2)(800.0f, 600.0f);
   	
	float m  = SampleSimplexNoise(uv *0.8f, (float2)(time, 0));
	float m1 = SampleSimplexNoise(uv * 500.0f, (float2)(time, 0)) - m;

    uv *=  1.0f - uv.yx;   //vec2(1.0)- uv.yx; -> 1.-u.yx; Thanks FabriceNeyret !
    
    float vig = uv.x * uv.y * 10.0f; // multiply with sth for intensity
    
    vig = 1.0f - pow(vig, 0.35f); // change pow for modifying the extend of the  vignette
	
	write_imagef(inout, (int2)(gidx, gidy), (float4)(m - vig , m1, 0, 1.0f));
}
