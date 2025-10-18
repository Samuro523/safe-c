
// guiandroidkeyboard.c

use ../arithm, ../gui, ../image, ../strings, ../keyboard/keyboard, ../keyboard/keyboard_pos;
use guiandroid;

//------------------------------------------------------------------------------------------

bool g_keyboad_enabled;
bool g_position_initialized;
int  g_x, g_y, g_x_size, g_y_size;
int  g_current_set = -1;
int  g_bm_x_size[3], g_bm_y_size[3];

const int KEYBOARD_TITLE_HEIGHT = 50;

IMAGE_INFO g_img[3];

//------------------------------------------------------------------------------------------

void prepare_keyboard_bitmap (int set)
{
  int x_size, y_size, rc;

  get_size (out x_size, out y_size, id => 100);
  x_size = ui_scale(x_size);
  y_size = ui_scale(y_size);

  if (g_bm_x_size[set] == x_size && g_bm_y_size[set] == y_size)
    return;
    
  g_bm_x_size[set] = x_size;
  g_bm_y_size[set] = y_size;
  

  free_image (ref g_img[set]);

  rc = decompress_image (    compressed_image => keyboard.KEYBOARD_JPG[set],
                         out info             => g_img[set]);
  assert rc == 0;

  rc = resize_image (ref info              => g_img[set],
                         width             => (uint)x_size,
                         height            => (uint)y_size,
                         border_color      => 0xFF000000,  // opaque black
                         bestfit           => false,
                         use_linear_colors => true);
  assert rc == 0;
}

//------------------------------------------------------------------------------------------

void display_keyboard_bitmap (int set)
{
  prepare_keyboard_bitmap (set);

  window_raster (x         => 0,
                 y         => 0,
                 size_x    => g_bm_x_size[set],
                 size_y    => g_bm_y_size[set],
                 image     => g_img[set].pixel^,
                 id        => 100,
                 immediate => false,
                 scaled    => false);
}

//------------------------------------------------------------------------------------------

void find_key (float u, float v)
{
  // position on image
  int x = (int)(u * (float)KEYBOARD_WIDTH);
  int y = (int)(v * (float)KEYBOARD_HEIGHT);
  int near, i, j, best_i, best_j;

  near = 99999;
  best_j = -1;
  for (j=0; j<KEYBOARD_CODES'length; j++)
  {
    int dist = abs (KEYBOARD_CODES[j].y - y);
    if (dist < near)
    {
      near = dist;
      best_j = j;
    }
  }

  near = 99999;
  best_i = -1;
  for (i=0; i<KEYBOARD_CODES[best_j].keys'length; i++)
  {
    int dist = abs (KEYBOARD_CODES[best_j].keys[i].x - x);
    if (dist < near)
    {
      near = dist;
      best_i = i;
    }
  }

  {
    ref KEY_CODE kc = KEYBOARD_CODES[best_j].keys[best_i];
    int          key;

    if (kc.code == 0)
      key = (int)kc.letter[g_current_set];
    else
      key = kc.code;

    if (key == keyboard_pos.KEY_CMD + VK_CONTROL)
    {
      if (g_current_set != 2)
        g_current_set = 2;
      else
        g_current_set = 0;

      display_keyboard_bitmap (set => g_current_set);
    }
    else if (key == keyboard_pos.KEY_CMD + VK_SHIFT)
    {
      if (g_current_set != 1)
        g_current_set = 1;
      else
        g_current_set = 0;

      display_keyboard_bitmap (set => g_current_set);
    }
    else
    {
      EVENT ev;
      int   d = -10;
      clear ev;
      ev.d = (uint)d;
      ev.key = key;

      guiandroid.post_event (ev);
    }
  }
}

//------------------------------------------------------------------------------------------

void keyboard_handler (EVENT e)
{
  switch (e.type)
  {
    case EVENT_NEW_DIALOG:
      {
        if (!g_position_initialized)
        {
          int intern_x_size, intern_y_size;

          get_main_window_intern_size (out intern_x_size, out intern_y_size);
         
          if (intern_x_size < intern_y_size)   // portrait
          {
            g_x = 0;
            g_y = intern_y_size * 2 / 3;   // 1/3 of lower screen
          }
          else  // landscape
          {
            g_x = intern_x_size * 40 / 100;   // 60% of screen
            g_y = intern_y_size * 40 / 100;   // 60% of screen
          }

          g_x_size = intern_x_size - g_x;
          g_y_size = intern_y_size - g_y;

          g_position_initialized = true;
        }

        
        set_dialog_position (g_x, g_y);
        set_dialog_size (g_x_size, g_y_size);

        {
          FONT f;
          clear f;
          strcpy (out f.name, "Tahoma");
          f.height = KEYBOARD_TITLE_HEIGHT * 3 / 4;
          set_dialog_title_font (font => f);
          set_dialog_title_height (KEYBOARD_TITLE_HEIGHT);
          set_dialog_default_font (f);
        }

        gui.set_dialog_title (" QWERTY Keyboard");
        set_dialog_border_visual_size (0);    // no border
        set_dialog_border_drag_size (4);

        set_dialog_close_button (true);
        set_dialog_transparency (focus => 255, non_focus => 255);
        redirect_keyboard_to_main_window (redirect => true);

        create_window (x => 0, y => 0, x_size => g_x_size, y_size => g_y_size-KEYBOARD_TITLE_HEIGHT, id => 100);

        g_current_set = 0;
        display_keyboard_bitmap (set => g_current_set);
      }
      break;

    case EVENT_MOVE:
    {
      g_x = e.x;
      g_y = e.y;
      set_dialog_position (e.x, e.y);
    }
    break;

    case EVENT_RESIZE:
    {
      g_x_size = e.x_size;
      g_y_size = e.y_size;

      set_dialog_size (e.x_size, e.y_size);
      set_size (x_size => e.x_size,
                y_size => e.y_size-KEYBOARD_TITLE_HEIGHT,
                id     => 100);

      display_keyboard_bitmap (set => g_current_set);
    }
    break;

    case EVENT_WINDOW_CLICKED:          // user clicked in window draw area
      find_key ((float)e.x / (float)g_x_size, (float)e.y / (float)(g_y_size-KEYBOARD_TITLE_HEIGHT));
      break;

    case EVENT_WINDOW_DROP:             // user click released
      // e.x, e.y
      break;

    case EVENT_CLOSE_DIALOG:            // last event before closing dialog.
      g_keyboad_enabled = false;
      break;

    default:
      break;
  }
}

//------------------------------------------------------------------------------------------

// called by gui thread
public void show_keyboard ()
{
  if (g_keyboad_enabled)
    return;

  guiandroid.create_dialog (d => 1_500_000_000, keyboard_handler);

  g_keyboad_enabled = true;
}

//------------------------------------------------------------------------------------------

// called by gui thread
public void hide_keyboard ()
{
  if (!g_keyboad_enabled)
    return;

  guiandroid.close_dialog (d => 1_500_000_000);

  g_keyboad_enabled = false;
}

//------------------------------------------------------------------------------------------
