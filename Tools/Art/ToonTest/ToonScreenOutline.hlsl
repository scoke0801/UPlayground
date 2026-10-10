// UE 5.8, before DOF/TSR. Only draw the silhouette of visible stencil 73.
// GBuffer/custom-depth UVs are buffer UVs, not viewport UVs.
float2 uv = GetDefaultSceneTextureUV(Parameters, 13);
float2 texel = GetSceneTextureBufferSize(13).zw;
float depth = SceneTextureLookup(uv, 1, false).r;
float ownDepth = SceneTextureLookup(uv, 13, false).r;
float ownStencil = SceneTextureLookup(uv, 25, false).r;
float center = (abs(ownStencil - StencilID) < .5 && ownDepth <= depth + DepthBias) ? 1.0 : 0.0;
float silhouette = 0.0;
float overlap = 0.0;
// Before DOF executes at primary render resolution. Author width in final output
// pixels, including the view's primary/secondary screen percentage.
float radius = clamp(WidthPixels, 0.0, 3.0) * View.ViewResolutionFraction;
float2 offsets[8] = {float2(1,0), float2(-1,0), float2(0,1), float2(0,-1),
                     float2(.707,.707), float2(-.707,.707), float2(.707,-.707), float2(-.707,-.707)};
[unroll] for (int i=0; i<8; ++i)
{
    float2 sampleUV = ClampSceneTextureUV(uv + offsets[i] * texel * radius, 13);
    float stencil = SceneTextureLookup(sampleUV, 25, false).r;
    float customDepth = SceneTextureLookup(sampleUV, 13, false).r;
    float visibleDepth = SceneTextureLookup(sampleUV, 1, false).r;
    // Both tests matter: the neighbor must be visible and not behind this foreground pixel.
    float visible = (abs(stencil - StencilID) < .5 && customDepth <= visibleDepth + DepthBias
                     && customDepth <= depth + DepthBias) ? 1.0 : 0.0;
    silhouette = max(silhouette, visible);
    // Draw on the nearer character only. Both samples must be visible toon
    // surfaces; large depth discontinuities separate overlapping actors without
    // tracing facial normals, clothing texture or an occluding environment wall.
    float separation = max(OverlapDepthCm, ownDepth * OverlapRelativeDepth);
    float neighborToon = (abs(stencil - StencilID) < .5 && customDepth <= visibleDepth + DepthBias) ? 1.0 : 0.0;
    overlap = max(overlap, center * neighborToon * step(separation, customDepth - ownDepth));
}
float mask = max((1.0 - center) * silhouette, overlap * saturate(OverlapStrength)) * saturate(Strength);
mask *= step(1e-4, WidthPixels);
return lerp(SceneColor.rgb, OutlineColor * View.PreExposure, mask);
