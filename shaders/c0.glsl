#version 460
layout(row_major) uniform;
layout(row_major) buffer;

#line 4 0
layout(std430, binding = 2) buffer StructuredBuffer_float_t_0 {
    float _data[];
} result_0;

#line 2
layout(std430, binding = 0) readonly buffer StructuredBuffer_float_t_1 {
    float _data[];
} buffer0_0;

#line 3
layout(std430, binding = 1) readonly buffer StructuredBuffer_float_t_2 {
    float _data[];
} buffer1_0;



layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main()
{

#line 11
    uint index_0 = gl_GlobalInvocationID.x;
    result_0._data[uint(index_0)] = buffer0_0._data[uint(index_0)] + buffer1_0._data[uint(index_0)];
    return;
}

