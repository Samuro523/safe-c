
// aslab.h : tree that stores labels for jumping under the function's code and raise int5 exceptions

// --------------------------------

void create_ll_tree ();

// returns label_nr (either new of from existing node for that line)
int4 store_ll (int line, int4 label_nr);

// stores always the label, with an occurence >= 1 (for P_ASSERT)
void store_ll_forced (int line, int4 label_nr);

// --------------------------------

struct LINE_LABEL_DATA
{
  int   line;
  int   occurence;  // usually 0, 1 or more for P_ASSERT where all occurences are stored
  int4  label_nr;
}

typedef int LL_FUNC (bool^           user,
                     LINE_LABEL_DATA p);

void traverse_ll_tree (LL_FUNC func);

// --------------------------------

void close_ll_tree ();

// --------------------------------
