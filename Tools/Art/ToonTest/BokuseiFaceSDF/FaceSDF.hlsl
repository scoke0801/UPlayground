// R/G contain model-derived +right/-right shadow transition angles / pi.
// L already points TOWARD the light; LightDirection points the other way.
float3 faceF = HeadForwardWS * rsqrt(max(dot(HeadForwardWS, HeadForwardWS), 1e-6));
float3 faceR = HeadRightWS - faceF * dot(HeadRightWS, faceF);
faceR *= rsqrt(max(dot(faceR, faceR), 1e-6));
float faceX = dot(L, faceR);
float faceY = dot(L, faceF);
float facePlanarLength = sqrt(max(faceX * faceX + faceY * faceY, 0.0));
float faceAngle = acos(clamp(faceY / max(facePlanarLength, 1e-5), -1.0, 1.0)) / 3.14159265359;
float faceThreshold = lerp(FaceSDFSample.r, FaceSDFSample.g, step(faceX, 0.0));
float faceDelta = faceThreshold - saturate(faceAngle + FaceSDFBias);
float faceAA = max(fwidth(faceDelta) * max(BandAA, 0.0), 1e-4);
float faceWidth = max(max(FaceSDFSoftness, 0.0), faceAA);
float faceLit = smoothstep(-faceWidth, faceWidth, faceDelta);
// At zenith/nadir, azimuth is undefined. Fade to the existing geometric bands.
float faceMix = saturate(FaceSDFEnabled) * saturate(FaceSDFSample.b)
    * smoothstep(0.08, 0.25, facePlanarLength);
float3 faceBand = lerp(ShadowTint, LightTint, faceLit);
faceBand = lerp(LightTint, faceBand, saturate(FaceSDFStrength));
band = lerp(band, faceBand, faceMix);
litWeight = lerp(litWeight, faceLit, faceMix);
