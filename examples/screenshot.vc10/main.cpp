#include "../../src/vnc_client.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

int main(int argc, char** argv)
{
  if (argc != 6)
  {
    printf("Usage: screenshot ip-address port username password capture-filename\n");
    printf("For example: screenshot 192.168.1.100 5900 user pass screen.png\n");
    return;
  }

  Network::initialize();

  Network::VncClient client(argv[1], argv[2]);

  client.set_password(argv[3], argv[4]);

  bool waiting_update = false;
  int last_framebuffer_version = client.framebuffer_version();

  while (client.update())
  {
    if (!client.connected())
      continue;

    if (waiting_update)
    {
      if (client.framebuffer_version() != last_framebuffer_version)
      {
        stbi_write_png(argv[5], 
          client.framebuffer_width(), 
          client.framebuffer_height(), 
          client.framebuffer_bpp(), 
          client.framebuffer(),
          client.framebuffer_width() * client.framebuffer_bpp());

        break;
      }
    }
    else
    {
      client.set_keep_framebuffer(true);

      client.request_screen(false, 0, 0, client.framebuffer_width(), client.framebuffer_height());

      waiting_update = true;
    }
  }
}