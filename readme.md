# DirectDraw 7.0

DirectDraw's headers are inconsistent across vintage version of Visual Studio and other older compilers. To ensure full access to the DirectDraw 7.0 headers, this repository contains the latest version of the DirectDraw headers modified to work on numerous older compilers.

## Why?

When working on older codebases, some headers, such as ``ddrawex.h`` have been deprecated and don't exist in current iterations of the Windows SDKs. These older headers were located and fixed to work both on older and current compilers so vintage codebases can be quickly brought up on a modern development compiler toolchain.

## MultiMon.h

This header is missing from the Open Watcom 1.9 distribution. It was copied into this library so the DirectDraw sample **MultiMon** can compile and link on all supported compilers. The header was also modified so it will compile without errors or warnings. Read the information in the [Multimon Readme file](Samples/multimon/readme.md)

## Source of the SDK

The SDK is obtained from numerous sources, from DirectX SDK June 2010, the Windows DDK, Visual Studio 2022, Codewarrior, and scouring old MSDN CDs. This SDK can be used to supercede even modern SDKs since it contains all APIs available to DirectDraw 7.0.

## Supported compilers

This SDK is tested on Open Watcom 1.9, CodeWarrior 9.0 for Windows, Visual Studio .NET 2003, Visual Studio 2005, Visual Studio 2008, Visual Studio 2010, Visual Studio 2012, Visual Studio 2013, Visual Studio 2015, Visual Studio 2017, Visual Studio 2019, and Visual Studio 2022.
