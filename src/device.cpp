#include "device.hpp"

#include <iostream>
#include <stdexcept>
#include <vector>

void initVulkan(Device& device) {
    // Initialize Vulkan instance
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "Hello Vulkan";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "No Engine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_4;
    VkInstanceCreateInfo createInfo{};
    createInfo.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    if (vkCreateInstance(&createInfo, nullptr, &device.instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }
    // Select physical device
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(device.instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(device.instance, &deviceCount, devices.data());
    // Just pick the first device for simplicity
    device.physicalDevice = devices[0];

    // Print device name
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device.physicalDevice, &deviceProperties);
    std::cout << "Selected GPU: " << deviceProperties.deviceName << std::endl;

    std::cout << "Vulkan initialized successfully." << std::endl;
}

void cleanupVulkan(Device& device) {
    vkDestroyInstance(device.instance, nullptr);
}

void cleanupVma(Device& device) {
    vmaDestroyAllocator(device.allocator);
}

void initDevice(Device& device) {
    // Find a suitable queue family
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device.physicalDevice, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0) {
        throw std::runtime_error("failed to find any queue families!");
    }
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device.physicalDevice, &queueFamilyCount, queueFamilies.data());

    // Find a queue family that supports graphics and compute
    uint32_t queueFamilyIndex = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueCount > 0 &&
            (queueFamilies[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) ==
                (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
            queueFamilyIndex = i;
            break;
        }
    }
    if (queueFamilyIndex == UINT32_MAX) {
        throw std::runtime_error("failed to find a suitable queue family!");
    }

    // Check timeline semaphore support
    VkPhysicalDeviceVulkan12Features supportedVulkan12Features{};
    supportedVulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{};
    physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    physicalDeviceFeatures2.pNext = &supportedVulkan12Features;
    vkGetPhysicalDeviceFeatures2(device.physicalDevice, &physicalDeviceFeatures2);
    if (!supportedVulkan12Features.timelineSemaphore) {
        throw std::runtime_error("timeline semaphores are not supported!");
    }

    // Create logical device
    float                   queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
    queueCreateInfo.queueCount       = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo       createInfo{};
    VkPhysicalDeviceFeatures deviceFeatures{};

    VkPhysicalDeviceVulkan13Features vulkan13Features{};
    vulkan13Features.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan13Features.synchronization2 = VK_TRUE;
    vulkan13Features.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceVulkan12Features vulkan12Features{};
    vulkan12Features.sType               = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12Features.timelineSemaphore   = VK_TRUE;
    vulkan12Features.bufferDeviceAddress = VK_TRUE;
    vulkan12Features.pNext               = &vulkan13Features;

    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext                   = &vulkan12Features;
    createInfo.pEnabledFeatures        = &deviceFeatures;
    createInfo.pQueueCreateInfos       = &queueCreateInfo;
    createInfo.queueCreateInfoCount    = 1;
    createInfo.enabledExtensionCount   = 0;
    createInfo.ppEnabledExtensionNames = nullptr;
    createInfo.enabledLayerCount       = 0;
    createInfo.ppEnabledLayerNames     = nullptr;

    if (vkCreateDevice(device.physicalDevice, &createInfo, nullptr, &device.device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }

    // Get the queue
    vkGetDeviceQueue(device.device, queueFamilyIndex, 0, &device.queue);
    std::cout << "Device and queue created successfully." << std::endl;
}

void initVma(Device& device) {
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice         = device.physicalDevice;
    allocatorInfo.device                 = device.device;
    allocatorInfo.instance               = device.instance;
    allocatorInfo.vulkanApiVersion       = VK_API_VERSION_1_4;
    allocatorInfo.flags                  = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT |
                          VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT | VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE4_BIT;

    if (vmaCreateAllocator(&allocatorInfo, &device.allocator) != VK_SUCCESS) {
        throw std::runtime_error("failed to create VMA allocator!");
    }

    VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
    vmaGetHeapBudgets(device.allocator, budgets);
    for (uint32_t i = 0; i < VK_MAX_MEMORY_HEAPS; ++i) {
        if (budgets[i].budget == 0 && budgets[i].usage == 0)
            continue; // skip unused heaps
        std::cout << "Heap " << i << " usage=" << static_cast<unsigned long long>(budgets[i].usage)
                  << " budget=" << static_cast<unsigned long long>(budgets[i].budget) << std::endl;
    }
}

void makeTimelineSemaphore(Device& device) {
    // Create a timeline semaphore
    VkSemaphoreTypeCreateInfo timelineCreateInfo{};
    timelineCreateInfo.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineCreateInfo.initialValue  = 0;
    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = &timelineCreateInfo;

    if (vkCreateSemaphore(device.device, &semaphoreCreateInfo, nullptr, &device.timelineSemaphore) != VK_SUCCESS) {
        throw std::runtime_error("failed to create timeline semaphore!");
    }
    std::cout << "Timeline semaphore created successfully." << std::endl;
}