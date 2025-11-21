#!/bin/bash

# Test script for Homework 4: Concurrency

echo "======================================"
echo "Homework 4 Concurrency Tests"
echo "======================================"
echo ""

# Test with different thread counts
for threads in 1 2 4 8; do
    echo "========================================="
    echo "Testing with $threads thread(s)"
    echo "========================================="
    echo ""

    echo "--- Original (unsafe) ---"
    ./parallel_hashtable $threads
    echo ""

    echo "--- Mutex ---"
    ./parallel_mutex $threads
    echo ""

    echo "--- Spinlock ---"
    ./parallel_spin $threads
    echo ""

    echo "--- Optimized (per-bucket mutex) ---"
    ./parallel_mutex_opt $threads
    echo ""
    echo ""
done

echo "======================================"
echo "All tests completed!"
echo "======================================"
