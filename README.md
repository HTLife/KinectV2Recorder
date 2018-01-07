# KinectV2IMURecorder

KinectV2IMURecorder is a **KinectV2** and **LORD Microstrain IMU** recorder modified from  [Po-Chen Wu](https://github.com/Po-Chen)'s KinectV2Recorder.

![](./figs/screenshot.png)

## Dependency
  - [LORD Microstrain MSCL library](https://github.com/LORD-MicroStrain/MSCL)
  - Boost 1.64 ([Download Pre-Built Boost binaries for Windows](https://sourceforge.net/projects/boost/files/boost-binaries/1.64.0/))
  - OpenSSL [Win64 OpenSSL v1.1.0g](https://slproweb.com/download/Win64OpenSSL-1_1_0g.exe)
  - OpenCV3.4 [Windows self-extracting archive](https://sourceforge.net/projects/opencvlibrary/files/opencv-win/3.4.0/opencv-3.4.0-vc14_vc15.exe/download)

## Drivers
1. [Kinect for Windows SDK 2.0](https://www.microsoft.com/en-us/download/details.aspx?id=44561)
2. [Lord Microstrain Inertial Driver](http://www.microstrain.com/sites/default/files/lord_inertial_drivers_v4.1.0.msi)
3. [MIP monitor](https://s3.amazonaws.com/download.microstrain.com/MIP/MIP_Monitor_3.5.0.71_Installer.zip) (for IMU setup)

## MIP monitor setting
![](./figs/MIP1.jpg)
![](./figs/MIP2.jpg)
![](./figs/MIP3.jpg)

After the setting above is been setup, you could than open the KinectV2IMURecord and fetch packet from IMU sensor.  

Make sure COM port in  setting.ini is identical with the MIP monitor (last figure in this section, which is COM3).



# KinectV2Recorder

**Original Authors:** [Po-Chen Wu](https://github.com/Po-Chen)

Original KinectV2Recorder is a Win32 program used to record the **Color**, **Depth**, and **Infrared** sequences captured from Kinect V2. These three different sequences are somehow synchronized in hardware (30 fps). The depth and infrared frames are captured in the exact same time, followed by the color frame (~ 6 ms later).



### System Requirements for Using Kinect V2 Recorder

The system requirements for Kinect V2 according to [MSDN](https://msdn.microsoft.com/en-us/library/dn782036.aspx) are showed below.

##### a. Supported Operating Systems and Architectures
* Windows 8 (x64)
* Windows 8.1 (x64)
* Windows 8 Embedded Standard (x64)
* Windows 8.1 Embedded Standard (x64)
* Windows 10 (x64)

##### b. Recommended Hardware Configuration
* 64 bit (x64) processor
* 4 GB Memory (or more)
* i7 3.1 GHz (or higher)
* Built-in USB 3.0 host controller (only **Intel** or **Renesas** chipset is supported; Latest **ASMedia** chipset also works).
* DX11 capable graphics adapter
* A Kinect v2 sensor, which includes a power hub and USB cabling.
* **SSD** (for fast image storage)

##### c. Software Requirements
* Visual Studio 2012 or Visual Studio 2013 (or later)
* Kinect for Windows SDK 2.0 ([download](https://www.microsoft.com/en-us/download/details.aspx?id=44561))
* (Optional) Intel® Integrated Performance Primitives (IPP) ([download](https://software.intel.com/en-us/articles/free_ipp)) 
* (Optional) Use **RAM Disk** if SSD isn't fast enough ([download](https://www.softperfect.com/products/ramdisk/))

### Program Description
Kinect V2 Recorder is used to record image sequences at 30 fps (or just take pictures) with Kinect V2. Color images are stored in **PPM** (or **BMP** by *#define COLOR_BMP*) format (24 bits per pixel). Depth and infrared images are stored in **PGM** format (16 bits per pixel). D2D is used to achieve real-time display. Intel IPP is further used in regards to optimization. To enable using IPP, please following the project setup showed below.

![alt tag](https://raw.githubusercontent.com/Po-Chen/KinectV2Recorder/master/image/UseIntelIPP.png)

![alt tag](https://raw.githubusercontent.com/Po-Chen/KinectV2Recorder/master/image/Preprocessor.png)

### Proper Display
To facilitate better display of KinectV2Recorder, please go to your Desktop and right-click your mouse. Then go to Display Settings → Display → Change the size of text, apps, and other items: **100%**

### IPP DLL
Here are the necessary dlls in this program.  
[C:\Program Files (x86)\IntelSWTools\compilers_and_libraries_2016.2.180\windows\redist\ia32_win\ipp]
 -  ippi-9.0.dll
 -  ippcore-9.0.dll
 -  ippiw7-9.0.dll
