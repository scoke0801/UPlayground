// UE 5.8, before DOF/TSR. Only draw the silhouette of visible stencil 73.
// GBuffer/custom-depth UVs are buffer UVs, not viewport UVs.
float2 uv = GetDefaultSceneTextureUV(Parameters, 13);
float2 texel = GetSceneTextureBufferSize(13).zw;
float depth = SceneTextureLookup(uv, 1, false).r;
float ownDepth = SceneTextureLookup(uv, 13, false).r;
float ownStencil = SceneTextureLookup(uv, 25, false).r;
float center = (abs(ownStencil - StencilID) < .5 && ownDepth <= depth + DepthBias) ? 1.0 : 0.0;
float silhouette = 0.0;
float2 offsets[8] = {float2(1,0), float2(-1,0), float2(0,1), float2(0,-1),
                     float2(.707,.707), float2(-.707,.707), float2(.707,-.707), float2(-.707,-.707)};
[unroll] for (int i=0; i<8; ++i)
{
    float2 sampleUV = ClampSceneTextureUV(uv + offsets[i] * texel * clamp(WidthPixels, 0.0, 3.0), 13);
    float stencil = SceneTextureLookup(sampleUV, 25, false).r;
    float customDepth = SceneTextureLookup(sampleUV, 13, false).r;
    float visibleDepth = SceneTextureLookup(sampleUV, 1, false).r;
    // Both tests matter: the neighbor must be visible and not behind this foreground pixel.
    float visible = (abs(stencil - StencilID) < .5 && customDepth <= visibleDepth + DepthBias
                     && customDepth <= depth + DepthBias) ? 1.0 : 0.0;
    silhouette = max(silhouette, visible);
}
float mask = (1.0 - center) * silhouette * saturate(Strength);
return lerp(SceneColor.rgb, OutlineColor * View.PreExposure, mask);
