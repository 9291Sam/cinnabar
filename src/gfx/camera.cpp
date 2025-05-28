#include "camera.hpp"
#include "transform.hpp"
#include <algorithm>
#include <format>

namespace gfx
{
    Camera::Camera()
        : Camera {CameraDescriptor {}}
    {}

    Camera::Camera(CameraDescriptor cameraDescriptor)
        : pitch {cameraDescriptor.pitch}
        , yaw {cameraDescriptor.yaw}
        , fov_y {cameraDescriptor.fov_y}
        , aspect_ratio {cameraDescriptor.aspect_ratio}
        , transform {.translation {cameraDescriptor.position}}
    {}

    glm::mat4 InfiniteReversedPerspective(f32 fov, f32 aspectRatio, f32 nearPlane)
    {
        f32 f = 1.0f / tan(fov * 0.5f);

        glm::mat4 result = glm::mat4(0.0f);
        result[0][0]     = f / aspectRatio;
        result[1][1]     = f;
        result[2][2]     = 0.0f; // This makes the depth buffer reversed
        result[2][3]     = -1.0f;
        result[3][2]     = nearPlane; // Pushing near plane to zero
        result[3][3]     = 0.0f;

        return result;
    }

    glm::mat4 Camera::getPerspectiveMatrix(const Transform& transform_) const
    {
        return this->getProjectionMatrix() * this->getViewMatrix() * transform_.asModelMatrix();
    }

    glm::mat4 Camera::getProjectionMatrix() const
    {
        return InfiniteReversedPerspective(this->fov_y, this->aspect_ratio, 0.1f);
    }

    glm::mat4 Camera::getViewMatrix() const
    {
        return glm::inverse(this->transform.asTranslationMatrix() * this->transform.asRotationMatrix());
    }

    glm::vec3 Camera::getForwardVector() const
    {
        return this->transform.getForwardVector();
    }

    glm::vec3 Camera::getRightVector() const
    {
        return this->transform.getRightVector();
    }

    glm::vec3 Camera::getUpVector() const
    {
        return this->transform.getUpVector();
    }

    glm::vec3 Camera::getPosition() const
    {
        return this->transform.translation;
    }

    f32 Camera::getAspectRatio() const
    {
        return this->aspect_ratio;
    }

    f32 Camera::getFovYRadians() const
    {
        return this->fov_y;
    }

    f32 Camera::getPitch() const
    {
        return this->pitch;
    }

    f32 Camera::getYaw() const
    {
        return this->yaw;
    }

    Transform Camera::getTransform() const
    {
        return this->transform;
    }

    void Camera::setPosition(glm::vec3 newPos)
    {
        this->transform.translation = newPos;
    }

    void Camera::addPosition(glm::vec3 positionToAdd)
    {
        this->transform.translation += positionToAdd;
    }

    void Camera::addPitch(f32 pitchToAdd)
    {
        this->pitch += pitchToAdd;
        this->updateTransformFromRotations();
        // const f32 pitchBefore =
        // glm::eulerAngles(this->transform.rotation).p; if (pitchBefore +
        // pitchToAdd > glm::half_pi<f32>())
        // {
        //     this->transform.pitchBy(glm::half_pi<f32>() - pitchBefore);

        //     util::logTrace("Yawing by {:.8f}", yawToAdd);
        // }
        // else
        // {
        //     this->transform.pitchBy(pitchToAdd);
        // }
    }

    void Camera::addYaw(f32 yawToAdd)
    {
        this->yaw += yawToAdd;
        this->updateTransformFromRotations();
    }

    Camera::operator std::string () const
    {
        return std::format("{}Pitch {} | Yaw {}", static_cast<std::string>(this->transform), this->pitch, this->yaw);
    }

    void Camera::updateTransformFromRotations()
    {
        glm::quat q {1.0f, 0.0f, 0.0f, 0.0f};

        this->pitch = std::clamp(this->pitch, -glm::half_pi<f32>(), glm::half_pi<f32>());
        this->yaw   = glm::mod(this->yaw, glm::two_pi<f32>());

        q *= glm::angleAxis(this->pitch, -Transform::RightVector);

        q = glm::angleAxis(this->yaw, -Transform::UpVector) * q;

        this->transform.rotation = q;
    }
} // namespace gfx
