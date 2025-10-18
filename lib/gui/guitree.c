
use ../text;

public bool tree_line_is_expanded (ref W_TEXT wtext, int ln, uint2 level)
{
  if (ln >= wnb_text_lines (wtext))   // no line below -> [+]
    return false;

  {
    bool minus;
    wstring^ line = new wchar [PREFIX_WLEN];
    int      length;
    wretrieve_text_line (ref wtext, ln+1, out line^, out length);
    _unused length;
#begin unsafe    
    minus = ((PREFIX*)&line^)->level > level;  // inner child -> [-]
#end unsafe    
    free line;
    return minus;
  }
}
