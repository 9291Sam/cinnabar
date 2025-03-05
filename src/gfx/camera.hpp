#pragma once

#include "transform.hpp"
#include <glm/trigonometric.hpp>

namespace gfx
{
    class Game;

    class Camera
    {
    public:
        struct CameraDescriptor
        {
            float     aspect_ratio {};
            glm::vec3 position {0.0f, 0.0f, 0.0f};
            float     fov_y = glm::radians(70.0f);
            float     pitch = 0.0f;
            float     yaw   = 0.0f;
        };
    public:

        explicit Camera(CameraDescriptor);

        [[nodiscard]] glm::mat4 getPerspectiveMatrix(const Transform&) const;
        [[nodiscard]] glm::mat4 getModelMatrix() const;
        [[nodiscard]] glm::mat4 getViewMatrix() const;

        [[nodiscard]] glm::vec3 getForwardVector() const;
        [[nodiscard]] glm::vec3 getRightVector() const;
        [[nodiscard]] glm::vec3 getUpVector() const;
        [[nodiscard]] glm::vec3 getPosition() const;
        [[nodiscard]] float     getPitch() const;
        [[nodiscard]] float     getYaw() const;

        [[nodiscard]] gfx::Transform getTransform() const;

        void setPosition(glm::vec3);
        void addPosition(glm::vec3);
        void addPitch(float);
        void addYaw(float);

        explicit operator std::string () const;

    private:
        void updateTransformFromRotations();

        float pitch;
        float yaw;
        float fov_y;
        float aspect_ratio;

        Transform transform;
    };
} // namespace gfx
