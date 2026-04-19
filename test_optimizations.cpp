// Test file to verify the new optimizations work
#include "base/tl/lookup_table.h"
#include "base/tl/bit_ops.h"
#include "base/tl/branch_prediction.h"
#include <iostream>
#include <cmath>

int main()
{
    std::cout << "Testing 3 new optimizations:\n\n";
    
    // Test 1: Lookup table optimization
    std::cout << "1. Trigonometric Lookup Table Test:\n";
    CTrigLUT& lut = CTrigLUT::Get();
    
    float angle = 0.5f; // radians
    float sin_val = lut.FastSin(angle);
    float cos_val = lut.FastCos(angle);
    float real_sin = sinf(angle);
    float real_cos = cosf(angle);
    
    std::cout << "   Angle: " << angle << " rad\n";
    std::cout << "   FastSin: " << sin_val << " vs Real: " << real_sin 
              << " (Error: " << fabs(sin_val - real_sin) << ")\n";
    std::cout << "   FastCos: " << cos_val << " vs Real: " << real_cos 
              << " (Error: " << fabs(cos_val - real_cos) << ")\n";
    std::cout << "   Table size: " << lut.GetTableSize() 
              << " entries, Memory: " << lut.GetMemoryUsage() << " bytes\n\n";
    
    // Test 2: Bit operations optimization
    std::cout << "2. Bit Operations Test:\n";
    uint32_t value = 12345;
    
    std::cout << "   Value: " << value << "\n";
    std::cout << "   IsPowerOfTwo: " << (bit_ops::IsPowerOfTwo(value) ? "Yes" : "No") << "\n";
    std::cout << "   NextPowerOfTwo: " << bit_ops::NextPowerOfTwo(value) << "\n";
    std::cout << "   Leading zeros: " << bit_ops::CountLeadingZeros32(value) << "\n";
    std::cout << "   Trailing zeros: " << bit_ops::CountTrailingZeros32(value) << "\n";
    std::cout << "   Population count: " << bit_ops::PopulationCount32(value) << "\n";
    std::cout << "   FastLog2: " << bit_ops::FastLog2(value) << "\n\n";
    
    // Test 3: Branch prediction optimization
    std::cout << "3. Branch Prediction Test:\n";
    
    // Test branchless functions
    float a = 3.14f, b = 2.71f;
    std::cout << "   Values: a=" << a << ", b=" << b << "\n";
    std::cout << "   BranchlessMin: " << branch_opt::BranchlessMin(a, b) << "\n";
    std::cout << "   BranchlessMax: " << branch_opt::BranchlessMax(a, b) << "\n";
    std::cout << "   BranchlessAbs(a): " << branch_opt::BranchlessAbs(a) << "\n";
    std::cout << "   BranchlessAbs(-a): " << branch_opt::BranchlessAbs(-a) << "\n";
    
    // Test LIKELY/UNLIKELY macros
    bool condition = true;
    if(LIKELY(condition))
    {
        std::cout << "   LIKELY macro works (condition was true)\n";
    }
    
    if(UNLIKELY(false))
    {
        // This should not execute
    }
    else
    {
        std::cout << "   UNLIKELY macro works (condition was false)\n";
    }
    
    // Test branchless selection
    int select_result = branch_opt::Select(true, 100, 200);
    std::cout << "   Branchless Select(true, 100, 200): " << select_result << "\n";
    
    std::cout << "\nAll 3 optimizations compiled successfully!\n";
    
    return 0;
}