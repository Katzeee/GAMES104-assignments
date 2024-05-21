#include "runtime/function/render/include/render/vulkan_manager/vulkan_common.h"
#include "runtime/function/render/include/render/vulkan_manager/vulkan_mesh.h"
#include "runtime/function/render/include/render/vulkan_manager/vulkan_misc.h"
#include "runtime/function/render/include/render/vulkan_manager/vulkan_passes.h"
#include "runtime/function/render/include/render/vulkan_manager/vulkan_util.h"

#include <random>
#include <ssao_frag.h>
#include <post_process_vert.h>

namespace Pilot
{
    void PSsaoPass::initialize(VkRenderPass render_pass, VkImageView input_attachment, VkImageView g_buffer_normal_attachment)
    {
        _framebuffer.render_pass = render_pass;
        setupDescriptorSetLayout();
        setupPipelines();
        setupDescriptorSet();
        updateAfterFramebufferRecreate(input_attachment, g_buffer_normal_attachment);
    }

    // set descriptor set layout
    void PSsaoPass::setupDescriptorSetLayout()
    {
        _descriptor_infos.resize(1);

        VkDescriptorSetLayoutBinding ssao_desc_set_layout_bindings[4]{};

        auto& color_attachment_binding = ssao_desc_set_layout_bindings[0];
        color_attachment_binding.binding = 0;
        color_attachment_binding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        color_attachment_binding.descriptorCount = 1;
        color_attachment_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        auto& depth_attachment_binding = ssao_desc_set_layout_bindings[1];
        depth_attachment_binding.binding = 1;
        depth_attachment_binding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        depth_attachment_binding.descriptorCount = 1;
        depth_attachment_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        auto& g_buffer_normal_attachment_binding = ssao_desc_set_layout_bindings[2];
        g_buffer_normal_attachment_binding.binding = 2;
        g_buffer_normal_attachment_binding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        g_buffer_normal_attachment_binding.descriptorCount = 1;
        g_buffer_normal_attachment_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        auto& sample_points_binding = ssao_desc_set_layout_bindings[3];
        sample_points_binding.binding = 3;
        sample_points_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        sample_points_binding.descriptorCount = 1;
        sample_points_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo ssao_desc_set_layout_create_info{};
        ssao_desc_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ssao_desc_set_layout_create_info.pNext = nullptr;
        ssao_desc_set_layout_create_info.flags = 0;
        ssao_desc_set_layout_create_info.bindingCount = ARRAY_SIZE(ssao_desc_set_layout_bindings);
        ssao_desc_set_layout_create_info.pBindings = ssao_desc_set_layout_bindings;

        if (VK_SUCCESS != vkCreateDescriptorSetLayout(m_p_vulkan_context->_device,
                                                      &ssao_desc_set_layout_create_info,
                                                      nullptr,
                                                      &_descriptor_infos[0].layout))
        {
            throw std::runtime_error("create ssao layout error");
        }

        std::uniform_real_distribution<float> random_float(0.0f, 1.0f);
        std::default_random_engine generator;
        for (auto & sample_point : m_per_frame_data.sample_points) 
        {
            sample_point.point = {
                random_float(generator) * 2.0f - 1.0f,
                random_float(generator) * 2.0f - 1.0f, 
                random_float(generator)};
            sample_point.point.normalise();
            sample_point._padding_point = 0.0f;
        }
        std::memcpy(m_p_global_render_resource->_storage_buffer._ssao_sample_storage_buffer_memory_pointer, &m_per_frame_data, sizeof(PerFrameData));
    }

    void PSsaoPass::setupPipelines()
    {
        _render_pipelines.resize(1);

        VkDescriptorSetLayout descriptor_set_layouts[1] = {_descriptor_infos[0].layout};
        VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
        pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_create_info.setLayoutCount = 1;
        pipeline_layout_create_info.pSetLayouts = descriptor_set_layouts;

        if (VK_SUCCESS != vkCreatePipelineLayout(m_p_vulkan_context->_device, &pipeline_layout_create_info, nullptr, &_render_pipelines[0].layout))
        {
            throw std::runtime_error("create ssao pipeline error");
        }

        VkShaderModule vert_shader_module = 
            PVulkanUtil::createShaderModule(m_p_vulkan_context->_device, POST_PROCESS_VERT);
        VkShaderModule frag_shader_module = 
            PVulkanUtil::createShaderModule(m_p_vulkan_context->_device, SSAO_FRAG);

        VkPipelineShaderStageCreateInfo vert_pipeline_shader_stage_create_info{};
        vert_pipeline_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vert_pipeline_shader_stage_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vert_pipeline_shader_stage_create_info.module = vert_shader_module;
        vert_pipeline_shader_stage_create_info.pName = "main";

        VkPipelineShaderStageCreateInfo frag_pipeline_shader_stage_create_info{};
        frag_pipeline_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        frag_pipeline_shader_stage_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        frag_pipeline_shader_stage_create_info.module = frag_shader_module;
        frag_pipeline_shader_stage_create_info.pName = "main";

        VkPipelineShaderStageCreateInfo shader_stages[] = { vert_pipeline_shader_stage_create_info, frag_pipeline_shader_stage_create_info };

        VkPipelineVertexInputStateCreateInfo vert_input_state_create_info{};
        vert_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vert_input_state_create_info.vertexBindingDescriptionCount = 0;
        vert_input_state_create_info.pVertexBindingDescriptions = nullptr;
        vert_input_state_create_info.vertexAttributeDescriptionCount = 0;
        vert_input_state_create_info.pVertexAttributeDescriptions = nullptr;

        VkPipelineInputAssemblyStateCreateInfo input_asm_create_info{};
        input_asm_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_asm_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        input_asm_create_info.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewport_state_create_info{};
        viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state_create_info.viewportCount = 1;
        viewport_state_create_info.pViewports = &m_command_info._viewport;
        viewport_state_create_info.scissorCount = 1;
        viewport_state_create_info.pScissors = &m_command_info._scissor;

        VkPipelineRasterizationStateCreateInfo rasterization_state_create_info{};
        rasterization_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization_state_create_info.depthClampEnable = VK_FALSE;
        rasterization_state_create_info.rasterizerDiscardEnable = VK_FALSE;
        rasterization_state_create_info.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization_state_create_info.lineWidth = 1.0f;
        rasterization_state_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterization_state_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterization_state_create_info.depthBiasEnable = VK_FALSE;
        rasterization_state_create_info.depthBiasConstantFactor = 0.0f;
        rasterization_state_create_info.depthBiasClamp = 0.0f;
        rasterization_state_create_info.depthBiasSlopeFactor = 0.0f;

        VkPipelineMultisampleStateCreateInfo multisample_state_create_info{};
        multisample_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample_state_create_info.sampleShadingEnable = VK_FALSE;
        multisample_state_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState color_blend_attachment_state{};
        color_blend_attachment_state.colorWriteMask = 
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment_state.blendEnable = VK_FALSE;
        color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo color_blend_state_create_info{};
        color_blend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state_create_info.logicOpEnable = VK_FALSE;
        color_blend_state_create_info.logicOp = VK_LOGIC_OP_COPY;
        color_blend_state_create_info.attachmentCount = 1;
        color_blend_state_create_info.pAttachments = &color_blend_attachment_state;
        color_blend_state_create_info.blendConstants[0] = 0.0f;
        color_blend_state_create_info.blendConstants[1] = 0.0f;
        color_blend_state_create_info.blendConstants[2] = 0.0f;
        color_blend_state_create_info.blendConstants[3] = 0.0f;

        VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info{};
        depth_stencil_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil_create_info.depthTestEnable = VK_TRUE;
        depth_stencil_create_info.depthWriteEnable = VK_TRUE;
        depth_stencil_create_info.depthCompareOp = VK_COMPARE_OP_LESS;
        depth_stencil_create_info.depthBoundsTestEnable = VK_FALSE;
        depth_stencil_create_info.stencilTestEnable = VK_FALSE;

        VkDynamicState dynamic_state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

        VkPipelineDynamicStateCreateInfo dynamic_state_create_info{};
        dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state_create_info.dynamicStateCount = 2;
        dynamic_state_create_info.pDynamicStates = dynamic_state;

        VkGraphicsPipelineCreateInfo pipeline_create_info{};
        pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_create_info.stageCount = 2;
        pipeline_create_info.pStages = shader_stages;
        pipeline_create_info.pVertexInputState = &vert_input_state_create_info;
        pipeline_create_info.pInputAssemblyState = &input_asm_create_info;
        pipeline_create_info.pViewportState = &viewport_state_create_info;
        pipeline_create_info.pRasterizationState = &rasterization_state_create_info;
        pipeline_create_info.pMultisampleState = &multisample_state_create_info;
        pipeline_create_info.pColorBlendState = &color_blend_state_create_info;
        pipeline_create_info.pDepthStencilState = &depth_stencil_create_info;
        pipeline_create_info.layout = _render_pipelines[0].layout;
        pipeline_create_info.renderPass = _framebuffer.render_pass;
        pipeline_create_info.subpass = _main_camera_subpass_ssao;
        pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_create_info.pDynamicState = &dynamic_state_create_info;

        if (VK_SUCCESS != vkCreateGraphicsPipelines(m_p_vulkan_context->_device,
                                                    VK_NULL_HANDLE,
                                                    1,
                                                    &pipeline_create_info,
                                                    nullptr,
                                                    &_render_pipelines[0].pipeline))
        {
            throw std::runtime_error("create ssao graphics pipeline fail");
        }

        vkDestroyShaderModule(m_p_vulkan_context->_device, vert_shader_module, nullptr);
        vkDestroyShaderModule(m_p_vulkan_context->_device, frag_shader_module, nullptr);
    }

    // allocate descriptor set's memory
    void PSsaoPass::setupDescriptorSet()
    {
        VkDescriptorSetAllocateInfo descriptor_set_allocate_info{};
        descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptor_set_allocate_info.pNext = nullptr;
        descriptor_set_allocate_info.descriptorPool = m_descriptor_pool;
        descriptor_set_allocate_info.descriptorSetCount = 1;
        descriptor_set_allocate_info.pSetLayouts = &_descriptor_infos[0].layout;

        if (VK_SUCCESS != vkAllocateDescriptorSets(m_p_vulkan_context->_device, &descriptor_set_allocate_info, &_descriptor_infos[0].descriptor_set))
        {
            throw std::runtime_error("allocate ssao descriptor set fail");
        }

        VkDescriptorBufferInfo sample_point_buffer_info{};
        sample_point_buffer_info.offset = 0;
        sample_point_buffer_info.range = sizeof(PerFrameData);
        sample_point_buffer_info.buffer = m_p_global_render_resource->_storage_buffer._ssao_sample_storage_buffer;

        VkWriteDescriptorSet ssao_descriptor_set[1]{};
        auto &sample_points_descriptor_set = ssao_descriptor_set[0];
        sample_points_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sample_points_descriptor_set.pNext = nullptr;
        sample_points_descriptor_set.dstSet = _descriptor_infos[0].descriptor_set;
        sample_points_descriptor_set.dstBinding = 3;
        sample_points_descriptor_set.dstArrayElement = 0;
        sample_points_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        sample_points_descriptor_set.descriptorCount = 1;
        sample_points_descriptor_set.pBufferInfo = &sample_point_buffer_info;

        vkUpdateDescriptorSets(m_p_vulkan_context->_device,
                               ARRAY_SIZE(ssao_descriptor_set),
                               ssao_descriptor_set,
                               0,
                               nullptr);
    }

    // bind descriptor
    void PSsaoPass::updateAfterFramebufferRecreate(VkImageView cur_frame_attachment, VkImageView g_buffer_normal_attachment)
    {
        VkDescriptorImageInfo cur_frame_image_info{};
        cur_frame_image_info.sampler = 
            PVulkanUtil::getOrCreateNearestSampler(m_p_vulkan_context->_physical_device, m_p_vulkan_context->_device);
        cur_frame_image_info.imageView = cur_frame_attachment;
        cur_frame_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo depth_buffer_image_info{};
        depth_buffer_image_info.sampler = 
            PVulkanUtil::getOrCreateNearestSampler(m_p_vulkan_context->_physical_device, m_p_vulkan_context->_device);
        depth_buffer_image_info.imageView = m_p_vulkan_context->_depth_image_view;
        depth_buffer_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo g_buffer_normal_image_info{};
        g_buffer_normal_image_info.sampler = 
            PVulkanUtil::getOrCreateNearestSampler(m_p_vulkan_context->_physical_device, m_p_vulkan_context->_device);
        g_buffer_normal_image_info.imageView = g_buffer_normal_attachment;
        g_buffer_normal_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet ssao_descriptor_set[3]{};

        auto& cur_frame_attachment_ssao_descriptor_set = ssao_descriptor_set[0];
        cur_frame_attachment_ssao_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        cur_frame_attachment_ssao_descriptor_set.pNext = nullptr;
        cur_frame_attachment_ssao_descriptor_set.dstSet = _descriptor_infos[0].descriptor_set;
        cur_frame_attachment_ssao_descriptor_set.dstBinding = 0;
        cur_frame_attachment_ssao_descriptor_set.dstArrayElement = 0;
        cur_frame_attachment_ssao_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        cur_frame_attachment_ssao_descriptor_set.descriptorCount = 1;
        cur_frame_attachment_ssao_descriptor_set.pImageInfo = &cur_frame_image_info;

        auto& depth_attachment_ssao_descriptor_set = ssao_descriptor_set[1];
        depth_attachment_ssao_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        depth_attachment_ssao_descriptor_set.pNext = nullptr;
        depth_attachment_ssao_descriptor_set.dstSet = _descriptor_infos[0].descriptor_set;
        depth_attachment_ssao_descriptor_set.dstBinding = 1;
        depth_attachment_ssao_descriptor_set.dstArrayElement = 0;
        depth_attachment_ssao_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        depth_attachment_ssao_descriptor_set.descriptorCount = 1;
        depth_attachment_ssao_descriptor_set.pImageInfo = &depth_buffer_image_info;

        auto& g_buffer_normal_attachment_ssao_descriptor_set = ssao_descriptor_set[2];
        g_buffer_normal_attachment_ssao_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        g_buffer_normal_attachment_ssao_descriptor_set.pNext = nullptr;
        g_buffer_normal_attachment_ssao_descriptor_set.dstSet = _descriptor_infos[0].descriptor_set;
        g_buffer_normal_attachment_ssao_descriptor_set.dstBinding = 2;
        g_buffer_normal_attachment_ssao_descriptor_set.dstArrayElement = 0;
        g_buffer_normal_attachment_ssao_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        g_buffer_normal_attachment_ssao_descriptor_set.descriptorCount = 1;
        g_buffer_normal_attachment_ssao_descriptor_set.pImageInfo = &g_buffer_normal_image_info;

        vkUpdateDescriptorSets(m_p_vulkan_context->_device, 
                               ARRAY_SIZE(ssao_descriptor_set),
                               ssao_descriptor_set,
                               0,
                               nullptr);

    }

    void PSsaoPass::draw()
    {
        if(m_render_config._enable_debug_untils_label)
        {
            VkDebugUtilsLabelEXT label_info = 
                { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, nullptr, "SSAO", {1.0f, 1.0f, 1.0f, 1.0f}};
            m_p_vulkan_context->_vkCmdBeginDebugUtilsLabelEXT(m_command_info._current_command_buffer, &label_info);
        }
        m_p_vulkan_context->_vkCmdBindPipeline(m_command_info._current_command_buffer, 
                                               VK_PIPELINE_BIND_POINT_GRAPHICS, 
                                               _render_pipelines[0].pipeline);
        m_p_vulkan_context->_vkCmdSetViewport(m_command_info._current_command_buffer, 0, 1, &m_command_info._viewport);
        m_p_vulkan_context->_vkCmdSetScissor(m_command_info._current_command_buffer, 0, 1, &m_command_info._scissor);
        reinterpret_cast<PerFrameData*>(reinterpret_cast<uintptr_t>(m_p_global_render_resource->_storage_buffer._ssao_sample_storage_buffer_memory_pointer))->near_plane = m_per_frame_data.near_plane;
        reinterpret_cast<PerFrameData*>(reinterpret_cast<uintptr_t>(m_p_global_render_resource->_storage_buffer._ssao_sample_storage_buffer_memory_pointer))->far_plane = m_per_frame_data.far_plane;
        reinterpret_cast<PerFrameData*>(reinterpret_cast<uintptr_t>(m_p_global_render_resource->_storage_buffer._ssao_sample_storage_buffer_memory_pointer))->proj_mat = m_per_frame_data.proj_mat;
        m_p_vulkan_context->_vkCmdBindDescriptorSets(m_command_info._current_command_buffer, 
                                                     VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                     _render_pipelines[0].layout,
                                                     0,
                                                     1,
                                                     &_descriptor_infos[0].descriptor_set,
                                                     0,
                                                     nullptr);
        vkCmdDraw(m_command_info._current_command_buffer, 3, 1, 0, 0);

        if (m_render_config._enable_debug_untils_label)
        {
            m_p_vulkan_context->_vkCmdEndDebugUtilsLabelEXT(m_command_info._current_command_buffer);
        }
    }

} // end Pilot