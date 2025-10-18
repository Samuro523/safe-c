
// guitreatevent.h

use guitree;

//--------------------------------------------------------------------------

// called when creating dialog window
void set_tab_on_next_or_previous_object (ref DIALOG_INFO d, bool next);

void update_scroll_x_edit_or_combo (ref CONTROL_INFO o, ref EDIT_INFO edit, int x_size);

//--------------------------------------------------------------------------

void mouse_click_left           (CONTROL_INFO^ pc, int d_x, int d_y, bool is_double_click, int dialog_x, int dialog_y);
void mouse_click_right          (CONTROL_INFO^ pc, int d_x, int d_y, int dialog_x, int dialog_y);
void mouse_click_left_released  (CONTROL_INFO^ pc, int d_x, int d_y);
void mouse_click_right_released (CONTROL_INFO^ pc, int d_x, int d_y, int dialog_x, int dialog_y);
void mouse_drag (CONTROL_INFO^ pc, int d_x, int d_y);
void mouse_hover (CONTROL_INFO^ pc, int d_x, int d_y);
void mouse_wheel (int delta);

//--------------------------------------------------------------------------

void treat_keyboard_key (int key);

//--------------------------------------------------------------------------

void process_key_editbox (ref CONTROL_INFO o,
                          ref EDITBOX_INFO editbox,
                              int          key,
                          out bool         key_was_treated);

//--------------------------------------------------------------------------

void save_edit_undo_context (ref EDIT_INFO edit);

//--------------------------------------------------------------------------

void deallocate_controls (CONTROL_INFO^ list);

//--------------------------------------------------------------------------

// x, y are relative to the dialog window

#begin unsafe
CONTROL_INFO^ touched_control (DIALOG_INFO* p, int x, int y);
#end unsafe

//--------------------------------------------------------------------------
