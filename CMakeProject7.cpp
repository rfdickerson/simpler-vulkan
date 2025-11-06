#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdint>

#include <vk_mem_alloc.h>

using namespace std;

struct Device {
 VkInstance instance;
 VkPhysicalDevice physicalDevice;
 VkDevice device;
 VkQueue queue;
 VkSemaphore timelineSemaphore;

 VmaAllocator allocator;
};

struct Buffer {
 VkBuffer buffer;
 VmaAllocation allocation;
};

void initVulkan(Device& device)
{
 // Initialize Vulkan instance
 VkApplicationInfo appInfo{};
 appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
 appInfo.pApplicationName = "Hello Vulkan";
 appInfo.applicationVersion = VK_MAKE_VERSION(1,0,0);
 appInfo.pEngineName = "No Engine";
 appInfo.engineVersion = VK_MAKE_VERSION(1,0,0);
 appInfo.apiVersion = VK_API_VERSION_1_4;
 VkInstanceCreateInfo createInfo{};
 createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
 createInfo.pApplicationInfo = &appInfo;
 if (vkCreateInstance(&createInfo, nullptr, &device.instance) != VK_SUCCESS) {
 throw runtime_error("failed to create instance!");
 }
 // Select physical device
 uint32_t deviceCount =0;
 vkEnumeratePhysicalDevices(device.instance, &deviceCount, nullptr);
 if (deviceCount ==0) {
 throw runtime_error("failed to find GPUs with Vulkan support!");
 }
 vector<VkPhysicalDevice> devices(deviceCount);
 vkEnumeratePhysicalDevices(device.instance, &deviceCount, devices.data());
 // Just pick the first device for simplicity
 device.physicalDevice = devices[0];

 // Print device name
 VkPhysicalDeviceProperties deviceProperties;
 vkGetPhysicalDeviceProperties(device.physicalDevice, &deviceProperties);
 cout << "Selected GPU: " << deviceProperties.deviceName << endl;

 cout << "Vulkan initialized successfully." << endl;
}

void cleanupVulkan(Device& device)
{
 vkDestroyInstance(device.instance, nullptr);
}

void cleanupVma(Device& device)
{
 vmaDestroyAllocator(device.allocator);
}

void initDevice(Device& device)
{
 // Find a suitable queue family
 uint32_t queueFamilyCount = 0;
 vkGetPhysicalDeviceQueueFamilyProperties(device.physicalDevice, &queueFamilyCount, nullptr);
 if (queueFamilyCount == 0) {
 throw runtime_error("failed to find any queue families!");
 }
 vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
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
 throw runtime_error("failed to find a suitable queue family!");
 }

 // Check timeline semaphore support
 VkPhysicalDeviceVulkan12Features supportedVulkan12Features{};
 supportedVulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
 VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{};
 physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
 physicalDeviceFeatures2.pNext = &supportedVulkan12Features;
 vkGetPhysicalDeviceFeatures2(device.physicalDevice, &physicalDeviceFeatures2);
 if (!supportedVulkan12Features.timelineSemaphore) {
 throw runtime_error("timeline semaphores are not supported!");
 }

 // Create logical device
 float queuePriority = 1.0f;
 VkDeviceQueueCreateInfo queueCreateInfo{};
 queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
 queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
 queueCreateInfo.queueCount = 1;
 queueCreateInfo.pQueuePriorities = &queuePriority;

 VkDeviceCreateInfo createInfo{};
 VkPhysicalDeviceFeatures deviceFeatures{};
 deviceFeatures.shaderStorageImageMultisample = VK_TRUE;
 deviceFeatures.shaderStorageImageReadWithoutFormat = VK_TRUE;
 deviceFeatures.shaderStorageImageWriteWithoutFormat = VK_TRUE;

 VkPhysicalDeviceVulkan13Features vulkan13Features{};
 vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
 vulkan13Features.synchronization2 = VK_TRUE;
 vulkan13Features.dynamicRendering = VK_TRUE;

 VkPhysicalDeviceVulkan12Features vulkan12Features{};
 vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
 vulkan12Features.timelineSemaphore = VK_TRUE;
 vulkan12Features.pNext = &vulkan13Features;

 createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
 createInfo.pNext = &vulkan12Features;
 createInfo.pEnabledFeatures = &deviceFeatures;
 createInfo.pQueueCreateInfos = &queueCreateInfo;
 createInfo.queueCreateInfoCount = 1;
 createInfo.enabledExtensionCount = 0;
 createInfo.ppEnabledExtensionNames = nullptr;
 createInfo.enabledLayerCount = 0;
 createInfo.ppEnabledLayerNames = nullptr;

 if (vkCreateDevice(device.physicalDevice, &createInfo, nullptr, &device.device) != VK_SUCCESS) {
 throw runtime_error("failed to create logical device!");
 }

 // Get the queue
 vkGetDeviceQueue(device.device, queueFamilyIndex, 0, &device.queue);
 cout << "Device and queue created successfully." << endl;
}

void initVma(Device& device)
{
 VmaAllocatorCreateInfo allocatorInfo = {};
 allocatorInfo.physicalDevice = device.physicalDevice;
 allocatorInfo.device = device.device;
 allocatorInfo.instance = device.instance;
 allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_4;
 allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

 if (vmaCreateAllocator(&allocatorInfo, &device.allocator) != VK_SUCCESS) {
 throw runtime_error("failed to create VMA allocator!");
 }
}

void makeTimelineSemaphore(Device& device)
{
 // Create a timeline semaphore
 VkSemaphoreTypeCreateInfo timelineCreateInfo{};
 timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
 timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
 timelineCreateInfo.initialValue = 0;
 VkSemaphoreCreateInfo semaphoreCreateInfo{};
 semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
 semaphoreCreateInfo.pNext = &timelineCreateInfo;

 if (vkCreateSemaphore(device.device, &semaphoreCreateInfo, nullptr, &device.timelineSemaphore) != VK_SUCCESS) {
 throw runtime_error("failed to create timeline semaphore!");
 }
 cout << "Timeline semaphore created successfully." << endl;

}

// Create a Storage Buffer (SSBO) using VMA. The buffer is host-accessible for easy CPU writes.
Buffer makeSSBOBuffer(Device& device, VkDeviceSize size) {
 Buffer buffer{};

 VkBufferCreateInfo bufferInfo{};
 bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
 bufferInfo.size = size;
 bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
 VK_BUFFER_USAGE_TRANSFER_SRC_BIT; // optional, useful for uploads/downloads
 bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

 VmaAllocationCreateInfo allocInfo{};
 allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
 // Make it host-visible so the CPU can write into the SSBO easily.
 allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

 VkResult res = vmaCreateBuffer(device.allocator, &bufferInfo, &allocInfo,
 &buffer.buffer, &buffer.allocation, nullptr);
 if (res != VK_SUCCESS) {
 throw runtime_error("failed to create SSBO buffer with VMA!");
 }

 return buffer;
}

void destroyBuffer(Device& device, Buffer& buffer)
{
 if (buffer.buffer != VK_NULL_HANDLE) {
 vmaDestroyBuffer(device.allocator, buffer.buffer, buffer.allocation);
 buffer.buffer = VK_NULL_HANDLE;
 buffer.allocation = nullptr;
 }
}

int main()
{
 cout << "Hello CMake." << endl;

 Device device;
 initVulkan(device);
 initDevice(device);
 initVma(device);
 makeTimelineSemaphore(device);

 // Example: create a 4KB SSBO
 Buffer ssbo = makeSSBOBuffer(device, 4 *1024);
 cout << "SSBO created." << endl;

 // Cleanup
 destroyBuffer(device, ssbo);
 vkDestroySemaphore(device.device, device.timelineSemaphore, nullptr);
 cleanupVma(device);
 vkDestroyDevice(device.device, nullptr);
 cleanupVulkan(device);

 return 0;
}
