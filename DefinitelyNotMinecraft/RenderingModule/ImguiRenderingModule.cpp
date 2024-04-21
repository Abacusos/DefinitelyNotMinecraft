#include <Core/Config.hpp>
#include <Core/Profiler.hpp>
#include <RenderingModule/ImguiRenderingModule.hpp>

#include "imgui.h"
#include "imgui_impl_vulkan.h"

namespace dnm {
ImguiRenderingModule::ImguiRenderingModule(Config* config, Renderer* renderer)
    : m_config{config}, m_renderer{renderer} {
  const auto& device = m_renderer->getDevice();

  m_renderPass =
      makeRenderPass(device, renderer->getColorFormat(), vk::Format::eD32Sfloat,
                     vk::AttachmentLoadOp::eLoad);

  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = *renderer->getInstance();
  init_info.PhysicalDevice = *renderer->getPhysicalDevice();
  init_info.Device = *renderer->getDevice();
  init_info.QueueFamily = renderer->getIndices().graphicsQueueFamilyIndex;
  init_info.Queue = *renderer->getGraphicsQueue();
  init_info.PipelineCache = *renderer->getPipelineCache();
  init_info.DescriptorPool = *m_renderer->getDescriptorPool();
  init_info.Subpass = 0;
  init_info.MinImageCount = 2;
  init_info.ImageCount = 2;
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  init_info.Allocator = nullptr;
  ImGui_ImplVulkan_Init(&init_info, *m_renderPass);

  m_commandBuffer = renderer->getCommandBuffer();
  registerDebugMarker(device, m_commandBuffer, "imgui Command Buffer");

  m_renderingFinished = vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
  registerDebugMarker(device, m_renderingFinished, "Finished imgui rendering");
  uploadFontTexture();
}

ImguiRenderingModule::~ImguiRenderingModule() { ImGui_ImplVulkan_Shutdown(); }

void ImguiRenderingModule::drawFrame(
    const vk::raii::Framebuffer& frameBuffer, TimeSpan dt,
    const vk::raii::Semaphore& previousRenderStepFinished) const {
  ImGui_ImplVulkan_NewFrame();

  ImGui::Render();
  ImDrawData* draw_data = ImGui::GetDrawData();
  const bool is_minimized =
      (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
  if (!is_minimized) {
    const auto extent = m_renderer->getExtent();

    std::array<vk::ClearValue, 2> clearValues;
    clearValues[0].color = vk::ClearColorValue(0.45f, 0.55f, 0.60f, 0.2f);
    clearValues[1].depthStencil = vk::ClearDepthStencilValue(0.0f, 0);

    const vk::RenderPassBeginInfo renderPassBeginInfo(
        *m_renderPass, *frameBuffer, vk::Rect2D(vk::Offset2D(0, 0), extent),
        clearValues);

    m_commandBuffer.reset();

    m_commandBuffer.begin(vk::CommandBufferBeginInfo(
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    m_commandBuffer.beginRenderPass(renderPassBeginInfo,
                                    vk::SubpassContents::eInline);

    // Record dear imgui primitives into command buffer
    ImGui_ImplVulkan_RenderDrawData(draw_data, *m_commandBuffer);

    // Submit command buffer

    m_commandBuffer.endRenderPass();
    m_commandBuffer.end();

    const auto graphicsQueue = m_renderer->getGraphicsQueue();

    vk::PipelineStageFlags waitDestinationStageMask(
        vk::PipelineStageFlagBits::eColorAttachmentOutput);

    const vk::SubmitInfo graphicsInfo{*previousRenderStepFinished,
                                      waitDestinationStageMask,
                                      *m_commandBuffer, *m_renderingFinished};
    graphicsQueue.submit(graphicsInfo);
  }
}

vk::raii::Semaphore& ImguiRenderingModule::getRenderingFinishedSemaphore() {
  return m_renderingFinished;
}

void ImguiRenderingModule::recreatePipeline() { m_renderer->waitIdle(); }

void ImguiRenderingModule::uploadFontTexture() {
  const ImGuiIO& io = ImGui::GetIO();

  unsigned char* pixels;
  int width, height;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  size_t upload_size = width * height * 4 * sizeof(char);

  VkResult err;

  auto& physicalDevice = m_renderer->getPhysicalDevice();
  auto& device = m_renderer->getDevice();

  m_fontTexture = TextureData(
      physicalDevice, device, vk::Extent2D(width, height),
      vk::Format::eB8G8R8A8Unorm, {}, 1u,
      vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
  registerDebugMarker(device, m_fontTexture.imageData.image,
                      "Imgui Font Texture");

  m_renderer->oneTimeSubmit([&](const vk::raii::CommandBuffer& commandBuffer) {
    m_fontTexture.setImage(commandBuffer, pixels);
  });

  std::array layout{std::tuple<vk::DescriptorType, u32, vk::ShaderStageFlags>{
      vk::DescriptorType::eCombinedImageSampler, 1,
      vk::ShaderStageFlagBits::eFragment}};

  m_descriptorSet.clear();

  m_descriptorSetLayout = makeDescriptorSetLayout(device, layout);
  m_pipelineLayout =
      vk::raii::PipelineLayout(device, {{}, *m_descriptorSetLayout});

  auto sets = vk::raii::DescriptorSets(
      device, {*m_renderer->getDescriptorPool(), *m_descriptorSetLayout});
  m_descriptorSet = std::move(sets.front());

  updateDescriptorSets(device, m_descriptorSet, {}, {m_fontTexture});

  io.Fonts->SetTexID((ImTextureID)*m_descriptorSet);
}
}  // namespace dnm