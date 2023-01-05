#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>

class CommandList;
class Resource;

class ResourceStateTracker {
public:
	ResourceStateTracker();
	virtual ~ResourceStateTracker();

	void ResourceBarrier(const D3D12_RESOURCE_BARRIER& barrier);

	void TransitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES state_after, UINT sub_resource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	void TransitionResource(const Resource& resource, D3D12_RESOURCE_STATES state_after, UINT sub_resource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

	void UAVBarrier(const Resource* resource = nullptr);
	void AliasBarrier(const Resource* resource_before = nullptr, const Resource* resource_after = nullptr);

	uint32_t FlushPendingResourceBarriers(const std::shared_ptr<CommandList>& command_list);
	void FlushResourceBarriers(const std::shared_ptr<CommandList>& command_list);

	void CommitFinalResourceStates();

	void Reset();

	static void Lock();
	static void Unlock();

	static void AddGlobalResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state);

protected:
private:
	using ResourceBarriers = std::vector<D3D12_RESOURCE_BARRIER>;

	// Pending resource transitions are committed before a command list
	// is executed on the command queue. This guarantees that resources will
	// be in the expected state at the beginning of a command list.
	ResourceBarriers m_pending_resource_barriers;

	// Resource barriers that need to be committed to the command list.
	ResourceBarriers m_resource_barriers;

	struct ResourceState {
		explicit ResourceState(D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON);

		void SetSubresourceState(UINT subresource, D3D12_RESOURCE_STATES state);
		D3D12_RESOURCE_STATES GetSubresourceState(UINT subresource) const;

		D3D12_RESOURCE_STATES State;
		std::map<UINT, D3D12_RESOURCE_STATES> SubresourceState;
	};

	using ResourceList = std::vector<ID3D12Resource*>;
	using ResourceStateMap = std::unordered_map<ID3D12Resource*, ResourceState>;

	// The final (last known state) of the resources within a command list.
	// The final resource state is committed to the global resource state when the 
	// command list is closed but before it is executed on the command queue.
	ResourceStateMap m_final_resource_state;

	// The global resource state array (map) stores the state of a resource
	// between command list execution.
	static ResourceStateMap ms_global_resource_state;

	static std::mutex ms_global_mutex;
	static bool ms_is_locked;
};