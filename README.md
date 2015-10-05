# SlimVDI Readme

This is a fork of CloneVDI (written by Don Mile) that has been renamed to SlimVDI according to the license of CloneVDI.
This readme is based on the "release notes.txt" of CloneVDI and has been updated for SlimVDI and markdown syntax.

PLEASE READ THIS DOCUMENT CAREFULLY, IT SHOULD CONTAIN ANSWERS TO MOST QUESTIONS.

**Intended audience:** users of Oracle's "VirtualBox", a Virtual Machine application.

**Addendum:** SlimVDI supports VDI and other formats as input. You may assume that, unless I specifically
say otherwise, then anything you can do with a VDI as input, you can also do with the other
formats as input. That specifically includes keeping the old or generating a new UUID, compacting
while copying, and enlarging the drive. The only thing to remember is that the output from
SlimVDI is always a dynamically expanding VDI, the other formats are supported for reading
only.

USAGE INSTRUCTIONS FOLLOW THE FAQ SECTION.


Legal Disclaimers
-----------------
Basically, you got this tool for nothing, so I accept no responsibility for anything
that goes wrong. While I made every effort to make this tool safe and reliable, I offer
NO guarantee that I succeeded. The software may be used by anyone at his or her own risk.


FAQ
---
### What does it do?
   It makes copies of (i.e. clones) Oracle VirtualBox Virtual Disk Image (VDI) files. It can also
   convert Microsoft VHD, VMWare VMDK, Parallels HDD (v2), raw disk or partition image files,
   and even physical Windows drives - into a VDI file. Note that SlimVDI is not a GUI for VBoxManage,
   it is entirely stand-alone, using my own code for reading and writing VDI, and for reading the
   other supported formats.

### What's the point? - VirtualBox has cloning built in as standard!
   In 2009, VirtualBox only supported cloning from the command
   line, which many users disliked. I also wanted to add a number of features that VirtualBox
   did not provide. And, I wanted the flexibility to add more features in the future, independant
   of what the VBox developers choose to do.
   In particular :-
   * I wanted a simple GUI interface (I later added a command line interface as well, to satisfy
     those strange people who like that sort of thing!).
   * I wanted better feedback from the tool, in particular an accurate answer to the question,
     "so how long is this going to take?".
   * Speed. I wanted to see if cloning could be any faster, though I have not really made a
     serious effort in that direction yet. However, in a test I performed, the "VBoxManage
     clonehd" command took 101s to clone a 1.6GB VDI. My SlimVDI tool did the same in 68
     seconds. But, I find that Windows disk I/O speeds are highly variable, so that could have
     been dumb luck.
   * I wanted more flexibility regarding the UUID (a signature embedded in VDI and most other
     virtual disk formats, which VBox uses to identify them). By default I generate a new UUID for
     the clone (as "VBoxManage clonehd" always does), but I also have an option to keep the old
     UUID. USE THIS FEATURE WITH CARE - ALWAYS GENERATE A NEW UUID UNLESS YOU KNOW WHEN/WHY THE
     OLD UUID CAN/SHOULD BE RETAINED.
   * I wanted more flexibility regarding the drive size. SlimVDI has an option to increase
     the virtual drive size of the clone, without disturbing the existing partitions. There is
     a separate option to expand the main partition too (for certain filesystems only) to occupy
     the increased space on the drive.
     VirtualBox added a limited disk "resize" feature to VBoxManage v4.0.0, however I feel
     that SlimVDI's version is still preferable for many reasons: the short form of which is
     that SlimVDI is more capable and yet easier to use.
   * I wanted to try optimization of the clone on the fly, i.e. detect blocks which are no
     longer used by the guest filesystem, and discard those from the clone. This feature requires
     me to interpret the contents of the drive, which entails more risk of an error and is therefore
     optional (see the "Compact while copying" checkbox). The easier and entirely safe optimizations
     already performed by "VBoxManage clonehd", such as defragmenting the block order and skipping
	 zero blocks is of course also done by SlimVDI, regardless of the setting of the compact option.
   * I wanted to try other optimizations, such as adjusting the target VDI header size to ensure that
     guest filesystem clusters are aligned on host cluster boundaries, for faster disk I/O.
   * People sometimes have problems getting VBoxManage to perform a particular cloning operation.
     When that happens it is useful to have an independant tool you can try instead.
     
   VirtualBox finally added GUI support for cloning (both VMs and bare disks) in v4.1.0.  This
   represented a brilliant step forward, and for most everyday uses the integrated VirtualBox feature
   will be now much more convenient to use than the SlimVDI original. However, SlimVDI continues to
   be a useful tool for cloning disks in unusual circumstances, e.g. when the VBox media registry 
   refuses to accept a disk, or it has a broken snapshot chain, or when the user wants to preserve
   the UUID, or when the user wants to perform some other operation at the same time, such as compaction
   or disk enlargement. And it continues to serve as a "second source" for cloning and backup making.
	 	  
### Why clone at all? - Why not use Snapshots instead?
   By all means use snapshots if you like them. But, snapshots worry me. I worry that they are
   inherently fragile: a series of links in a chain, and if any link in the chain is broken then
   the chain itself may be rendered useless. By contrast a clone is entirely independant of the
   original on which it was based. Damaging the original will not affect the clone at all (or vice
   versa). Also, it's easier to move VDIs to a different host, or to a backup drive, if they don't
   have snapshots. Finally, it's easier to switch back and forth between numerous variants of a VM
   if they are done as clones.

### Will it corrupt my virtual disks?
   SlimVDI creates a copy of the original file, so you can always revert back to the original provided you haven't deleted
   it. If you do intend to delete the original then I suggest that you use the clone for a while
   first, to make sure that everything is stable.

### Can it clone VDIs with snapshots?
   Yes, SlimVDI has the ability to clone a snapshot VDI (the .VDI files with the strange
   names that live in your \<VM>\Snapshots folder). It will follow the chain of dependencies back
   to the base VDI and then create a clone which includes all the data from the point in time
   represented by the snapshot.  However this feature only works with VirtualBox native (VDI)
   snapshots. Support for cloning snapshots in other formats than VMDK and VHD are not supported.

### What is this UUID thing that the dialog box mentions?
   Simply put, the header of every VDI file includes an identifying number, called a UUID, which
   VirtualBox uses to identify VDI files and tell them apart. VirtualBox requires every registered
   VDI to have a unique UUID, and new VirtualBox users often encounter the problem that if they
   simply use the host operating system file copy function to copy a VDI then VirtualBox refuses
   to let you register the copy, because of conflicting UUID signatures. VHD files also have UUIDs,
   VMDK files usually (but not always) have them too. RAW files obviously don't have UUIDs, nor do
   Parallels .HDD images. If the input format doesn't have a UUID then SlimVDI has no alternative
   but to generate one, and the "keep UUID" option would be ignored.

Using SlimVDI
-------------
Just run the executable and a simple dialog box is displayed. The fields on this dialog
box are discussed below.

### Source and Destination filenames
Source filename: enter the full path and filename of the original VDI, VHD, VMDK, HDD or RAW
file. There is a Browse button beside this field to help you find the right file. Once you
have selected an existing file then several other fields on the dialog will be enabled.

Dest filename: the full path and filename you would like for the clone. Once you select
a source file <filename> then this field defaults to "Clone of <filename>". The extension
is forced to "VDI", since SlimVDI currently cannot write other formats.

NOTE: it is perfectly ok for the source and destination path\filenames to be the same. In
that case the tool will create a backup of the original file.
If you do this then you probably also want to use the option to keep the old UUID,
that way VirtualBox Media Manager should not notice the change. [ Obviously this does not apply
when converting VHD,VMDK or RAW, since the output filename will always have a VDI extension,
hence the filenames can never match ].

SlimVDI remembers the contents of the source and dest fields between sessions.

### Options
Select **Generate new UUID** if you intend to use both original and clone on the same host PC.
Selecting **Keep old UUID** is a possibility if you plan to delete the original file or use the clone
on a different PC. There are many other circumstances in which you may prefer one or other of these
options. If in doubt then **Generate new** is probably safest (that option is compatible with VBox
**clonehd** behaviour). Note that if you use the **Keep** option then only the creation UUID is inherited
by the clone. The "modification" UUID is given a new value. If you were unwise enough to clone and
replace the base VDI for a snapshot chain then this should ensure that the error is not catastrophic
(VBox should complain - correctly - that the base VDI has been modified, at which time you should
restore the original VDI).

Select the **Increase virtual drive size** option if you want to (er..) increase the virtual drive
maximum size. If this option is selected then you can enter a new drive size in the edit box beside
it. The new size must be at least as large as the old size.

The **Increase partition size** option is only available when the **Increase virtual drive size** option
is selected. If the **Increase partition size** option is selected then SlimVDI will automatically
increase the size of the main partition to fill the new drive size. This feature only works at
present on FAT16, FAT32 and NTFS partitions, however it has side effects which may be useful to
you even with other guest filesystems (see the notes on increasing partition size later on).

The **Increase partition size** feature works in a simplistic way, and doesn't work at all for some
filesystems. An alternative is to let SlimVDI enlarge the drive, then run a third party tool inside
the VM to adjust the partition size as required. I know that the free  [gparted live CD](http://sourceforge.net/projects/gparted/files/gparted-live-stable) works
well for this. Download
the latest ISO and mount it in your guest as a virtual CD, then boot up the VM, after ensuring that
the CD/DVD drive comes first in the boot order.

The **Compact drive while copying** checkbox enables a feature whereby the tool identifies [unused
blocks](#remarks-about-unused-blocks) in the guest filesystem, and omits those from the clone. This would be most useful if a
large amount of file data has been deleted inside the guest. This saves you from having to run SDelete
or its Linux equivalent before making the clone, to get the same result. This feature only does
something if SlimVDI recognizes the guest filesystem, and at present the only supported filesystems
are NTFS (tested with NT4 and later), EXT2/3/4, FAT16 and FAT32. Also, I can only recognize the
filesystem inside a partition if the disk uses a traditional MBR partition map. I also have experimental
(not heavily tested) support for Windows Dynamic Disk. I don't yet support Linux LVM or GPT partitioning
schemes.

#### Remarks about unused blocks
All the clusters that fall inside a VDI block must be unused in order for the block to be
considered unused. VDI blocks are usually 1MB, which typically covers 256x4K guest clusters. You
might get better results by defragmenting inside your guest VM first.

### Buttons
- **Partition Info** shows the partition map from the MBR of the source VDI.
- **Header Info** shows information taken from the VDI, VHD or VMDK header. RAW files obviously have no
header to show, but SlimVDI internally treats a RAW disk image as a VMDK with a single flat extent,
so what you will see is the synthesized VMDK header.
- **Sector viewer** lets you view source disk sectors in hex format, and/or dump sectors to a file.
- **Proceed** tells the tool to begin the cloning operation.
- **Exit** causes the application to close.


Resizing Partitions
-------------------
As mentioned above, if you select the "Enlarge disk" feature then you also have the option to
select the "Increase partition size" feature. These two features in combination will :-

    1. Enlarge the drive to the selected size.
    2. Identify the main partition on the source drive.
    3. "Slide" any partitions which follow the main partition up to the end of the drive.
    4. Increase the size of the main partition filesystem to use the space made available
       after step 3. (Step 4 only happens for FAT16, FAT32 and NTFS partitions).

Notice that in step 2 I say "main partition" rather than "boot partition". With the increasing
use of boot managers the fact that a particular partition is marked as bootable is no longer a
reliable indicator that this is the main partition. Hence, what I do instead is assume that the
largest partition on a drive is the main partition, and that is the partition I enlarge. Although
I expect this assumption to be true in most cases there will of course be times when it is not,
in which case you will need to use a third party partition manager.

Although complete functionality (including step 4) is only available for FAT and NTFS
partitions, steps 1-3 may be useful even for other filesystems. For example a Unix hard
disk consisting of a main partition followed by a swap partition will have the swap partition
moved to the end of the enlarged drive, making it easier for a third party partitioning tool
to enlarge the main partition.

Sorry Linux users, but ExtX partitions cannot be expanded using this feature. I did originally
intend that "Increase partition size" would work for ExtX partitions too, but it turned out to
be harder to do than I expected, much harder than for the other supported filesystems. It's
possible that I simply don't "get" ExtX well enough to see how it could be done easily.


Cloning Snapshots
-----------------

In this section I will use the term "snapshot" somewhat loosely (and inaccurately) to refer to
any differencing VDI file, i.e. an incomplete VDI file which requires the presence of at least
one parent VDI to get a complete disk image. What users generally think of as snapshot files
are the ones with strange names such as "{1234.5678.abcd.etc...}.vdi" and which generally live
in a "Snapshots" folder which contains all other snapshots created for the same VM. One file
missing from this folder is the base VDI, which always acts as the first link in a snapshot
chain, and is needed to complete the virtual disk image. In VirtualBox v4 the base VDI is
stored by default in the parent folder of the Snapshots folder. In previous VirtualBox versions
the base VDI was stored in your "<userdoc>\.VirtualBox\HardDisks" folder.
 
If you select a snapshot (child) VDI file as the source then SlimVDI will scan the remaining VDI
files in the same folder to see where they fit in the snapshot chain. Also, if the folder
happens to be called "Snapshots" then the parent folder will also be scanned, and any VDI files
found there will receive similar treatment.

Note that no xml media registry need exist. VirtualBox.xml is not checked, nor is the VM.xml
or VM.vbox file. The "SlimVDI_Media.xml" feature (supported by previous SlimVDI versions) has
been discarded.

This new snapshot chain reconstruction method works well with the new VirtualBox v4 folder
structure, and it also provides the simplest possible method of creating a flat VDI file from a
collection of snapshot files: just make sure to put them all in one folder, including the base
VDI, and SlimVDI should have no problem creating a clone of any one of them.

If you still receive an error message that "differencing images are not supported", then it
means that a valid VDI snapshot chain couldn't be created from the files you provided. Bear in
mind also that only VDI snapshots are supported.


Using SlimVDI to do a P2V ("Physical To Virtual" Disk Conversion)
------------------------------------------------------------------
SlimVDI recognizes when you have used a Windows disk device name as the source filename
(for example, "\\.\PhysicalDrive0" identifies the boot drive on most Windows hosts), and
slightly modifies its behaviour in that case to make it possible to clone the physical host
hard disk to a VDI file. Basically, all it does is use a different method to get the "source
file size", otherwise everything works as before. Be warned that this feature is extremely
rudimentary, so it is up to you to use it sensibly. In particular you should have no other
applications running (certainly nothing that creates and/or writes to files), and the clone
should be written to a different drive.  On some Windows hosts you may also need to run
SlimVDI with elevated privileges ("Run as administrator") since this feature requires
SlimVDI to have sector level (read only) access to the source drive.

## Notes on image file formats

### VDI
As far as I know, I support all variants of the basic VDI file format, from the oldest Innotek
format to the latest Oracle format. When writing I write the latest VDI format, which most recent
VirtualBox versions should support. SlimVDI always writes a dynamic VDI.

### VHD
VHD is supported for input only. I support fixed and dynamic VHDs using any block size. Differencing
VHDs are not supported. Split VHD files are not supported: if you have a split VHD then you should
download one of the many available free file split/join tools and join the segments into one file
before processing it with SlimVDI. If you are comfortable with the Windows command prompt then the
"copy" command can also be used to join files (do some web research first, you need to get this VERY
right!).

### VMDK
I support monolithic and split VMDK files having FLAT, ZERO or SPARSE extent types. Due to lack of
clear documentation I do not yet support the strange extent types you may find on an ESX server. I
do not support compressed VMDK files.  I do not support VMDK snapshot files.

### Parallels HDD
I could find no open specification of the current Parallels .HDD format, the only information I found
was for the Parallels v2 format as implemented in qemu-img (I did not use any QEMU code, I simply used
them as a source of file structure information). On the upside, all of the Parallel's appliances
I've found online do seem to use the V2 format.

### RAW
A RAW file is a simple uncompressed image of a disk or partition, the file must have a .raw or .img
extension, otherwise (since it has no header) SlimVDI has no other way to reliably identify the file
as a raw image.  A whole disk image is best, but if you give SlimVDI a partition image then it will
try to identify the partition contents and then reconstruct a full disk image around it (adding a new
MBR and track0 at the front, and adding padding at the back to create a rounded drive size). One must
bear in mind however that SlimVDI is trying to reconstruct important missing information and there
is NO guarantee that the result will work. Also I must apologise to Linux fans, since this ability to
convert a partition image will not work at all with a Linux partition image, because every working
Linux disk I've seen has multiple partitions and puts a lot of extra data in track0 (grub boot stuff) -
all of which makes it impractical to reconstruct a whole disk starting with an image of just one of the
necessary partitions. Put another way, it MAY be possible, but I don't know enough about Linux to do
it yet.


### Additional notes about Converting VHD/VMDK/HDD/RAW to VDI
Be warned: all that SlimVDI does when you give it a VHD/VMDK/HDD/RAW file is convert one media type to
another media type. There is no guarantee that you'll be able to use the media /content/ without
problems inside VirtualBox. For example, let's say you convert an XP.VHD that previously ran under
Virtual PC: the same install of XP may not run under VirtualBox right away, due to the different hardware
simulations on the two platforms. Even if XP does run it is very likely you will be prompted to reactivate.
Activation on the new platform may give you "Windows Update" problems if you continue to use the
same XP license on the old platform. These same issues would arise if you mounted the source file
under VirtualBox without converting.


## Additional notes on the "Sector Viewer" feature
When developing SlimVDI I found myself often wanting a convenient way to view guest disk sectors,
and when testing support for RAW files I also found myself in need of a reliable and friendly tool
for creating raw files by dumping disk sectors from a VDI (usually I needed entire disks or partitions
dumped to a flat file, and you may (or may not!) be amazed at how unfriendly some of the freely
available tools are!). I had had a "Sector editor" feature in mind since day one (the button was always
there, but grayed out), so I finally got around to implementing it in v1.40; though I decided to make it
a sector viewer rather than a sector editor.  Non-technical users can safely ignore this feature
entirely, for you it has nothing more than curiosity value.  Technical users may find it an interesting
tool for analysing guest filesystems. The ability to dump sectors to a file may also be useful, for
example this can be used to export an entire disk or partition to a flat file.


Notes on the Command Line Interface
-----------------------------------
Three run modes are supported:

   * Normal GUI mode, which is when SlimVDI is run from the Windows desktop without arguments.
   * GUI mode with command line, when SlimVDI is run from the Windows desktop with arguments.
   * CLI mode, which is when SlimVDI is run from a command console with arguments.
   
For completeness, I should note that if you run SlimVDI from the command console /without/ arguments
then it runs in normal GUI mode. The CLI mode usage information is as follows :-

```
	Usage: SlimVDI <source file name> {options}
	
	Option Name       Effect
	-----------       ------
	-o or --output    Sets output filename. A filename should
					  follow as next argument. Dest filename
					  defaults to \"Clone of \"+<source> if this
					  option is not selected.
	-k or --keepuuid  Dest file inherits UUID from source.
					  Default is to generate a new UUID.
	-e or --enlarge   Enlarges drive max size. Intended size
					  should be provided as next argument,
					  e.g. 256.00MB, 64.00GB.
    -r or --repart    Enlarge the main partition (ignored if
                      --enlarge option is not also set).
	-c or --compact   Enables compaction feature (supported
					  guest filesystems only).
	-h or --help      Displays this usage information.
	
	Options can be grouped, eg. -kce or --keepuuid+enlarge. Option
	parameters should follow, and the order of the option parameters should
	match the order of the options within the option group.
	
Here are some example command lines, which all do the same thing :-

    SlimVDI "My Virtual Disk.vdi" -o "My Clone.vdi" -k -e 64.00GB -c
    SlimVDI "My Virtual Disk.vdi" -o "My Clone.vdi" -k -e "64.00 GB" -c
    SlimVDI "My Virtual Disk.vdi" --output "My Clone.vdi" --keeupuuid --enlarge 64.00GB --compact
    SlimVDI "My Virtual Disk.vdi" -okec "My Clone.vdi" 64.00GB
    SlimVDI "My Virtual Disk.vdi" --keepuuid+compact+output+enlarge "My Clone.vdi" "64.00 GB"
```

You should bear in mind that the Command Line Interface (CLI) feature is just a different way to
access the features of SlimVDI, the features themselves still work in the same way. For example,
it is legal to make source and destination filenames exactly the same, this would be handled
exactly as if you had filled the dialog box in the same way, i.e. you will end up with a clone
with the selected name, and the original file renamed to "Original of "+<filename>.
	
SlimVDI is an example of a "dual mode" console/GUI application. This presents a small problem when
running from the Windows command console: the console expects an application to be /either/ a GUI
application, or a console application, and not both. A field in the EXE header says which of these
types a particular application is. In the GUI case the console launches the application as an
independant process and does not wait for completion. In the CLI case the console does wait for
completion of the command.

SlimVDI is an application which supports a CLI but is marked as a GUI app, hence the command console
will normally launch it as an independant process and not wait for completion. In most cases this is
no big deal: SlimVDI will do what you asked it to (without a start dialog) and will terminate itself
when finished. However any text output by SlimVDI will overwrite the next command prompt, which is not
very elegant (you have to press enter once SlimVDI is done, to make CMD repeat the command prompt).

If you don't like the above behaviour then the Windows command console supports a special run command
"start /wait <appname>" which forces it to wait until <appname> terminates. So, just prepend the example
commands given above with "start /wait " and everything should work ok. Alternatively, running SlimVDI
inside a batch file also seems to make the system wait for completion (I've provided an example batch
file - see CloneCLI.bat - which you may want to use). I can't speak for how other scripting languages
will handle the the situation. I guess you'll let me know.

There is one small quirk that you should probably be aware of, which is that SlimVDI treats the
source folder as the target unless told otherwise. For example if you type the command like this:-

    SlimVDI "\Source Folder\Source Name.vdi" --output "Dest Name.vdi"
	
Then SlimVDI creates "Dest Name.vdi" in the source folder, /not/ the current folder (if different). To
change this you need to explicitly give the required target folder in the output filename.

Incidentally, script writers may wish to know that SlimVDI returns an error code of 0 to the shell if
all goes well, and a non-zero result code if there was an error. If run from the command console then
there should also be an error message in the latter case.


Notes on Language Support (Localization)
----------------------------------------
After careful thought I decided that SlimVDI would not adjust itself automatically to a locale, since
I was not convinced that the preferred language need always be the same as the locale. So, if you want
SlimVDI to use a different language then you need to add a "Language=n" line to the [Options] section
of the "slimvdi.ini" file (or modify the option if it is already there). 'n' is a number indicating
the preferred language. Currently supported numbers are 0=English, 1=Dutch, 2=German, 3=French. English
is the default if the option is not present or the language number not recognized. Incidentally, there
is no significance in the numbering, I simply allocate new numbers in the order that translations are
sent to me.


Other .INI File Features
------------------------
The .INI file is used to store source and destination filenames from the last session, and also includes
the Language=n option described above. A new addition is an optional line "Compact=n", which
sets the startup default state of the "Compact" switch in the dialog. n=0(off) is the default if this
.INI line is not present; "Compact=1" (or any other non-zero value) causes the compact option to be
enabled on startup. This .INI file setting has no effect on CLI mode (compact is always disabled by
default for CLI mode).

Other Contributers
------------------
My thanks to the following people :-

* Sasquatch @ forums.virtualbox.org for providing the Dutch translation of the SlimVDI resource
  file.
* Erich N. Weitzeil (a.k.a. Etepetete @ forums.virtualbox.org) for the German translation of the
  SlimVDI resource file.
* Gregory (a.k.a. Gregoo @ forums.virtualbox.org) for the French translation of the SlimVDI
  resource file.
* MarkCranness @ forums.virtualbox.org for contributing a better looking application icon.
* Makoto Matsumoto and Takuji Nishimura for making available an open source permissive licensed
  C implementation of their "Mersenne Twister" pseudo-random number generator algorithm. SlimVDI
  itself makes very minor use of this, but I have found the code useful in several applications
  (in fact this is now my standard PRNG module).
* Chris Waters (a.k.a. cgwaters @ forums.virtualbox.org) for going to a lot of trouble to provide
  me with enough information to reproduce and fix a bug in the NTFS partition enlargement code.
  
Third Party Copyrights, Licenses, Patents et al...
--------------------------------------------------
Just one of these that I know of...

The module "Random.c" implements the Mersenne Twister psuedo random number generator. The code is
a slightly modified version of an original file "MT19937.c" which I found on the web a few years
ago. The authors original copyright applies to Random.c only and can be found in the source file.
