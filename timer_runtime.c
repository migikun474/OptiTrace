#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_FUNCTIONS 100
#define MAX_CALL_DEPTH 1024

// Stores accumulated statistics for each function.
struct FunctionStats {
    char *function_name;
    uint64_t call_count;
    uint64_t total_time_ns;
};

// Stores the start time of one active function call.
struct ActiveCall {
    const char *function_name;
    struct timespec start_time;
};

// Global statistics.
struct FunctionStats function_stats[MAX_FUNCTIONS];
int function_count = 0;
// Stack for active function calls.
struct ActiveCall call_stack[MAX_CALL_DEPTH];
int stack_depth = 0;
// Prevent registering the report function multiple times.
int report_registered = 0;
// Find an existing statistics entry or create a new one.
struct FunctionStats *find_or_create_stats(const char *function_name) {
    for (int i = 0; i < function_count; i++) {
        if (strcmp(function_stats[i].function_name, function_name) == 0) {
            return &function_stats[i];
        }
    }

    if (function_count >= MAX_FUNCTIONS) {
        return NULL;
    }
    struct FunctionStats *stats = &function_stats[function_count];
    stats->function_name = strdup(function_name);
    if (stats->function_name == NULL) {
        return NULL;
    }

    stats->call_count = 0;
    stats->total_time_ns = 0;

    function_count++;

    return stats;
}
// Print the final timing report.
void print_report(void) {
    printf("\n========== Function Timing Report ==========\n");
    uint64_t total_time_ns = 0;
    for (int i = 0; i < function_count; i++) {
        printf(
            "Function: %-20s Calls: %llu Total time: %llu ns\n",
            function_stats[i].function_name,
            (unsigned long long)function_stats[i].call_count,
            (unsigned long long)function_stats[i].total_time_ns
        );
        total_time_ns += function_stats[i].total_time_ns;
    }
    printf("--------------------------------------------\n");
    printf("Sum of function times: %llu ns\n",
           (unsigned long long)total_time_ns);

    printf("============================================\n");

    for (int i = 0; i < function_count; i++) {
        free(function_stats[i].function_name);
    }
}
// Called when a function begins.
void timer_start(const char *function_name) {
    // Ensure the report is printed when the program exits.
    if (!report_registered) {
        atexit(print_report);
        report_registered = 1;
    }
    // Find or create this function's statistics.
    struct FunctionStats *stats =
        find_or_create_stats(function_name);
    if (stats == NULL) {
        fprintf(stderr, "Could not record function statistics.\n");
        return;
    }
    // Count this invocation.
    stats->call_count++;
    // Ensure the call stack has enough space.
    if (stack_depth >= MAX_CALL_DEPTH) {
        fprintf(stderr, "Function call stack limit exceeded.\n");
        return;
    }
    // Save the function name and its start time on the stack.
    struct ActiveCall *call = &call_stack[stack_depth];
    call->function_name = function_name;
    if (clock_gettime(CLOCK_MONOTONIC, &call->start_time) != 0) {
        fprintf(stderr, "Could not read start time.\n");
        return;
    }

    stack_depth++;
}
// Called when a function returns.
void timer_stop(const char *function_name) {
    if (stack_depth <= 0) {
        fprintf(stderr, "timer_stop called with an empty call stack.\n");
        return;
    }
    // Retrieve the most recently started function call.
    struct ActiveCall *call = &call_stack[stack_depth - 1];
    // Check that calls are being stopped in the correct order.
    if (strcmp(call->function_name, function_name) != 0) {
        fprintf(stderr,
                "Warning: function call stack mismatch for %s.\n",
                function_name);
        return;
    }
    // Record the end time.
    struct timespec end_time;
    if (clock_gettime(CLOCK_MONOTONIC, &end_time) != 0) {
        fprintf(stderr, "Could not read end time.\n");
        return;
    }
    // Calculate elapsed time in nanoseconds.
    long long elapsed_ns =
        (long long)(end_time.tv_sec - call->start_time.tv_sec)
            * 1000000000LL
        + (long long)end_time.tv_nsec
        - (long long)call->start_time.tv_nsec;
    // Add elapsed time to the function's accumulated statistics.
    for (int i = 0; i < function_count; i++) {
        if (strcmp(function_stats[i].function_name, function_name) == 0) {
            if (elapsed_ns > 0) {
                function_stats[i].total_time_ns +=
                    (uint64_t)elapsed_ns;
            }
            break;
        }
    }
    // Remove this invocation from the stack.
    stack_depth--;
}