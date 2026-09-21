#pragma once

// Chrome trace-format output, loadable in chrome://tracing, ui.perfetto.dev or
// Speedscope. Shares a timebase with the SDK core's `tracing::*` events, so
// both sides land on one merged timeline.
//
// Enable with $VIDEOSDK_TRACE=/path/trace.json before constructing a Meeting,
// or call start()/stop() directly. The macros are always defined; they become
// no-ops when the SDK core is built without the `chrome-trace` feature.

#include <cstdint>
#include <string>

namespace videosdk {
namespace trace {

/// Start writing a trace file. True on success or if already active.
/// Known limitation: start() after stop() cannot reopen the writer —
/// tracing_subscriber::set_global_default only installs once per process.
bool start(const std::string& path);

/// Flush and close the trace file. No-op if no trace is active.
void stop();

/// Emit an instant event (a single tick-mark) on the current thread's lane.
void instant(const char* category, const char* name);

/// RAII span: `ph=B` at construction, `ph=E` at destruction. Nested scopes
/// stack in the viewer. Must begin and end on the same thread — enforced by
/// being non-copyable and non-movable.
class Scope {
public:
    Scope(const char* category, const char* name);
    ~Scope();

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    Scope(Scope&&) = delete;
    Scope& operator=(Scope&&) = delete;

private:
    uint64_t handle_;
};

}  // namespace trace
}  // namespace videosdk

// __LINE__ concatenation keeps two scopes in the same block from colliding.
#define VSDK_TRACE_CAT_(a, b) a##b
#define VSDK_TRACE_CAT(a, b) VSDK_TRACE_CAT_(a, b)

#define VSDK_TRACE_INSTANT(cat, name) ::videosdk::trace::instant((cat), (name))

#define VSDK_TRACE_SCOPE(cat, name) \
    ::videosdk::trace::Scope VSDK_TRACE_CAT(_vsdk_trace_scope_, __LINE__)((cat), (name))
