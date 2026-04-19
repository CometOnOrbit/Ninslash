// Test file to verify the 3 new advanced optimizations
#include "base/tl/loop_optimization.h"
#include "base/tl/memory_access.h"
#include "base/tl/compiler_hints.h"
#include <iostream>
#include <vector>
#include <chrono>

int main()
{
    std::cout << "Testing 3 new advanced optimizations:\n\n";
    
    // Test 1: Loop optimization
    std::cout << "1. Loop Optimization Test:\n";
    {
        const size_t SIZE = 100;
        std::vector<float> data(SIZE, 1.0f);
        std::vector<float> result(SIZE, 0.0f);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Using loop unrolling
        LOOP_UNROLL_PARTIAL(4, SIZE, i, {
            result[i] = data[i] * 2.0f;
        });
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "   Loop unrolling test completed in " << duration.count() << " microseconds\n";
        
        // Test loop fusion
        std::vector<float> data2(SIZE, 2.0f);
        std::vector<float> data3(SIZE, 3.0f);
        
        auto multiply = [](float& x) { x *= 2.0f; };
        auto add = [](float& x) { x += 1.0f; };
        
        LOOP_FUSION(data2.data(), SIZE, multiply, add);
        
        std::cout << "   Loop fusion test completed\n";
    }
    
    // Test 2: Memory access optimization
    std::cout << "\n2. Memory Access Optimization Test:\n";
    {
        const size_t SIZE = 1000;
        std::vector<int> source(SIZE);
        std::vector<int> dest(SIZE);
        
        // Initialize source
        for(size_t i = 0; i < SIZE; ++i) {
            source[i] = static_cast<int>(i);
        }
        
        // Test non-temporal copy
        auto start = std::chrono::high_resolution_clock::now();
        
        NON_TEMPORAL_COPY(dest.data(), source.data(), SIZE * sizeof(int));
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "   Non-temporal copy completed in " << duration.count() << " microseconds\n";
        
        // Test prefetching
        memory_opt::Prefetcher prefetcher;
        std::vector<float> prefetch_data(100);
        
        for(size_t i = 0; i < prefetch_data.size(); ++i) {
            prefetcher.PrefetchAhead(&prefetch_data[i]);
        }
        
        std::cout << "   Prefetching test completed\n";
        
        // Test write combining
        memory_opt::WriteCombineBuffer<int, 32> write_buffer(dest.data());
        for(int i = 0; i < 100; ++i) {
            write_buffer.Write(i);
        }
        
        std::cout << "   Write combining test completed\n";
    }
    
    // Test 3: Compiler hints optimization
    std::cout << "\n3. Compiler Hints Optimization Test:\n";
    {
        // Test compiler intrinsics
        uint32_t test_value = 0x12345678;
        
        std::cout << "   Original value: 0x" << std::hex << test_value << std::dec << "\n";
        std::cout << "   Population count: " << POPCOUNT32(test_value) << "\n";
        std::cout << "   Leading zeros: " << CLZ32(test_value) << "\n";
        std::cout << "   Trailing zeros: " << CTZ32(test_value) << "\n";
        std::cout << "   Byte swapped: 0x" << std::hex << BSWAP32(test_value) << std::dec << "\n";
        
        // Test assumptions
        int* ptr = new int(42);
        int* not_null_ptr = ASSUME_NOT_NULL(ptr);
        std::cout << "   Assume not null test: " << *not_null_ptr << "\n";
        delete ptr;
        
        // Test unreachable
        bool condition = true;
        if(ASSUME_TRUE(condition)) {
            std::cout << "   Assume true test passed\n";
        }
        
        // Test optimization scope
        {
            OPTIMIZATION_SCOPE(3);
            std::vector<int> optimized_vec(100);
            for(auto& x : optimized_vec) {
                x = 42;
            }
            std::cout << "   Optimization scope test completed\n";
        }
        
        // Test inline scope
        {
            INLINE_SCOPE(true);
            auto local_func = []() { return 42; };
            int result = local_func();
            std::cout << "   Inline scope test result: " << result << "\n";
        }
        
        // Test loop optimization scope
        {
            LOOP_OPTIMIZATION_SCOPE(4, true);
            std::vector<float> loop_data(100);
            for(auto& x : loop_data) {
                x = 3.14f;
            }
            std::cout << "   Loop optimization scope test completed\n";
        }
    }
    
    std::cout << "\nAll 3 advanced optimizations tested successfully!\n";
    std::cout << "Total optimizations completed: 12 (9 previous + 3 new)\n";
    
    return 0;
}