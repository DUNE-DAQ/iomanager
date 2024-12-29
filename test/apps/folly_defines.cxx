
#include "folly/Portability.h"

#include <iostream>

int main(int , char** )
{

  std::cout << "A study of the various #defines found in folly's F14IntrinsicsAvailability.h header" << "\n";

  std::cout << "FoLLY_SSE == " << FOLLY_SSE << "\n";
  std::cout << "FoLLY_NEON == " << FOLLY_NEON << "\n";
  std::cout << "FoLLY_AARCH64 == " << FOLLY_AARCH64 << "\n";
  std::cout << "FoLLY_RISCV64 == " << FOLLY_RISCV64 << "\n";
  std::cout << "FoLLY_MOBILE == " << FOLLY_MOBILE << "\n";
  std::cout << "FoLLY_SSE_PREREQ(4, 2) == " << FOLLY_SSE_PREREQ(4, 2) << "\n";
  std::cout << "FoLLY_ARM_FEATURE_CRC32 == " << FOLLY_ARM_FEATURE_CRC32 << "\n";

  // The following section of code is directly lifted from F14IntrinsicsAvailability.h in folly's v2024.12.02.00 commit
  
#if (FOLLY_SSE >= 2 || (FOLLY_NEON && FOLLY_AARCH64) || FOLLY_RISCV64) && \
  !FOLLY_MOBILE &&                                                      \
  !(defined(FOLLY_F14_FORCE_FALLBACK) && FOLLY_F14_FORCE_FALLBACK)
#define FOLLY_F14_VECTOR_INTRINSICS_AVAILABLE 1
#else
#define FOLLY_F14_VECTOR_INTRINSICS_AVAILABLE 0
#endif

#if FOLLY_SSE_PREREQ(4, 2) || FOLLY_ARM_FEATURE_CRC32
#define FOLLY_F14_CRC_INTRINSIC_AVAILABLE 1
#else
#define FOLLY_F14_CRC_INTRINSIC_AVAILABLE 0
#endif

  std::cout << "\nUnder v2024.12.02.00 : " << "\n";
  std::cout << "FoLLY_F14_VECTOR_INTRINSICS_AVAILABLE == " << FOLLY_F14_VECTOR_INTRINSICS_AVAILABLE << "\n";
  std::cout << "FoLLY_F14_CRC_INTRINSIC_AVAILABLE == " << FOLLY_F14_CRC_INTRINSIC_AVAILABLE  << "\n";

  // The following section of code is directly lifted from F14IntrinsicsAvailability.h in folly's 2021.12.13.00 commit
  
  std::cout << "\nUnder v2021.12.13.00 : " << "\n";
  std::cout << "FoLLY_F14_VECTOR_INTRINSICS_AVAILABLE == " << FOLLY_F14_VECTOR_INTRINSICS_AVAILABLE << "\n";
  std::cout << "FoLLY_F14_CRC_INTRINSIC_AVAILABLE == " << FOLLY_F14_CRC_INTRINSIC_AVAILABLE  << "\n";

}

