struct IaToVs
{
    float3 pos  : POSITION;
    float4 color : COLOR;
};

struct VsToPs
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
};

cbuffer MyConstants : register(b0)
{
    float4x4 worldTransform;
    float4x4 viewTransform;
    float4x4 projectionTransform;
};


VsToPs main(IaToVs input)
{
    VsToPs output;

    // world space
    float4 outPosition = mul(float4(input.pos, 1.f), worldTransform);

    // camera space
    outPosition = mul(outPosition, viewTransform);

    // clip space
    outPosition = mul(outPosition, projectionTransform);

    output.pos = outPosition;
    output.color = input.color;

    return output;
}
