/*
 Copyright (C) 2010 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "mdl/Path.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tb::mdl
{
namespace
{
double parameter(const double t)
{
  if (!std::isfinite(t))
  {
    throw std::invalid_argument{"Path parameter must be finite"};
  }
  return std::clamp(t, 0.0, 1.0);
}

std::array<vm::vec3d, 4> controls(const Path& path, const size_t segment)
{
  const auto count = path.nodes.size();
  const auto next = (segment + 1) % count;
  const auto& start = path.nodes[segment];
  const auto& end = path.nodes[next];
  const auto automatic = [&](const size_t index) {
    const auto previous = index == 0 ? (path.closed ? count - 1 : 0) : index - 1;
    const auto following = index + 1 == count ? (path.closed ? 0 : index) : index + 1;
    return (path.nodes[following].position - path.nodes[previous].position) / 6.0;
  };
  if (path.kind == PathKind::Linear)
  {
    const auto delta = (end.position - start.position) / 3.0;
    return {start.position, start.position + delta, end.position - delta, end.position};
  }
  const auto handleOut =
    path.kind == PathKind::CatmullRom || start.handleMode == PathHandleMode::Auto
      ? start.position + automatic(segment)
      : start.handleOut.value_or(start.position);
  const auto handleIn =
    path.kind == PathKind::CatmullRom || end.handleMode == PathHandleMode::Auto
      ? end.position - automatic(next)
      : end.handleIn.value_or(end.position);
  return {start.position, handleOut, handleIn, end.position};
}

std::pair<size_t, double> segmentParameter(const Path& path, const double t)
{
  const auto scaled = parameter(t) * double(path.segmentCount());
  const auto segment = std::min(size_t(scaled), path.segmentCount() - 1);
  return {segment, scaled - double(segment)};
}

size_t sampleCount(const Path& path, const size_t subdivisions)
{
  const auto count = std::max(size_t{1}, subdivisions);
  if (path.segmentCount() > (std::numeric_limits<size_t>::max() - 1) / count)
  {
    throw std::length_error{"Too many path samples"};
  }
  return path.segmentCount() * count;
}
} // namespace

size_t Path::segmentCount() const
{
  return nodes.size() < 2 ? 0 : nodes.size() - (closed ? 0 : 1);
}

std::array<vm::vec3d, 4> Path::segmentControls(const size_t segment) const
{
  if (segment >= segmentCount())
  {
    throw std::out_of_range{"Invalid path segment"};
  }
  return controls(*this, segment);
}

vm::vec3d Path::sample(const double t) const
{
  parameter(t);
  if (segmentCount() == 0)
  {
    return nodes.empty() ? vm::vec3d{} : nodes.front().position;
  }
  const auto [segment, u] = segmentParameter(*this, t);
  const auto p = controls(*this, segment);
  const auto v = 1.0 - u;
  return v * v * v * p[0] + 3.0 * v * v * u * p[1] + 3.0 * v * u * u * p[2]
         + u * u * u * p[3];
}

vm::vec3d Path::tangent(const double t) const
{
  parameter(t);
  if (segmentCount() == 0)
  {
    return vm::vec3d{};
  }
  const auto [segment, u] = segmentParameter(*this, t);
  const auto p = controls(*this, segment);
  const auto v = 1.0 - u;
  const auto derivative = 3.0 * v * v * (p[1] - p[0]) + 6.0 * v * u * (p[2] - p[1])
                          + 3.0 * u * u * (p[3] - p[2]);
  const auto magnitude = vm::length(derivative);
  return magnitude > 0.0 ? derivative / magnitude : vm::vec3d{};
}

double Path::sampleRoll(const double t) const
{
  parameter(t);
  if (nodes.empty())
  {
    return 0.0;
  }
  if (segmentCount() == 0)
  {
    return nodes.front().roll;
  }
  const auto [segment, u] = segmentParameter(*this, t);
  const auto start = nodes[segment].roll;
  const auto end = nodes[(segment + 1) % nodes.size()].roll;
  return start + u * std::remainder(end - start, 360.0);
}

double Path::length(const size_t samplesPerSegment) const
{
  const auto count = sampleCount(*this, samplesPerSegment);
  auto previous = sample(0.0);
  auto result = 0.0;
  for (size_t i = 1; i <= count; ++i)
  {
    const auto current = sample(double(i) / double(count));
    result += vm::length(current - previous);
    previous = current;
  }
  return result;
}

vm::vec3d Path::sampleDistance(
  const double distance, const size_t samplesPerSegment) const
{
  if (!std::isfinite(distance))
  {
    throw std::invalid_argument{"Path distance must be finite"};
  }
  if (distance <= 0.0)
  {
    return sample(0.0);
  }
  const auto count = sampleCount(*this, samplesPerSegment);
  auto previous = sample(0.0);
  auto remaining = distance;
  for (size_t i = 1; i <= count; ++i)
  {
    const auto current = sample(double(i) / double(count));
    const auto step = vm::length(current - previous);
    if (step > 0.0 && remaining <= step)
    {
      return previous + (remaining / step) * (current - previous);
    }
    remaining -= step;
    previous = current;
  }
  return sample(1.0);
}

vm::vec3d Path::closestPoint(
  const vm::vec3d& position, const size_t samplesPerSegment) const
{
  const auto count = sampleCount(*this, samplesPerSegment);
  auto previous = sample(0.0);
  auto result = previous;
  auto best = vm::squared_length(result - position);
  for (size_t i = 1; i <= count; ++i)
  {
    const auto current = sample(double(i) / double(count));
    const auto delta = current - previous;
    const auto squaredLength = vm::squared_length(delta);
    const auto u =
      squaredLength > 0.0
        ? std::clamp(vm::dot(position - previous, delta) / squaredLength, 0.0, 1.0)
        : 0.0;
    const auto candidate = previous + u * delta;
    const auto squaredDistance = vm::squared_length(candidate - position);
    if (squaredDistance < best)
    {
      result = candidate;
      best = squaredDistance;
    }
    previous = current;
  }
  return result;
}

Path Path::transformed(const vm::mat4x4d& transformation) const
{
  auto result = *this;
  for (auto& node : result.nodes)
  {
    node.position = transformation * node.position;
    if (node.handleIn)
    {
      node.handleIn = transformation * *node.handleIn;
    }
    if (node.handleOut)
    {
      node.handleOut = transformation * *node.handleOut;
    }
  }
  return result;
}

} // namespace tb::mdl
