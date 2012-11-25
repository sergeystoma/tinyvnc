# TinyVNC #

TinyVNC is a minimalistic VNC library, main part of Blender Keypad (http://itunes.apple.com/us/app/blender-keypad/id430784289) remote control app. It is oriented primarily on sending keystokes over network to remote computer. It also has ability of capturing a screen of that computer, even though it is not a primary function of the software, so only RAW (warning, huge amount of traffic) encoding is supported.

# Building #

Easiest way to use tinyvnc is as a static library. Simply include all .cpp files into the project and you should be good to go. You may need to declare CRYPTOPP_MANUALLY_INSTANTIATE_TEMPLATES macro, in which case exclude ec2n.cpp, eccrypto.cpp, eprecomp.cpp, strciphr.cpp from build to avoid duplicate class and method definitions.

# Building for XCode Step by Step #

* Add all library files to your project:
  * des_local.cpp, des_local.h, keysymdef.h, raw_query.cpp, raw_query.hpp, vnc_client.cpp, vnc_client.hpp
  * All files from cryptoppmin directory


* Exclude following files from the build: ec2n.cpp, eccrypto.cpp, eprecomp.cpp, strciphr.cpp
  * Click on the file in the Project Navigator (Command + 1, on the left by default)
  * Clear checkbox for Target Membership in the File Inspector (Command + Alt + 1, on the right by default)


* In project Build Settings, define following Preprocessor Macros:
  * CRYPTOPP_DISABLE_ASM
  * CRYPTOPP_MANUALLY_INSTANTIATE_TEMPLATES


* Make sure that files from which you are using TinyVNC are compiled as Objective-C++. Easiest way is to change file extension from .m to .mm


# Basic Usage

```
#include "vnc_client.hpp"
#define XK_MISCELLANY
#define XK_LATIN1
#include "keysymdef.h"

// Initialize network client. Needs to be done only once, e.g. in viewDidLoad.
Network::initialize();

// Create and configure client.
Network::VncClient client("192.168.1.1", "5900");

// Set username/password pair, if VNC authentication is used, only password will be sent to the server.
client.set_password("username", "password");

// USE THREADING FOR BEST RESULTS!

// Wait to connect.
while (client.update())
  if (client.connected())
    break;

// Send Hello + enter key.
client.pulse_key('H');
client.pulse_key('e');
client.pulse_key('l');
client.pulse_key('l');
client.pulse_key('o');
client.pulse_key(XK_Return);

// Send to server.
client.update();

// More complex keys or combinations require usage of XK_ codes and send_key methods.
// E.g. send $ keystroke.

client.send_key(XK_Shift_L, true);
client.send_key(XK_4, true);
client.send_key(XK_4, false);
client.send_key(XK_Shift_L, false);

```

# TODO #

XCode static library project file

Threaded example

# Examples #

examples/screenshot contains a small command line test app that will take a screenshot of a remote server and save it to .png file.

# Authentication #

TinyVNC supports anonymous access (No authentication), VNC password authentication, or OS X authentication method using embedded Crypto++ library.

# Third Party Software and Licenses #

TinyVNC is built with the help of and contains portions of:

Crypto++, web site: http://www.cryptopp.com/, license: http://www.cryptopp.com/License.txt

D3DES by Richard Outerbridge

    /* D3DES (V5.09) -
     *
     * A portable, public domain, version of the Data Encryption Standard.
     *
     * Written with Symantec's THINK (Lightspeed) C by Richard Outerbridge.
     * Thanks to: Dan Hoey for his excellent Initial and Inverse permutation
     * code;  Jim Gillogly & Phil Karn for the DES key schedule code; Dennis
     * Ferguson, Eric Young and Dana How for comparing notes; and Ray Lau,
     * for humouring me on.
     *
     * Copyright (c) 1988,1989,1990,1991,1992 by Richard Outerbridge.
     * (GEnie : OUTER; CIS : [71755,204]) Graven Imagery, 1992.
     */

STB Image Writer

    /* stbiw-0.92 - public domain - http://nothings.org/stb/stb_image_write.h
       writes out PNG/BMP/TGA images to C stdio - Sean Barrett 2010
                                no warranty implied; use at your own risk
