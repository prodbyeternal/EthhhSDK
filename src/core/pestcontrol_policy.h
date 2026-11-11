#pragma once
#include "strat_identity.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace eth::patch::pest {
constexpr size_t kMaxStrats = 8192, kMaxRequests = 64;
constexpr uint32_t kDeleteFlags = 5; // Native 128: stop ordinary script ticks; request native teardown.
constexpr uint32_t kScanTicks = 15; // Four searches per simulation second, never per render.
struct Layout { uintptr_t manager = 0, head = 0, tail = 0, current = 0, next = 0; };
struct Target { uintptr_t address = 0, pClass = 0; uint32_t handle = 0; };
struct Node {
    uint32_t flags = 0, type = 0, next = 0, previous = 0, handle = 0, unused = 0, pClass = 0;
};
static_assert(sizeof(Node) == 0x1c, "strat identity span +0x190..+0x1AB");
struct Plan {
    std::array<Target, kMaxRequests> targets{};
    size_t count = 0, visited = 0, detected = 0;
    bool complete = false, budgetStopped = false;
};
template<class Read> bool Idle(const Layout& l, Read read) {
    uint32_t current = 0, next = 0;
    return l.manager && read(l.current, &current, 4) && !current && read(l.next, &next, 4) && !next;
}
template<class Read> bool ReadNode(uintptr_t address, Node& node, Read read) {
    return address >= 0x10000 && address <= UINT32_MAX - 0x1ac && read(address + 0x190, &node, sizeof(node));
}
template<class Read, class WithinBudget>
Plan Search(bool enabled, const Layout& l, Read read, WithinBudget withinBudget) {
    Plan plan;
    if (!enabled || !Idle(l, read)) return plan;
    uint32_t head = 0, tail = 0, checkHead = 0, checkTail = 0;
    if (!read(l.head, &head, 4) || !read(l.tail, &tail, 4)) return plan;
    uintptr_t address = head, previous = 0;
    while (address && plan.visited < kMaxStrats) {
        if (!withinBudget()) { plan.budgetStopped = true; return plan; }
        Node node;
        if (!ReadNode(address, node, read) || node.previous != previous || node.next == address) return plan;
        ++plan.visited;
        char name[game::kClassNameCapacity] = {};
        if (!game::ReadClassName(node.pClass, name, read)) return plan;
        if (!strcmp(name, "Rat")) {
            ++plan.detected;
            if (!(node.flags & 4) && plan.count < kMaxRequests && node.handle &&
                game::ResolveOwnerHandle(l.manager, node.handle, read) == address)
                plan.targets[plan.count++] = {address, node.pClass, node.handle};
        }
        previous = address; address = node.next;
    }
    // No requests from a corrupt, truncated, transitioning, or busy list.
    plan.complete = !address && previous == tail && read(l.head, &checkHead, 4) && checkHead == head &&
        read(l.tail, &checkTail, 4) && checkTail == tail && Idle(l, read);
    return plan;
}
template<class Read>
bool Validate(const Layout& l, const Target& target, Read read) {
    Node node;
    if (!Idle(l, read) || !ReadNode(target.address, node, read) || node.flags & 4 ||
        node.pClass != target.pClass || node.handle != target.handle || !node.handle ||
        game::ResolveOwnerHandle(l.manager, node.handle, read) != target.address) return false;
    char name[game::kClassNameCapacity] = {};
    if (!game::ReadClassName(node.pClass, name, read) || strcmp(name, "Rat")) return false;
    uint32_t previousNext = 0, nextPrevious = 0;
    if (!read(node.previous ? uintptr_t(node.previous) + 0x198 : l.head, &previousNext, 4) ||
        previousNext != target.address ||
        !read(node.next ? uintptr_t(node.next) + 0x19c : l.tail, &nextPrevious, 4) || nextPrevious != target.address)
        return false;
    Node check;
    return ReadNode(target.address, check, read) && !memcmp(&node, &check, sizeof(node)) &&
        game::ResolveOwnerHandle(l.manager, target.handle, read) == target.address && Idle(l, read);
}
// The caller supplies only the verified native request, never free/list unlink.
template<class Read, class Request>
size_t Apply(bool enabled, const Layout& l, const Plan& plan, Read read, Request request) {
    if (!enabled || !plan.complete || plan.count > kMaxRequests) return 0;
    size_t requested = 0;
    for (size_t i = 0; i < plan.count; ++i)
        if (Validate(l, plan.targets[i], read) && request(plan.targets[i])) ++requested;
    return requested;
}
} // namespace eth::patch::pest
