struct VsToPs
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
};


float4 main(VsToPs input) : SV_TARGET
{
    return input.color;
}