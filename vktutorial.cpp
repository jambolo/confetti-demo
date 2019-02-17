#define GLFW_INCLUDE_VULKAN
#include "Glfwx/Glfwx.h"
#include "Vkx/Vkx.h"

#include <vulkan/vulkan.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>
#include <stdexcept>

template <typename T>
class optional
{
public:
    optional() = default;
    optional(T const & value) : value_(value)
        , hasValue_(true)
    {
    }
    optional & operator =(T const & rhs)
    {
        value_    = rhs;
        hasValue_ = true;
        return *this;
    }
    bool hasValue() const
    {
        return hasValue_;
    }
    T value() const
    {
        if (hasValue_)
            return value_;
        else
            throw std::exception("bad_optional_access");
    }
private:
    T value_;
    bool hasValue_ = false;
};

#ifdef NDEBUG
static bool constexpr VALIDATION_LAYERS_REQUESTED = false;
#else
static bool constexpr VALIDATION_LAYERS_REQUESTED = true;
#endif

std::vector<char const *> const VALIDATION_LAYERS =
{
    "VK_LAYER_LUNARG_standard_validation",
};

std::vector<char const *> const DEVICE_EXTENSIONS =
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

class HelloTriangleApplication
{
public:
    void run()
    {
        initializeWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:

    static int constexpr WIDTH  = 800;
    static int constexpr HEIGHT = 600;

    struct QueueFamilyIndices
    {
        optional<uint32_t> graphicsFamily;
        optional<uint32_t> presentFamily;
        bool isComplete() const
        {
            return graphicsFamily.hasValue() && presentFamily.hasValue();
        }
    };

    struct SwapChainSupportInfo
    {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };

    int initializeWindow()
    {
        window_ = new Glfwx::Window(WIDTH, HEIGHT, "vktutorial");
        Glfwx::Window::hint(Glfwx::Hint::eCLIENT_API, Glfwx::eNO_API);
        Glfwx::Window::hint(Glfwx::Hint::eRESIZABLE, Glfwx::eFALSE);
        return window_->create();
    }

    void initVulkan()
    {
        vk::ApplicationInfo appInfo("Hello Triangle",
                                    VK_MAKE_VERSION(1, 0, 0),
                                    "No Engine",
                                    VK_MAKE_VERSION(1, 0, 0),
                                    VK_API_VERSION_1_0);

        std::vector<char const *> requiredExtensions = myRequiredExtensions();
        vk::InstanceCreateInfo    createInfo;
        createInfo.pApplicationInfo        = &appInfo;
        createInfo.enabledExtensionCount   = (uint32_t)requiredExtensions.size();
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();
        instance_ = vk::createInstance(createInfo);

        dynamicLoader_.init(instance_);

        if (VALIDATION_LAYERS_REQUESTED && !Vkx::allLayersAvailable(VALIDATION_LAYERS))
            throw std::runtime_error("validation layers requested, but not available!");

        setupDebugMessenger();
        surface_ = window_->createSurface(instance_, nullptr);
        vk::PhysicalDevice physicalDevice = firstSuitablePhysicalDevice();
        createLogicalDevice(physicalDevice);
        createSwapChain(physicalDevice);
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFrameBuffers();
    }

    std::vector<char const *> myRequiredExtensions()
    {
        std::vector<char const *> requiredExtensions = Glfwx::requiredInstanceExtensions();

        if (VALIDATION_LAYERS_REQUESTED)
            requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        return requiredExtensions;
    }

    void setupDebugMessenger()
    {
        if (!VALIDATION_LAYERS_REQUESTED)
            return;
        vk::DebugUtilsMessengerCreateInfoEXT info({},
                                                  (vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                                                   vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                   vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                                                   vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo),
                                                  (vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                                   vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                                                   vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance),
                                                  debugCallback);
        messenger_ = instance_.createDebugUtilsMessengerEXT(info, nullptr, dynamicLoader_);
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT       messageSeverity,
                                                        VkDebugUtilsMessageTypeFlagsEXT              messageTypes,
                                                        VkDebugUtilsMessengerCallbackDataEXT const * pCallbackData,
                                                        void *                                       pUserData)
    {
        std::cerr << "(" << vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity)) << ")"
                  << vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageTypes)) << " - "
                  << pCallbackData->pMessage
                  << std::endl;
        return VK_FALSE;
    }

    vk::PhysicalDevice firstSuitablePhysicalDevice()
    {
        std::vector<vk::PhysicalDevice> physicalDevices = instance_.enumeratePhysicalDevices();
        for (auto const & device : physicalDevices)
        {
            if (isSuitable(device))
                return device;
        }

        throw std::runtime_error("failed to find a suitable GPU!");
    }

    bool isSuitable(vk::PhysicalDevice const & device)
    {
//         bool suitable = false;
//         vk::PhysicalDeviceProperties properties = device.getProperties();
//         vk::PhysicalDeviceFeatures features = device.getFeatures();
//
//         if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu && features.geometryShader)
//             suitable = true;
//
//         return suitable;
        QueueFamilyIndices indices = findQueueFamilies(device);
        bool extensionsSupported   = Vkx::allExtensionsSupported(device, DEVICE_EXTENSIONS);
        bool swapChainAdequate     = false;
        if (extensionsSupported)
        {
            SwapChainSupportInfo swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }
        return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    SwapChainSupportInfo querySwapChainSupport(vk::PhysicalDevice device)
    {
        return
            {
                device.getSurfaceCapabilitiesKHR(surface_),
                device.getSurfaceFormatsKHR(surface_),
                device.getSurfacePresentModesKHR(surface_)
            };
    }

    void createLogicalDevice(vk::PhysicalDevice const & physicalDevice)
    {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        // De-duplicate the family indices
        std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

        float priority = 1.0f;
        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
        for (auto const & i : uniqueQueueFamilies)
        {
            queueCreateInfos.emplace_back(vk::DeviceQueueCreateFlags(), i, 1, &priority);
        }

        vk::PhysicalDeviceFeatures deviceFeatures;
        vk::DeviceCreateInfo       createInfo({},
                                              (uint32_t)queueCreateInfos.size(), queueCreateInfos.data(),
                                              0, nullptr,
                                              (int)DEVICE_EXTENSIONS.size(), DEVICE_EXTENSIONS.data(),
                                              &deviceFeatures);
        if (VALIDATION_LAYERS_REQUESTED)
        {
            createInfo.setPpEnabledLayerNames(VALIDATION_LAYERS.data());
            createInfo.setEnabledLayerCount(static_cast<uint32_t>(VALIDATION_LAYERS.size()));
        }

        device_        = physicalDevice.createDevice(createInfo);
        graphicsQueue_ = device_.getQueue(indices.graphicsFamily.value(), 0);
        presentQueue_  = device_.getQueue(indices.presentFamily.value(), 0);
    }

    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device)
    {
        std::vector<vk::QueueFamilyProperties> families = device.getQueueFamilyProperties();

        QueueFamilyIndices indices;
        for (int i = 0; i < (int)families.size(); ++i)
        {
            vk::QueueFamilyProperties const & f = families[i];
            if (f.queueCount > 0 && f.queueFlags & vk::QueueFlagBits::eGraphics)
                indices.graphicsFamily = i;
            VkBool32 presentSupport = device.getSurfaceSupportKHR(i, surface_);
            if (f.queueCount > 0 && presentSupport)
                indices.presentFamily = i;

            if (indices.isComplete())
                break;
        }
        return indices;
    }

    void createSwapChain(vk::PhysicalDevice device)
    {
        SwapChainSupportInfo swapChainSupport = querySwapChainSupport(device);

        vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        vk::PresentModeKHR   presentMode   = chooseSwapPresentMode(swapChainSupport.presentModes);
        vk::Extent2D         extent        = chooseSwapExtent(swapChainSupport.capabilities);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
            imageCount = swapChainSupport.capabilities.maxImageCount;

        vk::SwapchainCreateInfoKHR createInfo(vk::SwapchainCreateFlagsKHR(),
                                              surface_,
                                              imageCount,
                                              surfaceFormat.format,
                                              surfaceFormat.colorSpace,
                                              extent,
                                              1,
                                              vk::ImageUsageFlagBits::eColorAttachment);
        QueueFamilyIndices indices = findQueueFamilies(device);
        uint32_t           queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

        if (indices.graphicsFamily.value() != indices.presentFamily.value())
        {
            createInfo.imageSharingMode      = vk::SharingMode::eConcurrent;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices   = queueFamilyIndices;
        }
        else
        {
            createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        }
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.presentMode  = presentMode;
        createInfo.clipped      = VK_TRUE;

        swapChain_            = device_.createSwapchainKHR(createInfo);
        swapChainImages_      = device_.getSwapchainImagesKHR(swapChain_);
        swapChainImageFormat_ = surfaceFormat.format;
        swapChainExtent_      = extent;
    }

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const & available)
    {
        // We get to choose ...
        if (available.size() == 1 && available[0].format == vk::Format::eUndefined)
            return { vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };

        // Has what we want?
        for (auto const & format : available)
        {
            if (format.format == vk::Format::eB8G8R8A8Unorm && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
                return format;
        }

        // Whatever ...
        return available[0];
    }

    vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const & available)
    {
        // Normally VK_PRESENT_MODE_FIFO_KHR is preferred over VK_PRESENT_MODE_IMMEDIATE_KHR, but ...
        //      "Unfortunately some drivers currently don't properly support VK_PRESENT_MODE_FIFO_KHR, so we should prefer
        //      VK_PRESENT_MODE_IMMEDIATE_KHR if VK_PRESENT_MODE_MAILBOX_KHR is not available."

        vk::PresentModeKHR bestMode = vk::PresentModeKHR::eFifo;

        for (auto const & mode : available)
        {
            if (mode == vk::PresentModeKHR::eMailbox)
                return mode;
            else if (mode == vk::PresentModeKHR::eImmediate)
                bestMode = mode;
        }

        return bestMode;
    }

    vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const & capabilities)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }
        else
        {
            uint32_t width, height;

            width  = std::clamp((uint32_t)WIDTH, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            height = std::clamp((uint32_t)HEIGHT, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return { width, height };
        }
    }

    void createImageViews()
    {
        vk::ImageSubresourceRange subresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        swapChainImageViews_.reserve(swapChainImages_.size());
        for (auto const & image : swapChainImages_)
        {
            vk::ImageViewCreateInfo createInfo(vk::ImageViewCreateFlags(),
                                               image,
                                               vk::ImageViewType::e2D,
                                               swapChainImageFormat_,
                                               vk::ComponentMapping(),
                                               subresourceRange);
            swapChainImageViews_.push_back(device_.createImageView(createInfo));
        }
    }

    void createRenderPass()
    {
        vk::AttachmentDescription colorAttachment(vk::AttachmentDescriptionFlags(),
                                                  swapChainImageFormat_,
                                                  vk::SampleCountFlagBits::e1,
                                                  vk::AttachmentLoadOp::eClear,
                                                  vk::AttachmentStoreOp::eStore,
                                                  vk::AttachmentLoadOp::eDontCare,
                                                  vk::AttachmentStoreOp::eDontCare,
                                                  vk::ImageLayout::eUndefined,
                                                  vk::ImageLayout::ePresentSrcKHR);

        vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);
        vk::SubpassDescription  subpass(vk::SubpassDescriptionFlags(),
                                        vk::PipelineBindPoint::eGraphics,
                                        1,
                                        &colorAttachmentRef);

        vk::RenderPassCreateInfo renderPassInfo(vk::RenderPassCreateFlags(),
                                                1, &colorAttachment, 1, &subpass);
        renderPass_ = device_.createRenderPass(renderPassInfo);
    }

    void createGraphicsPipeline()
    {
        vk::ShaderModule vertShaderModule = Vkx::loadShaderModule("shaders/shader.vert.spv", device_);
        vk::ShaderModule fragShaderModule = Vkx::loadShaderModule("shaders/shader.frag.spv", device_);

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo(vk::PipelineShaderStageCreateFlags(),
                                                              vk::ShaderStageFlagBits::eVertex,
                                                              vertShaderModule,
                                                              "main");

        vk::PipelineVertexInputStateCreateInfo vertexInputInfo;

        vk::PipelineInputAssemblyStateCreateInfo inputAssembly(vk::PipelineInputAssemblyStateCreateFlags(),
                                                               vk::PrimitiveTopology::eTriangleList,
                                                               VK_FALSE);

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo(vk::PipelineShaderStageCreateFlags(),
                                                              vk::ShaderStageFlagBits::eFragment,
                                                              fragShaderModule,
                                                              "main");

        vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        vk::Viewport viewport(0.0f, 0.0f, (float)swapChainExtent_.width, (float)swapChainExtent_.height, 0.0f, 1.0f);
        vk::Rect2D   scissor({ 0, 0 }, swapChainExtent_);
        vk::PipelineViewportStateCreateInfo viewportState(vk::PipelineViewportStateCreateFlags(), 1, &viewport, 1, &scissor);

        vk::PipelineRasterizationStateCreateInfo rasterizer(vk::PipelineRasterizationStateCreateFlags(),
                                                            VK_FALSE,
                                                            VK_FALSE,
                                                            vk::PolygonMode::eFill,
                                                            vk::CullModeFlagBits::eBack,
                                                            vk::FrontFace::eClockwise);

        vk::PipelineMultisampleStateCreateInfo multisampling;

//         vk::PipelineColorBlendAttachmentState colorBlendAttachment(VK_TRUE,
//                                                                    vk::BlendFactor::eSrcAlpha,
//                                                                    vk::BlendFactor::eOneMinusSrcAlpha,
//                                                                    vk::BlendOp::eAdd,
//                                                                    vk::BlendFactor::eOne,
//                                                                    vk::BlendFactor::eZero,
//                                                                    vk::BlendOp::eAdd,
//                                                                    vk::ColorComponentFlags(
//                                                                        vk::FlagTraits<vk::ColorComponentFlags>::allFlags));
        vk::PipelineColorBlendAttachmentState colorBlendAttachment;
        colorBlendAttachment.setColorWriteMask(vk::ColorComponentFlags(vk::FlagTraits<vk::ColorComponentFlags>::allFlags));

        vk::PipelineColorBlendStateCreateInfo colorBlending;
        colorBlending.setPAttachments(&colorBlendAttachment);
        colorBlending.setAttachmentCount(1);

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
        pipelineLayout_ = device_.createPipelineLayout(pipelineLayoutInfo);

        vk::GraphicsPipelineCreateInfo pipelineInfo(vk::PipelineCreateFlags(),
                                                    2,
                                                    shaderStages,
                                                    &vertexInputInfo,
                                                    &inputAssembly,
                                                    nullptr,
                                                    &viewportState,
                                                    &rasterizer,
                                                    &multisampling,
                                                    nullptr,
                                                    &colorBlending,
                                                    nullptr,
                                                    pipelineLayout_,
                                                    renderPass_,
                                                    0);
        graphicsPipelines_ = device_.createGraphicsPipelines(vk::PipelineCache(), pipelineInfo);

        device_.destroy(fragShaderModule);
        device_.destroy(vertShaderModule);
    }

    void createFrameBuffers()
    {
        swapChainFramebuffers_.reserve(swapChainImageViews_.size());
        for (auto const & view : swapChainImageViews_)
        {
            vk::FramebufferCreateInfo framebufferInfo(vk::FramebufferCreateFlags(),
                                                      renderPass_,
                                                      1,
                                                      &view,
                                                      swapChainExtent_.width,
                                                      swapChainExtent_.height,
                                                      1);

            swapChainFramebuffers_.push_back(device_.createFramebuffer(framebufferInfo));
        }
    }

    void cleanup()
    {
        for (auto const & framebuffer : swapChainFramebuffers_)
        {
            device_.destroy(framebuffer);
        }
        for (auto const & pipeline : graphicsPipelines_)
        {
            device_.destroy(pipeline);
        }
        device_.destroy(pipelineLayout_);
        device_.destroy(renderPass_);
        for (auto const & imageView : swapChainImageViews_)
        {
            device_.destroy(imageView);
        }
        device_.destroy(swapChain_);
        device_.destroy();
        instance_.destroy(surface_);
        instance_.destroy(messenger_, nullptr, dynamicLoader_);
        instance_.destroy();
        if (window_)
            delete window_;
    }

    void mainLoop()
    {
    }

    Glfwx::Window * window_ = nullptr;
    vk::Instance instance_;
    vk::DispatchLoaderDynamic dynamicLoader_;
    vk::DebugUtilsMessengerEXT messenger_;
    vk::Device device_;
    vk::Queue graphicsQueue_;
    vk::Queue presentQueue_;
    vk::SurfaceKHR surface_;
    vk::SwapchainKHR swapChain_;
    std::vector<vk::Image> swapChainImages_;
    std::vector<vk::ImageView> swapChainImageViews_;
    vk::Format swapChainImageFormat_;
    vk::Extent2D swapChainExtent_;
    vk::RenderPass renderPass_;
    vk::PipelineLayout pipelineLayout_;
    std::vector<vk::Pipeline> graphicsPipelines_;
    std::vector<vk::Framebuffer> swapChainFramebuffers_;
};

int main()
{
    {
        std::vector<vk::ExtensionProperties> extensions = vk::enumerateInstanceExtensionProperties(nullptr);
        std::cout << (int)extensions.size() << " instance extensions available:" << std::endl;
        for (auto const e : extensions)
        {
            std::cout << "\t" << e.extensionName << std::endl;
        }
    }

    HelloTriangleApplication app;

    try
    {
        Glfwx::init();
        app.run();
        Glfwx::terminate();
    }
    catch (const std::exception & e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
