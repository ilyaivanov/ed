#pragma once
#include <math.h>

typedef struct Spring {
  float target;
  float velocity;
  float current;
  // u32 isDone;
} Spring;

float stiffness;
float damping;

void InitAnimations() {
  stiffness = 600;
  damping = 1.7 * sqrtf(stiffness);
}

void UpdateSpring(Spring *spring, float deltaSec) {
  float displacement = spring->target - spring->current;
  float springForce = displacement * stiffness;
  float dampingForce = spring->velocity * damping;

  spring->velocity += (springForce - dampingForce) * deltaSec;
  spring->current += spring->velocity * deltaSec;

  // if (Math.abs(spring.current - spring.target) < 0.1) {
  //     spring.isDone = true;
  //     spring.current = spring.target;
  // }
}
