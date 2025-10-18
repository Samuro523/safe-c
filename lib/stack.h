
// stack.h : unlimited depth stack

generic <ELEMENT>
package STACK

  struct Stack;

  void push (ref Stack s, ELEMENT e);
  void pop  (ref Stack s, out ELEMENT e);

  int  nb_elements (Stack s);

  void dispose (ref Stack s);

end STACK;
