#pragma once

#include <HardwareAbstractionLayer/ResourceState.hpp>
#include "render_pass_metadata.h"

#include <vector>
#include <list>
#include <functional>
#include <stack>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

class RenderPassGraph {
public:
	using SubresourceName = uint64_t;
	using WriteDependencyRegistry = std::unordered_map<SubresourceName, std::string>;

	class Node {
	public:
		inline static const std::string BackBufferName = "BackBuffer_1TJWWnf7GA";

		using SubresourceList = std::vector<uint32_t>;
		using QueueIndex = uint64_t;

		Node(const RenderPassMetadata& passMetadata, WriteDependencyRegistry* writeDependencyRegistry);

		bool operator==(const Node& that) const;
		bool operator!=(const Node& that) const;

		void AddReadDependency(std::string resourceName, uint32_t subresourceCount);
		void AddReadDependency(std::string resourceName, uint32_t firstSubresourceIndex, uint32_t lastSubresourceIndex);
		void AddReadDependency(std::string resourceName, const SubresourceList& subresources);

		void AddWriteDependency(std::string resourceName, std::optional<std::string> originalResourceName, uint32_t subresourceCount);
		void AddWriteDependency(std::string resourceName, std::optional<std::string> originalResourceName, uint32_t firstSubresourceIndex, uint32_t lastSubresourceIndex);
		void AddWriteDependency(std::string resourceName, std::optional<std::string> originalResourceName, const SubresourceList& subresources);

		bool HasDependency(std::string resourceName, uint32_t subresourceIndex) const;
		bool HasDependency(SubresourceName subresourceName) const;
		bool HasAnyDependencies() const;

		uint64_t ExecutionQueueIndex = 0;
		bool UsesRayTracing = false;

		const auto& PassMetadata() const { return mPassMetadata; }
		const auto& ReadSubresources() const { return mReadSubresources; }
		const auto& WrittenSubresources() const { return mWrittenSubresources; }
		const auto& ReadAndWritten() const { return mReadAndWrittenSubresources; }
		const auto& AllResources() const { return mAllResources; }
		const auto& NodesToSyncWith() const { return mNodesToSyncWith; }
		auto GlobalExecutionIndex() const { return mGlobalExecutionIndex; }
		auto DependencyLevelIndex() const { return mDependencyLevelIndex; }
		auto LocalToDependencyLevelExecutionIndex() const { return mLocalToDependencyLevelExecutionIndex; }
		auto LocalToQueueExecutionIndex() const { return mLocalToQueueExecutionIndex; }
		bool IsSyncSignalRequired() const { return mSyncSignalRequired; }

	private:
		using SynchronizationIndexSet = std::vector<uint64_t>;
		inline static const uint64_t InvalidSynchronizationIndex = std::numeric_limits<uint64_t>::max();

		friend RenderPassGraph;

		void EnsureSingleWriteDependency(SubresourceName name);
		void Clear();

		uint64_t mGlobalExecutionIndex = 0;
		uint64_t mDependencyLevelIndex = 0;
		uint64_t mLocalToDependencyLevelExecutionIndex = 0;
		uint64_t mLocalToQueueExecutionIndex = 0;
		uint64_t mIndexInUnorderedList = 0;

		RenderPassMetadata mPassMetadata;
		WriteDependencyRegistry* mWriteDependencyRegistry = nullptr;

		std::unordered_set<SubresourceName> mReadSubresources;
		std::unordered_set<SubresourceName> mWrittenSubresources;
		std::unordered_set<SubresourceName> mReadAndWrittenSubresources;

		// Aliased subresources form node dependencies same as read resources, 
		// but are not actually being read and not participating in state transitions
		std::unordered_set<SubresourceName> mAliasedSubresources;
		std::unordered_set<std::string> mAllResources;

		SynchronizationIndexSet mSynchronizationIndexSet;
		std::vector<const Node*> mNodesToSyncWith;
		bool mSyncSignalRequired = false;
	};

	class DependencyLevel {
	public:
		friend RenderPassGraph;

		using NodeList = std::list<Node*>;
		using NodeIterator = NodeList::iterator;

	private:
		void AddNode(Node* node);
		Node* RemoveNode(NodeIterator it);

		uint64_t mLevelIndex = 0;
		NodeList mNodes;
		std::vector<std::vector<const Node*>> mNodesPerQueue;

		// Storage for queues that read at least one common resource. Resource state transitions
		// for such queues need to be handled differently.
		std::unordered_set<Node::QueueIndex> mQueuesInvoledInCrossQueueResourceReads;
		std::unordered_set<SubresourceName> mSubresourcesReadByMultipleQueues;

	public:
		inline const auto& Nodes() const { return mNodes; }
		inline const auto& NodesForQueue(Node::QueueIndex queueIndex) const { return mNodesPerQueue[queueIndex]; }
		inline const auto& QueuesInvoledInCrossQueueResourceReads() const { return mQueuesInvoledInCrossQueueResourceReads; }
		inline const auto& SubresourcesReadByMultipleQueues() const { return mSubresourcesReadByMultipleQueues; }
		inline auto LevelIndex() const { return mLevelIndex; }
	};

	using NodeList = std::vector<Node>;
	using NodeListIterator = NodeList::iterator;
	using ResourceUsageTimeline = std::pair<uint64_t, uint64_t>;
	using ResourceUsageTimelines = std::unordered_map<std::string, ResourceUsageTimeline>;

	static SubresourceName ConstructSubresourceName(std::string resourceName, uint32_t subresourceIndex);
	static std::pair<std::string, uint32_t> DecodeSubresourceName(SubresourceName name);

	uint64_t NodeCountForQueue(uint64_t queueIndex) const;
	const ResourceUsageTimeline& GetResourceUsageTimeline(std::string resourceName) const;
	const Node* GetNodeThatWritesToSubresource(SubresourceName subresourceName) const;

	uint64_t AddPass(const RenderPassMetadata& passMetadata);

	void Build();
	void Clear();

	const auto& NodesInGlobalExecutionOrder() const { return mNodesInGlobalExecutionOrder; }
	const auto& Nodes() const { return mPassNodes; }
	auto& Nodes() { return mPassNodes; }
	const auto& DependencyLevels() const { return mDependencyLevels; }
	auto DetectedQueueCount() const { return mDetectedQueueCount; }
	const auto& NodesForQueue(Node::QueueIndex queueIndex) const { return mNodesPerQueue[queueIndex]; }
	const Node* FirstNodeThatUsesRayTracingOnQueue(Node::QueueIndex queueIndex) const { return mFirstNodesThatUseRayTracing[queueIndex]; }

private:
	using DependencyLevelList = std::vector<DependencyLevel>;
	using OrderedNodeList = std::vector<Node*>;
	using RenderPassRegistry = std::unordered_set<std::string>;
	using QueueNodeCounters = std::unordered_map<uint64_t, uint64_t>;
	using AdjacencyLists = std::vector<std::vector<uint64_t>>;
	using WrittenSubresourceToPassMap = std::unordered_map<SubresourceName, const Node*>;

	struct SyncCoverage {
		const Node* NodeToSyncWith = nullptr;
		uint64_t NodeToSyncWithIndex = 0;
		std::vector<uint64_t> SyncedQueueIndices;
	};

	void EnsureRenderPassUniqueness(std::string passName);
	void BuildAdjacencyLists();
	void DepthFirstSearch(uint64_t nodeIndex, std::vector<bool>& visited, std::vector<bool>& onStack, bool& isCyclic);
	void TopologicalSort();
	void BuildDependencyLevels();
	void FinalizeDependencyLevels();
	void CullRedundantSynchronizations();

	NodeList mPassNodes;
	AdjacencyLists mAdjacencyLists;
	DependencyLevelList mDependencyLevels;

	// In order to avoid any unambiguity in graph nodes execution order
	// and avoid cyclic dependencies to make graph builds fully automatic
	// we must ensure that there can only be one write dependency for each subresource in a frame
	WriteDependencyRegistry mGlobalWriteDependencyRegistry;

	ResourceUsageTimelines mResourceUsageTimelines;
	RenderPassRegistry mRenderPassRegistry;
	QueueNodeCounters mQueueNodeCounters;
	OrderedNodeList mTopologicallySortedNodes;
	OrderedNodeList mNodesInGlobalExecutionOrder;
	WrittenSubresourceToPassMap mWrittenSubresourceToPassMap;
	uint64_t mDetectedQueueCount = 1;
	std::vector<std::vector<const Node*>> mNodesPerQueue;
	std::vector<const Node*> mFirstNodesThatUseRayTracing;
};