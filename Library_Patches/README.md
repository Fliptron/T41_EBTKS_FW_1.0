# Explanation about the Library Patches

This information comes from Commit_Details.md commit \#133 .

More explicit instruction are given here on the procedure to update the
library modules.


-   Three functions in Teensyduino still need to be updated due to issues discovered
    in 2020 and 2021 that we have been unable to convince the maintainers are an
    issue.
    
    **Before** and **After** versions are now available in a new directory
    named **Library_Patches**
    
    The recomendation is to match the before versions of the functions with the copy
    in TeensyDuino V1.58. They should be identical. Then rename the ones in the
    TeensyDuino Library by appending **_OLD** to file name extension.
    Then copy the replacement from the after directory to the appropriate place.
    
    In C:\Users\\--your-name---\\.platformio\packages\framework-arduinoteensy\cores\teensy4 you will find

    -   HardwareSerial.cpp
    -   usb.c

    In C:\Users\\--your-name---\\.platformio\packages\framework-arduinoteensy\libraries\USBHost_t36 you will find
    
    -   ehci.cpp

## Tools for comparing text files

**the following is a direct copy from the \#135 commit notes** found in Commit_Details.md

My preferred text file comparison tool is UltraCompare that creates very nice side-by-side
displays of the differences between two files. You may have your own favorite.

Although I haven't used it for this purpose, Visual Studio Code
obviously has a built-in file compare capability. For files that are
not part of the repo (the ones in the library directories ouside the
repo), they can be accessed with Windows file Explorer, and dragged
onto the tab area at the top of the main window. This allows editing
and comparison.

So, Options are:

-   UltraCompare
-   Your favorite text compare utility
-   Visual Studio Code built-in side by side compare (which is very much like UltraCompare)
-   Windows FC
-   Git Bash diff

### UltraCompare or your fav alternative

For UltraCompare, if you have it, you know how to use it. Same for your own favorite.

### VS Code file compare

For files in the Library_Patches directory, first make them visible by
expanding the folders in the left most panel. For files that are
outside the repo (the library files that you will be replacing with
the patched versions, open the Windows File Explorer, and navigate to
their location. Drag the file name to the tab bar at the top of the
main VS Code window, to open the file for editing. Do NOT edit the
file.

The three relevant library files to be patched are here:

    C:\Users\\--your-name---\\.platformio\packages\framework-arduinoteensy\cores\teensy4\HardwareSerial.cpp
    C:\Users\\--your-name---\\.platformio\packages\framework-arduinoteensy\cores\teensy4\usb.c
    C:\Users\\--your-name---\\.platformio\packages\framework-arduinoteensy\libraries\USBHost_t36\ehci.cpp

The procedure is as follows

-   Follow the above instructions to add the file you want to compare from the external library
-   At the top of the left most panel in VSCode, expand "OPEN EDITORS" , the file you just
    added to the tab area should be visible
-   Expand the Librar_Patches section of the projects files
-   To make sure that the library you are about to patch is the same one that this repo
    expects, go to the "Before" sub directory, and on the file you want to compare,
    right click and select "Select for Compare"
-   Go to the OPEN EDITORS section, and right click on the file that is in the external
    library and click on "Compare with Selected". You should see the two-up display of the
    file, and there should be no visible differences. If there are differences, you have either
    already patched the library, or we are not working on the same version of TeensyDuino.
    If it is a version issue, then the next step *may*  guide you on how to do the required patches.
-   Do this for all 3 library files
-   To see what the changes will be, select the same files from the before and after directories
    with "Select for Compare" and "Compare with Selected" to see the required patch.
-   Finally, after checking all three files, append _OLD to the extension of each file in
    the library, to disable it, without  removing it. Then copy in the patched version from
    the After directory. Note that ehci.cpp is not in the same directory as the other two.

### Diff and FC

As you are using Visual Studio Code and Platformio, **Git** tools have probably been installed
so that you can clone the repo from GitHub. In the Windows File Explorer, open the directory
that contains platformio.ini (the root of the repo) and see if

    "Open Git Bash here"

is an option. If so, then you will be able to run a version of **diff**

In desperation, you can use the Microsoft Windows supplied utility **FC**.

For the two copies of the repo, don't make any changes to reference. In the other copy,
before you start making code changes, do a clean and a build. select all the text in
the build process logging windows, and save it in the same directory as platformio.ini
and name it Compile.log . Then in a cmd.exe window, compare the build.log you just
created with the one in the reference directory.

    Git Bash shell example
    $ diff /i/Repos/EBTKS_FW/build.log /i/Repos/reference/build.log

    Windows 11 FC example
    I:\>fc /L /N I:\Repos\EBTKS_FW\build.log  I:\Repos\reference\build.log

For the rest of the patching process, it is described in detail in the "VS Code file compare"
section above.

















First off, thank you very much for using ArduinoJson.

We'll be very happy to help you, but first please read the following.

## Before asking for help

1. Read the [FAQ](https://arduinojson.org/faq/?utm_source=github&utm_medium=support)
2. Search in the [API Reference](https://arduinojson.org/api/?utm_source=github&utm_medium=support)

If you did not find the answer, please create a [new issue on GitHub](https://github.com/bblanchon/ArduinoJson/issues/new).

It is OK to add a comment to a currently opened issue, but please avoid adding comments to a closed issue.

## Before hitting the Submit button

Please provide all the relevant information:

* Good title
* Short description of the problem
* Target platform
* Compiler model and version
* [MVCE](https://stackoverflow.com/help/mcve)
* Compiler output

Good questions get fast answers!
