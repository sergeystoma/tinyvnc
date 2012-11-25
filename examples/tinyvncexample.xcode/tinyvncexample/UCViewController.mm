//
//  UCViewController.m
//  tinyvncexample
//
//  Created by Sergey Stoma on 11/25/12.
//  Copyright (c) 2012 Under Clouds Studio LLC. All rights reserved.
//

#import "UCViewController.h"

#import "vnc_client.hpp"

#define XK_MISCELLANY
#define XK_LATIN1
#import "keysymdef.h"

@interface DataImage : UIImage
{
@private
  CGImageRef      sourceImage;
}
- (id)initWithCGImage:(CGImageRef)imageRef;
@end

@implementation DataImage

- (id)initWithCGImage:(CGImageRef)imageRef
{
  self = [super initWithCGImage:imageRef];
  
  if (self)
  {
    sourceImage = imageRef;
    CGImageRetain(imageRef);
  }
  
  return  self;
}

- (void)dealloc
{
  CGImageRelease(sourceImage);
}
@end


@interface UCViewController ()
@end

@implementation UCViewController

@synthesize txtIp;
@synthesize txtPort;
@synthesize txtUsername;
@synthesize txtPassword;
@synthesize txtKeys;
@synthesize vwImage;

// Since CGDataProviderCreateWithData does not retain pixel data, we need to make a copy of it and provide some means of cleanup.
void releaseImageDataBuffer(void *info, const void *data, size_t size)
{
  free((void*)data);
}

-(IBAction)grabScreenshot:(id)sender
{
  [self.view endEditing:YES];
  
  // Main VNC client. Leaving this scope will disconnect client from server.
  Network::VncClient client([txtIp.text UTF8String], [txtPort.text UTF8String]);
 
  // Set username/password pair, if VNC authentication is used, only password will be sent to the server.
  client.set_password([txtUsername.text UTF8String], [txtPassword.text UTF8String]);
  
  bool waiting_update = false;
  int last_framebuffer_version = client.framebuffer_version();
  
  // client.update() must be called periodically. In order to avoid blocking UI thread, move it to a background thread.
  while (client.update())
  {
    // Connection lost or not yet established. Quit, or try to reestablish connection.
    if (!client.connected())
      continue;
    
    if (!waiting_update)
    {
      // Make client to maintain a most current current copy of remote screen.
      client.set_keep_framebuffer(true);
      
      // Ask server for screen capture.
      client.request_screen(false, 0, 0, client.framebuffer_width(), client.framebuffer_height());
      
      // Wait for screenshot.
      waiting_update = true;
    }
    else
    {
      // Framebuffer version counts a number of screen updates sent by the server.
      if (client.framebuffer_version() != last_framebuffer_version)
      {
        // Ugly code to create UIImage from raw pixel data.
        int width = client.framebuffer_width();
        int height = client.framebuffer_height();
        int nrOfColorComponents = client.framebuffer_bpp();
        int bitsPerColorComponent = 8;
        int rawImageDataLength = width * height * nrOfColorComponents;
        BOOL interpolateAndSmoothPixels = YES;
        CGBitmapInfo bitmapInfo = kCGBitmapByteOrderDefault;
        CGColorRenderingIntent renderingIntent = kCGRenderingIntentDefault;
        
        CGDataProviderRef dataProviderRef;
        CGColorSpaceRef colorSpaceRef;
        CGImageRef imageRef;
        
        @try
        {
          // Since VNC client will be destroyed at the end of this function, we need to extend lifetime of pixel data beyond.
          // Copy data and provide a way to cleanup.
          void *rawImageDataBuffer = malloc(rawImageDataLength);
          memcpy(rawImageDataBuffer, client.framebuffer(), rawImageDataLength);
          
          dataProviderRef = CGDataProviderCreateWithData(NULL, rawImageDataBuffer, rawImageDataLength, releaseImageDataBuffer);
          colorSpaceRef = CGColorSpaceCreateDeviceRGB();
          imageRef = CGImageCreate(width, height, bitsPerColorComponent, bitsPerColorComponent * nrOfColorComponents, width * nrOfColorComponents, colorSpaceRef, bitmapInfo, dataProviderRef, NULL, interpolateAndSmoothPixels, renderingIntent);
          UIImage *image = [DataImage imageWithCGImage:imageRef];
          [vwImage setImage:image];
        }
        @finally
        {
          CGDataProviderRelease(dataProviderRef);
          CGColorSpaceRelease(colorSpaceRef);
          CGImageRelease(imageRef);
        }
        
        break;
      }
    }
  }
}

-(IBAction)sendKeys:(id)sender
{
  // Main VNC client. Leaving this scope will disconnect client from server.
  Network::VncClient client([txtIp.text UTF8String], [txtPort.text UTF8String]);
  
  // Set username/password pair, if VNC authentication is used, only password will be sent to the server.
  client.set_password([txtUsername.text UTF8String], [txtPassword.text UTF8String]);
  
  bool sentSomething = false;
  
  // client.update() must be called periodically. In order to avoid blocking UI thread, move it to a background thread.
  while (client.update())
  {
    // Connection lost or not yet established. Quit, or try to reestablish connection.
    if (!client.connected())
      continue;
    
    if (!sentSomething)
    {
      NSString *text = txtKeys.text;
      
      for (int i = 0; i < text.length; ++i)
        // Simple alpha numeric key ids match their ASCII symbols.
        // pulse_key sends both key down and key up events.
        client.pulse_key([text characterAtIndex:i]);
      
      // More complex keys or combinations require usage of XK_ codes and send_key methods.
      //
      // client.send_key(XK_Shift_L, true);
      // client.send_key(XK_4, true);
      // client.send_key(XK_4, false);
      // client.send_key(XK_Shift_L, false);
      
      sentSomething = true;
      
      // Can't break just yet, we need to let client to send keystrokes on the next update()
    }
    else
    {
      break;
    }
  }
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
  [self.view endEditing:YES];
}

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  Network::initialize();
}

- (void)didReceiveMemoryWarning
{
  [super didReceiveMemoryWarning];
}

@end
