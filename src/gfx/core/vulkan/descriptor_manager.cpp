

#include "descriptor_manager.hpp"
#include "device.hpp"
#include "util/logger.hpp"
#include <limits>
#include <ranges>
#include <utility>

namespace gfx::core::vulkan
{

    static constexpr std::array<vk::DescriptorType, 5> SupportedDescriptors {
        vk::DescriptorType::eSampler,
        vk::DescriptorType::eSampledImage,
        vk::DescriptorType::eStorageImage,
        vk::DescriptorType::eUniformBuffer,
        vk::DescriptorType::eStorageBuffer,
    };

    static_assert(SupportedDescriptors[0] == vk::DescriptorType::eSampler);
    static_assert(SupportedDescriptors[1] == vk::DescriptorType::eSampledImage);
    static_assert(SupportedDescriptors[2] == vk::DescriptorType::eStorageImage);
    static_assert(SupportedDescriptors[3] == vk::DescriptorType::eUniformBuffer);
    static_assert(SupportedDescriptors[4] == vk::DescriptorType::eStorageBuffer);

    static constexpr std::array<u32, 5> SupportedDescriptorAmounts {
        std::numeric_limits<DescriptorHandle<vk::DescriptorType::eSampler>::StorageType>::max() + 1,
        std::numeric_limits<DescriptorHandle<vk::DescriptorType::eSampledImage>::StorageType>::max() + 1,
        std::numeric_limits<DescriptorHandle<vk::DescriptorType::eStorageImage>::StorageType>::max() + 1,
        std::numeric_limits<DescriptorHandle<vk::DescriptorType::eUniformBuffer>::StorageType>::max() + 1,
        std::numeric_limits<DescriptorHandle<vk::DescriptorType::eStorageBuffer>::StorageType>::max() + 1,
    };

    static constexpr std::array<u32, 5> ShaderBindingLocations {0, 1, 2, 3, 4};

    namespace
    {
        constexpr u32 acquireShaderBindingLocation(vk::DescriptorType t)
        {
            decltype(SupportedDescriptors)::const_iterator thisDescriptorBindingLocation =
                std::ranges::find(SupportedDescriptors, t);

            if (thisDescriptorBindingLocation == SupportedDescriptors.cend())
            {
                panic("erm what");
                std::terminate();
            }
            else
            {
                return ShaderBindingLocations[static_cast<usize>(
                    thisDescriptorBindingLocation - SupportedDescriptors.cbegin())];
            }
        }
    } // namespace

    u32 getShaderBindingLocation(vk::DescriptorType t)
    {
        return acquireShaderBindingLocation(t);
    }

    DescriptorManager::DescriptorManager(const Device& d)
        : device {d.getDevice()}
    {
        {
            std::vector<vk::DescriptorPoolSize> poolSizes {};

            for (auto [descriptorType, maxAmount] : std::views::zip(SupportedDescriptors, SupportedDescriptorAmounts))
            {
                poolSizes.push_back(vk::DescriptorPoolSize {.type {descriptorType}, .descriptorCount {maxAmount}});
            }

            const vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo {
                .sType {vk::StructureType::eDescriptorPoolCreateInfo},
                .pNext {},
                .flags {vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind},
                .maxSets {1},
                .poolSizeCount {static_cast<u32>(poolSizes.size())},
                .pPoolSizes {poolSizes.data()},
            };

            this->bindless_descriptor_pool = this->device.createDescriptorPoolUnique(descriptorPoolCreateInfo);
        }

        {
            std::vector<vk::DescriptorSetLayoutBinding> bindingLocations {};
            bindingLocations.reserve(SupportedDescriptors.size());

            for (auto [descriptorType, maxAmount, shaderBindingLocation] :
                 std::views::zip(SupportedDescriptors, SupportedDescriptorAmounts, ShaderBindingLocations))
            {
                bindingLocations.push_back(
                    vk::DescriptorSetLayoutBinding {
                        .binding {shaderBindingLocation},
                        .descriptorType {descriptorType},
                        .descriptorCount {maxAmount},
                        .stageFlags {vk::ShaderStageFlagBits::eAll},
                        .pImmutableSamplers {nullptr},
                    });
            }

            std::vector<vk::DescriptorBindingFlags> bindingFlags {};
            bindingFlags.reserve(bindingLocations.size());

            for (auto b : bindingLocations)
            {
                std::ignore = b;

                bindingFlags.push_back(
                    vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind);
            }

            vk::DescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlagsCreateInfo {
                .sType {vk::StructureType::eDescriptorSetLayoutBindingFlagsCreateInfo},
                .pNext {nullptr},
                .bindingCount {static_cast<u32>(bindingFlags.size())},
                .pBindingFlags {bindingFlags.data()},
            };

            const vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo {
                .sType {vk::StructureType::eDescriptorSetLayoutCreateInfo},
                .pNext {&descriptorSetLayoutBindingFlagsCreateInfo},
                .flags {vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool},
                .bindingCount {static_cast<u32>(bindingLocations.size())},
                .pBindings {bindingLocations.data()},
            };

            this->bindless_descriptor_set_layout =
                this->device.createDescriptorSetLayoutUnique(descriptorSetLayoutCreateInfo);
        }

        {
            const vk::PushConstantRange pushConstantRange {
                .stageFlags {vk::ShaderStageFlagBits::eAll},
                .offset {0},
                .size {128},
            };

            const vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo {
                .sType {vk::StructureType::ePipelineLayoutCreateInfo},
                .pNext {nullptr},
                .flags {},
                .setLayoutCount {1},
                .pSetLayouts {&*this->bindless_descriptor_set_layout},
                .pushConstantRangeCount {1},
                .pPushConstantRanges {&pushConstantRange},
            };

            this->bindless_pipeline_layout = this->device.createPipelineLayoutUnique(pipelineLayoutCreateInfo);
        }

        {
            const vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo {
                .sType {vk::StructureType::eDescriptorSetAllocateInfo},
                .pNext {nullptr},
                .descriptorPool {*this->bindless_descriptor_pool},
                .descriptorSetCount {1},
                .pSetLayouts {&*this->bindless_descriptor_set_layout},
            };

            std::vector<vk::DescriptorSet> outSets = this->device.allocateDescriptorSets(descriptorSetAllocateInfo);

            assert::warn(outSets.size() == 1, "I hate you nvidia {}", outSets.size());
            assert::critical(!outSets.empty(), "nvidia what the fuck");

            this->bindless_descriptor_set = outSets[0];
        }

        {
            this->critical_section.lock(
                [&](CriticalSection& criticalSection)
                {
                    for (auto [descriptorType, maxAmount] :
                         std::views::zip(SupportedDescriptors, SupportedDescriptorAmounts))
                    {
                        criticalSection.descriptor_info[descriptorType].resize(maxAmount, std::nullopt);
                    }
                });
        }
    }

    std::map<vk::DescriptorType, std::vector<DescriptorManager::DescriptorReport>>
    DescriptorManager::getAllDescriptorsDebugInfo() const
    {
        std::map<vk::DescriptorType, std::vector<DescriptorReport>> output {};

        this->critical_section.lock(
            [&](CriticalSection& criticalSection)
            {
                for (auto& [descriptorType, info] : criticalSection.descriptor_info)
                {
                    u32 idx = 0;
                    for (const std::optional<DescriptorInfo>& i : info)
                    {
                        if (i.has_value())
                        {
                            output[descriptorType].push_back(
                                DescriptorReport {.offset {idx}, .name {i->name}, .maybe_size_bytes {i->size_bytes}});
                        }

                        idx += 1;
                    }
                }
            });

        return output;
    }

    vk::DescriptorSet DescriptorManager::getGlobalDescriptorSet() const
    {
        return this->bindless_descriptor_set;
    }

    vk::PipelineLayout DescriptorManager::getGlobalPipelineLayout() const
    {
        return *this->bindless_pipeline_layout;
    }

    template<vk::DescriptorType D>
    DescriptorHandle<D> DescriptorManager::registerDescriptor(
        RegisterDescriptorArgs<D> descriptorArgs,
        std::string               name,
        u8                        bindingLocation,
        std::source_location      location) const
    {
        return this->critical_section.lock(
            [&](CriticalSection& criticalSection)
            {
                std::optional<DescriptorInfo>& thisBindingInfo =
                    criticalSection.descriptor_info.at(D).at(static_cast<usize>(bindingLocation));

                if (thisBindingInfo.has_value())
                {
                    panic<std::string, const u8&>(
                        "Descriptor registration of type {} has failed. Slot {} is already occupied!",
                        vk::to_string(D),
                        bindingLocation,
                        location);
                }

                thisBindingInfo = DescriptorInfo {.name {std::move(name)}, .size_bytes {}};

                vk::DescriptorImageInfo descriptorImageInfo {.sampler {nullptr}, .imageView {nullptr}, .imageLayout {}};
                vk::DescriptorBufferInfo descriptorBufferInfo {.buffer {nullptr}, .offset {0}, .range {}};

                if constexpr (D == vk::DescriptorType::eSampler)
                {
                    descriptorImageInfo.sampler = descriptorArgs.sampler;
                }
                else if constexpr (D == vk::DescriptorType::eSampledImage || D == vk::DescriptorType::eStorageImage)
                {
                    descriptorImageInfo.imageView   = descriptorArgs.view;
                    descriptorImageInfo.imageLayout = descriptorArgs.layout;
                }
                else if constexpr (D == vk::DescriptorType::eUniformBuffer || D == vk::DescriptorType::eStorageBuffer)
                {
                    descriptorBufferInfo.buffer = descriptorArgs.buffer;
                    descriptorBufferInfo.range  = descriptorArgs.size_bytes;

                    thisBindingInfo->size_bytes = descriptorArgs.size_bytes;
                }
                else
                {
                    static_assert(false, "oops");
                }

                vk::WriteDescriptorSet descriptorSetWrite {
                    .sType {vk::StructureType::eWriteDescriptorSet},
                    .pNext {nullptr},
                    .dstSet {this->bindless_descriptor_set},
                    .dstBinding {acquireShaderBindingLocation(D)},
                    .dstArrayElement {bindingLocation},
                    .descriptorCount {1},
                    .descriptorType {D},
                    .pImageInfo {nullptr},
                    .pBufferInfo {nullptr},
                    .pTexelBufferView {nullptr},
                };

                if constexpr (
                    D == vk::DescriptorType::eSampler || D == vk::DescriptorType::eSampledImage
                    || D == vk::DescriptorType::eStorageImage)
                {
                    descriptorSetWrite.pImageInfo = &descriptorImageInfo;
                }

                if constexpr (D == vk::DescriptorType::eUniformBuffer || D == vk::DescriptorType::eStorageBuffer)
                {
                    descriptorSetWrite.pBufferInfo = &descriptorBufferInfo;
                }

                this->device.updateDescriptorSets(descriptorSetWrite, {});

                return DescriptorHandle<D> {bindingLocation};
            });
    }

    template<vk::DescriptorType D>
    void DescriptorManager::deregisterDescriptor(DescriptorHandle<D> handle) const
    {
        this->critical_section.lock(
            [&](CriticalSection& criticalSection)
            {
                criticalSection.descriptor_info.at(D).at(handle.getOffset()) = {};
            });
    }

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define INSTANTIATE_REGISTER_DESCRIPTOR(Type)                                                                          \
    template DescriptorHandle<Type> DescriptorManager::registerDescriptor<Type>(                                       \
        RegisterDescriptorArgs<Type>, std::string, u8, std::source_location) const;

    INSTANTIATE_REGISTER_DESCRIPTOR(vk::DescriptorType::eSampler);
    INSTANTIATE_REGISTER_DESCRIPTOR(vk::DescriptorType::eSampledImage);
    INSTANTIATE_REGISTER_DESCRIPTOR(vk::DescriptorType::eStorageImage);
    INSTANTIATE_REGISTER_DESCRIPTOR(vk::DescriptorType::eUniformBuffer);
    INSTANTIATE_REGISTER_DESCRIPTOR(vk::DescriptorType::eStorageBuffer);

#undef INSTANTIATE_REGISTER_DESCRIPTOR

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define INSTANTIATE_DEREGISTER_DESCRIPTOR(Type)                                                                        \
    template void DescriptorManager::deregisterDescriptor<Type>(DescriptorHandle<Type>) const;

    INSTANTIATE_DEREGISTER_DESCRIPTOR(vk::DescriptorType::eSampler);
    INSTANTIATE_DEREGISTER_DESCRIPTOR(vk::DescriptorType::eSampledImage);
    INSTANTIATE_DEREGISTER_DESCRIPTOR(vk::DescriptorType::eStorageImage);
    INSTANTIATE_DEREGISTER_DESCRIPTOR(vk::DescriptorType::eUniformBuffer);
    INSTANTIATE_DEREGISTER_DESCRIPTOR(vk::DescriptorType::eStorageBuffer);

#undef INSTANTIATE_DEREGISTER_DESCRIPTOR

} // namespace gfx::core::vulkan