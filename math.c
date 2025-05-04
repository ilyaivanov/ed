#pragma once

inline i32 MinI32(i32 v1, i32 v2) {
  return v1 < v2 ? v1 : v2;
}
inline i32 MaxI32(i32 v1, i32 v2) {
  return v1 > v2 ? v1 : v2;
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
