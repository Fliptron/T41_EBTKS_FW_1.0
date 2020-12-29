# Commit Details

It seems the Check-ins to the repo have way too many minor, major, and cosmetic
changes, so the limited comment line really does not cover what has changed.
Starting with Commit 76 on 11/22/2020 , I will try and do better, and keep track of what is going on in this file.


## Commit \#76 11/22/2020

Changes:
-   Locked which version of **SdFat** and **FastLed** libraries are used in platformio.ini lib_deps section
-   Moved the Boot Log output to the Poll Loop, improvements to how it is displayed
-   Improved handling of SD Card if it is not installed
-   Fix to HPIBOutput()
-   SDREN re-write
-   Minimal Verbose reporting added to all Keywords. More to come.
-   Improvements to the built-in Logic Analyzer output formatting
-   Resolve_Path() now trims leading and trailing spaces from the new path
-   Many minor documentation improvements in about 12 files
-   TXD diagnostic signal is now pulsed by the built-in Logic Analyzer to trigger
    an external Logic Analyzer. This allows comparison of the two Logic Analyzers,
    and the built-in Logic Analyzer now has time stamps on all sample to
    facilitate this capability. This needs to be made conditional so that TXD can be used for other diagnostic purposes
-   A deliberate glitch in the RXD diagnostic signal used to separate Phi 1
    from Phi 2 interrupt processing has been removed, as it contributed a
    minor additional delay to the RC signal.
-   Moved the built-in Logic Analyzer code in Phi 2 processing to after RC
    is asserted, thus reducing the delay to the start of RC. There is more
    work to be done on this front.
-   Started adding support for HP86/86 type ROMs, and the special ROM headers
    for the Secondary AUXROMs.
-   Minor bug fixed in the built-in Logic Analyzer if Ctrl-C was used to end
    the prior acquisition. Affected re-issuing `la go` without doing a new `la setup`
-   With the recent acquisition of a better external Logic analyzer, documented
    the new probing arrangement: `Logic Analyzer Configuration 6 for TLA5201`

### What's going on

There has been an ongoing search for what is causing random failures of the
status reported by the SDREN (rename a file on the SD Card, including changing
the path). The command always does as intended (assuming correct parameters)
but can report either that the command failed, or even a general SD error.
It appears that the issue is the timing of the RC signal has a late start.
The current working theory is that something in the SdFat library, or the
SDIO library that it depends on is delaying the start of RC being asserted,
possibly due to DMA activity on the Teensy 4.1  . Yet other commands that do
reads and writes to the SD card do not show the problem to the same extent.
This commit rearranged some of the code in the Phi 2 processing (were RC is
controlled) seems to have mitigated the issue but further work is required
to achieve a better level of confidence.

Oscilloscope captures still indicate the RC signal generated by EBTKS still
violates timing spec Tif2 (380 ns max) from the 1MB5 spec, by 100 ns.

Fixing this will involve the most of the timing critical code in
`   EBTKS_Bus_Interface_ISR.cpp`

One of the attempted workarounds was to put the HP85 into DMA hold while
this statement executed:
`   rename_status = SD.rename(Resolved_Old_Path, Resolved_Path);`

yet even this failed, casting doubt on the above theory of the root cause
of the problem.

Another experiment involved moving the storage for all the ROMs from DMAMEM
to FASTRUN memory. It only had a minor effect on the issue. This should be
tried again after other experiments have been tried and implemented.

## Commit \#78 11/23/2020

Changes:
-   Modify MOUNT for tape and disk when the media needs to be created. Now uses
    a shared copy function that is also used for SDCOPY.
-   Implemented SDCOPY

## Commit \#79 11/25/2020

Changes:
-   Updated the possible error messages and AUXERRN numbers reported in
    [Keyword Documentation](http://www.fliptronics.com/HP-85_Adventures/AUXROM_Keywords.html)
-   Update the documentation for AUXCMD, AUXOPT$, AUXBUF$ in
    [Keyword Documentation](http://www.fliptronics.com/HP-85_Adventures/AUXROM_Keywords.html)
-   Change all references to AUXROM_SDMEDIA() to AUXROM_MEDIA()
-   Update some documentation at the top of EBTKS_AUXROM.cpp for AUXCMD, AUXBUF$, AUXOPT$
-   Implement SPRINTF drawing extensively from Everett's emulator code.
    Only the most minimal tests. Crashes HP85 when format string is bad, like "%D"
    about 300 lines of untested code.
-   Disabled USBHost related class instances at top of EBTKS.cpp

## Commit \#80 11/26/2020

Changes:
-   SPRINTF, structure and functionality from Everett, but rewritten in the style
    of the surrounding code, and with many variable names changed, and different
    buffer structure. Also a few bugs found.

## Commit \#81 11/26/2020

Changes:
-   SDCOPY now has an optional Overwrite flag. If no flag is provided, or it is 0
    then Overwrite is an error. If it is 1, the overwrite is allowed.
    Update related website documentation
-   SDSLASH$ Updated related website documentation
-   In EBTKS_SD.cpp, improve documentaion about how ROM headers are processed during
    boot, given new support for HP86/87. Also some mostly cosmetic changes to
    other documentation, and some error messages
-   Trivial formatting bug in EBTKS.cpp

## Commit \#82 12/02/2020

Changes:
-   Added SDEOF(file\#) keyword
-   Added SDEXISTS(filePathAndName$) keyword
-   Updated Documentation for SDCAT, SDFFIRST and SDFNEXT. Update the associated custom error messages
-   Implemented EBTKSREV$
    added EBTKS_FIRMWARE_VERSION in EBTKS.h
-   Some prior commit added SETLED
-   SDCLOSE now sends a message to the serial console for each file closed
-   VERBOSE_KEYWORDS now sends a message to the serial console for each Keyword called
    May also provide additional information
-   Minor changes and more debug serial output for SPRINTF
-   Minor changes to SDREN while debugging Phi 2 to RC timing.

## Commit \#83 12/02/2020

Changes:
-   Added documentation on the Term85 virtual terminal object. By RB
    Light editing by PMF

## Commit \#84 12/03/2020

Changes:
-   Integrate the first pass at EMS support, by RB, into EBTKS_Bus_Interface_ISR.cpp
    also cleaned up some formatting
-   change onReadData() to just use the global addReg rather than passing it as a parameter
    since it is only called from one place, and addReg was the only value that is ever passed to it
-   Removed EBTKS_EXTMEM.cpp, as it was just comments

## Commit \#85 12/12/2020

Changes:
-   First successful attempt at dealing with random bus cycle timing when SD Card
    reads occur using quite byzantine code (by PMF). Issue was tracked down to a
    small critical region in the SdFat library that was disabling all interrups.
    This then randomly made the EBTKS processing of HP85 bus cycles violate timing.
-   Second successful attempt at dealing with random bus cycle timing when SD Card
    reads occur by changing the opperating mode of the SD Card from FIFO_SDIO to
    DMA_SDIO which then leads to the code never going down the path in the SdFat
    library that has the critical section (RB). Unless further issues are
    identified, we will stick with this solution 
-   Change from supporting Pin interrupts for Phi 1 Rising and Phi 2 Rising to
    only Phi 1 Rising
-   Rename onPhi_2_Rise() to mid_cycle_processing() since it is no longer anchored
    to to the Phi 2 rising edge
-   Reorganize all the pin change interrupt code, and make function sequence match
    execution sequence
-   Identify and document all ISR operations. Look for it just prior to
    pinChange_isr() in EBTKS_Bus_Interface_ISR.cpp
-   Change some global functions to local static
-   Extensive changes to EBTKS_Bus_Interface_ISR.cpp
-       Significant new documentation, detailed timing analysis will be in next commit
-       In mid_cycle_processing() move some code that recognizes DMA and Interrupt acknowledgement
-       Merge various EMC_SUPPORT code sequences
-   Delete some of Philip's code that has been commented out for months
-   Mark places in the code that we need to return to with #### tags
-   Print some dots on the diagnostic terminal when waiting for HP85 to boot after the EBTKS boot
-   Change /roms to /roms85 on the SD Card, in preparation to support HP86/87
-   Removed a 2 second delay in the EBTKS boot process

## Commit \#86 12/12/2020

Changes:
-   Correct the date in commit 85 note, above.
-   Fix the Firmware version message in EBTKS.h

## Commit \#87 12/13/2020

Changes:
-   Add new feature to UNMOUNT. Now recognizes "SDCard"              --- not case sensitive
-   Add new feature to MOUNT.   Now recognizes "SDCard", "anything"  --- not case sensitive

## Commit \#88 12/16/2020

Changes:
-   Add diagnostic messages to MOUNT for mode 1, Create if LIF does not exits
-   Figured out why SD Card read acces while copying Ref LIF image for create
    new virtual disk in MOUNT was randomly failing. When we changed from FIFO_SDIO
    mode to DMA_SDIO, the memory used for the temporary buffer for the copy
    operation became a problem, as it was in EXTMEM. Once it was changed to
    DMAMEM, the problems stopped.
-   Added a documentation section titled "Main Uses of DMAMEM" to EBTKS.cpp
-   In EBTKS_Bus_Interface_ISR.cpp , added noe above pinChange_isr() reminding
    that the function prototype must include
        __attribute__ ((interrupt ("IRQ")))
-   Now that MOUNT LIF create works, benchmark different read block sizes
-   General improvements to copy_sd_file() that catches errors better, and
    reports them. Part of MOUNT improvement.
-   General improvements to MOUNT error reporting and diagnostic prints.
-   Platformio.ini
        Changed SdFat library from 2.0.0-beta.8 to 2.0.2-beta.3
        Added "-Wl,--print-memory-usage" which better reports memory usage at
        and of compile/Link

## Commit \#89 12/20/2020

Changes:
-   Exhaustive characterization of the timing delays within the very many
    parts of the Pin Change Interrupt
-   Added dump_devices_array() in EBTKS_Utilities.cpp , not currently used
-   There is an unresolved issue with the Serial Monitor command "media"
    with handling the printer association with the file /printers/printfile.txt
-   There is an unresolved issue with the "printer is 310" assignment (which
    needs ROM 360 printer/plotter to be installed) does not actually seem to
    write to the associated file when PRINT ... command are given. This used
    to work.
-   There is an unresolved issue with the MOUNT/UNMOUNT "SDCard" crashing the
    system.

## Commit \#90 12/28/2020

Changes:
-   More Timing Analysis of the Pin Change Interrupt
-   Report Series 80 Model during boot, improve other boot messages
-   Re-organize the code around the SD.Begin()
-   Changed all the TXD_Pulser() to RXD_Pulser()
-   Minor documentation edit to platformio.ini
-   Updates to HPIB based Printing (to a file on the SD Card)
-       HpibDisk type becomes HpibDevice type
-       HpibDevice array now includes device type and Talk/Listen address (tla / _tla)
-       New virtual functions close() and getFilename()
-       New function to check device type isType()
-   Modify HPIBOutput if the character is EOL
-   Diagnostic terminal "media" command now lists printer output file
-   Detailed documentaion in EBTKS.cpp describing the issues with FIFO_SDIO mode
    access to the SD Card, and our changing over to DMA_SDIO mode

## Commit \#91 12/??/2020                                                                           Remember to update EBTKS.h

Changes:
-   




