
//---------------------------------------------------------------------------------
// edline.h : edit a text line with cursor movements
//---------------------------------------------------------------------------------

//---------------------------------------------------------------------------------

struct EDIT_LINE;

//---------------------------------------------------------------------------------

struct LINE_MARK
{
  bool active;   // true = mark is on this line   
  int  col;      // in range 0 .. line^'length
}

//---------------------------------------------------------------------------------

void edit_line_allocate (out EDIT_LINE edit_line,
                             wstring^  line,                // preallocated current line
                             int       length,              // active length of line, has no trailing spaces
                             int       col,                 // in range 0 ..  line^'length-1, or 0 .. line^.length if extra_col.
                             bool      extra_col,           // can cursor go past last col
                             bool      modify_allowed,      // false = line may not change
                             bool      line_modified,       // true = line was changed
                             bool      insert_mode,         // false = delete, true = insert
                             bool      switch_mode_allowed, // false = no switch allowed
                             bool      set_marks_allowed,   // true = marks can be set
                             bool      marks_modified,      // true = marks were modified
                             LINE_MARK first,               // .col included
                             LINE_MARK last);               // .col excluded
                         
void edit_line_dispose (ref EDIT_LINE edit_line);  // frees line

//---------------------------------------------------------------------------------

// assertion: line'length must have at least max_length.
// line is filled with filler char.
void edit_line_get_line             (EDIT_LINE edit_line, out wstring line, wchar filler = L' ');

int  edit_line_get_max_length       (EDIT_LINE edit_line);
int  edit_line_get_length           (EDIT_LINE edit_line);
int  edit_line_get_col              (EDIT_LINE edit_line);
bool edit_line_get_extra_col        (EDIT_LINE edit_line);
bool edit_line_get_modify_allowed   (EDIT_LINE edit_line);
bool edit_line_get_line_modified    (EDIT_LINE edit_line);
bool edit_line_get_insert_mode      (EDIT_LINE edit_line);
bool edit_line_get_marks_modified   (EDIT_LINE edit_line);
void edit_line_get_first_mark       (EDIT_LINE edit_line, out LINE_MARK first);
void edit_line_get_last_mark        (EDIT_LINE edit_line, out LINE_MARK last);

//---------------------------------------------------------------------------------

void edit_line_set_line             (ref EDIT_LINE edit_line, wstring line);
void edit_line_set_col              (ref EDIT_LINE edit_line, int col);
void edit_line_set_modify_allowed   (ref EDIT_LINE edit_line, bool modify_allowed);
void edit_line_set_line_modified    (ref EDIT_LINE edit_line, bool line_modified);
void edit_line_set_insert_mode      (ref EDIT_LINE edit_line, bool insert_mode);
void edit_line_set_first_mark       (ref EDIT_LINE edit_line, LINE_MARK first);
void edit_line_set_last_mark        (ref EDIT_LINE edit_line, LINE_MARK last);

//---------------------------------------------------------------------------------

void edit_line_achar           (ref EDIT_LINE edit_line, wchar achar);
void edit_line_switch_insert   (ref EDIT_LINE edit_line);
void edit_line_home            (ref EDIT_LINE edit_line);
void edit_line_end             (ref EDIT_LINE edit_line);
void edit_line_cursor_left     (ref EDIT_LINE edit_line);
void edit_line_cursor_right    (ref EDIT_LINE edit_line);
void edit_line_set_top_mark    (ref EDIT_LINE edit_line);
void edit_line_set_bottom_mark (ref EDIT_LINE edit_line);
void edit_line_mark_all        (ref EDIT_LINE edit_line);

//---------------------------------------------------------------------------------

/* return 1 if current column was behind end-of-line and thus     */
/*          the line below has to be joined with this line        */
/*          at cursor position (only for mode insert),            */
/*        0 if the delete operation was done, or not done in      */
/*          case no modifications are allowed or for mode delete. */

int edit_line_delete (ref EDIT_LINE edit_line);

//---------------------------------------------------------------------------------

/* return 1 if current column was behind end-of-line and thus     */
/*          the line below has to be joined with this line        */
/*          at cursor position (only for mode insert),            */
/*        0 if the delete operation was done, or not done in      */
/*          case no modifications are allowed or for mode delete. */

int edit_line_delete_word (ref EDIT_LINE edit_line);

//---------------------------------------------------------------------------------

/* return 1 if current column was start-of-line and thus          */
/*          this line has to be joined with the line above        */
/*          (only for mode insert),                               */
/*        0 if the backspace operation was done, or not done in   */
/*          case no modifications are allowed or for mode delete. */

int edit_line_backspace (ref EDIT_LINE edit_line);

//---------------------------------------------------------------------------------

/* return 1 if there was no previous word on this line         */
/*          (the cursor column is left unchanged in this case) */
/*        0 if the cursor was correctly placed on the          */
/*          first character of the previous word.              */

int edit_line_previous_word (ref EDIT_LINE edit_line, bool begin_of_word, bool come_from_next_line = false);

//---------------------------------------------------------------------------------

/* return 1 if there was no next word on this line             */
/*          (the cursor column is left unchanged in this case) */
/*        0 if the cursor was correctly placed on the          */
/*          first character of the next word.                  */

int edit_line_next_word (ref EDIT_LINE edit_line, bool begin_of_word, bool come_from_previous_line = false);

//---------------------------------------------------------------------------------

wstring^ edit_line_get_marked_text (ref EDIT_LINE edit_line);
  
//---------------------------------------------------------------------------------

// delete the marked text and set col to its start
// returns 0 if OK, 1 if no text marked

int edit_line_delete_block (ref EDIT_LINE edit_line);
  
//---------------------------------------------------------------------------------

// insert text at cursor col, set marks around inserted text, put cursor at end of insertion
// returns 0 if OK, 1 if text too large

int edit_line_insert_text_block (ref EDIT_LINE edit_line, wstring text);

//---------------------------------------------------------------------------------

void edit_line_clone (EDIT_LINE source, out EDIT_LINE target);

//--------------------------------------------------------------------------

// source is deleted
void edit_line_unsafe_copy (ref EDIT_LINE source, out EDIT_LINE target);

//--------------------------------------------------------------------------

bool edit_line_identical (EDIT_LINE a, EDIT_LINE b);

//--------------------------------------------------------------------------
