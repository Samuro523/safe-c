
// set.h : sets stored using balanced binary tree of intervals

/************************************************************************/
struct SET;
/************************************************************************/

/* initialize a set structure */

void SET_create (out SET set);

/************************************************************************/

/* deallocate the set structure */

void SET_close (ref SET set);

/************************************************************************/

/* insert new item into set.                                   */
/* returns true if ok (the item was inserted),                 */
/*         false if not done (the item is already in the set). */

bool SET_insert_item (ref SET set, long item);

/************************************************************************/

/* delete item from set.                                           */
/* returns true if ok (the item was deleted),                      */
/*         false if not done (the item is not present in the set). */

bool SET_delete_item (ref SET set, long item);

/************************************************************************/

/* insert all items between first and last into the set.                  */
/* returns true if OK (all items were inserted),                          */
/*         false if not done (some or all items were already in the set). */

bool SET_insert_interval (ref SET set, long first, long last);

/************************************************************************/

/* delete all items between first and last from the set.                      */
/* returns true if OK (all items were deleted),                               */
/*         false if not done (some or all items were not present in the set). */

bool SET_delete_interval (ref SET set, long first, long last);

/************************************************************************/

/* get first/last item in the set.                           */
/* returns with first == 1 && last == 0 if the set is empty. */

void SET_get_range (SET set, out long first, out long last);

/************************************************************************/

/* count the number of items in the set */

long SET_nb_items (SET set);

/************************************************************************/

/* count the number of disjoint intervals in the set */

long SET_nb_intervals (SET set);

/************************************************************************/

/* test if an item is in the set */

bool SET_item_found (SET set, long item);

/************************************************************************/

/* test if all items in the interval [first .. last] are in the set */

bool SET_interval_found (SET set, long first, long last);

/************************************************************************/

/* search an item that is >= 'low'.              */
/* returns true if found,                        */
/*         false if no such item is in the set.  */

bool SET_search_any_item (    SET  set,
                              long low,
                          out long item);

/************************************************************************/

/* search an item that is <= 'high'.            */
/* returns true if found,                       */
/*         false if no such item is in the set. */

bool SET_search_any_item2 (    SET  set,
                               long high,
                           out long item);

/************************************************************************/

/* search for the nearest interval of consecutive elements  */
/* that are >= 'low'.                                       */
/* returns true if found,                                   */
/*         false if no items larger or equal to 'low'.      */

bool SET_search_any_interval (    SET  set,
                                  long low,
                              out long first,
                              out long last);

/************************************************************************/

/* search for the nearest interval of consecutive elements  */
/* that are <= 'high'.                                      */
/* returns true if found,                                   */
/*         false if no items smaller or equal to 'high'.    */

bool SET_search_any_interval2 (    SET  set,
                                   long high,
                               out long first,
                               out long last);

/************************************************************************/

/* insert all items of set 'source' to set 'target' */

void SET_union (ref SET target, SET source);

/************************************************************************/

/* remove all items from set 'target' that do not exist in set 'source' */

void SET_intersect (ref SET target, SET source);

/************************************************************************/

/* copy set 'source' to set 'target' */

void SET_copy (out SET target, SET source);

/************************************************************************/
