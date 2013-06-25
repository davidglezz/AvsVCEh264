
#H.264 Video Encoding Using OpenVideo

##Description
The OpenVideo Encode library provides an OpenCL API that leverages the video compression engine (VCE) on AMD platforms for H.264 encoding.


## Usage

```
AvsVCEh264 -i input.avs -o output.264 -c myConfig.ini

```

##Configuration
Read default_explained.ini file.

## Limitations
### Software
The OpenVideo library is currently supported only on Windows 7 (maybe Vista).
Catalyst driver release 12.8+

### Hardware
TrinityAPU
Radeon HD 7000 Series (Tahiti XT, CapeVerde) or newer

###Build
Need AMD APP SDK 2.7 or later and:
- Mingw
- Microsoft Visual Studio 2008 or 2010

## TODO
- Stdout output support
- Do case insensitive config file
- Timers
- Bufered


