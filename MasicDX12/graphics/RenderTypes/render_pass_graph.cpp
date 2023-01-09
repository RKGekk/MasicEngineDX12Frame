#include "render_pass_graph.h"

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
