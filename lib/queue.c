
// queue.c

from std use thread;

package body SAFE_QUEUE

  struct ITEM
  {
    ITEM^   next;
    INFO    info;
  }

  struct QUEUE
  {
    SHARED_OBJECT o;
    ITEM^         head, tail;
    int           count;
  }

  public void enqueue (ref QUEUE queue, INFO info)
  {
    ITEM^ p = new ITEM ' {null, info};

    enter_shared_object (ref queue.o);

    if (queue.head == null)
      queue.head = p;
    else
      queue.tail^.next = p;

    queue.tail = p;
    queue.count++;

    leave_shared_object (ref queue.o);
  }

  // returns 0 if OK, -1 if no more items yet
  public int dequeue (ref QUEUE queue, out INFO info)
  {
    ITEM^ p;

    if (queue.count == 0)
    {
      clear info;
      return -1;
    }

    enter_shared_object (ref queue.o);

    if (queue.count == 0)
    {
      leave_shared_object (ref queue.o);
      clear info;
      return -1;
    }

    p = queue.head;
    info = p^.info;
    queue.head = p^.next;
    if (queue.head == null)
      queue.tail = null;
    queue.count--;

    free p;

    leave_shared_object (ref queue.o);
    return 0;
  }

  public void requeue (ref QUEUE queue, INFO info)
  {
    ITEM^ p;

    enter_shared_object (ref queue.o);

    p = new ITEM ' {queue.head, info};

    if (queue.tail == null)
      queue.tail = p;

    queue.head = p;
    queue.count++;

    leave_shared_object (ref queue.o);
  }
  
  public int count (QUEUE queue)
  {
    return queue.count;
  }

  public void delete_all_items (ref QUEUE queue)
  {
    INFO info;
    while (dequeue (ref queue, out info) == 0)   // will create queue.o if not exists
      ;
    _unused info;
  }

  public void dispose (ref QUEUE queue)
  {
    delete_all_items (ref queue);
    destroy_shared_object (ref queue.o);
    clear queue;
  }

end SAFE_QUEUE;
