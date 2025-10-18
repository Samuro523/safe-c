
// edtext.h

use text;

// =====================================================================

struct EDIT_TEXT;

// =====================================================================

void edit_text_allocate (out EDIT_TEXT edit_text,
                             int  max_line_length,
                             int  max_text_lines,
                             bool insert_mode);

void edit_text_dispose (ref EDIT_TEXT edit_text);

// =====================================================================

packed struct TEXT_MARK
{
  int  ln;          // in range  1 .. nb_text_lines
  int  col;         // in range  0 .. max_length
}

// =====================================================================

int edit_text_count_lines (EDIT_TEXT edit_text);

// =====================================================================

// assertion: ln in range 1 .. count_lines()
// assertion: line'length == max_line_length
// line is filled with trailing spaces
void edit_text_get_line (ref EDIT_TEXT edit_text, int ln, out wstring line, out int actual_length);

// =====================================================================

int edit_text_get_col             (EDIT_TEXT edit_text);
int edit_text_get_ln              (EDIT_TEXT edit_text);
int edit_text_get_max_line_length (EDIT_TEXT edit_text);
bool edit_text_get_modify_allowed (EDIT_TEXT edit_text);
bool edit_text_get_text_modified  (EDIT_TEXT edit_text);
bool edit_text_get_insert_mode    (EDIT_TEXT edit_text);
wstring^ edit_text_get_full_text   (ref EDIT_TEXT edit_text);
wstring^ edit_text_get_marked_text (ref EDIT_TEXT edit_text);
void edit_text_get_first_mark     (EDIT_TEXT edit_text, out TEXT_MARK first);
void edit_text_get_last_mark      (EDIT_TEXT edit_text, out TEXT_MARK last);
bool edit_text_get_redraw_line_needed   (EDIT_TEXT edit_text);
bool edit_text_get_redraw_screen_needed (EDIT_TEXT edit_text);

// =====================================================================

void edit_text_set_col            (ref EDIT_TEXT edit_text, int col);
void edit_text_set_ln             (ref EDIT_TEXT edit_text, int ln);
void edit_text_set_modify_allowed (ref EDIT_TEXT edit_text, bool modify_allowed);
void edit_text_set_insert_mode    (ref EDIT_TEXT edit_text, bool insert_mode);
void edit_text_set_text_modified  (ref EDIT_TEXT edit_text, bool text_modified);
void edit_text_set_redraw_line_needed   (ref EDIT_TEXT edit_text, bool redraw_line_needed = true);
void edit_text_set_redraw_screen_needed (ref EDIT_TEXT edit_text, bool redraw_screen_needed = true);
void edit_text_clear_text         (ref EDIT_TEXT edit_text);
void edit_text_set_full_text      (ref EDIT_TEXT edit_text, wstring text);
void edit_text_set_marks          (ref EDIT_TEXT edit_text, TEXT_MARK mark1, TEXT_MARK mark2);  // marks in any order
void edit_text_set_marks_on_all_text (ref EDIT_TEXT edit_text);

// The function returns :
//    0 if OK,
//   +1 if the text is read-only and thus no insertion was done.
//   -1 if text too large,
int edit_text_insert_text_block   (ref EDIT_TEXT edit_text, wstring text);

// =====================================================================

void edit_text_achar           (ref EDIT_TEXT edit_text, wchar achar);
void edit_text_switch_insert   (ref EDIT_TEXT edit_text);
void edit_text_home            (ref EDIT_TEXT edit_text);
void edit_text_end             (ref EDIT_TEXT edit_text);
void edit_text_cursor_left     (ref EDIT_TEXT edit_text);
void edit_text_cursor_right    (ref EDIT_TEXT edit_text);
void edit_text_enter           (ref EDIT_TEXT edit_text);
void edit_text_delete          (ref EDIT_TEXT edit_text);
void edit_text_cursor_up       (ref EDIT_TEXT edit_text);
void edit_text_cursor_down     (ref EDIT_TEXT edit_text);
void edit_text_delete_word     (ref EDIT_TEXT edit_text);
void edit_text_backspace       (ref EDIT_TEXT edit_text);
void edit_text_previous_word   (ref EDIT_TEXT edit_text, bool begin_of_word);
void edit_text_next_word       (ref EDIT_TEXT edit_text, bool begin_of_word);
void edit_text_top_of_text     (ref EDIT_TEXT edit_text);
void edit_text_bottom_of_text  (ref EDIT_TEXT edit_text);
void edit_text_delete_line     (ref EDIT_TEXT edit_text);
void edit_text_tab             (ref EDIT_TEXT edit_text);
void edit_text_untab           (ref EDIT_TEXT edit_text);

// =====================================================================

// offset = nb pages up (<0) or down (>0)
// if not possible, we move the max nb of lines possible
void edit_text_page (ref EDIT_TEXT edit_text, int offset);

// =====================================================================

// this is just a little utility routine
bool is_empty_range (TEXT_MARK first, TEXT_MARK last);

// =====================================================================

// +1=indent, -1=unindent
int edit_text_indent_block (ref EDIT_TEXT edit_text, int indent);

// =================================================================

// copy marked block of 'edit_text' into 'text'.
// 'text' must be created before the call and closed after it.
// The function returns 0, or +1 if no text block was marked.

int edit_text_copy_block_to_text (ref EDIT_TEXT edit_text, ref W_TEXT text);

// =================================================================

// insert 'text' in 'edit_text' at current cursor position.
// 'text' must be created before the call and closed after it.
// The last crlf of 'text' is not copied.
// The marks of 'edit_text' are set around the inserted text
// and the cursor position is set to the end of the inserted text.
// The function returns :
//    0 if OK,
//   +1 if the text is read-only and thus no insertion was done.
//   -1 if text too large,

int edit_text_insert_block (ref EDIT_TEXT edit_text, ref W_TEXT text);

// =================================================================

// current col,line is always set to the start of the marked text.
// delete marked block of 'edit_text'
// returns 0 if OK, +1 if not done

int edit_text_delete_block (ref EDIT_TEXT edit_text);

// =================================================================

// find text fragment, start searching at current text position.   
// returns (+1) if no further occurence of the fragment was found. 
// returns -3 in case of illegal value for 'direction'.            
// returns -6 if fragment_length is zero.                          

int edit_text_find (ref EDIT_TEXT  edit_text,
                        wstring    fragment,
                        int        direction,    // -1 = up, +1 = down 
                        bool       match_case,   // true = case must match 
                        bool       whole_word);  // true = delimited by non-alpha 

// =================================================================

// replace text of length 'fragment_length' by 'replacer' of length 
//   'replacer_length'.                                             
// this function should be called after 'edit_text_find' found      
//   a fragment to be replaced.                                     
// returns (+2) if the text is read-only.                           
// returns (-8) if the fragment cannot be contained in the line.    
// returns (-9) if the line becomes too long.                       

int edit_text_replace (ref EDIT_TEXT  edit_text,
                           int        fragment_length,
                           wstring    replacer);

// =================================================================

void edit_clone (ref EDIT_TEXT source, out EDIT_TEXT target);

// =================================================================

bool edit_are_identical (ref EDIT_TEXT a, ref EDIT_TEXT b);

// =================================================================

// copies source into target, then deletes source.

void edit_unsafe_copy (ref EDIT_TEXT source, out EDIT_TEXT target);

// =================================================================
