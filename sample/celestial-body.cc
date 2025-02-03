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
#include "png-texture.h"
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
    sphere_.set_color(glm::vec4(0.1F));
    bounded_.Commit();
  };
  void RayLeave(
      uint32_t /*serial*/, zukou::VirtualObject* /*virtual_object*/) override
  {
    std::puts(">>>> mime_types");
    for (const auto& mime_type : system_.data_offer_mime_types()) {
      printf("     %s\n", mime_type.c_str());
    }
    std::puts(">>>> mime_types end");

    sphere_.set_color(glm::vec4(0.F));
    bounded_.Commit();
  };

  void RayButton(uint32_t serial, uint32_t /*time*/, uint32_t button,
      bool pressed) override
  {
    if (button == BTN_LEFT && pressed) {
      bounded_.Move(serial);
    }
    if (button != BTN_RIGHT || !pressed) {
      return;
    }

    std::string text_plain("text/plain");
    std::string image_png("image/png");
    std::string image_jpeg("image/jpeg");  // TODO
    for (const auto& mime_type : system_.data_offer_mime_types()) {
      if (mime_type.substr(0, text_plain.size()).c_str() == text_plain) {
        PasteFilePath(mime_type);
      } else if (mime_type.substr(0, image_png.size()).c_str() == image_png) {
        PastePng(mime_type);
      }
    }
  };

  void RayAxisFrame(const zukou::RayAxisEvent& event) override
  {
    if (glm::abs(event.vertical) <= 0.1f) {
      return;
    }
    float diff = event.vertical / -500.0f;
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

  void PastePng(const std::string& mime_type)
  {
    printf("requesting clipboard (type=%s)\n", mime_type.c_str());
    system_.RequestDataOfferReceive(
        mime_type,
        [this](int fd, bool is_succeeded, void* /*data*/) {
          if (!is_succeeded) {
            return;
          }
          auto png_texture = std::make_unique<PngTexture>(&system_);
          if (!png_texture->Init()) {
            puts("Failed to load PNG texture");
            return;
          }
          if (!png_texture->Load(fd)) {
            puts("Failed to load PNG texture");
            return;
          }
          sphere_.ReBind(std::move(png_texture));
          bounded_.Commit();
        },
        nullptr);
  }
  void PasteFilePath(const std::string& mime_type)
  {
    printf("requesting clipboard (type=%s)\n", mime_type.c_str());
    system_.RequestDataOfferReceive(
        mime_type,
        [this](int fd, bool is_succeeded, void* /*data*/) {
          if (!is_succeeded) {
            return;
          }
          std::array<char, 1024> path_buffer;
          int size = read(fd, path_buffer.data(), path_buffer.size());
          path_buffer[size] = '\0';
          printf("try to open `%s`\n", path_buffer.data());
          close(fd);

          auto jpeg_texture = std::make_unique<JpegTexture>(&system_);
          if (!jpeg_texture->Init()) {
            puts("Failed to initialize JPEG texture");
            return;
          }
          if (!jpeg_texture->Load(path_buffer.data())) {
            puts("Failed to load JPEG texture");
            return;
          }
          sphere_.ReBind(std::move(jpeg_texture));
          bounded_.Commit();
        },
        nullptr);
  }
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
