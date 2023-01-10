#pragma once

#define WIN32_LEAN_AND_MEAN

#include "../../tools/unique_name.h"
#include "render_pass_metadata.h"

#include <vector>
#include <list>
#include <functional>
#include <stack>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <d3d12.h>

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

class RenderPassGraph {
public:
	using SubresourceName = uint64_t;
	using WriteDependencyRegistry = std::unordered_map<SubresourceName, UniqueName>;

	class Node {
	public:
		inline static const UniqueName BackBufferName = "BackBuffer_1TJWWnf7GA";

		using SubresourceList = std::vector<uint32_t>;
		using QueueIndex = uint64_t;

		Node(const RenderPassMetadata& pass_metadata, RenderPassGraph::WriteDependencyRegistry* write_dependency_registry);

		bool operator==(const Node& that) const;
		bool operator!=(const Node& that) const;

		void AddReadDependency(UniqueName resource_name, uint32_t subresource_count);
		void AddReadDependency(UniqueName resource_name, uint32_t first_subresource_index, uint32_t last_subresource_index);
		void AddReadDependency(UniqueName resource_name, const SubresourceList& subresources);

		void AddWriteDependency(UniqueName resource_name, std::optional<UniqueName> original_resource_name, uint32_t subresource_count);
		void AddWriteDependency(UniqueName resource_name, std::optional<UniqueName> original_resource_name, uint32_t first_subresource_index, uint32_t last_subresource_index);
		void AddWriteDependency(UniqueName resource_name, std::optional<UniqueName> original_resource_name, const SubresourceList& subresources);

		bool HasDependency(UniqueName resource_name, uint32_t subresource_index) const;
		bool HasDependency(RenderPassGraph::SubresourceName subresource_name) const;
		bool HasAnyDependencies() const;

		uint64_t ExecutionQueueIndex = 0;
		bool UsesRayTracing = false;

		const RenderPassMetadata& PassMetadata() const;
		const std::unordered_set<RenderPassGraph::SubresourceName>& ReadSubresources() const;
		const std::unordered_set<RenderPassGraph::SubresourceName>& WrittenSubresources() const;
		const std::unordered_set<RenderPassGraph::SubresourceName>& ReadAndWritten() const;
		const std::unordered_set<UniqueName>& AllResources() const;
		const std::vector<const Node*>& NodesToSyncWith() const;
		uint64_t GlobalExecutionIndex() const;
		uint64_t DependencyLevelIndex() const;
		uint64_t LocalToDependencyLevelExecutionIndex() const;
		uint64_t LocalToQueueExecutionIndex() const;
		bool IsSyncSignalRequired() const;

	private:
		using SynchronizationIndexSet = std::vector<uint64_t>;
		inline static const uint64_t InvalidSynchronizationIndex = std::numeric_limits<uint64_t>::max();

		friend RenderPassGraph;

		void EnsureSingleWriteDependency(RenderPassGraph::SubresourceName name);
		void Clear();

		uint64_t m_global_execution_index = 0;
		uint64_t m_dependency_level_index = 0;
		uint64_t m_local_to_dependency_level_execution_index = 0;
		uint64_t m_local_to_queue_execution_index = 0;
		uint64_t m_index_in_unordered_list = 0;

		RenderPassMetadata m_pass_metadata;
		WriteDependencyRegistry* m_write_dependency_registry = nullptr;

		std::unordered_set<RenderPassGraph::SubresourceName> m_read_subresources;
		std::unordered_set<RenderPassGraph::SubresourceName> m_written_subresources;
		std::unordered_set<RenderPassGraph::SubresourceName> m_read_and_written_subresources;

		// Aliased subresources form node dependencies same as read resources, 
		// but are not actually being read and not participating in state transitions
		std::unordered_set<RenderPassGraph::SubresourceName> m_aliased_subresources;
		std::unordered_set<UniqueName> m_all_resources;

		SynchronizationIndexSet m_synchronization_index_set;
		std::vector<const Node*> m_nodes_to_sync_with;
		bool m_sync_signal_required = false;
	};

	class DependencyLevel {
	public:
		friend RenderPassGraph;

		using NodeList = std::list<Node*>;
		using NodeIterator = NodeList::iterator;

		const NodeList& Nodes() const;
		const std::vector<const Node*>& NodesForQueue(Node::QueueIndex queue_index) const;
		const std::unordered_set<Node::QueueIndex>& QueuesInvoledInCrossQueueResourceReads() const;
		const std::unordered_set<RenderPassGraph::SubresourceName>& SubresourcesReadByMultipleQueues() const;
		auto LevelIndex() const { return m_level_index; }

	private:
		void AddNode(Node* node);
		Node* RemoveNode(NodeIterator it);

		uint64_t m_level_index = 0;
		NodeList m_nodes;
		std::vector<std::vector<const Node*>> m_nodes_per_queue;

		// Storage for queues that read at least one common resource. Resource state transitions
		// for such queues need to be handled differently.
		std::unordered_set<Node::QueueIndex> m_queues_involed_in_cross_queue_resource_reads;
		std::unordered_set<RenderPassGraph::SubresourceName> m_subresources_read_by_multiple_queues;
	};

	using NodeList = std::vector<Node>;
	using NodeListIterator = NodeList::iterator;
	using ResourceUsageTimeline = std::pair<uint64_t, uint64_t>;
	using ResourceUsageTimelines = std::unordered_map<UniqueName, ResourceUsageTimeline>;

	using DependencyLevelList = std::vector<DependencyLevel>;
	using OrderedNodeList = std::vector<Node*>;
	using RenderPassRegistry = std::unordered_set<UniqueName>;
	using QueueNodeCounters = std::unordered_map<uint64_t, uint64_t>;
	using AdjacencyLists = std::vector<std::vector<uint64_t>>;
	using WrittenSubresourceToPassMap = std::unordered_map<SubresourceName, const Node*>;

	static SubresourceName ConstructSubresourceName(UniqueName resource_name, uint32_t subresource_index);
	static std::pair<UniqueName, uint32_t> DecodeSubresourceName(SubresourceName name);

	uint64_t NodeCountForQueue(uint64_t queue_index) const;
	const ResourceUsageTimeline& GetResourceUsageTimeline(UniqueName resource_name) const;
	const Node* GetNodeThatWritesToSubresource(SubresourceName subresource_name) const;

	uint64_t AddPass(const RenderPassMetadata& pass_metadata);

	void Build();
	void Clear();

	const OrderedNodeList& NodesInGlobalExecutionOrder() const;
	const NodeList& Nodes() const;
	NodeList& Nodes();
	const DependencyLevelList& DependencyLevels() const;
	uint64_t DetectedQueueCount() const;
	const std::vector<const Node*>& NodesForQueue(Node::QueueIndex queueIndex) const;
	const Node* FirstNodeThatUsesRayTracingOnQueue(Node::QueueIndex queueIndex) const;

private:

	struct SyncCoverage {
		const Node* NodeToSyncWith = nullptr;
		uint64_t NodeToSyncWithIndex = 0;
		std::vector<uint64_t> SyncedQueueIndices;
	};

	void EnsureRenderPassUniqueness(UniqueName pass_name);
	void BuildAdjacencyLists();
	void DepthFirstSearch(uint64_t node_index, std::vector<bool>& visited, std::vector<bool>& on_stack, bool& is_cyclic);
	void TopologicalSort();
	void BuildDependencyLevels();
	void FinalizeDependencyLevels();
	void CullRedundantSynchronizations();

	NodeList m_pass_nodes;
	AdjacencyLists m_adjacency_lists;
	DependencyLevelList m_dependency_levels;

	// In order to avoid any unambiguity in graph nodes execution order
	// and avoid cyclic dependencies to make graph builds fully automatic
	// we must ensure that there can only be one write dependency for each subresource in a frame
	WriteDependencyRegistry m_global_write_dependency_registry;

	ResourceUsageTimelines m_resource_usage_timelines;
	RenderPassRegistry m_render_pass_registry;
	QueueNodeCounters m_queue_node_counters;
	OrderedNodeList m_topologically_sorted_nodes;
	OrderedNodeList m_nodes_in_global_execution_order;
	WrittenSubresourceToPassMap m_written_subresource_to_pass_map;
	uint64_t m_detected_queue_count = 1;
	std::vector<std::vector<const Node*>> m_nodes_per_queue;
	std::vector<const Node*> m_first_nodes_that_use_ray_tracing;
};