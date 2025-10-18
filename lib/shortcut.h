
// shortcut.h

//*************************************************************************

/* 'folder_name' can have the following values :

 - for single users :
   "AppData",
   "Desktop",
   "Favorites",
   "Personal",
   "Programs",
   "Recent",
   "SendTo",
   "Start Menu",
   "Startup", 
 
 - for all users :
   "Common Desktop",
   "Common Documents",
   "Common Programs",
   "Common Start Menu",
   "Common Startup",
   "ProgramFilesDir",        (folder where programs should be installed)
   "ProgramFilesDir (x86)",  (folder where 32-bit programs should be installed on a 64-bit machine)
*/

int get_system_directory (    string folder_name,
                          out char   directory[260]);

/*
AppData -> C:\Users\Samuro\AppData\Roaming
Desktop -> C:\Users\Samuro\Desktop
Favorites -> C:\Users\Samuro\Favorites
Personal -> C:\Users\Samuro\Documents
Programs -> C:\Users\Samuro\AppData\Roaming\Microsoft\Windows\Start Menu\Programs
Recent -> C:\Users\Samuro\AppData\Roaming\Microsoft\Windows\Recent
SendTo -> C:\Users\Samuro\AppData\Roaming\Microsoft\Windows\SendTo
Start Menu -> C:\Users\Samuro\AppData\Roaming\Microsoft\Windows\Start Menu
Startup -> C:\Users\Samuro\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Startup
Common Desktop -> C:\Users\Public\Desktop
Common Documents -> C:\Users\Public\Documents
Common Programs -> C:\ProgramData\Microsoft\Windows\Start Menu\Programs
Common Start Menu -> C:\ProgramData\Microsoft\Windows\Start Menu
Common Startup -> C:\ProgramData\Microsoft\Windows\Start Menu\Programs\Startup
ProgramFilesDir -> C:\Program Files
ProgramFilesDir (x86) -> C:\Program Files (x86)
*/

//*************************************************************************

// shortcut : shortcut name within directory computed by get_system_directory(),
//            must have extension .LNK

int create_shortcut (string shortcut,
                     string target_directory);

//*************************************************************************
