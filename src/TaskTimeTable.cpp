#include "TaskTimeTable.hpp"
#include <istream>
#include <vector>
#include <cassert>
#include <algorithm>

// Preconditions:
// - curve.points is nonempty
// - points are sorted by batch_size
// - no point has duration == -1
double interpolate(const TimingCurve& curve, int size) {
  assert(!curve.points.empty());
  if (size <= curve.points.front().batch_size) {
    return curve.points.front().duration;
  }

  if (curve.points.back().batch_size <= size) {
    return curve.points.back().duration;
  }
  
  for (std::size_t i = 0; i + 1 < curve.points.size(); ++i) {
    const TimingPoint& left = curve.points[i];
    const TimingPoint& right = curve.points[i + 1];

    if (left.batch_size <= size && size <= right.batch_size) {
      return left.duration 
        + (size - left.batch_size)
        * (right.duration - left.duration) 
        / (right.batch_size - left.batch_size);
    }
  }

  assert(false);
  return 0.0;
}

namespace {

using DurationMember = double TaskTimeRow::*;

void finalizeCurve(TimingCurve& curve) {
  assert(!curve.points.empty());
  std::sort(curve.points.begin(), curve.points.end(), [](const TimingPoint& a, const TimingPoint& b) {
    return a.batch_size < b.batch_size;
  });
}

TimingCurve makeTimingCurve(const TaskTimeTable& table, DurationMember member) {
  TimingCurve curve;
  curve.points.reserve(table.rows.size());
  for (const TaskTimeRow& row : table.rows) {
    const double duration = row.*member;

    if (duration != -1.0) {
      curve.points.push_back({row.batch_size, duration});
    }
  }

  finalizeCurve(curve);
  return curve;
}

} // namespace

TimingCurves buildTimingCurves(const TaskTimeTable& table) {
  return {
    .prefill_pre  = makeTimingCurve(table, &TaskTimeRow::prefill_pre),
    .prefill_proc = makeTimingCurve(table, &TaskTimeRow::prefill_proc),
    .prefill_post = makeTimingCurve(table, &TaskTimeRow::prefill_post),
    .decode_pre   = makeTimingCurve(table, &TaskTimeRow::decode_pre),
    .decode_proc  = makeTimingCurve(table, &TaskTimeRow::decode_proc),
    .decode_post  = makeTimingCurve(table, &TaskTimeRow::decode_post),
  };
}

TaskTimeTable readTaskTimeTable(std::istream& input) {
  int n;
  input >> n;
  TaskTimeTable table;
  table.rows.reserve(n);
  for (int i = 0; i < n; i++) {
    TaskTimeRow row{};
    input >> row.batch_size;
    input >> row.prefill_pre >> row.prefill_proc >> row.prefill_post;
    input >> row.decode_pre >> row.decode_proc >> row.decode_post;
    table.rows.push_back(row);
  }
  return table;
}