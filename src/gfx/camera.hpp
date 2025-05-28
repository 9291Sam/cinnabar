#pragma once

#include "transform.hpp"
#include "util/util.hpp"
#include <glm/trigonometric.hpp>

namespace gfx
{

    class Camera
    {
    public:
        struct CameraDescriptor
        {
            f32       aspect_ratio {};
            glm::vec3 position {0.0f, 0.0f, 0.0f};
            f32       fov_y = glm::radians(70.0f);
            f32       pitch = 0.0f;
            f32       yaw   = 0.0f;
        };
    public:

        Camera();
        explicit Camera(CameraDescriptor);
        ~Camera() = default;

        Camera(const Camera&)             = default;
        Camera(Camera&&)                  = default;
        Camera& operator= (const Camera&) = default;
        Camera& operator= (Camera&&)      = default;

        [[nodiscard]] glm::mat4 getPerspectiveMatrix(const Transform&) const;
        [[nodiscard]] glm::mat4 getProjectionMatrix() const;
        [[nodiscard]] glm::mat4 getViewMatrix() const;

        [[nodiscard]] glm::vec3 getForwardVector() const;
        [[nodiscard]] glm::vec3 getRightVector() const;
        [[nodiscard]] glm::vec3 getUpVector() const;
        [[nodiscard]] glm::vec3 getPosition() const;
        [[nodiscard]] f32       getAspectRatio() const;
        [[nodiscard]] f32       getFovYRadians() const;
        [[nodiscard]] f32       getPitch() const;
        [[nodiscard]] f32       getYaw() const;

        [[nodiscard]] gfx::Transform getTransform() const;

        void setPosition(glm::vec3);
        void addPosition(glm::vec3);
        void addPitch(f32);
        void addYaw(f32);

        explicit operator std::string () const;

    private:
        void updateTransformFromRotations();

        f32 pitch;
        f32 yaw;
        f32 fov_y;
        f32 aspect_ratio;

        Transform transform;
    };
} // namespace gfx
