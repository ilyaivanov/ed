#pragma once
#define _USE_MATH_DEFINES
#include "primitives.h"
#include <math.h>

V2 operator*(f32 a, V2 b) {
  return V2{a * b.x, a * b.y};
}

V2 operator*(V2 B, f32 A) {
  return A * B;
}

V2 operator/(V2 a, f32 b) {
  return V2{a.x / b, a.y / b};
}

V2 operator*(V2 a, V2 b) {
  return V2{a.x * b.x, a.y * b.y};
}

V2 operator/(f32 a, V2 b) {
  return V2{a / b.x, a / b.y};
}
V2 operator/(V2 a, V2 b) {
  return V2{a.x / b.x, a.y / b.y};
}
V2 operator+(V2 a, V2 b) {
  return V2{a.x + b.x, a.y + b.y};
}

V2 operator+(V2 a, f32 b) {
  return V2{a.x + b, a.y + b};
}

V2 operator+(f32 b, V2 a) {
  return V2{a.x + b, a.y + b};
}

// dot product
f32 operator|(V2 a, V2 b) {
  return a.x * b.x + a.y * b.y;
}

// cross product
f32 operator^(V2 a, V2 b) {
  return a.x * b.y - a.y * b.x;
}

V2& operator*=(V2& b, f32 a) {
  b = a * b;

  return b;
}

V2& operator+=(V2& b, f32 a) {
  b = a + b;

  return b;
}

V2& operator+=(V2& a, V2 b) {
  a = a + b;

  return a;
}

V2 operator-(V2 a) {
  return V2{-a.x, -a.y};
}

V2 operator-(V2 a, V2 b) {
  return V2{a.x - b.x, a.y - b.y};
}

V2 operator-(V2 a, f32 s) {
  return V2{a.x - s, a.y - s};
}

//
// V3
//

V3 operator*(f32 a, V3 b) {
  return V3{a * b.x, a * b.y, a * b.z};
}

V3 operator*(V3 B, f32 A) {
  return A * B;
}

V3 operator*(V3 a, V3 b) {
  return V3{a.x * b.x, a.y * b.y, a.z * b.z};
}

V3& operator*=(V3& b, f32 a) {
  b = a * b;

  return b;
}

V3 operator/(V3 a, f32 b) {
  return V3{a.x / b, a.y / b, a.z / b};
}

V3 operator+(V3 a, V3 b) {
  return V3{a.x + b.x, a.y + b.y, a.z + b.z};
}

V3 operator+(V3 a, f32 b) {
  return V3{a.x + b, a.y + b, a.z + b};
}

V3 operator+(f32 b, V3 a) {
  return V3{a.x + b, a.y + b, a.z + b};
}

V3& operator+=(V3& b, f32 a) {
  b = a + b;

  return b;
}

V3& operator+=(V3& a, V3 b) {
  a = a + b;

  return a;
}

V3 operator-(V3 a) {
  return V3{-a.x, -a.y, -a.z};
}

V3 operator-(V3 a, V3 b) {
  return V3{a.x - b.x, a.y - b.y, a.z - b.z};
}

V3 operator-(V3 a, f32 s) {
  return V3{a.x - s, a.y - s, a.z - s};
}
V3& operator-=(V3& a, V3 b) {
  a = a - b;

  return a;
}

//
// V4
//

V4 operator*(f32 a, V4 b) {
  return V4{a * b.x, a * b.y, a * b.z, a * b.w};
}

V4 operator*(V4 B, f32 A) {
  return A * B;
}

V4 operator*(V4 a, V4 b) {
  return V4{a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w};
}

V4& operator*=(V4& b, f32 a) {
  b = a * b;

  return b;
}

V4 operator/(V4 a, f32 b) {
  return V4{a.x / b, a.y / b, a.z / b, a.w / b};
}

V4 operator/(V4 a, V4 b) {
  return V4{a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w};
}

V4 operator+(V4 a, V4 b) {
  return V4{a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}

V4 operator+(V4 a, f32 b) {
  return V4{a.x + b, a.y + b, a.z + b, a.w + b};
}

V4 operator+(f32 b, V4 a) {
  return a + b;
}

V4& operator+=(V4& b, f32 a) {
  b = a + b;

  return b;
}

V4& operator+=(V4& a, V4 b) {
  a = a + b;

  return a;
}

V4 operator-(V4 a) {
  return V4{-a.x, -a.y, -a.z, -a.w};
}

V4 operator-(V4 a, V4 b) {
  return V4{a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
}

V4 operator-(V4 a, f32 s) {
  return V4{a.x - s, a.y - s, a.z - s, a.w - s};
}
//
//
//

//
// Mat4
//

V4 operator*(M4 m, V4 a) {
  return m.cols[0] * a.x + m.cols[1] * a.y + m.cols[2] * a.z + m.cols[3] * a.w;
}
M4 operator*(M4 m1, M4 m2) {
  M4 res;
  res.cols[0] = m1 * m2.cols[0];
  res.cols[1] = m1 * m2.cols[1];
  res.cols[2] = m1 * m2.cols[2];
  res.cols[3] = m1 * m2.cols[3];
  return res;
}

V4 operator*(V4 a, M4 m) {
  return m * a;
}

V4& operator*=(V4& a, M4 m) {
  a = a * m;

  return a;
}
M4& operator*=(M4& a, M4 m) {
  a = a * m;

  return a;
}

// clang-format off
M4 M4Identity() {
  return {
      V4{1, 0, 0, 0},
      V4{0, 1, 0, 0},
      V4{0, 0, 1, 0},
      V4{0, 0, 0, 1},
  };
}

M4 M4RotateZ(f32 r) {
  return M4{
    V4{cosf(r), -sinf(r), 0},
    V4{sinf(r),  cosf(r), 0},
    V4{      0,        0, 0},
    V4{      0,        0, 1}
  };
}

M4 M4RotateX(f32 r) {
  return M4{
        V4{1, 0, 0},
        V4{0, cosf(r), -sinf(r)},
        V4{0, sinf(r), cosf(r)},
            V4{0, sinf(r), cosf(r)}};
}

M4 M4RotateY(f32 r) {
  return M4{V4{cosf(r), 0, -sinf(r)}, V4{0, 1, 0}, V4{sinf(r), 0, cosf(r)},
            V4{sinf(r), 0, cosf(r)}};
}
M4 M4Move(V4 move) {
  return M4{
    V4{1, 0, 0, 0},
    V4{0, 1, 0, 0},
    V4{0, 0, 1, 0},
    move,
  };
}

m4 M4Translate(f32 X, f32 Y, f32 Z)
{
    m4 Result = M4Identity();
    Result.cols[3].xyz = V3{X, Y, Z};
    return Result;
}

m4 RotationMatrix(f32 X, f32 Y, f32 Z)
{
    m4 Result = {};

    m4 RotateX = M4Identity();
    RotateX.cols[1].y = cos(X);
    RotateX.cols[2].y = -sin(X);
    RotateX.cols[1].z = sin(X);
    RotateX.cols[2].z = cos(X);

    m4 RotateY = M4Identity();
    RotateY.cols[0].x = cos(Y);
    RotateY.cols[2].x = -sin(Y);
    RotateY.cols[0].z = sin(Y);
    RotateY.cols[2].z = cos(Y);

    m4 RotateZ = M4Identity();
    RotateZ.cols[0].x = cos(Z);
    RotateZ.cols[1].x = -sin(Z);
    RotateZ.cols[0].y = sin(Z);
    RotateZ.cols[1].y = cos(Z);

    Result = RotateZ * RotateY * RotateX;
    return Result;
}
// clang-format on
//
// Mat2
//

V2 operator*(M2 m, V2 a) {
  return m.cols[0] * a.x + m.cols[1] * a.y;
}
M2 operator*(M2 m1, M2 m2) {
  M2 res;
  res.cols[0] = m1 * m2.cols[0];
  res.cols[1] = m1 * m2.cols[1];
  return res;
}

V2 operator*(V2 a, M2 m) {
  return m * a;
}

V2& operator*=(V2& a, M2 m) {
  a = a * m;

  return a;
}
M2& operator*=(M2& a, M2 m) {
  a = a * m;

  return a;
}

//
// Mat3
//

V3 operator*(M3 m, V3 a) {
  return m.cols[0] * a.x + m.cols[1] * a.y + m.cols[2] * a.z;
}
M3 operator*(M3 m1, M3 m2) {
  M3 res;
  res.cols[0] = m1 * m2.cols[0];
  res.cols[1] = m1 * m2.cols[1];
  res.cols[2] = m1 * m2.cols[2];
  return res;
}

V3 operator*(V3 a, M3 m) {
  return m * a;
}

V3& operator*=(V3& a, M3 m) {
  a = a * m;

  return a;
}
M3& operator*=(M3& a, M3 m) {
  a = a * m;

  return a;
}

// clang-format off
M3 M3Rotate(f32 r) {
  return M3{
    V3{cosf(r), -sinf(r), 0},
    V3{sinf(r),  cosf(r), 0},
    V3{      0,        0, 1}
  };
}

M3 M3RotateZ(f32 r){
  return M3Rotate(r);
}
M3 M3RotateX(f32 r) {
  return M3{
    V3{1,       0,        0},
    V3{0, cosf(r), -sinf(r)},
    V3{0, sinf(r),  cosf(r)}
  };
}

M3 M3RotateY(f32 r) {
  return M3{
    V3{cosf(r), 0, -sinf(r)},
    V3{      0, 1,        0},
    V3{sinf(r), 0, cosf(r)}
  };
}

// clang-format on
M3 M3Move(V3 move) {
  return M3{V3{1, 0, 0}, V3{0, 1, 0}, move};
}

i32 MinI32(i32 v1, i32 v2) {
  return v1 < v2 ? v1 : v2;
}

inline i32 MaxI32(i32 v1, i32 v2) {
  return v1 > v2 ? v1 : v2;
}

f32 Min(f32 a, f32 b) {
  if (a < b)
    return a;
  else
    return b;
}

f32 Max(f32 a, f32 b) {
  if (a > b)
    return a;
  else
    return b;
}

i32 Clampi32(i32 val, i32 min, i32 max) {
  if (val < min)
    return min;
  if (val > max)
    return max;
  return val;
}

float Clamp(float val, float min, float max) {
  if (val < min)
    return min;
  if (val > max)
    return max;
  return val;
}
i32 Clampi(i32 val, i32 min, i32 max) {
  if (val < min)
    return min;
  if (val > max)
    return max;
  return val;
}

float V2Length(V2 v) {
  return sqrt(v.x * v.x + v.y * v.y);
}

float V3Length(V3 v) {
  return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

V2 V2Norm(V2 v) {
  f32 len = V2Length(v);
  if (len == 0)
    return V2{0, 0};
  return V2{v.x / len, v.y / len};
}

V3 V3Norm(V3 v) {
  f32 len = V3Length(v);
  if (len == 0)
    return V3{0, 0};
  return V3{v.x / len, v.y / len, v.z / len};
}

V2 V2Scale(V2 v, float s) {
  V2 result = {v.x * s, v.y * s};
  return result;
}

V2 ClampV2Len(V2 v, float minLen, float maxLen) {
  float len = V2Length(v);
  if (len == 0.0f)
    return v;

  float scale = Clamp(len, minLen, maxLen) / len;
  return v * scale;
}

inline i32 RoundI32(f32 v) {
  if (v < 0)
    return (i32)(v - 0.5);
  else
    return (i32)(v + 0.5);
}

inline u8 RoundU8(f32 v) {
  return (u8)(v + 0.5);
}

inline f32 lerp(f32 from, f32 to, f32 factor) {
  return from * (1 - factor) + to * factor;
}

V2 V2Lerp(V2 from, V2 to, f32 factor) {
  return V2{lerp(from.x, to.x, factor), lerp(from.y, to.y, factor)};
}
V3 V3Lerp(V3 from, V3 to, f32 factor) {
  return V3{lerp(from.x, to.x, factor), lerp(from.y, to.y, factor), lerp(from.z, to.z, factor)};
}

f32 easeOutSine(f32 v) {
  return sin((v * M_PI) / 2);
}

i32 CheckTwoSquareOverlap(V2 centerA, f32 sizeA, V2 centerB, f32 sizeB) {
  V2 bottomLeftA = centerA - sizeA / 2;
  V2 bottomLeftB = centerB - sizeB / 2;
  f32 leftA = bottomLeftA.x;
  f32 rightA = bottomLeftA.x + sizeA;
  f32 topA = bottomLeftA.y + sizeA;
  f32 bottomA = bottomLeftA.y;

  f32 leftB = bottomLeftB.x;
  f32 rightB = bottomLeftB.x + sizeB;
  f32 topB = bottomLeftB.y + sizeB;
  f32 bottomB = bottomLeftB.y;

  return (leftA < rightB && rightA > leftB && topA > bottomB && bottomA < topB);
}

i32 IsPointInsideSquare(V2 center, f32 size, V2 point) {
  V2 topLeft = center - size / 2;
  return point.x >= topLeft.x && point.x < (topLeft.x + size) && point.y >= topLeft.y &&
         point.y < (topLeft.y + size);
}

i32 IsPointInsideRect(V2 center, f32 width, f32 height, V2 point) {
  V2 topLeft = center - V2{width / 2.0f, height / 2.0f};
  return point.x >= topLeft.x && point.x < (topLeft.x + width) && point.y >= topLeft.y &&
         point.y < (topLeft.y + height);
}
// https://stackoverflow.com/a/565282/1283124
static bool LinesIntersect(V2 start1, V2 end1, V2 start2, V2 end2, V2* out) {
  V2 cmP = start2 - start1;
  V2 r = end1 - start1;
  V2 s = end2 - start2;

  f32 cmPxr = cmP ^ r;
  f32 cmPxs = cmP ^ s;
  f32 rxs = r ^ s;

  if (cmPxr == 0.0f) {
    // Lines are collinear, and so intersect if they have any overlap
    return ((start2.x - start1.x < 0.0f) != (start2.x - end1.x < 0.0f)) ||
           ((start2.y - start1.y < 0.0f) != (start2.y - end1.y < 0.0f));
  }

  if (rxs == 0.0f)
    return false; // Lines are parallel.

  f32 rxsr = 1.0f / rxs;
  f32 t = cmPxs * rxsr;
  f32 u = cmPxr * rxsr;

  if ((t >= 0.0f) && (t <= 1.0f) && (u >= 0.0f) && (u <= 1.0f)) {
    if (out)
      *out = V2Lerp(start2, end2, u);
    // *out = V2Lerp(start1, end1, t);
    return true;
  }
  return false;
}

/**
 * Return solutions for quadratic
 */
i32 quad(f32 a, f32 b, f32 c, f32* s1, f32* s2) {
  f32 sol = 0;
  if (fabs(a) < 1e-6) {
    if (fabs(b) < 1e-6) {
      if (fabs(c) < 1e-6) {
        *s1 = 0;
        *s2 = 0;
        return 1;
      }
    } else {
      *s1 = -c / b;
      *s2 = -c / b;
      return 1;
    }
  } else {
    f32 disc = b * b - 4 * a * c;
    if (disc >= 0) {
      disc = sqrtf(disc);
      a = 2 * a;
      *s1 = (-b - disc) / a;
      *s2 = (-b + disc) / a;
      return 1;
    }
  }
  return 0;
}

// https://stackoverflow.com/a/3487761/1283124
v2 intercept(v2 src, v2 dst, v2 dstVel, f32 bulletVel) {
  f32 tx = dst.x - src.x;
  f32 ty = dst.y - src.y;
  f32 tvx = dstVel.x;
  f32 tvy = dstVel.y;

  // Get quadratic equation components
  f32 a = tvx * tvx + tvy * tvy - bulletVel * bulletVel;
  f32 b = 2 * (tvx * tx + tvy * ty);
  f32 c = tx * tx + ty * ty;

  f32 t0;
  f32 t1;
  i32 found = quad(a, b, c, &t0, &t1);

  // Find smallest positive solution
  // let sol = null;
  if (found) {
    // const t0 = ts[0];
    // const t1 = ts[1];
    f32 t = fminf(t0, t1);
    if (t < 0)
      t = fmaxf(t0, t1);
    if (t > 0) {
      return V2{
          dst.x + dstVel.x * t,
          dst.y + dstVel.y * t,
      };
    }
  }

  return {};
}
