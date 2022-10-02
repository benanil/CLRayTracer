__kernel void hello(__global int* inout, int startValue) 
{
    int i = get_global_id(0);
    inout[i] += startValue;
}