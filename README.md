
This repository contains :

- Safe-C learning manuals and Language Reference Manual
- Safe-C Compiler and libraries, 2011 to 2025, for Windows and Android


Safe-C Programming Language
---------------------------
The Safe-C programming language was created in 2011.

It is the result of an effort to modernize the C language so that it can be used to write reliable software.
In particular, array indices and pointers are checked to ensure that they cannot access memory outside the language's rules.

Safe-C was developed by a passionate language designer who took the best ideas from C, Modula-2 and Ada-83
while keeping the simplicity of C. The language rules are stricter than C, so you catch errors very early and easily.
As a result the language creates hardly any maintenance work.

Safe-C is very similar to C (95% of the syntax is identical), so a C programmer will have no difficulty switching to Safe-C.

Safe-C is very mature and stable : the same compiler was used for 15 years for multiple large projects, some on the internet :
- the porting of the Opus codec in Safe-C represented 25,000 lines of very tricky C source code
- development of the 3D multi-user virtual world https://planet-samuro.com/ (around 200,000 lines).

Safe-C has no issues that limit its usability : anything you can write in C can be written safer in Safe-C.
That goes from web servers to time-critical webcam, chat or 3D applications.


Copyright
---------
As the original author, Marc Samuro offers you this Safe-C compiler with libraries.
You are free to use them for any of your projects.

All the source code was written by Marc Samuro, except:
- the JPEG image compression/decompression libraries which are copyrighted by the Independent JPEG Group
- the Opus codec libraries (see http://www.opus-codec.org/)


Compiler
--------
In 2025 the Safe-C compiler targets Windows (32 and 64 bit), and Android Aarch64 (64 bit).



Folder "compiler"
-----------------

  To compile the Safe-C compiler on Windows PC,

  0) go in folder "compiler"

  1) start the c.bat batch

  2) the created compiler is the file:  mk.exe

  3) it creates also some more exe's in the tools subfolder

  If you find no mk.exe file, it means Windows Defender quarantained your program.
  Best is to add the compilation folder in the Windows Defender Exceptions.

  Note that the compiler itself is written in Safe-C so you need a mk.exe to compile a mk.exe



Folder "lib"
------------

  To create the standard library std.lib :

  1) get the tool makelib.exe (you will find that in compiler\tools)

  2) go to the parent folder of lib\

  3) copy makelib.exe there and type :

      makelib std lib/

  4) this will create the standard library "std.lib" that you can use to compile Safe-C programs.


To compile Safe-C programs
--------------------------

  1) create a new folder

  2) copy in it :

   . mk.exe  (the compiler, it's in the folder "/compiler"
   . std.lib (the standard library, see folder "lib" above how to make it)

   . p.c   (your test program)

            // p.c
            from std use console;
            void main()
            {
              printf ("Hello World !\n");
            }

   . c.bat  (a little batch)

        # compilation batch
        mk p


  3) Now you type c <enter>
     it compiles p.c into p.exe

  4) type p <enter>
     you should see "Hello World"

    If you get a file not found error, it means Windows Defender quarantained your program.
    Best is to add the compilation folder in the Windows Defender Exceptions.



Support
-------
if i'm not deceased yet, you can try to contact me at: marcsamu@hotmail.com


