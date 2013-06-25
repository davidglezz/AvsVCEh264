
#AvsVCEh264
Avisynth to H.264 Video Encoding using OpenVideo Encode library that provides an OpenCL API that leverages the video compression engine (VCE) on AMD platforms.

## Usage

```
AvsVCEh264 -i input.avs -o output.264 -c myConfig.ini
```

##Configuration file
You can use configuration files located in configs folder or create your own.
If you have questions about settings values you can read and take as an example default_explained.ini configuration file 

## Limitations
### Software
The OpenVideo library is currently supported only on Windows 7 (maybe Vista).
Catalyst driver release 12.8+

### Hardware
TrinityAPU, Radeon HD 7000 Series (Tahiti XT, CapeVerde) or newer GPU.

##Build
To build you need AMD APP SDK 2.7 or later and a compiler (Mingw, Microsoft Visual Studio 2008 or 2010)

## TODO
- Stdout output support.
- Do case insensitive config file.
- Timers.
- Buffered.


