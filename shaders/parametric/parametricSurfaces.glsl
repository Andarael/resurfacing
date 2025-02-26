#ifndef PARAMETRIC_SURFACES_GLSL
#define PARAMETRIC_SURFACES_GLSL

#include "../lods.glsl"
#include "parametricGrids.glsl"

// ================== Constants =====================

#define PI 3.14159265359

// ================== Parametric Surfaces =====================

void parametricTorus(vec2 uv, out vec3 pos, out vec3 normal, float R, float r) {

    pos.x = (R + r * cos(uv.y * 2.0 * PI)) * cos(uv.x * 2.0 * PI);
    pos.y = (R + r * cos(uv.y * 2.0 * PI)) * sin(uv.x * 2.0 * PI);
    pos.z = r * sin(uv.y * 2.0 * PI);

    normal.x = cos(uv.x * 2.0 * PI) * cos(uv.y * 2.0 * PI);
    normal.y = sin(uv.x * 2.0 * PI) * cos(uv.y * 2.0 * PI);
    normal.z = sin(uv.y * 2.0 * PI);
}

void parametricSphere(vec2 uv, out vec3 pos, out vec3 normal, float radius) {
    float theta = uv.x * PI;     // Polar angle
    float phi = uv.y * 2.0 * PI; // Azimuthal angle

    pos.x = radius * sin(theta) * cos(phi);
    pos.y = radius * sin(theta) * sin(phi);
    pos.z = radius * cos(theta);

    normal.x = sin(theta) * cos(phi);
    normal.y = sin(theta) * sin(phi);
    normal.z = cos(theta);
}

void parametricMobiusStrip(vec2 uv, out vec3 pos, out vec3 normal, float width) {
    float t = uv.x * 2.0 * PI;
    float s = (uv.y - 0.5) * width;

    pos.x = (1.0 + s * cos(t / 2.0)) * cos(t);
    pos.y = (1.0 + s * cos(t / 2.0)) * sin(t);
    pos.z = s * sin(t / 2.0);

    normal.x = cos(t) * cos(t / 2.0) * (1.0 + s * cos(t / 2.0)) - sin(t) * s * sin(t / 2.0);
    normal.y = sin(t) * cos(t / 2.0) * (1.0 + s * cos(t / 2.0)) + cos(t) * s * sin(t / 2.0);
    normal.z = cos(t / 2.0) * s;
}

void parametricKleinBottle(vec2 uv, out vec3 pos, out vec3 normal, float a) {
    float t = uv.x * 2.0 * PI;
    float s = uv.y * 2.0 * PI;

    pos.x = (a * cos(t) * (1.0 + sin(t)) + a * (1.0 - cos(t) / 2.0) * cos(s));
    pos.y = (a * sin(t) * (1.0 + sin(t)) + a * (1.0 - cos(t) / 2.0) * sin(s));
    pos.z = a * (1.0 - cos(t) / 2.0) * sin(s);

    normal.x = -sin(t) * (1.0 + sin(t)) - a * (cos(t) * sin(t) + sin(t)) * cos(s);
    normal.y = cos(t) * (1.0 + sin(t)) - a * (cos(t) * sin(t) + sin(t)) * sin(s);
    normal.z = a * (1.0 - cos(t) / 2.0) * cos(s);
}

void parametricHyperbolicParaboloid(vec2 uv, out vec3 pos, out vec3 normal, float a, float b) {
    float x = (uv.x - 0.5) * 4.0 * a;
    float y = (uv.y - 0.5) * 4.0 * b;

    pos.x = x;
    pos.y = y;
    pos.z = (x * x) / (a * a) - (y * y) / (b * b);

    normal.x = -2.0 * x / (a * a);
    normal.y = 2.0 * y / (b * b);
    normal.z = -1.0;
}

void parametricHelicoid(vec2 uv, out vec3 pos, out vec3 normal, float a) {
    float t = uv.x * 4.0 * PI;
    float z = uv.y * 4.0;

    pos.x = a * t * cos(t);
    pos.y = a * t * sin(t);
    pos.z = z;

    normal.x = cos(t);
    normal.y = sin(t);
    normal.z = 0.0;
}

void parametricCone(vec2 uv, out vec3 pos, out vec3 normal, float height, float radius) {
    float theta = uv.x * 2.0 * PI;
    float r = (1.0 - uv.y) * radius;

    pos.x = r * cos(theta);
    pos.y = r * sin(theta);
    pos.z = uv.y * height * 8;

    normal.x = cos(theta);
    normal.y = sin(theta);
    normal.z = 1; // radius / height;
}

void parametricCylinder(vec2 uv, out vec3 pos, out vec3 normal, float height, float radius) {
    float theta = uv.x * 2.0 * PI;
    float z = (uv.y - 0.5) * height;

    pos.x = radius * cos(theta);
    pos.y = radius * sin(theta);
    pos.z = z;

    normal.x = cos(theta);
    normal.y = sin(theta);
}

void parametricEgg(vec2 uv, out vec3 pos, out vec3 normal, float a, float b) {
    float theta = uv.x * PI;
    float phi = uv.y * 2.0 * PI;

    pos.x = a * sin(theta) * cos(phi);
    pos.y = a * sin(theta) * sin(phi);
    pos.z = b * cos(theta);

    normal.x = sin(theta) * cos(phi);
    normal.y = sin(theta) * sin(phi);
}

// ================== Parametric Primitive Size Functions =====================
void parametricTorusScreenSpaceSize(in out LodInfos lodInfos, float R, float r) {
    lodInfos.minBound = vec3(-(R + r), -(R + r), -r);
    lodInfos.maxBound = vec3((R + r), (R + r), r);
}

void parametricSphereScreenSpaceSize(in out LodInfos lodInfos, float radius) {
    lodInfos.minBound = vec3(-radius);
    lodInfos.maxBound = vec3(radius);
}

void parametricMobiusStripScreenSpaceSize(in out LodInfos lodInfos, float width) { // not very accurate
    lodInfos.minBound = vec3(-width * 0.5, -width * 0.5, -width * 0.5);
    lodInfos.maxBound = vec3(width, width, width * 0.5);
}

void parametricKleinBottleScreenSpaceSize(in out LodInfos lodInfos, float a) { // pretty good
    lodInfos.minBound = vec3(-3.0 * a, -2.0 * a, -1.5 * a);
    lodInfos.maxBound = vec3(2.0 * a, 3.0 * a, 1.5 * a);
}

void parametricHyperbolicParaboloidScreenSpaceSize(in out LodInfos lodInfos, float a, float b) {
    lodInfos.minBound = vec3(-2.0 * a, -2.0 * b, -(a * a + b * b));
    lodInfos.maxBound = vec3(2.0 * a, 2.0 * b, (a * a + b * b));
}

void parametricHelicoidScreenSpaceSize(in out LodInfos lodInfos, float a, float b) {
    lodInfos.minBound = vec3(-a * 3.0 * PI, -a * 3.5 * PI, 0);
    lodInfos.maxBound = vec3(a * 4.0 * PI, a * 2.5 * PI, 2.0 * b);
}

void parametricConeScreenSpaceSize(in out LodInfos lodInfos, float height, float radius) {
    lodInfos.minBound = vec3(-radius, -radius, 0.0);
    lodInfos.maxBound = vec3(radius, radius, height * 8.0);
}

void parametricCylinderScreenSpaceSize(in out LodInfos lodInfos, float height, float radius) {
    lodInfos.minBound = vec3(-radius, -radius, -height * 0.5);
    lodInfos.maxBound = vec3(radius, radius, height * 0.5);
}

void parametricEggScreenSpaceSize(in out LodInfos lodInfos, float a, float b) {
    lodInfos.minBound = vec3(-a, -a, -b);
    lodInfos.maxBound = vec3(a, a, b);
}

#endif // PARAMETRIC_SURFACES_GLSL