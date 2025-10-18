
// queue.h

generic <INFO>
package SAFE_QUEUE   // multi-thread protected queue

  struct QUEUE;

  // add an item at tail
  void enqueue (ref QUEUE queue, INFO info);

  // retrieve an item from head
  // returns 0 if OK, -1 if no more items yet
  int dequeue (ref QUEUE queue, out INFO info);

  // put back a just dequeued item (undo the dequeue, adds an item at head)
  void requeue (ref QUEUE queue, INFO info);
  
  int count (QUEUE queue);

  void delete_all_items (ref QUEUE queue);

  void dispose (ref QUEUE queue);
  
end SAFE_QUEUE;
