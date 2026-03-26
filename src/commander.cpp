#include <polycpp/commander/detail/aggregator.hpp>

// Compiled translation unit for polycpp_commander.
// The program() singleton must be in a single TU to avoid ODR issues.

namespace polycpp {
namespace commander {

Command& program() {
    static Command instance;
    return instance;
}

} // namespace commander
} // namespace polycpp
