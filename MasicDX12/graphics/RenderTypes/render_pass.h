#pragma once

#include <string>

#include "render_pass_metadata.h"

class RenderPass {
public:
    RenderPass(const std::string& name, RenderPassPurpose purpose = RenderPassPurpose::Default);
    virtual ~RenderPass() = 0;

    virtual void SetupRootSignatures() = 0;
    virtual void SetupPipelineStates() = 0;
    virtual void ScheduleResources() {};
    virtual void ScheduleSubPasses() {};
    virtual void ScheduleSamplers() {};
    virtual void Render() {};

    const RenderPassMetadata& GetMetadata() const;

private:
    RenderPassMetadata m_metadata;
};    