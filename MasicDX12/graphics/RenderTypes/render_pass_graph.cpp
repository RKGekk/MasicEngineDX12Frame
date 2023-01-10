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

RenderPassGraph::SubresourceName RenderPassGraph::ConstructSubresourceName(UniqueName resource_name, uint32_t subresource_index) {
	SubresourceName name = resource_name.ToId();
	name <<= 32;
	name |= subresource_index;
	return name;
}

std::pair<UniqueName, uint32_t> RenderPassGraph::DecodeSubresourceName(SubresourceName name) {
	return { UniqueName{ name >> 32 }, name & 0x0000FFFF };
}

uint64_t RenderPassGraph::NodeCountForQueue(uint64_t queue_index) const {
	auto count_it = m_queue_node_counters.find(queue_index);
	return count_it != m_queue_node_counters.end() ? count_it->second : 0ull;
}

const RenderPassGraph::ResourceUsageTimeline& RenderPassGraph::GetResourceUsageTimeline(UniqueName resource_name) const {
	auto timelineIt = m_resource_usage_timelines.find(resource_name);
	assert(timelineIt != m_resource_usage_timelines.end()); // Resource timeline doesn't exist
	return timelineIt->second;
}

const RenderPassGraph::Node* RenderPassGraph::GetNodeThatWritesToSubresource(SubresourceName subresource_name) const {
	auto it = m_written_subresource_to_pass_map.find(subresource_name);
	assert(it != m_written_subresource_to_pass_map.end()); // Subresource is not registered for writing in the graph.
	return it->second;
}

uint64_t RenderPassGraph::AddPass(const RenderPassMetadata& pass_metadata) {
	EnsureRenderPassUniqueness(pass_metadata.Name);
	m_pass_nodes.emplace_back(Node{ pass_metadata, &m_global_write_dependency_registry });
	m_pass_nodes.back().m_index_in_unordered_list = m_pass_nodes.size() - 1;
	return m_pass_nodes.size() - 1;
}

void RenderPassGraph::Build() {
	BuildAdjacencyLists();
	TopologicalSort();
	BuildDependencyLevels();
	FinalizeDependencyLevels();
	CullRedundantSynchronizations();
}

void RenderPassGraph::Clear() {
	m_global_write_dependency_registry.clear();
	m_dependency_levels.clear();
	m_resource_usage_timelines.clear();
	m_queue_node_counters.clear();
	m_topologically_sorted_nodes.clear();
	m_nodes_in_global_execution_order.clear();
	m_adjacency_lists.clear();
	m_detected_queue_count = 1;
	m_nodes_per_queue.clear();
	m_first_nodes_that_use_ray_tracing.clear();

	for (Node& node : m_pass_nodes) {
		node.Clear();
	}
}

void RenderPassGraph::EnsureRenderPassUniqueness(UniqueName pass_name) {
	assert(!m_render_pass_registry.count(pass_name)); // Render pass is already added to the graph.
	m_render_pass_registry.insert(pass_name);
}

void RenderPassGraph::BuildAdjacencyLists() {
	m_adjacency_lists.resize(m_pass_nodes.size());

	for (auto node_idx = 0; node_idx < m_pass_nodes.size(); ++node_idx) {
		Node& node = m_pass_nodes[node_idx];

		if (!node.HasAnyDependencies()) continue;

		std::vector<uint64_t>& adjacent_node_indices = m_adjacency_lists[node_idx];

		for (auto other_node_idx = 0; other_node_idx < m_pass_nodes.size(); ++other_node_idx) {
			// Do not check dependencies on itself
			if (node_idx == other_node_idx) continue;

			Node& other_node = m_pass_nodes[other_node_idx];

			auto establish_adjacency = [&](SubresourceName other_node_read_resource) -> bool {
				// If other node reads a subresource written by the current node, then it depends on current node and is an adjacent dependency
				bool other_node_depends_on_current_node = node.WrittenSubresources().find(other_node_read_resource) != node.WrittenSubresources().end();

				if (other_node_depends_on_current_node) {
					adjacent_node_indices.push_back(other_node_idx);
					if (node.ExecutionQueueIndex != other_node.ExecutionQueueIndex) {
						node.m_sync_signal_required = true;
						other_node.m_nodes_to_sync_with.push_back(&node);
					}

					return true;
				}

				return false;
			};

			for (SubresourceName other_node_read_resource : other_node.ReadSubresources()) {
				if (establish_adjacency(other_node_read_resource)) break;
			}

			for (SubresourceName other_node_read_resource : other_node.m_aliased_subresources) {
				if (establish_adjacency(other_node_read_resource)) break;
			}
		}
	}
}

void RenderPassGraph::DepthFirstSearch(uint64_t node_index, std::vector<bool>& visited, std::vector<bool>& on_stack, bool& is_cyclic) {
	if (is_cyclic) return;

	visited[node_index] = true;
	on_stack[node_index] = true;

	uint64_t adjacency_list_index = m_pass_nodes[node_index].m_index_in_unordered_list;

	for (uint64_t neighbour : m_adjacency_lists[adjacency_list_index]) {
		if (visited[neighbour] && on_stack[neighbour]) {
			is_cyclic = true;
			return;
		}

		if (!visited[neighbour]) {
			DepthFirstSearch(neighbour, visited, on_stack, is_cyclic);
		}
	}

	on_stack[node_index] = false;
	m_topologically_sorted_nodes.push_back(&m_pass_nodes[node_index]);
}

void RenderPassGraph::TopologicalSort() {
	std::vector<bool> visited_nodes(m_pass_nodes.size(), false);
	std::vector<bool> on_stack_nodes(m_pass_nodes.size(), false);

	bool is_cyclic = false;

	for (auto node_index = 0; node_index < m_pass_nodes.size(); ++node_index) {
		const Node& node = m_pass_nodes[node_index];

		// Visited nodes and nodes without outputs are not processed
		if (!visited_nodes[node_index] && node.HasAnyDependencies()) {
			DepthFirstSearch(node_index, visited_nodes, on_stack_nodes, is_cyclic);
			assert(!is_cyclic); // Cyclic dependency detected in pass
		}
	}

	std::reverse(m_topologically_sorted_nodes.begin(), m_topologically_sorted_nodes.end());
}

void RenderPassGraph::BuildDependencyLevels() {
	std::vector<int64_t> longest_distances(m_pass_nodes.size(), 0);

	uint64_t dependency_level_count = 1;

	// Perform longest node distance search
	for (auto node_index = 0; node_index < m_topologically_sorted_nodes.size(); ++node_index) {
		uint64_t original_index = m_topologically_sorted_nodes[node_index]->m_index_in_unordered_list;
		uint64_t adjacency_list_index = original_index;

		for (uint64_t adjacent_node_index : m_adjacency_lists[adjacency_list_index]) {
			if (longest_distances[adjacent_node_index] < longest_distances[original_index] + 1) {
				int64_t new_longest_distance = longest_distances[original_index] + 1;
				longest_distances[adjacent_node_index] = new_longest_distance;
				dependency_level_count = std::max(uint64_t(new_longest_distance + 1), dependency_level_count);
			}
		}
	}

	m_dependency_levels.resize(dependency_level_count);
	m_detected_queue_count = 1;

	// Dispatch nodes to corresponding dependency levels.
	for (auto node_index = 0; node_index < m_topologically_sorted_nodes.size(); ++node_index) {
		Node* node = m_topologically_sorted_nodes[node_index];
		uint64_t level_index = longest_distances[node->m_index_in_unordered_list];
		DependencyLevel& dependency_level = m_dependency_levels[level_index];
		dependency_level.m_level_index = level_index;
		dependency_level.AddNode(node);
		node->m_dependency_level_index = level_index;
		m_detected_queue_count = std::max(m_detected_queue_count, node->ExecutionQueueIndex + 1);
	}
}

void RenderPassGraph::FinalizeDependencyLevels() {
	uint64_t global_execution_index = 0;

	m_nodes_in_global_execution_order.resize(m_topologically_sorted_nodes.size(), nullptr);
	m_nodes_per_queue.resize(m_detected_queue_count);
	m_first_nodes_that_use_ray_tracing.resize(m_detected_queue_count);
	std::vector<const Node*> per_queue_previous_nodes(m_detected_queue_count, nullptr);

	for (DependencyLevel& dependency_level : m_dependency_levels) {
		uint64_t local_execution_index = 0;

		std::unordered_map<SubresourceName, std::unordered_set<Node::QueueIndex>> resource_reading_queue_tracker;
		dependency_level.m_nodes_per_queue.resize(m_detected_queue_count);

		for (Node* node : dependency_level.m_nodes) {
			// Track which resource is read by which queue in this dependency level
			for (SubresourceName subresource_name : node->ReadSubresources()) {
				resource_reading_queue_tracker[subresource_name].insert(node->ExecutionQueueIndex);
			}

			// Associate written subresource with render pass that writes to it for quick access when needed
			for (SubresourceName subresource_name : node->WrittenSubresources()) {
				m_written_subresource_to_pass_map[subresource_name] = node;
			}

			node->m_global_execution_index = global_execution_index;
			node->m_local_to_dependency_level_execution_index = local_execution_index;
			node->m_local_to_queue_execution_index = m_queue_node_counters[node->ExecutionQueueIndex]++;

			m_nodes_in_global_execution_order[global_execution_index] = node;

			dependency_level.m_nodes_per_queue[node->ExecutionQueueIndex].push_back(node);
			m_nodes_per_queue[node->ExecutionQueueIndex].push_back(node);

			// Add previous node on that queue as a dependency for sync optimization later
			if (per_queue_previous_nodes[node->ExecutionQueueIndex]) {
				node->m_nodes_to_sync_with.push_back(per_queue_previous_nodes[node->ExecutionQueueIndex]);
			}

			per_queue_previous_nodes[node->ExecutionQueueIndex] = node;

			for (UniqueName resource_name : node->AllResources()) {
				auto timeline_it = m_resource_usage_timelines.find(resource_name);
				bool timeline_exists = timeline_it != m_resource_usage_timelines.end();

				if (timeline_exists) {
					// Update "end" 
					timeline_it->second.second = node->GlobalExecutionIndex();
				}
				else {
					// Create "start"
					auto& timeline = m_resource_usage_timelines[resource_name];
					timeline.first = node->GlobalExecutionIndex();
					timeline.second = node->GlobalExecutionIndex();
				}
			}

			// Track first RT-using node to sync BVH builds with
			if (node->UsesRayTracing && !m_first_nodes_that_use_ray_tracing[node->ExecutionQueueIndex]) {
				m_first_nodes_that_use_ray_tracing[node->ExecutionQueueIndex] = node;
			}

			local_execution_index++;
			global_execution_index++;
		}

		// Record queue indices that are detected to read common resources
		for (auto& [subresource_name, queue_indices] : resource_reading_queue_tracker) {
			// If resource is read by more than one queue
			if (queue_indices.size() > 1) {
				for (Node::QueueIndex queue_index : queue_indices) {
					dependency_level.m_queues_involed_in_cross_queue_resource_reads.insert(queue_index);
					dependency_level.m_subresources_read_by_multiple_queues.insert(subresource_name);
				}
			}
		}
	}
}

void RenderPassGraph::CullRedundantSynchronizations() {
	// Initialize synchronization index sets
	for (Node& node : m_pass_nodes) {
		node.m_synchronization_index_set.resize(m_detected_queue_count, Node::InvalidSynchronizationIndex);
	}

	for (DependencyLevel& dependency_level : m_dependency_levels) {
		// First pass: find closest nodes to sync with, compute initial SSIS (sufficient synchronization index set)
		for (Node* node : dependency_level.m_nodes) {
			// Closest node to sync with on each queue
			std::vector<const Node*> closest_nodes_to_sync_with{ m_detected_queue_count, nullptr };

			// Find closest dependencies from other queues for the current node
			for (const Node* dependency_node : node->m_nodes_to_sync_with) {
				const Node* closest_node = closest_nodes_to_sync_with[dependency_node->ExecutionQueueIndex];

				if (!closest_node || dependency_node->LocalToQueueExecutionIndex() > closest_node->LocalToQueueExecutionIndex()) {
					closest_nodes_to_sync_with[dependency_node->ExecutionQueueIndex] = dependency_node;
				}
			}

			// Get rid of nodes to sync that may have had redundancies
			node->m_nodes_to_sync_with.clear();

			// Compute initial SSIS
			for (auto queue_idx = 0; queue_idx < m_detected_queue_count; ++queue_idx) {
				const Node* closest_node = closest_nodes_to_sync_with[queue_idx];

				if (!closest_node) {
					// If we do not have a closest node to sync with on another queue (queueIdx),
					// we need to use SSIS value for that queue from the previous node on this node's queue (closestNodesToSyncWith[node->ExecutionQueueIndex])
					// to correctly propagate SSIS values for all queues through the graph and do not lose them
					const Node* previous_node_on_nodes_queue = closest_nodes_to_sync_with[node->ExecutionQueueIndex];

					// Previous node can be null if we're dealing with first node in the queue
					if (previous_node_on_nodes_queue) {
						uint64_t sync_index_for_other_queue_from_previous_node = previous_node_on_nodes_queue->m_synchronization_index_set[queue_idx];
						node->m_synchronization_index_set[queue_idx] = sync_index_for_other_queue_from_previous_node;
					}
				}
				else {
					// Update SSIS using closest nodes' indices
					if (closest_node->ExecutionQueueIndex != node->ExecutionQueueIndex)
						node->m_synchronization_index_set[closest_node->ExecutionQueueIndex] = closest_node->LocalToQueueExecutionIndex();

					// Store only closest nodes to sync with
					node->m_nodes_to_sync_with.push_back(closest_node);
				}
			}

			// Use node's execution index as synchronization index on its own queue
			node->m_synchronization_index_set[node->ExecutionQueueIndex] = node->LocalToQueueExecutionIndex();
		}

		// Second pass: cull redundant dependencies by searching for indirect synchronizations
		for (Node* node : dependency_level.m_nodes) {
			// Keep track of queues we still need to sync with
			std::unordered_set<uint64_t> queue_to_sync_with_indices;

			// Store nodes and queue syncs they cover
			std::vector<SyncCoverage> sync_coverage_array;

			// Final optimized list of nodes without redundant dependencies
			std::vector<const Node*> optimal_nodes_to_sync_with;

			for (const Node* node_to_sync_with : node->m_nodes_to_sync_with) {
				queue_to_sync_with_indices.insert(node_to_sync_with->ExecutionQueueIndex);
			}

			while (!queue_to_sync_with_indices.empty()) {
				uint64_t max_number_of_syncs_covered_by_single_node = 0;

				for (auto dependency_node_idx = 0u; dependency_node_idx < node->m_nodes_to_sync_with.size(); ++dependency_node_idx) {
					const Node* dependency_node = node->m_nodes_to_sync_with[dependency_node_idx];

					// Take a dependency node and check how many queues we would sync with 
					// if we would only sync with this one node. We very well may encounter a case
					// where by synchronizing with just one node we will sync with more then one queue
					// or even all of them through indirect synchronizations, 
					// which will make other synchronizations previously detected for this node redundant.

					std::vector<uint64_t> synced_queue_indices;

					for (uint64_t queue_index : queue_to_sync_with_indices) {
						uint64_t current_node_desired_sync_index = node->m_synchronization_index_set[queue_index];
						uint64_t dependency_node_sync_index = dependency_node->m_synchronization_index_set[queue_index];

						assert(current_node_desired_sync_index != Node::InvalidSynchronizationIndex); // "Bug! Node that wants to sync with some queue must have a valid sync index for that queue."

						if (queue_index == node->ExecutionQueueIndex) {
							current_node_desired_sync_index -= 1;
						}

						if (dependency_node_sync_index != Node::InvalidSynchronizationIndex && dependency_node_sync_index >= current_node_desired_sync_index) {
							synced_queue_indices.push_back(queue_index);
						}
					}

					sync_coverage_array.emplace_back(SyncCoverage{ dependency_node, dependency_node_idx, synced_queue_indices });
					max_number_of_syncs_covered_by_single_node = std::max(max_number_of_syncs_covered_by_single_node, synced_queue_indices.size());
				}

				for (const SyncCoverage& sync_coverage : sync_coverage_array) {
					auto covered_sync_count = sync_coverage.SyncedQueueIndices.size();

					if (covered_sync_count >= max_number_of_syncs_covered_by_single_node) {
						// Optimal list of synchronizations should not contain nodes from the same queue,
						// because work on the same queue is synchronized automatically and implicitly
						if (sync_coverage.NodeToSyncWith->ExecutionQueueIndex != node->ExecutionQueueIndex) {
							optimal_nodes_to_sync_with.push_back(sync_coverage.NodeToSyncWith);

							// Update SSIS
							auto& index = node->m_synchronization_index_set[sync_coverage.NodeToSyncWith->ExecutionQueueIndex];
							index = std::max(index, node->m_synchronization_index_set[sync_coverage.NodeToSyncWith->ExecutionQueueIndex]);
						}

						// Remove covered queues from the list of queues we need to sync with
						for (uint64_t synced_queue_index : sync_coverage.SyncedQueueIndices) {
							queue_to_sync_with_indices.erase(synced_queue_index);
						}
					}
				}

				// Remove nodes that we synced with from the original list. Reverse iterating to avoid index invalidation.
				for (auto sync_coverage_it = sync_coverage_array.rbegin(); sync_coverage_it != sync_coverage_array.rend(); ++sync_coverage_it) {
					node->m_nodes_to_sync_with.erase(node->m_nodes_to_sync_with.begin() + sync_coverage_it->NodeToSyncWithIndex);
				}
			}

			// Finally, assign an optimal list of nodes to sync with to the current node
			node->m_nodes_to_sync_with = optimal_nodes_to_sync_with;
		}
	}
}
