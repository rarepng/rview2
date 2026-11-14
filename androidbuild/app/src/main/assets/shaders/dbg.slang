

[shader("vertex")]
float4 vxmain() {

    return float4(0.0f, 0.0f, 0.0f, 1.0f);
}


[shader("pixel")]
float4 pxmain(float4 in) : SV_Target {

return float4(1.0f,1.0f,0.0f,1.0f);
}