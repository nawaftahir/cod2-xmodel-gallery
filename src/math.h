// Minimal vector / matrix / quaternion math for model transforms and the orbit camera.
// CoD uses an id-Tech Z-up space (X forward, Y left, Z up); the camera math below matches.
#pragma once
#include <cmath>
#include <cstring>

static inline float v3dot(const float *a, const float *b)
{ return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; }
static inline void v3cross(const float *a, const float *b, float *o)
{ o[0]=a[1]*b[2]-a[2]*b[1]; o[1]=a[2]*b[0]-a[0]*b[2]; o[2]=a[0]*b[1]-a[1]*b[0]; }
static inline float v3len(const float *v) { return sqrtf(v3dot(v, v)); }
static inline void v3norm(float *v) { float l=v3len(v); if(l>1e-6f){ v[0]/=l; v[1]/=l; v[2]/=l; } }
static inline void v3sub(const float *a, const float *b, float *o)
{ o[0]=a[0]-b[0]; o[1]=a[1]-b[1]; o[2]=a[2]-b[2]; }

typedef float mat4[16]; // column-major, GL layout

static inline void mat4_identity(mat4 m) { memset(m, 0, 64); m[0]=m[5]=m[10]=m[15]=1.f; }

static inline void mat4_mul(const mat4 a, const mat4 b, mat4 out)
{
    mat4 tmp;
    for(int j=0;j<4;j++) for(int i=0;i<4;i++) {
        float s=0; for(int k=0;k<4;k++) s += a[i+k*4]*b[k+j*4];
        tmp[i+j*4]=s;
    }
    memcpy(out, tmp, 64);
}

static inline void mat4_perspective(mat4 m, float fovy_rad, float aspect, float zn, float zf)
{
    memset(m, 0, 64);
    float f = 1.f/tanf(fovy_rad*0.5f);
    m[0]=f/aspect; m[5]=f;
    m[10]=(zf+zn)/(zn-zf); m[11]=-1.f;
    m[14]=2.f*zf*zn/(zn-zf);
}

static inline void mat4_lookat(mat4 m, const float *eye, const float *center, const float *up)
{
    float f[3], r[3], u[3];
    v3sub(center, eye, f); v3norm(f);
    v3cross(f, up, r); v3norm(r);
    v3cross(r, f, u);
    mat4_identity(m);
    m[0]=r[0]; m[4]=r[1]; m[8]=r[2];
    m[1]=u[0]; m[5]=u[1]; m[9]=u[2];
    m[2]=-f[0]; m[6]=-f[1]; m[10]=-f[2];
    m[12]=-v3dot(r, eye);
    m[13]=-v3dot(u, eye);
    m[14]=v3dot(f, eye);
}

// Quaternion stored [w,x,y,z].
static inline void quat_mul(const float *a, const float *b, float *o)
{
    o[0]=a[0]*b[0]-a[1]*b[1]-a[2]*b[2]-a[3]*b[3];
    o[1]=a[0]*b[1]+a[1]*b[0]+a[2]*b[3]-a[3]*b[2];
    o[2]=a[0]*b[2]-a[1]*b[3]+a[2]*b[0]+a[3]*b[1];
    o[3]=a[0]*b[3]+a[1]*b[2]-a[2]*b[1]+a[3]*b[0];
}

static inline void quat_rotvec(const float *q, const float *v, float *o)
{
    float tx=2.f*(q[2]*v[2]-q[3]*v[1]);
    float ty=2.f*(q[3]*v[0]-q[1]*v[2]);
    float tz=2.f*(q[1]*v[1]-q[2]*v[0]);
    o[0]=v[0]+q[0]*tx+q[2]*tz-q[3]*ty;
    o[1]=v[1]+q[0]*ty+q[3]*tx-q[1]*tz;
    o[2]=v[2]+q[0]*tz+q[1]*ty-q[2]*tx;
}
