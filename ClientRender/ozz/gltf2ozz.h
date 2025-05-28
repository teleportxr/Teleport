//----------------------------------------------------------------------------//
//                                                                            //
// ozz-animation is hosted at http://github.com/guillaumeblanc/ozz-animation  //
// and distributed under the MIT License (MIT).                               //
//                                                                            //
// Copyright (c) Guillaume Blanc                                              //
//                                                                            //
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without limitation  //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included in //
// all copies or substantial portions of the Software.                        //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
//                                                                            //
//----------------------------------------------------------------------------//

#//---------------------------------------------------------------------------//
// Initial gltf2ozz implementation author: Alexander Dzhoganov                //
// https://github.com/guillaumeblanc/ozz-animation/pull/70                    //
//----------------------------------------------------------------------------//

// Adapted from ozz-animation/src/animation/offline/gltf/gltf2ozz.cc

#include <cassert>
#include <cstring>

#include "ozz/animation/offline/raw_animation_utils.h"
#include "ozz/animation/offline/tools/import2ozz.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/containers/map.h"
#include "ozz/base/containers/set.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/log.h"
#include "ozz/base/maths/math_ex.h"
#include "ozz/base/maths/simd_math.h"


namespace {
template <typename _VectorType>
bool FixupNames(_VectorType& _data, const char* _pretty_name,
                const char* _prefix_name) {
  ozz::set<std::string> names;
  for (size_t i = 0; i < _data.size(); ++i) {
    bool renamed = false;
    typename _VectorType::const_reference data = _data[i];

    std::string name(data.name.c_str());

    // Fixes unnamed animations.
    if (name.length() == 0) {
      renamed = true;
      name = _prefix_name;
      name += std::to_string(i);
    }

    // Fixes duplicated names, while it has duplicates
    for (auto it = names.find(name); it != names.end(); it = names.find(name)) {
      renamed = true;
      name += "_";
      name += std::to_string(i);
    }

    // Update names index.
    if (!names.insert(name).second) {
      assert(false && "Algorithm must ensure no duplicated animation names.");
    }

    if (renamed) {
      ozz::log::LogV() << _pretty_name << " #" << i << " with name \""
                       << data.name << "\" was renamed to \"" << name
                       << "\" in order to avoid duplicates." << std::endl;

      // Actually renames tinygltf data.
      _data[i].name = name;
    }
  }

  return true;
}

// Returns the address of a gltf buffer given an accessor.
// Performs basic checks to ensure the data is in the correct format
template <typename T>
ozz::span<const T> BufferView(const tinygltf::Model& _model,
                              const tinygltf::Accessor& _accessor) {
  const int32_t component_size =
      tinygltf::GetComponentSizeInBytes(_accessor.componentType);
  const int32_t element_size =
      component_size * tinygltf::GetNumComponentsInType(_accessor.type);
  if (element_size != sizeof(T)) {
    ozz::log::Err() << "Invalid buffer view access. Expected element size '"
                    << sizeof(T) << " got " << element_size << " instead."
                    << std::endl;
    return ozz::span<const T>();
  }

  const tinygltf::BufferView& bufferView =
      _model.bufferViews[_accessor.bufferView];
  const tinygltf::Buffer& buffer = _model.buffers[bufferView.buffer];
  const T* begin = reinterpret_cast<const T*>(
      buffer.data.data() + bufferView.byteOffset + _accessor.byteOffset);
  return ozz::span<const T>(begin, _accessor.count);
}

// Samples a linear animation channel
// There is an exact mapping between gltf and ozz keyframes so we just copy
// everything over.
template <typename _KeyframesType>
bool SampleLinearChannel(const tinygltf::Model& _model,
                         const tinygltf::Accessor& _output,
                         const ozz::span<const float>& _timestamps,
                         _KeyframesType* _keyframes) {
  const size_t gltf_keys_count = _output.count;

  if (gltf_keys_count == 0) {
    _keyframes->clear();
    return true;
  }

  typedef typename _KeyframesType::value_type::Value ValueType;
  const ozz::span<const ValueType> values =
      BufferView<ValueType>(_model, _output);
  if (values.size_bytes() / sizeof(ValueType) != gltf_keys_count ||
      _timestamps.size() != gltf_keys_count) {
    ozz::log::Err() << "gltf format error, inconsistent number of keys."
                    << std::endl;
    return false;
  }

  _keyframes->reserve(_output.count);
  for (size_t i = 0; i < _output.count; ++i) {
    const typename _KeyframesType::value_type key{_timestamps[i], values[i]};
    _keyframes->push_back(key);
  }

  return true;
}

// Samples a step animation channel
// There are twice-1 as many ozz keyframes as gltf keyframes
template <typename _KeyframesType>
bool SampleStepChannel(const tinygltf::Model& _model,
                       const tinygltf::Accessor& _output,
                       const ozz::span<const float>& _timestamps,
                       _KeyframesType* _keyframes) {
  const size_t gltf_keys_count = _output.count;

  if (gltf_keys_count == 0) {
    _keyframes->clear();
    return true;
  }

  typedef typename _KeyframesType::value_type::Value ValueType;
  const ozz::span<const ValueType> values =
      BufferView<ValueType>(_model, _output);
  if (values.size_bytes() / sizeof(ValueType) != gltf_keys_count ||
      _timestamps.size() != gltf_keys_count) {
    ozz::log::Err() << "gltf format error, inconsistent number of keys."
                    << std::endl;
    return false;
  }

  // A step is created with 2 consecutive keys. Last step is a single key.
  size_t numKeyframes = gltf_keys_count * 2 - 1;
  _keyframes->resize(numKeyframes);

  for (size_t i = 0; i < _output.count; i++) {
    typename _KeyframesType::reference key = _keyframes->at(i * 2);
    key.time = _timestamps[i];
    key.value = values[i];

    if (i < _output.count - 1) {
      typename _KeyframesType::reference next_key = _keyframes->at(i * 2 + 1);
      next_key.time = nexttowardf(_timestamps[i + 1], 0.f);
      next_key.value = values[i];
    }
  }

  return true;
}

// Samples a hermite spline in the form
// p(t) = (2t^3 - 3t^2 + 1)p0 + (t^3 - 2t^2 + t)m0 + (-2t^3 + 3t^2)p1 + (t^3 -
// t^2)m1 where t is a value between 0 and 1 p0 is the starting point at t = 0
// m0 is the scaled starting tangent at t = 0
// p1 is the ending point at t = 1
// m1 is the scaled ending tangent at t = 1
// p(t) is the resulting point value
template <typename T>
T SampleHermiteSpline(float _alpha, const T& p0, const T& m0, const T& p1,
                      const T& m1) {
  assert(_alpha >= 0.f && _alpha <= 1.f);

  const float t1 = _alpha;
  const float t2 = _alpha * _alpha;
  const float t3 = t2 * _alpha;

  // a = 2t^3 - 3t^2 + 1
  const float a = 2.0f * t3 - 3.0f * t2 + 1.0f;
  // b = t^3 - 2t^2 + t
  const float b = t3 - 2.0f * t2 + t1;
  // c = -2t^3 + 3t^2
  const float c = -2.0f * t3 + 3.0f * t2;
  // d = t^3 - t^2
  const float d = t3 - t2;

  // p(t) = a * p0 + b * m0 + c * p1 + d * m1
  T pt = p0 * a + m0 * b + p1 * c + m1 * d;
  return pt;
}

// Samples a cubic-spline channel
// the number of keyframes is determined from the animation duration and given
// sample rate
template <typename _KeyframesType>
bool SampleCubicSplineChannel(const tinygltf::Model& _model,
                              const tinygltf::Accessor& _output,
                              const ozz::span<const float>& _timestamps,
                              float _sampling_rate, float _duration,
                              _KeyframesType* _keyframes) {
  (void)_duration;

  assert(_output.count % 3 == 0);
  size_t gltf_keys_count = _output.count / 3;

  if (gltf_keys_count == 0) {
    _keyframes->clear();
    return true;
  }

  typedef typename _KeyframesType::value_type::Value ValueType;
  const ozz::span<const ValueType> values =
      BufferView<ValueType>(_model, _output);
  if (values.size_bytes() / (sizeof(ValueType) * 3) != gltf_keys_count ||
      _timestamps.size() != gltf_keys_count) {
    ozz::log::Err() << "gltf format error, inconsistent number of keys."
                    << std::endl;
    return false;
  }

  // Iterate keyframes at _sampling_rate steps, between first and last time
  // stamps.
  ozz::animation::offline::FixedRateSamplingTime fixed_it(
      _timestamps[gltf_keys_count - 1] - _timestamps[0], _sampling_rate);
  _keyframes->resize(fixed_it.num_keys());
  size_t cubic_key0 = 0;
  for (size_t k = 0; k < fixed_it.num_keys(); ++k) {
    const float time = fixed_it.time(k) + _timestamps[0];

    // Creates output key.
    typename _KeyframesType::value_type key;
    key.time = time;

    // Makes sure time is in between the correct cubic keyframes.
    while (_timestamps[cubic_key0 + 1] < time) {
      cubic_key0++;
    }
    assert(_timestamps[cubic_key0] <= time &&
           time <= _timestamps[cubic_key0 + 1]);

    // Interpolate cubic key
    const float t0 = _timestamps[cubic_key0];      // keyframe before time
    const float t1 = _timestamps[cubic_key0 + 1];  // keyframe after time
    const float alpha = (time - t0) / (t1 - t0);
    const ValueType& p0 = values[cubic_key0 * 3 + 1];
    const ValueType m0 = values[cubic_key0 * 3 + 2] * (t1 - t0);
    const ValueType& p1 = values[(cubic_key0 + 1) * 3 + 1];
    const ValueType m1 = values[(cubic_key0 + 1) * 3] * (t1 - t0);
    key.value = SampleHermiteSpline(alpha, p0, m0, p1, m1);

    // Pushes interpolated key.
    _keyframes->at(k) = key;
  }

  return true;
}

template <typename _KeyframesType>
bool SampleChannel(const tinygltf::Model& _model,
                   const std::string& _interpolation,
                   const tinygltf::Accessor& _output,
                   const ozz::span<const float>& _timestamps,
                   float _sampling_rate, float _duration,
                   _KeyframesType* _keyframes) {
  bool valid = false;
  if (_interpolation == "LINEAR") {
    valid = SampleLinearChannel(_model, _output, _timestamps, _keyframes);
  } else if (_interpolation == "STEP") {
    valid = SampleStepChannel(_model, _output, _timestamps, _keyframes);
  } else if (_interpolation == "CUBICSPLINE") {
    valid = SampleCubicSplineChannel(_model, _output, _timestamps,
                                     _sampling_rate, _duration, _keyframes);
  } else {
    ozz::log::Err() << "Invalid or unknown interpolation type '"
                    << _interpolation << "'." << std::endl;
    valid = false;
  }

  // Check if sorted (increasing time, might not be stricly increasing).
  if (valid) {
    valid = std::is_sorted(_keyframes->begin(), _keyframes->end(),
                           [](typename _KeyframesType::const_reference _a,
                              typename _KeyframesType::const_reference _b) {
                             return _a.time < _b.time;
                           });
    if (!valid) {
      ozz::log::Log()
          << "gltf format error, keyframes are not sorted in increasing order."
          << std::endl;
    }
  }

  // Remove keyframes with strictly equal times, keeping the first one.
  if (valid) {
    auto new_end = std::unique(_keyframes->begin(), _keyframes->end(),
                               [](typename _KeyframesType::const_reference _a,
                                  typename _KeyframesType::const_reference _b) {
                                 return _a.time == _b.time;
                               });
    if (new_end != _keyframes->end()) {
      _keyframes->erase(new_end, _keyframes->end());

      ozz::log::Log() << "gltf format error, keyframe times are not unique. "
                         "Imported data were modified to remove keyframes at "
                         "consecutive equivalent times."
                      << std::endl;
    }
  }
  return valid;
}

ozz::animation::offline::RawAnimation::TranslationKey
CreateTranslationRestPoseKey(const tinygltf::Node& _node) {
  ozz::animation::offline::RawAnimation::TranslationKey key;
  key.time = 0.0f;

  if (_node.translation.empty()) {
    key.value = ozz::math::Float3::zero();
  } else {
    key.value = ozz::math::Float3(static_cast<float>(_node.translation[0]),
                                  static_cast<float>(_node.translation[1]),
                                  static_cast<float>(_node.translation[2]));
  }

  return key;
}

ozz::animation::offline::RawAnimation::RotationKey CreateRotationRestPoseKey(
    const tinygltf::Node& _node) {
  ozz::animation::offline::RawAnimation::RotationKey key;
  key.time = 0.0f;

  if (_node.rotation.empty()) {
    key.value = ozz::math::Quaternion::identity();
  } else {
    key.value = ozz::math::Quaternion(static_cast<float>(_node.rotation[0]),
                                      static_cast<float>(_node.rotation[1]),
                                      static_cast<float>(_node.rotation[2]),
                                      static_cast<float>(_node.rotation[3]));
  }
  return key;
}

ozz::animation::offline::RawAnimation::ScaleKey CreateScaleRestPoseKey(
    const tinygltf::Node& _node) {
  ozz::animation::offline::RawAnimation::ScaleKey key;
  key.time = 0.0f;

  if (_node.scale.empty()) {
    key.value = ozz::math::Float3::one();
  } else {
    key.value = ozz::math::Float3(static_cast<float>(_node.scale[0]),
                                  static_cast<float>(_node.scale[1]),
                                  static_cast<float>(_node.scale[2]));
  }
  return key;
}

// Creates the default transform for a gltf node
bool CreateNodeTransform(const tinygltf::Node& _node,
                         ozz::math::Transform* _transform) {
  *_transform = ozz::math::Transform::identity();
  if (!_node.matrix.empty()) {
    const ozz::math::Float4x4 matrix = {
        {ozz::math::simd_float4::Load(static_cast<float>(_node.matrix[0]),
                                      static_cast<float>(_node.matrix[1]),
                                      static_cast<float>(_node.matrix[2]),
                                      static_cast<float>(_node.matrix[3])),
         ozz::math::simd_float4::Load(static_cast<float>(_node.matrix[4]),
                                      static_cast<float>(_node.matrix[5]),
                                      static_cast<float>(_node.matrix[6]),
                                      static_cast<float>(_node.matrix[7])),
         ozz::math::simd_float4::Load(static_cast<float>(_node.matrix[8]),
                                      static_cast<float>(_node.matrix[9]),
                                      static_cast<float>(_node.matrix[10]),
                                      static_cast<float>(_node.matrix[11])),
         ozz::math::simd_float4::Load(static_cast<float>(_node.matrix[12]),
                                      static_cast<float>(_node.matrix[13]),
                                      static_cast<float>(_node.matrix[14]),
                                      static_cast<float>(_node.matrix[15]))}};

    if (!ToAffine(matrix, _transform)) {
      ozz::log::Err() << "Failed to extract transformation from node \""
                      << _node.name << "\"." << std::endl;
      return false;
    }
    return true;
  }

  if (!_node.translation.empty()) {
    _transform->translation =
        ozz::math::Float3(static_cast<float>(_node.translation[0]),
                          static_cast<float>(_node.translation[1]),
                          static_cast<float>(_node.translation[2]));
  }
  if (!_node.rotation.empty()) {
    _transform->rotation =
        ozz::math::Quaternion(static_cast<float>(_node.rotation[0]),
                              static_cast<float>(_node.rotation[1]),
                              static_cast<float>(_node.rotation[2]),
                              static_cast<float>(_node.rotation[3]));
  }
  if (!_node.scale.empty()) {
    _transform->scale = ozz::math::Float3(static_cast<float>(_node.scale[0]),
                                          static_cast<float>(_node.scale[1]),
                                          static_cast<float>(_node.scale[2]));
  }

  return true;
}
}  // namespace
