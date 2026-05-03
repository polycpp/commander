#pragma once

/**
 * @file detail/aggregator.hpp
 * @brief Umbrella header — includes all commander declarations and implementations.
 *
 * Include order is critical to handle circular dependencies:
 * 1. Declarations (no method bodies)
 * 2. `Command::Impl` — defined once, used by both Command's inline bodies
 *    and by Help's internals
 * 3. Inline implementations (need all declarations and Impl above)
 *
 * @since 1.0.0
 */

// Declarations
#include <polycpp/commander/error.hpp>
#include <polycpp/commander/argument.hpp>
#include <polycpp/commander/option.hpp>
#include <polycpp/commander/help.hpp>
#include <polycpp/commander/suggest_similar.hpp>
#include <polycpp/commander/command.hpp>
#include <polycpp/commander/commander.hpp>

// Pimpl state — defined before any inline body that touches `impl_->...`.
#include <polycpp/commander/detail/command_impl.hpp>

// Inline implementations
#include <polycpp/commander/detail/error.hpp>
#include <polycpp/commander/detail/argument.hpp>
#include <polycpp/commander/detail/option.hpp>
#include <polycpp/commander/detail/suggest_similar.hpp>
#include <polycpp/commander/detail/help.hpp>
#include <polycpp/commander/detail/command.hpp>
#include <polycpp/commander/detail/commander.hpp>
