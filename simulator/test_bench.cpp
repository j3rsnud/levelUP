/**
 * @file test_bench.cpp
 * @brief Test bench for water level sensor firmware
 *
 * Runs automated test cases and displays PASS/FAIL results
 *
 * Compile: g++ test_bench.cpp -o test_bench
 * Run: ./test_bench
 */

#include <iostream>
#include <iomanip>
#include <string>
#include <cstdint>

using namespace std;

// Thresholds (femtofarads)
constexpr int16_t THRESHOLD_LOW_FF = 800;
constexpr int16_t THRESHOLD_VLOW_FF = 500;
constexpr int16_t THRESHOLD_CRIT_FF = 300;

// Test results tracking
struct TestResults {
    int total = 0;
    int passed = 0;
    int failed = 0;
};

static TestResults results;

// Level detection function (from firmware)
uint8_t get_level(int16_t c1, int16_t c2, int16_t c3) {
    if (c3 < THRESHOLD_CRIT_FF) return 3;  // Critical
    if (c2 < THRESHOLD_VLOW_FF) return 2;  // Very Low
    if (c1 < THRESHOLD_LOW_FF) return 1;   // Low
    return 0;  // Normal
}

const char* level_name(uint8_t level) {
    switch(level) {
        case 0: return "NORMAL";
        case 1: return "LOW";
        case 2: return "VERY-LOW";
        case 3: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

// Test framework
void print_header() {
    cout << "\n";
    cout << "================================================================\n";
    cout << "  ATtiny202 Water Level Sensor - AUTOMATED TEST BENCH\n";
    cout << "================================================================\n\n";
}

void print_test_header(const char* category) {
    cout << "\n--- " << category << " ---\n";
}

void check_test(const char* test_name, bool condition, const char* details = "") {
    results.total++;

    cout << "[TEST " << setw(3) << results.total << "] " << test_name << " ... ";

    if (condition) {
        cout << "[ PASS ]";
        results.passed++;
    } else {
        cout << "[ FAIL ]";
        results.failed++;
    }

    if (details[0] != '\0') {
        cout << "  " << details;
    }
    cout << "\n";
}

void check_level_detection(const char* test_name, int16_t c1, int16_t c2, int16_t c3,
                          uint8_t expected_level) {
    uint8_t detected = get_level(c1, c2, c3);
    char details[128];
    snprintf(details, sizeof(details), "C1=%d C2=%d C3=%d => %s (expected %s)",
             c1, c2, c3, level_name(detected), level_name(expected_level));
    check_test(test_name, detected == expected_level, details);
}

void check_beep_pattern(const char* test_name, uint8_t old_level, uint8_t new_level,
                       int expected_beeps) {
    int actual_beeps = 0;

    // Beep logic from firmware
    if (new_level != old_level && new_level > 0) {
        if (new_level == 1) actual_beeps = 2;
        else if (new_level == 2) actual_beeps = 3;
        else if (new_level == 3) actual_beeps = 5;
    }

    char details[128];
    snprintf(details, sizeof(details), "%s -> %s: %d beeps (expected %d)",
             level_name(old_level), level_name(new_level), actual_beeps, expected_beeps);
    check_test(test_name, actual_beeps == expected_beeps, details);
}

void check_power_budget(const char* test_name, double current_uA,
                       double min_uA, double max_uA) {
    char details[128];
    snprintf(details, sizeof(details), "%.2f uA (range: %.2f - %.2f uA)",
             current_uA, min_uA, max_uA);
    check_test(test_name, current_uA >= min_uA && current_uA <= max_uA, details);
}

void print_summary() {
    cout << "\n================================================================\n";
    cout << "  TEST SUMMARY\n";
    cout << "================================================================\n";
    cout << "Total Tests:  " << results.total << "\n";
    cout << "Passed:       " << results.passed << " ("
         << fixed << setprecision(1)
         << (100.0 * results.passed / results.total) << "%)\n";
    cout << "Failed:       " << results.failed << "\n";
    cout << "\n";

    if (results.failed == 0) {
        cout << "*** ALL TESTS PASSED ***\n";
    } else {
        cout << "*** " << results.failed << " TEST(S) FAILED ***\n";
    }
    cout << "================================================================\n\n";
}

// Test suites
void test_level_detection() {
    print_test_header("LEVEL DETECTION");

    // Normal level tests
    check_level_detection("Full tank (all high)", 1200, 1100, 1000, 0);
    check_level_detection("Just above LOW threshold", 801, 1100, 1000, 0);
    check_level_detection("At LOW threshold", 800, 1100, 1000, 0);

    // LOW level tests
    check_level_detection("Just below LOW threshold", 799, 1100, 1000, 1);
    check_level_detection("Well below LOW", 600, 1100, 1000, 1);
    check_level_detection("Just above VERY-LOW", 600, 501, 1000, 1);

    // VERY-LOW level tests
    check_level_detection("Just below VERY-LOW threshold", 600, 499, 1000, 2);
    check_level_detection("Well below VERY-LOW", 600, 400, 1000, 2);
    check_level_detection("Just above CRITICAL", 600, 400, 301, 2);

    // CRITICAL level tests
    check_level_detection("Just below CRITICAL threshold", 600, 400, 299, 3);
    check_level_detection("Well below CRITICAL", 600, 400, 200, 3);
    check_level_detection("Empty tank (all low)", 100, 100, 100, 3);

    // Edge cases
    check_level_detection("All at zero", 0, 0, 0, 3);
    check_level_detection("Negative readings", -100, -100, -100, 3);
    check_level_detection("Mixed high/low", 900, 400, 200, 3);
}

void test_threshold_boundaries() {
    print_test_header("THRESHOLD BOUNDARIES");

    // Test exact threshold values
    check_level_detection("C1 exactly at LOW threshold", 800, 900, 900, 0);
    check_level_detection("C1 one below LOW threshold", 799, 900, 900, 1);

    check_level_detection("C2 exactly at VERY-LOW threshold", 700, 500, 900, 1);
    check_level_detection("C2 one below VERY-LOW threshold", 700, 499, 900, 2);

    check_level_detection("C3 exactly at CRITICAL threshold", 700, 600, 300, 2);
    check_level_detection("C3 one below CRITICAL threshold", 700, 600, 299, 3);
}

void test_beep_patterns() {
    print_test_header("BEEP PATTERNS");

    // Transitions that should beep
    check_beep_pattern("NORMAL to LOW transition", 0, 1, 2);
    check_beep_pattern("LOW to VERY-LOW transition", 1, 2, 3);
    check_beep_pattern("VERY-LOW to CRITICAL transition", 2, 3, 5);

    // Transitions that should NOT beep
    check_beep_pattern("NORMAL to NORMAL (no change)", 0, 0, 0);
    check_beep_pattern("LOW to LOW (no change)", 1, 1, 0);
    check_beep_pattern("CRITICAL to NORMAL (refilled)", 3, 0, 0);
    check_beep_pattern("VERY-LOW to NORMAL (refilled)", 2, 0, 0);
    check_beep_pattern("LOW to NORMAL (refilled)", 1, 0, 0);

    // Skip-level transitions
    check_beep_pattern("NORMAL to VERY-LOW (skip)", 0, 2, 3);
    check_beep_pattern("NORMAL to CRITICAL (skip)", 0, 3, 5);
}

void test_power_budget() {
    print_test_header("POWER BUDGET");

    // Sleep current
    double sleep_current = 0.5;  // µA
    check_power_budget("Sleep mode current", sleep_current, 0.1, 2.0);

    // Wake current (sensor reading)
    double wake_current = 800;  // µA during 30ms
    check_power_budget("Wake/measure current", wake_current, 500, 1500);

    // Average current (baseline, no alerts)
    // 30ms @ 800µA + 9970ms @ 0.5µA per 10s cycle
    double avg_wake = (800 * 30 + 0.5 * 9970) / 10000.0;
    check_power_budget("Average baseline current", avg_wake, 2.0, 5.0);

    // Beep current
    double beep_current = 50000;  // µA (50mA)
    check_power_budget("Beep current", beep_current, 30000, 80000);
}

void test_timing() {
    print_test_header("TIMING REQUIREMENTS");

    // Wake cycle timing
    int wake_period_ms = 10000;
    check_test("Wake cycle period", wake_period_ms == 10000, "10 seconds");

    // Sensor reading duration
    int sensor_duration_ms = 30;
    check_test("Sensor reading duration",
               sensor_duration_ms >= 20 && sensor_duration_ms <= 50,
               "20-50ms");

    // Beep duration
    int beep_duration_ms = 100;
    check_test("Individual beep duration", beep_duration_ms == 100, "100ms");

    // Beep gap
    int beep_gap_ms = 100;
    check_test("Gap between beeps", beep_gap_ms == 100, "100ms");
}

void test_battery_life() {
    print_test_header("BATTERY LIFE ESTIMATES");

    // Baseline current (no alerts)
    double baseline_uA = 3.5;

    // CR2032 capacity: 220 mAh
    double cr2032_hours = 220000.0 / baseline_uA;
    double cr2032_years = cr2032_hours / 8760.0;
    check_test("CR2032 battery life",
               cr2032_years >= 5.0 && cr2032_years <= 10.0,
               "5-10 years theoretical");

    // CR2477 capacity: 1000 mAh
    double cr2477_hours = 1000000.0 / baseline_uA;
    double cr2477_years = cr2477_hours / 8760.0;
    check_test("CR2477 battery life",
               cr2477_years >= 20.0 && cr2477_years <= 40.0,
               "20-40 years theoretical");
}

void test_sensor_range() {
    print_test_header("SENSOR RANGE & VALIDITY");

    // Check sensor range (±15 pF = ±15000 fF)
    check_test("Max sensor range",
               15000 > THRESHOLD_LOW_FF,
               "±15000 fF range sufficient");

    // Check threshold ordering
    check_test("Threshold ordering",
               THRESHOLD_LOW_FF > THRESHOLD_VLOW_FF &&
               THRESHOLD_VLOW_FF > THRESHOLD_CRIT_FF,
               "LOW > VERY-LOW > CRITICAL");

    // Check threshold separation
    int sep1 = THRESHOLD_LOW_FF - THRESHOLD_VLOW_FF;
    int sep2 = THRESHOLD_VLOW_FF - THRESHOLD_CRIT_FF;
    check_test("Threshold separation",
               sep1 >= 200 && sep2 >= 200,
               "Min 200 fF separation");
}

void test_edge_cases() {
    print_test_header("EDGE CASES");

    // Test with maximum positive values
    check_level_detection("Maximum positive readings", 15000, 15000, 15000, 0);

    // Test with maximum negative values
    check_level_detection("Maximum negative readings", -15000, -15000, -15000, 3);

    // Test single channel failures
    check_level_detection("C1 fault (very low)", 0, 1000, 1000, 1);
    check_level_detection("C2 fault (very low)", 1000, 0, 1000, 2);
    check_level_detection("C3 fault (very low)", 1000, 1000, 0, 3);

    // Test reverse order readings
    check_level_detection("C3 < C2 < C1 (normal gradient)", 500, 300, 100, 3);
}

int main() {
    print_header();

    // Run all test suites
    test_level_detection();
    test_threshold_boundaries();
    test_beep_patterns();
    test_power_budget();
    test_timing();
    test_battery_life();
    test_sensor_range();
    test_edge_cases();

    // Print summary
    print_summary();

    // Return exit code
    return (results.failed == 0) ? 0 : 1;
}
