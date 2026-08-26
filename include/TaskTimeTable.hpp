#pragma once
#include <iosfwd>
#include <vector>

// Read an integer N, then N rows of 7 values; no response is expected:
// batch_size  prefill_pre  prefill_proc  prefill_post  decode_pre  decode_proc  decode_post

// batch_size:    the number of requests grouped into one task
// prefill:       the input stage
// decode:        output steps

// a more advanced scheduler will use the complete task-time table, including values between listed rows.

struct TaskTimeRow {
  int batch_size; // L_in: vals are distinct positive integers in [1, 4096]

  double prefill_pre;
  double prefill_proc;
  double prefill_post;

  double decode_pre;
  double decode_proc;
  double decode_post;
};

struct TaskTimeTable {
  std::vector<TaskTimeRow> rows;
};

struct TimingPoint {
  int batch_size;
  double duration;
};

struct TimingCurve {
  std::vector < TimingPoint > points;
};

struct TimingCurves {
  TimingCurve prefill_pre;
  TimingCurve prefill_proc;
  TimingCurve prefill_post;
  TimingCurve decode_pre;
  TimingCurve decode_proc;
  TimingCurve decode_post;
};

TaskTimeTable readTaskTimeTable(std::istream& input);
TimingCurves buildTimingCurves(const TaskTimeTable& table);
double interpolate(const TimingCurve& curve, int size);