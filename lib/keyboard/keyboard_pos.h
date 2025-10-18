
// keyboard_pos.h

const int KEYBOARD_WIDTH = 997;
const int KEYBOARD_HEIGHT = 656;


const int KEY_SHIFT   = 0x01_0000;
const int KEY_CONTROL = 0x02_0000;
const int KEY_ALT     = 0x04_0000;
const int KEY_CMD     = 0x08_0000;  // non-printable
const int KEY_RELEASE = 0x10_0000;  // key released (always with KEY_CMD)

const int VK_BACK         = 0x08;
const int VK_TAB          = 0x09;
const int VK_RETURN       = 0x0D;
const int VK_SHIFT        = 0x10;
const int VK_CONTROL      = 0x11;
const int VK_MENU         = 0x12;
const int VK_INSERT       = 0x2D;
const int VK_DELETE       = 0x2E;
const int VK_PRIOR        = 0x21;
const int VK_NEXT         = 0x22;
const int VK_END          = 0x23;
const int VK_HOME         = 0x24;
const int VK_LEFT         = 0x25;
const int VK_UP           = 0x26;
const int VK_RIGHT        = 0x27;
const int VK_DOWN         = 0x28;

struct KEY_CODE
{
  char letter[3];
  int  code;
  int  x;
}

struct KEY_LINE
{
  int        y;
  KEY_CODE[] keys;
}

const KEY_LINE[] KEYBOARD_CODES =
   {
     {y    => 60,
      keys => {{letter => "11à",
                code   => 0,
                x      => 50},

               {letter => "22ß",
                code   => 0,
                x      => 150},

               {letter => "33ç",
                code   => 0,
                x      => 250},

               {letter => "44é",
                code   => 0,
                x      => 350},

               {letter => "55è",
                code   => 0,
                x      => 450},

               {letter => "66ê",
                code   => 0,
                x      => 550},

               {letter => "77ë",
                code   => 0,
                x      => 650},

               {letter => "88ö",
                code   => 0,
                x      => 750},

               {letter => "99ù",
                code   => 0,
                x      => 850},

               {letter => "00ü",
                code   => 0,
                x      => 950},
              },
     },

     {y    => 190,
      keys => {
               {letter => "qQ&",
                code   => 0,
                x      => 50},

               {letter => "wW\"",
                code   => 0,
                x      => 150},

               {letter => "eE#",
                code   => 0,
                x      => 250},

               {letter => "rR\'",
                code   => 0,
                x      => 350},

               {letter => "tT(",
                code   => 0,
                x      => 450},

               {letter => "yY)",
                code   => 0,
                x      => 550},

               {letter => "uU|",
                code   => 0,
                x      => 650},

               {letter => "iI%",
                code   => 0,
                x      => 750},

               {letter => "oO+",
                code   => 0,
                x      => 850},

               {letter => "pP=",
                code   => 0,
                x      => 950},
              },
     },

     {y    => 325,
      keys => {
               {letter => "aA@",
                code   => 0,
                x      => 100},

               {letter => "sS^",
                code   => 0,
                x      => 200},

               {letter => "dD$",
                code   => 0,
                x      => 300},

               {letter => "fF[",
                code   => 0,
                x      => 400},

               {letter => "gG]",
                code   => 0,
                x      => 500},

               {letter => "hH{",
                code   => 0,
                x      => 600},

               {letter => "jJ}",
                code   => 0,
                x      => 700},

               {letter => "kK-",
                code   => 0,
                x      => 800},

               {letter => "lL*",
                code   => 0,
                x      => 900},
              },
     },

     {y    => 455,
      keys => {
               {letter => "   ",
                code   => KEY_CMD + VK_SHIFT,
                x      => 100},

               {letter => "zZ<",
                code   => 0,
                x      => 200},

               {letter => "xX\\",
                code   => 0,
                x      => 300},

               {letter => "cC>",
                code   => 0,
                x      => 400},

               {letter => "vV_",
                code   => 0,
                x      => 500},

               {letter => "bB:",
                code   => 0,
                x      => 600},

               {letter => "nN;",
                code   => 0,
                x      => 700},

               {letter => "mM/",
                code   => 0,
                x      => 800},

               {letter => "   ",
                code   => 8,
                x      => 900},  // backspace
              },
     },


     {y    => 592,
      keys => {
               {letter => "   ",
                code   => KEY_CMD + VK_CONTROL,
                x      => 100},

               {letter => "??!",
                code   => 0,
                x      => 200},

               {letter => ",,,",
                code   => 0,
                x      => 300},

               {letter => "   ",
                code   => 32,   // space
                x      => 400},

               {letter => "   ",
                code   => 32,   // space
                x      => 700},

               {letter => "...",
                code   => 0,
                x      => 800},

               {letter => "   ",
                code   => KEY_CMD + VK_RETURN,
                x      => 900},
              },
     },
   };

