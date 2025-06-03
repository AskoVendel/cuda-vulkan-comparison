#define _USE_MATH_DEFINES
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <stdio.h>
#include <cmath>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan.h>

#include "stb_image.h"
#include "stb_image_write.h"

#include <chrono>

using namespace std;

#define CHANNELS 4

struct Constants {
    int width;                  
    int height;                  
    int N;
    float divider;
};

void average_filter(int N, float* coeff) {
    for (int i = 0; i <= N; i++)
        for (int j = 0; j <= N; j++)
            coeff[i * (N + 1) + j] = 1;
}

void sharpening_filter(int N, float* coeff) {
    int w = 2 * N + 1; // filtri tegelik laius

    float dist = sqrt(2 * N * N);
    float mul = dist / asin(1);
    //printf("%.3f %.3f %.3f\n", dist, asin(1), mul);

    for (int i = 0; i <= N; i++)
        for (int j = 0; j <= N; j++) {
            coeff[i * (N + 1) + j] = -cos(sqrt(i * i + j * j) / mul);
            //printf("%d,%d(%.3f): %.3f\n", i, j, sqrt(i * i + j * j), coeff[i * (N + 1) + j]);
        }
    coeff[0] = 0;
    float divider = 0;
    for (int i = -N; i <= N; i++)
        for (int j = -N; j <= N; j++)
            divider = divider + coeff[abs(i) * (N + 1) + abs(j)];
    coeff[0] = -divider;
    //printf("%.3f\n", divider);
}

void process_noise(void) {

    using std::chrono::high_resolution_clock;
    using std::chrono::duration;
    using std::chrono::milliseconds;

	int x;
	int y;
	int n;
	int ok;

	ok = stbi_info("noise.bmp", &x, &y, &n);

	cout << "OK?: " << ok << "\n";
	cout << "width: " << x << "\n";
	cout << "height: " << y << "\n";
	cout << "bytes per pixel: " << n << "\n";
	cout << "-------------------------" << "\n";

	unsigned char* data = stbi_load("noise.bmp", &x, &y, &n, CHANNELS);

	unsigned char* out1 = new unsigned char[x * y * CHANNELS];
	unsigned char* out2 = new unsigned char[x * y * CHANNELS];

    //Misc

    n = 4;
    float* c1 = new float[(n + 1) * (n + 1)];
    average_filter(n, c1);

    float divider = 0;
    for (int i = -n; i <= n; i++)
        for (int j = -n; j <= n; j++)
            divider += c1[abs(i) * (n + 1) + abs(j)];

    int nSharp = 2;
    float* c4 = new float[(nSharp + 1) * (nSharp + 1)];
    sharpening_filter(nSharp, c4);

    //App creation
    vk::ApplicationInfo AppInfo{
    "VulkanCompute",      
    1,                    
    nullptr,              
    0,                    
    VK_API_VERSION_1_3    
    };

    const std::vector<const char*> Layers = { "VK_LAYER_KHRONOS_validation" };
    vk::InstanceCreateInfo InstanceCreateInfo(vk::InstanceCreateFlags(), 
        &AppInfo,                  
        Layers,                    
        {});                       //Extensions
    vk::Instance Instance = vk::createInstance(InstanceCreateInfo);
    auto availableLayers = vk::enumerateInstanceLayerProperties();


    //Enumerate phsyical devices
    vk::PhysicalDevice PhysicalDevice = Instance.enumeratePhysicalDevices().front();
    vk::PhysicalDeviceProperties DeviceProps = PhysicalDevice.getProperties();
    std::cout << "Device Name    : " << DeviceProps.deviceName << std::endl;
    const uint32_t ApiVersion = DeviceProps.apiVersion;
    std::cout << "Vulkan Version : " << VK_VERSION_MAJOR(ApiVersion) << "." << VK_VERSION_MINOR(ApiVersion) << "." << VK_VERSION_PATCH(ApiVersion) << std::endl;
    vk::PhysicalDeviceLimits DeviceLimits = DeviceProps.limits;
    std::cout << "Max Compute Shared Memory Size: " << DeviceLimits.maxComputeSharedMemorySize / 1024 << " KB" << std::endl;

    //Queue creation
    std::vector<vk::QueueFamilyProperties> QueueFamilyProps = PhysicalDevice.getQueueFamilyProperties();
    auto PropIt = std::find_if(QueueFamilyProps.begin(), QueueFamilyProps.end(), [](const vk::QueueFamilyProperties& Prop)
        {
            return Prop.queueFlags & vk::QueueFlagBits::eCompute;
        });
    const uint32_t ComputeQueueFamilyIndex = std::distance(QueueFamilyProps.begin(), PropIt);
    std::cout << "Compute Queue Family Index: " << ComputeQueueFamilyIndex << std::endl;
    if (PropIt == QueueFamilyProps.end()) {
        throw std::runtime_error("No compute-capable queue family found!");
    }

    //Device creation
    const float QueuePriority = 1.0f;
    vk::DeviceQueueCreateInfo DeviceQueueCreateInfo(vk::DeviceQueueCreateFlags(),  
        ComputeQueueFamilyIndex,        
        1,
        &QueuePriority);                             
    vk::DeviceCreateInfo DeviceCreateInfo(vk::DeviceCreateFlags(),   
        DeviceQueueCreateInfo);  
    vk::Device Device = PhysicalDevice.createDevice(DeviceCreateInfo);
    vk::DeviceSize BufferSize1 = sizeof(unsigned char) * y * x * CHANNELS;
    vk::DeviceSize BufferSize2 = sizeof(unsigned char) * y * x * CHANNELS;
    vk::DeviceSize BufferSizeCoeff = sizeof(float) * (n + 1) * (n + 1);
    vk::DeviceSize BufferSizeC4 = sizeof(float) * (nSharp + 1) * (nSharp + 1);

    //Buffers

    //Buffer creation info-s
    vk::BufferCreateInfo bufferCreateInfo1{
        vk::BufferCreateFlags(),                   
        BufferSize1,                                
        vk::BufferUsageFlagBits::eTransferSrc |    
        vk::BufferUsageFlagBits::eStorageBuffer,   
        vk::SharingMode::eExclusive                
    };

    vk::BufferCreateInfo bufferCreateInfo2{
        vk::BufferCreateFlags(),                   
        BufferSize2,                                
        vk::BufferUsageFlagBits::eTransferSrc |    
        vk::BufferUsageFlagBits::eStorageBuffer,   
        vk::SharingMode::eExclusive                
    };

    vk::BufferCreateInfo bufferCreateInfoCoeff{
        vk::BufferCreateFlags(),
        BufferSizeCoeff,
        vk::BufferUsageFlagBits::eTransferSrc |
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::SharingMode::eExclusive
    };

    vk::BufferCreateInfo bufferCreateInfoC4{
        vk::BufferCreateFlags(),
        BufferSizeC4,
        vk::BufferUsageFlagBits::eTransferSrc |
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::SharingMode::eExclusive
    };

    auto tCreateBuffer1 = high_resolution_clock::now();

    //Buffer creation
    vk::Buffer dataBuffer = Device.createBuffer(bufferCreateInfo1);
    vk::Buffer outBuffer = Device.createBuffer(bufferCreateInfo2);
    vk::Buffer CoeffBuffer = Device.createBuffer(bufferCreateInfoCoeff);
    //vk::Buffer C4Buffer = Device.createBuffer(bufferCreateInfoC4);

    auto tCreateBuffer2 = high_resolution_clock::now();

    duration<double, std::milli> memBufferCreateTime = tCreateBuffer2 - tCreateBuffer1;
    std::cout << "\n" << "Buffer creation time: " << memBufferCreateTime.count() << " ms\n";

    auto tMemReqs1 = high_resolution_clock::now();

    //Buffer memory reqs
    vk::MemoryRequirements dataMemReqs = Device.getBufferMemoryRequirements(dataBuffer);
    vk::MemoryRequirements outMemReqs = Device.getBufferMemoryRequirements(outBuffer);
    vk::MemoryRequirements CoeffMemReqs = Device.getBufferMemoryRequirements(CoeffBuffer);
    //vk::MemoryRequirements C4MemReqs = Device.getBufferMemoryRequirements(C4Buffer);

    auto tMemReqs2 = high_resolution_clock::now();

    duration<double, std::milli> memReqsTime = tMemReqs2 - tMemReqs1;
    std::cout << "Get Buffer Memory Requirements time: " << memReqsTime.count() << " ms\n";

    if (BufferSize1 > dataMemReqs.size || BufferSize2 > outMemReqs.size || BufferSizeCoeff > CoeffMemReqs.size /*|| BufferSizeC4 > C4MemReqs.size*/) {
        throw std::runtime_error("Buffer size exceeds allocated memory size!");
    }

    //Finding memory types
    auto findMemoryType = [&](uint32_t typeFilter, vk::MemoryPropertyFlags properties) -> uint32_t {
        vk::PhysicalDeviceMemoryProperties memProps = PhysicalDevice.getMemoryProperties();
        for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        throw std::runtime_error("Failed to find suitable memory type!");
        };

    auto tMemAllocate1 = high_resolution_clock::now();

    //Memory allocation to buffers
    vk::MemoryAllocateInfo dataAllocInfo{
        dataMemReqs.size,
        findMemoryType(dataMemReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
    };
    vk::DeviceMemory dataMemory = Device.allocateMemory(dataAllocInfo);

    vk::MemoryAllocateInfo outAllocInfo{
        outMemReqs.size,
        findMemoryType(outMemReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
    };
    vk::DeviceMemory outMemory = Device.allocateMemory(outAllocInfo);

    vk::MemoryAllocateInfo CoeffAllocInfo{
    CoeffMemReqs.size,
    findMemoryType(CoeffMemReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
    };
    vk::DeviceMemory CoeffMemory = Device.allocateMemory(CoeffAllocInfo);

    /* C4 not needed for now
    vk::MemoryAllocateInfo C4AllocInfo{
    C4MemReqs.size,
    findMemoryType(C4MemReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
    };
    vk::DeviceMemory C4Memory = Device.allocateMemory(C4AllocInfo);
    */

    auto tMemAllocate2 = high_resolution_clock::now();

    duration<double, std::milli> memAllocateTime = tMemAllocate2 - tMemAllocate1;
    std::cout << "Memory allocation to buffers time: " << memAllocateTime.count() << " ms\n";

    auto tMemBinding1 = high_resolution_clock::now();

    //Buffer binding to memory
    Device.bindBufferMemory(dataBuffer, dataMemory, 0);
    Device.bindBufferMemory(outBuffer, outMemory, 0);
    Device.bindBufferMemory(CoeffBuffer, CoeffMemory, 0);
    //Device.bindBufferMemory(C4Buffer, C4Memory, 0);

    auto tMemBinding2 = high_resolution_clock::now();

    duration<double, std::milli> memBindingTime = tMemBinding2 - tMemBinding1;
    std::cout << "Buffer binding to memory time: " << memBindingTime.count() << " ms\n";

    auto tMemMapMemory1 = high_resolution_clock::now();

    //Mapping and copying data to memory
    void* mappedData = Device.mapMemory(dataMemory, 0, BufferSize1);
    if (!mappedData) {
        throw std::runtime_error("Failed to map data memory!");
    }

    void* mappedOut = Device.mapMemory(outMemory, 0, BufferSize2);
    if (!mappedOut) {
        throw std::runtime_error("Failed to map zeros memory!");
    }

    void* mappedCoeff = Device.mapMemory(CoeffMemory, 0, BufferSizeCoeff);
    if (!mappedCoeff) {
        throw std::runtime_error("Failed to map Coeff memory!");
    }

    auto tMemMapMemory2 = high_resolution_clock::now();

    duration<double, std::milli> memMapTime = tMemMapMemory2 - tMemMapMemory1;
    std::cout << "All memory mapping time: " << memMapTime.count() << " ms\n";

    auto tMemTransfer1 = high_resolution_clock::now();

    memcpy(mappedData, data, BufferSize1);
    memcpy(mappedOut, out1, BufferSize2);
    memcpy(mappedCoeff, c1, BufferSizeCoeff);

    auto tMemTransfer2 = high_resolution_clock::now();

    duration<double, std::milli> memTransferTime = tMemTransfer2 - tMemTransfer1;
    std::cout << "All memcpy time: " << memTransferTime.count() << " ms\n";

    //Unmapping memory
    Device.unmapMemory(dataMemory);
    //Device.unmapMemory(outMemory);

    /* for high-pass filter, not needed yet
    void* mappedC4 = Device.mapMemory(C4Memory, 0, BufferSizeC4);
    if (!mappedC4) {
        throw std::runtime_error("Failed to map Coeff memory!");
    }
    memcpy(mappedC4, c4, BufferSizeC4);
    */
    //Device.unmapMemory(outMemory);

    std::cout << "\n" << "Data, c1 and out1 copied to device memory successfully." << std::endl;

    //Shadermodule creation
    std::vector<char> ShaderContents;
    if (std::ifstream ShaderFile{ "ComputeShader.spv", std::ios::binary | std::ios::ate })
    {
        const size_t FileSize = ShaderFile.tellg();
        ShaderFile.seekg(0);
        ShaderContents.resize(FileSize, '\0');
        ShaderFile.read(ShaderContents.data(), FileSize);
    }

    vk::ShaderModuleCreateInfo ShaderModuleCreateInfo(
        vk::ShaderModuleCreateFlags(),                                
        ShaderContents.size(),                                        
        reinterpret_cast<const uint32_t*>(ShaderContents.data()));    
    vk::ShaderModule ShaderModule = Device.createShaderModule(ShaderModuleCreateInfo);

    //Descriptor sets

    //Layout
    const std::vector<vk::DescriptorSetLayoutBinding> DescriptorSetLayoutBinding = {
    {0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
    {1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
    {2, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
    {3, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute}
    };
    vk::DescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(
        vk::DescriptorSetLayoutCreateFlags(),
        DescriptorSetLayoutBinding);
    vk::DescriptorSetLayout DescriptorSetLayout = Device.createDescriptorSetLayout(DescriptorSetLayoutCreateInfo);

    // Define the push constant range
    vk::PushConstantRange pushConstantRange{
        vk::ShaderStageFlagBits::eCompute, // Shader stage using the push constant
        0,                                 // Offset in bytes
        sizeof(Constants)                  // Size of the push constant block
    };

    // Pipeline layout with push constant range
    vk::PipelineLayoutCreateInfo PipelineLayoutCreateInfo(
        vk::PipelineLayoutCreateFlags(),  // Flags
        1,                                // Set Layout Count (assuming 1 descriptor set layout)
        &DescriptorSetLayout,             // Pointer to descriptor set layout(s)
        1,                                // Push Constant Range Count
        &pushConstantRange                // Pointer to push constant ranges
    );
    vk::PipelineLayout PipelineLayout = Device.createPipelineLayout(PipelineLayoutCreateInfo);
    vk::PipelineCache PipelineCache = Device.createPipelineCache(vk::PipelineCacheCreateInfo());

    vk::PipelineShaderStageCreateInfo PipelineShaderCreateInfo(
        vk::PipelineShaderStageCreateFlags(),  // Flags
        vk::ShaderStageFlagBits::eCompute,     // Stage
        ShaderModule,                          // Shader Module
        "main");                               // Shader Entry Point
    vk::ComputePipelineCreateInfo ComputePipelineCreateInfo(
        vk::PipelineCreateFlags(),    // Flags
        PipelineShaderCreateInfo,     // Shader Create Info struct
        PipelineLayout);              // Pipeline Layout
    vk::Pipeline ComputePipeline = Device.createComputePipeline(PipelineCache, ComputePipelineCreateInfo).value;

    vk::DescriptorPoolSize DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 4);
    vk::DescriptorPoolCreateInfo DescriptorPoolCreateInfo(vk::DescriptorPoolCreateFlags(), 1, DescriptorPoolSize);
    vk::DescriptorPool DescriptorPool = Device.createDescriptorPool(DescriptorPoolCreateInfo);

    vk::DescriptorSetAllocateInfo DescriptorSetAllocInfo(DescriptorPool, 1, &DescriptorSetLayout);
    const std::vector<vk::DescriptorSet> DescriptorSets = Device.allocateDescriptorSets(DescriptorSetAllocInfo);
    vk::DescriptorSet DescriptorSet = DescriptorSets.front();
    vk::DescriptorBufferInfo InBufferInfo(dataBuffer, 0, BufferSize1);
    vk::DescriptorBufferInfo OutBufferInfo(outBuffer, 0, BufferSize2);
    vk::DescriptorBufferInfo CoeffBufferInfo(CoeffBuffer, 0, BufferSizeCoeff);

    const std::vector<vk::WriteDescriptorSet> WriteDescriptorSets = {
    {DescriptorSet, 0, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &InBufferInfo},
    {DescriptorSet, 1, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &OutBufferInfo},
    {DescriptorSet, 2, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &CoeffBufferInfo}
    };
    Device.updateDescriptorSets(WriteDescriptorSets, {});

    vk::CommandPoolCreateInfo CommandPoolCreateInfo(vk::CommandPoolCreateFlags(), ComputeQueueFamilyIndex);
    vk::CommandPool CommandPool = Device.createCommandPool(CommandPoolCreateInfo);

    vk::CommandBufferAllocateInfo CommandBufferAllocInfo(
        CommandPool,                         // Command Pool
        vk::CommandBufferLevel::ePrimary,    // Level
        1);                                  // Num Command Buffers
    const std::vector<vk::CommandBuffer> CmdBuffers = Device.allocateCommandBuffers(CommandBufferAllocInfo);
    vk::CommandBuffer CmdBuffer = CmdBuffers.front();

    vk::CommandBufferBeginInfo CmdBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    CmdBuffer.begin(CmdBufferBeginInfo);
    CmdBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, ComputePipeline);
    CmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,    // Bind point
        PipelineLayout,                  // Pipeline Layout
        0,                               // First descriptor set
        { DescriptorSet },               // List of descriptor sets
        {});                             // Dynamic offsets

    struct PushConstantsLow {
        int width;
        int height;
        int N;
        float divider;
    } params;

    params.width = x;
    params.height = y;
    params.N = n;
    params.divider = divider;

    CmdBuffer.pushConstants(
        PipelineLayout,                           // Pipeline layout used by the pipeline
        vk::ShaderStageFlagBits::eCompute,        // Shader stage
        0,                                        // Offset
        sizeof(Constants),                        // Size of the push constant data
        &params                            // Pointer to the push constant data
    );

    CmdBuffer.dispatch((x + 15) / 16, (y + 15) / 16, 1);
    CmdBuffer.end();

    vk::Queue Queue = Device.getQueue(ComputeQueueFamilyIndex, 0);
    vk::Fence Fence = Device.createFence(vk::FenceCreateInfo());

    vk::SubmitInfo SubmitInfo(0,                // Num Wait Semaphores
        nullptr,        // Wait Semaphores
        nullptr,        // Pipeline Stage Flags
        1,              // Num Command Buffers
        &CmdBuffer);    // List of command buffers

    auto t1 = high_resolution_clock::now();

    Queue.submit({ SubmitInfo }, Fence);
    Device.waitForFences({ Fence },             // List of fences
        true,               // Wait All
        uint64_t(-1));      // Timeout

    auto t2 = high_resolution_clock::now();

    duration<double, std::milli> ms_double = t2 - t1;
    std::cout << "low-pass filter kernel runtime: " << ms_double.count() << " ms\n";

    Device.destroyFence(Fence);

    unsigned char* out = new unsigned char[x * y * CHANNELS];

    auto tMemBack1 = high_resolution_clock::now();

    memcpy(out, mappedOut, BufferSize2);

    auto tMemBack2 = high_resolution_clock::now();

    duration<double, std::milli> memTransferBack = tMemBack2 - tMemBack1;
    std::cout << "All memcpy back to RAM time: " << memTransferBack.count() << " ms\n";

    Device.unmapMemory(outMemory);

    stbi_write_bmp("noise_blur1_vulkan.bmp", x, y, CHANNELS, out);

	delete[] data;
    delete[] out;
	delete[] out1;
}

int main() {
	process_noise();

	return 0;
}