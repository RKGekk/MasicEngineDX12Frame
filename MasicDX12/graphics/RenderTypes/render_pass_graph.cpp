#include "render_pass_graph.h"

#include <cassert>

const RenderPassMetadata& RenderPassGraph::Node::PassMetadata() const {
	return m_pass_metadata;
}

const std::unordered_set<RenderPassGraph::SubresourceName>& RenderPassGraph::Node::ReadSubresources() const {
	return m_read_subresources;
}

const std::unordered_set<RenderPassGraph::SubresourceName>& RenderPassGraph::Node::WrittenSubresources() const {
	return m_written_subresources;
}

const std::unordered_set<RenderPassGraph::SubresourceName>& RenderPassGraph::Node::ReadAndWritten() const {
	return m_read_and_written_subresources;
}

const std::unordered_set<UniqueName>& RenderPassGraph::Node::AllResources() const {
	return m_all_resources;
}

const std::vector<const RenderPassGraph::Node*>& RenderPassGraph::Node::NodesToSyncWith() const {
	return m_nodes_to_sync_with;
}

uint64_t RenderPassGraph::Node::GlobalExecutionIndex() const {
	return m_global_execution_index;
}

uint64_t RenderPassGraph::Node::DependencyLevelIndex() const {
	return m_dependency_level_index;
}

uint64_t RenderPassGraph::Node::LocalToDependencyLevelExecutionIndex() const {
	return m_local_to_dependency_level_execution_index;
}

uint64_t RenderPassGraph::Node::LocalToQueueExecutionIndex() const {
	return m_local_to_queue_execution_index;
}

bool RenderPassGraph::Node::IsSyncSignalRequired() const {
	return m_sync_signal_required;
}

void RenderPassGraph::Node::EnsureSingleWriteDependency(RenderPassGraph::SubresourceName name) {
	auto [resource_name, subresource_index] = DecodeSubresourceName(name);
	assert(!m_write_dependency_registry->count(name)); // Resource already has a write dependency. Use Aliases to perform multiple writes into the same resource in pass.");

	m_write_dependency_registry->insert({ name, m_pass_metadata.Name });
}

void RenderPassGraph::Node::Clear() {
	m_read_subresources.clear();
	m_written_subresources.clear();
	m_read_and_written_subresources.clear();
	m_all_resources.clear();
	m_aliased_subresources.clear();
	m_nodes_to_sync_with.clear();
	m_synchronization_index_set.clear();
	m_dependency_level_index = 0ull;
	m_sync_signal_required = false;
	ExecutionQueueIndex = 0ull;
	UsesRayTracing = false;
	m_global_execution_index = 0ull;
	m_local_to_dependency_level_execution_index = 0ull;
}

const RenderPassGraph::DependencyLevel::NodeList& RenderPassGraph::DependencyLevel::Nodes() const {
	return m_nodes;
}

const std::vector<const RenderPassGraph::Node*>& RenderPassGraph::DependencyLevel::NodesForQueue(Node::QueueIndex queue_index) const {
	return m_nodes_per_queue[queue_index];
}

const std::unordered_set<RenderPassGraph::Node::QueueIndex>& RenderPassGraph::DependencyLevel::QueuesInvoledInCrossQueueResourceReads() const {
	return m_queues_involed_in_cross_queue_resource_reads;
}

const std::unordered_set<RenderPassGraph::SubresourceName>& RenderPassGraph::DependencyLevel::SubresourcesReadByMultipleQueues() const {
	return m_subresources_read_by_multiple_queues;
}

void RenderPassGraph::DependencyLevel::AddNode(Node* node) {
	m_nodes.push_back(node);
}

RenderPassGraph::Node* RenderPassGraph::DependencyLevel::RemoveNode(NodeIterator it) {
	Node* node = *it;
	m_nodes.erase(it);
	return node;
}

const RenderPassGraph::OrderedNodeList& RenderPassGraph::NodesInGlobalExecutionOrder() const {
	return m_nodes_in_global_execution_order;
}

const RenderPassGraph::NodeList& RenderPassGraph::Nodes() const {
	return m_pass_nodes;
}

RenderPassGraph::NodeList& RenderPassGraph::Nodes() {
	return m_pass_nodes;
}

const RenderPassGraph::DependencyLevelList& RenderPassGraph::DependencyLevels() const {
	return m_dependency_levels;
}

uint64_t RenderPassGraph::DetectedQueueCount() const {
	return m_detected_queue_count;
}

const std::vector<const RenderPassGraph::Node*>& RenderPassGraph::NodesForQueue(Node::QueueIndex queueIndex) const {
	return m_nodes_per_queue[queueIndex];
}

const RenderPassGraph::Node* RenderPassGraph::FirstNodeThatUsesRayTracingOnQueue(Node::QueueIndex queueIndex) const {
	return m_first_nodes_that_use_ray_tracing[queueIndex];
}

RenderPassGraph::Node::Node(const RenderPassMetadata& pass_metadata, RenderPassGraph::WriteDependencyRegistry* write_dependency_registry) : m_pass_metadata(pass_metadata), m_write_dependency_registry(write_dependency_registry) {}

bool RenderPassGraph::Node::operator==(const Node& that) const {
	return m_pass_metadata.Name == that.m_pass_metadata.Name;
}

bool RenderPassGraph::Node::operator!=(const Node& other) const {
	return !(*this == other);
}

void RenderPassGraph::Node::AddReadDependency(UniqueName resource_name, uint32_t subresource_count) {
	assert(subresource_count > 0u);
	AddReadDependency(resource_name, 0u, subresource_count - 1u);
}

void RenderPassGraph::Node::AddReadDependency(UniqueName resource_name, uint32_t first_subresource_index, uint32_t last_subresource_index) {
	for (auto i = first_subresource_index; i <= last_subresource_index; ++i) {
		SubresourceName name = ConstructSubresourceName(resource_name, i);
		m_read_subresources.insert(name);
		m_read_and_written_subresources.insert(name);
		m_all_resources.insert(resource_name);
	}
}

void RenderPassGraph::Node::AddReadDependency(UniqueName resource_name, const SubresourceList& subresources) {
	if (subresources.empty()) {
		AddReadDependency(resource_name, 1u);
	}
	else {
		for (uint32_t subresource_index : subresources) {
			SubresourceName name = ConstructSubresourceName(resource_name, subresource_index);
			m_read_subresources.insert(name);
			m_read_and_written_subresources.insert(name);
			m_all_resources.insert(resource_name);
		}
	}
}

void RenderPassGraph::Node::AddWriteDependency(UniqueName resource_name, std::optional<UniqueName> original_resource_name, uint32_t subresource_count) {
	assert(subresource_count > 0u);
	AddWriteDependency(resource_name, original_resource_name, 0u, subresource_count - 1u);
}

void RenderPassGraph::Node::AddWriteDependency(UniqueName resource_name, std::optional<UniqueName> original_resource_name, uint32_t first_subresource_index, uint32_t last_subresource_index) {
	for (auto i = first_subresource_index; i <= last_subresource_index; ++i) {
		SubresourceName name = ConstructSubresourceName(resource_name, i);
		EnsureSingleWriteDependency(name);
		m_written_subresources.insert(name);
		m_read_and_written_subresources.insert(name);
		m_all_resources.insert(resource_name);

		if (original_resource_name.has_value()) {
			SubresourceName original_subresoruce = ConstructSubresourceName(*original_resource_name, i);
			m_aliased_subresources.insert(original_subresoruce);
			m_all_resources.insert(*original_resource_name);
		}
	}
}

void RenderPassGraph::Node::AddWriteDependency(UniqueName resource_name, std::optional<UniqueName> original_resource_name, const SubresourceList& subresources) {
	if (subresources.empty()) {
		AddWriteDependency(resource_name, original_resource_name, 1u);
	}
	else {
		for (auto subresource_index : subresources) {
			SubresourceName name = ConstructSubresourceName(resource_name, subresource_index);
			EnsureSingleWriteDependency(name);
			m_written_subresources.insert(name);
			m_read_and_written_subresources.insert(name);
			m_all_resources.insert(resource_name);
		}
	}
}

bool RenderPassGraph::Node::HasDependency(UniqueName resource_name, uint32_t subresource_index) const {
	return HasDependency(ConstructSubresourceName(resource_name, subresource_index));
}

bool RenderPassGraph::Node::HasDependency(RenderPassGraph::SubresourceName subresource_name) const {
	return m_read_and_written_subresources.count(subresource_name);
}

bool RenderPassGraph::Node::HasAnyDependencies() const {
	return !m_read_and_written_subresources.empty();
}
