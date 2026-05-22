// Anchor TU for core_lib. The static library aggregates real code from
// src/util and src/storage; this file exists so the target always
// builds even before any module has its first .cpp populated.
namespace at {
namespace {
[[maybe_unused]] constexpr int core_lib_anchor = 1;
}
}  // namespace at
