
// common.h : used by front-end and back-end

struct TEXT_POSITION
{
  int col, line;
}

struct LOCATION
{
  int unit_key;   // 0 means undefined location
  int source_line;
}

enum COMPARISON_FLAG
{
  CMP_FILLER,
  CMP_SMALLER,           // 1
  CMP_EQUAL,             // 2
  CMP_SMALLER_OR_EQUAL,  // 3
  CMP_LARGER,            // 4
  CMP_NOT_EQUAL,         // 5
  CMP_LARGER_OR_EQUAL,   // 6
};
