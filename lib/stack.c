
// stack.c : unlimited depth stack

use arraye, thread;

package body STACK

  package P = new ARRAY_EXTENDER (ELEMENT => ELEMENT);

  struct Stack
  {
    ELEMENT[]^ e;
    int        count;
  }

  public void push (ref Stack s, ELEMENT e)
  {
    thread.fetch_cache ();
   
    if (s.count == 0 || s.e^'length == s.count)
      P.enlarge (ref s.e, by_count => 1024);
    s.e^[s.count++] = e;

    thread.flush_cache ();
  }

  public void pop (ref Stack s, out ELEMENT e)
  {
    thread.fetch_cache ();

    assert s.count > 0;
    if (s.e^'length - s.count > 2*1024)
      P.shrink (ref s.e, by_count => 1024);
    e = s.e^[--s.count];

    thread.flush_cache ();
  }

  public int nb_elements (Stack s)
  {
    thread.fetch_cache ();
    return s.count;
  }

  public void dispose (ref Stack s)
  {
    thread.fetch_cache ();
    free s.e;
    clear s;
    thread.flush_cache ();
  }

end STACK;
