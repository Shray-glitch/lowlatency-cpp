#include "time_utils.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>


// Print an explanation when the test fails.
int fail(const char* message)
{
    std::cerr << "FAILED: " << message << '\n';
    return 1;
}


int main()
{
    // Confirm that the unit constants describe nanoseconds correctly.
    if (NANOS_TO_MICROS != 1'000) {
        return fail("One microsecond should contain 1,000 nanoseconds.");
    }

    if (NANOS_TO_MILLIS != 1'000'000) {
        return fail("One millisecond should contain 1,000,000 nanoseconds.");
    }

    if (NANOS_TO_SECS != 1'000'000'000) {
        return fail("One second should contain 1,000,000,000 nanoseconds.");
    }


    // A current Unix timestamp should be a positive value.
    if (getCurrentNanos() <= 0) {
        return fail("The wall-clock timestamp should be positive.");
    }


    // steady_clock is the clock used for elapsed-time measurements.
    const Nanos start = getSteadyNanos();

    std::this_thread::sleep_for(
        std::chrono::milliseconds(1)
    );

    const Nanos finish = getSteadyNanos();

    if (finish < start) {
        return fail("Steady time should never move backwards.");
    }

    if (finish - start <= 0) {
        return fail("The measured duration should be positive.");
    }


    std::string formatted_time;

    // The function updates and returns the same caller-owned string.
    std::string& returned =
        getCurrentTimeStr(formatted_time);

    if (&returned != &formatted_time) {
        return fail("The function should return the supplied string.");
    }

    // YYYY-MM-DD HH:MM:SS contains 19 characters at fixed positions.
    if (
        formatted_time.size() != 19 ||
        formatted_time[4] != '-' ||
        formatted_time[7] != '-' ||
        formatted_time[10] != ' ' ||
        formatted_time[13] != ':' ||
        formatted_time[16] != ':'
    )
    {
        return fail("The readable timestamp has an unexpected format.");
    }

    std::cout << "All time utility tests passed.\n";
    return 0;
}
