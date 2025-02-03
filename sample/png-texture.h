#pragma once

#include <zukou.h>

class PngTexture final : public zukou::GlTexture
{
 public:
  DISABLE_MOVE_AND_COPY(PngTexture);
  PngTexture(zukou::System *system);
  ~PngTexture();

  bool Init() { return zukou::GlTexture::Init(); }
  bool Load(int fd);

 private:
  bool loaded_ = false;

  int fd_ = 0;
  zukou::ShmPool pool_;
  zukou::Buffer texture_buffer_;
};
