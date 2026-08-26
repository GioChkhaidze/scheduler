#include "TaskTimeTable.hpp"

#include <cassert>
#include <cmath>
#include <initializer_list>
#include <sstream>

namespace {

constexpr double epsilon = 1e-9;

bool isEqual(double actual, double expected) {
  return std::abs(actual - expected) < epsilon;
}

void testParsesEveryColumn() {
  std::istringstream input{
    "2\n"
    "1 3.0 10.0 2.0 1.0 4.0 1.0\n"
    "4 6.0 20.0 5.0 2.0 8.0 3.0\n"
  };

  const TaskTimeTable table = readTaskTimeTable(input);

  assert(table.rows.size() == 2);

  const TaskTimeRow& first = table.rows[0];
  assert(first.batch_size == 1);
  assert(isEqual(first.prefill_pre, 3.0));
  assert(isEqual(first.prefill_proc, 10.0));
  assert(isEqual(first.prefill_post, 2.0));
  assert(isEqual(first.decode_pre, 1.0));
  assert(isEqual(first.decode_proc, 4.0));
  assert(isEqual(first.decode_post, 1.0));

  const TaskTimeRow& second = table.rows[1];
  assert(second.batch_size == 4);
  assert(isEqual(second.prefill_pre, 6.0));
  assert(isEqual(second.prefill_proc, 20.0));
  assert(isEqual(second.prefill_post, 5.0));
  assert(isEqual(second.decode_pre, 2.0));
  assert(isEqual(second.decode_proc, 8.0));
  assert(isEqual(second.decode_post, 3.0));
}

void testUnsortedRowsMissingValuesAndInputConsumption() {
  std::istringstream input{
    "3\n"
    "8 -1 80.0 8.0 4.0 -1 2.0\n"
    "1 1.0 -1 2.0 -1 5.0 1.0\n"
    "4 4.0 40.0 -1 2.0 10.0 -1\n"
    "777\n"
  };

  const TaskTimeTable table = readTaskTimeTable(input);

  assert(table.rows.size() == 3);
  assert(table.rows[0].batch_size == 8);
  assert(table.rows[1].batch_size == 1);
  assert(table.rows[2].batch_size == 4);
  assert(isEqual(table.rows[0].prefill_pre, -1.0));
  assert(isEqual(table.rows[0].prefill_proc, 80.0));
  assert(isEqual(table.rows[0].decode_proc, -1.0));
  assert(isEqual(table.rows[1].prefill_proc, -1.0));
  assert(isEqual(table.rows[1].decode_pre, -1.0));
  assert(isEqual(table.rows[2].prefill_post, -1.0));
  assert(isEqual(table.rows[2].decode_post, -1.0));

  int marker = 0;
  input >> marker;
  assert(marker == 777);
}

void testRepeatedParsesAreIndependent() {
  std::istringstream firstInput{
    "2\n"
    "1 1 2 3 4 5 6\n"
    "2 2 3 4 5 6 7\n"
  };
  std::istringstream secondInput{
    "2\n"
    "16 6 5 4 3 2 1\n"
    "32 7 6 5 4 3 2\n"
  };

  const TaskTimeTable first = readTaskTimeTable(firstInput);
  const TaskTimeTable second = readTaskTimeTable(secondInput);

  assert(first.rows.size() == 2);
  assert(second.rows.size() == 2);
  assert(first.rows[0].batch_size == 1);
  assert(second.rows[0].batch_size == 16);
}

struct ExpectedInterpolation {
  int size;
  double duration;
};

struct ExpectedTimingPoint {
  int batch_size;
  double duration;
};

void assertCurvePoints(
  const TimingCurve& curve,
  std::initializer_list<ExpectedTimingPoint> expected_points
) {
  assert(curve.points.size() == expected_points.size());

  std::size_t index = 0;
  for (const ExpectedTimingPoint& expected : expected_points) {
    const TimingPoint& actual = curve.points[index];
    assert(actual.batch_size == expected.batch_size);
    assert(isEqual(actual.duration, expected.duration));
    ++index;
  }
}

void assertInterpolationCases(
  const TimingCurve& curve,
  std::initializer_list<ExpectedInterpolation> cases
) {
  for (const ExpectedInterpolation& test_case : cases) {
    assert(isEqual(
      interpolate(curve, test_case.size),
      test_case.duration
    ));
  }
}

void testInterpolationAcrossEveryPointOfIrregularCurve() {
  const TimingCurve curve{{
    {1, 10.25},
    {4, 17.75},
    {9, 5.25},
    {12, 5.25},
    {20, 19.25},
  }};

  assertInterpolationCases(curve, {
    {1, 10.25},
    {2, 12.75},
    {3, 15.25},
    {4, 17.75},
    {5, 15.25},
    {6, 12.75},
    {7, 10.25},
    {8, 7.75},
    {9, 5.25},
    {10, 5.25},
    {11, 5.25},
    {12, 5.25},
    {13, 7.0},
    {14, 8.75},
    {15, 10.5},
    {16, 12.25},
    {17, 14.0},
    {18, 15.75},
    {19, 17.5},
    {20, 19.25},
  });
}

void testInterpolationClampsToNearestEndpoint() {
  const TimingCurve curve{{
    {4, 17.75},
    {9, 5.25},
    {20, 19.25},
  }};

  assertInterpolationCases(curve, {
    {1, 17.75},
    {3, 17.75},
    {4, 17.75},
    {20, 19.25},
    {21, 19.25},
    {4096, 19.25},
  });
}

void testInterpolationAcrossFullBatchSizeDomain() {
  const TimingCurve curve{{
    {1, 0.5},
    {7, 2.75},
    {64, 24.125},
    {511, 191.75},
    {4096, 1536.125},
  }};

  for (int size = 1; size <= 4096; ++size) {
    const double expected = 0.125 + 0.375 * size;
    assert(isEqual(interpolate(curve, size), expected));
  }
}

void testInterpolationWithOnePoint() {
  const TimingCurve curve{{{128, 7.125}}};

  assertInterpolationCases(curve, {
    {1, 7.125},
    {128, 7.125},
    {4096, 7.125},
  });
}

void testBuildTimingCurvesFiltersSortsAndInterpolates() {
  const TaskTimeTable table{{
    {8, -1.0, 80.0, 8.5, 24.0, -1.0, 12.0},
    {1, 3.0, -1.0, 1.5, -1.0, 0.5, 19.0},
    {4, 9.0, 40.0, -1.0, 12.0, 2.0, -1.0},
  }};
  const TimingCurves curves = buildTimingCurves(table);

  assertCurvePoints(curves.prefill_pre, {{1, 3.0}, {4, 9.0}});
  assertCurvePoints(curves.prefill_proc, {{4, 40.0}, {8, 80.0}});
  assertCurvePoints(curves.prefill_post, {{1, 1.5}, {8, 8.5}});
  assertCurvePoints(curves.decode_pre, {{4, 12.0}, {8, 24.0}});
  assertCurvePoints(curves.decode_proc, {{1, 0.5}, {4, 2.0}});
  assertCurvePoints(curves.decode_post, {{1, 19.0}, {8, 12.0}});

  assertInterpolationCases(curves.prefill_pre, {{2, 5.0}});
  assertInterpolationCases(curves.prefill_proc, {{6, 60.0}});
  assertInterpolationCases(curves.prefill_post, {{4, 4.5}});
  assertInterpolationCases(curves.decode_pre, {{6, 18.0}});
  assertInterpolationCases(curves.decode_proc, {{2, 1.0}});
  assertInterpolationCases(curves.decode_post, {{4, 16.0}});
}

void testBuildTimingCurvesWithOneAvailableSample() {
  const TaskTimeTable table{{
    {32, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0},
    {8, 7.125, 8.125, 9.125, 10.125, 11.125, 12.125},
  }};
  const TimingCurves curves = buildTimingCurves(table);

  assertCurvePoints(curves.prefill_pre, {{8, 7.125}});
  assertCurvePoints(curves.prefill_proc, {{8, 8.125}});
  assertCurvePoints(curves.prefill_post, {{8, 9.125}});
  assertCurvePoints(curves.decode_pre, {{8, 10.125}});
  assertCurvePoints(curves.decode_proc, {{8, 11.125}});
  assertCurvePoints(curves.decode_post, {{8, 12.125}});

  assert(isEqual(interpolate(curves.prefill_pre, 1), 7.125));
  assert(isEqual(interpolate(curves.decode_post, 4096), 12.125));
}

} // namespace

int main() {
  testParsesEveryColumn();
  testUnsortedRowsMissingValuesAndInputConsumption();
  testRepeatedParsesAreIndependent();
  testInterpolationAcrossEveryPointOfIrregularCurve();
  testInterpolationClampsToNearestEndpoint();
  testInterpolationAcrossFullBatchSizeDomain();
  testInterpolationWithOnePoint();
  testBuildTimingCurvesFiltersSortsAndInterpolates();
  testBuildTimingCurvesWithOneAvailableSample();
  return 0;
}
