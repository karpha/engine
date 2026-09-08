#include "light.h"

// normal distribution function - GGX/Trowbridge-Reitz Distribution
// produce realistic highlight shapes 
float DistributionGGX(float NdotH, float roughness){
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0 );
    denom = PI * denom * denom;
    return nom / denom;
}

// geometry function(G) Smith's method with Schlick-GGX approximation
// plays a crucial role 
// by accounting for the statistical correlation between masking (when the viewer can’t see a microfacet) 
// and shadowing (when light can’t reach a microfacet). 
// it prevents energy gain at grazing angles where naive models become unrealistically bright, ensuring our materials look believable under all viewing conditions
float GeometrySmith(float NdotV, float NdotL, float roughness ){
    float r = roughness + 1.0;
    float k = ( r * r ) / 8.0;

    // geometry obstruction from view direction
    float ggx1 = NdotV / ( NdotV * (1.0 - k) + k );
    float ggx2 = NdotL / ( NdotL * (1.0 - k) + k );

    return ggx1 * ggx2;
}

// Fresnel Reflectance - Schlick's approximation
// The Fresnel effect captures a phenomenon we see every day: 
// materials become more reflective at grazing angles, like water appearing mirror-like when viewed from the side

// The F0 parameter represents reflectance at normal incidence (0° viewing angle), 
// allowing us to control how reflective different materials appear when viewed head-on.
glm::vec3 FresnelSchlick(float cosTheta, glm::vec3 F0 ){
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
