#pragma once

// C++ Standard Library
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>
#include <utility>
#include <algorithm>
#include <execution>
#include <functional>
#include <chrono>
#include <random>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <future>
#include <optional>
#include <any>
#include <variant>
#include <stdexcept>
#include <cassert>
#include <cstddef>
#include <limits>
#include <cmath>

// Data Structures
#include <tuple>
#include <list>
#include <array>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <queue>
#include <stack>
#include <deque>

// Libs
#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/compatibility.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_structs.hpp>
#include <stbimage/stb_image.h>
#include <tinyobjloader/tiny_obj_loader.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <entt/entt.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
