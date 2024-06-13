#pragma once

#include <vendor/glm/vec3.hpp>

namespace librii::math {

//! Axis-aligned bounding box
//!
struct AABB {
  void expandBound(const AABB& other) {
    if (other.min.x < min.x)
      min.x = other.min.x;
    if (other.min.y < min.y)
      min.y = other.min.y;
    if (other.min.z < min.z)
      min.z = other.min.z;
    if (other.max.x > max.x)
      max.x = other.max.x;
    if (other.max.y > max.y)
      max.y = other.max.y;
    if (other.max.z > max.z)
      max.z = other.max.z;
  }

  bool operator==(const AABB& rhs) const = default;

  glm::vec3 min{0.0f};
  glm::vec3 max{0.0f};
};

} // namespace librii::math
