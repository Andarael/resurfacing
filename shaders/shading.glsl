#ifndef SHADING_GLSL
#define SHADING_GLSL

#include "common.glsl"

vec3 shading(vec3 worldPos, vec3 normal, vec2 localUv, vec2 baseUV, bool allowAO) {
    vec3 L = normalize(globalShadingUbo.lightPos - worldPos);
    vec3 V = normalize(globalShadingUbo.viewPos.xyz - worldPos);
    vec3 R = reflect(-L, normal);
    vec3 H = normalize(L + V);
    float cosTheta = max(dot(normal, L), 0);

    vec3 ambient = shadingUbo.ambient;

    vec3 diffuse = shadingUbo.diffuse;
    if (globalShadingUbo.shadingHack) {
        diffuse *= ((cosTheta * cosTheta) * .5 + .5);
    } else {
        diffuse *= cosTheta;
    }

    vec3 surfaceColor = vec3(1);

    if (shadingUbo.displayPrimId) {
        surfaceColor = IDtoColor(gl_PrimitiveID);
    }

    if (shadingUbo.displayNormals) {
        surfaceColor = normal * .5 + .5;
    }

    if (shadingUbo.displayLocalUv) {
        surfaceColor = vec3(localUv, 0);
    }

    if (shadingUbo.displayGlobalUv) {
        surfaceColor = vec3(baseUV, 0);
    }

    if (shadingUbo.showElementTexture) {
        vec3 textureColor = texture(sampler2D(textures[elementTextureID], samplers[nearestSamplerID]), baseUV).xyz;
        if (textureColor.r < 0.99 || textureColor.g < 0.99 || textureColor.b < 0.99) {
            diffuse = vec3(1);
        }
        surfaceColor = 0.2 + 0.8 * textureColor;
    }

    if (shadingUbo.doAo) {
        float ao = texture(sampler2D(textures[AOTextureID], samplers[linearSamplerID]), baseUV).x;
        surfaceColor *= ao;
    }

    if (!shadingUbo.doShading) {
        return surfaceColor;
    }

    vec3 specular = vec3(0);
    if (cosTheta > 0.5) {
        float specAngle = max(dot(normal, H), 0);
        specular = shadingUbo.specular * shadingUbo.specularStrength;
        specular *= pow(specAngle, shadingUbo.shininess);
    }

    vec3 lightColor = globalShadingUbo.lightColor;
    vec3 color = ambient + (diffuse + specular) * lightColor * surfaceColor;
    if (globalShadingUbo.filmic) {
        color = filmicTransform(color);
    }

    return color;
}

#endif // SHADING_GLSL