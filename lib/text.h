
// text.h : opaque text object holding n lines of data

//--------------------------------------------------------------------------
struct A_TEXT;
//--------------------------------------------------------------------------

void creat_text (out A_TEXT handle);
void close_text (ref A_TEXT handle);

//--------------------------------------------------------------------------

int nb_text_lines (A_TEXT handle);

//--------------------------------------------------------------------------

void delete_text (ref A_TEXT handle);

//--------------------------------------------------------------------------

/* line_number must be in range 1 .. nb_text_lines()+1 */

void insert_text_line (ref A_TEXT handle,
                           int    line_number,
                           byte[] line);

//--------------------------------------------------------------------------

/* line_number must be in range 1 .. nb_text_lines() */

void update_text_line (ref A_TEXT handle,
                           int    line_number,
                           byte[] line);

//--------------------------------------------------------------------------

/* line_number must be in range 1 .. nb_text_lines()       */
/* if the line is longer than buffer'size, it is truncated */

void retrieve_text_line (ref A_TEXT   handle,
                             int      line_number,
                         out byte[]   buffer,
                         out int      length);

//--------------------------------------------------------------------------

/* line_number must be in range 1 .. nb_text_lines() */

void delete_text_line (ref A_TEXT handle,
                       int line_number);

//--------------------------------------------------------------------------

/* sort text lines using ascii */

void sort_text_lines (ref A_TEXT handle);

//--------------------------------------------------------------------------

generic <USER_INFO>
package USER_SORT_TEXT_LINES

  /* must return -1 if data1 < data2, 0 if equal, +1 if > */
  typedef int COMPARE_TEXT_LINES (byte[]^ data1, byte[]^ data2, ref USER_INFO user_info);

  /* sort text lines using user-defined line comparison function */
  void sort (ref A_TEXT handle, COMPARE_TEXT_LINES compare, ref USER_INFO user_info);

end USER_SORT_TEXT_LINES;

//--------------------------------------------------------------------------

void text_copy (A_TEXT source, out A_TEXT target);

//--------------------------------------------------------------------------

// source is deleted
void text_unsafe_copy (ref A_TEXT source, out A_TEXT target);

//--------------------------------------------------------------------------

bool text_identical (A_TEXT a, A_TEXT b);

//--------------------------------------------------------------------------


//--------------------------------------------------------------------------
struct W_TEXT;
//--------------------------------------------------------------------------

void wcreat_text (out W_TEXT handle);
void wclose_text (ref W_TEXT handle);

//--------------------------------------------------------------------------

int wnb_text_lines (W_TEXT handle);

//--------------------------------------------------------------------------

void wdelete_text (ref W_TEXT handle);

//--------------------------------------------------------------------------

/* line_number must be in range 1 .. nb_text_lines()+1 */

void winsert_text_line (ref W_TEXT  handle,
                            int     line_number,
                            wstring line);

//--------------------------------------------------------------------------

/* line_number must be in range 1 .. nb_text_lines() */

void wupdate_text_line (ref W_TEXT  handle,
                            int     line_number,
                            wstring line);

//--------------------------------------------------------------------------

/* line_number must be in range 1 .. wnb_text_lines()       */
/* if the line is longer than buffer'size, it is truncated */

void wretrieve_text_line (ref W_TEXT   handle,
                              int      line_number,
                          out wstring  buffer,
                          out int      length);

//--------------------------------------------------------------------------

/* line_number must be in range 1 .. nb_text_lines() */

void wdelete_text_line (ref W_TEXT handle,
                           int     line_number);

//--------------------------------------------------------------------------

/* sort text lines using ascii */

void wsort_text_lines (ref W_TEXT handle);

//--------------------------------------------------------------------------

generic <USER_INFO>
package WUSER_SORT_TEXT_LINES

  /* must return -1 if data1 < data2, 0 if equal, +1 if > */
  typedef int WCOMPARE_TEXT_LINES (wstring^ data1, wstring^ data2, ref USER_INFO user_info);

  /* sort text lines using user-defined line comparison function */
  void sort (ref W_TEXT handle, WCOMPARE_TEXT_LINES compare, ref USER_INFO user_info);

end WUSER_SORT_TEXT_LINES;

//--------------------------------------------------------------------------

void wtext_clone (W_TEXT source, out W_TEXT target);

//--------------------------------------------------------------------------

// source is deleted
void wtext_unsafe_copy (ref W_TEXT source, out W_TEXT target);

//--------------------------------------------------------------------------

bool wtext_identical (W_TEXT a, W_TEXT b);

//--------------------------------------------------------------------------
