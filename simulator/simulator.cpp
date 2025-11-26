/**
 * @file simulator.cpp
 * @brief PC-based simulator for water level sensor firmware
 *
 * Simulates the complete firmware behavior without hardware:
 * - Displays level changes in terminal
 * - Shows timing
 * - Simulates beep patterns
 * - Power measurements
 * - Runs in real-time or accelerated
 *
 * Compile: g++ simulator.cpp -o simulator
 * Run: ./simulator
 */

#include <iostream>
#include <iomanip>
#include <string>
#include <cstdint>
#include <ctime>

#ifdef _WIN32
    #include <windows.h>
    #define sleep_ms(x) Sleep(x)
#else
    #include <unistd.h>
    #define sleep_ms(x) usleep((x) * 1000)
#endif

using namespace std;

// Simulation configuration
constexpr bool REAL_TIME = false;  // Set to true for real-time (10s cycles)
constexpr int TIME_SCALE = 100;    // Speed multiplier (100x faster)

// Thresholds (femtofarads)
constexpr int16_t THRESHOLD_LOW_FF = 800;
constexpr int16_t THRESHOLD_VLOW_FF = 500;
constexpr int16_t THRESHOLD_CRIT_FF = 300;

// Test levels
struct TestLevel {
    int16_t c1, c2, c3;
    const char* name;
};

const TestLevel TEST_LEVELS[] = {
    {1200, 1100, 1000, "NORMAL (Full tank)"},
    {600, 1100, 1000, "LOW (Below threshold 1)"},
    {600, 400, 1000, "VERY-LOW (Below threshold 2)"},
    {600, 400, 200, "CRITICAL (Below threshold 3)"}
};

constexpr int NUM_LEVELS = sizeof(TEST_LEVELS) / sizeof(TEST_LEVELS[0]);

// Current consumption (microamps)
constexpr double CURRENT_SLEEP = 0.5;      // Sleep mode
constexpr double CURRENT_WAKE = 800.0;     // Active (sensor reading)
constexpr double CURRENT_BEEP = 50000.0;   // Beeping (50mA)

constexpr int WAKE_DURATION_MS = 30;       // Time to read sensor
constexpr int BEEP_ON_MS = 100;            // Beep duration
constexpr int BEEP_GAP_MS = 100;           // Gap between beeps

// Simulation state
uint8_t current_level_index = 0;
uint8_t last_level = 0;
uint32_t cycle_count = 0;
uint32_t total_time_ms = 0;
double total_charge_uAh = 0.0;

// Terminal colors
const char* COLOR_RESET = "\033[0m";
const char* COLOR_GREEN = "\033[32m";
const char* COLOR_YELLOW = "\033[33m";
const char* COLOR_RED = "\033[31m";
const char* COLOR_CYAN = "\033[36m";
const char* COLOR_MAGENTA = "\033[35m";

void clear_screen() {
    cout << "\033[2J\033[1;1H";
}

void print_header() {
    cout << COLOR_CYAN << "╔═══════════════════════════════════════════════════════════════╗\n";
    cout << "║     ATtiny202 Water Level Sensor - PC Simulator              ║\n";
    cout << "╚═══════════════════════════════════════════════════════════════╝\n" << COLOR_RESET;
    cout << "\n";
}

void print_status_bar() {
    int hours = total_time_ms / 3600000;
    int minutes = (total_time_ms % 3600000) / 60000;
    int seconds = (total_time_ms % 60000) / 1000;

    double avg_current_uA = (total_time_ms > 0) ? (total_charge_uAh * 1000.0 / (total_time_ms / 3600000.0)) : 0;

    cout << COLOR_MAGENTA << "┌───────────────────────────────────────────────────────────────┐\n";
    cout << "│ Runtime: " << setw(2) << setfill('0') << hours << ":"
         << setw(2) << minutes << ":" << setw(2) << seconds
         << "   Cycles: " << setw(5) << cycle_count
         << "   Avg Current: " << fixed << setprecision(2) << avg_current_uA << " µA" << " │\n";
    cout << "└───────────────────────────────────────────────────────────────┘\n" << COLOR_RESET;
}

uint8_t get_level(int16_t c1, int16_t c2, int16_t c3) {
    if (c3 < THRESHOLD_CRIT_FF) return 3;
    if (c2 < THRESHOLD_VLOW_FF) return 2;
    if (c1 < THRESHOLD_LOW_FF) return 1;
    return 0;
}

const char* get_level_name(uint8_t level) {
    switch(level) {
        case 0: return "NORMAL";
        case 1: return "LOW";
        case 2: return "VERY-LOW";
        case 3: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

const char* get_level_color(uint8_t level) {
    switch(level) {
        case 0: return COLOR_GREEN;
        case 1: return COLOR_YELLOW;
        case 2: return "\033[38;5;208m";  // Orange
        case 3: return COLOR_RED;
        default: return COLOR_RESET;
    }
}

void play_beep(int count) {
    cout << "  🔊 BEEP: ";
    for (int i = 0; i < count; i++) {
        cout << "♪";
        if (i < count - 1) cout << " ";
    }
    cout << " (" << count << " beeps)\n";

    // Add power consumption for beeping
    double beep_time_ms = count * (BEEP_ON_MS + BEEP_GAP_MS) - BEEP_GAP_MS;
    total_charge_uAh += (CURRENT_BEEP * beep_time_ms) / 3600000.0;
}

void simulate_cycle() {
    cycle_count++;

    const TestLevel& test = TEST_LEVELS[current_level_index];

    clear_screen();
    print_header();
    print_status_bar();

    cout << "\n" << COLOR_CYAN << "━━━━━━━━━━━━━━━━━━ CYCLE " << cycle_count << " ━━━━━━━━━━━━━━━━━━\n" << COLOR_RESET;

    // Show wake event
    cout << "\n⏰ WAKE EVENT (every 10 seconds)\n";
    cout << "  └─ RTC/PIT interrupt triggered\n";

    // Simulate power-on
    cout << "\n🔌 POWER MANAGEMENT\n";
    cout << "  └─ PWR_EN = HIGH (VDD_SW enabled)\n";
    cout << "  └─ FDC1004 + DRV8210 powered on\n";

    // Simulate sensor reading
    cout << "\n📡 SENSOR READING\n";
    cout << "  └─ Test Level: " << test.name << "\n";
    cout << "  └─ CIN1 - CIN4: " << test.c1 << " fF\n";
    cout << "  └─ CIN2 - CIN4: " << test.c2 << " fF\n";
    cout << "  └─ CIN3 - CIN4: " << test.c3 << " fF\n";

    // Determine level
    uint8_t level = get_level(test.c1, test.c2, test.c3);
    const char* color = get_level_color(level);

    cout << "\n💧 WATER LEVEL\n";
    cout << "  └─ Detected: " << color << "█ " << get_level_name(level) << " █" << COLOR_RESET << "\n";
    cout << "  └─ Previous: " << get_level_name(last_level) << "\n";

    // Check if level changed
    if (level != last_level && level > 0) {
        cout << "\n⚠️  LEVEL CHANGE DETECTED!\n";
        cout << "  └─ " << get_level_name(last_level) << " → " << color << get_level_name(level) << COLOR_RESET << "\n";

        // Play beep
        int beep_count = (level == 1) ? 2 : (level == 2) ? 3 : 5;
        play_beep(beep_count);
    } else {
        cout << "\n✓ No level change (no beep)\n";
    }

    last_level = level;

    // Power down
    cout << "\n🔌 POWER MANAGEMENT\n";
    cout << "  └─ PWR_EN = LOW (VDD_SW disabled)\n";
    cout << "  └─ TWI disabled, I2C pins high-Z\n";

    // Sleep
    cout << "\n💤 SLEEP MODE\n";
    cout << "  └─ STANDBY mode (RTC running)\n";
    cout << "  └─ Sleep current: " << CURRENT_SLEEP << " µA\n";
    cout << "  └─ Sleeping for 10 seconds...\n";

    // Add power consumption
    double wake_charge = (CURRENT_WAKE * WAKE_DURATION_MS) / 3600000.0;
    double sleep_charge = (CURRENT_SLEEP * (10000 - WAKE_DURATION_MS)) / 3600000.0;
    total_charge_uAh += wake_charge + sleep_charge;

    // Update time
    total_time_ms += 10000;

    cout << "\n" << COLOR_CYAN << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n" << COLOR_RESET;

    // Cycle through levels (3 cycles per level = 30 seconds)
    if (cycle_count % 3 == 0) {
        current_level_index = (current_level_index + 1) % NUM_LEVELS;

        if (current_level_index == 0) {
            cout << "\n" << COLOR_MAGENTA << "🔄 TEST CYCLE COMPLETE - Restarting...\n" << COLOR_RESET;
        }
    }
}

void print_battery_life_estimate() {
    double avg_current_uA = (total_time_ms > 0) ? (total_charge_uAh * 1000.0 / (total_time_ms / 3600000.0)) : 0;

    cout << "\n" << COLOR_CYAN << "╔═══════════════════════════════════════════════════════════════╗\n";
    cout << "║                    BATTERY LIFE ESTIMATE                      ║\n";
    cout << "╚═══════════════════════════════════════════════════════════════╝\n" << COLOR_RESET;

    cout << "\nAverage Current: " << fixed << setprecision(2) << avg_current_uA << " µA\n\n";

    // CR2032: 220 mAh
    double cr2032_hours = 220000.0 / avg_current_uA;
    double cr2032_years = cr2032_hours / 8760.0;

    // CR2477: 1000 mAh
    double cr2477_hours = 1000000.0 / avg_current_uA;
    double cr2477_years = cr2477_hours / 8760.0;

    cout << "CR2032 (220 mAh):\n";
    cout << "  └─ Estimated life: " << fixed << setprecision(1) << cr2032_years << " years\n";
    cout << "  └─ Practical life: ~" << (int)(cr2032_years * 0.5) << " years (50% derating)\n\n";

    cout << "CR2477 (1000 mAh):\n";
    cout << "  └─ Estimated life: " << fixed << setprecision(1) << cr2477_years << " years\n";
    cout << "  └─ Practical life: ~" << (int)(cr2477_years * 0.5) << " years (50% derating)\n\n";
}

int main() {
    clear_screen();
    print_header();

    cout << "Simulation Mode: " << (REAL_TIME ? "REAL-TIME" : "ACCELERATED") << "\n";
    if (!REAL_TIME) {
        cout << "Time Scale: " << TIME_SCALE << "x faster\n";
    }
    cout << "\nPress Ctrl+C to stop simulation\n\n";

    cout << "Starting simulation in 3 seconds...\n";
    sleep_ms(3000);

    // Run simulation
    int delay_ms = REAL_TIME ? 10000 : (10000 / TIME_SCALE);

    try {
        while (true) {
            simulate_cycle();

            // Show battery estimate every 100 cycles
            if (cycle_count % 100 == 0) {
                print_battery_life_estimate();
            }

            sleep_ms(delay_ms);
        }
    } catch (...) {
        cout << "\n\nSimulation stopped.\n";
        print_battery_life_estimate();
    }

    return 0;
}
