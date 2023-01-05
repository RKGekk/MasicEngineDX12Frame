#include "render_pass.h"

RenderPass::RenderPass(const std::string& name, RenderPassPurpose purpose) : m_metadata{ name, purpose } {}

const RenderPassMetadata& RenderPass::GetMetadata() const {
	return m_metadata;
}