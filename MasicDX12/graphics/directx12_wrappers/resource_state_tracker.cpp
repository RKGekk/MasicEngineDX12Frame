#include "resource_state_tracker.h"

#include "command_list.h"
#include "resource.h"

#include <directx/d3dx12.h>

std::mutex ResourceStateTracker::ms_global_mutex;
bool ResourceStateTracker::ms_is_locked = false;
ResourceStateTracker::ResourceStateMap ResourceStateTracker::ms_global_resource_state;

ResourceStateTracker::ResourceStateTracker() {}

ResourceStateTracker::~ResourceStateTracker() {}

// Push a resource barrier to the resource state tracker.
void ResourceStateTracker::ResourceBarrier(const D3D12_RESOURCE_BARRIER& barrier) {
    if (barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) {
        const D3D12_RESOURCE_TRANSITION_BARRIER& transition_barrier = barrier.Transition;

        // First check if there is already a known "final" state for the given resource.
        // If there is, the resource has been used on the command list before and
        // already has a known state within the command list execution.
        const auto iter = m_final_resource_state.find(transition_barrier.pResource);
        if (iter != m_final_resource_state.end()) {
            auto& resource_state = iter->second;
            if (transition_barrier.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES && !resource_state.SubresourceState.empty()) {
                // First transition all of the subresources if they are different than the StateAfter.
                for (auto subresource_state : resource_state.SubresourceState) {
                    if (transition_barrier.StateAfter != subresource_state.second) {
                        D3D12_RESOURCE_BARRIER new_barrier = barrier;
                        new_barrier.Transition.Subresource = subresource_state.first;
                        new_barrier.Transition.StateBefore = subresource_state.second;
                        m_resource_barriers.push_back(new_barrier);
                    }
                }
            }
            else {
                auto final_state = resource_state.GetSubresourceState(transition_barrier.Subresource);
                // Push a new transition barrier with the correct before state.
                if (transition_barrier.StateAfter != final_state) {
                    D3D12_RESOURCE_BARRIER new_barrier = barrier;
                    new_barrier.Transition.StateBefore = final_state;
                    m_resource_barriers.push_back(new_barrier);
                }
            }
        }
        else { // In this case, the resource is being used on the command list for the first time.
            // Add a pending barrier. The pending barriers will be resolved
            // before the command list is executed on the command queue.
            m_pending_resource_barriers.push_back(barrier);
        }
        // Push the final known state (possibly replacing the previously known state for the subresource).
        m_final_resource_state[transition_barrier.pResource].SetSubresourceState(transition_barrier.Subresource, transition_barrier.StateAfter);
    }
    else {
        // Just push non-transition barriers to the resource barriers array.
        m_resource_barriers.push_back(barrier);
    }
}

void ResourceStateTracker::TransitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES state_after, UINT sub_resource) {
    if (resource) {
        D3D12_RESOURCE_BARRIER rb = CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_COMMON, state_after, sub_resource);
        ResourceBarrier(rb);
    }
}

void ResourceStateTracker::TransitionResource(const Resource& resource, D3D12_RESOURCE_STATES state_after, UINT sub_resource) {
    TransitionResource(resource.GetD3D12Resource().Get(), state_after, sub_resource);
}

void ResourceStateTracker::UAVBarrier(const Resource* resource) {
    ID3D12Resource* pResource = resource != nullptr ? resource->GetD3D12Resource().Get() : nullptr;
    ResourceBarrier(CD3DX12_RESOURCE_BARRIER::UAV(pResource));
}

void ResourceStateTracker::AliasBarrier(const Resource* resource_before, const Resource* resource_after) {
    ID3D12Resource* pResourceBefore = resource_before != nullptr ? resource_before->GetD3D12Resource().Get() : nullptr;
    ID3D12Resource* pResourceAfter = resource_after != nullptr ? resource_after->GetD3D12Resource().Get() : nullptr;
    ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Aliasing(pResourceBefore, pResourceAfter));
}

// Flush any (non-pending) resource barriers that have been pushed to the resource state tracker.
void ResourceStateTracker::FlushResourceBarriers(const std::shared_ptr<CommandList>& command_list) {
    assert(command_list);

    UINT num_barriers = static_cast<UINT>(m_resource_barriers.size());
    if (num_barriers > 0u) {
        auto d3d12_command_list = command_list->GetD3D12CommandList();
        d3d12_command_list->ResourceBarrier(num_barriers, m_resource_barriers.data());
        m_resource_barriers.clear();
    }
}

// Flush any pending resource barriers to the command list.
uint32_t ResourceStateTracker::FlushPendingResourceBarriers(const std::shared_ptr<CommandList>& command_list) {
    assert(ms_is_locked);
    assert(command_list);

    ResourceBarriers resource_barriers;
    resource_barriers.reserve(m_pending_resource_barriers.size());

    for (auto pending_barrier : m_pending_resource_barriers) {
        if (pending_barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) {
            auto pending_transition = pending_barrier.Transition;
            const auto& iter = ms_global_resource_state.find(pending_transition.pResource);
            if (iter != ms_global_resource_state.end()) {
                auto& resource_state = iter->second;
                if (pending_transition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES && !resource_state.SubresourceState.empty()) {
                    for (auto subresource_state : resource_state.SubresourceState) {
                        if (pending_transition.StateAfter != subresource_state.second) {
                            D3D12_RESOURCE_BARRIER new_barrier = pending_barrier;
                            new_barrier.Transition.Subresource = subresource_state.first;
                            new_barrier.Transition.StateBefore = subresource_state.second;
                            resource_barriers.push_back(new_barrier);
                        }
                    }
                }
                else {
                    auto global_state = (iter->second).GetSubresourceState(pending_transition.Subresource);
                    if (pending_transition.StateAfter != global_state) {
                        pending_barrier.Transition.StateBefore = global_state;
                        resource_barriers.push_back(pending_barrier);
                    }
                }
            }
        }
    }

    UINT num_barriers = static_cast<UINT>(resource_barriers.size());
    if (num_barriers > 0u) {
        auto d3d12CommandList = command_list->GetD3D12CommandList();
        d3d12CommandList->ResourceBarrier(num_barriers, resource_barriers.data());
    }

    m_pending_resource_barriers.clear();

    return num_barriers;
}

// Commit final resource states to the global resource state map. This must be called when the command list is closed.
void ResourceStateTracker::CommitFinalResourceStates() {
    assert(ms_is_locked);

    for (const auto& resource_state : m_final_resource_state) {
        ms_global_resource_state[resource_state.first] = resource_state.second;
    }

    m_final_resource_state.clear();
}

// Reset state tracking. This must be done when the command list is reset.
void ResourceStateTracker::Reset() {
    m_pending_resource_barriers.clear();
    m_resource_barriers.clear();
    m_final_resource_state.clear();
}

// The global state must be locked before flushing pending resource barriers andcommitting the final resource state to the global resource state.
// This ensures consistency of the global resource state between command list executions.
void ResourceStateTracker::Lock() {
    ms_global_mutex.lock();
    ms_is_locked = true;
}

// Unlocks the global resource state after the final states have been committed to the global resource state array.
void ResourceStateTracker::Unlock() {
    ms_global_mutex.unlock();
    ms_is_locked = false;
}

// Add a resource with a given state to the global resource state array (map). This should be done when the resource is created for the first time.
void ResourceStateTracker::AddGlobalResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state) {
    if (resource != nullptr) {
        std::lock_guard<std::mutex> lock(ms_global_mutex);
        ms_global_resource_state[resource].SetSubresourceState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, state);
    }
}

ResourceStateTracker::ResourceState::ResourceState(D3D12_RESOURCE_STATES state) : State(state) {}

// Remove a resource from the global resource state array (map). This should only be done when the resource is destroyed.
void ResourceStateTracker::ResourceState::SetSubresourceState(UINT subresource, D3D12_RESOURCE_STATES state) {
    if (subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) {
        State = state;
        SubresourceState.clear();
    }
    else {
        SubresourceState[subresource] = state;
    }
}

D3D12_RESOURCE_STATES ResourceStateTracker::ResourceState::GetSubresourceState(UINT subresource) const {
    D3D12_RESOURCE_STATES state = State;
    const auto iter = SubresourceState.find(subresource);
    if (iter != SubresourceState.end()) {
        state = iter->second;
    }
    return state;
}