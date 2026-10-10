// UE Custom expression body. LightDirection is the direction light travels.
// Analytic art-directed lighting, not a replacement for UE shadow reception.
float3 N = NormalWS * rsqrt(max(dot(NormalWS, NormalWS), 1e-6));
float3 L = -LightDirection * rsqrt(max(dot(LightDirection, LightDirection), 1e-6));
float3 V = CameraVector * rsqrt(max(dot(CameraVector, CameraVector), 1e-6));
float wrap = saturate(DiffuseWrap);
float lighting = saturate((dot(N, L) + wrap) / (1.0 + wrap));
// Pixel derivatives keep the bands crisp at close range and cover subpixel edges.
float aa = max(fwidth(lighting) * max(BandAA, 0.0), 1e-4);
float low = saturate(ShadowThreshold);
float high = max(low + 1e-4, saturate(LightThreshold));
float sw = max(max(ShadowSoftness, 0.0), aa);
float lw = max(max(LightSoftness, 0.0), aa);
float midWeight = smoothstep(low - sw, low + sw, lighting);
float litWeight = smoothstep(high - lw, high + lw, lighting);
float3 band = lerp(ShadowTint, MidTint, midWeight);
band = lerp(band, LightTint, litWeight);
band = lerp(LightTint, band, saturate(ShadeStrength));
float3 base = max(BaseColor * BaseTint, 0.0);

float fresnel = pow(saturate(1.0 - abs(dot(N, V))), max(RimPower, .01));
float rw = max(max(RimSoftness, 0.0), max(fwidth(fresnel), 1e-4));
float rim = smoothstep(RimThreshold - rw, RimThreshold + rw, fresnel);
rim *= lerp(1.0, smoothstep(0.0, .65, saturate(dot(N, L))), saturate(RimLightMask));
// Backfaces must not light up inside collars, sleeves or double-sided hair cards.
rim *= step(0.0, FaceSign);
float3 rimTint = lerp(RimColor, RimColor * base, saturate(RimBaseBlend));

float3 H = L + V;
H *= rsqrt(max(dot(H, H), 1e-6));
float tangentH = dot(HairTangentWS * rsqrt(max(dot(HairTangentWS, HairTangentWS), 1e-6)), H);
float strandSpec = pow(sqrt(saturate(1.0 - tangentH * tangentH)), max(SpecularPower, 1.0));
float spec = lerp(pow(saturate(dot(N, H)), max(SpecularPower, 1.0)), strandSpec, saturate(HairAnisotropy));
float hw = max(max(SpecularSoftness, 0.0), max(fwidth(spec), 1e-4));
// Hair keeps a continuous lobe: a second hard threshold magnifies tangent seams
// and turns each low-poly lock into a separate reflective plate.
float hairSoft = saturate(HairSoftness);
spec = lerp(smoothstep(SpecularThreshold - hw, SpecularThreshold + hw, spec), spec, hairSoft);
float specLight = lerp(litWeight, smoothstep(0.0, .8, lighting), hairSoft);
spec *= specLight * step(0.0, FaceSign);
// Preserve dark hair hue instead of laying a near-white stripe over black locks.
float3 specTint = SpecularTint * lerp(1.0.xxx, max(sqrt(saturate(base)), .25.xxx), hairSoft);
spec *= saturate(ToonDetailWeight);
rim *= lerp(.5, 1.0, saturate(ToonDetailWeight));
// Separate outputs keep lighting-independent art accents and state feedback out
// of Default Lit's albedo. Unlit still uses the complete original expression.
ToonDiffuse = base * band;
ToonAccent = rimTint * rim * max(RimStrength, 0.0) + specTint * spec * max(SpecularStrength, 0.0);
return base * band + rimTint * rim * max(RimStrength, 0.0)
    + specTint * spec * max(SpecularStrength, 0.0) + StateColor * StateGlow;
