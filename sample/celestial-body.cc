#include <zukou.h>

#include <linux/input-event-codes.h>
#include <unistd.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <glm/common.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/quaternion_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>
#include <string>

#include "jpeg-texture.h"
#include "sphere.h"

class CelestialBody final : public zukou::IBoundedDelegate,
                            public zukou::ISystemDelegate
{
 public:
  CelestialBody(const char* texture_path)
      : system_(this),
        bounded_(&system_, this),
        texture_path_(texture_path),
        sphere_(&system_, &bounded_, 8)
  {}

  bool Init(float radius)
  {
    if (!system_.Init()) return false;
    if (!bounded_.Init(glm::vec3(radius))) return false;

    auto jpeg_texture = std::make_unique<JpegTexture>(&system_);

    if (!jpeg_texture->Init()) return false;
    if (!jpeg_texture->Load(texture_path_)) return false;

    sphere_.Bind(std::move(jpeg_texture));

    return true;
  }

  int Run() { return system_.Run(); }

  void Configure(glm::vec3 half_size, uint32_t serial) override
  {
    radius_ = glm::min(half_size.x, glm::min(half_size.y, half_size.z));
    sphere_.Render(radius_, glm::mat4(1));

    zukou::Region region(&system_);
    region.Init();
    region.AddCuboid(half_size, glm::vec3(), glm::quat());

    bounded_.SetTitle("Zukou Celestial Body");
    bounded_.SetRegion(&region);
    bounded_.AckConfigure(serial);
    bounded_.Commit();
  }

  void RayEnter(uint32_t /*serial*/, zukou::VirtualObject* /*virtual_object*/,
      glm::vec3 /*origin*/, glm::vec3 /*direction*/) override
  {
    sphere_.set_color(glm::vec4(0.2F));
    bounded_.Commit();
  };
  void RayLeave(
      uint32_t /*serial*/, zukou::VirtualObject* /*virtual_object*/) override
  {
    sphere_.set_color(glm::vec4(0.F));
    bounded_.Commit();
  };

  void RayButton(uint32_t serial, uint32_t /*time*/, uint32_t button,
      bool pressed) override
  {
    std::printf("Button: %d, pressed: %d\n", button, pressed);
    if (button == BTN_LEFT && pressed) {
      bounded_.Move(serial);
    }
    if (button != BTN_RIGHT || !pressed) {
      return;
    }
    std::string mime_type = "text/plain;charset=utf-8";
    system_.RequestDataOfferReceive(
        mime_type,
        [](int fd, bool is_succeeded, void* /*data*/) {
          if (is_succeeded) {
            std::array<char, 1024> buf;
            read(fd, buf.data(), buf.size());
            printf("clipboard: %s\n", buf.data());
            close(fd);
          }
        },
        nullptr);
  };

  void RayAxisFrame(const zukou::RayAxisEvent& event) override
  {
    if (glm::abs(event.vertical) <= 0.1f) {
      return;
    }
    float diff = event.vertical / 365.0f;
    rotate_ *= glm::rotate(glm::mat4(1.F), diff, glm::vec3{0, 1, 0});
    sphere_.Render(radius_, rotate_);
    bounded_.Commit();
  }

 private:
  zukou::System system_;
  zukou::Bounded bounded_;

  float radius_;
  glm::mat4 rotate_ = glm::mat4(1.F);

  const char* texture_path_;

  Sphere sphere_;
};

const char* usage =
    "Usage: %s <texture>\n"
    "\n"
    "    texture: Surface texture in JPEG format"
    "\n";

int
main(int argc, char const* argv[])
{
  if (argc != 2) {
    fprintf(stderr, "jpeg-file is not specified\n\n");
    fprintf(stderr, usage, argv[0]);
    return EXIT_FAILURE;
  }

  CelestialBody celestial_body(argv[1]);

  if (!celestial_body.Init(0.2)) return EXIT_FAILURE;

  return celestial_body.Run();
}
